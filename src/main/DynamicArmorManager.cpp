#include "DynamicArmorManager.h"
#include "Ext/Actor.h"

#include <mutex>

auto DynamicArmorManager::GetSingleton() -> DynamicArmorManager * {
  static DynamicArmorManager singleton{};
  return std::addressof(singleton);
}

auto DynamicArmorManager::CollectAffectedWornArmorsLocked(
    RE::Actor *a_actor, const ArmorVariant &a_variant) const
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

void DynamicArmorManager::SetOverrideSequenceLocked(
    std::unordered_map<std::string, std::uint64_t> &a_overrides,
    std::string_view a_variant, std::uint64_t a_sequence) {
  a_overrides.insert_or_assign(std::string(a_variant), a_sequence);
}

void DynamicArmorManager::RemoveOverrideLocked(
    std::unordered_map<std::string, std::uint64_t> &a_overrides,
    std::string_view a_variant) {
  a_overrides.erase(std::string(a_variant));
}

void DynamicArmorManager::RegisterArmorVariant(std::string_view a_name,
                                               ArmorVariant &&a_variant) {
  std::unique_lock lock(_stateMutex);
  ClearArmorAddonResolutionCache();
  auto [it, inserted] =
      _variants.try_emplace(std::string(a_name), std::move(a_variant));

  if (inserted) {
    return;
  }

  auto &variant = it.value();

  if (!a_variant.Linked.empty()) {
    variant.Linked = a_variant.Linked;
  }

  if (!a_variant.DisplayName.empty()) {
    variant.DisplayName = a_variant.DisplayName;
  }

  if (a_variant.OverrideHead != ArmorVariant::OverrideOption::Undefined) {
    variant.OverrideHead = a_variant.OverrideHead;
  }

  for (auto &[form, replacement] : a_variant.ReplaceByForm) {
    variant.ReplaceByForm.insert_or_assign(form, replacement);
    variant.ReplaceByForm[form] = replacement;
  }

  for (auto &[slot, replacement] : a_variant.ReplaceBySlot) {
    variant.ReplaceBySlot[slot] = replacement;
  }
}

void DynamicArmorManager::ReplaceArmorVariant(std::string_view a_name,
                                              ArmorVariant &&a_variant) {
  std::unique_lock lock(_stateMutex);
  ClearArmorAddonResolutionCache();
  _variants.insert_or_assign(std::string(a_name), std::move(a_variant));
}

auto DynamicArmorManager::DeleteArmorVariant(std::string_view a_name) -> bool {
  std::unique_lock lock(_stateMutex);
  ClearArmorAddonResolutionCache();
  const auto erased = _variants.erase(std::string(a_name)) > 0;
  _conditions.erase(std::string(a_name));

  for (auto actorIt = _variantOverrides.begin();
       actorIt != _variantOverrides.end();) {
    RemoveOverrideLocked(actorIt->second, a_name);
    if (actorIt->second.empty()) {
      actorIt = _variantOverrides.erase(actorIt);
    } else {
      ++actorIt;
    }
  }

  return erased;
}

void DynamicArmorManager::SetCondition(
    std::string_view a_state,
    const std::shared_ptr<RE::TESCondition> &a_condition) {
  std::unique_lock lock(_stateMutex);
  ClearArmorAddonResolutionCache();
  _conditions.insert_or_assign(std::string(a_state), a_condition);
}

void DynamicArmorManager::ClearCondition(std::string_view a_state) {
  std::unique_lock lock(_stateMutex);
  ClearArmorAddonResolutionCache();
  _conditions.erase(std::string(a_state));
}

auto DynamicArmorManager::GetVariants(RE::TESObjectARMO *a_armor) const
    -> std::vector<std::string> {
  std::shared_lock lock(_stateMutex);
  return GetVariantsLocked(a_armor);
}

auto DynamicArmorManager::GetVariantsLocked(RE::TESObjectARMO *a_armor) const
    -> std::vector<std::string> {
  std::vector<std::string> result;

  if (!a_armor) {
    return result;
  }

  auto names = std::set<std::string>();
  auto linkedNames = std::set<std::string>();

  for (auto &armorAddon : a_armor->armorAddons) {
    for (auto &[name, variant] : _variants) {
      if (variant.WouldReplace(armorAddon)) {
        names.insert(name);

        if (!variant.Linked.empty()) {
          linkedNames.insert(variant.Linked);
        }
      }
    }
  }

  std::set_difference(names.begin(), names.end(), linkedNames.begin(),
                      linkedNames.end(), std::back_insert_iterator(result));

  return result;
}

auto DynamicArmorManager::GetEquippedArmorsWithVariants(RE::Actor *a_actor)
    -> std::vector<RE::TESObjectARMO *> {
  std::shared_lock lock(_stateMutex);
  std::vector<RE::TESObjectARMO *> resultVector;
  resultVector.reserve(32);

  std::unordered_set<RE::TESObjectARMO *> resultSet;
  resultSet.reserve(32);

  for (std::uint32_t i = 0; i < 32; i++) {
    auto slot = static_cast<BipedObjectSlot>(1 << i);

    std::vector<std::string> variants;
    if (auto armor = a_actor->GetWornArmor(slot)) {
      variants = GetVariantsLocked(armor);
      if (!variants.empty() && resultSet.insert(armor).second) {
        resultVector.push_back(armor);
      }
    }
  }

  return resultVector;
}

auto DynamicArmorManager::GetDisplayName(const std::string &a_variant) const
    -> std::string {
  std::shared_lock lock(_stateMutex);
  if (auto it = _variants.find(a_variant); it != _variants.end()) {
    return it->second.DisplayName;
  }

  return ""s;
}

void DynamicArmorManager::ApplyVariant(RE::Actor *a_actor,
                                       const std::string &a_variant,
                                       bool a_keepExistingOverrides) {
  std::unique_lock lock(_stateMutex);
  ClearArmorAddonResolutionCache();

  auto it = _variants.find(a_variant);
  if (it == _variants.end()) {
    return;
  }

  const auto &newVariant = it->second;
  const auto armorItems = CollectAffectedWornArmorsLocked(a_actor, newVariant);
  if (armorItems.empty()) {
    return;
  }

  auto &[_, overrides] =
      *_variantOverrides
           .try_emplace(a_actor->GetFormID(),
                        std::unordered_map<std::string, std::uint64_t>{})
           .first;

  SetOverrideSequenceLocked(overrides, a_variant, _nextOverrideSequence++);

  if (!a_keepExistingOverrides) {
    for (auto overrideIt = overrides.begin(); overrideIt != overrides.end();) {
      if (overrideIt->first == a_variant) {
        ++overrideIt;
        continue;
      }

      const auto variantIt = _variants.find(overrideIt->first);
      if (variantIt == _variants.end()) {
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

  Ext::Actor::Update3DSafe(a_actor);
}

void DynamicArmorManager::ApplyVariant(RE::Actor *a_actor,
                                       const RE::TESObjectARMO *a_armor,
                                       const std::string &a_variant,
                                       bool a_keepExistingOverrides) {
  std::unique_lock lock(_stateMutex);
  ClearArmorAddonResolutionCache();

  auto it = _variants.find(a_variant);
  if (it == _variants.end()) {
    return;
  }

  const auto &newVariant = it->second;

  if (!a_actor->GetWornArmor(a_armor->GetFormID()) ||
      !newVariant.WouldReplaceAny(a_armor)) {
    return;
  }

  auto &[_, overrides] =
      *_variantOverrides
           .try_emplace(a_actor->GetFormID(),
                        std::unordered_map<std::string, std::uint64_t>{})
           .first;

  SetOverrideSequenceLocked(overrides, a_variant, _nextOverrideSequence++);

  if (!a_keepExistingOverrides) {
    for (auto overrideIt = overrides.begin(); overrideIt != overrides.end();) {
      if (overrideIt->first == a_variant) {
        ++overrideIt;
        continue;
      }

      const auto variantIt = _variants.find(overrideIt->first);
      if (variantIt == _variants.end()) {
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

  Ext::Actor::Update3DSafe(a_actor);
}

void DynamicArmorManager::ResetVariant(RE::Actor *a_actor,
                                       const RE::TESObjectARMO *a_armor) {
  std::unique_lock lock(_stateMutex);
  ClearArmorAddonResolutionCache();
  auto it = _variantOverrides.find(a_actor->GetFormID());
  if (it == _variantOverrides.end()) {
    return;
  }

  auto &overrides = it->second;
  for (auto overrideIt = overrides.begin(); overrideIt != overrides.end();) {
    const auto variantIt = _variants.find(overrideIt->first);
    if (variantIt == _variants.end()) {
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
    _variantOverrides.erase(it);
  }

  Ext::Actor::Update3DSafe(a_actor);
}

void DynamicArmorManager::RemoveVariantOverride(RE::Actor *a_actor,
                                                const std::string &a_variant) {
  std::unique_lock lock(_stateMutex);
  ClearArmorAddonResolutionCache();
  auto it = _variantOverrides.find(a_actor->GetFormID());
  if (it == _variantOverrides.end()) {
    return;
  }

  RemoveOverrideLocked(it->second, a_variant);
  if (it->second.empty()) {
    _variantOverrides.erase(it);
  }

  Ext::Actor::Update3DSafe(a_actor);
}

void DynamicArmorManager::ResetAllVariants(RE::Actor *a_actor) {
  std::unique_lock lock(_stateMutex);
  ClearArmorAddonResolutionCache();
  _variantOverrides.erase(a_actor->GetFormID());
  Ext::Actor::Update3DSafe(a_actor);
}

void DynamicArmorManager::ResetAllVariants(RE::FormID a_formID) {
  std::unique_lock lock(_stateMutex);
  ClearArmorAddonResolutionCache();
  _variantOverrides.erase(a_formID);
}
