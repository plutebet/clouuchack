#pragma once
#include <stdint.h>

// Build profile: Standrise 1.0.0F5 (private server of Standoff 2),
// package com.SKITLSE.StandRise, Unity engine (same il2cpp as SO2).
// Dump: Il2CppDumper on libil2cpp.so from apk 1.0.0F5.
// TypeInfo RVAs from script.json, field offsets from dump.cs.

namespace rise {
    inline constexpr char profile_name[] = "Standrise-1.0.0F5-arm64";
    inline constexpr char package_name[] = "com.SKITLSE.StandRise";

    // ---- TypeInfo slots (RVA in libil2cpp.so .data) ----
    inline constexpr uint64_t PlayerManager_TypeInfo         = 0x424AEF0;
    inline constexpr uint64_t PlayerController_TypeInfo      = 0x424AEC8;
    inline constexpr uint64_t MainCamera_TypeInfo            = 0x424A230;
    inline constexpr uint64_t Camera_TypeInfo                = 0x42446D8;
    inline constexpr uint64_t Team_TypeInfo                  = 0x424C570;
    inline constexpr uint64_t Transform_TypeInfo             = 0x424C968;
    inline constexpr uint64_t CoreAcClient_TypeInfo          = 0x4244C80;
    inline constexpr uint64_t Dict_int_PC_TypeInfo           = 0x4238998;
    inline constexpr uint64_t HashSet_PC_TypeInfo            = 0x423CD18;

    // ---- PlayerManager (LazySingleton, TypeDefIndex 2796) ----
    // Static field at offset 0x0 in the class's static-fields area.
    inline constexpr uint64_t pm_dict          = 0x20;  // Dictionary<int, PlayerController>
    inline constexpr uint64_t pm_hashset       = 0x28;  // HashSet<PlayerController>
    inline constexpr uint64_t pm_local_player  = 0x68;  // PlayerController (localPlayer)

    // ---- Controller base (TypeDefIndex 2781, extends PhotonBehavior) ----
    inline constexpr uint64_t ctrl_view_mode   = 0x20;  // enum (None=0, FPS=1, TPS=2)
    inline constexpr uint64_t ctrl_team        = 0x24;  // Team (byte enum)
    inline constexpr uint64_t ctrl_biped_map   = 0x28;
    inline constexpr uint64_t ctrl_transform   = 0x30;  // Transform
    inline constexpr uint64_t ctrl_photon_player = 0x38;
    inline constexpr uint64_t ctrl_health      = 0x40;  // int (default 100)
    inline constexpr uint64_t ctrl_armor       = 0x44;  // int (default 100)
    inline constexpr uint64_t ctrl_alive       = 0x48;  // bool

    // ---- PlayerController (TypeDefIndex 2784, extends PunBehaviour) ----
    inline constexpr uint64_t pc_camera_holder = 0x20;  // Transform _mainCameraHolder
    inline constexpr uint64_t pc_team          = 0x59;  // Team (byte)
    inline constexpr uint64_t pc_aim_ctrl      = 0x60;
    inline constexpr uint64_t pc_weaponry_ctrl = 0x68;
    inline constexpr uint64_t pc_hit_ctrl      = 0x88;  // PlayerHitController
    inline constexpr uint64_t pc_net_ctrl      = 0x98;  // NetworkController (extends Controller)
    inline constexpr uint64_t pc_main_camera   = 0xC0;  // PlayerMainCamera
    inline constexpr uint64_t pc_fps_camera    = 0xC8;
    inline constexpr uint64_t pc_transform     = 0xD8;  // Transform
    inline constexpr uint64_t pc_view_mode     = 0x10C; // view mode enum
    inline constexpr uint64_t pc_photon_view   = 0x120; // PhotonView
    inline constexpr uint64_t pc_photon_player = 0x130; // PhotonPlayer

    // ---- NetworkController (TypeDefIndex 2782, extends Controller) ----
    // Inherits Controller base fields: health @0x40, armor @0x44, alive @0x48

    // ---- CameraMovementController (TypeDefIndex 4946) ----
    inline constexpr uint64_t cmc_camera       = 0x98;  // UnityEngine.Camera
    inline constexpr uint64_t cmc_transform    = 0xA8;  // Transform

    // ---- MainCamera (TypeDefIndex 4952, extends CameraMovementController) ----
    // Static singleton at field offset 0x0 in class static area.
    inline constexpr uint64_t main_camera_singleton = 0x0;

    // ---- PhotonView (TypeDefIndex 1634) ----
    inline constexpr uint64_t pv_owner_id      = 0x20;  // int
    inline constexpr uint64_t pv_view_id_field = 0x68;

    // ---- Camera method RVAs (reference, in libil2cpp.so .text) ----
    inline constexpr uint64_t camera_get_view  = 0x2DDC28C;
    inline constexpr uint64_t camera_get_proj  = 0x2DDC33C;
    inline constexpr uint64_t camera_get_main  = 0x2DDCC38;

    // ---- Unity native Camera layout (same engine as SO2) ----
    inline constexpr uint64_t camera_view      = 0x70;  // native Camera worldToCamera matrix
    inline constexpr uint64_t camera_projection = 0xB0; // native Camera projection matrix
    inline constexpr uint64_t unity_cached_ptr = 0x10;  // UnityEngine.Object.m_CachedPtr

    // ---- Unity native Transform hierarchy (same engine) ----
    inline constexpr uint64_t transform_hierarchy = 0x28;
    inline constexpr uint64_t transform_index     = 0x10;
    inline constexpr uint64_t hierarchy_entries   = 0x18;
    inline constexpr uint64_t hierarchy_parents   = 0x20;

    // ---- Il2Cpp Dictionary<int, T> internal layout ----
    inline constexpr uint64_t dict_count       = 0x40;  // int _count
    inline constexpr uint64_t dict_entries     = 0x18;  // Entry[] _entries
    inline constexpr uint64_t array_length     = 0x18;  // Il2CppArray._length
    inline constexpr uint64_t array_data       = 0x20;  // Il2CppArray first element

    // Dictionary<int, PlayerController>.Entry layout (il2cpp):
    //   int hashCode (4) + int next (4) + int key (4, padded to 8) + ptr value (8) = 24
    inline constexpr uint64_t dict_entry_size  = 0x18;  // 24 bytes
    inline constexpr uint64_t dict_entry_key   = 0x08;  // offset of key within entry
    inline constexpr uint64_t dict_entry_value = 0x10;  // offset of value within entry

    // ---- Team enum ----
    // None = 0, Tr = 1, Ct = 2, Spectator = 3
}
