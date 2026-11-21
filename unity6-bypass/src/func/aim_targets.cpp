#include "aim_targets.hpp"
#include <mutex>

namespace aim_targets {

static std::mutex s_mutex;
static std::vector<Target> s_targets;
static bool s_valid = false;
static uint64_t s_local = 0;

void publish(const std::vector<Target>& targets) {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_targets = targets;
    s_valid = !targets.empty();
}

std::vector<Target> snapshot() {
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_targets;
}

void set_local(uint64_t object) {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_local = object;
}

uint64_t local() {
    std::lock_guard<std::mutex> lock(s_mutex);
    return s_local;
}

bool valid() {
    return s_valid;
}

}
