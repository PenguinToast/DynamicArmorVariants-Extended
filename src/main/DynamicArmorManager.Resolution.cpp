#include "DynamicArmorManager.h"
#include "DynamicArmorManager.Internal.h"

#include "Ext/Actor.h"
#include "Ext/TESObjectARMA.h"

#include <mutex>
#include <optional>

void DynamicArmorManager::VisitArmorAddons(
    RE::Actor *a_actor, RE::TESObjectARMO *a_defaultArmor,
    RE::TESObjectARMA *a_armorAddon,
    const DynamicArmorResolvedAddonVisitor &a_visit) const {
  auto &state = *state_;
  std::unique_lock lock(state.mutex);
  if (!a_armorAddon) {
    return;
  }

  if (!a_actor || !a_defaultArmor || Ext::Actor::IsSkin(a_actor, a_armorAddon)) {
    const auto initMask =
        a_defaultArmor ? a_defaultArmor->bipedModelData.bipedObjectSlots.get()
                       : a_armorAddon->bipedModelData.bipedObjectSlots.get();
    a_visit(DynamicArmorResolvedAddonVisit{
        .Armor = a_defaultArmor,
        .ArmorAddon = a_armorAddon,
        .EffectiveMask = initMask,
        .InitOverrideMask = std::nullopt,
    });
    return;
  }

  const auto &sourceContributionMap =
      dave::detail::GetOrBuildArmorSlotContributionMap(state, a_defaultArmor);
  const auto resolution =
      dave::detail::GetOrBuildArmorAddonResolution(state, a_actor, a_armorAddon);
  dave::detail::VisitResolvedArmorAddons(state, a_defaultArmor, a_armorAddon,
                                         sourceContributionMap, resolution,
                                         a_visit);
}

auto DynamicArmorManager::ShouldUseCustomInitWornArmor(
    RE::Actor *a_actor, RE::TESObjectARMO *a_armor) const -> bool {
  auto &state = *state_;
  std::unique_lock lock(state.mutex);
  if (!a_actor || !a_armor) {
    return false;
  }

  const auto race = a_actor->GetRace();
  if (!race) {
    return false;
  }

  for (auto *armorAddon : a_armor->armorAddons) {
    if (!armorAddon || !Ext::TESObjectARMA::HasRace(armorAddon, race)) {
      continue;
    }

    if (const auto resolution =
            dave::detail::GetOrBuildArmorAddonResolution(state, a_actor, armorAddon);
        resolution.ActiveVariant) {
      return true;
    }
  }

  return false;
}

auto DynamicArmorManager::GetBipedObjectSlots(RE::Actor *a_actor,
                                              RE::TESObjectARMO *a_armor) const
    -> BipedObjectSlot {
  auto &state = *state_;
  std::unique_lock lock(state.mutex);
  if (!a_armor) {
    return BipedObjectSlot::kNone;
  }

  auto slot = a_armor->bipedModelData.bipedObjectSlots;

  auto race = a_actor ? a_actor->GetRace() : nullptr;
  if (!race) {
    return slot.get();
  }

  if (Ext::Actor::IsSkin(a_actor, a_armor)) {
    return slot.get();
  }

  auto hasActiveVariant = false;
  auto overrideOption = ArmorVariant::OverrideOption::None;
  const auto resolvedMask =
      dave::detail::BuildResolvedCoverageMask(state, a_actor, a_armor,
                                              &hasActiveVariant,
                                              &overrideOption);

  // Preserve vanilla armor-mask behavior when no active variant affects this
  // armor, because some armors define slots differently from their addons.
  if (hasActiveVariant) {
    slot = resolvedMask;
  }

  auto headSlot =
      static_cast<BipedObjectSlot>(1 << race->data.headObject.get());
  auto hairSlot =
      static_cast<BipedObjectSlot>(1 << race->data.hairObject.get());
  constexpr auto beardSlot = BipedObjectSlot::kModMouth;

  switch (overrideOption) {
  case ArmorVariant::OverrideOption::ShowAll:
    slot.reset(headSlot, hairSlot, beardSlot);
    break;
  case ArmorVariant::OverrideOption::ShowHead:
    slot.reset(headSlot);
    break;
  case ArmorVariant::OverrideOption::HideHair:
    slot.set(headSlot);
    break;
  case ArmorVariant::OverrideOption::HideAll:
    slot.set(headSlot, hairSlot, beardSlot);
    break;
  }

  return slot.get();
}
