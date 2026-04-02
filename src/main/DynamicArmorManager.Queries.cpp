#include "DynamicArmorManager.h"
#include "DynamicArmorManager.Internal.h"

#include <set>

namespace dave::detail {
auto GetVariantsLocked(const DynamicArmorManagerState &a_state,
                       RE::TESObjectARMO *a_armor) -> std::vector<std::string> {
  std::vector<std::string> result;

  if (!a_armor) {
    return result;
  }

  auto names = std::set<std::string>();
  auto linkedNames = std::set<std::string>();

  for (auto &armorAddon : a_armor->armorAddons) {
    for (auto &[name, variant] : a_state.variants) {
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
} // namespace dave::detail

auto DynamicArmorManager::GetVariants(RE::TESObjectARMO *a_armor) const
    -> std::vector<std::string> {
  const auto &state = *state_;
  std::shared_lock lock(state.mutex);
  return dave::detail::GetVariantsLocked(state, a_armor);
}

auto DynamicArmorManager::GetEquippedArmorsWithVariants(RE::Actor *a_actor)
    -> std::vector<RE::TESObjectARMO *> {
  const auto &state = *state_;
  std::shared_lock lock(state.mutex);
  std::vector<RE::TESObjectARMO *> resultVector;
  resultVector.reserve(32);

  std::unordered_set<RE::TESObjectARMO *> resultSet;
  resultSet.reserve(32);

  for (std::uint32_t i = 0; i < 32; i++) {
    auto slot = static_cast<BipedObjectSlot>(1 << i);

    std::vector<std::string> variants;
    if (auto armor = a_actor->GetWornArmor(slot)) {
      variants = dave::detail::GetVariantsLocked(state, armor);
      if (!variants.empty() && resultSet.insert(armor).second) {
        resultVector.push_back(armor);
      }
    }
  }

  return resultVector;
}

auto DynamicArmorManager::GetDisplayName(const std::string &a_variant) const
    -> std::string {
  const auto &state = *state_;
  std::shared_lock lock(state.mutex);
  if (auto it = state.variants.find(a_variant); it != state.variants.end()) {
    return it->second.DisplayName;
  }

  return ""s;
}
