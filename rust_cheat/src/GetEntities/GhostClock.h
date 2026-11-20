
// ================================================================================
// Source provider: @imguisource_bot
// Channel: t.me/ImGui_Source_FIXNEY (ImGui Source)
// Channel: t.me/il2cpp_src (shit src 🧨)
// ================================================================================

#pragma once
#include <chrono>

// ── Shared wall clock for the last-known-position markers ────────────────────
// The entity worker stamps a sighting with this clock and the render thread
// fades the marker from that stamp, so both sides must read the SAME clock.
// steady_clock is monotonic, so a system time change cannot make a marker jump
// or linger.
//
// This is a double, not a float, on purpose: the epoch is unspecified (MSVC uses
// boot time), so the raw value can be ~1e9 ms after a week of uptime - far too
// big for a float's 24-bit mantissa, which would quantise timestamps to tens of
// milliseconds. A double keeps sub-millisecond resolution at any uptime.
inline double GhostNowMs() {
  using namespace std::chrono;
  return (double)duration_cast<microseconds>(
             steady_clock::now().time_since_epoch())
      .count() *
      0.001;
}
