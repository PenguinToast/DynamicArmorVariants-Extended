#include "Hooks.h"
#include "DynamicArmorManager.h"
#include "Ext/InventoryChanges.h"
#include "Ext/TESObjectARMA.h"
#include "GetWornMaskVisitor.h"
#include "Patches.h"

#include <unordered_set>
#include <vector>

namespace {
using BipedObjectSlot = RE::BGSBipedObjectForm::BipedObjectSlot;

auto BodySlotToMask(std::uint32_t a_bodySlot) -> BipedObjectSlot {
  if (a_bodySlot >= 32) {
    return BipedObjectSlot::kNone;
  }

  return static_cast<BipedObjectSlot>(1u << a_bodySlot);
}

auto GetWornExtraData(RE::InventoryEntryData *a_entryData)
    -> RE::ExtraDataList * {
  if (!a_entryData || !a_entryData->extraLists) {
    return nullptr;
  }

  for (auto *extraData : *a_entryData->extraLists) {
    if (!extraData) {
      continue;
    }

    if (extraData->HasType<RE::ExtraWorn>() ||
        extraData->HasType<RE::ExtraWornLeft>()) {
      return extraData;
    }
  }

  return nullptr;
}

struct PendingUnequip {
  RE::TESObjectARMO *armor{nullptr};
  RE::ExtraDataList *extraData{nullptr};
};

class EquipConflictVisitor : public Ext::IItemChangeVisitor {
public:
  explicit EquipConflictVisitor(BipedObjectSlot a_bodySlot) : bodySlot(a_bodySlot) {}

  std::uint32_t Visit(RE::InventoryEntryData *a_entryData) override {
    auto *armor = a_entryData && a_entryData->object
                      ? a_entryData->object->As<RE::TESObjectARMO>()
                      : nullptr;
    if (!armor || !armor->HasPartOf(bodySlot)) {
      return 1;
    }

    auto *wornExtraData = GetWornExtraData(a_entryData);
    if (!wornExtraData && !a_entryData->IsWorn()) {
      return 1;
    }

    pendingUnequips.push_back({armor, wornExtraData});
    return 1;
  }

  BipedObjectSlot bodySlot{BipedObjectSlot::kNone};
  std::vector<PendingUnequip> pendingUnequips;
};

} // namespace

void Hooks::Install(const bool a_installEquipConflictHook) {
  Patches::WriteInitWornPatch(&InitWornArmor);
  Patches::WriteGetWornMaskPatch(&GetWornMask);
  if (a_installEquipConflictHook) {
    logger::info("Installing equip conflict hook"sv);
    Patches::WriteFixEquipConflictPatch(&FixEquipConflictCheck);
  } else {
    logger::info("Equip conflict hook disabled by settings"sv);
  }
}

void Hooks::InitWornArmor(RE::TESObjectARMO *a_armor, RE::Actor *a_actor,
                          RE::BSTSmartPointer<RE::BipedAnim> *a_biped) {
  auto race = a_actor->GetRace();
  auto sex = a_actor->GetActorBase()->GetSex();
  std::unordered_set<RE::TESObjectARMA *> initializedAddons;
  initializedAddons.reserve(RE::BIPED_OBJECTS::kTotal * 2);

  for (auto &armorAddon : a_armor->armorAddons) {
    if (Ext::TESObjectARMA::HasRace(armorAddon, race)) {

      auto visitor = [&initializedAddons, a_biped,
                      sex](auto *visitedArmor, auto *visitedArmorAddon) {
        if (!visitedArmorAddon ||
            !initializedAddons.insert(visitedArmorAddon).second) {
          return;
        }

        return Ext::TESObjectARMA::InitWornArmorAddon(
            visitedArmorAddon, visitedArmor, a_biped, sex);
      };

      DynamicArmorManager::GetSingleton()->VisitArmorAddons(
          a_actor, a_armor, armorAddon, visitor);
    }
  }
}

auto Hooks::GetWornMask(RE::InventoryChanges *a_inventoryChanges)
    -> BipedObjectSlot {
  auto owner = a_inventoryChanges->owner;
  auto actor = owner->As<RE::Actor>();

  GetWornMaskVisitor visitor{actor};
  Ext::InventoryChanges::Accept(a_inventoryChanges, std::addressof(visitor));
  return visitor.slotMask.get();
}

bool Hooks::FixEquipConflictCheck(std::uintptr_t a_itemAddr,
                                  std::uint32_t a_bodySlot,
                                  RE::Actor *a_actor) {
  auto *item = reinterpret_cast<RE::TESForm *>(a_itemAddr);
  auto *armor = item ? item->As<RE::TESObjectARMO>() : nullptr;
  if (!armor || !a_actor) {
    return true;
  }

  const auto slotMask = BodySlotToMask(a_bodySlot);
  if (slotMask == BipedObjectSlot::kNone) {
    return true;
  }

  auto *inventory = a_actor->GetInventoryChanges();
  if (!inventory) {
    return true;
  }

  EquipConflictVisitor visitor{slotMask};
  Ext::InventoryChanges::Accept(inventory, std::addressof(visitor));

  auto *equipManager = RE::ActorEquipManager::GetSingleton();
  if (!equipManager) {
    return true;
  }

  for (const auto &pendingUnequip : visitor.pendingUnequips) {
    equipManager->UnequipObject(a_actor, pendingUnequip.armor,
                                pendingUnequip.extraData, 1, nullptr, false,
                                false, true, false, nullptr);
  }

  return false;
}
