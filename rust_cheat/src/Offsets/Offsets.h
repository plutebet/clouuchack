
// ================================================================================
// Exclusive release:
// TG Bot: t.me/imguisource_bot
// TG 1: t.me/ImGui_Source_FIXNEY (ImGui Source)
// TG 2: t.me/il2cpp_src (shit src 🧨)
// ================================================================================

// Dump generated on: 2026-11-06 (Rust build 26480361)
//
// Updated from that dump:
//   offsets      -> resources.hpp (included below) and the values below
//   decryptions  -> decryptor.hpp
//   chams        -> material IDs in Features/Visuals/Chams/Chams.cpp
// Values still marked "OLD BUILD" were NOT present in this dump and keep the
// previous build's value (il2cpp function RVAs, Unity engine internals,
// GameAssembly exports, ...).
#pragma once
#include <cstdint>

#include "resources.hpp"

namespace GameAssembly {
constexpr const static size_t timestamp                         = 0x6aed8d9a;
constexpr const static size_t il2cpp_resolve_icall              = 0x796930;
constexpr const static size_t il2cpp_array_new                  = 0x796950;
constexpr const static size_t il2cpp_assembly_get_image         = 0x58d0;
constexpr const static size_t il2cpp_class_from_name            = 0x780b40;
constexpr const static size_t il2cpp_class_get_method_from_name = 0x796d50;
constexpr const static size_t il2cpp_class_get_type             = 0x6883b0;
constexpr const static size_t il2cpp_domain_get                 = 0x797670;
constexpr const static size_t il2cpp_domain_get_assemblies      = 0x797690;
constexpr const static size_t il2cpp_gchandle_get_target        = 0x797db0;
constexpr const static size_t il2cpp_gchandle_new               = 0x797d60;
constexpr const static size_t il2cpp_gchandle_free              = 0x797e50;
constexpr const static size_t il2cpp_method_get_name            = 0xa7b0;
constexpr const static size_t il2cpp_object_new                 = 0x7986f0;
constexpr const static size_t il2cpp_type_get_object            = 0x7997d0;
}  // namespace GameAssembly

#define Object_TypeDefinitionIndex 464

namespace Object_Offsets {
inline constexpr std::uintptr_t typeinfo      = 0xf04b708;
constexpr auto                  static_fields = 0xb8;

// Offsets
constexpr const static size_t m_CachedPtr = resources::Object::m_CachedPtr;

// Functions
constexpr const static size_t GetInstanceID            = 0xc5941d0;
constexpr const static size_t Destroy                  = 0xc5949d0;
constexpr const static size_t DestroyImmediate         = 0xc594a50;
constexpr const static size_t DontDestroyOnLoad        = 0x1c7c540;
constexpr const static size_t FindObjectFromInstanceID = 0xc595730;
constexpr const static size_t GetName                  = 0xc8600;
constexpr const static size_t get_hideFlags            = 0xc594ba0;
constexpr const static size_t set_hideFlags            = 0xc594c10;
}  // namespace Object_Offsets

#define GameObject_TypeDefinitionIndex 431

namespace GameObject_Offsets {
inline constexpr std::uintptr_t typeinfo = 0xf1fb790;

// Functions
constexpr const static size_t SetActive                     = 0xC59BE80;
constexpr const static size_t Internal_AddComponentWithType = 0xc58d680;
constexpr const static size_t GetComponent                  = 0xc58cdd0;
constexpr const static size_t GetComponentCount             = 0xc58d710;
constexpr const static size_t GetComponentInChildren        = 0xc58cef0;
constexpr const static size_t GetComponentInParent          = 0xc58d020;
constexpr const static size_t GetComponentsInternal         = 0xc58d150;
constexpr const static size_t Internal_CreateGameObject     = 0xc58e750;
constexpr const static size_t get_layer                     = 0xc58d890;
constexpr const static size_t get_tag                       = 0xc58ddb0;
constexpr const static size_t get_transform                 = 0xc58d780;
}  // namespace GameObject_Offsets

#define Component_TypeDefinitionIndex 417

namespace Component_Offsets {
inline constexpr std::uintptr_t typeinfo = 0xf23cef0;

// Functions
constexpr const static size_t get_gameObject = 0x3C5D9C0;
constexpr const static size_t get_transform  = 0x25c9070;
}  // namespace Component_Offsets

#define Behaviour_TypeDefinitionIndex 412

namespace Behaviour_Offsets {
inline constexpr std::uintptr_t typeinfo = 0xf0f6da8;

// Functions
constexpr const static size_t get_enabled = 0xaa3b80;
constexpr const static size_t set_enabled = 0xC5968E0;
}  // namespace Behaviour_Offsets

#define Transform_TypeDefinitionIndex 506

namespace Transform_Offsets {
inline constexpr std::uintptr_t typeinfo = 0xf11c750;

// Functions
constexpr const static size_t get_eulerAngles                      = 0xc5a3a70;
constexpr const static size_t GetChild                             = 0xc5a7c10;
constexpr const static size_t GetParent                            = 0xc5a4cf0;
constexpr const static size_t GetRoot                              = 0xc5a71d0;
constexpr const static size_t InverseTransformDirection_Injected   = 0xd0b50;
constexpr const static size_t InverseTransformPoint_Injected       = 0xd0eb0;
constexpr const static size_t InverseTransformVector_Injected      = 0xd0d10;
constexpr const static size_t GetPositionAndRotation               = 0xc5a5220;
constexpr const static size_t SetLocalPositionAndRotation_Injected = 0xd0980;
constexpr const static size_t SetPositionAndRotation_Injected      = 0xd0940;
constexpr const static size_t TransformDirection_Injected          = 0xd0a80;
constexpr const static size_t TransformPoint_Injected              = 0xd0de0;
constexpr const static size_t TransformVector_Injected             = 0xd0c40;
constexpr const static size_t get_childCount                       = 0xc5a7250;
constexpr const static size_t get_forward_Injected                 = 0xc5a4590;
constexpr const static size_t get_right_Injected                   = 0xc5a3e10;
constexpr const static size_t get_up_Injected                      = 0xc5a41d0;
constexpr const static size_t get_localPosition_Injected           = 0xd0430;
constexpr const static size_t get_localRotation_Injected           = 0xd0570;
constexpr const static size_t get_localScale_Injected              = 0xd0650;
constexpr const static size_t get_lossyScale_Injected              = 0xd1530;
constexpr const static size_t get_position_Injected                = 0xd03c0;
constexpr const static size_t get_rotation_Injected                = 0xd04e0;
constexpr const static size_t set_localPosition_Injected           = 0xd0460;
constexpr const static size_t set_localRotation_Injected           = 0xd0610;
constexpr const static size_t set_localScale_Injected              = 0xd0680;
constexpr const static size_t set_position_Injected                = 0xd03f0;
constexpr const static size_t set_rotation_Injected                = 0xd0530;
}  // namespace Transform_Offsets

#define Camera_TypeDefinitionIndex 168

namespace Camera_Offsets {
inline constexpr std::uintptr_t typeinfo      = 0xf1c1998;
constexpr auto                  static_fields = 0xb8;

// Functions
constexpr const static size_t get_main                      = 0xc519f60;
constexpr const static size_t WorldToScreenPoint_Injected   = 0x7a980;
constexpr const static size_t ScreenToWorldPoint_Injected   = 0x7ac80;
constexpr const static size_t GetAllCamerasCount            = 0xc51a840;
constexpr const static size_t CopyFrom                      = 0xc51adf0;
constexpr const static size_t get_fieldOfView               = 0xc515a10;
constexpr const static size_t set_fieldOfView               = 0xc515a80;
constexpr const static size_t get_nearClipPlane             = 0xc515830;
constexpr const static size_t set_nearClipPlane             = 0xc5158a0;
constexpr const static size_t get_farClipPlane              = 0xc515920;
constexpr const static size_t set_farClipPlane              = 0xc515990;
constexpr const static size_t get_depth                     = 0xc516370;
constexpr const static size_t set_depth                     = 0xc5163e0;
constexpr const static size_t get_projectionMatrix_Injected = 0x7a7d0;
constexpr const static size_t set_projectionMatrix_Injected = 0x7a810;
constexpr const static size_t set_cullingMask               = 0xc516650;
constexpr const static size_t set_clearFlags                = 0xc517180;
constexpr const static size_t set_backgroundColor_Injected  = 0x79130;
constexpr const static size_t set_targetTexture             = 0xc518730;
constexpr const static size_t Render                        = 0xc51ab70;
constexpr const static size_t RenderWithShader              = 0xc51abe0;
}  // namespace Camera_Offsets

#define Time_TypeDefinitionIndex 490

namespace Time_Offsets {

// Functions
constexpr const static size_t get_deltaTime            = 0x6bbb350;
constexpr const static size_t get_fixedDeltaTime       = 0x6bbf8c0;
constexpr const static size_t get_fixedTime            = 0x6bbda60;
constexpr const static size_t get_frameCount           = 0xc59d7b0;
constexpr const static size_t get_realtimeSinceStartup = 0xc59d850;
constexpr const static size_t get_smoothDeltaTime      = 0x6cb66b0;
constexpr const static size_t get_time                 = 0x6ba3b00;
}  // namespace Time_Offsets

#define Material_TypeDefinitionIndex 242

namespace Material_Offsets {
inline constexpr std::uintptr_t typeinfo      = 0xf1e2d58;
constexpr auto                  static_fields = 0xb8;

// Functions
constexpr const static size_t SetFloatImpl          = 0xc547970;
constexpr const static size_t SetColorImpl_Injected = 0x9e750;
constexpr const static size_t SetTextureImpl        = 0xc547b20;
constexpr const static size_t CreateWithMaterial    = 0xc544da0;
constexpr const static size_t CreateWithShader      = 0xc544d00;
constexpr const static size_t SetBufferImpl         = 0xc547bd0;
constexpr const static size_t set_shader            = 0xc544ec0;
constexpr const static size_t get_shader            = 0xc544e40;
}  // namespace Material_Offsets

#define MaterialPropertyBlock_TypeDefinitionIndex 237

namespace MaterialPropertyBlock_Offsets {
inline constexpr std::uintptr_t typeinfo = 0xf0e8500;

// Functions
constexpr const static size_t ctor           = 0xc53b210;
constexpr const static size_t SetFloatImpl   = 0xc539b80;
constexpr const static size_t SetTextureImpl = 0xc53a020;
}  // namespace MaterialPropertyBlock_Offsets

#define Shader_TypeDefinitionIndex 241

namespace Shader_Offsets {

// Functions
constexpr const static size_t Find             = 0xc541c90;
constexpr const static size_t PropertyToID     = 0xc5428e0;
constexpr const static size_t GetPropertyCount = 0xc543ed0;
constexpr const static size_t GetPropertyName  = 0xc543a50;
constexpr const static size_t GetPropertyType  = 0xc543b60;
}  // namespace Shader_Offsets

#define Mesh_TypeDefinitionIndex 301

namespace Mesh_Offsets {
inline constexpr std::uintptr_t typeinfo = 0xf1f4f28;

// Functions
constexpr const static size_t Internal_Create              = 0xc551f90;
constexpr const static size_t MarkDynamicImpl              = 0xc554c30;
constexpr const static size_t ClearImpl                    = 0xc554a30;
constexpr const static size_t set_subMeshCount             = 0xc554670;
constexpr const static size_t SetVertexBufferParamsFromPtr = 0xa5050;
constexpr const static size_t InternalSetVertexBufferData  = 0xa5100;
constexpr const static size_t UploadMeshDataImpl           = 0xc554d10;
}  // namespace Mesh_Offsets

#define Renderer_TypeDefinitionIndex 239

namespace Renderer_Offsets {

// Functions
constexpr const static size_t get_enabled      = 0xc53d470;
constexpr const static size_t get_isVisible    = 0xc53d560;
constexpr const static size_t GetMaterial      = 0xc53cce0;
constexpr const static size_t GetMaterialArray = 0xc53ce80;
}  // namespace Renderer_Offsets

#define Texture_TypeDefinitionIndex 306

namespace Texture_Offsets {
inline constexpr std::uintptr_t typeinfo      = 0xf1ba540;
constexpr auto                  static_fields = 0xb8;

// Functions
constexpr const static size_t set_filterMode      = 0xc561510;
constexpr const static size_t GetNativeTexturePtr = 0xc5617f0;
}  // namespace Texture_Offsets

#define Texture2D_TypeDefinitionIndex 307

namespace Texture2D_Offsets {
inline constexpr std::uintptr_t typeinfo = 0xf1e2680;

// Functions
constexpr const static size_t ctor                 = 0xc564960;
constexpr const static size_t Internal_CreateImpl  = 0xac950;
constexpr const static size_t GetWritableImageData = 0xc563a20;
constexpr const static size_t ApplyImpl            = 0xc5631c0;
}  // namespace Texture2D_Offsets

#define Sprite_TypeDefinitionIndex 144

namespace Sprite_Offsets {
inline constexpr std::uintptr_t typeinfo = 0xf07c818;

// Functions
constexpr const static size_t get_texture = 0xc50e370;
}  // namespace Sprite_Offsets

#define RenderTexture_TypeDefinitionIndex 313

namespace RenderTexture_Offsets {
inline constexpr std::uintptr_t typeinfo = 0xf140608;

// Functions
constexpr const static size_t GetTemporary     = 0xc56f330;
constexpr const static size_t ReleaseTemporary = 0xc56d280;
}  // namespace RenderTexture_Offsets

#define CommandBuffer_TypeDefinitionIndex 896

namespace CommandBuffer_Offsets {
inline constexpr std::uintptr_t typeinfo      = 0xf0d95f8;
constexpr auto                  static_fields = 0xb8;

// Functions
constexpr const static size_t ctor                                    = 0xc5cf970;
constexpr const static size_t Clear                                   = 0xc5c4a80;
constexpr const static size_t SetRenderTargetSingle_Internal_Injected = 0xf02a0;
constexpr const static size_t ClearRenderTarget_Injected              = 0xc5c82c0;
constexpr const static size_t SetViewport_Injected                    = 0xe9bc0;
constexpr const static size_t SetViewProjectionMatrices_Injected      = 0xeee90;
constexpr const static size_t EnableScissorRect_Injected              = 0xe9c80;
constexpr const static size_t DisableScissorRect                      = 0xc5c6050;
constexpr const static size_t Internal_DrawProceduralIndexedIndirect_Injected =
    0xe9100;
constexpr const static size_t Internal_DrawMesh_Injected = 0xe8710;
constexpr const static size_t Internal_DrawRenderer      = 0xc5c4db0;
}  // namespace CommandBuffer_Offsets

#define RenderTargetIdentifier_TypeDefinitionIndex 856

namespace RenderTargetIdentifier_Offsets {
inline constexpr std::uintptr_t typeinfo      = 0xf1e4080;
constexpr auto                  static_fields = 0xb8;

// Functions
constexpr const static size_t ctor = 0xc5bd870;
}  // namespace RenderTargetIdentifier_Offsets

#define ComputeBuffer_TypeDefinitionIndex 480

namespace ComputeBuffer_Offsets {
inline constexpr std::uintptr_t typeinfo = 0xf0daf70;

// Functions
constexpr const static size_t ctor                  = 0xc597560;
constexpr const static size_t get_count             = 0xc597830;
constexpr const static size_t Release               = 0xc5972e0;
constexpr const static size_t InternalSetNativeData = 0xc597d70;
}  // namespace ComputeBuffer_Offsets

#define GraphicsBuffer_TypeDefinitionIndex 244

namespace GraphicsBuffer_Offsets {
inline constexpr std::uintptr_t typeinfo = 0xf0e7d30;

// Functions
constexpr const static size_t ctor                  = 0xc54b410;
constexpr const static size_t get_count             = 0xc54b8c0;
constexpr const static size_t Dispose               = 0xc54b200;
constexpr const static size_t InternalSetNativeData = 0xc54be00;
}  // namespace GraphicsBuffer_Offsets

#define Event_TypeDefinitionIndex 1

namespace Event_Offsets {
inline constexpr std::uintptr_t typeinfo      = 0xf07e068;
constexpr auto                  static_fields = 0xb8;

// Functions
constexpr const static size_t get_current  = 0xc6026c0;
constexpr const static size_t get_type     = 0xc6017f0;
constexpr const static size_t PopEvent     = 0xc601cd0;
constexpr const static size_t Internal_Use = 0xc601ac0;
}  // namespace Event_Offsets

#define Graphics_TypeDefinitionIndex 217

namespace Graphics_Offsets {
inline constexpr std::uintptr_t typeinfo      = 0xf147d88;
constexpr auto                  static_fields = 0xb8;

// Functions
constexpr const static size_t Internal_BlitMaterial5 = 0xc52ea90;
constexpr const static size_t ExecuteCommandBuffer   = 0xc52ed80;
}  // namespace Graphics_Offsets

#define Matrix4x4_TypeDefinitionIndex 339

namespace Matrix4x4_Offsets {
inline constexpr std::uintptr_t typeinfo      = 0xf246008;
constexpr auto                  static_fields = 0xb8;

// Functions
constexpr const static size_t Ortho_Injected = 0xb6550;
}  // namespace Matrix4x4_Offsets

#define AssetBundle_TypeDefinitionIndex 1

namespace AssetBundle_Offsets {

// Functions
constexpr const static size_t LoadFromFile_Internal = 0xc4f9780;
constexpr const static size_t LoadAsset_Internal    = 0xc4f9ae0;
constexpr const static size_t Unload                = 0xc4fa040;
}  // namespace AssetBundle_Offsets

#define Screen_TypeDefinitionIndex 214

namespace Screen_Offsets {

// Functions
constexpr const static size_t get_width  = 0x6d61fb0;
constexpr const static size_t get_height = 0x6d622d0;
}  // namespace Screen_Offsets

#define Input_TypeDefinitionIndex 9

namespace Input_Offsets {
constexpr auto static_fields = 0xb8;

// Functions
constexpr const static size_t get_mousePosition_Injected    = 0x1ad5f0;
constexpr const static size_t get_mouseScrollDelta_Injected = 0x1ad7c0;
constexpr const static size_t GetMouseButtonDown            = 0xc641a40;
constexpr const static size_t GetMouseButtonUp              = 0xc641a90;
constexpr const static size_t GetMouseButton                = 0xc6419f0;
constexpr const static size_t GetKeyDownInt                 = 0xc6419a0;
constexpr const static size_t GetKeyUpInt                   = 0x0;
constexpr const static size_t GetKeyInt                     = 0xc641950;
}  // namespace Input_Offsets

#define Application_TypeDefinitionIndex 151

namespace Application_Offsets {
inline constexpr std::uintptr_t typeinfo      = 0xf280058;
constexpr auto                  static_fields = 0xb8;

// Functions
constexpr const static size_t get_version   = 0xc512760;
constexpr const static size_t Quit          = 0xc511c60;
constexpr const static size_t get_isFocused = 0x6d80c00;
}  // namespace Application_Offsets

#define Gradient_TypeDefinitionIndex 336

namespace Gradient_Offsets {
inline constexpr std::uintptr_t typeinfo = 0xf0c18a8;

// Functions
constexpr const static size_t SetKeys = 0xb6190;
}  // namespace Gradient_Offsets

#define Physics_TypeDefinitionIndex 14

namespace Physics_Offsets {
inline constexpr std::uintptr_t typeinfo      = 0xf1d2570;
constexpr auto                  static_fields = 0xb8;

// Functions
constexpr const static size_t Raycast         = 0xc667db0;
constexpr const static size_t RaycastNonAlloc = 0xc669650;
constexpr const static size_t CheckCapsule    = 0xc66a9f0;
}  // namespace Physics_Offsets

#define Image_TypeDefinitionIndex 39

namespace Image_Offsets {
inline constexpr std::uintptr_t typeinfo      = 0xf17e098;
constexpr auto                  static_fields = 0xb8;

// Offsets
constexpr const static size_t m_Sprite = 0xd8;
}  // namespace Image_Offsets

#define GraphicsSettings_TypeDefinitionIndex 886

namespace GraphicsSettings_Offsets {
inline constexpr std::uintptr_t typeinfo      = 0xf1a22d8;
constexpr auto                  static_fields = 0xb8;

// Functions
constexpr const static size_t get_INTERNAL_defaultRenderPipeline = 0xc5be780;
}  // namespace GraphicsSettings_Offsets

#define Cursor_TypeDefinitionIndex 324

namespace Cursor_Offsets {

// Functions
constexpr const static size_t get_visible = 0xc572d20;
}  // namespace Cursor_Offsets

// ── Unity native struct offsets

namespace IL2CPP_String_Native {
constexpr size_t length = 0x10;
constexpr size_t chars  = 0x14;
}  // namespace IL2CPP_String_Native

namespace IL2CPP_Array_Native {
constexpr size_t size = 0x18;
constexpr size_t data = 0x28;
}  // namespace IL2CPP_Array_Native

namespace IL2CPP_List_Native {
constexpr size_t items = 0x10;
constexpr size_t size  = 0x10;
}  // namespace IL2CPP_List_Native

namespace IL2CPP_Dictionary_Native {
constexpr size_t entries        = 0x18;
constexpr size_t count          = 0x20;
constexpr size_t Entry_hashCode = 0x0;
constexpr size_t Entry_next     = 0x4;
constexpr size_t Entry_key      = 0x8;
constexpr size_t Entry_value    = 0x10;
constexpr size_t Entry_stride   = 0x18;
}  // namespace IL2CPP_Dictionary_Native

namespace Unity_Component_Native {
constexpr size_t m_GameObject = 0x20;
}

namespace Unity_GameObject_Native {
constexpr size_t m_Component           = 0x20;
constexpr size_t m_ComponentCount      = 0x30;
constexpr size_t m_ComponentPairOffset = 0x8;
constexpr size_t m_Layer               = 0x40;
constexpr size_t m_Tag                 = 0x44;
constexpr size_t m_IsActive            = 0x46;
}  // namespace Unity_GameObject_Native

namespace Unity_Transform_Native {
constexpr size_t m_Hierarchy = 0x28;
constexpr size_t m_Index     = 0x30;
constexpr size_t m_Children  = 0x48;
}  // namespace Unity_Transform_Native

namespace Unity_TransformHierarchy_Native {
constexpr size_t m_LocalTransforms = 0x18;
constexpr size_t m_ParentIndices   = 0x20;
constexpr size_t m_LocalPosition   = 0x90;
}  // namespace Unity_TransformHierarchy_Native

namespace Unity_TrsX_Native {
constexpr size_t stride = 0x30;
}

namespace Unity_NativeRenderer_Native {
constexpr size_t m_Materials      = 0x140;
constexpr size_t cameraViewMatrix = 0x2fc;
constexpr size_t cameraPosition   = 0x444;
}  // namespace Unity_NativeRenderer_Native

#define BaseNetworkable_TypeDefinitionIndex 1161

namespace BaseNetworkable_Offsets {

// Updated from the new dump (resources::BaseNetworkable)
constexpr const static size_t prefabID     = resources::BaseNetworkable::prefabID;
constexpr const static size_t net          = 0x88;  // OLD BUILD
constexpr const static size_t parentEntity = resources::BaseNetworkable::parentEntity;
constexpr const static size_t children     = resources::BaseNetworkable::children;
}  // namespace BaseNetworkable_Offsets

// obf name: ::%b1ab04c76f5c267eadb87173c78396b8f006c3fc
#define BaseNetworkable_Static_ClassName \
    "BaseNetworkable/%b1ab04c76f5c267eadb87173c78396b8f006c3fc"
#define BaseNetworkable_Static_ClassNameShort \
    "%b1ab04c76f5c267eadb87173c78396b8f006c3fc"
#define BaseNetworkable_Static_TypeDefinitionIndex 1168

namespace BaseNetworkable_Static_Offsets {
// TypeInfo from the 2026-11-06 dump (new build); the class resolver validates
// it against the live entity chain before latching.
inline constexpr std::uintptr_t typeinfo      = 0xF067418;
constexpr auto                  static_fields = 0xb8;

// Updated from the new dump (resources::BaseNetworkableStatic::clientEntities)
constexpr const static size_t clientEntities = resources::BaseNetworkableStatic::clientEntities;
}  // namespace BaseNetworkable_Static_Offsets

// obf name: ::%36e8d7060cf0f653779f9e911b4aa62484f32ed5
#define BaseNetworkable_EntityRealm_ClassName \
    "BaseNetworkable/%36e8d7060cf0f653779f9e911b4aa62484f32ed5"
#define BaseNetworkable_EntityRealm_ClassNameShort \
    "%36e8d7060cf0f653779f9e911b4aa62484f32ed5"
#define BaseNetworkable_EntityRealm_TypeDefinitionIndex 1166

namespace BaseNetworkable_EntityRealm_Offsets {

// Updated from the new dump (resources::EntityRealm::entityList)
constexpr const static size_t entityList = resources::EntityRealm::entityList;

// Functions
constexpr const static size_t Find = 0x6f6a8f0;
}  // namespace BaseNetworkable_EntityRealm_Offsets

// obf name: ::%5eb4cde877f32bce3740f3f4804a524186d8875d
#define System_ListDictionary_ClassName           \
    "%5eb4cde877f32bce3740f3f4804a524186d8875d<%" \
    "2202bb9f4801e431c505ead2030c25fb4dc1770e,BaseNetworkable>"
#define System_ListDictionary_ClassNameShort \
    "%5eb4cde877f32bce3740f3f4804a524186d8875d"
#define System_ListDictionary_TypeDefinitionIndex 14

namespace System_ListDictionary_Offsets {
// Updated from the new dump (resources::ListDictionary::TypeInfo)
inline constexpr std::uintptr_t typeinfo = 0xF1781F8;

// Updated from the new dump (resources::ListDictionary::vals)
constexpr const static size_t vals = resources::ListDictionary::vals;

// Functions (OLD BUILD: not present in the new dump)
constexpr const static size_t TryGetValue            = 0x8598ed0;
constexpr const static size_t TryGetValue_methodinfo = 0x10bbb450;
}  // namespace System_ListDictionary_Offsets

// Updated from the new dump (resources::ListHashSet — vals holds the
// underlying BufferList/List<T>)
namespace System_ListHashSet_Offsets {
constexpr const static size_t vals = resources::ListHashSet::vals;
}  // namespace System_ListHashSet_Offsets

// obf name: ::%056c697032bd9ea28ae0a894a945b8a632a59a03
#define System_BufferList_ClassName \
    "%056c697032bd9ea28ae0a894a945b8a632a59a03<BaseNetworkable>"
#define System_BufferList_ClassNameShort \
    "%056c697032bd9ea28ae0a894a945b8a632a59a03"
#define System_BufferList_TypeDefinitionIndex 67

namespace System_BufferList_Offsets {

// BufferList::TypeInfo from the new dump (resources::BufferList::TypeInfo
// = 0xF36CDB0); the List<T> internals buffer=_items / count=size are
// unchanged from the previous build.
constexpr const static size_t count  = 0x18;
constexpr const static size_t buffer = 0x10;
}  // namespace System_BufferList_Offsets

// obf name: ::SingletonComponent`1
#define SingletonComponent_ClassName           "SingletonComponent<MainCamera>"
#define SingletonComponent_ClassNameShort      "SingletonComponent`1"
#define SingletonComponent_TypeDefinitionIndex 63

namespace SingletonComponent_Offsets {
inline constexpr std::uintptr_t typeinfo      = 0xF146D98;
constexpr auto                  static_fields = 0xb8;

// Offsets
constexpr const static size_t Instance = 0x20;
}  // namespace SingletonComponent_Offsets

#define Model_TypeDefinitionIndex 9310

namespace Model_Offsets {

// Offsets (new dump: Model)
constexpr const static size_t rootBone       = 0x28;  // OLD BUILD
constexpr const static size_t headBone       = 0x30;  // OLD BUILD
constexpr const static size_t eyeBone        = 0x38;  // OLD BUILD
constexpr const static size_t boneTransforms = 0x50;
constexpr const static size_t boneNames      = 0x58;
}  // namespace Model_Offsets

#define BaseEntity_TypeDefinitionIndex 5098

namespace BaseEntity_Offsets {
inline constexpr std::uintptr_t typeinfo = 0xF1CCDF0;  // NeoRed klass_rvas

// Offsets (new dump: BaseEntity)
constexpr const static size_t bounds       = 0x1A4;
constexpr const static size_t model        = 0x1D0;
constexpr const static size_t flags        = 0x1D8;
constexpr const static size_t triggers     = 0x158;
constexpr const static size_t positionLerp = 0xB0;

// Functions (new dump: BaseEntity)
constexpr const static size_t SendSignalBroadcast = 0x31DA5E0;
constexpr const static size_t ServerRPC           = 0x32589C0;
// Functions (OLD BUILD)
constexpr const static size_t FindBone          = 0x36866d0;
constexpr const static size_t GetWorldVelocity  = 0x4851CC0;
constexpr const static size_t GetParentVelocity = 0x4879800;
}  // namespace BaseEntity_Offsets

// obf name: ::%915976499b0654a8e250c1e3a3b156c5ca786eb5
#define PositionLerp_ClassName           "%915976499b0654a8e250c1e3a3b156c5ca786eb5"
#define PositionLerp_ClassNameShort      "%915976499b0654a8e250c1e3a3b156c5ca786eb5"
#define PositionLerp_TypeDefinitionIndex 1546

namespace PositionLerp_Offsets {

// Offsets (new dump: PositionLerp)
constexpr const static size_t interpolator = 0x50;
}  // namespace PositionLerp_Offsets

// obf name: ::%77497369c3d48c92bb5051dd45198f093642a2bf
#define Interpolator_ClassName                    \
    "%77497369c3d48c92bb5051dd45198f093642a2bf<%" \
    "122c8e32d10c5d7ea049ad8c588c24b2104bdf60>"
#define Interpolator_ClassNameShort      "%77497369c3d48c92bb5051dd45198f093642a2bf"
#define Interpolator_TypeDefinitionIndex 5516

namespace Interpolator_Offsets {
inline constexpr std::uintptr_t typeinfo = 0xF1ADBA0;

// Offsets
constexpr const static size_t list = 0x30;
constexpr const static size_t last = 0x10;
}  // namespace Interpolator_Offsets

#define BaseCombatEntity_TypeDefinitionIndex 6518

namespace BaseCombatEntity_Offsets {
inline constexpr std::uintptr_t typeinfo = 0xF092380;  // NeoRed klass_rvas

// Offsets (new dump: BaseCombatEntity)
constexpr const static size_t skeletonProperties  = 0x248;
constexpr const static size_t baseProtection      = 0x250;
constexpr const static size_t startHealth         = 0x258;
constexpr const static size_t lifestate           = 0x2C0;
constexpr const static size_t markAttackerHostile = 0x2C6;
constexpr const static size_t _health             = 0x2CC;
constexpr const static size_t _maxHealth          = 0x2D0;
constexpr const static size_t lastNotifyFrame     = 0x2E0;
}  // namespace BaseCombatEntity_Offsets

#define SkeletonProperties_TypeDefinitionIndex 1128

namespace SkeletonProperties_Offsets {

// Offsets
constexpr const static size_t bones       = 0x20;
constexpr const static size_t quickLookup = 0x28;
}  // namespace SkeletonProperties_Offsets

#define SkeletonProperties_BoneProperty_TypeDefinitionIndex 1129

namespace SkeletonProperties_BoneProperty_Offsets {

// Offsets (new dump: SkeletonPropertiesBoneProperty)
constexpr const static size_t bone     = 0x10;
constexpr const static size_t boneName = 0x18;
constexpr const static size_t area     = 0x20;
}  // namespace SkeletonProperties_BoneProperty_Offsets

#define DamageProperties_TypeDefinitionIndex 2235

namespace DamageProperties_Offsets {

// Offsets
constexpr const static size_t fallback = 0x18;
constexpr const static size_t bones    = 0x20;
}  // namespace DamageProperties_Offsets

#define DamageProperties_HitAreaProperty_TypeDefinitionIndex 2236

namespace DamageProperties_HitAreaProperty_Offsets {

// Offsets
constexpr const static size_t area   = 0x10;
constexpr const static size_t damage = 0x14;
}  // namespace DamageProperties_HitAreaProperty_Offsets

// obf name: ::%245a9bfa663445ae9d41989e37fe19ddd2b0f412
#define DamageTypeList_ClassName "%245a9bfa663445ae9d41989e37fe19ddd2b0f412"
#define DamageTypeList_ClassNameShort \
    "%245a9bfa663445ae9d41989e37fe19ddd2b0f412"
#define DamageTypeList_TypeDefinitionIndex 4180

namespace DamageTypeList_Offsets {
inline constexpr std::uintptr_t typeinfo = 0x10c47920;

// Offsets
constexpr const static size_t types = 0x10;
}  // namespace DamageTypeList_Offsets

#define ProtectionProperties_TypeDefinitionIndex 9471

namespace ProtectionProperties_Offsets {
constexpr auto static_fields = 0xb8;

// Offsets
constexpr const static size_t amounts = 0x30;
}  // namespace ProtectionProperties_Offsets

#define ItemDefinition_TypeDefinitionIndex 1145

namespace ItemDefinition_Offsets {

// Offsets
constexpr const static size_t itemid          = 0x20;
constexpr const static size_t shortname       = 0x28;
constexpr const static size_t displayName     = 0x40;
constexpr const static size_t iconSprite      = 0x50;
constexpr const static size_t category        = 0x58;
constexpr const static size_t stackable       = 0x78;
constexpr const static size_t rarity          = 0x94;
constexpr const static size_t condition       = 0xb8;
constexpr const static size_t ItemModWearable = 0x1A8;
}  // namespace ItemDefinition_Offsets

#define RecoilProperties_TypeDefinitionIndex 9025

namespace RecoilProperties_Offsets {

// Offsets
constexpr const static size_t recoilYawMin             = 0x18;
constexpr const static size_t recoilYawMax             = 0x1c;
constexpr const static size_t recoilPitchMin           = 0x20;
constexpr const static size_t recoilPitchMax           = 0x24;
constexpr const static size_t overrideAimconeWithCurve = 0x5c;
constexpr const static size_t aimconeProbabilityCurve  = 0x70;
constexpr const static size_t newRecoilOverride        = 0x80;
}  // namespace RecoilProperties_Offsets

#define BaseProjectile_Magazine_Definition_TypeDefinitionIndex 901

namespace BaseProjectile_Magazine_Definition_Offsets {

// Offsets
constexpr const static size_t builtInSize = 0x0;
}  // namespace BaseProjectile_Magazine_Definition_Offsets

#define BaseProjectile_Magazine_TypeDefinitionIndex 900

namespace BaseProjectile_Magazine_Offsets {

// Offsets (new dump: Magazine)
constexpr const static size_t allowAmmoSwitching   = 0x29;
constexpr const static size_t allowPlayerReloading = 0x28;
constexpr const static size_t ammoType             = 0x20;
constexpr const static size_t capacity             = 0x18;
constexpr const static size_t contents             = 0x1c;
constexpr const static size_t definition           = 0x10;
}  // namespace BaseProjectile_Magazine_Offsets

#define AttackEntity_TypeDefinitionIndex 998

namespace AttackEntity_Offsets {
inline constexpr std::uintptr_t typeinfo = 0xf0397f8;

// Offsets (new dump: AttackEntity)
constexpr const static size_t animationDelay  = 0x308;
constexpr const static size_t deployDelay     = 0x300;
constexpr const static size_t effectiveRange  = 0x30C;
constexpr const static size_t lastTickTime    = 0x360;
constexpr const static size_t nextAttackTime  = 0x358;
constexpr const static size_t nextTickTime    = 0x368;
constexpr const static size_t noHeadshots     = 0x356;
constexpr const static size_t repeatDelay     = 0x304;
constexpr const static size_t timeSinceDeploy = 0x370;

// Functions (OLD BUILD)
constexpr const static size_t SpectatorNotifyTick = 0x0;
constexpr const static size_t StartAttackCooldown = 0x5165100;
}  // namespace AttackEntity_Offsets

#define BaseProjectile_TypeDefinitionIndex 899

namespace BaseProjectile_Offsets {
inline constexpr std::uintptr_t typeinfo = 0xf0ff2b8;

// Offsets (new dump: BaseProjectile)
constexpr const static size_t aimCone                 = 0x430;
constexpr const static size_t aimConePenaltyMax       = 0x43C;
constexpr const static size_t aimSway                 = 0x418;
constexpr const static size_t aimSwaySpeed            = 0x41C;
constexpr const static size_t aimconePenalty          = 0x484;
constexpr const static size_t aimconePenaltyPerShot   = 0x438;
constexpr const static size_t automatic               = 0x3B0;
constexpr const static size_t canChangeFireModes      = 0x458;
constexpr const static size_t currentBurst            = 0x4D0;
constexpr const static size_t fractionalReload        = 0x400;
constexpr const static size_t hipAimCone              = 0x434;
constexpr const static size_t isBurstWeapon           = 0x457;
constexpr const static size_t isReloading             = 0x49C;
constexpr const static size_t nextReloadTime          = 0x470;
constexpr const static size_t numShotsFired           = 0x46C;
constexpr const static size_t primaryMagazine         = 0x3F8;
constexpr const static size_t projectileVelocityScale = 0x3AC;
constexpr const static size_t recoil                  = 0x420;
constexpr const static size_t reloadEndDuration       = 0x40C;
constexpr const static size_t reloadFractionDuration  = 0x408;
constexpr const static size_t reloadStartDuration     = 0x404;
constexpr const static size_t reloadTime              = 0x3F0;
constexpr const static size_t stancePenalty           = 0x480;
constexpr const static size_t stancePenaltyScale      = 0x448;
constexpr const static size_t startReloadTime         = 0x478;

// Offsets (OLD BUILD: not present in the new dump)
constexpr const static size_t aimconeCurve            = 0x428;
constexpr const static size_t noAimingWhileCycling    = 0x44d;
constexpr const static size_t cachedModHash           = 0x488;
constexpr const static size_t sightAimConeScale       = 0x48C;
constexpr const static size_t sightAimConeOffset      = 0x490;
constexpr const static size_t hipAimConeScale         = 0x494;
constexpr const static size_t hipAimConeOffset        = 0x498;

// Functions (new dump: BaseProjectile::UpdateAmmoDisplay)
constexpr const static size_t UpdateAmmoDisplay           = 0x4C13940;
// Functions (OLD BUILD)
constexpr const static size_t LaunchProjectile            = 0x4C0C610;
constexpr const static size_t LaunchProjectileClientSide  = 0x4CD51C0;
constexpr const static size_t ScaleRepeatDelay            = 0x606d230;
constexpr const static size_t GetAimCone                  = 0x4CB90E0;
constexpr const static size_t GetAimCone_vtableoff        = 0x5858;
constexpr const static size_t UpdateAmmoDisplay_vtableoff = 0x0;
}  // namespace BaseProjectile_Offsets

#define BaseLauncher_TypeDefinitionIndex 908

namespace BaseLauncher_Offsets {

// Offsets
}

#define SpinUpWeapon_TypeDefinitionIndex 8205

namespace SpinUpWeapon_Offsets {

// Offsets
}

// obf name: ::%db9bcb5c82dffc15fa5f50a7cff1839f64dc28ab
#define HitTest_ClassName           "%db9bcb5c82dffc15fa5f50a7cff1839f64dc28ab"
#define HitTest_ClassNameShort      "%db9bcb5c82dffc15fa5f50a7cff1839f64dc28ab"
#define HitTest_TypeDefinitionIndex 7183

namespace HitTest_Offsets {
inline constexpr std::uintptr_t typeinfo = 0xF0990B8;

// Offsets
constexpr const static size_t type             = 0x80;
constexpr const static size_t AttackRay        = 0x28;
constexpr const static size_t RayHit           = 0x1c;
constexpr const static size_t damageProperties = 0xC8;
constexpr const static size_t gameObject       = 0x90;
constexpr const static size_t collider         = 0xb0;
constexpr const static size_t ignoredTypes     = 0x60;
constexpr const static size_t HitTransform     = 0x20;
constexpr const static size_t HitPart          = 0x68;
constexpr const static size_t HitMaterial      = 0x70;
}  // namespace HitTest_Offsets

#define Projectile_TypeDefinitionIndex 6040

namespace Projectile_Offsets {

// Offsets (new dump: Projectile)
constexpr const static size_t drag                   = 0x34;
constexpr const static size_t gravityModifier        = 0x38;
constexpr const static size_t thickness              = 0x3c;
constexpr const static size_t initialDistance        = 0x44;
constexpr const static size_t integrity              = 0x144;
constexpr const static size_t currentVelocity        = 0x16C;
constexpr const static size_t currentPosition        = 0x178;
constexpr const static size_t traveledDistance       = 0x184;
constexpr const static size_t traveledTime           = 0x188;

// Offsets (OLD BUILD: not present in the new dump)
constexpr const static size_t initialVelocity        = 0x28;
constexpr const static size_t swimScale              = 0xf8;
constexpr const static size_t swimSpeed              = 0x104;
constexpr const static size_t owner                  = 0x1E8;
constexpr const static size_t sourceProjectilePrefab = 0x110;
constexpr const static size_t mod                    = 0x1F8;
constexpr const static size_t hitTest                = 0x128;
constexpr const static size_t sentPosition           = 0x190;
constexpr const static size_t previousPosition       = 0x19C;
constexpr const static size_t previousVelocity       = 0x1A8;

// Functions (OLD BUILD)
constexpr const static size_t CalculateEffectScale           = 0x49A51F0;
constexpr const static size_t CalculateEffectScale_vtableoff = 0x1F8;
constexpr const static size_t Retire                         = 0x49A99E0;
constexpr const static size_t DoHit                          = 0x3C34A30;
}  // namespace Projectile_Offsets

// obf name: ::%15e33982b51327ccbb62d6e9a224cd8ec0d8c6d7
#define HitInfo_ClassName           "%15e33982b51327ccbb62d6e9a224cd8ec0d8c6d7"
#define HitInfo_ClassNameShort      "%15e33982b51327ccbb62d6e9a224cd8ec0d8c6d7"
#define HitInfo_TypeDefinitionIndex 1142

namespace HitInfo_Offsets {

// Offsets
constexpr const static size_t damageProperties = 0x38;
constexpr const static size_t damageTypes      = 0xF0;

// Functions
constexpr const static size_t get_boneArea = 0x6f2a5e0;
}  // namespace HitInfo_Offsets

#define BaseMelee_TypeDefinitionIndex 8512

namespace BaseMelee_Offsets {
inline constexpr std::uintptr_t typeinfo = 0xf0ff2c0;

// Offsets (new dump: BaseMelee)
constexpr const static size_t attackRadius         = 0x3BC;
constexpr const static size_t blockSprintOnAttack  = 0x3C1;
constexpr const static size_t canThrowAsProjectile = 0x398;
constexpr const static size_t damageProperties     = 0x3A0;
constexpr const static size_t gathering            = 0x400;
constexpr const static size_t maxDistance          = 0x3B8;

// Functions (OLD BUILD)
constexpr const static size_t ProcessAttack = 0x43B6130;
constexpr const static size_t DoThrow       = 0x60d4f70;
}  // namespace BaseMelee_Offsets

#define FlintStrikeWeapon_TypeDefinitionIndex 1309

namespace FlintStrikeWeapon_Offsets {

// Offsets (new dump: FlintStrikeWeapon)
constexpr const static size_t _didSparkThisFrame = 0x4E8;
constexpr const static size_t _isStriking        = 0x4E9;
constexpr const static size_t strikes            = 0x4EC;
constexpr const static size_t successFraction    = 0x4D8;
constexpr const static size_t successIncrease    = 0x4DC;

// Offsets (OLD BUILD: not present in the new dump)
constexpr const static size_t strikeRecoil       = 0x4e0;
}  // namespace FlintStrikeWeapon_Offsets

#define CompoundBowWeapon_TypeDefinitionIndex 456

namespace CompoundBowWeapon_Offsets {

// Offsets (new dump: CompoundBowWeapon)
constexpr const static size_t conditionLossHeldDelay     = 0x518;
constexpr const static size_t conditionLossPerSecondHeld = 0x514;
constexpr const static size_t movementPenalty            = 0x548;
constexpr const static size_t movementPenaltyRampUpTime  = 0x510;
constexpr const static size_t stringBonusDamage          = 0x504;
constexpr const static size_t stringBonusDistance        = 0x508;
constexpr const static size_t stringBonusVelocity        = 0x50C;
constexpr const static size_t stringHoldDurationMax      = 0x500;
constexpr const static size_t stringHoldTimeStart        = 0x554;

// Functions (OLD BUILD)
constexpr const static size_t GetStringBonusScale = 0xED7490;
}  // namespace CompoundBowWeapon_Offsets

// obf name: ::%9171053831f12a00f5b8917d3198502932c116ef
#define ItemContainer_ClassName           "%9171053831f12a00f5b8917d3198502932c116ef"
#define ItemContainer_ClassNameShort      "%9171053831f12a00f5b8917d3198502932c116ef"
#define ItemContainer_TypeDefinitionIndex 5371

namespace ItemContainer_Offsets {

// Offsets (new dump: ItemContainer)
constexpr const static size_t flags    = 0x34;
constexpr const static size_t itemList = 0x20;
constexpr const static size_t uid      = 0x68;

// Functions (OLD BUILD)
constexpr const static size_t GetSlot = 0x4F6DFA0;
}  // namespace ItemContainer_Offsets

#define PlayerLoot_TypeDefinitionIndex 5436

namespace PlayerLoot_Offsets {

// Offsets
constexpr const static size_t containers = 0x38;
}  // namespace PlayerLoot_Offsets

#define PlayerInventory_TypeDefinitionIndex 7939

namespace PlayerInventory_Offsets {
inline constexpr std::uintptr_t typeinfo = 0xF197678;

// Offsets (new dump: PlayerInventory)
constexpr const static size_t containerBelt = 0x60;
constexpr const static size_t containerMain = 0x68;
constexpr const static size_t containerWear = 0x70;
constexpr const static size_t loot          = 0x28;

// Functions (OLD BUILD)
constexpr const static size_t Initialize = 0x5b22320;
}  // namespace PlayerInventory_Offsets

#define PlayerEyes_TypeDefinitionIndex 6349

namespace PlayerEyes_Offsets {
inline constexpr std::uintptr_t typeinfo = 0xF197690;

// Offsets
constexpr const static size_t viewOffset   = 0x58;
constexpr const static size_t bodyRotation = 0x1C;
constexpr const static size_t headAngles   = 0x48;

// Functions
constexpr const static size_t get_position = 0x3AFEF90;
constexpr const static size_t get_rotation = 0x3AD1D90;
constexpr const static size_t set_rotation = 0x244ECD0;
constexpr const static size_t HeadForward  = 0x3AFF540;
}  // namespace PlayerEyes_Offsets

// obf name: ::%72c6da5495ec5776f4f306dc6b669ef08807dc24
#define PlayerEyes_Static_ClassName \
    "PlayerEyes/%72c6da5495ec5776f4f306dc6b669ef08807dc24"
#define PlayerEyes_Static_ClassNameShort \
    "%72c6da5495ec5776f4f306dc6b669ef08807dc24"
#define PlayerEyes_Static_TypeDefinitionIndex 6350

namespace PlayerEyes_Static_Offsets {
inline constexpr std::uintptr_t typeinfo      = 0x10c7d8e8;
constexpr auto                  static_fields = 0xb8;

// Offsets
constexpr const static size_t EyeOffset = 0x5C;
}  // namespace PlayerEyes_Static_Offsets

// obf name: ::%dcc2ea06c2812f5603b01cf2a766f091cc75b7b8
#define PlayerBelt_ClassName           "%dcc2ea06c2812f5603b01cf2a766f091cc75b7b8"
#define PlayerBelt_ClassNameShort      "%dcc2ea06c2812f5603b01cf2a766f091cc75b7b8"
#define PlayerBelt_TypeDefinitionIndex 2808

namespace PlayerBelt_Offsets {
inline constexpr std::uintptr_t typeinfo = 0x10c5fb40;

// Functions
constexpr const static size_t ChangeSelect  = 0x1fef2d0;
constexpr const static size_t GetActiveItem = 0x1febc20;
}  // namespace PlayerBelt_Offsets

// obf name: ::%f2b9336746dcf6c0b83e64498d0ff44bae6ea222
#define LocalPlayer_ClassName           "%f2b9336746dcf6c0b83e64498d0ff44bae6ea222"
#define LocalPlayer_ClassNameShort      "%f2b9336746dcf6c0b83e64498d0ff44bae6ea222"
#define LocalPlayer_TypeDefinitionIndex 5824

namespace LocalPlayer_Offsets {

// Functions
constexpr const static size_t ItemCommand = 0x4F4CA40;
constexpr const static size_t MoveItem    = 0x0;
constexpr const static size_t get_Entity  = 0x4F4ECE0;
}  // namespace LocalPlayer_Offsets

// obf name: ::%adb5899b8a4d81f71c6ff3aaa5de8b98bce4600f
#define LocalPlayer_Static_ClassName             \
    "%f2b9336746dcf6c0b83e64498d0ff44bae6ea222/" \
    "%adb5899b8a4d81f71c6ff3aaa5de8b98bce4600f"
#define LocalPlayer_Static_ClassNameShort \
    "%adb5899b8a4d81f71c6ff3aaa5de8b98bce4600f"
#define LocalPlayer_Static_TypeDefinitionIndex 5827

namespace LocalPlayer_Static_Offsets {
// Updated from the new dump (resources::LocalPlayerStatic::TypeInfo)
inline constexpr std::uintptr_t typeinfo      = 0xF1911E8;
constexpr auto                  static_fields = 0xb8;

// Updated from the new dump (resources::LocalPlayerStatic::Entity)
constexpr const static size_t Entity = resources::LocalPlayerStatic::Entity;
constexpr const static size_t hv_in_wrapper = 0x18;
}  // namespace LocalPlayer_Static_Offsets

// obf name: ::%c19a4ffc776fc683558cfff1fdd73055711422b2
#define BasePlayer_Static_ClassName \
    "BasePlayer/%c19a4ffc776fc683558cfff1fdd73055711422b2"
#define BasePlayer_Static_ClassNameShort \
    "%c19a4ffc776fc683558cfff1fdd73055711422b2"
#define BasePlayer_Static_TypeDefinitionIndex 5220

namespace BasePlayer_Static_Offsets {
inline constexpr std::uintptr_t typeinfo      = 0xF12D6D8;
constexpr auto                  static_fields = 0xb8;

// Offsets
constexpr const static size_t visiblePlayerList = 0x9B0;
}  // namespace BasePlayer_Static_Offsets

#define BasePlayer_TypeDefinitionIndex 5196

namespace BasePlayer_Offsets {
inline constexpr std::uintptr_t typeinfo = 0xF05C290;  // NeoRed klass_rvas

// Offsets (new dump: BasePlayer)
constexpr const static size_t playerModel                = 0x738;
constexpr const static size_t input                      = 0x678;
constexpr const static size_t movement                   = 0x590;
constexpr const static size_t clActiveItem               = 0x5B0;
constexpr const static size_t modelState                 = 0x4C0;
constexpr const static size_t playerFlags                = 0x708;
constexpr const static size_t eyes                       = 0x328;
constexpr const static size_t inventory                  = 0x528;
constexpr const static size_t _displayName               = 0x810;
constexpr const static size_t _lookingAt                 = 0x520;
constexpr const static size_t lastSentTickTime           = 0x6C0;
constexpr const static size_t lastSentTick               = 0x7E0;
constexpr const static size_t mounted                    = 0x608;
constexpr const static size_t weaponMoveSpeedScale       = 0x7F0;
constexpr const static size_t clothingMoveSpeedReduction = 0x7F8;
constexpr const static size_t clothingWaterSpeedBonus    = 0x7FC;
// new dump: additional BasePlayer members
constexpr const static size_t modifiers                  = 0x748;
// Not in the new dump; last known values (unverified on this build).
constexpr const static size_t currentTeam                = 0x580;
constexpr const static size_t userID                     = 0x750;
constexpr const static size_t clientTeam                 = 0x358;

// Offsets (OLD BUILD: not present in the new dump)
constexpr const static size_t playerRigidbody            = 0x6f0;
constexpr const static size_t UserIDString               = 0x628;
constexpr const static size_t CurrentTutorialAllowance   = 0x0;
constexpr const static size_t nextVisThink               = 0x0;
constexpr const static size_t Belt                       = 0x520;
constexpr const static size_t _lookingAtEntity           = 0x7a0;
constexpr const static size_t currentGesture             = 0x318;
constexpr const static size_t clothingBlocksAiming       = 0x7bc;
constexpr const static size_t equippingBlocked           = 0x7cc;

// Functions (new dump: BasePlayer::OnViewModeChanged)
constexpr const static size_t OnViewModeChanged       = 0x5F73CD0;
// Functions (OLD BUILD)
constexpr const static size_t MakeVisible             = 0x0;
constexpr const static size_t ClientUpdateLocalPlayer = 0x615BA40;
constexpr const static size_t Menu_AssistPlayer       = 0x61754D0;
constexpr const static size_t ChatMessage             = 0x0;
constexpr const static size_t IsOnGround              = 0x37f7d70;
constexpr const static size_t GetSpeed                = 0x61276A0;
constexpr const static size_t CanBuild                = 0x3809810;
constexpr const static size_t GetMounted              = 0x60CC5A0;
constexpr const static size_t GetHeldEntity           = 0x6128E70;
constexpr const static size_t get_inventory           = 0x60E7E00;
constexpr const static size_t get_eyes                = 0x611FF70;
constexpr const static size_t SendClientTick          = 0x6070790;
constexpr const static size_t ClientInput             = 0x6152180;
constexpr const static size_t ClientInput_vtableoff   = 0x0;
constexpr const static size_t MaxHealth               = 0x0;
constexpr const static size_t MaxHealth_vtableoff     = 0x0;
constexpr const static size_t OnAttacked              = 0x0;
constexpr const static size_t OnAttacked_vtableoff    = 0x0;
constexpr const static size_t get_idealViewMode       = 0x61B8600;
}  // namespace BasePlayer_Offsets

#define ScientistNPC_TypeDefinitionIndex 8040

namespace ScientistNPC_Offsets {

// Offsets (new dump: ScientistNPC)
constexpr const static size_t IdleChatterRepeatRange = 0x8B8;
constexpr const static size_t deathStatName          = 0x8B0;
constexpr const static size_t radioChatterType       = 0x8C0;
}

#define TunnelDweller_TypeDefinitionIndex 3534

namespace TunnelDweller_Offsets {

// Offsets
}

#define UnderwaterDweller_TypeDefinitionIndex 730

namespace UnderwaterDweller_Offsets {

// Offsets
}

#define ScarecrowNPC_TypeDefinitionIndex 2034

namespace ScarecrowNPC_Offsets {

// Offsets
}

#define GingerbreadNPC_TypeDefinitionIndex 9085

namespace GingerbreadNPC_Offsets {

// Offsets
}

#define BaseMovement_TypeDefinitionIndex 4415

namespace BaseMovement_Offsets {

// Offsets (new dump: BaseMovement)
constexpr const static size_t adminCheat        = 0x0;  // OLD BUILD
constexpr const static size_t Owner             = 0x30;
constexpr const static size_t InheritedVelocity = 0x38;
constexpr const static size_t TargetMovement    = 0x44;
constexpr const static size_t Running           = 0x50;
constexpr const static size_t Ducking           = 0x54;
constexpr const static size_t Crawling          = 0x58;
constexpr const static size_t Grounded          = 0x5c;
constexpr const static size_t lastTeleportedTime = 0x60;
}  // namespace BaseMovement_Offsets

#define PlayerWalkMovement_TypeDefinitionIndex 1366

namespace PlayerWalkMovement_Offsets {

// Offsets (new dump: PlayerWalkMovement)
constexpr const static size_t maxVelocity               = 0x108;
constexpr const static size_t groundAngle               = 0x110;
constexpr const static size_t groundAngleNew            = 0x118;
constexpr const static size_t groundTime                = 0x120;
constexpr const static size_t jumpTime                  = 0x128;
constexpr const static size_t landTime                  = 0x130;
constexpr const static size_t previousPosition          = 0x138;
constexpr const static size_t previousVelocity          = 0x148;
constexpr const static size_t previousInheritedVelocity = 0x158;
constexpr const static size_t groundNormal              = 0x168;
constexpr const static size_t groundNormalNew           = 0x178;
constexpr const static size_t groundVelocity            = 0x188;
constexpr const static size_t groundVelocityNew         = 0x198;

// Offsets (OLD BUILD: not present in the new dump)
constexpr const static size_t capsule = 0xE8;
constexpr const static size_t ladder  = 0xF8;
constexpr const static size_t modify  = 0x1CC;

// Functions (OLD BUILD)
constexpr const static size_t Init                    = 0x0;
constexpr const static size_t BlockJump               = 0x1786B90;
constexpr const static size_t BlockSprint             = 0x17AE150;
constexpr const static size_t GroundCheck             = 0x17B0AE0;
constexpr const static size_t ClientInput             = 0x17E4510;
constexpr const static size_t ClientInput_vtableoff   = 0x0;
constexpr const static size_t DoFixedUpdate           = 0x17B83B0;
constexpr const static size_t DoFixedUpdate_vtableoff = 0x668;
constexpr const static size_t FrameUpdate             = 0x0;
constexpr const static size_t FrameUpdate_vtableoff   = 0x0;
constexpr const static size_t TeleportTo              = 0x3EBC100;
constexpr const static size_t TeleportTo_vtableoff    = 0x0;
}  // namespace PlayerWalkMovement_Offsets

#define BuildingPrivlidge_TypeDefinitionIndex 7851

namespace BuildingPrivlidge_Offsets {

// Offsets
constexpr const static size_t allowedConstructionItems = 0x428;
constexpr const static size_t cachedProtectedMinutes   = 0x418;
}  // namespace BuildingPrivlidge_Offsets

#define WorldItem_TypeDefinitionIndex 3172

namespace WorldItem_Offsets {
inline constexpr std::uintptr_t typeinfo = 0xF143040;  // NeoRed klass_rvas

// Offsets (new dump: WorldItem)
constexpr const static size_t allowPickup = 0x218;
constexpr const static size_t item        = 0x220;  // dump: Item
}  // namespace WorldItem_Offsets

#define HackableLockedCrate_TypeDefinitionIndex 3059

namespace HackableLockedCrate_Offsets {

// Offsets
constexpr const static size_t timerText   = 0x418;
constexpr const static size_t hackSeconds = 0x428;
}  // namespace HackableLockedCrate_Offsets

#define ProjectileWeaponMod_TypeDefinitionIndex 1516

namespace ProjectileWeaponMod_Offsets {
inline constexpr std::uintptr_t typeinfo = 0xf060be0;

// Offsets (new dump: ProjectileWeaponMod)
constexpr const static size_t isSilencer         = 0x230;
constexpr const static size_t projectileVelocity = 0x244;
constexpr const static size_t silencerType       = 0x234;

// Offsets (OLD BUILD: not present in the new dump)
constexpr const static size_t repeatDelay        = 0x238;
constexpr const static size_t projectileDamage   = 0x250;
constexpr const static size_t projectileDistance = 0x25c;
constexpr const static size_t aimsway            = 0x268;
constexpr const static size_t aimswaySpeed       = 0x274;
constexpr const static size_t recoil             = 0x280;
constexpr const static size_t sightAimCone       = 0x28c;
constexpr const static size_t hipAimCone         = 0x298;
constexpr const static size_t magazineCapacity   = 0x2ac;
constexpr const static size_t needsOnForEffects  = 0x2b8;
}  // namespace ProjectileWeaponMod_Offsets

#define ProjectileWeaponMod_Modifier_TypeDefinitionIndex 1518

namespace ProjectileWeaponMod_Modifier_Offsets {
constexpr const static size_t enabled = 0x0;
constexpr const static size_t scalar  = 0x4;
constexpr const static size_t offset  = 0x8;
}  // namespace ProjectileWeaponMod_Modifier_Offsets

// obf name: ::%3b1f7f18b8ef19b17dafed2db0070144a25a3536
#define ConsoleSystem_ClassName           "%3b1f7f18b8ef19b17dafed2db0070144a25a3536"
#define ConsoleSystem_ClassNameShort      "%3b1f7f18b8ef19b17dafed2db0070144a25a3536"
#define ConsoleSystem_TypeDefinitionIndex 2

namespace ConsoleSystem_Offsets {

// Functions
constexpr const static size_t Run = 0x0;
}  // namespace ConsoleSystem_Offsets

// obf name: ::%23e7a3f5b2ec3908d42357abfd9e78930c99172d
#define ConsoleSystem_Index_Static_ClassName      \
    "%3b1f7f18b8ef19b17dafed2db0070144a25a3536/"  \
    "%659d0ebd747d1d180d340f9b604cd2885f2dc741.%" \
    "23e7a3f5b2ec3908d42357abfd9e78930c99172d"
#define ConsoleSystem_Index_Static_ClassNameShort \
    "%23e7a3f5b2ec3908d42357abfd9e78930c99172d"
#define ConsoleSystem_Index_Static_TypeDefinitionIndex 16

namespace ConsoleSystem_Index_Static_Offsets {
inline constexpr std::uintptr_t typeinfo      = 0xF0201E0;
constexpr auto                  static_fields = 0xb8;

// Offsets
constexpr const static size_t All = 0x50;
}  // namespace ConsoleSystem_Index_Static_Offsets

#define LootableCorpse_TypeDefinitionIndex 7120

namespace LootableCorpse_Offsets {

// Offsets (new dump: LootableCorpse)
constexpr const static size_t lootPanelName = 0x330;

// Offsets (OLD BUILD: not present in the new dump)
constexpr const static size_t playerSteamID = 0x338;
constexpr const static size_t _playerName   = 0x328;
}  // namespace LootableCorpse_Offsets

#define DroppedItemContainer_TypeDefinitionIndex 4185

namespace DroppedItemContainer_Offsets {
inline constexpr std::uintptr_t typeinfo = 0xF196900;  // NeoRed klass_rvas

// Offsets
constexpr const static size_t playerSteamID = 0x300;
constexpr const static size_t _playerName   = 0x2E8;
}  // namespace DroppedItemContainer_Offsets

#define MainCamera_TypeDefinitionIndex 3541

namespace MainCamera_Offsets {
// NeoRed SDK (build 26499011, probed at runtime): MainCamera klass rva.
inline constexpr std::uintptr_t typeinfo      = 0xF0C83A0;
constexpr auto                  static_fields = 0xb8;

// Updated from the current dump (MainCamera_Static::camera)
constexpr const static size_t mainCamera          = resources::MainCamera::mainCamera;
constexpr const static size_t mainCameraTransform = 0x68;  // NeoRed main_camera_static
constexpr const static size_t position            = 0x444;
constexpr const static size_t viewMatrix          = 0x2FC;
constexpr const static size_t cullingMask         = 0x42C;

// Functions
constexpr const static size_t Update    = 0x3f8b330;
constexpr const static size_t OnPreCull = 0x3f8c360;
constexpr const static size_t Trace     = 0x3F7D7B0;
}  // namespace MainCamera_Offsets

#define CameraMan_TypeDefinitionIndex 7607

namespace CameraMan_Offsets {

// Offsets
constexpr const static size_t OnlyControlWhenCursorHidden = 0x20;
constexpr const static size_t NeedBothMouseButtonsToZoom  = 0x21;
constexpr const static size_t LookSensitivity             = 0x24;
constexpr const static size_t MoveSpeed                   = 0x28;
constexpr const static size_t canvas                      = 0x30;
constexpr const static size_t guides                      = 0x38;
}  // namespace CameraMan_Offsets

// obf name: ::%918c3b3bcd7d4bae8d906d7a60733f7ea63b1872
#define PlayerTick_ClassName           "%918c3b3bcd7d4bae8d906d7a60733f7ea63b1872"
#define PlayerTick_ClassNameShort      "%918c3b3bcd7d4bae8d906d7a60733f7ea63b1872"
#define PlayerTick_TypeDefinitionIndex 253

namespace PlayerTick_Offsets {

// Offsets (new dump: PlayerTick)
constexpr const static size_t activeItem = 0x20;
constexpr const static size_t eyePos     = 0x34;
constexpr const static size_t inputState = 0x10;
constexpr const static size_t modelState = 0x50;
constexpr const static size_t parentID   = 0x40;
constexpr const static size_t position   = 0x28;

// Functions (OLD BUILD)
constexpr const static size_t WriteToStreamDelta           = 0xA966800;
constexpr const static size_t WriteToStreamDelta_vtableoff = 0x1b8;
constexpr const static size_t WriteToStream                = 0xA96E040;
constexpr const static size_t WriteToStream_vtableoff      = 0x1D8;
}  // namespace PlayerTick_Offsets

// obf name: ::%b8f262f841f3c6c447d7f81fd6f4923545be89c3
#define InputMessage_ClassName           "%b8f262f841f3c6c447d7f81fd6f4923545be89c3"
#define InputMessage_ClassNameShort      "%b8f262f841f3c6c447d7f81fd6f4923545be89c3"
#define InputMessage_TypeDefinitionIndex 691

namespace InputMessage_Offsets {
inline constexpr std::uintptr_t typeinfo = 0xF037450;

// Offsets
constexpr const static size_t buttons    = 0x34;
constexpr const static size_t aimAngles  = 0x28;
constexpr const static size_t mouseDelta = 0x18;
}  // namespace InputMessage_Offsets

// obf name: ::%4e72d93d9afe4a5fce6a40252e0065d505333b1e
#define InputState_ClassName           "%4e72d93d9afe4a5fce6a40252e0065d505333b1e"
#define InputState_ClassNameShort      "%4e72d93d9afe4a5fce6a40252e0065d505333b1e"
#define InputState_TypeDefinitionIndex 3786

namespace InputState_Offsets {
inline constexpr std::uintptr_t typeinfo = 0xF1D0F00;

// Offsets
constexpr const static size_t current  = 0x20;
constexpr const static size_t previous = 0x10;
}  // namespace InputState_Offsets

#define PlayerInput_TypeDefinitionIndex 4877

namespace PlayerInput_Offsets {
inline constexpr std::uintptr_t typeinfo      = 0xf1bd330;
constexpr auto                  static_fields = 0xb8;

// Offsets
constexpr const static size_t state      = 0x28;
constexpr const static size_t bodyAngles = 0x44;
}  // namespace PlayerInput_Offsets

// obf name: ::%2838d3190d9986c7e95f635cd226f378e2e0e2c5
#define ModelState_ClassName           "%2838d3190d9986c7e95f635cd226f378e2e0e2c5"
#define ModelState_ClassNameShort      "%2838d3190d9986c7e95f635cd226f378e2e0e2c5"
#define ModelState_TypeDefinitionIndex 527

namespace ModelState_Offsets {
inline constexpr std::uintptr_t typeinfo = 0xF0A8FC8;

// Offsets (new dump: ModelState)
constexpr const static size_t flags      = 0x10;
constexpr const static size_t lookDir    = 0x14;
constexpr const static size_t waterLevel = 0x70;

// Offsets (OLD BUILD: not present in the new dump)
constexpr const static size_t guidePosition     = 0x2c;
constexpr const static size_t ducked            = 0x40;
constexpr const static size_t sprinting         = 0x69;
constexpr const static size_t poseType          = 0x14;
constexpr const static size_t ladderType        = 0x3c;
constexpr const static size_t movementType      = 0x78;
constexpr const static size_t parentID          = 0x24;
constexpr const static size_t waterLevelHead    = 0x38;
constexpr const static size_t inheritedVelocity = 0x44;
constexpr const static size_t localShieldPos    = 0x50;
constexpr const static size_t guideRotation     = 0x5c;
constexpr const static size_t localShieldRot    = 0x6c;
}  // namespace ModelState_Offsets

// obf name: ::%580b72c4a898da2110fea9ef0c300da7d2eb3ac7
#define Item_ClassName           "%580b72c4a898da2110fea9ef0c300da7d2eb3ac7"
#define Item_ClassNameShort      "%580b72c4a898da2110fea9ef0c300da7d2eb3ac7"
#define Item_TypeDefinitionIndex 8299

namespace Item_Offsets {
inline constexpr std::uintptr_t typeinfo = 0xF03F138;

// Offsets (new dump: Item)
constexpr const static size_t amount     = 0x60;
constexpr const static size_t heldEntity = 0xC0;
constexpr const static size_t info       = 0x38;
constexpr const static size_t position   = 0x5C;
constexpr const static size_t uid        = 0xD8;
constexpr const static size_t worldEnt   = 0x18;

// Offsets (OLD BUILD: not present in the new dump)
constexpr const static size_t clientAmmoCount = 0xD0;
constexpr const static size_t contents        = 0x20;
constexpr const static size_t parent          = 0x38;
constexpr const static size_t _condition      = 0x68;
constexpr const static size_t _maxCondition   = 0x98;

// Functions (OLD BUILD)
constexpr const static size_t get_iconSprite = 0x5f1c9f0;
}  // namespace Item_Offsets

// obf name: ::%212075ba8fd7f55c828ed853268227b73eccc325
#define WaterLevel_ClassName           "%212075ba8fd7f55c828ed853268227b73eccc325"
#define WaterLevel_ClassNameShort      "%212075ba8fd7f55c828ed853268227b73eccc325"
#define WaterLevel_TypeDefinitionIndex 5777

namespace WaterLevel_Offsets {
inline constexpr std::uintptr_t typeinfo      = 0x10c8a2e8;
constexpr auto                  static_fields = 0xb8;

// Functions
constexpr const static size_t Test          = 0x40edbd0;
constexpr const static size_t GetWaterLevel = 0x4110880;
}  // namespace WaterLevel_Offsets

// obf name: ::%e811f2a520c49d12baf319a57be3641ea6d6fbeb
#define ConVar_Graphics_Static_ClassName         \
    "%f36914e3a709c87961a9c9aa6b629645380ad02d/" \
    "%e811f2a520c49d12baf319a57be3641ea6d6fbeb"
#define ConVar_Graphics_Static_ClassNameShort \
    "%e811f2a520c49d12baf319a57be3641ea6d6fbeb"
#define ConVar_Graphics_Static_TypeDefinitionIndex 6866

namespace ConVar_Graphics_Static_Offsets {
inline constexpr std::uintptr_t typeinfo      = 0xf0fec78;
constexpr auto                  static_fields = 0xb8;

// Offsets (new dump: ConvarGraphicsStatic::_fov)
constexpr const static size_t _fov = 0x6a8;

// Functions
}  // namespace ConVar_Graphics_Static_Offsets

#define BaseFishingRod_TypeDefinitionIndex 7145

namespace BaseFishingRod_Offsets {
inline constexpr std::uintptr_t typeinfo = 0x10c09fe8;

// Offsets
constexpr const static size_t CurrentState                 = 0x0;
constexpr const static size_t currentBobber                = 0x310;
constexpr const static size_t MaxCastDistance              = 0x344;
constexpr const static size_t BobberPreview                = 0x350;
constexpr const static size_t clientStrainAmountNormalised = 0x380;
constexpr const static size_t strainGainMod                = 0x378;
constexpr const static size_t aimAnimationReady            = 0x398;

// Functions
constexpr const static size_t UpdateLineRenderer      = 0x4ecd9c0;
constexpr const static size_t EvaluateFishingPosition = 0x4eda220;
}  // namespace BaseFishingRod_Offsets

#define FishingBobber_TypeDefinitionIndex 4860

namespace FishingBobber_Offsets {

// Offsets
constexpr const static size_t bobberRoot = 0x300;
}  // namespace FishingBobber_Offsets

#define GameManifest_TypeDefinitionIndex 8472

namespace GameManifest_Offsets {

// Functions
constexpr const static size_t GUIDToObject = 0x1C732F0;
}  // namespace GameManifest_Offsets

// obf name: ::%310bade51621c994ea3a1d812b6203cf64a1a29c
#define GameManager_ClassName           "%310bade51621c994ea3a1d812b6203cf64a1a29c"
#define GameManager_ClassNameShort      "%310bade51621c994ea3a1d812b6203cf64a1a29c"
#define GameManager_TypeDefinitionIndex 161

namespace GameManager_Offsets {
inline constexpr std::uintptr_t typeinfo = 0xF0BEF48;

// Offsets
constexpr const static size_t pool = 0x10;

// Functions
constexpr const static size_t CreatePrefab = 0x1738140;
}  // namespace GameManager_Offsets

// obf name: ::%da47aec4ff0d4e97d887fc0aa6e325f03b909835
#define GameManager_Static_ClassName             \
    "%310bade51621c994ea3a1d812b6203cf64a1a29c/" \
    "%da47aec4ff0d4e97d887fc0aa6e325f03b909835"
#define GameManager_Static_ClassNameShort \
    "%da47aec4ff0d4e97d887fc0aa6e325f03b909835"
#define GameManager_Static_TypeDefinitionIndex 165

namespace GameManager_Static_Offsets {
inline constexpr std::uintptr_t typeinfo      = 0xF0C5128;
constexpr auto                  static_fields = 0xb8;

// Offsets (new dump: GameManagerStatic::client)
constexpr const static size_t client = 0x8;
}  // namespace GameManager_Static_Offsets

// obf name: ::%310bade51621c994ea3a1d812b6203cf64a1a29c
#define PrefabPoolCollection_ClassName \
    "%310bade51621c994ea3a1d812b6203cf64a1a29c"
#define PrefabPoolCollection_ClassNameShort \
    "%310bade51621c994ea3a1d812b6203cf64a1a29c"
#define PrefabPoolCollection_TypeDefinitionIndex 161

namespace PrefabPoolCollection_Offsets {
inline constexpr std::uintptr_t typeinfo = 0x10bc7ca8;

// Offsets
constexpr const static size_t storage = 0x10;
}  // namespace PrefabPoolCollection_Offsets

// obf name: ::%40e108d0f01ca6ceee2ea6733755f1eb97f76706
#define PrefabPool_ClassName           "%40e108d0f01ca6ceee2ea6733755f1eb97f76706"
#define PrefabPool_ClassNameShort      "%40e108d0f01ca6ceee2ea6733755f1eb97f76706"
#define PrefabPool_TypeDefinitionIndex 1016

namespace PrefabPool_Offsets {

// Offsets
constexpr const static size_t stack = 0x0;
}  // namespace PrefabPool_Offsets

#define ItemModProjectile_TypeDefinitionIndex 876

namespace ItemModProjectile_Offsets {

// Offsets
constexpr const static size_t projectileObject         = 0x20;
constexpr const static size_t ammoType                 = 0x30;
constexpr const static size_t projectileSpread         = 0x3c;
constexpr const static size_t projectileVelocity       = 0x40;
constexpr const static size_t projectileVelocitySpread = 0x44;
constexpr const static size_t useCurve                 = 0x48;
constexpr const static size_t spreadScalar             = 0x50;
constexpr const static size_t category                 = 0x68;
}  // namespace ItemModProjectile_Offsets

#define CraftingQueue_TypeDefinitionIndex 823

namespace CraftingQueue_Offsets {

// Offsets
constexpr const static size_t icons = 0x30;
}  // namespace CraftingQueue_Offsets

// obf name: ::%20bd4f14959c3ba644df45d869f4f9231cbb02d1
#define CraftingQueue_Static_ClassName \
    "CraftingQueue/%20bd4f14959c3ba644df45d869f4f9231cbb02d1"
#define CraftingQueue_Static_ClassNameShort \
    "%20bd4f14959c3ba644df45d869f4f9231cbb02d1"
#define CraftingQueue_Static_TypeDefinitionIndex 824

namespace CraftingQueue_Static_Offsets {
inline constexpr std::uintptr_t typeinfo      = 0xF2762F0;
constexpr auto                  static_fields = 0xb8;

// Offsets
constexpr const static size_t isCrafting = 0x0;
}  // namespace CraftingQueue_Static_Offsets

#define CraftingQueueIcon_TypeDefinitionIndex 2615

namespace CraftingQueueIcon_Offsets {

// Offsets
constexpr const static size_t endTime = 0x5c;
constexpr const static size_t item    = 0x70;
}  // namespace CraftingQueueIcon_Offsets

// obf name: ::%0d597890ac98fb1f4ed24e30f8dc5dd62ab633f7
#define Planner_Static_ClassName \
    "Planner/%0d597890ac98fb1f4ed24e30f8dc5dd62ab633f7"
#define Planner_Static_ClassNameShort \
    "%0d597890ac98fb1f4ed24e30f8dc5dd62ab633f7"
#define Planner_Static_TypeDefinitionIndex 6854

namespace Planner_Static_Offsets {
inline constexpr std::uintptr_t typeinfo      = 0xF0D0CD8;
constexpr auto                  static_fields = 0xb8;

// Offsets
constexpr const static size_t guide = 0x110;
}  // namespace Planner_Static_Offsets

// obf name: ::%180dee5d6fe56dbf163d33e93610989bc8e9488b
#define Planner_Guide_ClassName \
    "Planner/%180dee5d6fe56dbf163d33e93610989bc8e9488b"
#define Planner_Guide_ClassNameShort      "%180dee5d6fe56dbf163d33e93610989bc8e9488b"
#define Planner_Guide_TypeDefinitionIndex 6847

namespace Planner_Guide_Offsets {
inline constexpr std::uintptr_t typeinfo = 0xF06E960;

// Offsets
constexpr const static size_t lastPlacement = 0x40;
}  // namespace Planner_Guide_Offsets

#define Planner_TypeDefinitionIndex 6846

namespace Planner_Offsets {
inline constexpr std::uintptr_t typeinfo = 0xf1d7e90;

// Offsets
constexpr const static size_t _currentConstruction = 0x318;
}  // namespace Planner_Offsets

#define Construction_TypeDefinitionIndex 529

namespace Construction_Offsets {

// Offsets
constexpr const static size_t holdToPlaceDuration = 0x170;
constexpr const static size_t grades              = 0x168;
}  // namespace Construction_Offsets

#define BuildingBlock_TypeDefinitionIndex 9120

namespace BuildingBlock_Offsets {

// Offsets (new dump: BuildingBlock)
constexpr const static size_t apartmentList   = 0x20;
constexpr const static size_t blockDefinition = 0x318;  // OLD BUILD
constexpr const static size_t grade           = 0x368;

// Functions
}  // namespace BuildingBlock_Offsets

// obf name: ::HeldEntity
#define HeldEntity_ClassName           "HeldEntity"
#define HeldEntity_ClassNameShort      "HeldEntity"
#define HeldEntity_TypeDefinitionIndex 6501

namespace HeldEntity_Offsets {
inline constexpr std::uintptr_t typeinfo = 0xf1dfb18;

// Offsets (new dump: HeldEntity)
constexpr const static size_t viewModel    = 0x218;

// Offsets (OLD BUILD: not present in the new dump)
constexpr const static size_t ownerItemUID = 0x2F8;
constexpr const static size_t _punches     = 0x258;

// Functions (OLD BUILD)
constexpr const static size_t OnDeploy = 0x0;
}  // namespace HeldEntity_Offsets

// obf name: ::%08841741149fc7f4f79b8637034bb043da0559e7
#define PunchEntry_ClassName \
    "HeldEntity/%08841741149fc7f4f79b8637034bb043da0559e7"
#define PunchEntry_ClassNameShort      "%08841741149fc7f4f79b8637034bb043da0559e7"
#define PunchEntry_TypeDefinitionIndex 6502

namespace PunchEntry_Offsets {
inline constexpr std::uintptr_t typeinfo = 0x10bee8c8;

// Offsets
}  // namespace PunchEntry_Offsets

#define IronSights_TypeDefinitionIndex 132

namespace IronSights_Offsets {

// Offsets
constexpr const static size_t zoomFactor         = 0x2c;
constexpr const static size_t ironsightsOverride = 0x68;
}  // namespace IronSights_Offsets

#define IronSightOverride_TypeDefinitionIndex 4606

namespace IronSightOverride_Offsets {

// Offsets
constexpr const static size_t zoomFactor = 0x2c;
constexpr const static size_t fovBias    = 0x30;
}  // namespace IronSightOverride_Offsets

#define BaseViewModel_TypeDefinitionIndex 1490

namespace BaseViewModel_Offsets {

// Offsets (new dump: BaseViewModel)
constexpr const static size_t useViewModelCamera = 0x40;
constexpr const static size_t punch              = 0xC0;
constexpr const static size_t lower              = 0xB8;
constexpr const static size_t aspectOffset       = 0x88;
constexpr const static size_t sway               = 0xC8;
constexpr const static size_t animator           = 0xA0;
constexpr const static size_t bob                = 0x90;

// Offsets (OLD BUILD: not present in the new dump)
constexpr const static size_t ironSights         = 0xD0;
constexpr const static size_t model              = 0x88;

// Functions (OLD BUILD)
constexpr const static size_t get_ActiveModel                   = 0x3BFC780;
constexpr const static size_t OnCameraPositionChanged           = 0x0;
constexpr const static size_t OnCameraPositionChanged_vtableoff = 0x0;
}  // namespace BaseViewModel_Offsets

#define ViewModel_TypeDefinitionIndex 7934

namespace ViewModel_Offsets {

// Offsets
constexpr const static size_t instance = 0x28;

// Functions
constexpr const static size_t PlayInt    = 0x16933B0;
constexpr const static size_t PlayString = 0x1693420;
}  // namespace ViewModel_Offsets

#define MedicalTool_TypeDefinitionIndex 6588

namespace MedicalTool_Offsets {

// Offsets
constexpr const static size_t resetTime = 0x3a0;
}  // namespace MedicalTool_Offsets

#define WaterBody_TypeDefinitionIndex 3837

namespace WaterBody_Offsets {

// Offsets
constexpr const static size_t Type                    = 0x20;
constexpr const static size_t Renderer                = 0x28;
constexpr const static size_t Triggers                = 0x30;
constexpr const static size_t MaterialBlend           = 0x38;
constexpr const static size_t WantBlendFactorOverride = 0x40;
constexpr const static size_t BlendFactorOverride     = 0x44;
constexpr const static size_t IsOcean                 = 0x48;
constexpr const static size_t meshFilter              = 0x98;
constexpr const static size_t FishingType             = 0x58;
}  // namespace WaterBody_Offsets

// obf name: ::%e5b187b5f926ad45477e011538a6eb88f764417f
#define WaterSystem_Static_ClassName \
    "WaterSystem/%e5b187b5f926ad45477e011538a6eb88f764417f"
#define WaterSystem_Static_ClassNameShort \
    "%e5b187b5f926ad45477e011538a6eb88f764417f"
#define WaterSystem_Static_TypeDefinitionIndex 2578

namespace WaterSystem_Static_Offsets {
inline constexpr std::uintptr_t typeinfo      = 0xF0F0E90;
constexpr auto                  static_fields = 0xb8;

// Offsets
}  // namespace WaterSystem_Static_Offsets

#define WaterSystem_TypeDefinitionIndex 2573

namespace WaterSystem_Offsets {

// Offsets
constexpr const static size_t oceanSettings    = 0x20;
constexpr const static size_t Quality          = 0x70;
constexpr const static size_t oceanMaterial    = 0x78;
constexpr const static size_t Rendering        = 0x80;
constexpr const static size_t oceanVFaceShader = 0x88;
constexpr const static size_t TropicalMaterial = 0x90;
constexpr const static size_t patchSize        = 0x98;
constexpr const static size_t patchCount       = 0x9c;
constexpr const static size_t patchScale       = 0xa0;
constexpr const static size_t forceDeepSea     = 0xa4;

// Functions
constexpr const static size_t get_Ocean = 0x66F0780;
}  // namespace WaterSystem_Offsets

#define TerrainMeta_TypeDefinitionIndex 2317

namespace TerrainMeta_Offsets {

// Functions
constexpr const static size_t Position    = 0x470;
constexpr const static size_t Size        = 0x8C8;
constexpr const static size_t OneOverSize = 0x20;
constexpr const static size_t Collision   = 0x4B8;
constexpr const static size_t HeightMap   = 0x100;
constexpr const static size_t SplatMap    = 0x578;
constexpr const static size_t TopologyMap = 0x3B8;
constexpr const static size_t Texturing   = 0x2A8;
}  // namespace TerrainMeta_Offsets

#define TerrainCollision_TypeDefinitionIndex 5289

namespace TerrainCollision_Offsets {

// Functions
constexpr const static size_t GetIgnore = 0x45E20F0;
}  // namespace TerrainCollision_Offsets

#define TerrainHeightMap_TypeDefinitionIndex 842

namespace TerrainHeightMap_Offsets {

// Offsets
constexpr const static size_t normY = 0x80;
}  // namespace TerrainHeightMap_Offsets

#define TerrainSplatMap_TypeDefinitionIndex 7505

namespace TerrainSplatMap_Offsets {

// Offsets
constexpr const static size_t num = 0x7c;
}  // namespace TerrainSplatMap_Offsets

#define TerrainTexturing_TypeDefinitionIndex 5964

namespace TerrainTexturing_Offsets {

// Offsets
constexpr const static size_t shoreVectors = 0x178;
}  // namespace TerrainTexturing_Offsets

// obf name: ::%1a966eb190360930b9f6474cf4edbf35160678c4
#define World_Static_ClassName                   \
    "%278655174c8edac17bbc78b3f2b8d424896aa26d/" \
    "%1a966eb190360930b9f6474cf4edbf35160678c4"
#define World_Static_ClassNameShort      "%1a966eb190360930b9f6474cf4edbf35160678c4"
#define World_Static_TypeDefinitionIndex 5429

namespace World_Static_Offsets {
inline constexpr std::uintptr_t typeinfo      = 0xF0CEF38;
constexpr auto                  static_fields = 0xb8;

// Offsets
}  // namespace World_Static_Offsets

#define ItemIcon_TypeDefinitionIndex 5451

namespace ItemIcon_Offsets {

// Offsets
constexpr const static size_t backgroundImage = 0xe8;

// Functions
constexpr const static size_t TryToMove           = 0x0;
constexpr const static size_t TryToMove_vtableoff = 0x0;
constexpr const static size_t RunTimedAction      = 0x0;
}  // namespace ItemIcon_Offsets

// obf name: ::%fe043ad86b3c258a7c0d538166d0fe99314ea5bf
#define ItemIcon_Static_ClassName \
    "ItemIcon/%fe043ad86b3c258a7c0d538166d0fe99314ea5bf"
#define ItemIcon_Static_ClassNameShort \
    "%fe043ad86b3c258a7c0d538166d0fe99314ea5bf"
#define ItemIcon_Static_TypeDefinitionIndex 5456

namespace ItemIcon_Static_Offsets {
inline constexpr std::uintptr_t typeinfo      = 0xF192F18;
constexpr auto                  static_fields = 0xb8;

// Offsets
}  // namespace ItemIcon_Static_Offsets

// obf name: ::%9cf421a3e24b2809fcb7b5c9eafc523efa940a9d
#define EffectData_ClassName           "%9cf421a3e24b2809fcb7b5c9eafc523efa940a9d"
#define EffectData_ClassNameShort      "%9cf421a3e24b2809fcb7b5c9eafc523efa940a9d"
#define EffectData_TypeDefinitionIndex 203

namespace EffectData_Offsets {

// Offsets
constexpr const static size_t entity = 0x30;
constexpr const static size_t source = 0x28;
}  // namespace EffectData_Offsets

// obf name: ::%ff6f7abc03925c5b6cd4ad9e7527725dfc00a678
#define Effect_ClassName           "%ff6f7abc03925c5b6cd4ad9e7527725dfc00a678"
#define Effect_ClassNameShort      "%ff6f7abc03925c5b6cd4ad9e7527725dfc00a678"
#define Effect_TypeDefinitionIndex 7960

namespace Effect_Offsets {
inline constexpr std::uintptr_t typeinfo = 0xF0E95B8;

// Offsets
constexpr const static size_t pooledString = 0xC0;
constexpr const static size_t worldPos     = 0x98;
}  // namespace Effect_Offsets

// obf name: ::%97c76f2ee7ed1bd6613b4b8d85dc549b1a576025
#define EffectNetwork_ClassName           "%97c76f2ee7ed1bd6613b4b8d85dc549b1a576025"
#define EffectNetwork_ClassNameShort      "%97c76f2ee7ed1bd6613b4b8d85dc549b1a576025"
#define EffectNetwork_TypeDefinitionIndex 873

namespace EffectNetwork_Offsets {

// Functions
}

// obf name: ::%297df1022c5ea15458457dc46ba1abe458265419
#define EffectNetwork_Static_ClassName           \
    "%97c76f2ee7ed1bd6613b4b8d85dc549b1a576025/" \
    "%297df1022c5ea15458457dc46ba1abe458265419"
#define EffectNetwork_Static_ClassNameShort \
    "%297df1022c5ea15458457dc46ba1abe458265419"
#define EffectNetwork_Static_TypeDefinitionIndex 874

namespace EffectNetwork_Static_Offsets {
inline constexpr std::uintptr_t typeinfo      = 0xF0B3498;
constexpr auto                  static_fields = 0xb8;

// Offsets
constexpr const static size_t effect = 0x20;

// Functions
constexpr const static size_t cctor = 0x541BBD0;
}  // namespace EffectNetwork_Static_Offsets

// obf name: ::%4aeb1f2433af3fd5837d757c36f3d157056483af
#define GameObjectEx_ClassName           "%4aeb1f2433af3fd5837d757c36f3d157056483af"
#define GameObjectEx_ClassNameShort      "%4aeb1f2433af3fd5837d757c36f3d157056483af"
#define GameObjectEx_TypeDefinitionIndex 4258

namespace GameObjectEx_Offsets {

// Functions
constexpr const static size_t ToBaseEntity = 0x2e6f4f0;
}  // namespace GameObjectEx_Offsets

#define UIDeathScreen_TypeDefinitionIndex 1639

namespace UIDeathScreen_Offsets {

// Functions
constexpr const static size_t SetVisible = 0x32a3fd0;
}  // namespace UIDeathScreen_Offsets

// obf name: ::%7cab04d1d3a2c9488619ecd15abe0ea804040dba
#define BaseScreenShake_Static_ClassName \
    "BaseScreenShake/%7cab04d1d3a2c9488619ecd15abe0ea804040dba"
#define BaseScreenShake_Static_ClassNameShort \
    "%7cab04d1d3a2c9488619ecd15abe0ea804040dba"
#define BaseScreenShake_Static_TypeDefinitionIndex 5239

namespace BaseScreenShake_Static_Offsets {
inline constexpr std::uintptr_t typeinfo      = 0xF268EA0;
constexpr auto                  static_fields = 0xb8;

// Offsets
constexpr const static size_t list = 0x90;
}  // namespace BaseScreenShake_Static_Offsets

#define FlashbangOverlay_TypeDefinitionIndex 7883

namespace FlashbangOverlay_Offsets {
inline constexpr std::uintptr_t typeinfo      = 0xf17fdb0;
constexpr auto                  static_fields = 0xb8;

// Offsets
constexpr const static size_t Instance    = 0x18;
constexpr const static size_t flashLength = 0x58;
}  // namespace FlashbangOverlay_Offsets

// obf name: ::%71137ea0ded4e0a0c736b91750a0704e2a0a692c
#define StringPool_ClassName           "%71137ea0ded4e0a0c736b91750a0704e2a0a692c"
#define StringPool_ClassNameShort      "%71137ea0ded4e0a0c736b91750a0704e2a0a692c"
#define StringPool_TypeDefinitionIndex 453

namespace StringPool_Offsets {
inline constexpr std::uintptr_t typeinfo      = 0xF1CBD28;
constexpr auto                  static_fields = 0xb8;

// Offsets (new dump: StringPool::toNumber)
constexpr const static size_t toNumber = 0x80;

// Functions (OLD BUILD)
constexpr const static size_t Get = 0x3022BA0;
}  // namespace StringPool_Offsets

// obf name: ::%acdbf8ec27705964aef08a32de64b99ad5ae6ce4
#define Network_Networkable_ClassName \
    "%acdbf8ec27705964aef08a32de64b99ad5ae6ce4"
#define Network_Networkable_ClassNameShort \
    "%acdbf8ec27705964aef08a32de64b99ad5ae6ce4"
#define Network_Networkable_TypeDefinitionIndex 42

namespace Network_Networkable_Offsets {

// Offsets
constexpr const static size_t ID = 0x18;
}  // namespace Network_Networkable_Offsets

// obf name: ::%db78a311e69e50aac3cd94dfd84e36ec6148c1f4
#define Network_Net_ClassName           "%db78a311e69e50aac3cd94dfd84e36ec6148c1f4"
#define Network_Net_ClassNameShort      "%db78a311e69e50aac3cd94dfd84e36ec6148c1f4"
#define Network_Net_TypeDefinitionIndex 124

namespace Network_Net_Offsets {
inline constexpr std::uintptr_t typeinfo      = 0xF0404D8;
constexpr auto                  static_fields = 0xb8;

// Offsets
constexpr const static size_t cl = 0x20;
}  // namespace Network_Net_Offsets

// obf name: ::%7639913094299e7aa0408afd0d5c7df289315589
#define Network_Client_ClassName "%7639913094299e7aa0408afd0d5c7df289315589"
#define Network_Client_ClassNameShort \
    "%7639913094299e7aa0408afd0d5c7df289315589"
#define Network_Client_TypeDefinitionIndex 14

namespace Network_Client_Offsets {

// Offsets
constexpr const static size_t Connection       = 0xF8;
constexpr const static size_t ConnectedPort    = 0xf4;
constexpr const static size_t ConnectedAddress = 0xd8;
constexpr const static size_t ServerName       = 0xe0;

// Offsets
constexpr const static size_t CreateNetworkable  = 0x738b420;
constexpr const static size_t DestroyNetworkable = 0x738b1b0;
}  // namespace Network_Client_Offsets

// obf name: ::%6e4fac02033a2488133f486d69ffabdd959007d4
#define Network_BaseNetwork_ClassName \
    "%6e4fac02033a2488133f486d69ffabdd959007d4"
#define Network_BaseNetwork_ClassNameShort \
    "%6e4fac02033a2488133f486d69ffabdd959007d4"
#define Network_BaseNetwork_TypeDefinitionIndex 32

namespace Network_BaseNetwork_Offsets {}

// obf name: ::%993708e2e00ae8911aa5a7bc8d0e54e0e301c599
#define Network_SendInfo_ClassName "%993708e2e00ae8911aa5a7bc8d0e54e0e301c599"
#define Network_SendInfo_ClassNameShort \
    "%993708e2e00ae8911aa5a7bc8d0e54e0e301c599"
#define Network_SendInfo_TypeDefinitionIndex 69

namespace Network_SendInfo_Offsets {

// Offsets
constexpr const static size_t method      = 0x0;
constexpr const static size_t channel     = 0x4;
constexpr const static size_t priority    = 0x8;
constexpr const static size_t connections = 0x10;
constexpr const static size_t connection  = 0x18;
}  // namespace Network_SendInfo_Offsets

// obf name: ::%cc29022643a75e1ee49c841fd90e3ca86fc4a6fb
#define Network_Message_ClassName "%cc29022643a75e1ee49c841fd90e3ca86fc4a6fb"
#define Network_Message_ClassNameShort \
    "%cc29022643a75e1ee49c841fd90e3ca86fc4a6fb"
#define Network_Message_TypeDefinitionIndex 73

namespace Network_Message_Offsets {

// Offsets
constexpr const static size_t type = 0x20;
constexpr const static size_t read = 0x18;
}  // namespace Network_Message_Offsets

// obf name: ::%c14d2ce7c38a8a6df26df172455fda8c2912bea2
#define Network_NetRead_ClassName "%c14d2ce7c38a8a6df26df172455fda8c2912bea2"
#define Network_NetRead_ClassNameShort \
    "%c14d2ce7c38a8a6df26df172455fda8c2912bea2"
#define Network_NetRead_TypeDefinitionIndex 20

namespace Network_NetRead_Offsets {

// Offsets
constexpr const static size_t stream = 0x40;
}  // namespace Network_NetRead_Offsets

// obf name: ::%329ea8ebc5f0d99bd7a898dae01b7aef4376bac9
#define Network_NetWrite_ClassName "%329ea8ebc5f0d99bd7a898dae01b7aef4376bac9"
#define Network_NetWrite_ClassNameShort \
    "%329ea8ebc5f0d99bd7a898dae01b7aef4376bac9"
#define Network_NetWrite_TypeDefinitionIndex 51

namespace Network_NetWrite_Offsets {

// Offsets
constexpr const static size_t stream = 0x38;

// Functions
constexpr const static size_t WriteByte = 0x69C5C00;
constexpr const static size_t String    = 0x69C4E00;
constexpr const static size_t Send      = 0x69C6060;
}  // namespace Network_NetWrite_Offsets

#define LootPanel_TypeDefinitionIndex 1076

namespace LootPanel_Offsets {

// Functions
constexpr const static size_t get_Container_00 = 0x418C6C0;
}  // namespace LootPanel_Offsets

#define UIInventory_TypeDefinitionIndex 9408

namespace UIInventory_Offsets {
inline constexpr std::uintptr_t typeinfo      = 0xf0d3c78;
constexpr auto                  static_fields = 0xb8;

// Functions
constexpr const static size_t Close = 0x37D7FD0;
}  // namespace UIInventory_Offsets

#define GrowableEntity_TypeDefinitionIndex 2198

namespace GrowableEntity_Offsets {
inline constexpr std::uintptr_t typeinfo = 0xf206690;

// Offsets
constexpr const static size_t Properties = 0x360;
constexpr const static size_t State      = 0x358;
}  // namespace GrowableEntity_Offsets

#define PlantProperties_TypeDefinitionIndex 9274

namespace PlantProperties_Offsets {

// Offsets
constexpr const static size_t stages = 0x28;
}  // namespace PlantProperties_Offsets

#define PlantProperties_Stage_TypeDefinitionIndex 9276

namespace PlantProperties_Stage_Offsets {

// Offsets
constexpr const static size_t resources = 0xc;
}  // namespace PlantProperties_Stage_Offsets

#define Text_TypeDefinitionIndex 117

namespace Text_Offsets {
inline constexpr std::uintptr_t typeinfo      = 0xf0d0158;
constexpr auto                  static_fields = 0xb8;

// Offsets
constexpr const static size_t m_Text = 0xe0;
}  // namespace Text_Offsets

#define TOD_Sky_TypeDefinitionIndex 1873

namespace TOD_Sky_Offsets {

// Offsets
constexpr const static size_t Cycle      = 0x40;
constexpr const static size_t Atmosphere = 0x50;
constexpr const static size_t Day        = 0x58;
constexpr const static size_t Night      = 0x60;
constexpr const static size_t Stars      = 0x78;
constexpr const static size_t Clouds     = 0x80;
constexpr const static size_t Ambient    = 0x98;

// Functions
constexpr const static size_t get_Instance = 0xA36260;
}  // namespace TOD_Sky_Offsets

// obf name: ::%251288043bd15b0c9881d762a80c6dd24f215b9a
#define TOD_Sky_Static_ClassName \
    "TOD_Sky/%251288043bd15b0c9881d762a80c6dd24f215b9a"
#define TOD_Sky_Static_ClassNameShort \
    "%251288043bd15b0c9881d762a80c6dd24f215b9a"
#define TOD_Sky_Static_TypeDefinitionIndex 1875

namespace TOD_Sky_Static_Offsets {
inline constexpr std::uintptr_t typeinfo      = 0xF1B7C88;
constexpr auto                  static_fields = 0xb8;

// Offsets
constexpr const static size_t instances = 0x40;
}  // namespace TOD_Sky_Static_Offsets

#define TOD_CycleParameters_TypeDefinitionIndex 2347

namespace TOD_CycleParameters_Offsets {

// Functions
constexpr const static size_t get_DateTime = 0xec0a40;
}  // namespace TOD_CycleParameters_Offsets

#define TOD_AtmosphereParameters_TypeDefinitionIndex 1387

namespace TOD_AtmosphereParameters_Offsets {
inline constexpr std::uintptr_t typeinfo = 0xf0e6e90;

// Offsets
constexpr const static size_t RayleighMultiplier = 0x10;
}  // namespace TOD_AtmosphereParameters_Offsets

#define TOD_DayParameters_TypeDefinitionIndex 589

namespace TOD_DayParameters_Offsets {

// Offsets
constexpr const static size_t SkyColor = 0x28;
}  // namespace TOD_DayParameters_Offsets

#define TOD_NightParameters_TypeDefinitionIndex 1639

namespace TOD_NightParameters_Offsets {

// Offsets
constexpr const static size_t MoonColor                   = 0x10;
constexpr const static size_t MoonColorRed                = 0x18;
constexpr const static size_t LightColor                  = 0x20;
constexpr const static size_t RayColor                    = 0x28;
constexpr const static size_t SkyColor                    = 0x30;
constexpr const static size_t CloudColor                  = 0x38;
constexpr const static size_t FogColor                    = 0x40;
constexpr const static size_t AmbientColor                = 0x48;
constexpr const static size_t runtimeLightIntensity       = 0x54;
constexpr const static size_t ShadowStrength              = 0x58;
constexpr const static size_t runtimeAmbientMultiplier    = 0x60;
constexpr const static size_t runtimeReflectionMultiplier = 0x68;
constexpr const static size_t ReflectionMaxClamp          = 0x6c;
constexpr const static size_t AmbientMultiplier           = 0x5c;
}  // namespace TOD_NightParameters_Offsets

#define TOD_StarParameters_TypeDefinitionIndex 1982

namespace TOD_StarParameters_Offsets {

// Offsets
constexpr const static size_t Size       = 0x10;
constexpr const static size_t Brightness = 0x14;
}  // namespace TOD_StarParameters_Offsets

#define TOD_CloudParameters_TypeDefinitionIndex 856

namespace TOD_CloudParameters_Offsets {
inline constexpr std::uintptr_t typeinfo = 0xf0e6e80;

// Offsets
constexpr const static size_t Brightness = 0x30;
}  // namespace TOD_CloudParameters_Offsets

#define TOD_AmbientParameters_TypeDefinitionIndex 725

namespace TOD_AmbientParameters_Offsets {

// Offsets
constexpr const static size_t Mode           = 0x10;
constexpr const static size_t Saturation     = 0x14;
constexpr const static size_t UpdateInterval = 0x18;
}  // namespace TOD_AmbientParameters_Offsets

#define UIHUD_TypeDefinitionIndex 3450

namespace UIHUD_Offsets {
inline constexpr std::uintptr_t typeinfo      = 0xf23c1b8;
constexpr auto                  static_fields = 0xb8;

// Offsets
constexpr const static size_t Hunger = 0x28;
}  // namespace UIHUD_Offsets

#define HudElement_TypeDefinitionIndex 2624

namespace HudElement_Offsets {

// Offsets
constexpr const static size_t lastValue = 0x30;
}  // namespace HudElement_Offsets

#define UIBelt_TypeDefinitionIndex 7599

namespace UIBelt_Offsets {

// Offsets
constexpr const static size_t ItemIcons = 0x20;
}  // namespace UIBelt_Offsets

#define ItemModCompostable_TypeDefinitionIndex 5900

namespace ItemModCompostable_Offsets {

// Offsets
constexpr const static size_t MaxBaitStack = 0x38;
}  // namespace ItemModCompostable_Offsets

// obf name: ::ResourceRef`1
#define GameObjectRef_ClassName           "ResourceRef<UnityEngine/GameObject>"
#define GameObjectRef_ClassNameShort      "ResourceRef`1"
#define GameObjectRef_TypeDefinitionIndex 7583

namespace GameObjectRef_Offsets {
inline constexpr std::uintptr_t typeinfo = 0xf2817e0;

// Offsets
constexpr const static size_t guid = 0x10;
}  // namespace GameObjectRef_Offsets

#define EnvironmentManager_TypeDefinitionIndex 8019

namespace EnvironmentManager_Offsets {

// Functions
}

// obf name: ::Phrase
#define Translate_Phrase_ClassName \
    "%ebbef51ac8259d7c9ed5033a553ff0df2d568187/Phrase"
#define Translate_Phrase_ClassNameShort      "Phrase"
#define Translate_Phrase_TypeDefinitionIndex 2

namespace Translate_Phrase_Offsets {
inline constexpr std::uintptr_t typeinfo = 0xf0c0ed8;

// Offsets (new dump: TranslatePhrase)
constexpr const static size_t token         = 0x10;
constexpr const static size_t legacyEnglish = 0x20;
}  // namespace Translate_Phrase_Offsets

#define ResourceDispenser_GatherPropertyEntry_TypeDefinitionIndex 4851

namespace ResourceDispenser_GatherPropertyEntry_Offsets {

// Offsets
constexpr const static size_t gatherDamage    = 0x10;
constexpr const static size_t destroyFraction = 0x14;
constexpr const static size_t conditionLost   = 0x18;
}  // namespace ResourceDispenser_GatherPropertyEntry_Offsets

#define ResourceDispenser_GatherProperties_TypeDefinitionIndex 4852

namespace ResourceDispenser_GatherProperties_Offsets {

// Offsets
constexpr const static size_t Tree  = 0x10;
constexpr const static size_t Ore   = 0x18;
constexpr const static size_t Flesh = 0x20;
}  // namespace ResourceDispenser_GatherProperties_Offsets

// obf name: ::UIChat
#define UIChat_ClassName           "UIChat"
#define UIChat_ClassNameShort      "UIChat"
#define UIChat_TypeDefinitionIndex 5504

namespace UIChat_Offsets {

// Offsets
constexpr const static size_t chatArea = 0x28;
}  // namespace UIChat_Offsets

// obf name: ::ListComponent`1
#define ListComponent_ClassName           "ListComponent<UIChat>"
#define ListComponent_ClassNameShort      "ListComponent`1"
#define ListComponent_TypeDefinitionIndex 73

namespace ListComponent_Offsets {
inline constexpr std::uintptr_t typeinfo      = 0xF1E6CA0;
constexpr auto                  static_fields = 0xb8;

// Offsets (new dump: ListComponent::InstanceList)
constexpr const static size_t InstanceList = 0x20;
constexpr const static size_t instance     = 0x18;  // OLD BUILD
}  // namespace ListComponent_Offsets

// obf name: ::ListComponent`1
#define ListComponent_Projectile_ClassName           "ListComponent<Projectile>"
#define ListComponent_Projectile_ClassNameShort      "ListComponent`1"
#define ListComponent_Projectile_TypeDefinitionIndex 73

namespace ListComponent_Projectile_Offsets {
// Updated from the 2026-11-05 dump (ListComponentProjectile::TypeInfo).
inline constexpr std::uintptr_t typeinfo      = 0xF1E6CA0;
constexpr auto                  static_fields = 0xb8;

// new dump: ListComponentProjectile::TypeInfo (new-build RVA)
inline constexpr std::uintptr_t TypeInfo = 0xF1E6CA0;

// Offsets
constexpr const static size_t instance = 0x18;
}  // namespace ListComponent_Projectile_Offsets

// obf name: ::%d8b48153d2101b71b885068d1604c451fe65a28d
#define ListHashSet_ClassName \
    "%d8b48153d2101b71b885068d1604c451fe65a28d<UIChat>"
#define ListHashSet_ClassNameShort      "%d8b48153d2101b71b885068d1604c451fe65a28d"
#define ListHashSet_TypeDefinitionIndex 82

namespace ListHashSet_Offsets {

// Offsets
constexpr const static size_t vals = 0x18;
}  // namespace ListHashSet_Offsets

// obf name: ::%d8b48153d2101b71b885068d1604c451fe65a28d
#define ListHashSet_Projectile_ClassName \
    "%d8b48153d2101b71b885068d1604c451fe65a28d<Projectile>"
#define ListHashSet_Projectile_ClassNameShort \
    "%d8b48153d2101b71b885068d1604c451fe65a28d"
#define ListHashSet_Projectile_TypeDefinitionIndex 82

namespace ListHashSet_Projectile_Offsets {

// Offsets
constexpr const static size_t vals = 0x10;
}  // namespace ListHashSet_Projectile_Offsets

#define PatrolHelicopter_TypeDefinitionIndex 2892

namespace PatrolHelicopter_Offsets {
constexpr auto static_fields = 0xb8;

// Offsets
constexpr const static size_t mainRotor = 0x2f8;
constexpr const static size_t weakspots = 0x2e8;
}  // namespace PatrolHelicopter_Offsets

#define Chainsaw_TypeDefinitionIndex 3429

namespace Chainsaw_Offsets {

// Offsets
constexpr const static size_t ammo = 0x474;
}  // namespace Chainsaw_Offsets

// obf name: ::%0fa40172565d558011b842bea40026295f4dd725
#define CameraUpdateHook_Static_ClassName \
    "CameraUpdateHook/%0fa40172565d558011b842bea40026295f4dd725"
#define CameraUpdateHook_Static_ClassNameShort \
    "%0fa40172565d558011b842bea40026295f4dd725"
#define CameraUpdateHook_Static_TypeDefinitionIndex 3873

namespace CameraUpdateHook_Static_Offsets {
inline constexpr std::uintptr_t typeinfo      = 0xF12FD00;
constexpr auto                  static_fields = 0xb8;

// Offsets (new dump: CameraUpdateHookStatic::action)
constexpr const static size_t action = 0x48;
}  // namespace CameraUpdateHook_Static_Offsets

#define SteamClientWrapper_TypeDefinitionIndex 8609

namespace SteamClientWrapper_Offsets {

// Functions
constexpr const static size_t GetAvatarTexture = 0x35CF450;
}  // namespace SteamClientWrapper_Offsets

// obf name: ::%f12a8d8b8fd2984b4a317fc1f6d0b18fe514caeb
#define AimConeUtil_ClassName           "%f12a8d8b8fd2984b4a317fc1f6d0b18fe514caeb"
#define AimConeUtil_ClassNameShort      "%f12a8d8b8fd2984b4a317fc1f6d0b18fe514caeb"
#define AimConeUtil_TypeDefinitionIndex 7868

namespace AimConeUtil_Offsets {

// Functions
constexpr const static size_t GetModifiedAimConeDirection = 0x5F7A7C0;
}  // namespace AimConeUtil_Offsets

#define PlayerModel_TypeDefinitionIndex 6533

namespace PlayerModel_Offsets {

// Offsets (new dump: PlayerModel)
constexpr const static size_t _multiMesh = 0x550;
constexpr const static size_t visible    = 0xCC;

// Offsets (OLD BUILD: not present in the new dump)
constexpr const static size_t position   = 0x4C8;
constexpr const static size_t viewMatrix = 0x4D4;
}  // namespace PlayerModel_Offsets

#define SkinnedMultiMesh_TypeDefinitionIndex 5509

namespace SkinnedMultiMesh_Offsets {

// Offsets
constexpr const static size_t Renderers = 0x38;
}  // namespace SkinnedMultiMesh_Offsets

#define BaseMountable_TypeDefinitionIndex 7335

namespace BaseMountable_Offsets {
inline constexpr std::uintptr_t typeinfo = 0xf1c6b98;

// Offsets
constexpr const static size_t pitchClamp    = 0x30c;
constexpr const static size_t yawClamp      = 0x314;
constexpr const static size_t canWieldItems = 0x31c;
}  // namespace BaseMountable_Offsets

#define ProgressBar_TypeDefinitionIndex 1485

namespace ProgressBar_Offsets {
inline constexpr std::uintptr_t typeinfo      = 0xf1c3318;
constexpr auto                  static_fields = 0xb8;

// Offsets (new dump: ProgressBar)
constexpr const static size_t Instance      = 0x8;
constexpr const static size_t timeFinished  = 0x20;
constexpr const static size_t timeCounter   = 0x24;
constexpr const static size_t leftField     = 0x40;

// Offsets (OLD BUILD: not present in the new dump)
constexpr const static size_t scaleTarget   = 0x28;
constexpr const static size_t progressField = 0x30;
constexpr const static size_t iconField     = 0x38;
constexpr const static size_t rightField    = 0x48;
constexpr const static size_t clipOpen      = 0x50;
constexpr const static size_t clipCancel    = 0x58;
constexpr const static size_t canvas        = 0x70;
constexpr const static size_t canvasGroup   = 0x78;

// Functions (OLD BUILD)
constexpr const static size_t Update            = 0x11b1ab0;
constexpr const static size_t Start             = 0x11b2c60;
constexpr const static size_t Close             = 0x0;
constexpr const static size_t UpdateProgressBar = 0x0;
constexpr const static size_t SetPercent        = 0x0;
constexpr const static size_t PlayOpenSound     = 0x0;
constexpr const static size_t PlayCancelSound   = 0x0;
}  // namespace ProgressBar_Offsets

#define BowWeapon_TypeDefinitionIndex 1613

namespace BowWeapon_Offsets {

// Offsets (new dump: BowWeapon)
constexpr const static size_t attackReady = 0x4F0;
constexpr const static size_t arrowBack   = 0x4F4;
constexpr const static size_t wasAiming   = 0x4F8;
}  // namespace BowWeapon_Offsets

#define CrossbowWeapon_TypeDefinitionIndex 4777

namespace CrossbowWeapon_Offsets {

// Offsets
}

#define MiniCrossbow_TypeDefinitionIndex 943

namespace MiniCrossbow_Offsets {

// Offsets
}

// obf name: ::%eeb77896dba237ec8662a7f52d5e04215ccf8ed2
#define ConVar_Player_Static_ClassName           \
    "%1cc1a3da6f6b68960353cdd6b77eddcd8aafd472/" \
    "%eeb77896dba237ec8662a7f52d5e04215ccf8ed2"
#define ConVar_Player_Static_ClassNameShort \
    "%eeb77896dba237ec8662a7f52d5e04215ccf8ed2"
#define ConVar_Player_Static_TypeDefinitionIndex 7502

namespace ConVar_Player_Static_Offsets {
inline constexpr std::uintptr_t typeinfo      = 0xF1F8CC0;
constexpr auto                  static_fields = 0xb8;

// Offsets
constexpr const static size_t clientTickInterval = 0x34;

// Functions
constexpr const static size_t clientTickRate_getter = 0x52fb4a0;
constexpr const static size_t clientTickRate_setter = 0x53514a0;
}  // namespace ConVar_Player_Static_Offsets

#define ColliderInfo_TypeDefinitionIndex 6828

namespace ColliderInfo_Offsets {

// Offsets
constexpr const static size_t flags = 0x20;
}  // namespace ColliderInfo_Offsets

#define CodeLock_TypeDefinitionIndex 1539

namespace CodeLock_Offsets {

// Offsets
constexpr const static size_t hasCode      = 0x270;
constexpr const static size_t HasAuth      = 0x280;
constexpr const static size_t HasGuestAuth = 0x281;
}  // namespace CodeLock_Offsets

#define AutoTurret_TypeDefinitionIndex 5523

namespace AutoTurret_Offsets {

// Offsets
constexpr const static size_t authorizedPlayers = 0x3C8;
constexpr const static size_t lastYaw           = 0x440;
constexpr const static size_t muzzlePos         = 0x4e0;
constexpr const static size_t gun_yaw           = 0x500;
constexpr const static size_t gun_pitch         = 0x508;
constexpr const static size_t sightRange        = 0x510;
}  // namespace AutoTurret_Offsets

#define Client_TypeDefinitionIndex 2915

namespace Client_Offsets {

// Functions
constexpr const static size_t OnClientDisconnected           = 0x0;
constexpr const static size_t OnClientDisconnected_vtableoff = 0x0;
}  // namespace Client_Offsets

// obf name: ::%c63f6c0bbf62a3ec16378565e291b09112de1025
#define ItemManager_Static_ClassName             \
    "%ea702d6ec468522fe62362e8622adf637efeaab0/" \
    "%c63f6c0bbf62a3ec16378565e291b09112de1025"
#define ItemManager_Static_ClassNameShort \
    "%c63f6c0bbf62a3ec16378565e291b09112de1025"
#define ItemManager_Static_TypeDefinitionIndex 345

namespace ItemManager_Static_Offsets {
inline constexpr std::uintptr_t typeinfo      = 0xF179460;
constexpr auto                  static_fields = 0xb8;

// Offsets
constexpr const static size_t itemList             = 0x90;
constexpr const static size_t itemDictionary       = 0x308;
constexpr const static size_t itemDictionaryByName = 0x40;
}  // namespace ItemManager_Static_Offsets

// obf name: ::%29d8ec02e953ec416c8880014e1b52ba2bfce5dd
#define ConVar_Server_Static_ClassName \
    "%29d8ec02e953ec416c8880014e1b52ba2bfce5dd"
#define ConVar_Server_Static_ClassNameShort \
    "%29d8ec02e953ec416c8880014e1b52ba2bfce5dd"
#define ConVar_Server_Static_TypeDefinitionIndex 3918

namespace ConVar_Server_Static_Offsets {

// Offsets
}

#define UI_LoadingScreen_TypeDefinitionIndex 9671

namespace UI_LoadingScreen_Offsets {

// Offsets
constexpr const static size_t panel = 0x30;
}  // namespace UI_LoadingScreen_Offsets

#define MixerSnapshotManager_TypeDefinitionIndex 5728

namespace MixerSnapshotManager_Offsets {

// Offsets
constexpr const static size_t defaultSnapshot = 0x20;
constexpr const static size_t loadingSnapshot = 0x30;
}  // namespace MixerSnapshotManager_Offsets

#define MapView_Static_ClassName \
    "MapView/%bfff6d9a849afa29a7d9c7ce3d927905a93abe30"
#define MapView_Static_ClassNameShort \
    "%bfff6d9a849afa29a7d9c7ce3d927905a93abe30"
#define MapView_TypeDefinitionIndex 3234

namespace MapView_Offsets {

// Functions
constexpr const static size_t WorldPosToImagePos = 0x0;
}  // namespace MapView_Offsets

// obf name: ::GamePhysics
#define GamePhysics_ClassName           "GamePhysics"
#define GamePhysics_ClassNameShort      "GamePhysics"
#define GamePhysics_TypeDefinitionIndex 1958

namespace GamePhysics_Offsets {

// Functions
constexpr const static size_t Trace               = 0x3692DE0;
constexpr const static size_t LineOfSightInternal = 0x3662780;
constexpr const static size_t Verify              = 0x365E4B0;
}  // namespace GamePhysics_Offsets

#define InstancedDebugDraw_TypeDefinitionIndex 7529

namespace InstancedDebugDraw_Offsets {

// Functions
constexpr const static size_t AddInstance = 0x5669520;
}  // namespace InstancedDebugDraw_Offsets

#define ThrownWeapon_TypeDefinitionIndex 8114

namespace ThrownWeapon_Offsets {

// Offsets
constexpr const static size_t maxThrowVelocity = 0x3f8;
}  // namespace ThrownWeapon_Offsets

#define MapInterface_TypeDefinitionIndex 2797

namespace MapInterface_Offsets {
inline constexpr std::uintptr_t typeinfo      = 0xf0356d8;
constexpr auto                  static_fields = 0xb8;

// Offsets
constexpr const static size_t scrollRectZoom = 0x30;
}  // namespace MapInterface_Offsets

#define ScrollRectZoom_TypeDefinitionIndex 8849

namespace ScrollRectZoom_Offsets {

// Offsets
constexpr const static size_t zoom = 0x28;
}  // namespace ScrollRectZoom_Offsets

#define MapView_TypeDefinitionIndex 3234

namespace MapView_Offsets {

// Offsets
constexpr const static size_t scrollRect = 0x40;
}  // namespace MapView_Offsets

#define StorageContainer_TypeDefinitionIndex 5467

namespace StorageContainer_Offsets {
inline constexpr std::uintptr_t typeinfo = 0xf14fc10;

// Offsets
constexpr const static size_t inventorySlots = 0x338;
}  // namespace StorageContainer_Offsets

#define PlayerCorpse_TypeDefinitionIndex 7108

namespace PlayerCorpse_Offsets {

// Offsets (new dump: PlayerCorpse)
constexpr const static size_t underwearSkin = 0x360;

// Offsets (OLD BUILD: not present in the new dump)
constexpr const static size_t clientClothing = 0x370;
}  // namespace PlayerCorpse_Offsets

#define TimedExplosive_TypeDefinitionIndex 2880

namespace TimedExplosive_Offsets {

// Offsets
constexpr const static size_t explosionRadius = 0x224;
}  // namespace TimedExplosive_Offsets

#define SmokeGrenade_TypeDefinitionIndex 238

namespace SmokeGrenade_Offsets {
inline constexpr std::uintptr_t typeinfo = 0xf1a3680;

// Offsets
constexpr const static size_t smokeEffectInstance = 0x2b0;
}  // namespace SmokeGrenade_Offsets

#define GrenadeWeapon_TypeDefinitionIndex 9182

namespace GrenadeWeapon_Offsets {

// Offsets
constexpr const static size_t drop = 0x3ac;
}  // namespace GrenadeWeapon_Offsets

#define ViewmodelLower_TypeDefinitionIndex 5556

namespace ViewmodelLower_Offsets {

// Offsets (new dump: ViewmodelLower)
constexpr const static size_t lowerOnSprint       = 0x20;
constexpr const static size_t lowerWhenCantAttack = 0x21;
constexpr const static size_t forceLower          = 0x22;
constexpr const static size_t lowerScale          = 0x24;
constexpr const static size_t shouldLower         = 0x28;

// Offsets (OLD BUILD: not present in the new dump)
constexpr const static size_t rotateAngle         = 0x2c;
}  // namespace ViewmodelLower_Offsets

#define SamSite_TypeDefinitionIndex 5520

namespace SamSite_Offsets {

// Offsets
constexpr const static size_t staticRespawn   = 0x440;
constexpr const static size_t Flag_TargetMode = 0x47c;
}  // namespace SamSite_Offsets

#define ServerProjectile_TypeDefinitionIndex 5832

namespace ServerProjectile_Offsets {

// Offsets
constexpr const static size_t drag            = 0x34;
constexpr const static size_t gravityModifier = 0x38;
constexpr const static size_t speed           = 0x3c;
constexpr const static size_t radius          = 0x5c;
}  // namespace ServerProjectile_Offsets

#define UIFogOverlay_TypeDefinitionIndex 6690

namespace UIFogOverlay_Offsets {
inline constexpr std::uintptr_t typeinfo      = 0xf1a3438;
constexpr auto                  static_fields = 0xb8;

// Offsets
constexpr const static size_t group    = 0x20;
constexpr const static size_t Instance = 0x10;
}  // namespace UIFogOverlay_Offsets

#define FoliageGrid_TypeDefinitionIndex 3062

namespace FoliageGrid_Offsets {
inline constexpr std::uintptr_t typeinfo      = 0xf0e8330;
constexpr auto                  static_fields = 0xb8;

// Offsets
constexpr const static size_t CellSize = 0x28;
}  // namespace FoliageGrid_Offsets

#define ItemModWearable_TypeDefinitionIndex 2853

namespace ItemModWearable_Offsets {

// Offsets
constexpr const static size_t movementProperties = 0x50;
}  // namespace ItemModWearable_Offsets

#define ClothingMovementProperties_TypeDefinitionIndex 3639

namespace ClothingMovementProperties_Offsets {

// Offsets
constexpr const static size_t speedReduction = 0x18;
}  // namespace ClothingMovementProperties_Offsets

#define GestureConfig_TypeDefinitionIndex 6441

namespace GestureConfig_Offsets {

// Offsets
constexpr const static size_t actionType = 0x90;
}  // namespace GestureConfig_Offsets

#define RCMenu_TypeDefinitionIndex 669

namespace RCMenu_Offsets {
inline constexpr std::uintptr_t typeinfo      = 0xf051618;
constexpr auto                  static_fields = 0xb8;

// Offsets
constexpr const static size_t autoTurretFogDistance = 0x13c;
}  // namespace RCMenu_Offsets

// obf name: ::%1252503058087ca06a0d107650a3ea5321d64f38
#define Facepunch_Network_Raknet_Client_ClassName \
    "%1252503058087ca06a0d107650a3ea5321d64f38"
#define Facepunch_Network_Raknet_Client_ClassNameShort \
    "%1252503058087ca06a0d107650a3ea5321d64f38"
#define Facepunch_Network_Raknet_Client_TypeDefinitionIndex 1

namespace Facepunch_Network_Raknet_Client_Offsets {

// Functions
constexpr const static size_t IsConnected           = 0x6A76740;
constexpr const static size_t IsConnected_vtableoff = 0x4D8;
}  // namespace Facepunch_Network_Raknet_Client_Offsets

// obf name: ::%e9133b8cf245d7788fa60fcf1beffea8168cd52a
#define EncryptedValue_ClassName \
    "%e9133b8cf245d7788fa60fcf1beffea8168cd52a<System/UInt64>"
#define EncryptedValue_ClassNameShort \
    "%e9133b8cf245d7788fa60fcf1beffea8168cd52a"
#define EncryptedValue_TypeDefinitionIndex 8053

namespace EncryptedValue_Offsets {
inline constexpr std::uintptr_t typeinfo = 0x10dad780;

// Offsets
constexpr const static size_t _value   = 0x0;
constexpr const static size_t _padding = 0x18;
}  // namespace EncryptedValue_Offsets

// obf name: ::%b4ba15533c6ff05f0f46fed042b4d34e5d261fbe
#define HiddenValue_ClassName                                    \
    "%b4ba15533c6ff05f0f46fed042b4d34e5d261fbe<BaseNetworkable/" \
    "%15247d48b314fa7c7e2265a68c0ffb7ae9cbde22>"
#define HiddenValue_ClassNameShort      "%b4ba15533c6ff05f0f46fed042b4d34e5d261fbe"
#define HiddenValue_TypeDefinitionIndex 6774

namespace HiddenValue_Offsets {
inline constexpr std::uintptr_t typeinfo = 0xF1BDF88;

// Offsets
constexpr const static size_t _handle      = 0x18;
constexpr const static size_t _accessCount = 0x20;
constexpr const static size_t _hasValue    = 0x10;
}  // namespace HiddenValue_Offsets

#define ItemModRFListener_TypeDefinitionIndex 6660

namespace ItemModRFListener_Offsets {

// Functions
constexpr const static size_t ConfigureClicked = 0x0;
}  // namespace ItemModRFListener_Offsets

// obf name: ::%986f54ae114680bd6912d22cfffd45415ad56546
#define BufferStream_ClassName           "%986f54ae114680bd6912d22cfffd45415ad56546"
#define BufferStream_ClassNameShort      "%986f54ae114680bd6912d22cfffd45415ad56546"
#define BufferStream_TypeDefinitionIndex 995

namespace BufferStream_Offsets {

// Offsets
constexpr const static size_t _buffer = 0x28;

// Functions
constexpr const static size_t EnsureCapacity = 0xA396070;
}  // namespace BufferStream_Offsets

#define FreeableLootContainer_TypeDefinitionIndex 7859

namespace FreeableLootContainer_Offsets {

// Offsets
}

#define BlowPipeWeapon_TypeDefinitionIndex 8876

namespace BlowPipeWeapon_Offsets {

// Offsets
}

#define AttackHelicopterRockets_TypeDefinitionIndex 3756

namespace AttackHelicopterRockets_Offsets {

// Functions
constexpr const static size_t GetProjectedHitPos = 0x2a2a0b0;
}  // namespace AttackHelicopterRockets_Offsets

#define OutlineManager_TypeDefinitionIndex 8711

namespace OutlineManager_Offsets {

// Offsets
}

// obf name: ::%0ad64aee33cf0b2a97f4ca73c8da06a1d7d5b573
#define ConsoleSystem_Command_ClassName          \
    "%3b1f7f18b8ef19b17dafed2db0070144a25a3536/" \
    "%0ad64aee33cf0b2a97f4ca73c8da06a1d7d5b573"
#define ConsoleSystem_Command_ClassNameShort \
    "%0ad64aee33cf0b2a97f4ca73c8da06a1d7d5b573"
#define ConsoleSystem_Command_TypeDefinitionIndex 7

namespace ConsoleSystem_Command_Offsets {
inline constexpr std::uintptr_t typeinfo = 0xF18DEC8;

// Offsets
constexpr const static size_t GetOveride = 0x58;
constexpr const static size_t SetOveride = 0x20;
constexpr const static size_t Call       = 0x68;
}  // namespace ConsoleSystem_Command_Offsets

// obf name: ::%87e3ef2353e2600487ffcd28003021ff79f4deae
#define ConsoleSystem_Arg_ClassName              \
    "%3b1f7f18b8ef19b17dafed2db0070144a25a3536/" \
    "%87e3ef2353e2600487ffcd28003021ff79f4deae"
#define ConsoleSystem_Arg_ClassNameShort \
    "%87e3ef2353e2600487ffcd28003021ff79f4deae"
#define ConsoleSystem_Arg_TypeDefinitionIndex 3

namespace ConsoleSystem_Arg_Offsets {
inline constexpr std::uintptr_t typeinfo = 0xF195618;

// Offsets
constexpr const static size_t Option = 0x40;
}  // namespace ConsoleSystem_Arg_Offsets

// obf name: ::%b4a4ad09820c0838e1006242ff395b2f92ed5dcb
#define ConsoleSystem_Index_Client_ClassName      \
    "%3b1f7f18b8ef19b17dafed2db0070144a25a3536/"  \
    "%659d0ebd747d1d180d340f9b604cd2885f2dc741.%" \
    "b4a4ad09820c0838e1006242ff395b2f92ed5dcb"
#define ConsoleSystem_Index_Client_ClassNameShort \
    "%b4a4ad09820c0838e1006242ff395b2f92ed5dcb"
#define ConsoleSystem_Index_Client_TypeDefinitionIndex 11

namespace ConsoleSystem_Index_Client_Offsets {

// Functions
constexpr const static size_t Find = 0x6944D20;
}  // namespace ConsoleSystem_Index_Client_Offsets

#define String_TypeDefinitionIndex 142

namespace String_Offsets {
inline constexpr std::uintptr_t typeinfo      = 0xf6a0bc0;
constexpr auto                  static_fields = 0xb8;

// Offsets
constexpr const static size_t FastAllocateString = 0x738D80;
}  // namespace String_Offsets

// obf name: ::%425eb0f2ef259d12583149649e74038ce49e0fde
#define EntityRef_ClassName           "%425eb0f2ef259d12583149649e74038ce49e0fde"
#define EntityRef_ClassNameShort      "%425eb0f2ef259d12583149649e74038ce49e0fde"
#define EntityRef_TypeDefinitionIndex 75

namespace EntityRef_Offsets {

// Offsets
constexpr const static size_t Get = 0x13ACA70;
}  // namespace EntityRef_Offsets

// obf name: ConVar::Debugging
#define ConVar_Debugging_ClassName           "ConVar/Debugging"
#define ConVar_Debugging_ClassNameShort      "Debugging"
#define ConVar_Debugging_TypeDefinitionIndex 1901

namespace ConVar_Debugging_Offsets {

// Functions
}

#define CursorManager_TypeDefinitionIndex 4925

namespace CursorManager_Offsets {
inline constexpr std::uintptr_t typeinfo      = 0xf111d88;
constexpr auto                  static_fields = 0xb8;


} 

// ============================================================================
// Namespaces added from the new dump (alloffsetsdecs.txt) that had no
// counterpart in the previous dump.
// ============================================================================

// new dump: ## Rvas
namespace Rvas {
inline constexpr std::uintptr_t TypeManagerInstance = 0x2284890;
inline constexpr std::uintptr_t TypeToObjectSet     = 0x2284870;
inline constexpr std::uintptr_t m_Instance          = 0x21E89C8;
inline constexpr std::uintptr_t rigidActors         = 0x23D8;
inline constexpr std::uintptr_t assetBundleManager = 0x2196CC0;
inline constexpr std::uintptr_t chatOpen = 0x3CF1550;
inline constexpr std::uintptr_t physicsManager = 0x2215A58;
inline constexpr std::uintptr_t physicsSceneArray = 0x8;
inline constexpr std::uintptr_t queriesHitTriggers = 0xD3;
inline constexpr std::uintptr_t sizeOfImage = 0x247E000;
inline constexpr std::uintptr_t timeDateStamp = 0x6AD8CDD4;
}  // namespace Rvas

// new dump: ## SingletonComponentCameraMan
namespace SingletonComponentCameraMan_Offsets {
inline constexpr std::uintptr_t TypeInfo          = 0xF108798;
constexpr const static size_t   Instance          = 0x28;
constexpr auto                  static_fields     = 0xb8;
}  // namespace SingletonComponentCameraMan_Offsets

// new dump: ## SingletonComponentLoadingScreen
namespace SingletonComponentLoadingScreen_Offsets {
inline constexpr std::uintptr_t TypeInfo          = 0xF10F078;
constexpr const static size_t   Instance          = 0x28;
constexpr auto                  static_fields     = 0xb8;
}  // namespace SingletonComponentLoadingScreen_Offsets

// new dump: ## SingletonComponentMixerSnapshotManager
namespace SingletonComponentMixerSnapshotManager_Offsets {
inline constexpr std::uintptr_t TypeInfo          = 0xF115898;
constexpr const static size_t   Instance          = 0x28;
constexpr auto                  static_fields     = 0xb8;
}  // namespace SingletonComponentMixerSnapshotManager_Offsets

// new dump: ## ConvarClientStatic
namespace ConvarClientStatic_Offsets {
constexpr const static size_t camlerp  = 0x7F8;
constexpr const static size_t camspeed = 0x6D4;
}  // namespace ConvarClientStatic_Offsets

// new dump: ## ConvarPlayerStatic
namespace ConvarPlayerStatic_Offsets {
constexpr const static size_t clientTickInterval = 0x64;
}  // namespace ConvarPlayerStatic_Offsets

// new dump: ## LoadingScreen
namespace LoadingScreen_Offsets {
constexpr const static size_t panel = 0x30;
}  // namespace LoadingScreen_Offsets

// new dump: ## HumanNPC
namespace HumanNPC_Offsets {
constexpr const static size_t AdditionalLosBlockingLayer = 0x880;
constexpr const static size_t aimConeScale               = 0x890;
constexpr const static size_t lastDismountTime           = 0x894;
}  // namespace HumanNPC_Offsets

// new dump: ## NPCPlayer
namespace NPCPlayer_Offsets {
constexpr const static size_t LegacyNavigation               = 0x85C;
constexpr const static size_t MovementTickStartDelay         = 0x820;
constexpr const static size_t attackLengthMaxShortRangeScale = 0x870;
constexpr const static size_t damageScale                    = 0x868;
constexpr const static size_t finalDestination               = 0x830;
constexpr const static size_t shortRange                     = 0x86C;
}  // namespace NPCPlayer_Offsets

// new dump: ## ItemIconStatic
namespace ItemIconStatic_Offsets {
constexpr const static size_t containerLootStartTimes = 0x50;
}  // namespace ItemIconStatic_Offsets

// new dump: ## ServerAdminUGCEntry
namespace ServerAdminUGCEntry_Offsets {
constexpr const static size_t ReceivedDataFromServer = 0x60CBEB0;
}  // namespace ServerAdminUGCEntry_Offsets

// new dump: ## TriggerLadder
namespace TriggerLadder_Offsets {
constexpr const static size_t Type               = 0x50;
constexpr const static size_t ForceLookAt        = 0x54;
constexpr const static size_t RequireJumpToMount = 0x55;
}  // namespace TriggerLadder_Offsets

// new dump: ## TriggerMovement
namespace TriggerMovement_Offsets {
constexpr const static size_t losEyes        = 0x50;
constexpr const static size_t movementModify = 0x58;
constexpr const static size_t scale          = 0x5c;
}  // namespace TriggerMovement_Offsets

// new dump: ## UnityComponent
namespace UnityComponent_Offsets {
constexpr const static size_t GetGameObject = 0xBEFC0;
}  // namespace UnityComponent_Offsets

// new dump: ## UnityEngineUiText
namespace UnityEngineUiText_Offsets {
constexpr const static size_t m_Text = 0xE0;
}  // namespace UnityEngineUiText_Offsets

// new dump: ## UnityGameObject
namespace UnityGameObject_Offsets {
constexpr const static size_t m_IsActive = 0x46;
}  // namespace UnityGameObject_Offsets

// new dump: ## ViewmodelBob
namespace ViewmodelBob_Offsets {
constexpr const static size_t bobSpeedWalk  = 0x20;
constexpr const static size_t bobSpeedRun   = 0x24;
constexpr const static size_t bobAmountWalk = 0x28;
constexpr const static size_t bobAmountRun  = 0x2c;
constexpr const static size_t leftOffsetRun = 0x30;
}  // namespace ViewmodelBob_Offsets

// new dump: ## ViewmodelPunch
namespace ViewmodelPunch_Offsets {
constexpr const static size_t punchDuration  = 0x34;
constexpr const static size_t punchMagnitude = 0x38;
constexpr const static size_t punchStartTime = 0x3c;
}  // namespace ViewmodelPunch_Offsets

// new dump: ## ViewmodelSway
namespace ViewmodelSway_Offsets {
constexpr const static size_t positionalSwaySpeed  = 0x20;
constexpr const static size_t positionalSwayAmount = 0x24;
constexpr const static size_t rotationSwaySpeed    = 0x28;
constexpr const static size_t rotationSwayAmount   = 0x2c;
constexpr const static size_t rotateAmountTest     = 0x30;
}  // namespace ViewmodelSway_Offsets

// new dump: ## TodSky / TodSkyStatic (environment reference values; the full
// Tod*Parameters tables are in alloffsetsdecs.txt "## Tod*" sections)
namespace TodSky_Offsets {
constexpr const static size_t Atmosphere = 0x50;
constexpr const static size_t Day        = 0x58;
constexpr const static size_t Night      = 0x60;
constexpr const static size_t Sun        = 0x68;
constexpr const static size_t Moon       = 0x70;
constexpr const static size_t Stars      = 0x78;
constexpr const static size_t Clouds     = 0x80;
constexpr const static size_t Light      = 0x88;
constexpr const static size_t Fog        = 0x90;
constexpr const static size_t Ambient    = 0x98;
}  // namespace TodSky_Offsets

namespace TodSkyStatic_Offsets {
constexpr const static size_t instances = 0x68;
}  // namespace TodSkyStatic_Offsets

// ============================================================================
// SDK exploit support (full-rust-sdk-main port, offsets remapped to this dump)
// ============================================================================

// ListComponent<Projectile>: the static holder of every live Projectile
// component. The dump header carries its TypeInfo (build 26140533); it is read
// at runtime as *(g_gameBase + typeinfo) and validated by class name before
// the InstanceList is walked (the same rule as every other static class).
namespace ListComponentProjectile_Offsets {
inline constexpr std::uintptr_t typeinfo      = 0xF3A1FD0;
constexpr auto                  static_fields = 0xb8;
// ListComponent<T>::InstanceList (List<T*>): items 0x10 / size 0x18 / elems +0x20
constexpr const static size_t InstanceList    = 0x30;
}  // namespace ListComponentProjectile_Offsets

// Projectile fields the SDK exploits used, on the new dump's layout:
//   SDK 0x3C thickness      -> Projectile::thickness 0x3C   (unchanged)
//   SDK 0x44 initialVelocity-> NOT in the new dump; instant bullet uses
//                              Projectile::currentVelocity 0x164 instead
//   SDK 0xF0 swimScale      -> not in the new dump (dropped from spread)
// Projectile_Offsets above already carries drag 0x34, gravityModifier 0x38,
// thickness 0x3C, currentVelocity 0x164 and currentPosition 0x170.
