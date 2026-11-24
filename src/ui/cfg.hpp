// ================================================================================
// ESP configuration. Mirrors the reference menu: master switch, Box (Clean
// style only), Info bar (name + health merged), Weapon name.
// ================================================================================

#pragma once
#include "imgui.h"

namespace cfg {

namespace esp {
    // Master switch: off hides every ESP element without unloading.
    inline bool visuals = true;
    inline bool box = true;
    // Name pill above the box + HP line inside its bottom edge, one toggle.
    inline bool infobar = true;
    inline bool weapon = true;
    // Tracer lines from the top of the screen to every enemy, as on the
    // reference: thin white lines converging at one point near the timer.
    inline bool line = true;
    // Default white for the Clean box and the weapon name.
    inline ImVec4 box_col = ImVec4(1.f, 1.f, 1.f, 1.f);
    inline ImVec4 name_col = ImVec4(1.f, 1.f, 1.f, 1.f);
}

}
