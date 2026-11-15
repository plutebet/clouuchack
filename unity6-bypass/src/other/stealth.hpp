#pragma once

//
// stealth.hpp — обход античита для внешнего ESP на рутованном ARM64
//
// закрывает основные вектора детекта мобильных AC (ACE/GameGuard/custom):
//   1. скрытие процесса из /proc (mount namespace isolation)
//   2. маскировка имени процесса (prctl + cmdline rewrite)
//   3. скрытие рута (magisk denylist + проверка артефактов)
//   4. обход детекта process_vm_readv (подмена /proc/pid/status)
//   5. скрытие overlay слоя от SurfaceFlinger enumeration
//   6. SELinux context fix
//   7. зачистка /proc/self/maps от подозрительных записей
//   8. защита от ptrace attach со стороны AC
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
#include <linux/sched.h>
#include <cerrno>
#include "diag.hpp"
#include "../protect/oxorany.hpp"

namespace stealth {

// ─── 1. маскировка имени процесса ───────────────────────────────────────────

// подменяем имя в /proc/self/comm и argv[0]
// AC сканит /proc/<pid>/cmdline и /proc/<pid>/comm
inline void disguise_process_name(const char* fake_name) {
    prctl(PR_SET_NAME, fake_name, 0, 0, 0);
    diag::log("STEALTH", "process name -> %s", fake_name);
}

// перезаписываем argv[0] — меняет /proc/self/cmdline
inline void disguise_cmdline(int argc, char** argv, const char* fake) {
    if (!argv || !argv[0]) return;

    // считаем суммарную длину всех argv
    size_t total = 0;
    for (int i = 0; i < argc; i++) {
        total += strlen(argv[i]) + 1;
    }

    memset(argv[0], 0, total);
    strncpy(argv[0], fake, total - 1);
    diag::log("STEALTH", "cmdline -> %s", fake);
}

// ─── 2. mount namespace isolation ───────────────────────────────────────────

// создаём отдельный mount namespace — наш процесс не будет виден
// при enumerate /proc из пространства игры
inline bool isolate_mount_namespace() {
    if (unshare(CLONE_NEWNS) != 0) {
        diag::log("STEALTH", "unshare(CLONE_NEWNS) failed errno=%d (need root)", errno);
        return false;
    }

    // делаем mount private чтобы наши изменения не текли наружу
    if (mount("none", "/", nullptr, MS_REC | MS_PRIVATE, nullptr) != 0) {
        diag::log("STEALTH", "mount MS_PRIVATE failed errno=%d", errno);
    }

    diag::log("STEALTH", "mount namespace isolated");
    return true;
}

// ─── 3. скрытие из /proc enumeration ────────────────────────────────────────

// bind-mount пустой директории поверх /proc/self чтобы AC не мог
// прочитать наши maps/status/fd из другого процесса
// (работает только в нашем mount namespace)
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

// некоторые AC проверяют SELinux контекст процесса
// ставим permissive для нашего домена
inline void fix_selinux() {
    // проверяем текущий режим
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

    // пробуем через запись
    fd = open("/sys/fs/selinux/enforce", O_WRONLY);
    if (fd >= 0) {
        write(fd, "0", 1);
        close(fd);
        diag::log("STEALTH", "SELinux -> permissive (global)");
        return;
    }

    // через magiskpolicy — менее палевно, меняет только для нашего контекста
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

// добавляем пакет игры в Magisk DenyList (бывший MagiskHide)
// чтобы игра не видела рут
inline void setup_magisk_hide(const char* game_pkg) {
    char cmd[256];

    // magisk --denylist add
    snprintf(cmd, sizeof(cmd), "magisk --denylist add %s >/dev/null 2>&1", game_pkg);
    system(cmd);

    // на старых версиях — magiskhide
    snprintf(cmd, sizeof(cmd), "magiskhide add %s >/dev/null 2>&1", game_pkg);
    system(cmd);

    diag::log("STEALTH", "magisk denylist configured for %s", game_pkg);
}

// скрываем su binary и другие артефакты рута
// AC проверяет access() к этим путям
inline void hide_root_artifacts() {
    const char* su_paths[] = {
        "/system/bin/su",
        "/system/xbin/su",
        "/sbin/su",
        "/data/local/su",
        "/data/local/bin/su",
        "/data/local/xbin/su",
    };

    // bind-mount /dev/null поверх su (если mount namespace изолирован)
    for (const char* path : su_paths) {
        struct stat st;
        if (stat(path, &st) == 0) {
            if (mount("/dev/null", path, nullptr, MS_BIND, nullptr) == 0) {
                diag::log("STEALTH", "hidden %s", path);
            }
        }
    }

    // прячем Magisk Manager
    const char* magisk_paths[] = {
        "/data/adb/magisk",
        "/data/adb/modules",
        "/sbin/.magisk",
    };
    for (const char* path : magisk_paths) {
        struct stat st;
        if (stat(path, &st) == 0) {
            const char* empty = "/data/local/tmp/.proc_hide";
            mkdir(empty, 0755);
            mount(empty, path, nullptr, MS_BIND, nullptr);
        }
    }
}

// ─── 6. anti-ptrace (защита от аттача AC) ───────────────────────────────────

// занимаем слот ptrace самим собой — AC не сможет аттачнуться
// к нашему процессу для анализа
inline bool self_ptrace_protect() {
    pid_t child = fork();
    if (child < 0) return false;

    if (child == 0) {
        // дочерний процесс аттачится к родителю
        pid_t parent = getppid();
        if (ptrace(PTRACE_ATTACH, parent, nullptr, nullptr) == 0) {
            waitpid(parent, nullptr, 0);
            ptrace(PTRACE_CONT, parent, nullptr, nullptr);
            // держим аттач навечно
            while (true) {
                int status;
                waitpid(parent, &status, 0);
                if (WIFEXITED(status) || WIFSIGNALED(status)) break;
                ptrace(PTRACE_CONT, parent, nullptr, nullptr);
            }
        }
        _exit(0);
    }

    // даём дочернему время аттачнуться
    usleep(100000);
    diag::log("STEALTH", "anti-ptrace guard pid=%d", child);
    return true;
}

// ─── 7. скрытие overlay от детекта ──────────────────────────────────────────

// AC может enum'ить слои через SurfaceFlinger dumpsys
// мы переименовываем наш слой в системное имя
// (делается при создании surface — патчим имя до вызова createSurface)
inline const char* get_stealth_surface_name() {
    // маскируем под системный UI overlay
    return "StatusBar";
}

// скрываем наш процесс от dumpsys SurfaceFlinger
// bind-mount'им /dev/null поверх dumpsys (грязно, но работает на некоторых AC)
inline void block_dumpsys() {
    const char* paths[] = {
        "/system/bin/dumpsys",
        "/system/bin/service",
    };
    for (const char* p : paths) {
        struct stat st;
        if (stat(p, &st) == 0) {
            // НЕ блокируем полностью — заменяем на скрипт-фильтр
            // (полная блокировка палится)
            // вместо этого, полагаемся на имя слоя "StatusBar"
        }
    }
}

// ─── 8. чистка maps от палевных записей ─────────────────────────────────────

// удаляем VMA naming для наших маппингов (Android 11+)
// AC может сканить /proc/self/maps на подозрительные имена
inline void clean_vma_names() {
    // prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, ...) — ставим пустое имя
    // на все наши анонимные маппинги
    FILE* f = fopen("/proc/self/maps", "r");
    if (!f) return;

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        // ищем наши маппинги (anonymous, rw)
        uintptr_t start, end;
        char perms[5];
        if (sscanf(line, "%lx-%lx %4s", &start, &end, perms) != 3) continue;

        // только rw- анонимные
        if (perms[0] != 'r' || perms[1] != 'w') continue;
        if (strchr(line, '/') != nullptr) continue; // не анонимный

        // убираем VMA name
        prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, start, end - start, "");
    }
    fclose(f);
    diag::log("STEALTH", "VMA names cleaned");
}

// ─── 9. обход detect'а process_vm_readv ─────────────────────────────────────

// некоторые AC мониторят /proc/<game_pid>/status на TracerPid != 0
// process_vm_readv НЕ ставит TracerPid (в отличие от ptrace) — это уже ок
// но AC может проверять /proc/<game_pid>/mem open handles через /proc/<pid>/fd
//
// мы используем direct syscall вместо libc обёртки чтобы обойти
// хуки на process_vm_readv в libc (если AC их ставит через LD_PRELOAD)

inline ssize_t safe_vm_readv(pid_t pid, const struct iovec* local,
                              unsigned long liovcnt,
                              const struct iovec* remote,
                              unsigned long riovcnt, unsigned long flags) {
    #if defined(__aarch64__)
    register long x8 __asm__("x8") = 270; // __NR_process_vm_readv
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

// ─── 10. suppress dumpable ──────────────────────────────────────────────────

// отключаем core dump и делаем наш процесс undumpable
// чтобы другие процессы не могли читать наш /proc/<pid>/mem
inline void make_undumpable() {
    prctl(PR_SET_DUMPABLE, 0);
    diag::log("STEALTH", "process set undumpable");
}

// ─── full stealth init ──────────────────────────────────────────────────────

struct stealth_opts {
    bool no_stealth       = false;
    bool no_namespace     = false;
    bool no_ptrace_guard  = false;
    const char* game_pkg  = "com.axlebolt.standoff2";
    const char* fake_name = "surfaceflinger"; // маскируемся под surfaceflinger
};

inline void init(int argc, char** argv, const stealth_opts& opts) {
    if (opts.no_stealth) {
        diag::log("STEALTH", "stealth disabled by flag");
        return;
    }

    diag::log("STEALTH", "initializing anti-detection...");

    // 1. undumpable первым делом
    make_undumpable();

    // 2. mount namespace (до всего остального)
    if (!opts.no_namespace) {
        isolate_mount_namespace();
    }

    // 3. маскировка процесса
    disguise_process_name(opts.fake_name);
    disguise_cmdline(argc, argv, opts.fake_name);

    // 4. SELinux
    fix_selinux();

    // 5. root hiding
    setup_magisk_hide(opts.game_pkg);
    if (!opts.no_namespace) {
        hide_root_artifacts();
    }

    // 6. скрытие /proc/<pid> (работает только с mount namespace)
    if (!opts.no_namespace) {
        hide_proc_self();
    }

    // 7. чистка VMA names
    clean_vma_names();

    // 8. anti-ptrace guard
    if (!opts.no_ptrace_guard) {
        self_ptrace_protect();
    }

    diag::log("STEALTH", "anti-detection initialized");
}

} // namespace stealth
