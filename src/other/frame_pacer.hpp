// ================================================================================
// Frame pacing ("late latch") for the overlay loop. Pure: no clocks, no GL;
// the caller passes timestamps, so the logic can be simulated on a host.
//
// The loop sleeps most of the display period and reads the game as late as
// possible before the swap. That needs the display period, and the period
// must never be measured on frames the loop itself slowed down: on device
// log 20261114-175419 the old estimator (median of all swap intervals) took a
// burst of slow swaps during a map load, then slept ~29 ms per frame, the
// swaps came every ~32 ms, the median confirmed 32 ms, and the overlay stayed
// at ~31 Hz on a 144 Hz panel for the rest of the session.
//
// Now the period comes only from calibration bursts: every 2 s, 8 iterations
// run without any sleep, so each swap blocks on the next vsync. From the 4th
// unslept swap in a row (the buffer queue has drained by then, earlier swaps
// can return at once) the swap interval is the display period, or the real
// effective period if the system cannot keep up. One burst with 3+ samples
// sets the period, so a wrong value lasts at most one burst interval.
// ================================================================================

#pragma once

#include <algorithm>

struct frame_pacer {
    static constexpr long long k_burst_interval_ms = 2000;
    static constexpr int k_burst_frames = 8;
    static constexpr int k_steady_after = 4;      // unslept swaps in a row before an interval counts
    static constexpr double k_min_period = 6.0;   // 166 Hz, above any phone panel in use
    static constexpr double k_max_period = 34.0;  // 29 Hz

    double period_ms{8.3};
    double work_ms{2.0};
    double margin_ms{2.5};

    long long next_burst_ms{0};
    int burst_left{0};
    double burst_samples[k_burst_frames]{};
    int burst_count{0};

    long long last_swap_us{0};
    bool has_last_swap{false};
    unsigned swap_streak{0};    // consecutive iterations that swapped
    int unslept_swaps{0};       // consecutive swaps with no sleep and no skipped present in between
    bool slept_since_swap{true};

    unsigned frames{0};         // swaps in the current report window
    unsigned missed{0};
    unsigned calibrations{0};

    bool in_burst() const { return burst_left > 0; }

    void begin_frame(long long now_ms) {
        if (burst_left == 0 && now_ms >= next_burst_ms) {
            burst_left = k_burst_frames;
            burst_count = 0;
            next_burst_ms = now_ms + k_burst_interval_ms;
        }
    }

    // After drawEnd(). work_us: frame start to the swap call.
    void after_present(bool swapped, long long work_us, long long now_us, bool count_misses) {
        // One stalled frame (log 20261114-175231: ~5 s while the system was
        // swapping) must not skew the sleep for the next hundred frames.
        work_ms = work_ms * 0.9 + std::min(double(work_us) / 1000.0, 20.0) * 0.1;
        if (swapped) {
            const double interval = has_last_swap ? double(now_us - last_swap_us) / 1000.0 : 0.0;
            ++swap_streak;
            unslept_swaps = slept_since_swap ? 1 : unslept_swaps + 1;
            if (burst_left > 0 && has_last_swap && unslept_swaps >= k_steady_after &&
                interval >= k_min_period && interval < k_max_period && burst_count < k_burst_frames)
                burst_samples[burst_count++] = interval;
            // Outside bursts an interval only feeds the miss counter (margin).
            if (count_misses && has_last_swap && swap_streak >= 3 && interval > period_ms * 1.6) ++missed;
            ++frames;
            last_swap_us = now_us;
            has_last_swap = true;
            slept_since_swap = false;
        } else {
            // A skipped present does not block and lets the queue drain.
            swap_streak = 0;
            unslept_swaps = 0;
            slept_since_swap = true;
        }
        if (burst_left > 0 && --burst_left == 0 && burst_count >= 3) {
            double sorted[k_burst_frames];
            std::copy(burst_samples, burst_samples + burst_count, sorted);
            std::sort(sorted, sorted + burst_count);
            period_ms = sorted[burst_count / 2];
            ++calibrations;
        }
    }

    // Sleep before the next iteration. Calibration frames never sleep.
    double sleep_ms(bool swapped, bool menu_open, bool game_running) const {
        if (burst_left > 0) return 0.0;
        double s = period_ms - work_ms - margin_ms;
        // An open menu relies on the blocking swap; a skipped present did not
        // block, so that frame sleeps the rest of the period itself.
        if (menu_open && swapped) s = std::min(s, 2.0);
        if (!swapped) s = std::max(1.0, period_ms - work_ms);
        if (!game_running) s = std::max(s, 8.0);
        return s > 0.3 ? s : 0.0;
    }

    void slept(double ms) { if (ms > 0.0) slept_since_swap = true; }

    // Every ~2 s: adapt the margin to the miss rate, reset the window.
    void end_window() {
        const double miss_rate = frames ? double(missed) / double(frames) : 0.0;
        if (miss_rate > 0.03) margin_ms = std::min(5.0, margin_ms + 0.5);
        else if (miss_rate < 0.005) margin_ms = std::max(1.5, margin_ms - 0.1);
        frames = missed = 0;
    }
};
