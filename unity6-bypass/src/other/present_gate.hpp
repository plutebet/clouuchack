// ================================================================================
// Present gate: skip eglSwapBuffers when the overlay frame is identical to the
// one already on screen. Every queued buffer makes SurfaceFlinger recompose
// the display (and the GPU clear + blend a full-screen layer), even when the
// ESP did not move: lobby, death cam, no enemy in view, static menu.
// Pure logic, no GL / ImGui dependency, so it can be checked on a host.
// ================================================================================

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace present {
    // 64-bit FNV-1a over 8-byte words (tail byte-wise). Change detection only:
    // a collision costs one skipped frame, never a wrong one being kept for
    // long, because the gate re-presents at least every k_refresh_ms.
    inline uint64_t hash_bytes(uint64_t h, const void* data, size_t size) noexcept {
        constexpr uint64_t prime = 0x100000001b3ULL;
        const unsigned char* p = static_cast<const unsigned char*>(data);
        while (size >= 8) {
            uint64_t w;
            std::memcpy(&w, p, 8);
            h = (h ^ w) * prime;
            p += 8;
            size -= 8;
        }
        while (size--) h = (h ^ *p++) * prime;
        return h;
    }
    inline constexpr uint64_t k_hash_seed = 0xcbf29ce484222325ULL;

    struct gate {
        // Re-present a static frame this often anyway: the compositor may drop
        // a layer's buffer on a display reconfiguration, and a periodic
        // present also keeps the swap-failure watchdog fed.
        static constexpr long long k_refresh_ms = 1000;
        uint64_t last_hash{0};
        long long last_present_ms{0};
        bool has_last{false};
        unsigned skipped{0};

        // True when the frame must be rendered and swapped.
        bool should_present(uint64_t frame_hash, long long now_ms) noexcept {
            if (has_last && frame_hash == last_hash && now_ms - last_present_ms < k_refresh_ms) {
                ++skipped;
                return false;
            }
            return true;
        }
        // Call only after a successful swap.
        void presented(uint64_t frame_hash, long long now_ms) noexcept {
            last_hash = frame_hash;
            last_present_ms = now_ms;
            has_last = true;
        }
        // A failed swap or a new surface: the screen content is unknown.
        void invalidate() noexcept { has_last = false; }
    };
}
