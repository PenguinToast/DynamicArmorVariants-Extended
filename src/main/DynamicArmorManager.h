#pragma once

#include "ArmorAddonResolutionCache.h"
#include "ArmorVariant.h"

#include <chrono>
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
      const std::function<void(RE::TESObjectARMO *, RE::TESObjectARMA *)>
          &a_visit) const;

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
  auto BuildArmorAddonResolution(RE::Actor *a_actor,
                                 RE::TESObjectARMA *a_armorAddon) const
      -> ArmorAddonResolutionCache::Value;
  void ClearArmorAddonResolutionCache() const;
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
  mutable std::shared_mutex _stateMutex;
};
