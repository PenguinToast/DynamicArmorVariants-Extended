#include "DynamicArmorManager.h"
#include "DynamicArmorManager.Internal.h"

#include "Ext/IItemChangeVisitor.h"
#include "Ext/InventoryChanges.h"

#include <array>
#include <set>

namespace dave::detail {
class EquippedArmorVisitor : public Ext::IItemChangeVisitor {
public:
  std::uint32_t Visit(RE::InventoryEntryData *a_entryData) override {
    if (!a_entryData || !a_entryData->IsWorn()) {
      return 1;
    }

    auto *armor = a_entryData->object
                      ? a_entryData->object->As<RE::TESObjectARMO>()
                      : nullptr;
    if (!armor || count_ >= armors_.size()) {
      return 1;
    }

    armors_[count_++] = armor;
    return 1;
  }

  [[nodiscard]] auto begin() const { return armors_.begin(); }
  [[nodiscard]] auto end() const {
    return armors_.begin() + static_cast<std::ptrdiff_t>(count_);
  }

private:
  std::array<RE::TESObjectARMO *, 32> armors_{};
  std::size_t count_{0};
};

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

auto HasVariantsLocked(const DynamicArmorManagerState &a_state,
                       RE::TESObjectARMO *a_armor) -> bool {
  if (!a_armor) {
    return false;
  }

  for (auto *armorAddon : a_armor->armorAddons) {
    if (!armorAddon) {
      continue;
    }

    for (auto &[_, variant] : a_state.variants) {
      if (variant.WouldReplace(armorAddon)) {
        return true;
      }
    }
  }

  return false;
}

auto ReplacementAddonsMatch(const std::optional<ArmorVariant::AddonList> &a_lhs,
                            const std::optional<ArmorVariant::AddonList> &a_rhs)
    -> bool {
  if (!a_lhs.has_value() || !a_rhs.has_value()) {
    return a_lhs.has_value() == a_rhs.has_value();
  }

  if (a_lhs->size() != a_rhs->size()) {
    return false;
  }

  for (std::size_t i = 0; i < a_lhs->size(); ++i) {
    if ((*a_lhs)[i].Armor != (*a_rhs)[i].Armor ||
        (*a_lhs)[i].ArmorAddon != (*a_rhs)[i].ArmorAddon) {
      return false;
    }
  }

  return true;
}

auto EquivalentRenderState(const ArmorAddonResolutionCache::Value &a_lhs,
                           const ArmorAddonResolutionCache::Value &a_rhs)
    -> bool {
  return a_lhs.ActiveVariant == a_rhs.ActiveVariant &&
         a_lhs.SourceAddonMatchesRace == a_rhs.SourceAddonMatchesRace &&
         ReplacementAddonsMatch(a_lhs.ResolvedAddonList,
                                a_rhs.ResolvedAddonList);
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

    if (auto armor = a_actor->GetWornArmor(slot)) {
      if (!dave::detail::HasVariantsLocked(state, armor)) {
        continue;
      }

      if (resultSet.insert(armor).second) {
        resultVector.push_back(armor);
      }
    }
  }

  return resultVector;
}

auto DynamicArmorManager::ResolveEquippedArmorVariants(RE::Actor *a_actor) const
    -> EquippedVariantResolutionStats {
  EquippedVariantResolutionStats stats{};
  if (!a_actor) {
    return stats;
  }

  auto &state = *state_;
  std::unique_lock lock(state.mutex);

  dave::detail::EquippedArmorVisitor equippedArmorVisitor{};
  if (auto *inventoryChanges = a_actor->GetInventoryChanges()) {
    Ext::InventoryChanges::Accept(inventoryChanges, &equippedArmorVisitor);
  }

  for (auto *armor : equippedArmorVisitor) {
    for (auto *armorAddon : armor->armorAddons) {
      if (!armorAddon) {
        continue;
      }
      const auto key = ArmorAddonResolutionCache::Key{
          .ActorFormID = a_actor->GetFormID(),
          .ArmorAddonFormID = armorAddon->GetFormID()};

      auto uncachedResolution =
          dave::detail::BuildArmorAddonResolution(state, a_actor, armorAddon);
      const auto previousResolution =
          state.armorAddonResolutionCache.PeekIgnoringTtl(key);
      const auto resolutionChanged =
          !previousResolution.has_value() ||
          !dave::detail::EquivalentRenderState(previousResolution->get(),
                                               uncachedResolution);
      state.armorAddonResolutionCache.UpsertIgnoringTtl(
          key, std::move(uncachedResolution));

      if (resolutionChanged) {
        stats.Changed = true;
      }
    }
  }
  return stats;
}

auto DynamicArmorManager::HasActiveArmorVariant(
    RE::Actor *a_actor, RE::TESObjectARMO *a_armor) const -> bool {
  if (!a_actor || !a_armor) {
    return false;
  }

  auto &state = *state_;
  std::unique_lock lock(state.mutex);

  for (auto *armorAddon : a_armor->armorAddons) {
    if (!armorAddon) {
      continue;
    }

    if (dave::detail::GetOrBuildArmorAddonResolution(state, a_actor, armorAddon)
            .ActiveVariant) {
      return true;
    }
  }

  return false;
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
