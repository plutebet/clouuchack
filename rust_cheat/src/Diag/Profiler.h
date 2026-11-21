
// ================================================================================
// Community links:
// - @imguisource_bot
// - @ImGui_Source_FIXNEY (ImGui Source)
// - @il2cpp_src (shit src 🧨)
// ================================================================================

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>

// Lightweight always-on interval profiler. One compact block every 3 seconds:
// entity tick time + snapshot age, driver read latency, W2S matrix age,
// skeleton tick/update rate and render rate.
namespace prof {

inline uint64_t NowUs() {
  using namespace std::chrono;
  return static_cast<uint64_t>(
      duration_cast<microseconds>(steady_clock::now().time_since_epoch())
          .count());
}

struct Stat {
  std::atomic<uint64_t> count{0};
  std::atomic<uint64_t> sum{0};
  std::atomic<uint64_t> max{0};

  void Add(uint64_t us) {
    count.fetch_add(1, std::memory_order_relaxed);
    sum.fetch_add(us, std::memory_order_relaxed);
    uint64_t previous = max.load(std::memory_order_relaxed);
    while (us > previous &&
           !max.compare_exchange_weak(previous, us, std::memory_order_relaxed,
                                      std::memory_order_relaxed)) {
    }
  }

  void Reset() {
    count.store(0, std::memory_order_relaxed);
    sum.store(0, std::memory_order_relaxed);
    max.store(0, std::memory_order_relaxed);
  }
};

inline Stat entityTick;      // one full entity worker Tick
inline Stat skeletonTick;    // one skeleton worker tick
inline Stat matrixTick;      // one W2S updater tick
inline Stat driverRead;      // successful driver RawRead (whole call)
inline Stat driverFail;      // failed driver RawRead
inline Stat snapshotAge;     // render: now - last entity snapshot publish
inline Stat matrixAge;       // render: now - last matrix publish
inline std::atomic<uint64_t> snapshots{0};
inline std::atomic<uint64_t> skeletonUpdates{0};
inline std::atomic<uint64_t> matrixChanges{0};
inline std::atomic<uint64_t> renderFrames{0};

inline bool enabled = true;

class ScopeTimer {
 public:
  explicit ScopeTimer(Stat *stat)
      : stat_(stat), start_(enabled ? NowUs() : 0) {}
  ~ScopeTimer() {
    if (stat_ && enabled)
      stat_->Add(NowUs() - start_);
  }

 private:
  Stat *stat_;
  uint64_t start_;
};

inline void Report() {
  if (!enabled)
    return;
  using namespace std::chrono;
  static auto last = steady_clock::now();
  const auto now = steady_clock::now();
  if (now - last < seconds(3))
    return;
  const double secondsSince =
      duration<double>(now - last).count();
  last = now;

  auto print = [](const char *name, Stat &s, double perSecondDivisor = 0.0) {
    const uint64_t n = s.count.exchange(0, std::memory_order_relaxed);
    const uint64_t sum = s.sum.exchange(0, std::memory_order_relaxed);
    const uint64_t max = s.max.exchange(0, std::memory_order_relaxed);
    const double avgMs = n ? (double)sum / n / 1000.0 : 0.0;
    const double maxMs = (double)max / 1000.0;
    if (perSecondDivisor > 0.0) {
      std::printf("%s %.0f/s avg=%.2fms max=%.1fms | ", name,
                  (double)n / perSecondDivisor, avgMs, maxMs);
    } else {
      std::printf("%s n=%llu avg=%.2fms max=%.1fms | ", name,
                  (unsigned long long)n, avgMs, maxMs);
    }
  };

  std::printf("[prof] ");
  print("ent", entityTick, secondsSince);
  print("drv", driverRead, secondsSince);
  print("fail", driverFail, secondsSince);
  print("w2s", matrixTick, secondsSince);
  print("skel", skeletonTick, secondsSince);
  print("snapAge", snapshotAge, 0.0);
  print("matAge", matrixAge, 0.0);
  const uint64_t frames =
      renderFrames.exchange(0, std::memory_order_relaxed);
  const uint64_t snaps = snapshots.exchange(0, std::memory_order_relaxed);
  const uint64_t skel = skeletonUpdates.exchange(0, std::memory_order_relaxed);
  const uint64_t matChg =
      matrixChanges.exchange(0, std::memory_order_relaxed);
  std::printf("render=%.0f/s snapPublish=%.0f/s skelPublish=%.0f/s "
              "matChg=%.0f/s\n",
              (double)frames / secondsSince, (double)snaps / secondsSince,
              (double)skel / secondsSince, (double)matChg / secondsSince);
  std::fflush(stdout);
}

} // namespace prof
