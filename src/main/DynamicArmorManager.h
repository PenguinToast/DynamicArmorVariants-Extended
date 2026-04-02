#pragma once

#include "ArmorAddonResolutionCache.h"
#include "ArmorVariant.h"

#include <chrono>
#include <optional>
#include <shared_mutex>
#include <unordered_set>
#include <vector>

class DynamicArmorManager {
public:
  ~DynamicArmorManager() = default;
  DynamicArmorManager(const DynamicArmorManager &) = delete;
  DynamicArmorManager(DynamicArmorManager &&) = delete;
  DynamicArmorManager &operator=(const DynamicArmorManager &) = delete;
  DynamicArmorManager &operator=(DynamicArmorManager &&) = delete;

  static auto GetSingleton() -> DynamicArmorManager *;

  void RegisterArmorVariant(std::string_view a_name, ArmorVariant &&a_variant);
  void ReplaceArmorVariant(std::string_view a_name, ArmorVariant &&a_variant);
  auto DeleteArmorVariant(std::string_view a_name) -> bool;
  void SetCondition(std::string_view a_state,
                    const std::shared_ptr<RE::TESCondition> &a_condition);
  void ClearCondition(std::string_view a_state);

  void VisitArmorAddons(
      RE::Actor *a_actor, RE::TESObjectARMO *a_defaultArmor,
      RE::TESObjectARMA *a_armorAddon,
      // Callback params:
      // 1. visited armor for this resolved branch
      // 2. visited addon for this resolved branch
      // 3. effective branch mask used by worn-mask aggregation
      // 4. optional narrower owning-armor mask override used during
      //    InitWornArmorAddon body-part tests; nullopt means vanilla owning
      //    armor mask is already correct for this branch
      const std::function<void(RE::TESObjectARMO *, RE::TESObjectARMA *,
                               BipedObjectSlot,
                               std::optional<BipedObjectSlot>)>
          &a_visit) const;
  auto ShouldUseCustomInitWornArmor(RE::Actor *a_actor,
                                    RE::TESObjectARMO *a_armor) const -> bool;

  auto GetBipedObjectSlots(RE::Actor *a_actor, RE::TESObjectARMO *a_armor) const
      -> BipedObjectSlot;

  auto GetVariants(RE::TESObjectARMO *a_armor) const
      -> std::vector<std::string>;

  auto GetEquippedArmorsWithVariants(RE::Actor *a_actor)
      -> std::vector<RE::TESObjectARMO *>;

  auto GetDisplayName(const std::string &a_variant) const -> std::string;

  void ApplyVariant(RE::Actor *a_actor, const std::string &a_variant,
                    bool a_keepExistingOverrides = false);

  void ApplyVariant(RE::Actor *a_actor, const RE::TESObjectARMO *a_armor,
                    const std::string &a_variant,
                    bool a_keepExistingOverrides = false);

  void RemoveVariantOverride(RE::Actor *a_actor, const std::string &a_variant);

  void ResetVariant(RE::Actor *a_actor, const RE::TESObjectARMO *a_armor);

  void ResetAllVariants(RE::Actor *a_actor);

  void ResetAllVariants(RE::FormID a_formID);

  void ClearArmorAddonResolutionCache(RE::FormID a_actorFormID) const;

  void Serialize(SKSE::SerializationInterface *a_skse);

  void Deserialize(SKSE::SerializationInterface *a_skse);

  void Revert();

private:
  struct ArmorSlotContribution {
    RE::TESObjectARMA *ArmorAddon{nullptr};
    BipedObjectSlot OwnershipMask{BipedObjectSlot::kNone};
  };

  struct ArmorSlotContributionMap {
    BipedObjectSlot ArmorMask{BipedObjectSlot::kNone};
    std::vector<ArmorSlotContribution> Sources;
  };

  static constexpr std::uint32_t SerializationVersion = 1;
  static constexpr std::uint32_t SerializationType = 'AAVO';
  static constexpr std::size_t ArmorAddonResolutionCacheCapacity = 500;
  static constexpr auto ArmorAddonResolutionCacheTtl =
      std::chrono::milliseconds(500);

  DynamicArmorManager() = default;

  auto IsVariantOverrideLocked(RE::Actor *a_actor,
                               std::string_view a_state) const -> bool;
  auto IsVariantConditionLocked(RE::Actor *a_actor,
                                std::string_view a_state) const -> bool;
  auto IsUsingVariantLocked(RE::Actor *a_actor, std::string_view a_state) const
      -> bool;
  auto GetOrBuildArmorAddonResolution(RE::Actor *a_actor,
                                      RE::TESObjectARMA *a_armorAddon) const
      -> ArmorAddonResolutionCache::Value;
  auto GetOrBuildArmorSlotContributionMap(RE::TESObjectARMO *a_armor) const
      -> const ArmorSlotContributionMap &;
  auto BuildResolvedCoverageMask(RE::Actor *a_actor, RE::TESObjectARMO *a_armor,
                                 bool *a_hasActiveVariant = nullptr,
                                 ArmorVariant::OverrideOption *a_overrideOption =
                                     nullptr) const -> BipedObjectSlot;
  auto BuildArmorAddonResolution(RE::Actor *a_actor,
                                 RE::TESObjectARMA *a_armorAddon) const
      -> ArmorAddonResolutionCache::Value;
  auto BuildArmorSlotContributionMap(RE::TESObjectARMO *a_armor) const
      -> ArmorSlotContributionMap;
  void VisitResolvedArmorAddonsLocked(
      RE::TESObjectARMO *a_defaultArmor, RE::TESObjectARMA *a_sourceArmorAddon,
      const ArmorSlotContributionMap &a_sourceContributionMap,
      const ArmorAddonResolutionCache::Value &a_resolution,
      const std::function<void(RE::TESObjectARMO *, RE::TESObjectARMA *,
                               BipedObjectSlot,
                               std::optional<BipedObjectSlot>)> &a_visit) const;
  static auto FindOwnershipMaskForAddon(const ArmorSlotContributionMap &a_map,
                                        const RE::TESObjectARMA *a_armorAddon)
      -> BipedObjectSlot;
  void ClearArmorAddonResolutionCache() const;
  void ClearArmorAddonResolutionCacheLocked(RE::FormID a_actorFormID) const;
  auto GetVariantsLocked(RE::TESObjectARMO *a_armor) const
      -> std::vector<std::string>;
  auto CollectAffectedWornArmorsLocked(RE::Actor *a_actor,
                                       const ArmorVariant &a_variant) const
      -> std::vector<RE::TESObjectARMO *>;
  static void SetOverrideSequenceLocked(
      std::unordered_map<std::string, std::uint64_t> &a_overrides,
      std::string_view a_variant, std::uint64_t a_sequence);
  static void RemoveOverrideLocked(
      std::unordered_map<std::string, std::uint64_t> &a_overrides,
      std::string_view a_variant);

  tsl::ordered_map<std::string, ArmorVariant> _variants;

  std::unordered_map<std::string, std::shared_ptr<RE::TESCondition>>
      _conditions;
  std::unordered_map<RE::FormID, std::unordered_map<std::string, std::uint64_t>>
      _variantOverrides;
  std::uint64_t _nextOverrideSequence{1};
  mutable ArmorAddonResolutionCache _armorAddonResolutionCache_{
      ArmorAddonResolutionCacheCapacity, ArmorAddonResolutionCacheTtl};
  mutable std::unordered_map<RE::FormID, ArmorSlotContributionMap>
      _armorSlotContributionCache_;
  mutable std::shared_mutex _stateMutex;
};
