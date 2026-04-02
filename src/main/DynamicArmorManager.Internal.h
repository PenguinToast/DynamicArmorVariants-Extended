#pragma once

#include "ArmorAddonResolutionCache.h"
#include "ArmorVariant.h"
#include "DynamicArmorManager.Types.h"
#include "Settings.h"

#include <list>
#include <optional>
#include <shared_mutex>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace dave::detail {

struct VariantCandidateRef {
  const std::string *Name{nullptr};
  const ArmorVariant *Variant{nullptr};
  const ArmorVariant::AddonList *AddonList{nullptr};
};

struct VariantCandidateCacheEntry {
  std::vector<VariantCandidateRef> Candidates;
  std::list<RE::FormID>::iterator LruIt;
};

struct ArmorSlotContribution {
  RE::TESObjectARMA *ArmorAddon{nullptr};
  BipedObjectSlot OwnershipMask{BipedObjectSlot::kNone};
};

struct ArmorSlotContributionMap {
  BipedObjectSlot ArmorMask{BipedObjectSlot::kNone};
  std::vector<ArmorSlotContribution> Sources;
};

struct DynamicArmorManagerState {
  static constexpr std::uint32_t SerializationVersion = 1;
  static constexpr std::uint32_t SerializationType = 'AAVO';

  tsl::ordered_map<std::string, ArmorVariant> variants;
  std::unordered_map<std::string_view, std::vector<const ArmorVariant *>>
      linkedVariantsByTarget;
  mutable std::list<RE::FormID> variantCandidatesLru;
  mutable std::unordered_map<RE::FormID, VariantCandidateCacheEntry>
      variantCandidatesByArmorAddon;
  std::unordered_map<std::string, std::shared_ptr<RE::TESCondition>> conditions;
  std::unordered_map<RE::FormID, std::unordered_map<std::string, std::uint64_t>>
      variantOverrides;
  std::uint64_t nextOverrideSequence{1};
  std::size_t refreshVariantCacheCapacity{
      Settings::DefaultRefreshVariantCacheCapacity};
  std::chrono::milliseconds refreshVariantCacheTtl{
      Settings::DefaultRefreshVariantCacheTtl};
  mutable ArmorAddonResolutionCache armorAddonResolutionCache{
      Settings::DefaultRefreshVariantCacheCapacity,
      Settings::DefaultRefreshVariantCacheTtl};
  mutable std::unordered_map<RE::FormID, ArmorSlotContributionMap>
      armorSlotContributionCache;
  mutable std::shared_mutex mutex;
};

auto IsVariantOverrideLocked(const DynamicArmorManagerState &a_state,
                             RE::Actor *a_actor, std::string_view a_stateName)
    -> bool;
auto IsVariantOverrideLocked(const DynamicArmorManagerState &a_state,
                             RE::Actor *a_actor, const std::string &a_stateName)
    -> bool;
auto IsVariantConditionLocked(const DynamicArmorManagerState &a_state,
                              RE::Actor *a_actor, std::string_view a_stateName)
    -> bool;
auto IsVariantConditionLocked(const DynamicArmorManagerState &a_state,
                              RE::Actor *a_actor,
                              const std::string &a_stateName) -> bool;
auto IsUsingVariantLocked(const DynamicArmorManagerState &a_state,
                          RE::Actor *a_actor, std::string_view a_stateName)
    -> bool;
auto GetOrBuildArmorAddonResolution(DynamicArmorManagerState &a_state,
                                    RE::Actor *a_actor,
                                    RE::TESObjectARMA *a_armorAddon)
    -> ArmorAddonResolutionCache::Value;
auto GetOrBuildArmorSlotContributionMap(DynamicArmorManagerState &a_state,
                                        RE::TESObjectARMO *a_armor)
    -> const ArmorSlotContributionMap &;
auto BuildResolvedCoverageMask(
    DynamicArmorManagerState &a_state, RE::Actor *a_actor,
    RE::TESObjectARMO *a_armor, bool *a_hasActiveVariant = nullptr,
    ArmorVariant::OverrideOption *a_overrideOption = nullptr)
    -> BipedObjectSlot;
auto BuildArmorAddonResolution(const DynamicArmorManagerState &a_state,
                               RE::Actor *a_actor,
                               RE::TESObjectARMA *a_armorAddon)
    -> ArmorAddonResolutionCache::Value;
auto HasVariantsLocked(const DynamicArmorManagerState &a_state,
                       RE::TESObjectARMO *a_armor) -> bool;
auto GetOrBuildVariantCandidatesForAddon(
    const DynamicArmorManagerState &a_state, RE::TESObjectARMA *a_armorAddon)
    -> const std::vector<VariantCandidateRef> &;
auto BuildArmorSlotContributionMap(RE::TESObjectARMO *a_armor)
    -> ArmorSlotContributionMap;
void VisitResolvedArmorAddons(
    DynamicArmorManagerState &a_state, RE::TESObjectARMO *a_defaultArmor,
    RE::TESObjectARMA *a_sourceArmorAddon,
    const ArmorSlotContributionMap *a_sourceContributionMap,
    const ArmorAddonResolutionCache::Value &a_resolution,
    const DynamicArmorResolvedAddonVisitor &a_visit);
auto FindOwnershipMaskForAddon(const ArmorSlotContributionMap &a_map,
                               const RE::TESObjectARMA *a_armorAddon)
    -> BipedObjectSlot;
void ClearArmorAddonResolutionCache(DynamicArmorManagerState &a_state);
void ClearArmorAddonResolutionCacheLocked(DynamicArmorManagerState &a_state,
                                          RE::FormID a_actorFormID);
auto GetVariantsLocked(const DynamicArmorManagerState &a_state,
                       RE::TESObjectARMO *a_armor) -> std::vector<std::string>;
auto CollectAffectedWornArmorsLocked(RE::Actor *a_actor,
                                     const ArmorVariant &a_variant)
    -> std::vector<RE::TESObjectARMO *>;
void SetOverrideSequenceLocked(
    std::unordered_map<std::string, std::uint64_t> &a_overrides,
    std::string_view a_variant, std::uint64_t a_sequence);
void RemoveOverrideLocked(
    std::unordered_map<std::string, std::uint64_t> &a_overrides,
    std::string_view a_variant);
void RebuildLinkedVariantsIndexLocked(DynamicArmorManagerState &a_state);

} // namespace dave::detail
