
// ================================================================================
// Uploaded to @imguisource_bot
// Sub: t.me/ImGui_Source_FIXNEY (ImGui Source)
// Sub: t.me/il2cpp_src (shit src 🧨)
// ================================================================================

#include "Rpm.h"
#include "../Diag/Profiler.h"

#include <chrono>
#include <cstring>
#include <intrin.h> // _mm_sfence / YieldProcessor

// ─────────────────────────────────────────────────────────────────────────────
//  topg / VexusShared kernel-driver protocol (matches the kernel main.c).
// ─────────────────────────────────────────────────────────────────────────────
static constexpr ULONG DRV_READ = 1;
static constexpr ULONG DRV_COMPLETE = 2;
static constexpr ULONG DRV_WRITE = 3;
static constexpr ULONG DRV_MODULE_BASE = 5;
static constexpr ULONG DRV_PING = 99;
static constexpr ULONG DRV_READY = 100;
static constexpr ULONG DRV_FAILED = 0x20;

static constexpr const char *kSectionName = "Global\\VexusShared";
static constexpr size_t kChunk = kNiohBufferSize; // NiohShmem::Buffer
static constexpr const char *kWakeEventName = "Global\\VexusWake";

// x64 PEB / loader-list offsets (valid for a 64-bit target process).
static constexpr uint64_t PEB_LDR = 0x18;          // PEB  -> PEB_LDR_DATA*
static constexpr uint64_t LDR_INMEM_LIST = 0x20;   // LDR  -> InMemoryOrderModuleList
static constexpr uint64_t LDR_INMEM_LINKS = 0x10;  // LDR_DATA_TABLE_ENTRY.InMemoryOrderLinks
static constexpr uint64_t ENT_DLLBASE = 0x30;      //            .DllBase
static constexpr uint64_t ENT_SIZEOFIMAGE = 0x40;  //            .SizeOfImage
static constexpr uint64_t ENT_BASEDLLNAME = 0x58;  //            .BaseDllName
static constexpr uint64_t UNICODE_BUFFER = 0x08;   // UNICODE_STRING.Buffer

Memory *Driver = nullptr;
thread_local int Memory::tls_slot = -1;

Memory::Memory()
    : m_processId(0), m_isAttached(false), m_hMap(nullptr), m_pMem(nullptr) {}

Memory::~Memory() {
  Detach();
  if (m_pMem) {
    UnmapViewOfFile(m_pMem);
    m_pMem = nullptr;
  }
  if (m_hWakeEvent) {
    CloseHandle(m_hWakeEvent);
    m_hWakeEvent = nullptr;
  }
  if (m_hMap) {
    CloseHandle(m_hMap);
    m_hMap = nullptr;
  }
}

// ── process lookup ──────────────────────────────────────────────────────────
DWORD Memory::GetProcessIdByName(const std::string &processName) {
  DWORD pid = 0;
  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

  if (snapshot != INVALID_HANDLE_VALUE) {
    PROCESSENTRY32W entry;
    entry.dwSize = sizeof(PROCESSENTRY32W);

    if (Process32FirstW(snapshot, &entry)) {
      do {
        std::wstring wProcessName(entry.szExeFile);
        std::string narrowName(wProcessName.begin(), wProcessName.end());

        if (_stricmp(narrowName.c_str(), processName.c_str()) == 0) {
          pid = entry.th32ProcessID;
          break;
        }
      } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
  }

  return pid;
}

// ── kernel-driver transport ─────────────────────────────────────────────────
// A request normally completes in microseconds. If it does not, the usual
// reason is that the target page had to be faulted back in from the page file,
// which the kernel can take hundreds of milliseconds (sometimes seconds) to do.
// The wait is bounded to 50 ms: on timeout the request is left published so the
// driver can still finish it safely, and the channel is marked in-flight so
// every other read fails fast until it drains. The overlay keeps drawing the
// last good snapshot/matrix instead of freezing for the fault duration.
static constexpr DWORD kDriverWaitMs = 50;

void Memory::SelectSlot(int index) {
  tls_slot = (index >= 0 && index < static_cast<int>(kNiohSlots)) ? index : 0;
}

Memory::Slot &Memory::CurrentSlot() {
  // Slot 0 is the default worker lane. The W2S updater selects slot 1 so the
  // high-frequency matrix read keeps moving even while the entity worker is
  // busy with a large ESP/image pass.
  if (tls_slot < 0 || tls_slot >= static_cast<int>(kNiohSlots))
    tls_slot = 0;
  return m_slots[static_cast<size_t>(tls_slot)];
}

bool Memory::DriverWait(Slot &slot, ULONG status) {
  if (!slot.mem)
    return false;
  using namespace std::chrono;
  const auto start = steady_clock::now();
  uint32_t spin = 0;

  while (slot.mem->Status == status) {
    // Spin hard for the common case - the driver's CR3 round-trip is usually
    // only a few microseconds, and a pure spin gives the lowest latency with no
    // scheduler involvement. But a request that outlives that (the driver is
    // momentarily busy) used to keep a whole core pinned at 100% for up to
    // 200 ms, on every one of the three reader threads. After ~4k spins, yield
    // to the scheduler instead: SwitchToThread/Sleep(0) return immediately when
    // no other thread is ready, so the fast path is untouched and a slow op no
    // longer starves RustClient of a core.
    // Spin briefly for the common case (a cached read finishes in a few us),
    // then yield. A slow driver operation should not pin a core while Rust is
    // competing for it; the shared lane already prevents parallel requests.
    ++spin;
    if (spin < 512u)
      YieldProcessor();
    else if (spin < 4096u)
      SwitchToThread();
    else if (spin < 32768u)
      Sleep(0);
    else
      Sleep(1);

    // Sample the clock every 1024 iterations: steady_clock::now() costs ~25ns,
    // so calling it every spin would dominate a normal (sub-10us) read.
    if ((spin & 0x3FF) != 0)
      continue;

    const auto ms =
        duration_cast<milliseconds>(steady_clock::now() - start).count();
    if (ms < (long long)kDriverWaitMs)
      continue;

    // Log once per stall episode - throttled heavily to avoid console spam
    // (a burst of stall logs can themselves block the console and appear as
    // "the whole ESP goes OFF").
    {
      using namespace std::chrono;
      static steady_clock::time_point lastStall{};
      const auto now = steady_clock::now();
      if (now - lastStall >= seconds(5)) {
        if (!slot.stallLogged.exchange(true)) {
          printf("[driver] request 0x%lX stuck >%ldms (addr=0x%llX size=%lu) - "
                 "skipping this read, not blocking\n",
                 (unsigned long)status, (long)kDriverWaitMs,
                 (unsigned long long)slot.mem->Address,
                 (unsigned long)slot.mem->Size);
          fflush(stdout);
          lastStall = now;
        }
      } else if (!slot.stallLogged.exchange(true)) {
        // suppress log for this episode to avoid spamming; still mark as logged
      }
    }
    return false;
  }

  slot.stallLogged.store(false);
  return true;
}

// Must be called with the slot mutex held. A timed-out request is only cleared
// once the driver has actually finished it: publishing a new request on top of
// one still in flight would let the driver's stale DRV_COMPLETE be mistaken for
// ours and hand back garbage data.
bool Memory::ChannelReady(Slot &slot) {
  if (!slot.mem)
    return false;
  if (!slot.inFlight.load(std::memory_order_acquire))
    return true;
  if (slot.mem->Status == slot.inFlightStatus)
    return false; // still being serviced - do not block, do not overwrite
  slot.inFlight.store(false, std::memory_order_release);
  return true;
}

// Publishes a request and waits for it (bounded). Must hold the slot mutex.
bool Memory::SubmitAndWait(Slot &slot, ULONG status) {
  if (!slot.mem || !ChannelReady(slot))
    return false;
  _mm_sfence(); // Address/Size (and any write payload) land before Status
  slot.mem->Status = status;
  // Signal only when the kernel is actually asleep. The full fence pairs with the
  // kernel's "asleep" store + fence + status re-check, so one side always sees the
  // other. A missed signal costs at most the kernel's 2 ms wait timeout.
  _mm_mfence();
  if (m_hWakeEvent && slot.mem->KernelAsleep)
    SetEvent(m_hWakeEvent);
  if (DriverWait(slot, status))
    return true;
  slot.inFlightStatus = status;
  slot.inFlight.store(true, std::memory_order_release);
  return false;
}

bool Memory::DriverConnect() {
  if (m_pMem)
    return true;

  printf("[driver] opening %s...\n", kSectionName);

  // The usermode side owns the section; the kernel driver keeps trying to open
  // it and services whatever request we publish.
  //
  // Map the full NIOH_SLOTS array. The new topg driver initializes/polls all 8
  // slots; mapping only one struct (the old protocol) makes those kernel writes
  // run past the section view and bugcheck the machine as soon as the client
  // starts. Slot 0 remains the request channel used by this client.
  m_hMap = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                              sizeof(NiohShmemArray), kSectionName);
  if (!m_hMap && GetLastError() == ERROR_ALREADY_EXISTS)
    m_hMap = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, kSectionName);
  if (!m_hMap && GetLastError() == ERROR_ACCESS_DENIED)
    m_hMap =
        OpenFileMappingA(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, kSectionName);
  if (!m_hMap) {
    const DWORD err = GetLastError();
    printf("[driver] could not create/open %s (err %lu)\n", kSectionName, err);
    if (err == ERROR_ACCESS_DENIED) {
      printf("[driver] the shared section is held by a previous client or the "
             "loaded driver, with an older/restricted mapping.\n"
             "[driver] reboot (or unload + reload the driver) before running "
             "this build. If it persists and the old client ran elevated, also "
             "start this client as administrator.\n");
      fflush(stdout);
    }
    return false;
  }

  m_pMem = (NiohShmem *)MapViewOfFile(m_hMap, FILE_MAP_ALL_ACCESS, 0, 0,
                                      sizeof(NiohShmemArray));
  if (!m_pMem) {
    printf("[driver] MapViewOfFile failed (err %lu)\n", GetLastError());
    CloseHandle(m_hMap);
    m_hMap = nullptr;
    return false;
  }

  for (size_t i = 0; i < kNiohSlots; ++i) {
    m_slots[i].mem = &m_pMem[i];
    m_slots[i].mem->Status = 0;
    m_slots[i].mem->ProcessId = m_processId;
  }
  printf("[driver] section mapped: %zu slots, %zu bytes (entity + matrix lanes)\n",
         kNiohSlots, sizeof(NiohShmemArray));

  // Auto-reset wake event; the kernel attaches to it by name when it connects.
  m_hWakeEvent = CreateEventA(nullptr, FALSE, FALSE, kWakeEventName);
  if (!m_hWakeEvent)
    printf("[driver] wake event unavailable (err %lu) - falling back to polling\n",
           GetLastError());

  printf("[driver] waiting for the kernel driver to answer...\n");
  const ULONGLONG start = GetTickCount64();
  Slot &ping_slot = m_slots[0];
  while (true) {
    for (int i = 0; i < 20; ++i) {
      ping_slot.mem->Status = DRV_PING;
      if (m_hWakeEvent)
        SetEvent(m_hWakeEvent);
      Sleep(100);
      if (ping_slot.mem->Status == DRV_READY) {
        for (size_t s = 0; s < kNiohSlots; ++s)
          m_slots[s].mem->ProcessId = m_processId;
        printf("[driver] connected in %.1fs (pid %lu, entity+matrix lanes)\n",
               (GetTickCount64() - start) / 1000.0, m_processId);
        return true;
      }
    }
    printf("[driver] no answer yet (%.0fs) - is the kernel driver loaded?\n",
           (GetTickCount64() - start) / 1000.0);
  }
}

bool Memory::RawRead(uint64_t address, void *buffer, size_t size) {
  struct ReadTimer {
    uint64_t start = prof::NowUs();
    bool ok = false;
    ~ReadTimer() {
      (ok ? prof::driverRead : prof::driverFail)
          .Add(prof::NowUs() - start);
    }
  } readTimer;

  Slot &slot = CurrentSlot();
  if (!slot.mem || !address || !buffer || !size)
    return false;

  std::lock_guard<std::mutex> lock(slot.mutex);

  size_t offset = 0;
  while (offset < size) {
    const ULONG chunk =
        (ULONG)((size - offset) < kChunk ? (size - offset) : kChunk);

    slot.mem->Address = address + offset;
    slot.mem->Size = chunk;
    if (!SubmitAndWait(slot, DRV_READ)) {
      m_readsFail.fetch_add(1, std::memory_order_relaxed);
      ZeroMemory(buffer, size); // all-or-nothing, like a failed RPM read
      return false;
    }

    if (slot.mem->Status != DRV_COMPLETE) {
      m_readsFail.fetch_add(1, std::memory_order_relaxed);
      ZeroMemory(buffer, size); // all-or-nothing, like a failed RPM read
      return false;
    }

    memcpy((BYTE *)buffer + offset, (const void *)slot.mem->Buffer, chunk);
    offset += chunk;
  }

  m_readsOk.fetch_add(1, std::memory_order_relaxed);
  readTimer.ok = true;
  return true;
}

bool Memory::RawWrite(uint64_t address, const void *buffer, size_t size) {
  Slot &slot = CurrentSlot();
  if (!slot.mem || !address || !buffer || !size)
    return false;

  std::lock_guard<std::mutex> lock(slot.mutex);

  size_t offset = 0;
  while (offset < size) {
    const ULONG chunk =
        (ULONG)((size - offset) < kChunk ? (size - offset) : kChunk);

    memcpy(slot.mem->Buffer, (const BYTE *)buffer + offset, chunk);
    slot.mem->Address = address + offset;
    slot.mem->Size = chunk;
    if (!SubmitAndWait(slot, DRV_WRITE)) {
      m_writesFail.fetch_add(1, std::memory_order_relaxed);
      return false;
    }

    if (slot.mem->Status == DRV_FAILED) {
      m_writesFail.fetch_add(1, std::memory_order_relaxed);
      return false;
    }

    offset += chunk;
  }

  m_writesOk.fetch_add(1, std::memory_order_relaxed);
  return true;
}

// ── module list via a PEB walk performed through the driver ─────────────────
uint64_t Memory::GetPeb() {
  Slot &slot = CurrentSlot();
  if (!slot.mem)
    return 0;

  std::lock_guard<std::mutex> lock(slot.mutex);

  if (!SubmitAndWait(slot, DRV_MODULE_BASE))
    return 0;

  uint64_t peb = 0;
  if (slot.mem->Status == DRV_COMPLETE)
    memcpy(&peb, (const void *)slot.mem->Buffer, sizeof(peb));

  return peb;
}

bool Memory::ReadUnicodeString(uint64_t address, std::wstring &out,
                               size_t maxChars) {
  out.clear();
  if (!address || !maxChars)
    return false;

  if (maxChars > 512)
    maxChars = 512;

  std::vector<wchar_t> buffer(maxChars + 1, 0);
  if (!RawRead(address, buffer.data(), maxChars * sizeof(wchar_t)))
    return false;

  buffer[maxChars] = 0;

  size_t len = 0;
  while (len < maxChars && buffer[len])
    ++len;

  out.assign(buffer.data(), len);
  return !out.empty();
}

bool Memory::EnumModules(std::vector<ModInfo> &out) {
  out.clear();

  const uint64_t peb = GetPeb();
  if (!peb) {
    static bool reported = false;
    if (!reported) {
      reported = true;
      printf("[driver] PEB query returned 0 (command 5 unsupported?)\n");
    }
    return false;
  }

  uint64_t ldr = 0;
  if (!RawRead(peb + PEB_LDR, &ldr, sizeof(ldr)) || !ldr) {
    static bool reported = false;
    if (!reported) {
      reported = true;
      printf("[driver] could not read PEB+0x18 (ldr=0x%llX)\n",
             (unsigned long long)ldr);
    }
    return false;
  }

  const uint64_t head = ldr + LDR_INMEM_LIST; // list head (LIST_ENTRY)
  uint64_t link = 0;
  if (!RawRead(head, &link, sizeof(link)))
    return false;

  for (int guard = 0; guard < 1024 && link && link != head; ++guard) {
    const uint64_t entry = link - LDR_INMEM_LINKS;

    ModInfo info{};
    info.base = 0;
    info.size = 0;

    RawRead(entry + ENT_DLLBASE, &info.base, sizeof(info.base));
    RawRead(entry + ENT_SIZEOFIMAGE, &info.size, sizeof(info.size));

    USHORT nameLen = 0;
    uint64_t nameBuffer = 0;
    RawRead(entry + ENT_BASEDLLNAME, &nameLen, sizeof(nameLen));
    RawRead(entry + ENT_BASEDLLNAME + UNICODE_BUFFER, &nameBuffer,
            sizeof(nameBuffer));

    if (nameBuffer && nameLen) {
      std::wstring name;
      if (ReadUnicodeString(nameBuffer, name, (size_t)nameLen / sizeof(wchar_t)))
        info.name = name;
    }

    out.push_back(info);

    if (!RawRead(link, &link, sizeof(link)))
      break;
  }

  static bool reported = false;
  if (!reported && !out.empty()) {
    reported = true;
    printf("[driver] PEB 0x%llX -> %zu modules visible\n",
           (unsigned long long)peb, out.size());
  }

  return !out.empty();
}

bool Memory::FindModule(const std::wstring &name, uint64_t &base,
                        uint64_t &size) {
  base = 0;
  size = 0;

  std::vector<ModInfo> modules;
  if (!EnumModules(modules))
    return false;

  for (const ModInfo &module : modules) {
    if (_wcsicmp(module.name.c_str(), name.c_str()) == 0) {
      base = module.base;
      size = module.size;
      return true;
    }
  }

  return false;
}

// ── process management ──────────────────────────────────────────────────────
bool Memory::Attach(const std::string &processName) {
  if (m_isAttached)
    Detach();

  const DWORD pid = GetProcessIdByName(processName);
  if (pid == 0)
    return false;

  m_processId = pid;
  m_processName = processName;

  // Blocks until the kernel driver answers; retried by the caller otherwise.
  if (!DriverConnect()) {
    m_processId = 0;
    m_processName.clear();
    return false;
  }

  for (size_t i = 0; i < kNiohSlots; ++i)
    m_slots[i].mem->ProcessId = m_processId;
  m_isAttached = true;
  return true;
}

bool Memory::AttachByPid(DWORD pid) {
  if (m_isAttached)
    Detach();

  if (pid == 0)
    return false;

  m_processId = pid;

  if (!DriverConnect()) {
    m_processId = 0;
    return false;
  }

  for (size_t i = 0; i < kNiohSlots; ++i)
    m_slots[i].mem->ProcessId = m_processId;
  m_isAttached = true;
  return true;
}

void Memory::Detach() {
  // The shared mapping stays open so a re-attach just refreshes the pid.
  m_processId = 0;
  m_processName.clear();
  m_isAttached = false;
}

bool Memory::IsProcessRunning() {
  if (!m_isAttached || m_processId == 0)
    return false;

  bool running = false;
  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snapshot != INVALID_HANDLE_VALUE) {
    PROCESSENTRY32W entry;
    entry.dwSize = sizeof(PROCESSENTRY32W);

    if (Process32FirstW(snapshot, &entry)) {
      do {
        if (entry.th32ProcessID == m_processId) {
          running = true;
          break;
        }
      } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
  }

  return running;
}

// ── module operations ───────────────────────────────────────────────────────
namespace {
// Some protected/IL2CPP processes hide or omit modules from the PEB loader
// list even though the module is mapped. Toolhelp's module snapshot is a
// separate path and often still sees it; it does not require us to open the
// target process (the snapshot API does the query internally).
bool ToolhelpModule(DWORD pid, const std::wstring &wanted, uint64_t &base,
                    uint64_t &size) {
  base = 0;
  size = 0;
  if (!pid || wanted.empty())
    return false;

  HANDLE snap =
      CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
  if (snap == INVALID_HANDLE_VALUE)
    return false;

  MODULEENTRY32W entry{};
  entry.dwSize = sizeof(entry);
  bool found = false;
  if (Module32FirstW(snap, &entry)) {
    do {
      if (_wcsicmp(entry.szModule, wanted.c_str()) == 0) {
        base = reinterpret_cast<uint64_t>(entry.modBaseAddr);
        size = entry.modBaseSize;
        found = true;
        break;
      }
    } while (Module32NextW(snap, &entry));
  }
  CloseHandle(snap);
  return found;
}
} // namespace

uint64_t Memory::GetModuleBase(const std::string &moduleName) {
  uint64_t base = 0;
  uint64_t size = 0;
  std::wstring wideName(moduleName.begin(), moduleName.end());
  FindModule(wideName, base, size);

  if (base == 0 && ToolhelpModule(m_processId, wideName, base, size)) {
    printf("[driver] module '%s' resolved via Toolhelp (base=0x%llX)\n",
           moduleName.c_str(), (unsigned long long)base);
    return base;
  }

  if (base == 0) {
    static std::string lastMissing;
    if (lastMissing != moduleName) {
      lastMissing = moduleName;
      printf("[driver] module '%s' not found in PEB list or Toolhelp\n",
             moduleName.c_str());

      // Print a compact name sample so a malformed PEB walk is obvious.
      std::vector<ModInfo> modules;
      if (EnumModules(modules)) {
        int shown = 0;
        std::printf("[driver] PEB module sample (%zu total):", modules.size());
        for (const ModInfo &module : modules) {
          const std::string name(module.name.begin(), module.name.end());
          if (name.find("GameAssembly") != std::string::npos ||
              name.find("Unity") != std::string::npos ||
              name.find("Rust") != std::string::npos) {
            std::printf(" %s", name.c_str());
            ++shown;
          }
        }
        if (!shown) {
          int printed = 0;
          for (const ModInfo &module : modules) {
            const std::string name(module.name.begin(), module.name.end());
            std::printf(" %s", name.c_str());
            if (++printed >= 24)
              break;
          }
        }
        std::printf("\n");
        std::fflush(stdout);
      }
    }
  }
  return base;
}

uint64_t Memory::GetModuleSize(const std::string &moduleName) {
  uint64_t base = 0;
  uint64_t size = 0;
  std::wstring wideName(moduleName.begin(), moduleName.end());
  FindModule(wideName, base, size);
  if (base == 0)
    ToolhelpModule(m_processId, wideName, base, size);
  return size;
}

std::vector<std::string> Memory::GetLoadedModules() {
  std::vector<std::string> modules;

  std::vector<ModInfo> found;
  if (EnumModules(found)) {
    for (const ModInfo &module : found)
      modules.push_back(std::string(module.name.begin(), module.name.end()));
  }

  return modules;
}

// ── reading ─────────────────────────────────────────────────────────────────
bool Memory::ReadBool(uint64_t address) { return Read<bool>(address); }
int8_t Memory::ReadInt8(uint64_t address) { return Read<int8_t>(address); }
int16_t Memory::ReadInt16(uint64_t address) { return Read<int16_t>(address); }
int32_t Memory::ReadInt32(uint64_t address) { return Read<int32_t>(address); }
int64_t Memory::ReadInt64(uint64_t address) { return Read<int64_t>(address); }
uint8_t Memory::ReadUInt8(uint64_t address) { return Read<uint8_t>(address); }
uint16_t Memory::ReadUInt16(uint64_t address) {
  return Read<uint16_t>(address);
}
uint32_t Memory::ReadUInt32(uint64_t address) {
  return Read<uint32_t>(address);
}
uint64_t Memory::ReadUInt64(uint64_t address) {
  return Read<uint64_t>(address);
}
float Memory::ReadFloat(uint64_t address) { return Read<float>(address); }
double Memory::ReadDouble(uint64_t address) { return Read<double>(address); }

std::vector<uint8_t> Memory::ReadBytes(uint64_t address, size_t size) {
  std::vector<uint8_t> buffer(size);
  if (size == 0)
    return buffer;

  if (RawRead(address, buffer.data(), size))
    return buffer;

  return std::vector<uint8_t>();
}

bool Memory::ReadBytesInto(uint64_t address, void *buffer, size_t size) {
  return RawRead(address, buffer, size);
}

std::string Memory::ReadString(uint64_t address, size_t maxLength) {
  if (maxLength == 0)
    return std::string();

  std::vector<char> buffer(maxLength);
  if (RawRead(address, buffer.data(), maxLength)) {
    for (size_t i = 0; i < maxLength; i++) {
      if (buffer[i] == '\0')
        return std::string(buffer.data(), i);
    }
    return std::string(buffer.data(), maxLength);
  }

  return std::string();
}

std::wstring Memory::ReadWString(uint64_t address, size_t maxLength) {
  if (maxLength == 0)
    return std::wstring();

  std::vector<wchar_t> buffer(maxLength);
  if (RawRead(address, buffer.data(), maxLength * sizeof(wchar_t))) {
    for (size_t i = 0; i < maxLength; i++) {
      if (buffer[i] == L'\0')
        return std::wstring(buffer.data(), i);
    }
    return std::wstring(buffer.data(), maxLength);
  }

  return std::wstring();
}

// ── writing ─────────────────────────────────────────────────────────────────
bool Memory::WriteBool(uint64_t address, bool value) {
  return Write<bool>(address, value);
}
bool Memory::WriteInt8(uint64_t address, int8_t value) {
  return Write<int8_t>(address, value);
}
bool Memory::WriteInt16(uint64_t address, int16_t value) {
  return Write<int16_t>(address, value);
}
bool Memory::WriteInt32(uint64_t address, int32_t value) {
  return Write<int32_t>(address, value);
}
bool Memory::WriteInt64(uint64_t address, int64_t value) {
  return Write<int64_t>(address, value);
}
bool Memory::WriteUInt8(uint64_t address, uint8_t value) {
  return Write<uint8_t>(address, value);
}
bool Memory::WriteUInt16(uint64_t address, uint16_t value) {
  return Write<uint16_t>(address, value);
}
bool Memory::WriteUInt32(uint64_t address, uint32_t value) {
  return Write<uint32_t>(address, value);
}
bool Memory::WriteUInt64(uint64_t address, uint64_t value) {
  return Write<uint64_t>(address, value);
}
bool Memory::WriteFloat(uint64_t address, float value) {
  return Write<float>(address, value);
}
bool Memory::WriteDouble(uint64_t address, double value) {
  return Write<double>(address, value);
}

bool Memory::WriteBytes(uint64_t address, const std::vector<uint8_t> &bytes) {
  if (bytes.empty())
    return false;
  return RawWrite(address, bytes.data(), bytes.size());
}

bool Memory::WriteString(uint64_t address, const std::string &str) {
  return RawWrite(address, str.c_str(), str.length() + 1);
}

bool Memory::WriteWString(uint64_t address, const std::wstring &wstr) {
  return RawWrite(address, wstr.c_str(), (wstr.length() + 1) * sizeof(wchar_t));
}

// ── pointer operations ──────────────────────────────────────────────────────
uint64_t Memory::ReadPointer(uint64_t address) {
#ifdef _WIN64
  return Read<uint64_t>(address);
#else
  return Read<uint32_t>(address);
#endif
}

uint64_t Memory::FollowPointerPath(uint64_t baseAddress,
                                   const std::vector<uint64_t> &offsets) {
  if (offsets.empty())
    return baseAddress;

  // For a chain {a, b, c} we want [[[base+a]+b]+c]. Deref each intermediate
  // (base+a), (that+b), then return the FINAL address (that+c) so the caller
  // decides whether to Read/Write it. Adding the offset first (instead of
  // dereferencing first) is the fix vs. the old version.
  uint64_t address = baseAddress;
  for (size_t i = 0; i + 1 < offsets.size(); i++) {
    address = ReadPointer(address + offsets[i]);
    if (address == 0)
      return 0;
  }
  return address + offsets.back();
}
