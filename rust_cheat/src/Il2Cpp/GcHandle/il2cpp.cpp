
// ================================================================================
// Release channel: t.me/ImGui_Source_FIXNEY (ImGui Source)
// Second channel: t.me/il2cpp_src (shit src 🧨)
// Telegram bot: t.me/imguisource_bot
// ================================================================================

#include "il2cpp.h"
#include <cstdint>
#include "../../src/Driver/Rpm.h"
/*
 Layout of the slab page (Reversed from gameassembly.dll the day game went from
 unity 5 to 6): page_base = handle & ~0x1FFF page_base + 0x10 : pointer to
 allocation bitmap (uint32_t[] per 32 slots) page_base + 0x1C : slot count
 (uint32_t) page_base + 0x20 : type byte (0/1 = weak-ish, >1 = normal; >=4 =
 invalid) page_base + 0x28 : start of slot data; slots are uint64_t each
 slot_index = (handle - page_base - 0x28) >> 3
 object_ptr = read64(page_base + 8 * (slot_index + 5))   // +5 = header qwords
 For type <= 1 the stored value is bitwise-inverted.
*/

namespace Il2CppHandle {
namespace Il2CppHandleOffsets {

constexpr uintptr_t A_PageMask     = 0xFFFFFFFFFFFFE000ULL;  // ~0x1FFF
constexpr uintptr_t A_BitmapOff    = 0x10;
constexpr uintptr_t A_SlotCountOff = 0x1C;
constexpr uintptr_t A_TypeOff      = 0x20;
constexpr uintptr_t A_SlotsBaseOff = 0x28;
constexpr uintptr_t A_HeaderQwords = 5;  // slot data starts 5 qwords past page_base
constexpr uint32_t  A_MaxType      = 4;

// Bitmap is uint32_t[], one bit per slot
constexpr uint32_t A_SlotsPerWord  = 32;
constexpr uint32_t A_SlotWordShift = 5;  // log2(A_SlotsPerWord)
constexpr uint32_t A_SlotBitMask   = A_SlotsPerWord - 1;

// Type 0/1 stores the pointer bitwise-inverted
constexpr uint8_t A_InvertedTypeMax = 1;

inline bool IsSlotAllocated(uintptr_t page, uint32_t slot) {
    const uint64_t bitmap = Driver->Read<uint64_t>(page + A_BitmapOff);
    if (!bitmap)
        return false;

    const uint32_t word = Driver->Read<uint32_t>(
        bitmap + sizeof(uint32_t) * (slot >> A_SlotWordShift));
    return (word >> (slot & A_SlotBitMask)) & 1u;
}

}  // namespace Il2CppHandleOffsets

uintptr_t Il2cppGetHandle(
    uint64_t ObjectHandleID,
    uintptr_t /*gameAssemblyBase*/) {  // too lazy to fix all the handle calls
                                       // through out the base

    if (!ObjectHandleID)
        return 0;

    const uintptr_t page =
        ObjectHandleID & Il2CppHandle::Il2CppHandleOffsets::A_PageMask;

    const uint8_t type = Driver->Read<uint8_t>(
        page + Il2CppHandle::Il2CppHandleOffsets::A_TypeOff);
    if (type >= Il2CppHandle::Il2CppHandleOffsets::A_MaxType)
        return 0;

    const uint32_t slot = static_cast<uint32_t>(
        static_cast<int64_t>(ObjectHandleID - page -
                             Il2CppHandle::Il2CppHandleOffsets::A_SlotsBaseOff) >> 3);

    if (slot >= Driver->Read<uint32_t>(page + Il2CppHandle::Il2CppHandleOffsets::A_SlotCountOff))
        return 0;

    if (!Il2CppHandle::Il2CppHandleOffsets::IsSlotAllocated(page, slot))
        return 0;

    const uint64_t entry = Driver->Read<uint64_t>(
        page + sizeof(uint64_t) *
                   (slot + Il2CppHandle::Il2CppHandleOffsets::A_HeaderQwords));

    // weak handles keep the pointer inverted; low 2 bits are flags
    const uint64_t resolved =
        (type <= Il2CppHandle::Il2CppHandleOffsets::A_InvertedTypeMax)
            ? ~entry
            : entry;
    return static_cast<uintptr_t>(resolved & ~3ull);
}

}  // namespace Il2CppHandle