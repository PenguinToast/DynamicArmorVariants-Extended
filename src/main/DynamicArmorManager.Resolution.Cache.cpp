#include "DynamicArmorManager.h"
#include "DynamicArmorManager.Internal.h"

#include "Ext/Actor.h"
#include "Ext/TESObjectARMA.h"

#include <limits>
#include <unordered_map>

namespace {
auto BuildConditionPriority(const ArmorVariant &a_variant) -> std::int64_t {
  return static_cast<std::int64_t>(a_variant.Priority);
}

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

auto dave::detail::IsUsingVariantLocked(const DynamicArmorManagerState &a_state,
                                        RE::Actor *a_actor,
                                        std::string_view a_stateName) -> bool {
  return IsVariantOverrideLocked(a_state, a_actor, a_stateName) ||
         IsVariantConditionLocked(a_state, a_actor, a_stateName);
}

auto dave::detail::IsVariantOverrideLocked(
    const DynamicArmorManagerState &a_state, RE::Actor *a_actor,
    std::string_view a_stateName) -> bool {
  if (!a_actor) {
    return false;
  }

  const auto stateName = std::string(a_stateName);

  if (auto it = a_state.variantOverrides.find(a_actor->GetFormID());
      it != a_state.variantOverrides.end()) {
    return it->second.contains(stateName);
  }

  return false;
}

auto dave::detail::IsVariantConditionLocked(
    const DynamicArmorManagerState &a_state, RE::Actor *a_actor,
    std::string_view a_stateName) -> bool {
  if (!a_actor) {
    return false;
  }

  const auto stateName = std::string(a_stateName);
  if (auto it = a_state.conditions.find(stateName);
      it != a_state.conditions.end()) {
    auto &condition = it->second;
    return condition ? condition->IsTrue(a_actor, a_actor) : false;
  }

  return false;
}

auto dave::detail::GetOrBuildArmorAddonResolution(
    DynamicArmorManagerState &a_state, RE::Actor *a_actor,
    RE::TESObjectARMA *a_armorAddon) -> ArmorAddonResolutionCache::Value {
  const ArmorAddonResolutionCache::Key key{
      .ActorFormID = a_actor ? a_actor->GetFormID() : 0,
      .ArmorAddonFormID = a_armorAddon ? a_armorAddon->GetFormID() : 0};

  if (const auto *resolution = a_state.armorAddonResolutionCache.Find(key)) {
    return *resolution;
  }

  a_state.armorAddonResolutionCache.Insert(
      key, BuildArmorAddonResolution(a_state, a_actor, a_armorAddon));
  return *a_state.armorAddonResolutionCache.Find(key);
}

auto dave::detail::GetOrBuildArmorSlotContributionMap(
    DynamicArmorManagerState &a_state, RE::TESObjectARMO *a_armor)
    -> const ArmorSlotContributionMap & {
  static const ArmorSlotContributionMap kEmptyContributionMap{};
  if (!a_armor) {
    return kEmptyContributionMap;
  }

  const auto formID = a_armor->GetFormID();
  if (const auto it = a_state.armorSlotContributionCache.find(formID);
      it != a_state.armorSlotContributionCache.end()) {
    return it->second;
  }

  return a_state.armorSlotContributionCache
      .emplace(formID, BuildArmorSlotContributionMap(a_armor))
      .first->second;
}

auto dave::detail::BuildArmorAddonResolution(
    const DynamicArmorManagerState &a_state, RE::Actor *a_actor,
    RE::TESObjectARMA *a_armorAddon) -> ArmorAddonResolutionCache::Value {
  ArmorAddonResolutionCache::Value resolution{};
  if (!a_actor || !a_armorAddon) {
    return resolution;
  }

  std::unordered_map<std::string_view, const ArmorVariant::AddonList *>
      linkedAddonLists;
  std::unordered_map<std::string, std::uint64_t> overridePriorityByName;
  if (const auto overrideIt =
          a_state.variantOverrides.find(a_actor->GetFormID());
      overrideIt != a_state.variantOverrides.end()) {
    overridePriorityByName = overrideIt->second;
  }

  const ArmorVariant *activeVariant = nullptr;
  const ArmorVariant::AddonList *stateAddonList = nullptr;
  const ArmorVariant::AddonList *linkedAddonList = nullptr;
  std::string activeVariantState;
  std::int64_t activePriority = (std::numeric_limits<std::int64_t>::min)();

  linkedAddonLists.clear();

  for (auto &[name, variant] : a_state.variants) {
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

    std::optional<std::int64_t> candidatePriority;
    if (const auto overrideIt = overridePriorityByName.find(name);
        overrideIt != overridePriorityByName.end()) {
      candidatePriority =
          (1ll << 62) + static_cast<std::int64_t>(overrideIt->second);
    } else if (IsVariantConditionLocked(a_state, a_actor, name)) {
      candidatePriority = BuildConditionPriority(variant);
    }

    if (!candidatePriority.has_value()) {
      continue;
    }

    if (*candidatePriority < activePriority) {
      continue;
    }

    activePriority = *candidatePriority;
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

void dave::detail::ClearArmorAddonResolutionCache(
    DynamicArmorManagerState &a_state) {
  a_state.armorAddonResolutionCache.Clear();
}

void DynamicArmorManager::ClearArmorAddonResolutionCache(
    const RE::FormID a_actorFormID) const {
  auto &state = *state_;
  std::unique_lock lock(state.mutex);
  dave::detail::ClearArmorAddonResolutionCacheLocked(state, a_actorFormID);
}

void dave::detail::ClearArmorAddonResolutionCacheLocked(
    DynamicArmorManagerState &a_state, const RE::FormID a_actorFormID) {
  a_state.armorAddonResolutionCache.ClearActor(a_actorFormID);
}
