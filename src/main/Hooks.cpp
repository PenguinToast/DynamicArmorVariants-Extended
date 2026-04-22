#include "Hooks.h"
#include "DynamicArmorManager.h"
#include "Ext/Actor.h"
#include "Ext/InventoryChanges.h"
#include "Ext/TESObjectARMA.h"
#include "GetWornMaskVisitor.h"
#include "LogUtil.h"
#include "Patches.h"
#include "Settings.h"

#include <optional>
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

struct BodyPartTestOverride {
  RE::BGSBipedObjectForm *TargetForm{nullptr};
  BipedObjectSlot OverrideMask{BipedObjectSlot::kNone};
};

thread_local std::optional<BodyPartTestOverride> g_bodyPartTestOverride;

class ScopedBodyPartTestOverride {
public:
  ScopedBodyPartTestOverride(RE::BGSBipedObjectForm *a_targetForm,
                             const BipedObjectSlot a_overrideMask)
      : previous_(g_bodyPartTestOverride) {
    if (!a_targetForm) {
      return;
    }

    g_bodyPartTestOverride = BodyPartTestOverride{
        .TargetForm = a_targetForm,
        .OverrideMask = a_overrideMask,
    };
  }

  ~ScopedBodyPartTestOverride() { g_bodyPartTestOverride = previous_; }

private:
  std::optional<BodyPartTestOverride> previous_;
};

struct PendingUnequip {
  RE::TESObjectARMO *armor{nullptr};
  RE::ExtraDataList *extraData{nullptr};
};

class EquipConflictVisitor : public Ext::IItemChangeVisitor {
public:
  explicit EquipConflictVisitor(BipedObjectSlot a_bodySlot)
      : bodySlot(a_bodySlot) {}

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

void Hooks::Install() {
  const auto &settings = Settings::Get();

  Patches::WriteInitWornPatch(&InitWornArmor);
  LogUtil::LogHookInstalled("InitWornArmor"sv);
  Patches::WriteGetWornMaskPatch(&GetWornMask);
  LogUtil::LogHookInstalled("GetWornMask"sv);

  if (settings.useOwnershipBasedArmorMasks) {
    Patches::WriteTestBodyPartByIndexPatch(&TestBodyPartByIndex);
    LogUtil::LogHookInstalled("Ownership-mask TestBodyPart"sv);
  } else {
    LogUtil::LogHookSkipped("Ownership-mask TestBodyPart"sv,
                            "disabled by settings"sv);
  }

  if (settings.installEquipConflictHook) {
    Patches::WriteFixEquipConflictPatch(&FixEquipConflictCheck);
    LogUtil::LogHookInstalled("Equip conflict"sv);
  } else {
    LogUtil::LogHookSkipped("Equip conflict"sv, "disabled by settings"sv);
  }
}

void Hooks::InitWornArmor(RE::TESObjectARMO *a_armor, RE::Actor *a_actor,
                          RE::BSTSmartPointer<RE::BipedAnim> *a_biped) {
  const auto useMaskOverrides =
      DynamicArmorManager::GetSingleton()->ShouldUseCustomInitWornArmor(
          a_actor, a_armor);
  auto sex = a_actor->GetActorBase()->GetSex();
  // Different source branches can converge on the same resolved visual ARMA.
  // Initialize each resolved addon once so we do not render duplicates.
  std::unordered_set<RE::TESObjectARMA *> initializedResolvedAddons;
  initializedResolvedAddons.reserve(RE::BIPED_OBJECTS::kTotal * 2);

  for (auto &armorAddon : a_armor->armorAddons) {
    auto visitor = [&initializedResolvedAddons, a_armor, a_biped, sex,
                    useMaskOverrides](
                       const DynamicArmorResolvedAddonVisit &a_visit) {
      auto *visitedArmor = a_visit.Armor;
      auto *visitedArmorAddon = a_visit.ArmorAddon;
      if (!visitedArmorAddon ||
          !initializedResolvedAddons.insert(visitedArmorAddon).second) {
        return;
      }

      if (useMaskOverrides && a_visit.InitOverrideMask.has_value()) {
        ScopedBodyPartTestOverride maskOverride{
            static_cast<RE::BGSBipedObjectForm *>(visitedArmor),
            *a_visit.InitOverrideMask};
        return Ext::TESObjectARMA::InitWornArmorAddon(
            visitedArmorAddon, visitedArmor, a_biped, sex);
      }

      return Ext::TESObjectARMA::InitWornArmorAddon(visitedArmorAddon,
                                                    visitedArmor, a_biped, sex);
    };

    DynamicArmorManager::GetSingleton()->VisitArmorAddons(a_actor, a_armor,
                                                          armorAddon, visitor);
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

bool Hooks::TestBodyPartByIndex(RE::BGSBipedObjectForm *a_form,
                                std::uint32_t a_bodyPart) {
  if (!a_form || a_bodyPart >= 32) {
    return false;
  }

  auto slotMask = a_form->bipedModelData.bipedObjectSlots.get();
  if (g_bodyPartTestOverride.has_value() &&
      g_bodyPartTestOverride->TargetForm == a_form) {
    slotMask = g_bodyPartTestOverride->OverrideMask;
  }

  return (std::to_underlying(slotMask) & (1u << a_bodyPart)) != 0;
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
