#include "DynamicArmorManager.h"

#include "Ext/Actor.h"
#include "Ext/TESObjectARMA.h"

#include <mutex>
#include <optional>
#include <unordered_map>

namespace {
auto FilterAddonListForRace(const ArmorVariant::AddonList *a_addonList,
                            RE::TESRace *a_race)
    -> std::optional<ArmorVariant::AddonList> {
  if (!a_addonList) {
    return std::nullopt;
  }

  ArmorVariant::AddonList filtered;
  filtered.reserve(a_addonList->size());
  for (const auto &entry : *a_addonList) {
    if (!entry.ArmorAddon || !a_race ||
        !Ext::TESObjectARMA::HasRace(entry.ArmorAddon, a_race)) {
      continue;
    }

    filtered.push_back(entry);
  }

  return filtered;
}
} // namespace

void DynamicArmorManager::VisitArmorAddons(
    RE::Actor *a_actor, RE::TESObjectARMO *a_defaultArmor,
    RE::TESObjectARMA *a_armorAddon,
    const std::function<void(RE::TESObjectARMO *, RE::TESObjectARMA *)>
        &a_visit) const {
  // Observed from player-side runtime logs during equip/variant changes:
  // VisitArmorAddons runs before GetBipedObjectSlots, and Skyrim often performs
  // another VisitArmorAddons pass immediately after the slot-mask evaluation.
  std::unique_lock lock(_stateMutex);
  if (Ext::Actor::IsSkin(a_actor, a_armorAddon)) {
    a_visit(a_defaultArmor, a_armorAddon);
    return;
  }

  const auto &resolution =
      GetOrBuildArmorAddonResolution(a_actor, a_armorAddon);
  if (!resolution.ResolvedAddonList.has_value()) {
    a_visit(a_defaultArmor, a_armorAddon);
    return;
  }

  for (const auto &replacement : *resolution.ResolvedAddonList) {
    if (!replacement.ArmorAddon) {
      continue;
    }

    a_visit(replacement.Armor ? replacement.Armor : a_defaultArmor,
            replacement.ArmorAddon);
  }
}

auto DynamicArmorManager::GetBipedObjectSlots(RE::Actor *a_actor,
                                              RE::TESObjectARMO *a_armor) const
    -> BipedObjectSlot {
  std::unique_lock lock(_stateMutex);
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

  auto resolvedSlots = decltype(slot){};
  auto hasActiveVariant = false;
  auto overrideOption = ArmorVariant::OverrideOption::None;

  for (auto &armorAddon : a_armor->armorAddons) {
    if (!armorAddon) {
      continue;
    }

    const auto &resolution =
        GetOrBuildArmorAddonResolution(a_actor, armorAddon);
    if (!resolution.ActiveVariant) {
      resolvedSlots |= armorAddon->bipedModelData.bipedObjectSlots;
      continue;
    }

    hasActiveVariant = true;
    if (util::to_underlying(resolution.ActiveVariant->OverrideHead) >
        util::to_underlying(overrideOption)) {
      overrideOption = resolution.ActiveVariant->OverrideHead;
    }

    if (!resolution.ResolvedAddonList.has_value()) {
      continue;
    }

    for (const auto &resolvedAddon : *resolution.ResolvedAddonList) {
      if (!resolvedAddon.ArmorAddon) {
        continue;
      }
      if (resolvedAddon.Armor) {
        resolvedSlots |= resolvedAddon.Armor->bipedModelData.bipedObjectSlots;
      } else {
        resolvedSlots |=
            resolvedAddon.ArmorAddon->bipedModelData.bipedObjectSlots;
      }
    }
  }

  // Preserve vanilla armor-mask behavior when no active variant affects this
  // armor, because some armors define slots differently from their addons.
  if (hasActiveVariant) {
    slot = resolvedSlots;
  }

  auto headSlot =
      static_cast<BipedObjectSlot>(1 << race->data.headObject.get());
  auto hairSlot =
      static_cast<BipedObjectSlot>(1 << race->data.hairObject.get());

  switch (overrideOption) {
  case ArmorVariant::OverrideOption::ShowAll:
    slot.reset(headSlot, hairSlot);
    break;
  case ArmorVariant::OverrideOption::ShowHead:
    slot.reset(headSlot);
    break;
  case ArmorVariant::OverrideOption::HideHair:
    slot.set(headSlot);
    break;
  case ArmorVariant::OverrideOption::HideAll:
    slot.set(headSlot, hairSlot);
    break;
  }

  return slot.get();
}

auto DynamicArmorManager::IsUsingVariantLocked(RE::Actor *a_actor,
                                               std::string_view a_state) const
    -> bool {
  return IsVariantOverrideLocked(a_actor, a_state) ||
         IsVariantConditionLocked(a_actor, a_state);
}

auto DynamicArmorManager::IsVariantOverrideLocked(
    RE::Actor *a_actor, std::string_view a_state) const -> bool {
  if (!a_actor) {
    return false;
  }

  const auto state = std::string(a_state);

  if (auto it = _variantOverrides.find(a_actor->GetFormID());
      it != _variantOverrides.end()) {
    return it->second.contains(state);
  }

  return false;
}

auto DynamicArmorManager::IsVariantConditionLocked(
    RE::Actor *a_actor, std::string_view a_state) const -> bool {
  if (!a_actor) {
    return false;
  }

  const auto state = std::string(a_state);
  if (auto it = _conditions.find(state); it != _conditions.end()) {
    auto &condition = it->second;
    return condition ? condition->IsTrue(a_actor, a_actor) : false;
  }

  return false;
}

auto DynamicArmorManager::GetOrBuildArmorAddonResolution(
    RE::Actor *a_actor, RE::TESObjectARMA *a_armorAddon) const
    -> ArmorAddonResolutionCache::Value {
  const ArmorAddonResolutionCache::Key key{
      .ActorFormID = a_actor ? a_actor->GetFormID() : 0,
      .ArmorAddonFormID = a_armorAddon ? a_armorAddon->GetFormID() : 0};

  if (const auto *resolution = _armorAddonResolutionCache_.Find(key)) {
    return *resolution;
  }

  _armorAddonResolutionCache_.Insert(
      key, BuildArmorAddonResolution(a_actor, a_armorAddon));
  return *_armorAddonResolutionCache_.Find(key);
}

auto DynamicArmorManager::BuildArmorAddonResolution(
    RE::Actor *a_actor, RE::TESObjectARMA *a_armorAddon) const
    -> ArmorAddonResolutionCache::Value {
  ArmorAddonResolutionCache::Value resolution{};
  if (!a_actor || !a_armorAddon) {
    return resolution;
  }

  std::unordered_map<std::string_view, const ArmorVariant::AddonList *>
      linkedAddonLists;
  std::unordered_map<std::string, std::uint64_t> overridePriorityByName;
  if (const auto overrideIt = _variantOverrides.find(a_actor->GetFormID());
      overrideIt != _variantOverrides.end()) {
    overridePriorityByName = overrideIt->second;
  }

  const ArmorVariant *activeVariant = nullptr;
  const ArmorVariant::AddonList *stateAddonList = nullptr;
  const ArmorVariant::AddonList *linkedAddonList = nullptr;
  std::string activeVariantState;
  std::uint64_t activePriority = 0;

  linkedAddonLists.clear();

  for (auto &[name, variant] : _variants) {
    const auto *addonList = variant.GetAddonList(a_armorAddon);
    if (!addonList) {
      continue;
    }

    if (!variant.Linked.empty()) {
      linkedAddonLists.insert_or_assign(variant.Linked, addonList);
      if (variant.Linked == activeVariantState) {
        linkedAddonList = addonList;
      }
    }

    std::uint64_t candidatePriority = 0;
    if (const auto overrideIt = overridePriorityByName.find(name);
        overrideIt != overridePriorityByName.end()) {
      candidatePriority = (1ull << 63) + overrideIt->second;
    } else if (IsVariantConditionLocked(a_actor, name)) {
      candidatePriority = 1;
    }

    if (candidatePriority < activePriority) {
      continue;
    }

    if (candidatePriority == 0) {
      continue;
    }

    activePriority = candidatePriority;
    activeVariantState = name;
    activeVariant = std::addressof(variant);
    stateAddonList = addonList;
    if (const auto it = linkedAddonLists.find(activeVariantState);
        it != linkedAddonLists.end()) {
      linkedAddonList = it->second;
    } else {
      linkedAddonList = nullptr;
    }
  }

  if (activeVariant) {
    resolution.ActiveVariant = activeVariant;
    resolution.ResolvedAddonList = FilterAddonListForRace(
        linkedAddonList ? linkedAddonList : stateAddonList, a_actor->GetRace());
  }

  return resolution;
}

void DynamicArmorManager::ClearArmorAddonResolutionCache() const {
  _armorAddonResolutionCache_.Clear();
}
