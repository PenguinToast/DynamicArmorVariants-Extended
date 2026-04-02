#include "DynamicArmorManager.h"
#include "DynamicArmorManager.Internal.h"

#include "Ext/Actor.h"

namespace dave::detail {
auto CollectAffectedWornArmorsLocked(RE::Actor *a_actor,
                                     const ArmorVariant &a_variant)
    -> std::vector<RE::TESObjectARMO *> {
  std::vector<RE::TESObjectARMO *> armorItems;
  if (!a_actor) {
    return armorItems;
  }

  std::unordered_set<RE::TESObjectARMO *> seenArmors;
  seenArmors.reserve(32);
  armorItems.reserve(32);

  for (std::uint32_t i = 0; i < 32; i++) {
    auto slot = static_cast<BipedObjectSlot>(1 << i);
    auto *armor = a_actor->GetWornArmor(slot);
    if (!armor || !seenArmors.insert(armor).second ||
        !a_variant.WouldReplaceAny(armor)) {
      continue;
    }

    armorItems.push_back(armor);
  }

  return armorItems;
}

void SetOverrideSequenceLocked(
    std::unordered_map<std::string, std::uint64_t> &a_overrides,
    std::string_view a_variant, std::uint64_t a_sequence) {
  a_overrides.insert_or_assign(std::string(a_variant), a_sequence);
}

void RemoveOverrideLocked(
    std::unordered_map<std::string, std::uint64_t> &a_overrides,
    std::string_view a_variant) {
  a_overrides.erase(std::string(a_variant));
}
} // namespace dave::detail

void DynamicArmorManager::ApplyVariant(RE::Actor *a_actor,
                                       const std::string &a_variant,
                                       bool a_keepExistingOverrides) {
  auto &state = *state_;
  std::unique_lock lock(state.mutex);
  dave::detail::ClearArmorAddonResolutionCache(state);

  auto it = state.variants.find(a_variant);
  if (it == state.variants.end()) {
    return;
  }

  const auto &newVariant = it->second;
  const auto armorItems =
      dave::detail::CollectAffectedWornArmorsLocked(a_actor, newVariant);
  if (armorItems.empty()) {
    return;
  }

  auto &[_, overrides] =
      *state.variantOverrides
           .try_emplace(a_actor->GetFormID(),
                        std::unordered_map<std::string, std::uint64_t>{})
           .first;

  dave::detail::SetOverrideSequenceLocked(overrides, a_variant,
                                          state.nextOverrideSequence++);

  if (!a_keepExistingOverrides) {
    for (auto overrideIt = overrides.begin(); overrideIt != overrides.end();) {
      if (overrideIt->first == a_variant) {
        ++overrideIt;
        continue;
      }

      const auto variantIt = state.variants.find(overrideIt->first);
      if (variantIt == state.variants.end()) {
        overrideIt = overrides.erase(overrideIt);
        continue;
      }

      bool conflicts = false;
      for (const auto *armor : armorItems) {
        if (variantIt->second.WouldReplaceAny(armor)) {
          conflicts = true;
          break;
        }
      }

      if (conflicts) {
        overrideIt = overrides.erase(overrideIt);
      } else {
        ++overrideIt;
      }
    }
  }

  dave::detail::ClearArmorAddonResolutionCacheLocked(state,
                                                     a_actor->GetFormID());
  Ext::Actor::Update3DSafe(a_actor);
}

void DynamicArmorManager::ApplyVariant(RE::Actor *a_actor,
                                       const RE::TESObjectARMO *a_armor,
                                       const std::string &a_variant,
                                       bool a_keepExistingOverrides) {
  auto &state = *state_;
  std::unique_lock lock(state.mutex);
  dave::detail::ClearArmorAddonResolutionCache(state);

  auto it = state.variants.find(a_variant);
  if (it == state.variants.end()) {
    return;
  }

  const auto &newVariant = it->second;

  if (!a_actor->GetWornArmor(a_armor->GetFormID()) ||
      !newVariant.WouldReplaceAny(a_armor)) {
    return;
  }

  auto &[_, overrides] =
      *state.variantOverrides
           .try_emplace(a_actor->GetFormID(),
                        std::unordered_map<std::string, std::uint64_t>{})
           .first;

  dave::detail::SetOverrideSequenceLocked(overrides, a_variant,
                                          state.nextOverrideSequence++);

  if (!a_keepExistingOverrides) {
    for (auto overrideIt = overrides.begin(); overrideIt != overrides.end();) {
      if (overrideIt->first == a_variant) {
        ++overrideIt;
        continue;
      }

      const auto variantIt = state.variants.find(overrideIt->first);
      if (variantIt == state.variants.end()) {
        overrideIt = overrides.erase(overrideIt);
        continue;
      }

      if (variantIt->second.WouldReplaceAny(a_armor)) {
        overrideIt = overrides.erase(overrideIt);
      } else {
        ++overrideIt;
      }
    }
  }

  dave::detail::ClearArmorAddonResolutionCacheLocked(state,
                                                     a_actor->GetFormID());
  Ext::Actor::Update3DSafe(a_actor);
}

void DynamicArmorManager::ResetVariant(RE::Actor *a_actor,
                                       const RE::TESObjectARMO *a_armor) {
  auto &state = *state_;
  std::unique_lock lock(state.mutex);
  dave::detail::ClearArmorAddonResolutionCache(state);
  auto it = state.variantOverrides.find(a_actor->GetFormID());
  if (it == state.variantOverrides.end()) {
    return;
  }

  auto &overrides = it->second;
  for (auto overrideIt = overrides.begin(); overrideIt != overrides.end();) {
    const auto variantIt = state.variants.find(overrideIt->first);
    if (variantIt == state.variants.end()) {
      overrideIt = overrides.erase(overrideIt);
      continue;
    }

    if (variantIt->second.WouldReplaceAny(a_armor)) {
      overrideIt = overrides.erase(overrideIt);
    } else {
      ++overrideIt;
    }
  }

  if (overrides.empty()) {
    state.variantOverrides.erase(it);
  }

  dave::detail::ClearArmorAddonResolutionCacheLocked(state,
                                                     a_actor->GetFormID());
  Ext::Actor::Update3DSafe(a_actor);
}

void DynamicArmorManager::RemoveVariantOverride(RE::Actor *a_actor,
                                                const std::string &a_variant) {
  auto &state = *state_;
  std::unique_lock lock(state.mutex);
  dave::detail::ClearArmorAddonResolutionCache(state);
  auto it = state.variantOverrides.find(a_actor->GetFormID());
  if (it == state.variantOverrides.end()) {
    return;
  }

  dave::detail::RemoveOverrideLocked(it->second, a_variant);
  if (it->second.empty()) {
    state.variantOverrides.erase(it);
  }

  dave::detail::ClearArmorAddonResolutionCacheLocked(state,
                                                     a_actor->GetFormID());
  Ext::Actor::Update3DSafe(a_actor);
}

void DynamicArmorManager::ResetAllVariants(RE::Actor *a_actor) {
  auto &state = *state_;
  std::unique_lock lock(state.mutex);
  dave::detail::ClearArmorAddonResolutionCache(state);
  state.variantOverrides.erase(a_actor->GetFormID());
  dave::detail::ClearArmorAddonResolutionCacheLocked(state,
                                                     a_actor->GetFormID());
  Ext::Actor::Update3DSafe(a_actor);
}

void DynamicArmorManager::ResetAllVariants(RE::FormID a_formID) {
  auto &state = *state_;
  std::unique_lock lock(state.mutex);
  dave::detail::ClearArmorAddonResolutionCache(state);
  state.variantOverrides.erase(a_formID);
}
