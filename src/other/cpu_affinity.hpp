// ================================================================================
// Keep the overlay off the cores the game renders on. On big.LITTLE SoCs the
// game's main/render threads sit on the fast clusters; the overlay's few
// milliseconds per frame fit on the slowest cluster. The cluster is measured
// from cpufreq (cpuinfo_max_freq) at runtime, never assumed. A homogeneous
// CPU or unreadable cpufreq leaves affinity untouched.
// ================================================================================

#pragma once

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <vector>
#include <sched.h>
#include <unistd.h>
#include "diag.hpp"

namespace affinity {
    // Pure: CPUs of the lowest max-frequency cluster, or empty when the
    // frequencies do not show at least two clusters. 0 = unreadable CPU.
    inline std::vector<int> efficiency_cpus(const std::vector<long>& max_khz) {
        long lo = 0, hi = 0;
        for (long f : max_khz) {
            if (f <= 0) continue;
            if (!lo || f < lo) lo = f;
            if (f > hi) hi = f;
        }
        std::vector<int> out;
        if (!lo || lo == hi) return out;
        for (size_t i = 0; i < max_khz.size(); ++i)
            if (max_khz[i] == lo) out.push_back(int(i));
        // One core is too little room for the frame; all cores is no change.
        if (out.size() < 2 || out.size() >= max_khz.size()) out.clear();
        return out;
    }

    inline long read_max_khz(int cpu) {
        char path[96];
        std::snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_max_freq", cpu);
        FILE* f = std::fopen(path, "r");
        if (!f) return 0;
        long v = 0;
        if (std::fscanf(f, "%ld", &v) != 1) v = 0;
        std::fclose(f);
        return v;
    }

    // Applies to the calling thread and to every thread it creates later.
    inline bool pin_to_efficiency_cluster() {
        const long n = ::sysconf(_SC_NPROCESSORS_CONF);
        if (n <= 1 || n > CPU_SETSIZE) return false;
        std::vector<long> freqs(size_t(n), 0);
        for (long i = 0; i < n; ++i) freqs[size_t(i)] = read_max_khz(int(i));
        const std::vector<int> cpus = efficiency_cpus(freqs);
        if (cpus.empty()) {
            diag::log("INFO", "cpu affinity unchanged (no distinct efficiency cluster in cpufreq)");
            return false;
        }
        cpu_set_t set;
        CPU_ZERO(&set);
        char list[128] = {0};
        size_t used = 0;
        for (int cpu : cpus) {
            CPU_SET(cpu, &set);
            const int w = std::snprintf(list + used, sizeof(list) - used, used ? ",%d" : "%d", cpu);
            if (w > 0 && size_t(w) < sizeof(list) - used) used += size_t(w);
        }
        if (::sched_setaffinity(0, sizeof(set), &set) != 0) {
            diag::log("WARN", "sched_setaffinity(%s) failed errno=%d; overlay stays on all cores", list, errno);
            return false;
        }
        diag::log("INFO", "overlay pinned to efficiency cores %s (%ld kHz max)", list, freqs[size_t(cpus.front())]);
        return true;
    }
}
