// ================================================================================
// Menu open gesture without the watermark: double tap in the screen centre.
// Pure state machine (no ImGui), fed once per frame with the touch state.
// Coordinates are overlay pixels, the same space as ImGuiIO::MousePos.
// ================================================================================

#pragma once
#include <cmath>

namespace ui::gesture {

    // Centre of the overlay surface and the radius of the tap zone around it.
    struct center_zone {
        float cx{0.f}, cy{0.f}, radius{0.f};
        float width{0.f}, height{0.f};
        bool valid{false};
    };

    // Zone radius as a fraction of the short screen side (1224 px -> 184 px).
    inline constexpr float k_zone_fraction = 0.15f;

    // Auto-detects the centre from the real overlay size. False for a
    // missing or nonsensical display size (not initialised, 0x0, NaN).
    inline bool zone_from_display(float width, float height, center_zone& out) {
        out = {};
        if (!std::isfinite(width) || !std::isfinite(height) || width < 2.f || height < 2.f ||
            width > 16384.f || height > 16384.f) return false;
        out.width = width;
        out.height = height;
        out.cx = width * 0.5f;
        out.cy = height * 0.5f;
        out.radius = (width < height ? width : height) * k_zone_fraction;
        out.valid = true;
        return true;
    }

    inline bool in_zone(const center_zone& z, float x, float y) {
        if (!z.valid) return false;
        const float dx = x - z.cx, dy = y - z.cy;
        return dx * dx + dy * dy <= z.radius * z.radius;
    }

    struct double_tap {
        // Timing in seconds, distances in px (scaled by the caller).
        float max_press_s = 0.30f;   // finger down -> up for one tap
        float max_gap_s = 0.40f;     // first release -> second release
        float max_move_px = 24.f;    // travel allowed within one tap
        float max_pair_px = 80.f;    // distance between the two taps

        bool prev_down = false;
        bool press_valid = false;    // current press started inside the zone and not blocked
        double press_t = 0.0;
        float press_x = 0.f, press_y = 0.f;
        bool armed = false;          // one valid tap seen, waiting for the second
        double first_t = 0.0;
        float first_x = 0.f, first_y = 0.f;

        void reset() { *this = double_tap{max_press_s, max_gap_s, max_move_px, max_pair_px}; }

        // blocked: the press started over UI that owns the touch (menu panel).
        // Returns true on the frame the second tap is released.
        bool update(bool down, float x, float y, double t, const center_zone& z, bool blocked) {
            bool fired = false;
            if (armed && t - first_t > max_gap_s) armed = false;
            if (down && !prev_down) {
                press_valid = !blocked && in_zone(z, x, y);
                press_t = t;
                press_x = x;
                press_y = y;
            } else if (!down && prev_down) {
                const float mx = x - press_x, my = y - press_y;
                const bool tap = press_valid && t - press_t <= max_press_s &&
                                 mx * mx + my * my <= max_move_px * max_move_px && in_zone(z, x, y);
                if (!tap) {
                    armed = false;
                } else if (armed) {
                    const float px = x - first_x, py = y - first_y;
                    if (t - first_t <= max_gap_s && px * px + py * py <= max_pair_px * max_pair_px) {
                        fired = true;
                        armed = false;
                    } else {
                        first_t = t; first_x = x; first_y = y;   // too far/late: this becomes the first tap
                    }
                } else {
                    armed = true;
                    first_t = t; first_x = x; first_y = y;
                }
                press_valid = false;
            }
            prev_down = down;
            return fired;
        }
    };
}
