#pragma once
#include <vector>
#include <cstdint>

namespace aim_targets {

struct Target {
    float x{}, y{};
    float distance{};
    float hp{}, max_hp{};
    uint64_t object{};
    float wx{}, wy{}, wz{};
};

void publish(const std::vector<Target>& targets);
void set_local(uint64_t object);
uint64_t local();
std::vector<Target> snapshot();
bool valid();

}
