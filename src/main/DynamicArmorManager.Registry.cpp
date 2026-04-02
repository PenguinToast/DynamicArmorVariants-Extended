#include "DynamicArmorManager.h"
#include "DynamicArmorManager.Internal.h"

namespace dave::detail {
void RebuildLinkedVariantsIndexLocked(DynamicArmorManagerState &a_state) {
  a_state.linkedVariantsByTarget.clear();

  for (auto &[name, variant] : a_state.variants) {
    if (variant.Linked.empty()) {
      continue;
    }

    a_state.linkedVariantsByTarget[variant.Linked].push_back(
        std::addressof(variant));
  }

  a_state.variantCandidatesLru.clear();
  a_state.variantCandidatesByArmorAddon.clear();
}
} // namespace dave::detail

auto DynamicArmorManager::GetSingleton() -> DynamicArmorManager * {
  static DynamicArmorManager singleton{};
  return std::addressof(singleton);
}

DynamicArmorManager::DynamicArmorManager()
    : state_(std::make_unique<dave::detail::DynamicArmorManagerState>()) {
  state_->refreshVariantCacheCapacity = Settings::Get().refreshVariantCacheCapacity;
  state_->refreshVariantCacheTtl = Settings::Get().refreshVariantCacheTtl;
  state_->armorAddonResolutionCache = ArmorAddonResolutionCache(
      state_->refreshVariantCacheCapacity,
      state_->refreshVariantCacheTtl);
}

DynamicArmorManager::~DynamicArmorManager() = default;

void DynamicArmorManager::RegisterArmorVariant(std::string_view a_name,
                                               ArmorVariant &&a_variant) {
  auto &state = *state_;
  std::unique_lock lock(state.mutex);
  dave::detail::ClearArmorAddonResolutionCache(state);
  auto [it, inserted] =
      state.variants.try_emplace(std::string(a_name), std::move(a_variant));

  if (inserted) {
    dave::detail::RebuildLinkedVariantsIndexLocked(state);
    return;
  }

  auto &variant = it.value();

  if (!a_variant.Linked.empty()) {
    variant.Linked = a_variant.Linked;
  }

  if (!a_variant.DisplayName.empty()) {
    variant.DisplayName = a_variant.DisplayName;
  }

  if (a_variant.HasExplicitPriority) {
    variant.Priority = a_variant.Priority;
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

  dave::detail::RebuildLinkedVariantsIndexLocked(state);
}

void DynamicArmorManager::ReplaceArmorVariant(std::string_view a_name,
                                              ArmorVariant &&a_variant) {
  auto &state = *state_;
  std::unique_lock lock(state.mutex);
  dave::detail::ClearArmorAddonResolutionCache(state);
  state.variants.insert_or_assign(std::string(a_name), std::move(a_variant));
  dave::detail::RebuildLinkedVariantsIndexLocked(state);
}

auto DynamicArmorManager::DeleteArmorVariant(std::string_view a_name) -> bool {
  auto &state = *state_;
  std::unique_lock lock(state.mutex);
  dave::detail::ClearArmorAddonResolutionCache(state);
  const auto erased = state.variants.erase(std::string(a_name)) > 0;
  state.conditions.erase(std::string(a_name));

  for (auto actorIt = state.variantOverrides.begin();
       actorIt != state.variantOverrides.end();) {
    dave::detail::RemoveOverrideLocked(actorIt->second, a_name);
    if (actorIt->second.empty()) {
      actorIt = state.variantOverrides.erase(actorIt);
    } else {
      ++actorIt;
    }
  }

  dave::detail::RebuildLinkedVariantsIndexLocked(state);
  return erased;
}

void DynamicArmorManager::SetCondition(
    std::string_view a_state,
    const std::shared_ptr<RE::TESCondition> &a_condition) {
  auto &state = *state_;
  std::unique_lock lock(state.mutex);
  dave::detail::ClearArmorAddonResolutionCache(state);
  state.conditions.insert_or_assign(std::string(a_state), a_condition);
}

void DynamicArmorManager::ClearCondition(std::string_view a_state) {
  auto &state = *state_;
  std::unique_lock lock(state.mutex);
  dave::detail::ClearArmorAddonResolutionCache(state);
  state.conditions.erase(std::string(a_state));
}
