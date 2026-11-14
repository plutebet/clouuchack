
// ================================================================================
// @project     ImGui Source
// @bot         t.me/imguisource_bot
// @channel_1   t.me/ImGui_Source_FIXNEY (ImGui Source)
// @channel_2   t.me/il2cpp_src (shit src 🧨)
// ================================================================================

#include "Android_draw/draw.h"
#include "fonts/menu/pixeloperator.h"
#include "fonts/esp/esp.h"
#include "fonts/esp/weapon_icons.h"
#include "fonts/menu/butterfly.h"
#include <cmath>
#include <cstdint>
#include <vector>

EGLDisplay display = EGL_NO_DISPLAY;
EGLConfig config;
EGLSurface surface = EGL_NO_SURFACE;
EGLContext context = EGL_NO_CONTEXT;

ANativeWindow *native_window = nullptr;
ImFont* fontBold;
ImFont* fontMedium;
ImFont* fontDesc;
ImFont* espFont;
ImFont* fontMenu = nullptr;
ImFont* fontWm = nullptr;
ImTextureID g_weapon_atlas_tex = nullptr;
static GLuint g_weapon_atlas_gl = 0;
ImTextureID g_logo_tex = nullptr;
static GLuint g_logo_gl = 0;

int native_window_screen_x = 0;
int native_window_screen_y = 0;
android::ANativeWindowCreator::DisplayInfo displayInfo{0};
uint32_t orientation = 0;
bool g_Initialized = false;
ImGuiWindow *g_window = nullptr;

bool initGUI_draw(uint32_t _screen_x, uint32_t _screen_y, bool log) {
    orientation = displayInfo.orientation;

    if (!init_egl(_screen_x, _screen_y, log)) {
        return false;
    }

    if (!ImGui_init()) {
        return false;
    }

    return true;
}

bool init_egl(uint32_t _screen_x, uint32_t _screen_y, bool log) {
    if (_screen_x < 2 || _screen_y < 2) return false;
    ::native_window = android::ANativeWindowCreator::Create("Overlay", _screen_x, _screen_y, false);
    if (!::native_window) {
        display = EGL_NO_DISPLAY;
        return false;
    }

    ANativeWindow_acquire(native_window);
    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY) {
        return false;
    }

    EGLint egl_major = 0, egl_minor = 0;
    if (eglInitialize(display, &egl_major, &egl_minor) != EGL_TRUE) {
        return false;
    }

    // RGBA8888 without depth/stencil. ImGui never uses depth or stencil, but
    // a full-screen D16S8 attachment that glClear(COLOR) does not touch is
    // loaded and stored by tiled GPUs every frame: pure bandwidth taken from
    // the game. The old 5/6/5 + depth/stencil list stays as a fallback for
    // drivers that expose no such config.
    const EGLint preferred_attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 0,
        EGL_STENCIL_SIZE, 0,
        EGL_NONE
    };
    const EGLint legacy_attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_BLUE_SIZE, 5,
        EGL_GREEN_SIZE, 6,
        EGL_RED_SIZE, 5,
        EGL_BUFFER_SIZE, 32,
        EGL_DEPTH_SIZE, 16,
        EGL_STENCIL_SIZE, 8,
        EGL_NONE
    };
    const EGLint attrib_list[] = {
        EGL_CONTEXT_CLIENT_VERSION,
        3,
        EGL_NONE
    };

    // "At least 0" still admits configs with depth; take the first exact
    // RGBA8888/D0S0 one among the matches.
    bool have_config = false;
    {
        EGLConfig configs[32];
        EGLint count = 0;
        if (eglChooseConfig(display, preferred_attribs, configs, 32, &count) == EGL_TRUE) {
            for (EGLint i = 0; i < count && !have_config; ++i) {
                EGLint r = 0, g = 0, b = 0, a = 0, d = -1, s = -1;
                eglGetConfigAttrib(display, configs[i], EGL_RED_SIZE, &r);
                eglGetConfigAttrib(display, configs[i], EGL_GREEN_SIZE, &g);
                eglGetConfigAttrib(display, configs[i], EGL_BLUE_SIZE, &b);
                eglGetConfigAttrib(display, configs[i], EGL_ALPHA_SIZE, &a);
                eglGetConfigAttrib(display, configs[i], EGL_DEPTH_SIZE, &d);
                eglGetConfigAttrib(display, configs[i], EGL_STENCIL_SIZE, &s);
                if (r == 8 && g == 8 && b == 8 && a == 8 && d == 0 && s == 0) {
                    config = configs[i];
                    have_config = true;
                }
            }
        }
    }
    g_egl_lean_config = have_config;
    if (!have_config) {
        EGLint num_config = 0;
        if (eglChooseConfig(display, legacy_attribs, &config, 1, &num_config) != EGL_TRUE || num_config < 1) {
            return false;
        }
    }

    EGLint egl_format = 0;
    if (eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &egl_format) != EGL_TRUE) {
        return false;
    }
    if (ANativeWindow_setBuffersGeometry(native_window, 0, 0, egl_format) != 0) {
        return false;
    }
    context = eglCreateContext(display, config, EGL_NO_CONTEXT, attrib_list);
    if (context == EGL_NO_CONTEXT) {
        return false;
    }

    surface = eglCreateWindowSurface(display, config, native_window, nullptr);
    if (surface == EGL_NO_SURFACE) {
        return false;
    }

    if (!eglMakeCurrent(display, surface, surface, context)) {
        return false;
    }
    // One buffer per vsync at most; the main loop paces itself on this.
    eglSwapInterval(display, 1);
    g_present_gate.invalidate();

    return true;
}

void screen_config() {
    displayInfo = android::ANativeWindowCreator::GetDisplayInfo();
}

bool ImGui_init() {
    if (g_Initialized) {
        return true;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplAndroid_Init(native_window);
    ImGui_ImplOpenGL3_Init("#version 300 es");

    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = NULL;

    // Every font below lives in a static array: the atlas must not free() it
    // on DestroyContext (FontDataOwnedByAtlas defaults to true).
    ImFontConfig fc;
    fc.FontDataOwnedByAtlas = false;
    fontBold = io.Fonts->AddFontFromMemoryTTF(pixeloperator, sizeof(pixeloperator), 32, &fc, io.Fonts->GetGlyphRangesCyrillic());
    fontMedium = io.Fonts->AddFontFromMemoryTTF(pixeloperator, sizeof(pixeloperator), 28, &fc, io.Fonts->GetGlyphRangesCyrillic());
    fontDesc = io.Fonts->AddFontFromMemoryTTF(pixeloperator, sizeof(pixeloperator), 22, &fc, io.Fonts->GetGlyphRangesCyrillic());
    espFont = io.Fonts->AddFontFromMemoryTTF(esp, sizeof(esp), 18, &fc, io.Fonts->GetGlyphRangesCyrillic());
    // cloudhack menu / watermark fonts: sized for the real panel (base 1224 px
    // short side, Infinix GT 30 Pro 2720x1224), so they stay crisp instead of
    // being scaled up from a small atlas glyph.
    {
        const float short_side = native_window_screen_y > 1 ? float(native_window_screen_y) : 1224.f;
        float k = short_side / 1224.f;
        if (k < 0.5f) k = 0.5f;
        if (k > 3.f) k = 3.f;
        fontMenu = io.Fonts->AddFontFromMemoryTTF(esp, sizeof(esp), roundf(36.f * k), &fc, io.Fonts->GetGlyphRangesCyrillic());
        fontWm = io.Fonts->AddFontFromMemoryTTF(esp, sizeof(esp), roundf(54.f * k), &fc, io.Fonts->GetGlyphRangesCyrillic());
    }
    if (!fontBold || !fontMedium || !fontDesc || !espFont || !fontMenu || !fontWm) {
        // The menu dereferences fontBold/fontMedium/fontDesc directly; a bad
        // font atlas would otherwise turn every menu frame into a null deref.
        return false;
    }
    io.FontDefault = fontMedium;

    // Menu logo (butterfly.h): 8-bit alpha expanded to white RGBA8 and uploaded
    // as a GL texture. The EGL context is current here (init_egl ran before ImGui_init).
    {
        std::vector<unsigned char> rgba(size_t(butterfly_w) * size_t(butterfly_h) * 4);
        for (size_t i = 0; i < size_t(butterfly_w) * size_t(butterfly_h); ++i) {
            rgba[i * 4 + 0] = 255;
            rgba[i * 4 + 1] = 255;
            rgba[i * 4 + 2] = 255;
            rgba[i * 4 + 3] = butterfly_alpha[i];
        }
        GLuint tex = 0;
        glGenTextures(1, &tex);
        if (tex) {
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, butterfly_w, butterfly_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
            if (glGetError() == GL_NO_ERROR) {
                g_logo_gl = tex;
                g_logo_tex = reinterpret_cast<ImTextureID>(static_cast<intptr_t>(tex));
            } else {
                glDeleteTextures(1, &tex);
            }
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }

    // Weapon icon atlas (weapon_icons.h): 8-bit alpha, expanded to white RGBA8 and uploaded
    // as one GL texture. The ESP tints it with the box colour. Missing atlas -> names only.
    {
        std::vector<unsigned char> rgba(size_t(weapon_icons_w) * size_t(weapon_icons_h) * 4);
        for (size_t i = 0; i < size_t(weapon_icons_w) * size_t(weapon_icons_h); ++i) {
            rgba[i * 4 + 0] = 255;
            rgba[i * 4 + 1] = 255;
            rgba[i * 4 + 2] = 255;
            rgba[i * 4 + 3] = weapon_icons_alpha[i];
        }
        GLuint tex = 0;
        glGenTextures(1, &tex);
        if (tex) {
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, weapon_icons_w, weapon_icons_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
            if (glGetError() == GL_NO_ERROR) {
                g_weapon_atlas_gl = tex;
                g_weapon_atlas_tex = reinterpret_cast<ImTextureID>(static_cast<intptr_t>(tex));
            } else {
                glDeleteTextures(1, &tex);
            }
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }

    ImGui::GetStyle().ScaleAllSizes(3.0f);
    ::g_Initialized = true;
    return true;
}

// Display state through SurfaceComposerClient costs up to three binder calls
// into SurfaceFlinger (display ids, token, state). Every frame that was ~360
// transactions a second against the process composing the game's frames.
// Rotation is detected within 250 ms this way. A failed query (returns
// zeros) keeps the last good state: before, one failed call reported
// orientation 0 and hid the ESP and the menu for that frame.
static long long g_last_display_query_ms = 0;
static long long mono_ms() {
    struct timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

void drawBegin() {
    const long long now = mono_ms();
    if (now - g_last_display_query_ms >= 250) {
        g_last_display_query_ms = now;
        const android::ANativeWindowCreator::DisplayInfo info = android::ANativeWindowCreator::GetDisplayInfo();
        if (info.width > 0 && info.height > 0) displayInfo = info;
    }
    if (::orientation != displayInfo.orientation) {
        ::orientation = displayInfo.orientation;
        touch::update(displayInfo.width, displayInfo.height, displayInfo.orientation);
        if (g_window) {
            g_window->Pos.x = 100;
            g_window->Pos.y = 125;
        }
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame(native_window_screen_x, native_window_screen_y);
    ImGui::NewFrame();
}

unsigned g_swap_failures = 0;
unsigned g_swap_consecutive_failures = 0;
unsigned g_swap_last_error = 0;

present::gate g_present_gate;
bool g_egl_lean_config = false;

// Everything that decides what reaches the screen: geometry, colours, UVs,
// textures and clip rects of every draw command.
static uint64_t draw_data_hash(const ImDrawData* data) {
    uint64_t h = present::k_hash_seed;
    if (!data || !data->Valid) return h;
    h = present::hash_bytes(h, &data->DisplaySize, sizeof(data->DisplaySize));
    for (int n = 0; n < data->CmdListsCount; ++n) {
        const ImDrawList* list = data->CmdLists[n];
        h = present::hash_bytes(h, list->VtxBuffer.Data, size_t(list->VtxBuffer.Size) * sizeof(ImDrawVert));
        h = present::hash_bytes(h, list->IdxBuffer.Data, size_t(list->IdxBuffer.Size) * sizeof(ImDrawIdx));
        for (const ImDrawCmd& cmd : list->CmdBuffer) {
            h = present::hash_bytes(h, &cmd.ClipRect, sizeof(cmd.ClipRect));
            h = present::hash_bytes(h, &cmd.TextureId, sizeof(cmd.TextureId));
            const unsigned offsets[3] = {cmd.VtxOffset, cmd.IdxOffset, cmd.ElemCount};
            h = present::hash_bytes(h, offsets, sizeof(offsets));
        }
    }
    return h;
}

bool drawEnd() {
    if (display == EGL_NO_DISPLAY || surface == EGL_NO_SURFACE || context == EGL_NO_CONTEXT) return false;
    ImGui::Render();
    const uint64_t frame_hash = draw_data_hash(ImGui::GetDrawData());
    const long long now = mono_ms();
    if (!g_present_gate.should_present(frame_hash, now)) return false;
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    if (eglSwapBuffers(display, surface) == EGL_TRUE) {
        g_swap_consecutive_failures = 0;
        g_present_gate.presented(frame_hash, now);
    } else {
        // Lost surface/context (display reconfiguration). main() restarts the
        // overlay through the supervisor once this persists.
        ++g_swap_failures;
        ++g_swap_consecutive_failures;
        g_swap_last_error = unsigned(eglGetError());
        g_present_gate.invalidate();
    }
    return true;
}

void shutdown() {
    if (!g_Initialized) {
        return;
    }

    if (g_logo_gl) {
        glDeleteTextures(1, &g_logo_gl);
        g_logo_gl = 0;
        g_logo_tex = nullptr;
    }
    if (g_weapon_atlas_gl) {
        glDeleteTextures(1, &g_weapon_atlas_gl);
        g_weapon_atlas_gl = 0;
        g_weapon_atlas_tex = nullptr;
    }
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplAndroid_Shutdown();
    ImGui::DestroyContext();

    if (display != EGL_NO_DISPLAY) {
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (context != EGL_NO_CONTEXT) {
            eglDestroyContext(display, context);
        }
        if (surface != EGL_NO_SURFACE) {
            eglDestroySurface(display, surface);
        }
        eglTerminate(display);
    }
    display = EGL_NO_DISPLAY;
    context = EGL_NO_CONTEXT;
    surface = EGL_NO_SURFACE;

    if (native_window) {
        ANativeWindow_release(native_window);
        android::ANativeWindowCreator::Destroy(native_window);
        native_window = nullptr;
    }
    ::g_Initialized = false;
}
