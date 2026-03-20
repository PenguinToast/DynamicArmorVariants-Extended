#include "DynamicArmorManager.h"
#include "Ext/Actor.h"

#include <mutex>

auto DynamicArmorManager::GetSingleton() -> DynamicArmorManager * {
  static DynamicArmorManager singleton{};
  return std::addressof(singleton);
}

void DynamicArmorManager::RegisterArmorVariant(std::string_view a_name,
                                               ArmorVariant &&a_variant) {
  std::unique_lock lock(_stateMutex);
  ClearArmorAddonResolutionCache();
  auto [it, inserted] =
      _variants.try_emplace(std::string(a_name), std::move(a_variant));

  if (inserted)
    return;

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

  for (auto &[formID, overrides] : _variantOverrides) {
    overrides.erase(std::string(a_name));
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

void DynamicArmorManager::VisitArmorAddons(
    RE::Actor *a_actor, RE::TESObjectARMA *a_armorAddon,
    const std::function<void(RE::TESObjectARMA *)> &a_visit) const {
  // Observed from player-side runtime logs during equip/variant changes:
  // VisitArmorAddons runs before GetBipedObjectSlots, and Skyrim often performs
  // another VisitArmorAddons pass immediately after the slot-mask evaluation.
  std::unique_lock lock(_stateMutex);
  if (Ext::Actor::IsSkin(a_actor, a_armorAddon)) {
    a_visit(a_armorAddon);
    return;
  }

  const auto &resolution =
      GetOrBuildArmorAddonResolution(a_actor, a_armorAddon);
  if (!resolution.ResolvedAddonList) {
    a_visit(a_armorAddon);
    return;
  }

  for (auto *armorAddon : *resolution.ResolvedAddonList) {
    a_visit(armorAddon);
  }
}

auto DynamicArmorManager::GetBipedObjectSlots(RE::Actor *a_actor,
                                              RE::TESObjectARMO *a_armor) const
    -> BipedObjectSlot {
  std::unique_lock lock(_stateMutex);
  if (!a_armor)
    return BipedObjectSlot::kNone;

  auto slot = a_armor->bipedModelData.bipedObjectSlots;

  auto race = a_actor ? a_actor->GetRace() : nullptr;

  if (!race)
    return slot.get();

  if (Ext::Actor::IsSkin(a_actor, a_armor))
    return slot.get();

  auto overrideOption = ArmorVariant::OverrideOption::None;

  for (auto &armorAddon : a_armor->armorAddons) {
    if (!armorAddon) {
      continue;
    }

    const auto &resolution =
        GetOrBuildArmorAddonResolution(a_actor, armorAddon);
    if (!resolution.ActiveVariant) {
      continue;
    }

    if (util::to_underlying(resolution.ActiveVariant->OverrideHead) >
        util::to_underlying(overrideOption)) {
      overrideOption = resolution.ActiveVariant->OverrideHead;
    }
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
  const auto state = std::string(a_state);

  if (auto it = _variantOverrides.find(a_actor->GetFormID());
      it != _variantOverrides.end()) {

    if (it->second.contains(state)) {
      return true;
    }
  }

  if (auto it = _conditions.find(state); it != _conditions.end()) {
    auto &condition = it->second;
    return condition ? condition->IsTrue(a_actor, a_actor) : false;
  } else {
    return false;
  }
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
  const ArmorVariant::AddonList *stateAddonList = nullptr;
  const ArmorVariant::AddonList *linkedAddonList = nullptr;
  std::string activeVariantState;

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

    if (!IsUsingVariantLocked(a_actor, name)) {
      continue;
    }

    activeVariantState = name;
    resolution.ActiveVariant = std::addressof(variant);
    stateAddonList = addonList;
    if (const auto it = linkedAddonLists.find(activeVariantState);
        it != linkedAddonLists.end()) {
      linkedAddonList = it->second;
    } else {
      linkedAddonList = nullptr;
    }
  }

  if (!resolution.ActiveVariant) {
    return resolution;
  }

  resolution.ResolvedAddonList =
      linkedAddonList ? linkedAddonList : stateAddonList;
  return resolution;
}

void DynamicArmorManager::ClearArmorAddonResolutionCache() const {
  _armorAddonResolutionCache_.Clear();
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
      if (!variants.empty()) {
        if (resultSet.insert(armor).second) {
          resultVector.push_back(armor);
        }
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
                                       const std::string &a_variant) {
  std::unique_lock lock(_stateMutex);
  ClearArmorAddonResolutionCache();
  // Find variant definition
  auto it = _variants.find(a_variant);
  if (it == _variants.end())
    return;

  auto &newVariant = it->second;

  // Get worn armor items
  std::unordered_set<RE::TESObjectARMO *> armorItems;
  armorItems.reserve(32);

  for (std::uint32_t i = 0; i < 32; i++) {
    auto slot = static_cast<BipedObjectSlot>(1 << i);

    if (auto armor = a_actor->GetWornArmor(slot)) {
      armorItems.insert(armor);
    }
  }

  // Consider only armor items affected by the new variant
  for (auto &armor : armorItems) {
    if (!newVariant.WouldReplaceAny(armor)) {
      armorItems.erase(armor);
    }
  }

  if (armorItems.empty())
    return;

  // Remove previous overrides that affect the same armor items
  auto [elem, inserted] = _variantOverrides.try_emplace(
      a_actor->GetFormID(), std::unordered_set<std::string>());

  auto &overrides = elem->second;

  for (auto &[name, variant] : _variants) {
    if (&variant == &newVariant)
      continue;

    if (overrides.contains(name)) {
      for (auto &armor : armorItems) {
        if (variant.WouldReplaceAny(armor)) {
          overrides.erase(name);
          break;
        }
      }
    }
  }

  // Add the new override
  overrides.insert(a_variant);
  Ext::Actor::Update3DSafe(a_actor);
}

void DynamicArmorManager::ApplyVariant(RE::Actor *a_actor,
                                       const RE::TESObjectARMO *a_armor,
                                       const std::string &a_variant) {
  std::unique_lock lock(_stateMutex);
  ClearArmorAddonResolutionCache();
  // Find variant definition
  auto it = _variants.find(a_variant);
  if (it == _variants.end())
    return;

  auto &newVariant = it->second;

  if (!a_actor->GetWornArmor(a_armor->GetFormID()))
    return;

  if (!newVariant.WouldReplaceAny(a_armor))
    return;

  // Remove previous overrides that affect the armor
  auto [elem, inserted] = _variantOverrides.try_emplace(
      a_actor->GetFormID(), std::unordered_set<std::string>());

  auto &overrides = elem->second;

  for (auto &[name, variant] : _variants) {
    if (&variant == &newVariant)
      continue;

    if (overrides.contains(name)) {
      if (variant.WouldReplaceAny(a_armor)) {
        overrides.erase(name);
      }
    }
  }

  // Add the new override
  overrides.insert(a_variant);
  Ext::Actor::Update3DSafe(a_actor);
}

void DynamicArmorManager::ResetVariant(RE::Actor *a_actor,
                                       const RE::TESObjectARMO *a_armor) {
  std::unique_lock lock(_stateMutex);
  ClearArmorAddonResolutionCache();
  auto it = _variantOverrides.find(a_actor->GetFormID());
  if (it == _variantOverrides.end())
    return;

  auto &overrides = it->second;

  for (auto &[name, variant] : _variants) {
    if (overrides.contains(name)) {
      if (variant.WouldReplaceAny(a_armor)) {
        overrides.erase(name);
      }
    }
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
