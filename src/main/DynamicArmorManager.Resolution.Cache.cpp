#include "DynamicArmorManager.h"
#include "DynamicArmorManager.Internal.h"

#include "Ext/Actor.h"
#include "Ext/TESObjectARMA.h"

#include <limits>

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
    const std::string &a_stateName) -> bool {
  if (!a_actor) {
    return false;
  }

  if (auto it = a_state.variantOverrides.find(a_actor->GetFormID());
      it != a_state.variantOverrides.end()) {
    return it->second.contains(a_stateName);
  }

  return false;
}

auto dave::detail::IsVariantOverrideLocked(
    const DynamicArmorManagerState &a_state, RE::Actor *a_actor,
    std::string_view a_stateName) -> bool {
  return IsVariantOverrideLocked(a_state, a_actor, std::string(a_stateName));
}

auto dave::detail::IsVariantConditionLocked(
    const DynamicArmorManagerState &a_state, RE::Actor *a_actor,
    const std::string &a_stateName) -> bool {
  if (!a_actor) {
    return false;
  }

  if (auto it = a_state.conditions.find(a_stateName);
      it != a_state.conditions.end()) {
    auto &condition = it->second;
    return condition ? condition->IsTrue(a_actor, a_actor) : false;
  }

  return false;
}

auto dave::detail::IsVariantConditionLocked(
    const DynamicArmorManagerState &a_state, RE::Actor *a_actor,
    std::string_view a_stateName) -> bool {
  return IsVariantConditionLocked(a_state, a_actor, std::string(a_stateName));
}

auto dave::detail::GetOrBuildArmorAddonResolution(
    DynamicArmorManagerState &a_state, RE::Actor *a_actor,
    RE::TESObjectARMA *a_armorAddon) -> ArmorAddonResolutionCache::Value {
  const ArmorAddonResolutionCache::Key key{
      .ActorFormID = a_actor ? a_actor->GetFormID() : 0,
      .ArmorAddonFormID = a_armorAddon ? a_armorAddon->GetFormID() : 0};

  if (const auto resolution = a_state.armorAddonResolutionCache.Find(key)) {
    return resolution->get();
  }

  a_state.armorAddonResolutionCache.Insert(
      key, BuildArmorAddonResolution(a_state, a_actor, a_armorAddon));
  return a_state.armorAddonResolutionCache.Find(key)->get();
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

auto dave::detail::GetOrBuildVariantCandidatesForAddon(
    const DynamicArmorManagerState &a_state, RE::TESObjectARMA *a_armorAddon)
    -> const std::vector<VariantCandidateRef> & {
  static const std::vector<VariantCandidateRef> kEmptyCandidates{};
  if (!a_armorAddon) {
    return kEmptyCandidates;
  }

  const auto formID = a_armorAddon->GetFormID();
  if (const auto it = a_state.variantCandidatesByArmorAddon.find(formID);
      it != a_state.variantCandidatesByArmorAddon.end()) {
    a_state.variantCandidatesLru.erase(it->second.LruIt);
    a_state.variantCandidatesLru.push_front(formID);
    it->second.LruIt = a_state.variantCandidatesLru.begin();
    return it->second.Candidates;
  }

  auto candidates = std::vector<VariantCandidateRef>{};
  candidates.reserve(a_state.variants.size());
  for (auto &[name, variant] : a_state.variants) {
    if (const auto *addonList = variant.GetAddonList(a_armorAddon); addonList) {
      candidates.push_back(
          VariantCandidateRef{.Name = std::addressof(name),
                              .Variant = std::addressof(variant),
                              .AddonList = addonList});
    }
  }

  a_state.variantCandidatesLru.push_front(formID);
  auto [it, _] = a_state.variantCandidatesByArmorAddon.emplace(
      formID, VariantCandidateCacheEntry{
                  .Candidates = std::move(candidates),
                  .LruIt = a_state.variantCandidatesLru.begin()});

  if (a_state.variantCandidatesByArmorAddon.size() >
      DynamicArmorManagerState::ArmorAddonResolutionCacheCapacity) {
    const auto lruFormID = a_state.variantCandidatesLru.back();
    a_state.variantCandidatesByArmorAddon.erase(lruFormID);
    a_state.variantCandidatesLru.pop_back();
  }

  return it->second.Candidates;
}

auto dave::detail::BuildArmorAddonResolution(
    const DynamicArmorManagerState &a_state, RE::Actor *a_actor,
    RE::TESObjectARMA *a_armorAddon) -> ArmorAddonResolutionCache::Value {
  ArmorAddonResolutionCache::Value resolution{};
  if (!a_actor || !a_armorAddon) {
    return resolution;
  }

  const std::unordered_map<std::string, std::uint64_t> *overridePriorityByName =
      nullptr;
  if (const auto overrideIt =
          a_state.variantOverrides.find(a_actor->GetFormID());
      overrideIt != a_state.variantOverrides.end()) {
    overridePriorityByName = std::addressof(overrideIt->second);
  }

  const ArmorVariant *activeVariant = nullptr;
  const ArmorVariant::AddonList *stateAddonList = nullptr;
  const ArmorVariant::AddonList *linkedAddonList = nullptr;
  std::string_view activeVariantState;
  std::int64_t activePriority = (std::numeric_limits<std::int64_t>::min)();
  const auto &candidateVariants =
      GetOrBuildVariantCandidatesForAddon(a_state, a_armorAddon);

  for (const auto &candidate : candidateVariants) {
    const auto *variant = candidate.Variant;
    if (!variant) {
      continue;
    }
    if (!candidate.Name) {
      continue;
    }
    if (!candidate.AddonList) {
      continue;
    }
    const auto &name = *candidate.Name;

    const auto *addonList = candidate.AddonList;

    std::optional<std::int64_t> candidatePriority;
    if (overridePriorityByName) {
      if (const auto overrideIt = overridePriorityByName->find(name);
          overrideIt != overridePriorityByName->end()) {
        candidatePriority =
            (1ll << 62) + static_cast<std::int64_t>(overrideIt->second);
      }
    }
    if (!candidatePriority.has_value()) {
      if (IsVariantConditionLocked(a_state, a_actor, name)) {
        candidatePriority = BuildConditionPriority(*variant);
      }
    }

    if (!candidatePriority.has_value()) {
      continue;
    }

    if (*candidatePriority < activePriority) {
      continue;
    }

    activePriority = *candidatePriority;
    activeVariantState = name;
    activeVariant = variant;
    stateAddonList = addonList;
  }

  if (activeVariant) {
    if (const auto linkedVariantsIt =
            a_state.linkedVariantsByTarget.find(activeVariantState);
        linkedVariantsIt != a_state.linkedVariantsByTarget.end()) {
      for (const auto *linkedVariant : linkedVariantsIt->second) {
        if (!linkedVariant) {
          continue;
        }

        if (const auto *addonList = linkedVariant->GetAddonList(a_armorAddon);
            addonList != nullptr) {
          linkedAddonList = addonList;
        }
      }
    }

    resolution.ActiveVariant = activeVariant;
    resolution.ResolvedAddonList = FilterAddonListForRace(
        linkedAddonList ? linkedAddonList : stateAddonList, a_actor->GetRace());
  }

  return resolution;
}

void dave::detail::ClearArmorAddonResolutionCache(
    DynamicArmorManagerState &a_state) {
  a_state.armorAddonResolutionCache.Clear();
  a_state.variantCandidatesByArmorAddon.clear();
  a_state.variantCandidatesLru.clear();
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
