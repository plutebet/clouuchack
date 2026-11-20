
// ================================================================================
// Telegram Hub: @imguisource_bot / ImGui Source @ImGui_Source_FIXNEY / shit src 🧨 @il2cpp_src
// ================================================================================

#pragma once
#include <array>
#include <cstdint>

#include "decryptor.hpp"

// Thin compatibility layer over the dump's `decryptor` namespace
// (decryptions.txt -> decryptor.hpp). All algorithms live in that file;
// this header only keeps the old Decryption::* names the base uses.
//
// Main.cpp installs the two dump hooks once at startup:
//   decryptor::read       -> Driver->Read<uint64_t>
//   decryptor::get_handle -> Il2CppHandle::Il2cppGetHandle

namespace Decryption {

// --- Pointer decryptors (read encrypted GC handle at address + 0x18) ---

inline uintptr_t decrypt_client_entities(uintptr_t address, uintptr_t) {
    return static_cast<uintptr_t>(decryptor::decrypt_client_entities(address));
}

inline uintptr_t decrypt_entity_list(uintptr_t address, uintptr_t) {
    return static_cast<uintptr_t>(decryptor::decrypt_entity_list(address));
}

inline uintptr_t decrypt_player_eyes(uintptr_t address, uintptr_t) {
    return static_cast<uintptr_t>(decryptor::decrypt_eyes(address));
}

inline uintptr_t DecryptPlayerInventory(uintptr_t address, uintptr_t) {
    return static_cast<uintptr_t>(decryptor::decrypt_inventory(address));
}

inline uintptr_t decrypt_local_player(uintptr_t address, uintptr_t) {
    return static_cast<uintptr_t>(decryptor::decrypt_localplayer(address));
}

// --- Pure value decryptors ---

inline uint64_t cl_active_item(uint64_t encrypted) {
    return decryptor::decrypt_clactiveitem(encrypted);
}

inline float decrypt_fov(uint32_t val) { return decryptor::decrypt_fov(val); }
inline uint32_t encrypt_fov(float val) { return decryptor::encrypt_fov(val); }

inline float decrypt_client_tick_interval(uint32_t value) {
    return decryptor::decrypt_client_tick_interval(value);
}
inline uint32_t encrypt_client_tick_interval(float value) {
    return decryptor::encrypt_client_tick_interval(value);
}

inline float decrypt_ground_angle(uint32_t value) {
    return decryptor::decrypt_ground_angle(value);
}
inline uint32_t encrypt_ground_angle(float value) {
    return decryptor::encrypt_ground_angle(value);
}
inline float decrypt_ground_angle_new(uint32_t value) {
    return decryptor::decrypt_ground_angle_new(value);
}
inline uint32_t encrypt_ground_angle_new(float value) {
    return decryptor::encrypt_ground_angle_new(value);
}

inline std::array<uint32_t, 3> decrypt_ground_normal(
    std::array<uint32_t, 3> value) {
    return decryptor::decrypt_ground_normal(value);
}
inline std::array<uint32_t, 3> encrypt_ground_normal(
    std::array<uint32_t, 3> value) {
    return decryptor::encrypt_ground_normal(value);
}
inline std::array<uint32_t, 3> decrypt_ground_normal_new(
    std::array<uint32_t, 3> value) {
    return decryptor::decrypt_ground_normal_new(value);
}
inline std::array<uint32_t, 3> encrypt_ground_normal_new(
    std::array<uint32_t, 3> value) {
    return decryptor::encrypt_ground_normal_new(value);
}

inline float decrypt_ground_time(uint32_t value) {
    return decryptor::decrypt_ground_time(value);
}
inline uint32_t encrypt_ground_time(float value) {
    return decryptor::encrypt_ground_time(value);
}

inline std::array<uint32_t, 3> decrypt_ground_velocity(
    std::array<uint32_t, 3> value) {
    return decryptor::decrypt_ground_velocity(value);
}
inline std::array<uint32_t, 3> encrypt_ground_velocity(
    std::array<uint32_t, 3> value) {
    return decryptor::encrypt_ground_velocity(value);
}
inline std::array<uint32_t, 3> decrypt_ground_velocity_new(
    std::array<uint32_t, 3> value) {
    return decryptor::decrypt_ground_velocity_new(value);
}
inline std::array<uint32_t, 3> encrypt_ground_velocity_new(
    std::array<uint32_t, 3> value) {
    return decryptor::encrypt_ground_velocity_new(value);
}

inline float decrypt_jump_time(uint32_t value) {
    return decryptor::decrypt_jump_time(value);
}
inline uint32_t encrypt_jump_time(float value) {
    return decryptor::encrypt_jump_time(value);
}
inline float decrypt_land_time(uint32_t value) {
    return decryptor::decrypt_land_time(value);
}
inline uint32_t encrypt_land_time(float value) {
    return decryptor::encrypt_land_time(value);
}
inline float decrypt_last_tick_time(uint32_t value) {
    return decryptor::decrypt_last_tick_time(value);
}
inline uint32_t encrypt_last_tick_time(float value) {
    return decryptor::encrypt_last_tick_time(value);
}
inline float decrypt_lastsentticktime(uint32_t value) {
    return decryptor::decrypt_lastsentticktime(value);
}
inline float decrypt_max_velocity(uint32_t value) {
    return decryptor::decrypt_max_velocity(value);
}
inline uint32_t encrypt_max_velocity(float value) {
    return decryptor::encrypt_max_velocity(value);
}
inline float decrypt_next_attack_time(uint32_t value) {
    return decryptor::decrypt_next_attack_time(value);
}
inline uint32_t encrypt_next_attack_time(float value) {
    return decryptor::encrypt_next_attack_time(value);
}
inline float decrypt_next_reload_time(uint32_t value) {
    return decryptor::decrypt_next_reload_time(value);
}
inline uint32_t encrypt_next_reload_time(float value) {
    return decryptor::encrypt_next_reload_time(value);
}
inline float decrypt_next_tick_time(uint32_t value) {
    return decryptor::decrypt_next_tick_time(value);
}
inline uint32_t encrypt_next_tick_time(float value) {
    return decryptor::encrypt_next_tick_time(value);
}

inline std::array<uint32_t, 3> decrypt_previous_inherited_velocity(
    std::array<uint32_t, 3> value) {
    return decryptor::decrypt_previous_inherited_velocity(value);
}
inline std::array<uint32_t, 3> encrypt_previous_inherited_velocity(
    std::array<uint32_t, 3> value) {
    return decryptor::encrypt_previous_inherited_velocity(value);
}
inline std::array<uint32_t, 3> decrypt_previous_position(
    std::array<uint32_t, 3> value) {
    return decryptor::decrypt_previous_position(value);
}
inline std::array<uint32_t, 3> encrypt_previous_position(
    std::array<uint32_t, 3> value) {
    return decryptor::encrypt_previous_position(value);
}

inline float decrypt_start_reload_time(uint32_t value) {
    return decryptor::decrypt_start_reload_time(value);
}
inline uint32_t encrypt_start_reload_time(float value) {
    return decryptor::encrypt_start_reload_time(value);
}
inline float decrypt_time_since_deploy(uint32_t value) {
    return decryptor::decrypt_time_since_deploy(value);
}
inline uint32_t encrypt_time_since_deploy(float value) {
    return decryptor::encrypt_time_since_deploy(value);
}

inline std::array<uint32_t, 3> decrypt_viewoffset(
    std::array<uint32_t, 3> value) {
    return decryptor::decrypt_viewoffset(value);
}
inline std::array<uint32_t, 3> encrypt_viewoffset(
    std::array<uint32_t, 3> value) {
    return decryptor::encrypt_viewoffset(value);
}

}  // namespace Decryption
