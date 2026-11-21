#include "../Android_draw/draw.h"
#include "../ui/theme/theme.hpp"
#include "../ui/menu.hpp"
#include "../ui/bar.hpp"
#include "../other/memory.hpp"
#include "../other/status.hpp"
#include "../other/crash.hpp"
#include "../other/stealth.hpp"
#include "../other/cpu_affinity.hpp"
#include "../other/frame_pacer.hpp"
#include "rise_game.hpp"
#include "rise_visuals.hpp"
#include "rise_anticheat.hpp"
#include "../protect/oxorany.hpp"
#include <cstdio>
#include <cstring>
#include <csignal>
#include <algorithm>
#include <string>
#include <atomic>
#include <thread>
#include <chrono>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/system_properties.h>
#include <fcntl.h>
#include <unistd.h>

enum : int {
    exit_ok = 0,
    exit_duplicate = 2,
    exit_display = 3,
    exit_init = 4,
};

static std::atomic<bool> g_quit{false};
static pid_t g_child = 0;

static void on_signal(int) { g_quit = true; }
static void on_parent_signal(int sig) {
    g_quit = true;
    if (g_child > 0) ::kill(g_child, sig);
}

static void print_status(const char* status) {
    diag::log("STATUS", "%s", status);
}

struct options {
    bool diag_only = false;
    bool no_launch = false;
    bool no_lock = false;
    bool no_supervisor = false;
    bool no_pin = false;
    bool no_stealth = false;
    bool no_namespace = false;
    bool no_ptrace_guard = false;
    const char* log_path = "/data/local/tmp/.rise_render.log";
    const char* crash_log_path = "/data/local/tmp/.rise_render_err.log";
};

struct heartbeat {
    std::atomic<long long> last_ms;
    std::atomic<unsigned long long> frame;
    std::atomic<int> phase;
    std::atomic<int> first_frame;
};
static heartbeat* g_hb = nullptr;

static std::string sys_prop(const char* name) {
    char value[PROP_VALUE_MAX + 1] = {0};
    const int len = __system_property_get(name, value);
    return len > 0 ? std::string(value, size_t(len)) : std::string();
}

static void launch_standrise() {
    int rc = system(oxorany("am start -n com.standrise.game/com.unity3d.player.UnityPlayerActivity >/dev/null 2>&1"));
    if (rc != 0)
        rc = system(oxorany("monkey -p com.standrise.game -c android.intent.category.LAUNCHER 1 >/dev/null 2>&1"));
    diag::log(rc == 0 ? "INFO" : "WARN", "game launch exit=%d", rc);
}

static int acquire_lock(const options& opts) {
    if (opts.no_lock) return -1;
    const char* paths[] = {
        oxorany("/data/local/tmp/.rise_render.lock"),
        oxorany("./.rise_render.lock"),
    };
    for (const char* path : paths) {
        int fd = ::open(path, O_CREAT | O_RDWR | O_CLOEXEC, 0600);
        if (fd < 0) continue;
        if (flock(fd, LOCK_EX | LOCK_NB) == 0) {
            diag::log("INFO", "instance lock %s", path);
            return fd;
        }
        diag::log("WARN", "lock contested at %s, killing old instance", path);
        ::close(fd);
        ::unlink(path);
        system(oxorany("killall -9 rise 2>/dev/null; true"));
        usleep(300000);
        fd = ::open(path, O_CREAT | O_RDWR | O_CLOEXEC, 0600);
        if (fd < 0) continue;
        if (flock(fd, LOCK_EX | LOCK_NB) == 0) {
            diag::log("INFO", "recovered lock %s", path);
            return fd;
        }
        ::close(fd);
        ::unlink(path);
        diag::log("WARN", "force-cleared lock %s", path);
        return -1;
    }
    return -1;
}

void sf_imgui_assert_fail(const char* expr, const char* file, int line) {
    static long long last = 0;
    static unsigned count = 0;
    ++count;
    if (diag::every(last, 2000))
        diag::log("ERROR", "ImGui assertion: %s at %s:%d (count=%u stage=%s)", expr, file, line, count, crash::g_stage);
}

static void beat(int phase) {
    if (!g_hb) return;
    g_hb->last_ms.store(diag::now_ms());
    g_hb->phase.store(phase);
}

static int run_overlay(const options& opts) {
    crash::install(opts.crash_log_path);
    crash::stage("startup");

    ::signal(SIGPIPE, SIG_IGN);
    ::signal(SIGINT, on_signal);
    ::signal(SIGTERM, on_signal);
    ::signal(SIGHUP, on_signal);

    {
        stealth::stealth_opts so;
        so.no_stealth      = opts.no_stealth;
        so.no_namespace    = opts.no_namespace;
        so.no_ptrace_guard = opts.no_ptrace_guard;
        so.game_pkg        = oxorany("com.standrise.game");
        so.fake_name       = oxorany("surfaceflinger");
        stealth::init(0, nullptr, so);
    }

    rise_ac::block_ac_network();
    diag::log("INFO", "starting profile=%s pid=%d", rise::profile_name, int(getpid()));
    diag::log("INFO", "device model=%s android=%s sdk=%s abi=%s",
              sys_prop("ro.product.model").c_str(),
              sys_prop("ro.build.version.release").c_str(),
              sys_prop("ro.build.version.sdk").c_str(),
              sys_prop("ro.product.cpu.abi").c_str());
    if (getuid() != 0) diag::log("WARN", "not running as root");

    const int instance_lock = acquire_lock(opts);
    if (instance_lock == -2) return exit_duplicate;

    if (!opts.no_pin) affinity::pin_to_efficiency_cluster();

    crash::stage("display");
    for (int attempt = 0; attempt < 30; ++attempt) {
        screen_config();
        if (displayInfo.width > 0 && displayInfo.height > 0) break;
        usleep(100000);
    }
    if (displayInfo.width <= 0 || displayInfo.height <= 0) {
        diag::log("ERROR", "display info unavailable");
        if (!opts.diag_only) return exit_init;
    }

    int max_size = (displayInfo.height > displayInfo.width ? displayInfo.height : displayInfo.width);
    int min_size = (displayInfo.height < displayInfo.width ? displayInfo.height : displayInfo.width);
    if (max_size < 2 || min_size < 2) {
        diag::log("ERROR", "invalid display size %dx%d", displayInfo.width, displayInfo.height);
        if (!opts.diag_only) return exit_init;
    }

    g_sw = static_cast<float>(max_size);
    g_sh = static_cast<float>(min_size);
    native_window_screen_x = max_size;
    native_window_screen_y = min_size;

    diag::log("INFO", "display %dx%d orientation=%d overlay=%dx%d",
              displayInfo.width, displayInfo.height, displayInfo.orientation,
              native_window_screen_x, native_window_screen_y);

    if (opts.diag_only) {
        const bool gui = displayInfo.width > 0 && initGUI_draw(native_window_screen_x, native_window_screen_y, true);
        diag::log(gui ? "INFO" : "ERROR", "overlay init %s", gui ? "OK" : "FAILED");
        const int pid = get_pid();
        diag::log(pid > 0 ? "INFO" : "WARN", "game pid=%d", pid);
        if (pid > 0) {
            proc::pid = pid;
            init_mem();
            proc::lib = get_lib();
            diag::log(proc::lib ? "INFO" : "WARN", "libil2cpp base=0x%llx", (unsigned long long)proc::lib);
        }
        if (gui) shutdown();
        return gui ? exit_ok : exit_init;
    }

    crash::stage("gui-init");
    if (!initGUI_draw(native_window_screen_x, native_window_screen_y, true)) {
        diag::log("ERROR", "initGUI_draw failed");
        return exit_init;
    }

    if (!touch::init(max_size, min_size, (uint8_t)displayInfo.orientation))
        diag::log("WARN", "no touch device");

    print_status(oxorany("Game detect ..."));
    if (!opts.no_launch && get_pid() <= 0) launch_standrise();

    game::init();
    stealth::post_game_check(proc::pid);

    static float alpha = 0.f;
    static bool prev = false;
    static bool game_started = false;

    frame_pacer pacer;
    long long last_pace_report = diag::now_ms();
    unsigned swap_failures_seen = 0;
    unsigned skipped_reported = 0;

    int exit_code = exit_ok;
    while (!g_quit && !status::g_quit_request) {
        crash::g_frame.fetch_add(1);
        beat(0);
        crash::stage("frame-begin");
        pacer.begin_frame(diag::now_ms());
        drawBegin();

        crash::stage("process");
        bool run = game::valid();
        const bool esp_allowed = run && proc::lib != 0;

#if defined(__x86_64__)
        bool is_landscape = (displayInfo.orientation == 0 || displayInfo.orientation == 2);
#else
        bool is_landscape = (displayInfo.orientation == 1 || displayInfo.orientation == 3);
#endif

        if (run && !prev) {
            print_status(game_started ? oxorany("Game detect OK") : oxorany("start cheat...."));
            game_started = true;
            prev = true;
        } else if (!run && prev) {
            print_status(oxorany("game closed"));
            prev = false;
            game_started = false;
            rise_game::reset();
            rise_ac::reset();
        }

        if (is_landscape) {
            ImGuiIO& io = ImGui::GetIO();
            float dt = io.DeltaTime;
            if (dt <= 0.f || dt > 0.1f) dt = 0.016f;
            float target = run ? 1.f : 0.f;
            float spd = run ? 4.f : 6.f;
            if (alpha < target) { alpha += dt * spd; if (alpha > target) alpha = target; }
            else if (alpha > target) { alpha -= dt * spd; if (alpha < target) alpha = target; }
            ui::bar::set_game_alpha(alpha);
            if (alpha > 0.001f) { crash::stage("menu"); ui::menu::render(); }

            if (esp_allowed) {
                if (!rise_game::g_il2cpp_base) {
                    rise_game::g_il2cpp_base = proc::lib;
                    rise_game::g_pid = proc::pid;
                    diag::log("RISE", "il2cpp base=0x%llx pid=%d", (unsigned long long)proc::lib, proc::pid);
                }
                if (rise_game::g_pid != proc::pid) {
                    rise_game::reset();
                    rise_game::g_il2cpp_base = proc::lib;
                    rise_game::g_pid = proc::pid;
                }
                game::check_lib(proc::lib);
                crash::stage("anticheat");
                rise_ac::tick();
                crash::stage("render");
                rise_visuals::draw();
            }
        }

        crash::stage("swap");
        const long long before_swap_us = diag::now_us();
        beat(1);
        const bool swapped = drawEnd();
        beat(0);
        const long long after_swap_us = diag::now_us();
        if (g_hb && !g_hb->first_frame.load()) g_hb->first_frame.store(1);

        if (g_swap_failures != swap_failures_seen) {
            swap_failures_seen = g_swap_failures;
            if (g_swap_consecutive_failures >= 120) {
                diag::log("ERROR", "eglSwapBuffers failed %u times; restarting", g_swap_consecutive_failures);
                exit_code = exit_display;
                break;
            }
        }

        pacer.after_present(swapped, before_swap_us - diag::now_us() + (after_swap_us - before_swap_us), after_swap_us, run);
        if (diag::now_ms() - last_pace_report > 2000) {
            pacer.end_window();
            skipped_reported = g_present_gate.skipped;
            last_pace_report = diag::now_ms();
        }
        crash::stage("sleep");
        const double sleep_ms = pacer.sleep_ms(swapped, ui::bar::g_open, run);
        if (sleep_ms > 0.0) usleep(useconds_t(sleep_ms * 1000.0));
        pacer.slept(sleep_ms);
    }

    crash::stage("shutdown");
    diag::log("INFO", "shutting down (code=%d)", exit_code);
    if (instance_lock >= 0) ::close(instance_lock);
    return exit_code;
}

static int supervise(const options& opts) {
    g_hb = static_cast<heartbeat*>(::mmap(nullptr, sizeof(heartbeat), PROT_READ | PROT_WRITE,
                                          MAP_SHARED | MAP_ANONYMOUS, -1, 0));
    if (g_hb == MAP_FAILED) {
        g_hb = nullptr;
        return run_overlay(opts);
    }
    ::signal(SIGINT, on_parent_signal);
    ::signal(SIGTERM, on_parent_signal);
    ::signal(SIGHUP, on_parent_signal);
    ::signal(SIGPIPE, SIG_IGN);

    long long restart_times[8] = {0};
    int restart_count = 0;
    int init_failures = 0;
    diag::log("INFO", "supervisor pid=%d", int(getpid()));

    while (!g_quit) {
        g_hb->last_ms.store(diag::now_ms());
        g_hb->frame.store(0);
        g_hb->phase.store(0);
        g_hb->first_frame.store(0);
        const pid_t child = ::fork();
        if (child < 0) return run_overlay(opts);
        if (child == 0) {
            ::signal(SIGINT, SIG_DFL);
            ::signal(SIGTERM, SIG_DFL);
            ::signal(SIGHUP, SIG_DFL);
            const int code = run_overlay(opts);
            diag::close_log();
            _exit(code);
        }
        g_child = child;
        diag::log("INFO", "supervisor: child pid=%d", int(child));

        int status = 0;
        bool exited = false;
        long long swap_block_logged = 0;
        while (!g_quit) {
            const pid_t r = ::waitpid(child, &status, WNOHANG);
            if (r == child) { exited = true; break; }
            if (r < 0 && errno != EINTR) { exited = true; status = 0; break; }
            usleep(200000);
            const long long age = diag::now_ms() - g_hb->last_ms.load();
            if (age > 30000 && g_hb->first_frame.load()) {
                if (g_hb->phase.load() == 1) {
                    if (diag::every(swap_block_logged, 60000))
                        diag::log("WARN", "supervisor: blocked in swap for %lld ms", age);
                    continue;
                }
                diag::log("ERROR", "supervisor: hung for %lld ms, killing", age);
                ::kill(child, SIGKILL);
                ::waitpid(child, &status, 0);
                exited = true;
                break;
            }
        }
        g_child = 0;
        if (g_quit && !exited) {
            ::kill(child, SIGTERM);
            for (int i = 0; i < 50 && ::waitpid(child, &status, WNOHANG) == 0; ++i) usleep(100000);
            if (::waitpid(child, &status, WNOHANG) == 0) ::kill(child, SIGKILL);
            break;
        }

        int delay_ms = 1000;
        if (WIFEXITED(status)) {
            const int code = WEXITSTATUS(status);
            if (code == exit_ok || code == exit_duplicate) return code;
            if (code == exit_display) { delay_ms = 500; }
            else if (code == exit_init) {
                if (++init_failures > 10) return code;
                delay_ms = 3000;
            }
        }

        const long long now = diag::now_ms();
        restart_times[restart_count % 8] = now;
        ++restart_count;
        if (restart_count >= 8 && now - restart_times[restart_count % 8] < 120000) return 5;
        for (int waited = 0; waited < delay_ms && !g_quit; waited += 100) usleep(100000);
    }
    return exit_ok;
}

int main(int argc, char** argv) {
    options opts;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--diag")) opts.diag_only = true;
        else if (!std::strcmp(argv[i], "--no-launch")) opts.no_launch = true;
        else if (!std::strcmp(argv[i], "--no-lock")) opts.no_lock = true;
        else if (!std::strcmp(argv[i], "--no-supervisor")) opts.no_supervisor = true;
        else if (!std::strcmp(argv[i], "--no-pin")) opts.no_pin = true;
        else if (!std::strcmp(argv[i], "--no-stealth")) opts.no_stealth = true;
        else if (!std::strcmp(argv[i], "--no-namespace")) opts.no_namespace = true;
        else if (!std::strcmp(argv[i], "--no-ptrace-guard")) opts.no_ptrace_guard = true;
        else if (!std::strcmp(argv[i], "--log") && i + 1 < argc) opts.log_path = argv[++i];
        else if (!std::strcmp(argv[i], "--crash-log") && i + 1 < argc) opts.crash_log_path = argv[++i];
    }

    const int stdout_flags = ::fcntl(STDOUT_FILENO, F_GETFL, 0);
    if (stdout_flags >= 0) ::fcntl(STDOUT_FILENO, F_SETFL, stdout_flags | O_NONBLOCK);
    if (!diag::open_log(opts.log_path, true) && !diag::open_log("./.rise_render.log", true))
        diag::log("WARN", "log file unavailable, stdout only");

    if (opts.diag_only || opts.no_supervisor) {
        const int code = run_overlay(opts);
        diag::close_log();
        _exit(code);
    }
    const int code = supervise(opts);
    diag::close_log();
    _exit(code);
}
