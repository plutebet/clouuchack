
// ================================================================================
// Find more on @imguisource_bot | @ImGui_Source_FIXNEY | @il2cpp_src
// ================================================================================

#pragma once
#include <Windows.h>
#include <Psapi.h>
#include <TlHelp32.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#pragma comment(lib, "psapi.lib")

// ─────────────────────────────────────────────────────────────────────────────
//  Kernel-driver transport (topg / VexusShared shared-memory protocol).
//
//  The external no longer opens RustClient.exe and never calls
//  ReadProcessMemory / WriteProcessMemory. Every read and write is forwarded
//  through the shared section Global\VexusShared, which the loaded kernel
//  driver services with a CR3 walk. The public Memory interface is unchanged,
//  so no call site has to change.
//
//  Protocol (matches kernel main.c):
//    0    -> driver replies 88 (connected)
//    1    = read           (PhysicalReadProcessMemory via CR3 walk)
//    2    = driver complete reply
//    3    = write          (PhysicalWriteProcessMemory via CR3 walk)
//    5    = get PEB address (written to Buffer, Size = 8, reply 2)
//    99   = ping -> 100 (ready)
//    0x20 = driver failed the request
//  Buffer is kNiohBufferSize bytes; every transfer is chunked to that size.
//  Must match NIOH_MAX_RW in the kernel's physical_memory.h.
//  Request signalling: after publishing a request, usermode signals the auto-reset
//  event "Global\VexusWake" so the kernel can sleep on an idle channel.
// ─────────────────────────────────────────────────────────────────────────────
static constexpr size_t kNiohBufferSize = 1024;

struct NiohShmem {
  volatile ULONG Status;
  ULONG ProcessId;
  ULONGLONG Address;
  ULONG Size;
  volatile ULONG KernelAsleep; // kernel sets 1 while sleeping on the wake event
  UCHAR Buffer[kNiohBufferSize];
};

// The current topg driver keeps NIOH_SLOTS independent request slots so
// concurrent reader threads do not serialize in the kernel. The section must be
// mapped at the full array size even if this client only uses slot 0: the driver
// initializes and polls every slot on connect, and a one-slot mapping makes it
// write past the mapping (a kernel bugcheck).
static constexpr size_t kNiohSlots = 8;
struct NiohShmemArray {
  NiohShmem slots[kNiohSlots];
};

static_assert(sizeof(NiohShmem) == 24 + kNiohBufferSize,
              "NiohShmem layout must match the kernel's SHARED_MEMORY");
static_assert(offsetof(NiohShmem, Status) == 0, "protocol Status offset");
static_assert(offsetof(NiohShmem, ProcessId) == 4, "protocol ProcessId offset");
static_assert(offsetof(NiohShmem, Address) == 8, "protocol Address offset");
static_assert(offsetof(NiohShmem, Size) == 16, "protocol Size offset");
static_assert(offsetof(NiohShmem, KernelAsleep) == 20,
              "protocol KernelAsleep offset");
static_assert(offsetof(NiohShmem, Buffer) == 24, "protocol Buffer offset");
static_assert(sizeof(NiohShmemArray) == sizeof(NiohShmem) * kNiohSlots,
              "slot array must be tightly packed");

class Memory {
private:
  DWORD m_processId;
  std::string m_processName;
  bool m_isAttached;

  // Shared section with the kernel driver.
  HANDLE m_hMap = nullptr;
  HANDLE m_hWakeEvent = nullptr;
  NiohShmem *m_pMem = nullptr;

  // The kernel keeps 8 request slots, but this client uses ONE shared lane
  // (slot 0), exactly like the early smooth build. Letting four workers publish
  // requests into four slots keeps the kernel worker busy back-to-back and
  // costs the game real CPU; serializing through one mutex lets it go idle.
  // The stall protection lives in the bounded wait + in-flight gate below.
  struct Slot {
    NiohShmem *mem = nullptr;
    std::mutex mutex;
    std::atomic<bool> inFlight{false};
    ULONG inFlightStatus = 0;
    std::atomic<bool> stallLogged{false};
  };
  std::array<Slot, kNiohSlots> m_slots{};
  static thread_local int tls_slot;
  Slot &CurrentSlot();

  // A request that exceeded kDriverWaitMs is left in flight (Status untouched)
  // so the driver can still finish it safely. Reads then fail immediately
  // instead of blocking, until the driver drains it and the channel is reused.
  // Atomic so IsChannelStalled() can be polled off the reader threads.

  // Diagnostics surfaced in the menu: real driver round-trips, not guesses.
  std::atomic<uint64_t> m_readsOk{0};
  std::atomic<uint64_t> m_readsFail{0};
  std::atomic<uint64_t> m_writesOk{0};
  std::atomic<uint64_t> m_writesFail{0};

  struct ModInfo {
    std::wstring name;
    uint64_t base;
    uint64_t size;
  };

  // Process lookup (Toolhelp process list only - never opens the target).
  DWORD GetProcessIdByName(const std::string &processName);

  // Kernel-driver transport.
  bool DriverConnect();
  // Bounded wait: returns false when the driver did not answer within
  // kDriverWaitMs. The request is left in flight on purpose (see Rpm.cpp).
  bool DriverWait(Slot &slot, ULONG status);
  // Must be held with the slot mutex: false when a previously timed-out request
  // is still being serviced, so the caller must not publish another one.
  bool ChannelReady(Slot &slot);
  bool SubmitAndWait(Slot &slot, ULONG status);
  bool RawRead(uint64_t address, void *buffer, size_t size);
  bool RawWrite(uint64_t address, const void *buffer, size_t size);

  // Module list via a PEB loader-list walk performed through the driver.
  uint64_t GetPeb();
  bool ReadUnicodeString(uint64_t address, std::wstring &out, size_t maxChars);
  bool EnumModules(std::vector<ModInfo> &out);
  bool FindModule(const std::wstring &name, uint64_t &base, uint64_t &size);

public:
  Memory();
  ~Memory();

  // Selects the driver request lane for the calling thread. Default is slot 0
  // (entity/feature worker lane). The W2S updater selects slot 1 so the
  // view-matrix read is never queued behind the heavy entity/image stream.
  void SelectSlot(int index);

  // Process management
  bool Attach(const std::string &processName);
  bool AttachByPid(DWORD pid);
  void Detach();
  bool IsAttached() const { return m_isAttached; }
  bool IsProcessRunning();

  // True while the shared request lane has a timed-out request still being
  // serviced. This is the "channel is backed up" signal: it lets the W2S
  // updater skip expensive recovery work and the menu show the real state.
  bool IsChannelStalled() const {
    for (const Slot &slot : m_slots) {
      if (slot.inFlight.load(std::memory_order_acquire))
        return true;
    }
    return false;
  }

  uint64_t ReadsOk() const { return m_readsOk.load(std::memory_order_relaxed); }
  uint64_t ReadsFail() const {
    return m_readsFail.load(std::memory_order_relaxed);
  }
  uint64_t WritesOk() const {
    return m_writesOk.load(std::memory_order_relaxed);
  }
  uint64_t WritesFail() const {
    return m_writesFail.load(std::memory_order_relaxed);
  }

  // Getters
  DWORD GetProcessId() const { return m_processId; }
  std::string GetProcessName() const { return m_processName; }
  HANDLE GetProcessHandle() const { return nullptr; }  // driver lives in kernel

  // Module operations (resolved through the driver, not Toolhelp modules)
  uint64_t GetModuleBase(const std::string &moduleName);
  uint64_t GetModuleSize(const std::string &moduleName);
  std::vector<std::string> GetLoadedModules();

  // Memory reading (driver-backed)
  template <typename T> T Read(uint64_t address);

  bool ReadBool(uint64_t address);
  int8_t ReadInt8(uint64_t address);
  int16_t ReadInt16(uint64_t address);
  int32_t ReadInt32(uint64_t address);
  int64_t ReadInt64(uint64_t address);
  uint8_t ReadUInt8(uint64_t address);
  uint16_t ReadUInt16(uint64_t address);
  uint32_t ReadUInt32(uint64_t address);
  uint64_t ReadUInt64(uint64_t address);
  float ReadFloat(uint64_t address);
  double ReadDouble(uint64_t address);

  std::vector<uint8_t> ReadBytes(uint64_t address, size_t size);
  bool ReadBytesInto(uint64_t address, void *buffer, size_t size);
  std::string ReadString(uint64_t address, size_t maxLength = 256);
  std::wstring ReadWString(uint64_t address, size_t maxLength = 256);

  // Memory writing (driver-backed)
  template <typename T> bool Write(uint64_t address, T value);

  bool WriteBool(uint64_t address, bool value);
  bool WriteInt8(uint64_t address, int8_t value);
  bool WriteInt16(uint64_t address, int16_t value);
  bool WriteInt32(uint64_t address, int32_t value);
  bool WriteInt64(uint64_t address, int64_t value);
  bool WriteUInt8(uint64_t address, uint8_t value);
  bool WriteUInt16(uint64_t address, uint16_t value);
  bool WriteUInt32(uint64_t address, uint32_t value);
  bool WriteUInt64(uint64_t address, uint64_t value);
  bool WriteFloat(uint64_t address, float value);
  bool WriteDouble(uint64_t address, double value);

  bool WriteBytes(uint64_t address, const std::vector<uint8_t> &bytes);
  bool WriteString(uint64_t address, const std::string &str);
  bool WriteWString(uint64_t address, const std::wstring &wstr);

  // Pointer operations
  uint64_t ReadPointer(uint64_t address);
  uint64_t FollowPointerPath(uint64_t baseAddress,
                             const std::vector<uint64_t> &offsets);

  template <typename T>
  T ReadPointerChain(uint64_t baseAddress,
                     const std::vector<uint64_t> &offsets);
};

// Global instance
extern Memory *Driver;

// Template implementations
template <typename T> T Memory::Read(uint64_t address) {
  T buffer{};
  if (!address)
    return buffer;
  RawRead(address, &buffer, sizeof(T));
  return buffer;
}

template <typename T> bool Memory::Write(uint64_t address, T value) {
  if (!address)
    return false;
  return RawWrite(address, &value, sizeof(T));
}

template <typename T>
T Memory::ReadPointerChain(uint64_t baseAddress,
                           const std::vector<uint64_t> &offsets) {
  uint64_t address = FollowPointerPath(baseAddress, offsets);
  if (address != 0) {
    return Read<T>(address);
  }
  return T{};
}
