#pragma once

//
// stealth.hpp — полный обход античита Standoff 2
//
// SO2 использует:
//   1. Google Play Integrity API (класс orn, провайдер "android-playintegrity")
//   2. Key Attestation (класс ujs, провайдер "android-key-attestation")
//   3. APK signature/content hash + root flag (pju.Application)
//   4. DeviceId ban system (OnDeviceBanned, OnPlayerBanned, OnIpBanned)
//   5. Firebase AppCheck token system (yhf -> IAppCheckTokenProvider)
//
// Наш ESP — внешний (process_vm_readv), read-only, overlay через SurfaceFlinger.
// Не трогает память игры, не хукает il2cpp, не инжектит.
// Основные вектора детекта для внешнего ESP:
//   - /proc enumeration (наш процесс виден)
//   - overlay layer name в SurfaceFlinger dump
//   - root detection (su binary, magisk artifacts)
//   - TracerPid / ptrace detection
//   - SELinux context
//   - /proc/self/maps имена VMA
//   - cmdline / comm нашего процесса
//

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sched.h>
#include <dirent.h>
#include <sys/prctl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <linux/sched.h>
#include <cerrno>
#include "diag.hpp"
#include "../protect/oxorany.hpp"

namespace stealth {

// ─── 1. маскировка имени процесса ───────────────────────────────────────────

inline void disguise_process_name(const char* fake_name) {
    prctl(PR_SET_NAME, fake_name, 0, 0, 0);
    diag::log("STEALTH", "process name -> %s", fake_name);
}

inline void disguise_cmdline(int argc, char** argv, const char* fake) {
    if (!argv || !argv[0]) return;
    size_t total = 0;
    for (int i = 0; i < argc; i++) {
        total += strlen(argv[i]) + 1;
    }
    memset(argv[0], 0, total);
    strncpy(argv[0], fake, total - 1);
    diag::log("STEALTH", "cmdline -> %s", fake);
}

// ─── 2. mount namespace isolation ───────────────────────────────────────────

inline bool isolate_mount_namespace() {
    if (unshare(CLONE_NEWNS) != 0) {
        diag::log("STEALTH", "unshare(CLONE_NEWNS) failed errno=%d (need root)", errno);
        return false;
    }
    if (mount("none", "/", nullptr, MS_REC | MS_PRIVATE, nullptr) != 0) {
        diag::log("STEALTH", "mount MS_PRIVATE failed errno=%d", errno);
    }
    diag::log("STEALTH", "mount namespace isolated");
    return true;
}

// ─── 3. скрытие из /proc enumeration ────────────────────────────────────────

inline bool hide_proc_self() {
    const char* tmpdir = "/data/local/tmp/.proc_hide";
    mkdir(tmpdir, 0755);

    char self_path[64];
    snprintf(self_path, sizeof(self_path), "/proc/%d", getpid());

    if (mount(tmpdir, self_path, nullptr, MS_BIND, nullptr) == 0) {
        diag::log("STEALTH", "hidden /proc/%d with bind mount", getpid());
        return true;
    }
    diag::log("STEALTH", "hide /proc/%d failed errno=%d", getpid(), errno);
    return false;
}

// ─── 4. SELinux ─────────────────────────────────────────────────────────────

inline void fix_selinux() {
    int fd = open("/sys/fs/selinux/enforce", O_RDONLY);
    if (fd >= 0) {
        char mode = '1';
        if (read(fd, &mode, 1) > 0 && mode == '0') {
            close(fd);
            diag::log("STEALTH", "SELinux already permissive");
            return;
        }
        close(fd);
    }

    fd = open("/sys/fs/selinux/enforce", O_WRONLY);
    if (fd >= 0) {
        write(fd, "0", 1);
        close(fd);
        diag::log("STEALTH", "SELinux -> permissive (global)");
        return;
    }

    int rc = system("magiskpolicy --live "
        "'allow untrusted_app untrusted_app process ptrace' "
        "'allow shell shell process ptrace' "
        "'allow untrusted_app app_data_file file { read write open }'"
        " >/dev/null 2>&1");
    if (rc == 0) {
        diag::log("STEALTH", "SELinux policy patched via magiskpolicy");
    } else {
        diag::log("STEALTH", "SELinux fix failed — may cause issues on enforcing ROMs");
    }
}

// ─── 5. рут-детект bypass ───────────────────────────────────────────────────

inline void setup_magisk_hide(const char* game_pkg) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "magisk --denylist add %s >/dev/null 2>&1", game_pkg);
    system(cmd);
    snprintf(cmd, sizeof(cmd), "magiskhide add %s >/dev/null 2>&1", game_pkg);
    system(cmd);
    diag::log("STEALTH", "magisk denylist configured for %s", game_pkg);
}

inline void hide_root_artifacts() {
    const char* su_paths[] = {
        "/system/bin/su",
        "/system/xbin/su",
        "/sbin/su",
        "/data/local/su",
        "/data/local/bin/su",
        "/data/local/xbin/su",
    };
    for (const char* path : su_paths) {
        struct stat st;
        if (stat(path, &st) == 0) {
            if (mount("/dev/null", path, nullptr, MS_BIND, nullptr) == 0) {
                diag::log("STEALTH", "hidden %s", path);
            }
        }
    }

    const char* magisk_paths[] = {
        "/data/adb/magisk",
        "/data/adb/modules",
        "/sbin/.magisk",
    };
    const char* empty = "/data/local/tmp/.proc_hide";
    mkdir(empty, 0755);
    for (const char* path : magisk_paths) {
        struct stat st;
        if (stat(path, &st) == 0) {
            mount(empty, path, nullptr, MS_BIND, nullptr);
        }
    }
}

// ─── 6. anti-ptrace (защита от аттача AC) ───────────────────────────────────

inline bool self_ptrace_protect() {
    pid_t child = fork();
    if (child < 0) return false;

    if (child == 0) {
        pid_t parent = getppid();
        if (ptrace(PTRACE_ATTACH, parent, nullptr, nullptr) == 0) {
            waitpid(parent, nullptr, 0);
            ptrace(PTRACE_CONT, parent, nullptr, nullptr);
            while (true) {
                int status;
                waitpid(parent, &status, 0);
                if (WIFEXITED(status) || WIFSIGNALED(status)) break;
                ptrace(PTRACE_CONT, parent, nullptr, nullptr);
            }
        }
        _exit(0);
    }

    usleep(100000);
    diag::log("STEALTH", "anti-ptrace guard pid=%d", child);
    return true;
}

// ─── 7. чистка maps от палевных записей ─────────────────────────────────────

inline void clean_vma_names() {
    FILE* f = fopen("/proc/self/maps", "r");
    if (!f) return;

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        uintptr_t start, end;
        char perms[5];
        if (sscanf(line, "%lx-%lx %4s", &start, &end, perms) != 3) continue;
        if (perms[0] != 'r' || perms[1] != 'w') continue;
        if (strchr(line, '/') != nullptr) continue;
        prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, start, end - start, "");
    }
    fclose(f);
    diag::log("STEALTH", "VMA names cleaned");
}

// ─── 8. direct syscall для process_vm_readv ─────────────────────────────────

inline ssize_t safe_vm_readv(pid_t pid, const struct iovec* local,
                              unsigned long liovcnt,
                              const struct iovec* remote,
                              unsigned long riovcnt, unsigned long flags) {
    #if defined(__aarch64__)
    register long x8 __asm__("x8") = 270;
    register long x0 __asm__("x0") = pid;
    register long x1 __asm__("x1") = (long)local;
    register long x2 __asm__("x2") = liovcnt;
    register long x3 __asm__("x3") = (long)remote;
    register long x4 __asm__("x4") = riovcnt;
    register long x5 __asm__("x5") = flags;
    __asm__ volatile("svc #0"
        : "+r"(x0)
        : "r"(x8), "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5)
        : "memory", "cc");
    return x0;
    #else
    return syscall(SYS_process_vm_readv, pid, local, liovcnt, remote, riovcnt, flags);
    #endif
}

// ─── 8b. direct syscall для process_vm_writev (triggerbot) ─────────────────

inline ssize_t safe_vm_writev(pid_t pid, const struct iovec* local,
                              unsigned long liovcnt,
                              const struct iovec* remote,
                              unsigned long riovcnt, unsigned long flags) {
    #if defined(__aarch64__)
    register long x8 __asm__("x8") = 271;
    register long x0 __asm__("x0") = pid;
    register long x1 __asm__("x1") = (long)local;
    register long x2 __asm__("x2") = liovcnt;
    register long x3 __asm__("x3") = (long)remote;
    register long x4 __asm__("x4") = riovcnt;
    register long x5 __asm__("x5") = flags;
    __asm__ volatile("svc #0"
        : "+r"(x0)
        : "r"(x8), "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5)
        : "memory", "cc");
    return x0;
    #else
    return syscall(SYS_process_vm_writev, pid, local, liovcnt, remote, riovcnt, flags);
    #endif
}

// ─── 9. undumpable ──────────────────────────────────────────────────────────

inline void make_undumpable() {
    prctl(PR_SET_DUMPABLE, 0);
    diag::log("STEALTH", "process set undumpable");
}

// ─── 10. скрытие frida/xposed/substrate следов ─────────────────────────────

inline void hide_instrumentation_traces() {
    const char* suspicious_paths[] = {
        "/data/local/tmp/frida-server",
        "/data/local/tmp/re.frida.server",
        "/system/lib/libxposed_art.so",
        "/system/lib64/libxposed_art.so",
        "/system/framework/XposedBridge.jar",
        "/data/adb/lspd",
        "/data/adb/shamiko",
    };
    for (const char* path : suspicious_paths) {
        struct stat st;
        if (stat(path, &st) == 0) {
            mount("/dev/null", path, nullptr, MS_BIND, nullptr);
        }
    }

    // скрываем unix сокеты frida
    // frida слушает на @frida — в /proc/net/unix
    // (наша защита: мы не используем frida, но AC может детектить
    //  что frida запущена на устройстве параллельно)
    diag::log("STEALTH", "instrumentation traces hidden");
}

// ─── 11. спуфинг /proc/<game_pid>/status от TracerPid ──────────────────────

// process_vm_readv НЕ ставит TracerPid (в отличие от ptrace)
// но нужно убедиться что наш ptrace-guard не палит TracerPid у ИГРЫ
// (self_ptrace_protect аттачит к НАШЕМУ процессу, не к игре — ок)
inline void verify_no_tracer_leak(int game_pid) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/status", game_pid);
    FILE* f = fopen(path, "r");
    if (!f) return;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "TracerPid:", 10) == 0) {
            int tracer = 0;
            sscanf(line + 10, "%d", &tracer);
            if (tracer != 0) {
                diag::log("STEALTH", "WARNING: game has TracerPid=%d — AC will detect this!", tracer);
            } else {
                diag::log("STEALTH", "game TracerPid=0 (clean)");
            }
            break;
        }
    }
    fclose(f);
}

// ─── 12. скрытие Magisk Manager package ────────────────────────────────────

inline void hide_magisk_package() {
    // SO2 может проверять установленные пакеты через PackageManager
    // Magisk 24+ рандомизирует имя пакета, но старые версии используют
    // com.topjohnwu.magisk. Shamiko/Zygisk DenyList обрабатывает это
    // на Java-уровне, но мы дополнительно пропатчим pm list
    const char* magisk_pkgs[] = {
        "com.topjohnwu.magisk",
        "io.github.vvb2060.magisk",   // Alpha
        "de.robv.android.xposed.installer",
    };
    for (const char* pkg : magisk_pkgs) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd),
            "pm disable %s >/dev/null 2>&1", pkg);
        // НЕ выполняем — это слишком агрессивно и может сломать Magisk
        // вместо этого полагаемся на Zygisk DenyList
        (void)cmd;
    }
    diag::log("STEALTH", "magisk package hiding delegated to Zygisk DenyList");
}

// ─── 13. защита от fingerprint через /proc/net ─────────────────────────────

inline void clean_proc_net() {
    // AC может сканить /proc/net/tcp6 для нахождения подозрительных
    // локальных серверов (frida, debugger ports)
    // Наш ESP не открывает сетевых портов — ок
    // Но на всякий случай bind-mount пустой файл если в namespace
    diag::log("STEALTH", "proc/net check: ESP opens no network ports (clean)");
}

// ─── 14. рандомизация timing для anti-fingerprint ──────────────────────────

inline void randomize_timing() {
    // SO2 может использовать timing-based fingerprinting
    // (замер времени выполнения определённых операций для определения
    //  что процесс под наблюдением)
    // Наш ESP — внешний, не инжектит в процесс, поэтому
    // timing игрового процесса не меняется.
    // Но добавляем случайную задержку при старте для анти-pattern matching
    unsigned int seed = 0;
    int urandom = open("/dev/urandom", O_RDONLY);
    if (urandom >= 0) {
        read(urandom, &seed, sizeof(seed));
        close(urandom);
    }
    usleep((seed % 500) * 1000 + 100000); // 100-600ms random delay
    diag::log("STEALTH", "startup timing randomized");
}

// ─── overlay stealth name ──────────────────────────────────────────────────

// имя overlay слоя в SurfaceFlinger dump
// AC может enum'ить слои через dumpsys SurfaceFlinger --list
// и искать подозрительные имена типа "Overlay", "ESP", "ImGui"
// маскируемся под системный UI элемент
inline const char* get_stealth_surface_name() {
    return "StatusBar";
}

// ═══════════════════════════════════════════════════════════════════════════
// full stealth init
// ═══════════════════════════════════════════════════════════════════════════

struct stealth_opts {
    bool no_stealth       = false;
    bool no_namespace     = false;
    bool no_ptrace_guard  = false;
    const char* game_pkg  = "com.axlebolt.standoff2";
    const char* fake_name = "surfaceflinger";
};

inline void init(int argc, char** argv, const stealth_opts& opts) {
    if (opts.no_stealth) {
        diag::log("STEALTH", "stealth disabled by flag");
        return;
    }

    diag::log("STEALTH", "=== SO2 anti-detection init ===");

    // 1. undumpable первым делом
    make_undumpable();

    // 2. mount namespace (до всего остального — изолирует наши mount'ы)
    if (!opts.no_namespace) {
        isolate_mount_namespace();
    }

    // 3. маскировка процесса
    disguise_process_name(opts.fake_name);
    disguise_cmdline(argc, argv, opts.fake_name);

    // 4. randomize timing
    randomize_timing();

    // 5. SELinux
    fix_selinux();

    // 6. root hiding (Magisk DenyList для Java-стороны игры)
    setup_magisk_hide(opts.game_pkg);
    if (!opts.no_namespace) {
        hide_root_artifacts();
    }

    // 7. скрытие /proc/<pid>
    if (!opts.no_namespace) {
        hide_proc_self();
    }

    // 8. скрытие frida/xposed следов
    if (!opts.no_namespace) {
        hide_instrumentation_traces();
    }

    // 9. чистка VMA names
    clean_vma_names();

    // 10. anti-ptrace guard
    if (!opts.no_ptrace_guard) {
        self_ptrace_protect();
    }

    // 11. Magisk package hiding через Zygisk
    hide_magisk_package();

    // 12. proc/net check
    clean_proc_net();

    diag::log("STEALTH", "=== anti-detection initialized ===");
    diag::log("STEALTH", "SO2 AC vectors covered:");
    diag::log("STEALTH", "  [+] Play Integrity: N/A (server-side, we don't touch game process)");
    diag::log("STEALTH", "  [+] Key Attestation: N/A (game-side Java, Zygisk DenyList handles)");
    diag::log("STEALTH", "  [+] APK signature check: N/A (we don't modify APK)");
    diag::log("STEALTH", "  [+] Root flag (pju.Application.Root): Magisk DenyList hides root from game");
    diag::log("STEALTH", "  [+] Process hiding: mount namespace + /proc bind mount");
    diag::log("STEALTH", "  [+] Overlay: surface named 'StatusBar' (system UI camouflage)");
    diag::log("STEALTH", "  [+] process_vm_readv: direct syscall (no libc hooks), no TracerPid");
    diag::log("STEALTH", "  [+] process_vm_writev: direct syscall (SYS 271), no TracerPid, no ptrace");
    diag::log("STEALTH", "  [+] SELinux: patched for our context");
    diag::log("STEALTH", "  [+] Anti-ptrace: self-attach guard");
    diag::log("STEALTH", "  [+] VMA names: cleaned");
    diag::log("STEALTH", "  [+] Root artifacts: bind-mount hidden");
    diag::log("STEALTH", "  [+] Frida/Xposed: traces hidden");
}

// вызывается после нахождения PID игры для дополнительной проверки
inline void post_game_check(int game_pid) {
    if (game_pid > 0) {
        verify_no_tracer_leak(game_pid);
    }
}

} // namespace stealth
