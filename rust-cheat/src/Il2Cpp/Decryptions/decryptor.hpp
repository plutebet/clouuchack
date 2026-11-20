
// ================================================================================
// Эксклюзив от @imguisource_bot
// Канал: @ImGui_Source_FIXNEY (ImGui Source)
// Резерв: @il2cpp_src (shit src 🧨)
// ================================================================================

#pragma once

#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <functional>

// Generated from alllatest.txt (dump date 11/06/2026, game update 2026-11-07).
// All algorithms are the dump's decryptor programs, flattened into the
// names the client historically used.
namespace decryptor
{
    inline auto read = std::function<std::uint64_t(std::uint64_t)>();
    inline auto get_handle = std::function<std::uint64_t(std::uint64_t)>();

    inline auto decrypt_clactiveitem(std::uint64_t value) -> std::uint64_t
    {
        auto words = std::bit_cast<std::array<std::uint32_t, 2>>(value);

        for (auto& word : words)
        {
            word ^= 0xf6757db7u;
            word += 0x2fcc66feu;
            word = std::rotl(word, 0xfu);
        }

        return std::bit_cast<std::uint64_t>(words);
    }

    inline auto decrypt_client_entities(std::uint64_t value) -> std::uint64_t
    {
        if (!value || (decryptor::read(value + 0x10ull) & 0xFFu) == 0)
            return 0;

        const auto handle = decryptor::read(value + 0x18ull);

        auto words = std::bit_cast<std::array<std::uint32_t, 2>>(handle);

        for (auto& word : words)
        {
            word = std::rotl(word, 0x8u);
            word ^= 0x9ccf7239u;
            word = std::rotl(word, 0x17u);
        }

        return decryptor::get_handle(std::bit_cast<std::uint64_t>(words));
    }

    inline auto decrypt_client_tick_interval(std::uint32_t value) -> float
    {
        auto words = std::bit_cast<std::array<std::uint32_t, 1>>(value);

        for (auto& word : words)
        {
            word += 0xf01aa56u;
            word = std::rotl(word, 0x2u);
            word += 0x2b323c2du;
        }

        auto result = 0.00f;
        std::memcpy(&result, words.data(), sizeof(result));

        return result;
    }

    inline auto encrypt_client_tick_interval(float value) -> std::uint32_t
    {
        auto result = std::uint32_t();
        std::memcpy(&result, &value, sizeof(result));

        auto words = std::bit_cast<std::array<std::uint32_t, 1>>(result);

        for (auto& word : words)
        {
            word += 0xd4cdc3d3u;
            word = std::rotl(word, 0x1eu);
            word += 0xf0fe55aau;
        }

        return words[0];
    }

    inline auto decrypt_entity_list(std::uint64_t value) -> std::uint64_t
    {
        if (!value || (decryptor::read(value + 0x10ull) & 0xFFu) == 0)
            return 0;

        const auto handle = decryptor::read(value + 0x18ull);

        auto words = std::bit_cast<std::array<std::uint32_t, 2>>(handle);

        for (auto& word : words)
        {
            word += 0x5333bdc6u;
            word = std::rotl(word, 0xcu);
            word += 0x19a12c8fu;
        }

        return decryptor::get_handle(std::bit_cast<std::uint64_t>(words));
    }

    inline auto decrypt_eyes(std::uint64_t value) -> std::uint64_t
    {
        if (!value || (decryptor::read(value + 0x10ull) & 0xFFu) == 0)
            return 0;

        const auto handle = decryptor::read(value + 0x18ull);

        auto words = std::bit_cast<std::array<std::uint32_t, 2>>(handle);

        for (auto& word : words)
        {
            word ^= 0xf6757db7u;
            word += 0x25cf2c1eu;
            word = std::rotl(word, 0xfu);
        }

        return decryptor::get_handle(std::bit_cast<std::uint64_t>(words));
    }

    inline auto decrypt_fov(std::uint32_t value) -> float
    {
        auto words = std::bit_cast<std::array<std::uint32_t, 1>>(value);

        for (auto& word : words)
        {
            word += 0xf01aa56u;
            word = std::rotl(word, 0x2u);
            word += 0x2b323c2du;
        }

        auto result = 0.00f;
        std::memcpy(&result, words.data(), sizeof(result));

        return result;
    }

    inline auto encrypt_fov(float value) -> std::uint32_t
    {
        auto result = std::uint32_t();
        std::memcpy(&result, &value, sizeof(result));

        auto words = std::bit_cast<std::array<std::uint32_t, 1>>(result);

        for (auto& word : words)
        {
            word += 0xd4cdc3d3u;
            word = std::rotl(word, 0x1eu);
            word += 0xf0fe55aau;
        }

        return words[0];
    }

    inline auto decrypt_ground_angle(std::uint32_t value) -> float
    {
        auto words = std::bit_cast<std::array<std::uint32_t, 1>>(value);

        for (auto& word : words)
        {
            word ^= 0xb2dfcb34u;
            word += 0x4b9ebd0du;
            word = std::rotl(word, 0xbu);
        }

        auto result = 0.00f;
        std::memcpy(&result, words.data(), sizeof(result));

        return result;
    }

    inline auto encrypt_ground_angle(float value) -> std::uint32_t
    {
        auto result = std::uint32_t();
        std::memcpy(&result, &value, sizeof(result));

        auto words = std::bit_cast<std::array<std::uint32_t, 1>>(result);

        for (auto& word : words)
        {
            word = std::rotl(word, 0x15u);
            word += 0xb46142f3u;
            word ^= 0xb2dfcb34u;
        }

        return words[0];
    }

    inline auto decrypt_ground_angle_new(std::uint32_t value) -> float
    {
        auto words = std::bit_cast<std::array<std::uint32_t, 1>>(value);

        for (auto& word : words)
        {
            word ^= 0xea60b3bfu;
            word += 0xfc7b4088u;
            word = std::rotl(word, 0x4u);
            word += 0x6f433474u;
        }

        auto result = 0.00f;
        std::memcpy(&result, words.data(), sizeof(result));

        return result;
    }

    inline auto encrypt_ground_angle_new(float value) -> std::uint32_t
    {
        auto result = std::uint32_t();
        std::memcpy(&result, &value, sizeof(result));

        auto words = std::bit_cast<std::array<std::uint32_t, 1>>(result);

        for (auto& word : words)
        {
            word += 0x90bccb8cu;
            word = std::rotl(word, 0x1cu);
            word += 0x384bf78u;
            word ^= 0xea60b3bfu;
        }

        return words[0];
    }

    inline auto decrypt_ground_normal(std::array<std::uint32_t, 3> value) -> std::array<std::uint32_t, 3>
    {
        auto words = std::bit_cast<std::array<std::uint32_t, 3>>(value);

        for (auto& word : words)
        {
            word = std::rotl(word, 0x5u);
            word ^= 0x6842fcb4u;
            word += 0x84a49387u;
            word ^= 0x37dacf39u;
        }

        return std::bit_cast<std::array<std::uint32_t, 3>>(words);
    }

    inline auto encrypt_ground_normal(std::array<std::uint32_t, 3> value) -> std::array<std::uint32_t, 3>
    {
        auto words = std::bit_cast<std::array<std::uint32_t, 3>>(value);

        for (auto& word : words)
        {
            word ^= 0x37dacf39u;
            word += 0x7b5b6c79u;
            word ^= 0x6842fcb4u;
            word = std::rotl(word, 0x1bu);
        }

        return std::bit_cast<std::array<std::uint32_t, 3>>(words);
    }

    inline auto decrypt_ground_normal_new(std::array<std::uint32_t, 3> value) -> std::array<std::uint32_t, 3>
    {
        auto words = std::bit_cast<std::array<std::uint32_t, 3>>(value);

        for (auto& word : words)
        {
            word = std::rotl(word, 0x11u);
            word ^= 0x86432606u;
            word = std::rotl(word, 0x3u);
        }

        return std::bit_cast<std::array<std::uint32_t, 3>>(words);
    }

    inline auto encrypt_ground_normal_new(std::array<std::uint32_t, 3> value) -> std::array<std::uint32_t, 3>
    {
        auto words = std::bit_cast<std::array<std::uint32_t, 3>>(value);

        for (auto& word : words)
        {
            word = std::rotl(word, 0x1du);
            word ^= 0x86432606u;
            word = std::rotl(word, 0xfu);
        }

        return std::bit_cast<std::array<std::uint32_t, 3>>(words);
    }

    inline auto decrypt_ground_time(std::uint32_t value) -> float
    {
        auto words = std::bit_cast<std::array<std::uint32_t, 1>>(value);

        for (auto& word : words)
        {
            word = std::rotl(word, 0x1bu);
            word += 0xebceefc2u;
            word ^= 0xd41b4113u;
            word += 0x6a270709u;
        }

        auto result = 0.00f;
        std::memcpy(&result, words.data(), sizeof(result));

        return result;
    }

    inline auto encrypt_ground_time(float value) -> std::uint32_t
    {
        auto result = std::uint32_t();
        std::memcpy(&result, &value, sizeof(result));

        auto words = std::bit_cast<std::array<std::uint32_t, 1>>(result);

        for (auto& word : words)
        {
            word += 0x95d8f8f7u;
            word ^= 0xd41b4113u;
            word += 0x1431103eu;
            word = std::rotl(word, 0x5u);
        }

        return words[0];
    }

    inline auto decrypt_ground_velocity(std::array<std::uint32_t, 3> value) -> std::array<std::uint32_t, 3>
    {
        auto words = std::bit_cast<std::array<std::uint32_t, 3>>(value);

        for (auto& word : words)
        {
            word = std::rotl(word, 0x18u);
            word += 0x9f070909u;
            word = std::rotl(word, 0xdu);
            word += 0x79ec38efu;
        }

        return std::bit_cast<std::array<std::uint32_t, 3>>(words);
    }

    inline auto encrypt_ground_velocity(std::array<std::uint32_t, 3> value) -> std::array<std::uint32_t, 3>
    {
        auto words = std::bit_cast<std::array<std::uint32_t, 3>>(value);

        for (auto& word : words)
        {
            word += 0x8613c711u;
            word = std::rotl(word, 0x13u);
            word += 0x60f8f6f7u;
            word = std::rotl(word, 0x8u);
        }

        return std::bit_cast<std::array<std::uint32_t, 3>>(words);
    }

    inline auto decrypt_ground_velocity_new(std::array<std::uint32_t, 3> value) -> std::array<std::uint32_t, 3>
    {
        auto words = std::bit_cast<std::array<std::uint32_t, 3>>(value);

        for (auto& word : words)
        {
            word += 0x85c7c023u;
            word = std::rotl(word, 0x4u);
            word ^= 0x338b38ddu;
        }

        return std::bit_cast<std::array<std::uint32_t, 3>>(words);
    }

    inline auto encrypt_ground_velocity_new(std::array<std::uint32_t, 3> value) -> std::array<std::uint32_t, 3>
    {
        auto words = std::bit_cast<std::array<std::uint32_t, 3>>(value);

        for (auto& word : words)
        {
            word ^= 0x338b38ddu;
            word = std::rotl(word, 0x1cu);
            word += 0x7a383fddu;
        }

        return std::bit_cast<std::array<std::uint32_t, 3>>(words);
    }

    inline auto decrypt_inventory(std::uint64_t value) -> std::uint64_t
    {
        if (!value || (decryptor::read(value + 0x10ull) & 0xFFu) == 0)
            return 0;

        const auto handle = decryptor::read(value + 0x18ull);

        auto words = std::bit_cast<std::array<std::uint32_t, 2>>(handle);

        for (auto& word : words)
        {
            word += 0x5e7d8b6cu;
            word = std::rotl(word, 0xbu);
            word += 0xe9b1eacau;
        }

        return decryptor::get_handle(std::bit_cast<std::uint64_t>(words));
    }

    inline auto decrypt_jump_time(std::uint32_t value) -> float
    {
        auto words = std::bit_cast<std::array<std::uint32_t, 1>>(value);

        for (auto& word : words)
        {
            word ^= 0x84b9143du;
            word = std::rotl(word, 0x4u);
            word ^= 0x33dd7b84u;
        }

        auto result = 0.00f;
        std::memcpy(&result, words.data(), sizeof(result));

        return result;
    }

    inline auto encrypt_jump_time(float value) -> std::uint32_t
    {
        auto result = std::uint32_t();
        std::memcpy(&result, &value, sizeof(result));

        auto words = std::bit_cast<std::array<std::uint32_t, 1>>(result);

        for (auto& word : words)
        {
            word ^= 0x33dd7b84u;
            word = std::rotl(word, 0x1cu);
            word ^= 0x84b9143du;
        }

        return words[0];
    }

    inline auto decrypt_land_time(std::uint32_t value) -> float
    {
        auto words = std::bit_cast<std::array<std::uint32_t, 1>>(value);

        for (auto& word : words)
        {
            word += 0x1a49a0e1u;
            word = std::rotl(word, 0x11u);
            word += 0xf7473b09u;
        }

        auto result = 0.00f;
        std::memcpy(&result, words.data(), sizeof(result));

        return result;
    }

    inline auto encrypt_land_time(float value) -> std::uint32_t
    {
        auto result = std::uint32_t();
        std::memcpy(&result, &value, sizeof(result));

        auto words = std::bit_cast<std::array<std::uint32_t, 1>>(result);

        for (auto& word : words)
        {
            word += 0x8b8c4f7u;
            word = std::rotl(word, 0xfu);
            word += 0xe5b65f1fu;
        }

        return words[0];
    }

    inline auto decrypt_lastsentticktime(std::uint32_t value) -> float
    {
        auto words = std::bit_cast<std::array<std::uint32_t, 1>>(value);

        for (auto& word : words)
        {
            word = std::rotl(word, 0x14u);
            word += 0xdd5f2080u;
            word ^= 0x23b538e0u;
        }

        auto result = 0.00f;
        std::memcpy(&result, words.data(), sizeof(result));

        return result;
    }

    inline auto decrypt_last_tick_time(std::uint32_t value) -> float
    {
        auto words = std::bit_cast<std::array<std::uint32_t, 1>>(value);

        for (auto& word : words)
        {
            word += 0xc9d253ebu;
            word ^= 0x94801eb9u;
            word = std::rotl(word, 0x1u);
            word += 0xeeb9f515u;
        }

        auto result = 0.00f;
        std::memcpy(&result, words.data(), sizeof(result));

        return result;
    }

    inline auto encrypt_last_tick_time(float value) -> std::uint32_t
    {
        auto result = std::uint32_t();
        std::memcpy(&result, &value, sizeof(result));

        auto words = std::bit_cast<std::array<std::uint32_t, 1>>(result);

        for (auto& word : words)
        {
            word += 0x11460aebu;
            word = std::rotl(word, 0x1fu);
            word ^= 0x94801eb9u;
            word += 0x362dac15u;
        }

        return words[0];
    }

    inline auto decrypt_localplayer(std::uint64_t value) -> std::uint64_t
    {
        if (!value || (decryptor::read(value + 0x10ull) & 0xFFu) == 0)
            return 0;

        const auto handle = decryptor::read(value + 0x18ull);

        auto words = std::bit_cast<std::array<std::uint32_t, 2>>(handle);

        for (auto& word : words)
        {
            word = std::rotl(word, 0x8u);
            word ^= 0x9ccf7239u;
            word = std::rotl(word, 0x17u);
        }

        return decryptor::get_handle(std::bit_cast<std::uint64_t>(words));
    }

    inline auto decrypt_max_velocity(std::uint32_t value) -> float
    {
        auto words = std::bit_cast<std::array<std::uint32_t, 1>>(value);

        for (auto& word : words)
        {
            word ^= 0x61afb7au;
            word += 0x952d8604u;
            word ^= 0x776ba0eeu;
        }

        auto result = 0.00f;
        std::memcpy(&result, words.data(), sizeof(result));

        return result;
    }

    inline auto encrypt_max_velocity(float value) -> std::uint32_t
    {
        auto result = std::uint32_t();
        std::memcpy(&result, &value, sizeof(result));

        auto words = std::bit_cast<std::array<std::uint32_t, 1>>(result);

        for (auto& word : words)
        {
            word ^= 0x776ba0eeu;
            word += 0x6ad279fcu;
            word ^= 0x61afb7au;
        }

        return words[0];
    }

    inline auto decrypt_next_attack_time(std::uint32_t value) -> float
    {
        auto words = std::bit_cast<std::array<std::uint32_t, 1>>(value);

        for (auto& word : words)
        {
            word = std::rotl(word, 0x7u);
            word += 0x7e724c8bu;
            word = std::rotl(word, 0xbu);
        }

        auto result = 0.00f;
        std::memcpy(&result, words.data(), sizeof(result));

        return result;
    }

    inline auto encrypt_next_attack_time(float value) -> std::uint32_t
    {
        auto result = std::uint32_t();
        std::memcpy(&result, &value, sizeof(result));

        auto words = std::bit_cast<std::array<std::uint32_t, 1>>(result);

        for (auto& word : words)
        {
            word = std::rotl(word, 0x15u);
            word += 0x818db375u;
            word = std::rotl(word, 0x19u);
        }

        return words[0];
    }

    inline auto decrypt_next_reload_time(std::uint32_t value) -> float
    {
        auto words = std::bit_cast<std::array<std::uint32_t, 1>>(value);

        for (auto& word : words)
        {
            word = std::rotl(word, 0x6u);
            word ^= 0x2bb7af36u;
            word = std::rotl(word, 0x1du);
            word += 0x605a160du;
        }

        auto result = 0.00f;
        std::memcpy(&result, words.data(), sizeof(result));

        return result;
    }

    inline auto encrypt_next_reload_time(float value) -> std::uint32_t
    {
        auto result = std::uint32_t();
        std::memcpy(&result, &value, sizeof(result));

        auto words = std::bit_cast<std::array<std::uint32_t, 1>>(result);

        for (auto& word : words)
        {
            word += 0x9fa5e9f3u;
            word = std::rotl(word, 0x3u);
            word ^= 0x2bb7af36u;
            word = std::rotl(word, 0x1au);
        }

        return words[0];
    }

    inline auto decrypt_next_tick_time(std::uint32_t value) -> float
    {
        auto words = std::bit_cast<std::array<std::uint32_t, 1>>(value);

        for (auto& word : words)
        {
            word = std::rotl(word, 0x17u);
            word += 0x48a12b98u;
            word = std::rotl(word, 0x16u);
            word ^= 0xbb9b331du;
        }

        auto result = 0.00f;
        std::memcpy(&result, words.data(), sizeof(result));

        return result;
    }

    inline auto encrypt_next_tick_time(float value) -> std::uint32_t
    {
        auto result = std::uint32_t();
        std::memcpy(&result, &value, sizeof(result));

        auto words = std::bit_cast<std::array<std::uint32_t, 1>>(result);

        for (auto& word : words)
        {
            word ^= 0xbb9b331du;
            word = std::rotl(word, 0xau);
            word += 0xb75ed468u;
            word = std::rotl(word, 0x9u);
        }

        return words[0];
    }

    inline auto decrypt_previous_inherited_velocity(std::array<std::uint32_t, 3> value) -> std::array<std::uint32_t, 3>
    {
        auto words = std::bit_cast<std::array<std::uint32_t, 3>>(value);

        for (auto& word : words)
        {
            word += 0xd54459ffu;
            word = std::rotl(word, 0xdu);
            word += 0x64cb19c8u;
            word = std::rotl(word, 0x6u);
        }

        return std::bit_cast<std::array<std::uint32_t, 3>>(words);
    }

    inline auto encrypt_previous_inherited_velocity(std::array<std::uint32_t, 3> value) -> std::array<std::uint32_t, 3>
    {
        auto words = std::bit_cast<std::array<std::uint32_t, 3>>(value);

        for (auto& word : words)
        {
            word = std::rotl(word, 0x1au);
            word += 0x9b34e638u;
            word = std::rotl(word, 0x13u);
            word += 0x2abba601u;
        }

        return std::bit_cast<std::array<std::uint32_t, 3>>(words);
    }

    inline auto decrypt_previous_position(std::array<std::uint32_t, 3> value) -> std::array<std::uint32_t, 3>
    {
        auto words = std::bit_cast<std::array<std::uint32_t, 3>>(value);

        for (auto& word : words)
        {
            word = std::rotl(word, 0x1fu);
            word ^= 0xb525f90eu;
            word += 0x29bc0302u;
        }

        return std::bit_cast<std::array<std::uint32_t, 3>>(words);
    }

    inline auto encrypt_previous_position(std::array<std::uint32_t, 3> value) -> std::array<std::uint32_t, 3>
    {
        auto words = std::bit_cast<std::array<std::uint32_t, 3>>(value);

        for (auto& word : words)
        {
            word += 0xd643fcfeu;
            word ^= 0xb525f90eu;
            word = std::rotl(word, 0x1u);
        }

        return std::bit_cast<std::array<std::uint32_t, 3>>(words);
    }

    inline auto decrypt_previous_velocity(std::array<std::uint32_t, 3> value) -> std::array<std::uint32_t, 3>
    {
        auto words = std::bit_cast<std::array<std::uint32_t, 3>>(value);

        for (auto& word : words)
        {
            word += 0x5f4462f1u;
            word = std::rotl(word, 0x7u);
            word ^= 0x86400aecu;
        }

        return std::bit_cast<std::array<std::uint32_t, 3>>(words);
    }

    inline auto encrypt_previous_velocity(std::array<std::uint32_t, 3> value) -> std::array<std::uint32_t, 3>
    {
        auto words = std::bit_cast<std::array<std::uint32_t, 3>>(value);

        for (auto& word : words)
        {
            word ^= 0x86400aecu;
            word = std::rotl(word, 0x19u);
            word += 0xa0bb9d0fu;
        }

        return std::bit_cast<std::array<std::uint32_t, 3>>(words);
    }

    inline auto decrypt_start_reload_time(std::uint32_t value) -> float
    {
        auto words = std::bit_cast<std::array<std::uint32_t, 1>>(value);

        for (auto& word : words)
        {
            word = std::rotl(word, 0x19u);
            word ^= 0xfde7dee3u;
            word += 0xba03d2d1u;
        }

        auto result = 0.00f;
        std::memcpy(&result, words.data(), sizeof(result));

        return result;
    }

    inline auto encrypt_start_reload_time(float value) -> std::uint32_t
    {
        auto result = std::uint32_t();
        std::memcpy(&result, &value, sizeof(result));

        auto words = std::bit_cast<std::array<std::uint32_t, 1>>(result);

        for (auto& word : words)
        {
            word += 0x45fc2d2fu;
            word ^= 0xfde7dee3u;
            word = std::rotl(word, 0x7u);
        }

        return words[0];
    }

    inline auto decrypt_time_since_deploy(std::uint32_t value) -> float
    {
        auto words = std::bit_cast<std::array<std::uint32_t, 1>>(value);

        for (auto& word : words)
        {
            word = std::rotl(word, 0x4u);
            word ^= 0xf854d7fcu;
            word = std::rotl(word, 0x16u);
            word += 0x662198f9u;
        }

        auto result = 0.00f;
        std::memcpy(&result, words.data(), sizeof(result));

        return result;
    }

    inline auto encrypt_time_since_deploy(float value) -> std::uint32_t
    {
        auto result = std::uint32_t();
        std::memcpy(&result, &value, sizeof(result));

        auto words = std::bit_cast<std::array<std::uint32_t, 1>>(result);

        for (auto& word : words)
        {
            word += 0x99de6707u;
            word = std::rotl(word, 0xau);
            word ^= 0xf854d7fcu;
            word = std::rotl(word, 0x1cu);
        }

        return words[0];
    }

    inline auto decrypt_viewoffset(std::array<std::uint32_t, 3> value) -> std::array<std::uint32_t, 3>
    {
        auto words = std::bit_cast<std::array<std::uint32_t, 3>>(value);

        for (auto& word : words)
        {
            word = std::rotl(word, 0xcu);
            word ^= 0x7805249au;
            word += 0x6cfade17u;
        }

        return std::bit_cast<std::array<std::uint32_t, 3>>(words);
    }

    inline auto encrypt_viewoffset(std::array<std::uint32_t, 3> value) -> std::array<std::uint32_t, 3>
    {
        auto words = std::bit_cast<std::array<std::uint32_t, 3>>(value);

        for (auto& word : words)
        {
            word += 0x930521e9u;
            word ^= 0x7805249au;
            word = std::rotl(word, 0x14u);
        }

        return std::bit_cast<std::array<std::uint32_t, 3>>(words);
    }

    // ── not present in the 2026-11-07 dump (kept from 26480361) ───────────

    inline auto decrypt_capsule_height(std::uint32_t value) -> float
    {
        auto result = value;
        result = std::rotl(result, 5);
        result += 0x45B9AB21u;
        result = std::rotl(result, 20);
        result ^= 0x2ECC54D2u;
        auto out = 0.0f;
        std::memcpy(&out, &result, sizeof(out));
        return out;
    }

    inline auto encrypt_capsule_height(float value) -> std::uint32_t
    {
        auto result = std::uint32_t();
        std::memcpy(&result, &value, sizeof(result));
        result ^= 0x2ECC54D2u;
        result = std::rotl(result, 12);
        result -= 0x45B9AB21u;
        result = std::rotl(result, 27);
        return result;
    }

    inline auto decrypt_capsule_center(std::uint32_t value) -> float
    {
        auto result = value;
        result ^= 0x55230DB3u;
        result = std::rotl(result, 18);
        result ^= 0xB27B68B3u;
        auto out = 0.0f;
        std::memcpy(&out, &result, sizeof(out));
        return out;
    }

    inline auto encrypt_capsule_center(float value) -> std::uint32_t
    {
        auto result = std::uint32_t();
        std::memcpy(&result, &value, sizeof(result));
        result ^= 0xB27B68B3u;
        result = std::rotl(result, 14);
        result ^= 0x55230DB3u;
        return result;
    }

    inline auto decrypt_gravity_multiplier(std::uint32_t value) -> float
    {
        auto result = value;
        result = std::rotl(result, 16);
        result += 0xACE65C6Au;
        result ^= 0x4763F93Au;
        result = std::rotl(result, 5);
        auto out = 0.0f;
        std::memcpy(&out, &result, sizeof(out));
        return out;
    }

    inline auto encrypt_gravity_multiplier(float value) -> std::uint32_t
    {
        auto result = std::uint32_t();
        std::memcpy(&result, &value, sizeof(result));
        result = std::rotl(result, 27);
        result ^= 0x4763F93Au;
        result -= 0xACE65C6Au;
        result = std::rotl(result, 16);
        return result;
    }

}