
// ================================================================================
// Telegram: @imguisource_bot | Join ImGui Source (t.me/ImGui_Source_FIXNEY) & shit src 🧨 (t.me/il2cpp_src)
// ================================================================================

#include "Entities.h"
#include "../Driver/Rpm.h"
#include "../Offsets/Offsets.h"
#include "../Il2Cpp/Decryptions/decryption.h"
#include "../Il2Cpp/Decryptions/decryptor.hpp"
#include "../Menu/Config.h"
#include "../Diag/Profiler.h"
#include "GhostClock.h"
#include "Prefabs/Prefabs.h"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <unordered_set>

extern uint64_t g_gameBase;

// Re-read the entity-pointer array at least this often even when the buffer
// pointer and count look unchanged (catches in-place replacements). The array
// is detected immediately when its pointer/count changes, so this slow in-place
// sweep can be rare: a large value removes the periodic multi-ms chunk burst
// (it used to run every 16 ticks and briefly freeze every entity position).
static constexpr uint32_t kListRefreshTicks = 64;
// Init for threading
BaseNetworkable::BaseNetworkable(uintptr_t gameAssembly)
    : gameAssemblyBase(gameAssembly), m_entitiesPtr(0), m_localPlayer(0),
      m_lastEntitiesPtr(0) {
    m_worker.Start([this] {
      if (Driver)
        Driver->SelectSlot(0);
      Tick();
    }, std::chrono::milliseconds(8));
    m_skeletonWorker.Start([this] {
      if (Driver)
        Driver->SelectSlot(2);
      SkeletonTick();
    }, std::chrono::milliseconds(8));
    printf("[+] Entity Thread started (8ms tick)\n");
    printf("[+] Skeleton Thread started (8ms tick, driver lane 2)\n");
}

// Dedicated skeleton worker. It consumes the latest entity snapshot, reads
// bones on its own driver lane, and publishes only the bone data. The entity
// thread never waits for a skeleton read, so names/boxes stay realtime.
void BaseNetworkable::SkeletonTick() {
  prof::ScopeTimer _skeletonTimer(&prof::skeletonTick);
  if (!g_cfg || !g_cfg->esp_enabled ||
      !g_cfg->Cat(Prefabs::Category::Player).enabled ||
      !g_cfg->Cat(Prefabs::Category::Player).draw_skeleton)
    return;

  auto snapshot = Snapshot();
  if (!snapshot || snapshot->empty())
    return;

  const uintptr_t local = GetLocalPlayer();
  Vector3 origin{};
  bool haveOrigin = GetLocalPlayerPos(origin);
  if (!haveOrigin && local) {
    for (const auto &e : *snapshot) {
      if (e.address == local && !e.position.Empty()) {
        origin = e.position;
        haveOrigin = true;
        break;
      }
    }
  }
  if (!haveOrigin)
    return;

  if (m_skeletonCaches.size() > 256)
    m_skeletonCaches.clear();

  skeleton::BeginTick();
  const double now = GhostNowMs();

  // Fair scheduling: collect every player whose skeleton is due, then refresh
  // the most overdue ones first. The worker runs every 8 ms; with six updates
  // per tick a normal near-player set updates at 60-125 Hz, which is what makes
  // the pose follow turns/aiming instead of stepping a few times a second.
  // Collecting the candidates once also avoids rescanning the whole snapshot
  // once per update.
  struct Candidate {
    uintptr_t address;
    Vector3 position;
    float distance;
    float speed;
    double overdue;
  };
  static thread_local std::vector<Candidate> candidates;
  candidates.clear();

  for (const auto &e : *snapshot) {
    if (!e.address || e.address == local)
      continue;
    if (e.category != static_cast<int>(Prefabs::Category::Player) || !e.alive)
      continue;
    if (g_cfg->hide_sleepers && e.sleeping)
      continue;

    const float distance = e.position.Distance(origin);
    if (distance > skeleton::kMaxDistance)
      continue;

    skeleton::Cache &cache = m_skeletonCaches[e.address];
    const float speed = e.velocity.Length();
    if (!skeleton::Due(cache, distance, speed, now))
      continue;
    const double overdue =
        cache.lastUpdateMs > 0.0 ? now - cache.lastUpdateMs : 1.0e30;
    candidates.push_back({e.address, e.position, distance, speed, overdue});
  }

  std::sort(candidates.begin(), candidates.end(),
            [](const Candidate &a, const Candidate &b) {
              return a.overdue > b.overdue;
            });

  const int count =
      std::min<int>(skeleton::kMaxUpdatesPerTick, (int)candidates.size());
  for (int i = 0; i < count; ++i) {
    const Candidate &c = candidates[i];
    skeleton::Cache &cache = m_skeletonCaches[c.address];
    if (skeleton::Update(c.address, c.position, c.distance, c.speed, now,
                         cache)) {
      skeleton::Publish(c.address, cache.data);
      prof::skeletonUpdates.fetch_add(1, std::memory_order_relaxed);
    }
  }
}

// Throttled chain debug so the console shows every failing step.
static bool DbgDue() {
    using namespace std::chrono;
    static steady_clock::time_point last{};
    const auto now = steady_clock::now();
    if (now - last < seconds(2))
        return false;
    last = now;
    return true;
}

uint64_t g_imageBase = 0;

static bool SanePtr(uint64_t p) {
    return p > 0x10000 && p < 0x00007FFFFFF00000ull && (p & 7) == 0;
}

// Walk the FULL encrypted entity chain on a candidate il2cpp image base:
// klass -> static_fields -> clientEntities -> decrypt(handle@+0x18) ->
// GCHandle slab -> entityList -> decrypt -> ListDictionary -> BufferList ->
// count. A false positive here is practically impossible.
static bool ValidateBase(uint64_t base, bool verbose) {
    const uint64_t klass = Driver->Read<uint64_t>(
        base + BaseNetworkable_Static_Offsets::typeinfo);
    if (!SanePtr(klass))
        return false;
    const uint64_t sf = Driver->Read<uint64_t>(
        klass + BaseNetworkable_Static_Offsets::static_fields);
    if (!SanePtr(sf))
        return false;
    const uint64_t ce = Driver->Read<uint64_t>(
        sf + BaseNetworkable_Static_Offsets::clientEntities);
    if (!ce)
        return false;
    const uint64_t realm =
        Decryption::decrypt_client_entities(ce, g_gameBase);
    if (!SanePtr(realm))
        return false;
    const uint64_t el = Driver->Read<uint64_t>(
        realm + BaseNetworkable_EntityRealm_Offsets::entityList);
    if (!el)
        return false;
    const uint64_t dict = Decryption::decrypt_entity_list(el, g_gameBase);
    if (!SanePtr(dict))
        return false;
    const uint64_t vals =
        Driver->Read<uint64_t>(dict + System_ListDictionary_Offsets::vals);
    if (!SanePtr(vals))
        return false;
    // current build: ListDictionary::vals holds the BufferList directly
    const uint64_t hs = vals;
    const uint64_t buf =
        Driver->Read<uint64_t>(hs + System_BufferList_Offsets::buffer);
    const int32_t count =
        Driver->Read<int32_t>(hs + System_BufferList_Offsets::count);
    if (!SanePtr(buf) || count < 0 || count > 100000)
        return false;
    if (verbose)
        printf("[resolve] VALID base GA+0x%llX: klass=0x%llX sf=0x%llX "
               "ce=0x%llX realm=0x%llX dict=0x%llX buf=0x%llX count=%d\n",
               (unsigned long long)(base - g_gameBase),
               (unsigned long long)klass, (unsigned long long)sf,
               (unsigned long long)ce, (unsigned long long)realm,
               (unsigned long long)dict, (unsigned long long)buf, count);
    return true;
}

// Weaker check for scanning from the MAIN MENU (clientEntities is null there):
// several independent TypeInfo globals must all resolve to sane classes whose
// static_fields are sane pointers.
static bool ValidateBaseWeak(uint64_t base) {
    const uint64_t tis[] = {
        BaseNetworkable_Static_Offsets::typeinfo,  // 0x1110F4B8
        LocalPlayer_Static_Offsets::typeinfo,      // 0x11156110
        0x1110c348,  // SingletonComponentCameraMan
    };
    for (uint64_t ti : tis) {
        const uint64_t klass = Driver->Read<uint64_t>(base + ti);
        if (!SanePtr(klass))
            return false;
        const uint64_t sf = Driver->Read<uint64_t>(
            klass + BaseNetworkable_Static_Offsets::static_fields);
        if (!SanePtr(sf))
            return false;
    }
    return true;
}

void ResolveImageBase() {
    // The dump's own Rvas block says sizeOfImage 0x2411000 / stamp 0x6aa9d1b0,
    // but the live GameAssembly module is 0x12489000 / 0x6acdecbe - the dump
    // was taken from the DECRYPTED GameAssembly while the process maps the
    // PROTECTED one. So "GameAssembly + TypeInfo RVA" reads packed junk
    // (we saw klass=0xC00A06B9, sf=0x80150000). Scan the module for the page
    // offset where the dump's RVAs actually resolve.
    const uint64_t ga = g_gameBase;

    const uint32_t e_lfanew  = Driver->Read<uint32_t>(ga + 0x3C);
    const uint32_t moduleSize = Driver->Read<uint32_t>(ga + e_lfanew + 0x50);
    printf("[*] dump claims sizeOfImage=0x2411000, live module=0x%X - "
           "scanning for the real il2cpp image...\n",
           moduleSize);

    // raw window around the BaseNetworkableStatic TypeInfo RVA (the log
    // shows whether this region is packed junk or real data)
    printf("[*] raw @GA+0x1110F4B8:");
    for (int i = -2; i < 6; i++)
        printf(" %+d=0x%llX", i,
               (unsigned long long)Driver->Read<uint64_t>(
                   ga + 0x1110f4b8ull + (long long)i * 8));
    printf("\n");

    // ---------- DEEP PROBE (affine base scan ruled out) ----------
    auto q = [&](uint64_t a) -> uint64_t {
        return Driver->Read<uint64_t>(a);
    };
    auto dumpq = [&](const char* tag, uint64_t addr, int n) {
        printf("[probe] %s @0x%llX:", tag, (unsigned long long)addr);
        for (int i = 0; i < n; i++)
            printf(" %llX", (unsigned long long)q(addr + (uint64_t)i * 8));
        printf("\n");
    };

    const uint64_t P = q(ga + 0x1110f4b8);
    printf("[probe] P=*(GA+0x1110F4B8)=0x%llX (module+0x%llX)\n",
           (unsigned long long)P, (unsigned long long)(P - ga));
    dumpq("struct@P -0x10..+0x108", P - 0x10, 35);

    const uint64_t PB = q(ga + 0x1110f4c0);  // neighbor slot (+1)
    printf("[probe] *(GA+0x1110F4C0)=0x%llX\n", (unsigned long long)PB);
    dumpq("struct@PB -0x10..+0x38", PB - 0x10, 9);

    const uint64_t Q = q(P + 0xB8);
    printf("[probe] Q=*(P+0xB8)=0x%llX\n", (unsigned long long)Q);
    if (Q > 0x10000) {
        dumpq("struct@Q +0x00..", Q, 24);
        const uint64_t pageQ = Q & ~0x1FFFull;
        printf("[probe] Q as GCHandle?: page=0x%llX bitmap=0x%llX count=%u "
               "type=%u\n",
               (unsigned long long)pageQ, (unsigned long long)q(pageQ + 0x10),
               (unsigned)q(pageQ + 0x1C), (unsigned)q(pageQ + 0x20));
    }

    const uint64_t pageP = P & ~0x1FFFull;
    printf("[probe] P as GCHandle?: page=0x%llX bitmap=0x%llX count=%u "
           "type=%u\n",
           (unsigned long long)pageP, (unsigned long long)q(pageP + 0x10),
           (unsigned)q(pageP + 0x1C), (unsigned)q(pageP + 0x20));

    const uint64_t P0 = q(P);
    printf("[probe] *P=0x%llX (*P)+0xB8=0x%llX *(P+8)=0x%llX *(P+0x10)=0x%llX "
           "*(P+0x18)=0x%llX\n",
           (unsigned long long)P0, (unsigned long long)q(P0 + 0xB8),
           (unsigned long long)q(P + 8), (unsigned long long)q(P + 0x10),
           (unsigned long long)q(P + 0x18));

    printf("[probe] alt RVAs: *(GA+0x110F4B8)=0x%llX *(GA+0x10F4B8)=0x%llX "
           "*(GA+0xF4B8)=0x%llX *(GA+0x8ED000+0x1110F4B8)=0x%llX "
           "*(GA+0x10BE9150 old)=0x%llX\n",
           (unsigned long long)q(ga + 0x110f4b8),
           (unsigned long long)q(ga + 0x10f4b8),
           (unsigned long long)q(ga + 0xf4b8),
           (unsigned long long)q(ga + 0x8ed000ull + 0x1110f4b8),
           (unsigned long long)q(ga + 0x10be9150ull));
    printf("[probe] LP: *(GA+0x11156110)=0x%llX *(GA+0x156110)=0x%llX "
           "*(GA+0x56110)=0x%llX\n",
           (unsigned long long)q(ga + 0x11156110),
           (unsigned long long)q(ga + 0x156110),
           (unsigned long long)q(ga + 0x56110));
    printf("[probe] TypeManager@GA+0x221D020: %llX %llX %llX %llX | "
           "ToObjectSet@GA+0x221CFF8: %llX %llX %llX %llX\n",
           (unsigned long long)q(ga + 0x221d020),
           (unsigned long long)q(ga + 0x221d028),
           (unsigned long long)q(ga + 0x221d030),
           (unsigned long long)q(ga + 0x221d038),
           (unsigned long long)q(ga + 0x221cff8),
           (unsigned long long)q(ga + 0x221d000),
           (unsigned long long)q(ga + 0x221d008),
           (unsigned long long)q(ga + 0x221d010));

    g_imageBase = ga;
    printf("[*] probe mode: image base = raw GameAssembly 0x%llX\n",
           (unsigned long long)ga);
    return;

    // the largest TypeInfo RVA we use must stay inside the module
    const uint64_t maxRva = 0x112a1590;  // BufferList TypeInfo (dump max)
    if (moduleSize <= maxRva + 0x1000) {
        printf("[!] module smaller than dump RVAs - cannot scan\n");
        g_imageBase = ga;
        return;
    }
    const uint64_t maxOff = (uint64_t)moduleSize - maxRva - 0x1000;

    auto scan = [&](uint64_t stride, const char* tag) -> uint64_t {
        uint64_t survivors = 0;
        uint64_t weak       = 0;
        uint64_t weakOff    = 0;
        for (uint64_t off = 0; off <= maxOff; off += stride) {
            if (off && !(off & 0xFFFFFF))  // progress every 16MB
                printf("[*] %s scan 0x%llX / 0x%llX\n", tag,
                       (unsigned long long)off, (unsigned long long)maxOff);

            // quick filter 1: BaseNetworkableStatic klass must be a pointer
            const uint64_t klass = Driver->Read<uint64_t>(
                ga + off + BaseNetworkable_Static_Offsets::typeinfo);
            if (!SanePtr(klass))
                continue;
            // quick filter 2: LocalPlayerStatic klass at the same base too
            const uint64_t klass2 = Driver->Read<uint64_t>(
                ga + off + LocalPlayer_Static_Offsets::typeinfo);
            if (!SanePtr(klass2))
                continue;

            survivors++;
            if (ValidateBase(ga + off, true)) {
                printf("[+] image base offset FOUND: GA + 0x%llX (%s)\n",
                       (unsigned long long)off, tag);
                return off;
            }
            // remember a menu-safe (weak) candidate
            if (!weak && ValidateBaseWeak(ga + off)) {
                weak    = 1;
                weakOff = off;
                printf("[*] weak candidate at GA + 0x%llX (no entities - "
                       "in menu?) klass=0x%llX\n",
                       (unsigned long long)off, (unsigned long long)klass);
            }
        }
        printf("[*] %s scan done: survivors=%llu weak=%llu\n", tag,
               (unsigned long long)survivors, (unsigned long long)weak);
        if (weak) {
            printf("[*] latching weak candidate GA + 0x%llX - join a server "
                   "and rerun for full validation\n",
                   (unsigned long long)weakOff);
            return weakOff;
        }
        return (uint64_t)-1;
    };

    // VirtualAlloc granularity is 64K - try that first (fast), then 4K
    uint64_t off = scan(0x10000, "coarse64K");
    if (off == (uint64_t)-1)
        off = scan(0x1000, "fine4K");

    if (off != (uint64_t)-1) {
        g_imageBase = ga + off;
    } else {
        printf("[!] no offset resolves the dump's TypeInfo RVAs - keeping raw "
               "GameAssembly base\n");
        g_imageBase = ga;
    }
    printf("[*] il2cpp image base = 0x%llX (GameAssembly 0x%llX + 0x%llX)\n",
           (unsigned long long)g_imageBase, (unsigned long long)ga,
           (unsigned long long)(g_imageBase - ga));
}

// ── Class resolver ───────────────────────────────────────────────────────────
// The dump's TypeInfo RVAs are per-build; the previous dump's RVAs pointed at
// stale globals (reading GameAssembly+0x1110F4B8 yielded an in-module
// pointer, not a heap class). Current-build values (dump 2026-10-13, GA
// stamp 0x6ACDECBE == live module) are tried FIRST:
//  0. direct pass  - klass = *(GA + typeinfo RVA), validated through the
//     real entity chain / transform chain before it is latched.
// Fallback (stale dump / menu), classes are found structurally:
//  1. name pass  - the TypeInfo pointer table in .data holds Il2CppClass*
//     (heap); klass+0x10 is the class name string (decrypted onto the heap
//     once the game has touched the class).
//  2. chain pass - names of untouched classes are still encrypted-at-rest, so
//     BaseNetworkable is identified by walking the REAL entity chain
//     (clientEntities -> decrypt_client_entities -> entityList ->
//     decrypt_entity_list -> ListDictionary -> BufferList)
//     with the decryptions' decryptors. LocalPlayer via its static
//     Entity field (sf+0x218, handle-encrypted) + transform-chain position
//     validation.
uint64_t g_bnKlass  = 0;  // BaseNetworkable
uint64_t g_lpKlass  = 0;  // LocalPlayer
uint64_t g_camKlass = 0;  // MainCamera
uint64_t g_camField = MainCamera_Offsets::mainCamera;  // static slot of Camera
bool g_camFieldLocked = false;  // set once the camera slot is confirmed
static std::vector<uint64_t> g_tableCandidates;  // TypeInfo table entries
static size_t g_lpIdx = 0;  // LocalPlayer's index in g_tableCandidates
static size_t g_bnIdx = 0;  // BaseNetworkable's index (cluster hint)

static bool SaneHeap(uint64_t p) {
    return p > 0x100000 && p < 0x00007FFFFFFFFFF8ull && (p & 7) == 0 &&
           (p >> 44) != 0x7FF;  // exclude module/image pointers (0x7FF....)
}

// Read that survives noaccess holes: full read, else page-by-page with the
// failed pages zeroed (ReadProcessMemory fails the whole call otherwise).
static bool ReadChunkRobust(uint64_t addr, void* buf, size_t size) {
    if (Driver->ReadBytesInto(addr, buf, size))
        return true;
    memset(buf, 0, size);
    auto* out = static_cast<uint8_t*>(buf);
    bool any = false;
    for (size_t off = 0; off < size; off += 0x1000) {
        const size_t n = (size - off < 0x1000) ? (size - off) : 0x1000;
        if (Driver->ReadBytesInto(addr + off, out + off, n))
            any = true;
    }
    return any;
}

// 16-byte stepwise string read so a page boundary mid-string can't fail it
static bool ReadStringRobust(uint64_t np, char* out, size_t cap) {
    size_t len = 0;
    char tmp[16];
    while (len + 1 < cap) {
        const size_t n = (cap - 1 - len < 16) ? (cap - 1 - len) : 16;
        if (!Driver->ReadBytesInto(np + len, tmp, n))
            break;
        for (size_t i = 0; i < n; i++) {
            if (!tmp[i]) {
                out[len] = '\0';
                return len > 0;
            }
            out[len++] = tmp[i];
        }
    }
    out[len] = '\0';
    return false;
}

// Il2CppClass::name sits at +0x10 and points to a readable C-string
static bool ReadKlassName(uint64_t klass, char* out, size_t cap) {
    const uint64_t np = Driver->Read<uintptr_t>(klass + 0x10);
    if (np < 0x10000 || np >= 0x00007FFFFFFFFFF8ull)
        return false;
    if (!ReadStringRobust(np, out, cap))
        return false;
    const size_t len = strlen(out);
    if (len < 2 || len > 60)
        return false;
    for (size_t i = 0; i < len; i++) {
        const char c = out[i];
        const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                        (c >= '0' && c <= '9') || c == '_' || c == '`' ||
                        c == '<' || c == '>' || c == '.' || c == '+';
        if (!ok)
            return false;
    }
    return true;
}

// Sane world position via the Unity transform chain (same as GetPosition)
static bool SaneTransformPos(uint64_t entity) {
    const uint64_t nat =
        Driver->Read<uint64_t>(entity + Object_Offsets::m_CachedPtr);
    if (!SaneHeap(nat))
        return false;
    const uint64_t go =
        Driver->Read<uint64_t>(nat + Unity_Component_Native::m_GameObject);
    if (!SaneHeap(go))
        return false;
    const uint64_t comp =
        Driver->Read<uint64_t>(go + Unity_GameObject_Native::m_Component);
    if (!SaneHeap(comp))
        return false;
    const uint64_t tr = Driver->Read<uint64_t>(comp + 0x08);
    if (!SaneHeap(tr))
        return false;
    const uint64_t hier =
        Driver->Read<uint64_t>(tr + Unity_Transform_Native::m_Hierarchy);
    if (!SaneHeap(hier))
        return false;
    const Vector3 pos = Driver->Read<Vector3>(
        hier + Unity_TransformHierarchy_Native::m_LocalPosition);
    return pos.x == pos.x && pos.y == pos.y && pos.z == pos.z &&  // finite
           pos.x > -10000.f && pos.x < 30000.f &&
           pos.y > -2000.f && pos.y < 10000.f &&
           pos.z > -10000.f && pos.z < 30000.f;
}

// Validates a klass as BaseNetworkable by walking the full entity chain
// exactly like GetClientEntities() does at runtime. `stages` (10 counters)
// records how far each candidate got, so the console shows exactly which
// link of the chain breaks on this build.
//   0 sf sane | 1 ce sane | 2 decrypt_client_entities | 3 el sane |
//   4 decrypt_entity_list | 5 bufferList sane (dict+vals, no ListHashSet
//   layer on this build) | 6 buffer+count sane | 7 ent0 sane |
//   8 ent0 positioned (diagnostic)
static bool ChainValidatesAsBN(uint64_t klass, uint64_t* stages) {
    const uint64_t sf = Driver->Read<uint64_t>(
        klass + BaseNetworkable_Static_Offsets::static_fields);
    if (!SaneHeap(sf))
        return false;
    stages[0]++;
    const uint64_t ce = Driver->Read<uint64_t>(
        sf + BaseNetworkable_Static_Offsets::clientEntities);
    if (!SaneHeap(ce))
        return false;
    stages[1]++;
    const uint64_t realm = Decryption::decrypt_client_entities(ce, g_gameBase);
    if (!realm || realm < 0x10000)
        return false;
    stages[2]++;
    const uint64_t el = Driver->Read<uint64_t>(
        realm + BaseNetworkable_EntityRealm_Offsets::entityList);
    if (!el || el < 0x10000)
        return false;
    stages[3]++;
    const uint64_t dict = Decryption::decrypt_entity_list(el, g_gameBase);
    if (!dict || dict < 0x10000)
        return false;
    stages[4]++;
    // current build: ListDictionary::vals IS the BufferList directly
    const uint64_t bufferList =
        Driver->Read<uint64_t>(dict + System_ListDictionary_Offsets::vals);
    if (!SaneHeap(bufferList))
        return false;
    stages[5]++;
    const uint64_t buffer =
        Driver->Read<uint64_t>(bufferList + System_BufferList_Offsets::buffer);
    const int32_t count =
        Driver->Read<int32_t>(bufferList + System_BufferList_Offsets::count);
    if (!SaneHeap(buffer) || count <= 0 || count > 20000)
        return false;
    stages[6]++;
    // The two decrypts + sane list structure are already definitive; the
    // first entity only has to be a sane object (it is not always a
    // positioned MonoBehaviour, so its transform is diagnostic only).
    // Array elements start at +0x28 on the 26480361 build
    // (klass+monitor+bounds+length header; dump: array_first = 0x28).
    const uint64_t ent0 = Driver->Read<uint64_t>(buffer + 0x28);
    if (!SaneHeap(ent0))
        return false;
    stages[7]++;
    if (SaneTransformPos(ent0))
        stages[8]++;
    return true;
}

// Collect TypeInfo-table candidates: whole module, chunk-density filtered
// (the table is nearly 100% heap pointers; .text is nearly 0%).
static void CollectCandidates(std::vector<uint64_t>& cands) {
    uint64_t moduleSize = Driver->GetModuleSize("GameAssembly.dll");
    if (!moduleSize)
        moduleSize = 0x13000000;
    const size_t chunkSize = 0x10000;  // 64KB
    std::vector<uint8_t> buffer(chunkSize);
    uint64_t denseBytes = 0;

    for (uint64_t off = 0; off < moduleSize; off += chunkSize) {
        if (!(off & 0x3FFFFFF))  // progress every 64MB
            printf("[*] dense scan 0x%llX / 0x%llX\n",
                   (unsigned long long)off, (unsigned long long)moduleSize);
        const size_t n = (moduleSize - off < chunkSize)
                             ? (size_t)(moduleSize - off)
                             : chunkSize;
        if (!ReadChunkRobust(g_gameBase + off, buffer.data(), n))
            continue;
        const size_t numPtrs = n / sizeof(uint64_t);
        const uint64_t* ptrs = reinterpret_cast<const uint64_t*>(buffer.data());
        size_t heap = 0;
        for (size_t i = 0; i < numPtrs; i++)
            if (SaneHeap(ptrs[i]))
                heap++;
        if (heap * 10 < numPtrs * 3)  // <30% heap ptrs -> not the table
            continue;
        denseBytes += n;
        for (size_t i = 0; i < numPtrs; i++)
            if (SaneHeap(ptrs[i]))
                cands.push_back(ptrs[i]);
        if (cands.size() > 6000000)
            break;
    }

    // dedupe but PRESERVE table order (metadata order - related classes
    // like BaseNetworkable/LocalPlayer cluster together)
    std::unordered_set<uint64_t> seen;
    seen.reserve(cands.size() * 2);
    std::vector<uint64_t> uniq;
    uniq.reserve(cands.size());
    for (uint64_t v : cands)
        if (seen.insert(v).second)
            uniq.push_back(v);
    cands.swap(uniq);

    printf("[*] dense TypeInfo regions: %llu bytes, candidates=%llu\n",
           (unsigned long long)denseBytes, (unsigned long long)cands.size());
}

// Slab/decrypt-path diagnostic + final status line, shared by both resolver
// paths (direct typeinfo and candidate scan).
static void FinishClassResolve() {
    // ── decrypt-path diagnostic (independent of BN) ────────────────────────
    // BasePlayer::eyes/inventory are handle-encrypted exactly like
    // clientEntities, so decrypting them through the found LP entity verifies
    // whether the GCHandle slab resolver still works on this build.
    if (g_lpKlass) {
        const uint64_t lpsf = Driver->Read<uint64_t>(
            g_lpKlass + LocalPlayer_Static_Offsets::static_fields);
        const uint64_t lpEntW =
            lpsf ? Driver->Read<uint64_t>(
                       lpsf + LocalPlayer_Static_Offsets::Entity)
                 : 0;
        const uint64_t lpEnt =
            lpEntW ? Decryption::decrypt_local_player(lpEntW, g_gameBase) : 0;
        if (SaneHeap(lpEnt)) {
            const uint64_t eyesEnc =
                Driver->Read<uint64_t>(lpEnt + BasePlayer_Offsets::eyes);
            const uint64_t eyesHandle =
                eyesEnc ? decryptor::read(eyesEnc + 0x18) : 0;
            const uint64_t eyes = decryptor::decrypt_eyes(eyesEnc);
            const uint64_t invEnc =
                Driver->Read<uint64_t>(lpEnt + BasePlayer_Offsets::inventory);
            const uint64_t inv = decryptor::decrypt_inventory(invEnc);
            printf("[diag] LP=0x%llX eyes enc=0x%llX handle=0x%llX -> 0x%llX "
                   "| inventory enc=0x%llX -> 0x%llX (slab %s)\n",
                   (unsigned long long)lpEnt, (unsigned long long)eyesEnc,
                   (unsigned long long)eyesHandle, (unsigned long long)eyes,
                   (unsigned long long)invEnc, (unsigned long long)inv,
                   (SaneHeap(eyes) || SaneHeap(inv)) ? "WORKS" : "BROKEN?");
        }
    }

    printf("[*] class resolve done: BN=%s LP=%s CAM=%s\n",
           g_bnKlass ? "OK" : "MISSING", g_lpKlass ? "OK" : "MISSING",
           g_camKlass ? "OK" : "MISSING");
}

// ── 0) direct TypeInfo fast path ───────────────────────────────────────────
// Current-build dump (unknowncheats #164256 p.1361, dumped 2026-10-13 from a
// running client, game_assembly_timestamp = 0x6ACDECBE == the live module):
// the TypeInfo globals hold Il2CppClass* directly. Read them and VALIDATE
// through the live chains - this skips the 700K-candidate scan when the dump
// matches the running build. Returns true only when all three confirmed.
static bool ResolveDirectTypeinfo() {
    const uint64_t bn = Driver->Read<uint64_t>(
        g_gameBase + BaseNetworkable_Static_Offsets::typeinfo);
    const uint64_t lp = Driver->Read<uint64_t>(
        g_gameBase + LocalPlayer_Static_Offsets::typeinfo);
    const uint64_t cam = Driver->Read<uint64_t>(
        g_gameBase + MainCamera_Offsets::typeinfo);
    printf("[ti] *(GA+0x%llX)=0x%llX *(GA+0x%llX)=0x%llX *(GA+0x%llX)=0x%llX\n",
           (unsigned long long)BaseNetworkable_Static_Offsets::typeinfo,
           (unsigned long long)bn,
           (unsigned long long)LocalPlayer_Static_Offsets::typeinfo,
           (unsigned long long)lp, (unsigned long long)MainCamera_Offsets::typeinfo,
           (unsigned long long)cam);

    // BaseNetworkable: the full chain (two decrypts + slab + list structure)
    // is definitive.
    if (SaneHeap(bn)) {
        uint64_t stages[10] = {};
        if (ChainValidatesAsBN(bn, stages)) {
            g_bnKlass = bn;
            printf("[+] BaseNetworkable (by typeinfo) klass=0x%llX sf=0x%llX\n",
                   (unsigned long long)bn,
                   (unsigned long long)Driver->Read<uint64_t>(
                       bn + BaseNetworkable_Static_Offsets::static_fields));
        } else {
            printf("[*] direct BN failed chain: sf=%llu ce=%llu dec1=%llu "
                   "el=%llu dec2=%llu bl=%llu buf=%llu ent0=%llu pos=%llu\n",
                   (unsigned long long)stages[0], (unsigned long long)stages[1],
                   (unsigned long long)stages[2], (unsigned long long)stages[3],
                   (unsigned long long)stages[4], (unsigned long long)stages[5],
                   (unsigned long long)stages[6], (unsigned long long)stages[7],
                   (unsigned long long)stages[8]);
            // In the main menu the entity list is empty, so the full chain
            // cannot validate even though the typeinfo class is correct. Latch
            // the class if its static_fields pointer is sane; the entity
            // thread will start reading entities as soon as a server loads.
            const uint64_t sf = Driver->Read<uint64_t>(
                bn + BaseNetworkable_Static_Offsets::static_fields);
            if (SaneHeap(sf)) {
                g_bnKlass = bn;
                printf("[+] BaseNetworkable (weak typeinfo, menu) "
                       "klass=0x%llX sf=0x%llX\n",
                       (unsigned long long)bn, (unsigned long long)sf);
            }
        }
    }

    // LocalPlayer: Entity is handle-encrypted at sf+0xD8 on this build. In the
    // menu the slot is empty; latch the class from typeinfo and let
    // ReadLocalPlayer() retry the entity slot after joining.
    if (SaneHeap(lp)) {
        const uint64_t sf = Driver->Read<uint64_t>(
            lp + LocalPlayer_Static_Offsets::static_fields);
        const uint64_t w =
            sf ? Driver->Read<uint64_t>(sf + LocalPlayer_Static_Offsets::Entity)
               : 0;
        const uint64_t e =
            w ? Decryption::decrypt_local_player(w, g_gameBase) : 0;
        if (SaneHeap(e) && SaneTransformPos(e)) {
            g_lpKlass = lp;
            printf("[+] LocalPlayer (by typeinfo) klass=0x%llX sf=0x%llX "
                   "entity=0x%llX\n",
                   (unsigned long long)lp, (unsigned long long)sf,
                   (unsigned long long)e);
        } else if (SaneHeap(sf)) {
            g_lpKlass = lp;
            printf("[+] LocalPlayer (weak typeinfo, menu) klass=0x%llX "
                   "sf=0x%llX\n",
                   (unsigned long long)lp, (unsigned long long)sf);
        } else {
            printf("[*] direct LP unconfirmed (sf=0x%llX w=0x%llX e=0x%llX)\n",
                   (unsigned long long)sf, (unsigned long long)w,
                   (unsigned long long)e);
        }
    }

    // MainCamera: only trust its typeinfo once the BN chain confirmed the
    // dump matches the live build (a wrong global would lock out the name
    // pass); the static field slot is probed/latched by W2S itself.
    if (SaneHeap(cam) && g_bnKlass) {
        const uint64_t sf = Driver->Read<uint64_t>(
            cam + MainCamera_Offsets::static_fields);
        char cname[64] = {};
        ReadKlassName(cam, cname, sizeof(cname));
        printf("[ti] MainCamera klass name=\"%s\"\n", cname);
        if (SaneHeap(sf)) {
            printf("[ti] MainCamera sf=0x%llX slots 0x40..0x98:",
                   (unsigned long long)sf);
            for (uint64_t o = 0x40; o <= 0x98; o += 8)
                printf(" %llX:0x%llX", (unsigned long long)o,
                       (unsigned long long)Driver->Read<uint64_t>(sf + o));
            printf("\n");
            g_camKlass = cam;
            printf("[+] MainCamera (by typeinfo) klass=0x%llX sf=0x%llX "
                   "field=0x%llX (probed by W2S)\n",
                   (unsigned long long)cam, (unsigned long long)sf,
                   (unsigned long long)MainCamera_Offsets::mainCamera);
        } else {
            printf("[*] direct MainCamera klass sane but sf=0x%llX\n",
                   (unsigned long long)sf);
        }
    } else if (SaneHeap(cam)) {
        printf("[*] direct MainCamera not latched (BN chain unconfirmed)\n");
    }

    return g_bnKlass && g_lpKlass && g_camKlass;
}

void ResolveClasses() {
    if (ResolveDirectTypeinfo()) {
        // all three confirmed directly from the current-build typeinfo
        // globals - skip the candidate scan entirely
        FinishClassResolve();
        return;
    }

    // The LocalPlayer static slot is empty in the main menu. If the direct
    // typeinfo pass already confirmed BaseNetworkable and MainCamera, do not
    // fall into the 1.3M-candidate scan just for LocalPlayer; ReadLocalPlayer()
    // retries that one slot cheaply once the game joins a server.
    if (g_bnKlass && g_camKlass) {
        printf("[*] BN/Camera resolved directly; LocalPlayer will latch on "
               "join\n");
        FinishClassResolve();
        return;
    }

    CollectCandidates(g_tableCandidates);
    std::vector<uint64_t>& cands = g_tableCandidates;
    if (cands.empty()) {
        printf("[!] no TypeInfo-like regions found in GameAssembly\n");
        FinishClassResolve();
        return;
    }

    // ── 1) name pass ────────────────────────────────────────────────────────
    char name[64];
    uint64_t readable = 0;
    for (size_t i = 0; i < cands.size(); i++) {
        if (g_bnKlass && g_lpKlass && g_camKlass)
            break;
        const uint64_t klass = cands[i];
        if (!ReadKlassName(klass, name, sizeof(name)))
            continue;
        readable++;
        if (!g_bnKlass && !strcmp(name, "BaseNetworkable")) {
            g_bnKlass = klass;
            printf("[+] BaseNetworkable (by name) klass=0x%llX sf=0x%llX\n",
                   (unsigned long long)klass,
                   (unsigned long long)Driver->Read<uint64_t>(
                       klass + BaseNetworkable_Static_Offsets::static_fields));
        } else if (!g_lpKlass && !strcmp(name, "LocalPlayer")) {
            g_lpKlass = klass;
            printf("[+] LocalPlayer (by name) klass=0x%llX sf=0x%llX\n",
                   (unsigned long long)klass,
                   (unsigned long long)Driver->Read<uint64_t>(
                       klass + LocalPlayer_Static_Offsets::static_fields));
        } else if (!g_camKlass && !strcmp(name, "MainCamera")) {
            g_camKlass = klass;
            printf("[+] MainCamera (by name) klass=0x%llX sf=0x%llX\n",
                   (unsigned long long)klass,
                   (unsigned long long)Driver->Read<uint64_t>(
                       klass + MainCamera_Offsets::static_fields));
        }
    }
    printf("[*] name pass: %llu readable class names\n",
           (unsigned long long)readable);

    // table neighborhood of a known Assembly-CSharp class (same metadata
    // order as the dump - BaseNetworkable sits near MainCamera/LocalPlayer)
    size_t anchorIdx = 0;
    bool haveAnchor = false;
    const uint64_t anchor = g_camKlass ? g_camKlass : g_bnKlass;
    if (anchor) {
        for (size_t i = 0; i < cands.size(); i++) {
            if (cands[i] == anchor) {
                anchorIdx   = i;
                haveAnchor  = true;
                break;
            }
        }
    }

    // ── 2) chain pass: BaseNetworkable ──────────────────────────────────────
    // Names of untouched classes are still encrypted at rest, so identify BN
    // by walking the real entity chain with the decryptions.txt decryptors.
    if (!g_bnKlass) {
        printf("[*] BN name unreadable - validating candidates via entity "
               "chain...\n");
        uint64_t stages[10] = {};
        auto tryBN = [&](size_t i) -> bool {
            const uint64_t klass = cands[i];
            if (ChainValidatesAsBN(klass, stages)) {
                g_bnKlass = klass;
                g_bnIdx   = i;
                printf("[+] BaseNetworkable (by chain) klass=0x%llX "
                       "sf=0x%llX [cand #%llu]\n",
                       (unsigned long long)klass,
                       (unsigned long long)Driver->Read<uint64_t>(
                           klass +
                           BaseNetworkable_Static_Offsets::static_fields),
                       (unsigned long long)i);
                return true;
            }
            return false;
        };
        bool found = false;
        if (haveAnchor) {  // same-assembly cluster first
            const size_t lo = (anchorIdx > 3000) ? anchorIdx - 3000 : 0;
            const size_t hi = (anchorIdx + 9000 < cands.size())
                                  ? anchorIdx + 9000
                                  : cands.size();
            for (size_t i = lo; i < hi && !found; i++) {
                if ((i & 0xFFF) == 0)
                    printf("[*] BN chain pass (cluster) %llu / %llu\n",
                           (unsigned long long)i, (unsigned long long)hi);
                found = tryBN(i);
            }
        }
        for (size_t i = 0; i < cands.size() && !found; i++) {
            if ((i & 0x1FFFF) == 0)
                printf("[*] BN chain pass %llu / %llu\n",
                       (unsigned long long)i, (unsigned long long)cands.size());
            found = tryBN(i);
        }
        printf("[*] BN chain stages: sf=%llu ce=%llu dec1=%llu el=%llu "
               "dec2=%llu bl=%llu buf=%llu ent0=%llu pos=%llu\n",
               (unsigned long long)stages[0], (unsigned long long)stages[1],
               (unsigned long long)stages[2], (unsigned long long)stages[3],
               (unsigned long long)stages[4], (unsigned long long)stages[5],
               (unsigned long long)stages[6], (unsigned long long)stages[7],
               (unsigned long long)stages[8]);
        if (!found)
            printf("[!] BaseNetworkable NOT found by chain either\n");
    }

    // ── 3) chain pass: LocalPlayer ──────────────────────────────────────────
    // LocalPlayer static holds the local BasePlayer at +Entity (0x218),
    // handle-encrypted (decrypt_local_player); validate through the
    // transform chain (sane world position).
    if (!g_lpKlass) {
        printf("[*] searching LocalPlayer via sf+0x%llX entity chain...\n",
               (unsigned long long)LocalPlayer_Static_Offsets::Entity);
        auto tryLP = [&](size_t i) -> bool {
            const uint64_t klass = cands[i];
            if (klass == g_bnKlass)
                return false;
            const uint64_t sf = Driver->Read<uint64_t>(
                klass + LocalPlayer_Static_Offsets::static_fields);
            if (!SaneHeap(sf))
                return false;
            // current build: Entity (sf+0x218) is handle-encrypted
            const uint64_t w = Driver->Read<uint64_t>(
                sf + LocalPlayer_Static_Offsets::Entity);
            if (!SaneHeap(w))
                return false;
            const uint64_t e = Decryption::decrypt_local_player(w, g_gameBase);
            if (!SaneHeap(e))
                return false;
            if (!SaneTransformPos(e))
                return false;
            g_lpKlass = klass;
            g_lpIdx   = i;
            printf("[+] LocalPlayer (by chain) klass=0x%llX sf=0x%llX "
                   "entity=0x%llX [cand #%llu]\n",
                   (unsigned long long)klass, (unsigned long long)sf,
                   (unsigned long long)e, (unsigned long long)i);
            return true;
        };
        bool found = false;
        if (haveAnchor) {
            const size_t lo = (anchorIdx > 2000) ? anchorIdx - 2000 : 0;
            const size_t hi = (anchorIdx + 6000 < cands.size())
                                  ? anchorIdx + 6000
                                  : cands.size();
            for (size_t i = lo; i < hi && !found; i++)
                found = tryLP(i);
        }
        for (size_t i = 0; i < cands.size() && !found; i++) {
            if ((i & 0x1FFFF) == 0)
                printf("[*] LP chain pass %llu / %llu\n",
                       (unsigned long long)i, (unsigned long long)cands.size());
            found = tryLP(i);
        }
        if (!found)
            printf("[!] LocalPlayer NOT found - join a server and re-run "
                   "(statics are empty in menu)\n");
    }

    FinishClassResolve();
}

// MainCamera's name string is flaky (names stay encrypted-at-rest until the
// game touches the class) - retry the name pass from the entity thread:
// first around the LocalPlayer cluster (same assembly, cheap), then over the
// whole table.
void RetryCameraByName() {
    using namespace std::chrono;
    static steady_clock::time_point last{};
    static int attempts = 0;
    if (g_camKlass || g_tableCandidates.empty())
        return;
    const auto now = steady_clock::now();
    const auto gap = (attempts < 3) ? seconds(15) : seconds(60);
    if (now - last < gap)
        return;
    last = now;
    attempts++;

    char name[64];
    const size_t n = g_tableCandidates.size();
    size_t lo = 0, hi = n;
    const size_t anchorIdx = g_lpIdx ? g_lpIdx : g_bnIdx;
    if (anchorIdx && attempts < 3) {  // same-assembly cluster first
        lo = (anchorIdx > 20000) ? anchorIdx - 20000 : 0;
        hi = (anchorIdx + 20000 < n) ? anchorIdx + 20000 : n;
    }
    uint64_t checked = 0;
    for (size_t i = lo; i < hi && !g_camKlass; i++) {
        const uint64_t klass = g_tableCandidates[i];
        if (!ReadKlassName(klass, name, sizeof(name)))
            continue;
        checked++;
        if (!strcmp(name, "MainCamera")) {
            g_camKlass = klass;
            printf("[+] MainCamera (by name, retry #%d) klass=0x%llX "
                   "sf=0x%llX [cand #%llu]\n",
                   attempts, (unsigned long long)klass,
                   (unsigned long long)Driver->Read<uint64_t>(
                       klass + MainCamera_Offsets::static_fields),
                   (unsigned long long)i);
        }
    }
    if (!g_camKlass)
        printf("[*] camera name retry #%d: %llu names checked, not found "
               "yet\n",
               attempts, (unsigned long long)checked);
}

uintptr_t BaseNetworkable::GetInstance() {
    if (!g_bnKlass) {
        if (DbgDue())
            printf("[chain] BaseNetworkable class not resolved yet\n");
        return 0;
    }

    uintptr_t sf = Driver->Read<uintptr_t>(
        g_bnKlass + BaseNetworkable_Static_Offsets::static_fields);
    if (!sf || sf < 0x10000) {
        if (DbgDue())
            printf("[chain] bnKlass=0x%llX -> sf=0x%llX BAD\n",
                   (unsigned long long)g_bnKlass, (unsigned long long)sf);
        return 0;
    }
    return sf;
}

uintptr_t BaseNetworkable::GetClientEntities() {
    const bool dbg = DbgDue();

    uintptr_t instance = GetInstance();
    if (!instance)
        return 0;

    uintptr_t clientEntities = Driver->Read<uintptr_t>(
        instance + BaseNetworkable_Static_Offsets::clientEntities);
    if (!clientEntities) {
        if (dbg)
            printf("[chain] sf=0x%llX cePtr=0 (clientEntities+0x%llX empty)\n",
                   (unsigned long long)instance,
                   (unsigned long long)BaseNetworkable_Static_Offsets::clientEntities);
        return 0;
    }

    uintptr_t decryptedEntities =
        Decryption::decrypt_client_entities(clientEntities, g_gameBase);
    if (!decryptedEntities) {
        if (dbg)
            printf("[chain] cePtr=0x%llX -> realm=0 (decrypt_client_entities failed)\n",
                   (unsigned long long)clientEntities);
        return 0;
    }

    uintptr_t addr = Driver->Read<uintptr_t>(
        decryptedEntities + BaseNetworkable_EntityRealm_Offsets::entityList);
    if (!addr) {
        if (dbg)
            printf("[chain] realm=0x%llX elPtr=0\n",
                   (unsigned long long)decryptedEntities);
        return 0;
    }

    uintptr_t entPtr = Decryption::decrypt_entity_list(addr, g_gameBase);
    if (!entPtr) {
        if (dbg)
            printf("[chain] elPtr=0x%llX -> dict=0 (decrypt_entity_list failed)\n",
                   (unsigned long long)addr);
        return 0;
    }

    // current build: ListDictionary::vals holds the BufferList directly
    // (no ListHashSet layer on this build).
    uintptr_t bufferList =
        Driver->Read<uintptr_t>(entPtr + System_ListDictionary_Offsets::vals);
    if (!bufferList || bufferList < 10000) {
        if (dbg)
            printf("[chain] dict=0x%llX bufferList=0x%llX BAD\n",
                   (unsigned long long)entPtr, (unsigned long long)bufferList);
        return 0;
    }

    return bufferList;
}

const std::vector<uintptr_t> &BaseNetworkable::FetchEntityList() {
  static const std::vector<uintptr_t> kEmpty;

  uintptr_t BufferList = GetClientEntities();
  if (!BufferList)
    return kEmpty;

  uintptr_t EntityArray =
      Driver->Read<uintptr_t>(BufferList + System_BufferList_Offsets::buffer);
  int32_t count =
      Driver->Read<int32_t>(BufferList + System_BufferList_Offsets::count);

  if (!EntityArray || count <= 0 || count > 50000) {
    if (DbgDue())
      printf("[chain] buf=0x%llX arr=0x%llX count=%d BAD\n",
             (unsigned long long)BufferList,
             (unsigned long long)EntityArray, count);
    // Transient read failure: keep working off the last good array for a short
    // while instead of publishing an empty list. An empty list used to make
    // every entity miss its refresh at the same instant (the whole ESP blinked
    // out, then had to be rebuilt from scratch).
    if (!m_listCache.empty() && (m_tickSeq - m_listReadAt) < 32)
      return m_listCache;
    return kEmpty;
  }

  // current build: il2cpp array elements start at +0x28
  // (klass+monitor+bounds+length header; dump: array_first = 0x28).
  const int32_t arrLen = Driver->Read<int32_t>(EntityArray + 0x18);
  if (arrLen > 0 && count > arrLen)
    count = arrLen;

  // Re-use the cached pointer array while the buffer pointer and count are
  // unchanged. The game swap-removes entities in place (remove + add without
  // changing the count), so the array is also force-refreshed on a short
  // cadence: small lists every few ticks (only a couple of 128 B chunks),
  // huge maps backed off so the chunked transfer cannot spike the worker.
  const uint32_t refreshTicks = (count <= 768) ? 16u : kListRefreshTicks;
  if (m_listArrayPtr == EntityArray && m_listCount == count &&
      (m_tickSeq - m_listReadAt) < refreshTicks)
    return m_listCache;

  std::vector<uintptr_t> entities;
  entities.reserve((size_t)count);

  // Read the pointer array one driver chunk (128 B) at a time. Each chunk is
  // its own locked transfer, so the shared-memory mutex is released between
  // them and the W2S updater (and any other reader) can slip in instead of
  // stalling for the whole multi-KB transfer - that multi-ms stall was what
  // made the ESP "freeze" on big servers.
  std::vector<uintptr_t> raw((size_t)count);
  auto *dst = reinterpret_cast<uint8_t *>(raw.data());
  const size_t total = raw.size() * sizeof(uintptr_t);
  bool ok = true;
  for (size_t off = 0; off < total; off += kNiohBufferSize) {
    const size_t n = (total - off < kNiohBufferSize) ? (total - off) : kNiohBufferSize;
    if (!Driver->ReadBytesInto(EntityArray + 0x28 + off, dst + off, n)) {
      ok = false;
      break;
    }
  }

  if (ok) {
    for (int i = 0; i < count; i++)
      if (raw[i] >= 0x10000)
        entities.push_back(raw[i]);
  } else {
    // Partial read: avoid doing N separate driver round-trips which can stall
    // the worker for seconds on large servers. Keep the last good cached list
    // if it was read recently; otherwise return empty to avoid a blocking loop.
    if (!m_listCache.empty() && (m_tickSeq - m_listReadAt) < 32)
      return m_listCache;
    return kEmpty;
  }

  m_listCache = entities;
  m_listArrayPtr = EntityArray;
  m_listCount = count;
  m_listReadAt = m_tickSeq;
  return m_listCache;
}

struct CharBuf {
    char data[128];
};

std::string BaseNetworkable::GetObjectClassName(uintptr_t entity) {
    if (!entity || entity < 0x10000)
        return {};
    const auto klass = Driver->Read<uintptr_t>(entity);
    if (!klass || klass < 0x10000)
        return {};
    const auto namePtr =
        Driver->Read<uintptr_t>(klass + Object_Offsets::m_CachedPtr);
    if (!namePtr || namePtr < 0x10000)
        return {};

    CharBuf cb   = Driver->Read<CharBuf>(namePtr);
    cb.data[127] = '\0';
    char c0      = cb.data[0];
    if (!((c0 >= 'A' && c0 <= 'Z') || (c0 >= 'a' && c0 <= 'z')))
        return {};
    int len = 0;
    while (len < 127 && cb.data[len])
        ++len;
    if (len < 2 || len > 100)
        return {};
    return std::string(cb.data, len);
}

uint32_t BaseNetworkable::GetPrefabID(uintptr_t entity) {
    if (!entity || entity < 0x10000)
        return 0;
    return Driver->Read<uint32_t>(entity + BaseNetworkable_Offsets::prefabID);
}

Vector3 BaseNetworkable::GetPosition(uintptr_t entity) {
    uintptr_t nativeComponent =
        Driver->Read<uintptr_t>(entity + Object_Offsets::m_CachedPtr);
    if (!nativeComponent || nativeComponent < 0x10000)
        return {};

    uintptr_t gameObject = Driver->Read<uintptr_t>(
        nativeComponent + Unity_Component_Native::m_GameObject);
    if (!gameObject || gameObject < 0x10000)
        return {};

    uintptr_t componentData = Driver->Read<uintptr_t>(
        gameObject + Unity_GameObject_Native::m_Component);
    if (!componentData || componentData < 0x10000)
        return {};

    uintptr_t transform = Driver->Read<uintptr_t>(componentData + 0x08);
    if (!transform || transform < 0x10000)
        return {};

    uintptr_t hierarchy =
        Driver->Read<uintptr_t>(transform + Unity_Transform_Native::m_Hierarchy);
    if (!hierarchy || hierarchy < 0x10000)
        return {};

    return Driver->Read<Vector3>(
        hierarchy + Unity_TransformHierarchy_Native::m_LocalPosition);
}

// Resolve the native transform chain down to the transform hierarchy that
// holds the entity's position. Five chained reads; cached by the caller.
bool BaseNetworkable::ResolveHierarchy(uintptr_t entity, uintptr_t &hierarchy) {
    hierarchy = 0;
    if (entity < 0x10000)
        return false;
    const uintptr_t nativeComponent =
        Driver->Read<uintptr_t>(entity + Object_Offsets::m_CachedPtr);
    if (nativeComponent < 0x10000)
        return false;
    const uintptr_t gameObject = Driver->Read<uintptr_t>(
        nativeComponent + Unity_Component_Native::m_GameObject);
    if (gameObject < 0x10000)
        return false;
    const uintptr_t componentData = Driver->Read<uintptr_t>(
        gameObject + Unity_GameObject_Native::m_Component);
    if (componentData < 0x10000)
        return false;
    const uintptr_t transform = Driver->Read<uintptr_t>(componentData + 0x08);
    if (transform < 0x10000)
        return false;
    hierarchy = Driver->Read<uintptr_t>(
        transform + Unity_Transform_Native::m_Hierarchy);
    return hierarchy >= 0x10000;
}

// One driver read: the entity's local position from a cached hierarchy.
Vector3 BaseNetworkable::ReadPositionAt(uintptr_t hierarchy) {
    if (hierarchy < 0x10000)
        return {};
    return Driver->Read<Vector3>(
        hierarchy + Unity_TransformHierarchy_Native::m_LocalPosition);
}

// The direct LocalPlayer typeinfo is valid but its static Entity slot is empty
// until the game joins a server. Retry that one slot instead of running the
// full candidate scan.
static bool TryLatchLocalPlayerDirect() {
    if (g_lpKlass)
        return true;
    const uint64_t lp = Driver->Read<uint64_t>(
        g_gameBase + LocalPlayer_Static_Offsets::typeinfo);
    if (!SaneHeap(lp))
        return false;
    const uint64_t sf = Driver->Read<uint64_t>(
        lp + LocalPlayer_Static_Offsets::static_fields);
    if (!SaneHeap(sf))
        return false;
    const uint64_t wrapper = Driver->Read<uint64_t>(
        sf + LocalPlayer_Static_Offsets::Entity);
    if (!wrapper)
        return false;
    const uint64_t entity =
        Decryption::decrypt_local_player(wrapper, g_gameBase);
    if (!SaneHeap(entity) || !SaneTransformPos(entity))
        return false;
    g_lpKlass = lp;
    printf("[+] LocalPlayer (late typeinfo) klass=0x%llX sf=0x%llX "
           "entity=0x%llX\n",
           (unsigned long long)lp, (unsigned long long)sf,
           (unsigned long long)entity);
    return true;
}

uintptr_t BaseNetworkable::ReadLocalPlayer() {
    const bool dbg = DbgDue();

    if (!g_lpKlass && !TryLatchLocalPlayerDirect()) {
        if (dbg)
            printf("[local] LocalPlayer class not resolved yet\n");
        return 0;
    }

    uintptr_t sf = Driver->Read<uintptr_t>(
        g_lpKlass + LocalPlayer_Static_Offsets::static_fields);
    if (!sf || sf < 0x10000) {
        if (dbg)
            printf("[local] lpKlass=0x%llX -> sf=0x%llX BAD\n",
                   (unsigned long long)g_lpKlass, (unsigned long long)sf);
        return 0;
    }

    // current build: Entity (sf+0x218) is a handle-encrypted hidden<> value
    const uintptr_t wrapper = Driver->Read<uintptr_t>(
        sf + LocalPlayer_Static_Offsets::Entity);
    const uintptr_t entity =
        wrapper ? Decryption::decrypt_local_player(wrapper, g_gameBase) : 0;
    return entity;
}