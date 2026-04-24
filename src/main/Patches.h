#pragma once

namespace Patches {
using InitWornArmorFunc = void(RE::TESObjectARMO *a_armor, RE::Actor *a_actor,
                               RE::BSTSmartPointer<RE::BipedAnim> *a_biped);

void WriteInitWornPatch(InitWornArmorFunc *a_func);

using GetWornMaskFunc = auto(RE::InventoryChanges *a_inventoryChanges)
    -> BipedObjectSlot;

void WriteGetWornMaskPatch(GetWornMaskFunc *a_func);

using TestBodyPartByIndexFunc = bool(RE::BGSBipedObjectForm *a_form,
                                     std::uint32_t a_bodyPart);

void WriteTestBodyPartByIndexPatch(TestBodyPartByIndexFunc *a_func);

using FixEquipConflictCheckFunc = bool(std::uintptr_t a_itemAddr,
                                       std::uint32_t a_bodySlot,
                                       RE::Actor *a_actor);

void WriteFixEquipConflictPatch(FixEquipConflictCheckFunc *a_func);

struct SkillLevelingVisitor {
  RE::TESObjectARMO **shield;
  RE::TESObjectARMO **torso;
  std::uint32_t light;
  std::uint32_t heavy;
};
static_assert(sizeof(SkillLevelingVisitor) == 0x18);
static_assert(offsetof(SkillLevelingVisitor, light) == 0x10);
static_assert(offsetof(SkillLevelingVisitor, heavy) == 0x14);

using FixSkillLevelingFunc = bool(RE::BipedAnim *a_biped,
                                  SkillLevelingVisitor *a_visitor);

auto WriteFixSkillLevelingPatch(FixSkillLevelingFunc *a_func) -> bool;
} // namespace Patches
