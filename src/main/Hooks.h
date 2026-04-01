#pragma once

class Hooks {
public:
  Hooks() = delete;

  static void Install();

private:
  static void InitWornArmor(RE::TESObjectARMO *a_armor, RE::Actor *a_actor,
                            RE::BSTSmartPointer<RE::BipedAnim> *a_biped);

  static auto GetWornMask(RE::InventoryChanges *a_inventoryChanges)
      -> BipedObjectSlot;

  static bool FixEquipConflictCheck(std::uintptr_t a_itemAddr,
                                    std::uint32_t a_bodySlot,
                                    RE::Actor *a_actor,
                                    std::uintptr_t a_originalComponentAddr);
};
