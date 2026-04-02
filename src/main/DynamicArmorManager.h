#pragma once

#include "ArmorVariant.h"
#include "DynamicArmorManager.Types.h"

#include <memory>
#include <vector>

namespace dave::detail {
struct DynamicArmorManagerState;
}

class DynamicArmorManager {
public:
  ~DynamicArmorManager();
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
      const DynamicArmorResolvedAddonVisitor &a_visit) const;
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
  DynamicArmorManager();

  std::unique_ptr<dave::detail::DynamicArmorManagerState> state_;
};
