#pragma once

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

inline void disguise_process_name(const char* fake_name) {
    prctl(PR_SET_NAME, fake_name, 0, 0, 0);
}

inline void disguise_cmdline(int argc, char** argv, const char* fake) {
    if (!argv || !argv[0]) return;
    size_t total = 0;
    for (int i = 0; i < argc; i++) {
        total += strlen(argv[i]) + 1;
    }
    memset(argv[0], 0, total);
    strncpy(argv[0], fake, total - 1);
}

inline bool isolate_mount_namespace() {
    if (unshare(CLONE_NEWNS) != 0) return false;
    mount(oxorany("none"), oxorany("/"), nullptr, MS_REC | MS_PRIVATE, nullptr);
    return true;
}

inline bool hide_proc_self() {
    const char* tmpdir = oxorany("/data/local/tmp/.sf_cache");
    mkdir(tmpdir, 0755);

    char self_path[64];
    snprintf(self_path, sizeof(self_path), oxorany("/proc/%d"), getpid());

    return mount(tmpdir, self_path, nullptr, MS_BIND, nullptr) == 0;
}

inline void fix_selinux() {
    int fd = open(oxorany("/sys/fs/selinux/enforce"), O_RDONLY);
    if (fd >= 0) {
        char mode = '1';
        if (read(fd, &mode, 1) > 0 && mode == '0') {
            close(fd);
            return;
        }
        close(fd);
    }

    fd = open(oxorany("/sys/fs/selinux/enforce"), O_WRONLY);
    if (fd >= 0) {
        write(fd, "0", 1);
        close(fd);
        return;
    }

    system(oxorany("magiskpolicy --live "
        "'allow untrusted_app untrusted_app process ptrace' "
        "'allow shell shell process ptrace' "
        "'allow untrusted_app app_data_file file { read write open }'"
        " >/dev/null 2>&1"));
}

inline void setup_magisk_hide(const char* game_pkg) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), oxorany("magisk --denylist add %s >/dev/null 2>&1"), game_pkg);
    system(cmd);
    snprintf(cmd, sizeof(cmd), oxorany("magiskhide add %s >/dev/null 2>&1"), game_pkg);
    system(cmd);
}

inline void hide_root_artifacts() {
    const char* su_paths[] = {
        oxorany("/system/bin/su"),
        oxorany("/system/xbin/su"),
        oxorany("/sbin/su"),
        oxorany("/data/local/su"),
        oxorany("/data/local/bin/su"),
        oxorany("/data/local/xbin/su"),
    };
    for (const char* path : su_paths) {
        struct stat st;
        if (stat(path, &st) == 0) {
            mount(oxorany("/dev/null"), path, nullptr, MS_BIND, nullptr);
        }
    }

    const char* magisk_paths[] = {
        oxorany("/data/adb/magisk"),
        oxorany("/data/adb/modules"),
        oxorany("/sbin/.magisk"),
    };
    const char* empty = oxorany("/data/local/tmp/.sf_cache");
    mkdir(empty, 0755);
    for (const char* path : magisk_paths) {
        struct stat st;
        if (stat(path, &st) == 0) {
            mount(empty, path, nullptr, MS_BIND, nullptr);
        }
    }
}

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
    return true;
}

inline void clean_vma_names() {
    FILE* f = fopen(oxorany("/proc/self/maps"), "r");
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
}

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

inline void make_undumpable() {
    prctl(PR_SET_DUMPABLE, 0);
}

inline void hide_instrumentation_traces() {
    const char* suspicious_paths[] = {
        oxorany("/data/local/tmp/frida-server"),
        oxorany("/data/local/tmp/re.frida.server"),
        oxorany("/system/lib/libxposed_art.so"),
        oxorany("/system/lib64/libxposed_art.so"),
        oxorany("/system/framework/XposedBridge.jar"),
        oxorany("/data/adb/lspd"),
        oxorany("/data/adb/shamiko"),
    };
    for (const char* path : suspicious_paths) {
        struct stat st;
        if (stat(path, &st) == 0) {
            mount(oxorany("/dev/null"), path, nullptr, MS_BIND, nullptr);
        }
    }
}

inline void verify_no_tracer_leak(int game_pid) {
    char path[64];
    snprintf(path, sizeof(path), oxorany("/proc/%d/status"), game_pid);
    FILE* f = fopen(path, "r");
    if (!f) return;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, oxorany("TracerPid:"), 10) == 0) {
            int tracer = 0;
            sscanf(line + 10, "%d", &tracer);
            if (tracer != 0) {
                diag::log("S", "game TracerPid=%d", tracer);
            }
            break;
        }
    }
    fclose(f);
}

inline void randomize_timing() {
    unsigned int seed = 0;
    int urandom = open(oxorany("/dev/urandom"), O_RDONLY);
    if (urandom >= 0) {
        read(urandom, &seed, sizeof(seed));
        close(urandom);
    }
    usleep((seed % 500) * 1000 + 100000);
}

inline const char* get_stealth_surface_name() {
    return oxorany("StatusBar");
}

struct stealth_opts {
    bool no_stealth       = false;
    bool no_namespace     = false;
    bool no_ptrace_guard  = false;
    const char* game_pkg  = "com.axlebolt.standoff2";
    const char* fake_name = "surfaceflinger";
};

inline void init(int argc, char** argv, const stealth_opts& opts) {
    if (opts.no_stealth) return;

    make_undumpable();

    if (!opts.no_namespace)
        isolate_mount_namespace();

    disguise_process_name(opts.fake_name);
    disguise_cmdline(argc, argv, opts.fake_name);
    randomize_timing();
    fix_selinux();
    setup_magisk_hide(opts.game_pkg);

    if (!opts.no_namespace) {
        hide_root_artifacts();
        hide_proc_self();
        hide_instrumentation_traces();
    }

    clean_vma_names();

    if (!opts.no_ptrace_guard)
        self_ptrace_protect();

    diag::log("S", "init ok");
}

inline void post_game_check(int game_pid) {
    if (game_pid > 0)
        verify_no_tracer_leak(game_pid);
}

} // namespace stealth
