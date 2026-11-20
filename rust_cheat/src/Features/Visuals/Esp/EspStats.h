
// ================================================================================
// Get latest sources from @imguisource_bot
// Join ImGui Source: t.me/ImGui_Source_FIXNEY
// Join shit src 🧨: t.me/il2cpp_src
// ================================================================================

#pragma once
#include <atomic>
#include <cstdint>

// ── Render-side ESP diagnostics ──────────────────────────────────────────────
// Esp.cpp bumps these while it walks the snapshot; the entity worker's rare
// console line reads and clears them. They answer the only question that
// matters when a box is missing: did the worker emit the entity, and if so
// which render-side filter dropped it?
//
// If the worker reports a player (actors=N) and pDraw stays at zero while one
// of the p* counters climbs, the box is being filtered HERE - not lost in the
// entity pipeline.
//
// Plain relaxed atomics: nothing is ever locked on the frame path.
namespace espstats {
inline std::atomic<uint32_t> total{0};   // entities iterated this interval
inline std::atomic<uint32_t> pDraw{0};   // player boxes drawn
inline std::atomic<uint32_t> pDead{0};   // player skipped: dead (lifestate)
inline std::atomic<uint32_t> pSleep{0};  // player skipped: hide_sleepers flag
inline std::atomic<uint32_t> pDist{0};   // player skipped: beyond draw distance
inline std::atomic<uint32_t> pW2S{0};    // player skipped: world->screen failed
inline std::atomic<uint32_t> pOff{0};    // player skipped: far off screen
inline std::atomic<uint32_t> gDraw{0};   // last-known-position markers drawn
// Duration of the most recent esp::Render() call, in microseconds (set, not
// summed). This is the CPU cost of ONE overlay frame's ESP pass, so the
// [cache] heartbeat can show whether the drawing is actually the bottleneck.
inline std::atomic<uint32_t> renderUs{0};

inline uint32_t take(std::atomic<uint32_t> &a) {
  return a.exchange(0, std::memory_order_relaxed);
}
}  // namespace espstats
