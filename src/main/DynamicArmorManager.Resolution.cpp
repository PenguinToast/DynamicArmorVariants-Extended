#include "DynamicArmorManager.h"

#include "Ext/Actor.h"
#include "Ext/TESObjectARMA.h"

#include <bit>
#include <limits>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace {
using SlotMask = std::uint32_t;

auto BuildConditionPriority(const ArmorVariant &a_variant) -> std::int64_t {
  return static_cast<std::int64_t>(a_variant.Priority);
}

auto FilterAddonListForRace(const ArmorVariant::AddonList *a_addonList,
                            RE::TESRace *a_race)
    -> std::optional<ArmorVariant::AddonList> {
  if (!a_addonList) {
    return std::nullopt;
  }

  ArmorVariant::AddonList filtered;
  filtered.reserve(a_addonList->size());
  for (const auto &entry : *a_addonList) {
    if (!entry.ArmorAddon || !a_race ||
        !Ext::TESObjectARMA::HasRace(entry.ArmorAddon, a_race)) {
      continue;
    }

    filtered.push_back(entry);
  }

  return filtered;
}

constexpr auto ToSlotMask(const BipedObjectSlot a_slot) -> SlotMask {
  return static_cast<SlotMask>(std::to_underlying(a_slot));
}

constexpr auto ToBipedObjectSlot(const SlotMask a_mask) -> BipedObjectSlot {
  return static_cast<BipedObjectSlot>(a_mask);
}

constexpr SlotMask kHeadLikeMask =
    ToSlotMask(BipedObjectSlot::kHead) | ToSlotMask(BipedObjectSlot::kHair) |
    ToSlotMask(BipedObjectSlot::kLongHair) |
    ToSlotMask(BipedObjectSlot::kCirclet) | ToSlotMask(BipedObjectSlot::kEars);

constexpr SlotMask kBodyLikeMask =
    ToSlotMask(BipedObjectSlot::kBody) | ToSlotMask(BipedObjectSlot::kHands) |
    ToSlotMask(BipedObjectSlot::kForearms) | ToSlotMask(BipedObjectSlot::kFeet) |
    ToSlotMask(BipedObjectSlot::kCalves);

constexpr SlotMask kNeckLikeMask =
    ToSlotMask(BipedObjectSlot::kAmulet) |
    ToSlotMask(BipedObjectSlot::kModNeck) | ToSlotMask(BipedObjectSlot::kBody);

constexpr SlotMask kRingLikeMask =
    ToSlotMask(BipedObjectSlot::kRing) |
    ToSlotMask(BipedObjectSlot::kModFaceJewelry) |
    ToSlotMask(BipedObjectSlot::kCirclet) | ToSlotMask(BipedObjectSlot::kEars);

auto GetAffinityMaskForArmorOnlyBit(const SlotMask a_bit) -> SlotMask {
  switch (a_bit) {
  case ToSlotMask(BipedObjectSlot::kHead):
  case ToSlotMask(BipedObjectSlot::kHair):
  case ToSlotMask(BipedObjectSlot::kLongHair):
  case ToSlotMask(BipedObjectSlot::kCirclet):
  case ToSlotMask(BipedObjectSlot::kEars):
    return kHeadLikeMask;
  case ToSlotMask(BipedObjectSlot::kBody):
  case ToSlotMask(BipedObjectSlot::kHands):
  case ToSlotMask(BipedObjectSlot::kForearms):
  case ToSlotMask(BipedObjectSlot::kFeet):
  case ToSlotMask(BipedObjectSlot::kCalves):
    return kBodyLikeMask;
  case ToSlotMask(BipedObjectSlot::kAmulet):
  case ToSlotMask(BipedObjectSlot::kModNeck):
    return kNeckLikeMask;
  case ToSlotMask(BipedObjectSlot::kRing):
    return kRingLikeMask;
  default:
    return 0;
  }
}

} // namespace

void DynamicArmorManager::VisitArmorAddons(
    RE::Actor *a_actor, RE::TESObjectARMO *a_defaultArmor,
    RE::TESObjectARMA *a_armorAddon,
    const std::function<void(RE::TESObjectARMO *, RE::TESObjectARMA *,
                             BipedObjectSlot,
                             std::optional<BipedObjectSlot>)>
        &a_visit) const {
  std::unique_lock lock(_stateMutex);
  if (!a_armorAddon) {
    return;
  }

  if (!a_actor || !a_defaultArmor || Ext::Actor::IsSkin(a_actor, a_armorAddon)) {
    const auto initMask =
        a_defaultArmor ? a_defaultArmor->bipedModelData.bipedObjectSlots.get()
                       : a_armorAddon->bipedModelData.bipedObjectSlots.get();
    a_visit(a_defaultArmor, a_armorAddon, initMask, std::nullopt);
    return;
  }

  const auto &sourceContributionMap =
      GetOrBuildArmorSlotContributionMap(a_defaultArmor);
  const auto resolution = GetOrBuildArmorAddonResolution(a_actor, a_armorAddon);
  VisitResolvedArmorAddonsLocked(a_defaultArmor, a_armorAddon,
                                 sourceContributionMap, resolution, a_visit);
}

auto DynamicArmorManager::ShouldUseCustomInitWornArmor(
    RE::Actor *a_actor, RE::TESObjectARMO *a_armor) const -> bool {
  std::unique_lock lock(_stateMutex);
  if (!a_actor || !a_armor) {
    return false;
  }

  const auto race = a_actor->GetRace();
  if (!race) {
    return false;
  }

  for (auto *armorAddon : a_armor->armorAddons) {
    if (!armorAddon || !Ext::TESObjectARMA::HasRace(armorAddon, race)) {
      continue;
    }

    if (const auto resolution = GetOrBuildArmorAddonResolution(a_actor, armorAddon);
        resolution.ActiveVariant) {
      return true;
    }
  }

  return false;
}

auto DynamicArmorManager::GetBipedObjectSlots(RE::Actor *a_actor,
                                              RE::TESObjectARMO *a_armor) const
    -> BipedObjectSlot {
  std::unique_lock lock(_stateMutex);
  if (!a_armor) {
    return BipedObjectSlot::kNone;
  }

  auto slot = a_armor->bipedModelData.bipedObjectSlots;

  auto race = a_actor ? a_actor->GetRace() : nullptr;
  if (!race) {
    return slot.get();
  }

  if (Ext::Actor::IsSkin(a_actor, a_armor)) {
    return slot.get();
  }

  auto hasActiveVariant = false;
  auto overrideOption = ArmorVariant::OverrideOption::None;
  const auto resolvedMask =
      BuildResolvedCoverageMask(a_actor, a_armor, &hasActiveVariant,
                                &overrideOption);

  // Preserve vanilla armor-mask behavior when no active variant affects this
  // armor, because some armors define slots differently from their addons.
  if (hasActiveVariant) {
    slot = resolvedMask;
  }

  auto headSlot =
      static_cast<BipedObjectSlot>(1 << race->data.headObject.get());
  auto hairSlot =
      static_cast<BipedObjectSlot>(1 << race->data.hairObject.get());
  constexpr auto beardSlot = BipedObjectSlot::kModMouth;

  switch (overrideOption) {
  case ArmorVariant::OverrideOption::ShowAll:
    slot.reset(headSlot, hairSlot, beardSlot);
    break;
  case ArmorVariant::OverrideOption::ShowHead:
    slot.reset(headSlot);
    break;
  case ArmorVariant::OverrideOption::HideHair:
    slot.set(headSlot);
    break;
  case ArmorVariant::OverrideOption::HideAll:
    slot.set(headSlot, hairSlot, beardSlot);
    break;
  }

  return slot.get();
}

auto DynamicArmorManager::IsUsingVariantLocked(RE::Actor *a_actor,
                                               std::string_view a_state) const
    -> bool {
  return IsVariantOverrideLocked(a_actor, a_state) ||
         IsVariantConditionLocked(a_actor, a_state);
}

auto DynamicArmorManager::IsVariantOverrideLocked(
    RE::Actor *a_actor, std::string_view a_state) const -> bool {
  if (!a_actor) {
    return false;
  }

  const auto state = std::string(a_state);

  if (auto it = _variantOverrides.find(a_actor->GetFormID());
      it != _variantOverrides.end()) {
    return it->second.contains(state);
  }

  return false;
}

auto DynamicArmorManager::IsVariantConditionLocked(
    RE::Actor *a_actor, std::string_view a_state) const -> bool {
  if (!a_actor) {
    return false;
  }

  const auto state = std::string(a_state);
  if (auto it = _conditions.find(state); it != _conditions.end()) {
    auto &condition = it->second;
    return condition ? condition->IsTrue(a_actor, a_actor) : false;
  }

  return false;
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

auto DynamicArmorManager::GetOrBuildArmorSlotContributionMap(
    RE::TESObjectARMO *a_armor) const -> const ArmorSlotContributionMap & {
  static const ArmorSlotContributionMap kEmptyContributionMap{};
  if (!a_armor) {
    return kEmptyContributionMap;
  }

  const auto formID = a_armor->GetFormID();
  if (const auto it = _armorSlotContributionCache_.find(formID);
      it != _armorSlotContributionCache_.end()) {
    return it->second;
  }

  return _armorSlotContributionCache_
      .emplace(formID, BuildArmorSlotContributionMap(a_armor))
      .first->second;
}

auto DynamicArmorManager::BuildResolvedCoverageMask(
    RE::Actor *a_actor, RE::TESObjectARMO *a_armor, bool *a_hasActiveVariant,
    ArmorVariant::OverrideOption *a_overrideOption) const -> BipedObjectSlot {
  SlotMask resolvedMask = 0;
  auto hasActiveVariant = false;
  auto overrideOption = ArmorVariant::OverrideOption::None;
  const auto &contributionMap = GetOrBuildArmorSlotContributionMap(a_armor);
  const auto race = a_actor ? a_actor->GetRace() : nullptr;

  for (const auto &source : contributionMap.Sources) {
    // Worn-mask coverage must only include source addons that can actually be
    // selected for this actor. Armors often carry multiple race-specific ARMA
    // records for the same visual branch; if we aggregate every source addon,
    // inactive race branches can incorrectly keep head/hair bits occupied.
    if (race && source.ArmorAddon &&
        !Ext::TESObjectARMA::HasRace(source.ArmorAddon, race)) {
      continue;
    }

    const auto &resolution =
        GetOrBuildArmorAddonResolution(a_actor, source.ArmorAddon);

    if (resolution.ActiveVariant) {
      hasActiveVariant = true;
      if (util::to_underlying(resolution.ActiveVariant->OverrideHead) >
          util::to_underlying(overrideOption)) {
        overrideOption = resolution.ActiveVariant->OverrideHead;
      }
    }

    VisitResolvedArmorAddonsLocked(
        a_armor, source.ArmorAddon, contributionMap, resolution,
        [&resolvedMask](RE::TESObjectARMO *, RE::TESObjectARMA *,
                        const BipedObjectSlot a_initMask,
                        const std::optional<BipedObjectSlot> a_overrideMask) {
          (void)a_overrideMask;
          resolvedMask |= ToSlotMask(a_initMask);
        });
  }

  if (a_hasActiveVariant) {
    *a_hasActiveVariant = hasActiveVariant;
  }
  if (a_overrideOption) {
    *a_overrideOption = overrideOption;
  }
  return ToBipedObjectSlot(resolvedMask);
}

auto DynamicArmorManager::BuildArmorSlotContributionMap(
    RE::TESObjectARMO *a_armor) const -> ArmorSlotContributionMap {
  ArmorSlotContributionMap contributionMap{};
  if (!a_armor) {
    return contributionMap;
  }

  contributionMap.ArmorMask = a_armor->bipedModelData.bipedObjectSlots.get();
  SlotMask originalAddonUnion = 0;
  contributionMap.Sources.reserve(a_armor->armorAddons.size());

  for (auto *armorAddon : a_armor->armorAddons) {
    if (!armorAddon) {
      continue;
    }

    auto &source = contributionMap.Sources.emplace_back();
    source.ArmorAddon = armorAddon;
    const auto sourceMask = armorAddon->bipedModelData.bipedObjectSlots.underlying();
    source.OwnershipMask =
        ToBipedObjectSlot(sourceMask & ToSlotMask(contributionMap.ArmorMask));
    originalAddonUnion |= sourceMask;
  }

  auto unassignedArmorOnlyBits =
      ToSlotMask(contributionMap.ArmorMask) & ~originalAddonUnion;

  for (SlotMask bit = 1; bit != 0; bit <<= 1) {
    if ((unassignedArmorOnlyBits & bit) == 0) {
      continue;
    }

    const auto affinityMask = GetAffinityMaskForArmorOnlyBit(bit);
    auto assigned = false;

    if (affinityMask != 0) {
      for (auto &source : contributionMap.Sources) {
        const auto sourceMask = source.ArmorAddon
                                    ? source.ArmorAddon->bipedModelData
                                          .bipedObjectSlots.underlying()
                                    : 0;
        if ((sourceMask & affinityMask) == 0) {
          continue;
        }

        source.OwnershipMask =
            ToBipedObjectSlot(ToSlotMask(source.OwnershipMask) | bit);
        assigned = true;
      }
    }

    if (!assigned) {
      for (auto &source : contributionMap.Sources) {
        source.OwnershipMask =
            ToBipedObjectSlot(ToSlotMask(source.OwnershipMask) | bit);
        assigned = true;
      }
    }

    if (assigned) {
      unassignedArmorOnlyBits &= ~bit;
    }
  }

  return contributionMap;
}

void DynamicArmorManager::VisitResolvedArmorAddonsLocked(
    RE::TESObjectARMO *a_defaultArmor, RE::TESObjectARMA *a_sourceArmorAddon,
    const ArmorSlotContributionMap &a_sourceContributionMap,
    const ArmorAddonResolutionCache::Value &a_resolution,
    const std::function<void(RE::TESObjectARMO *, RE::TESObjectARMA *,
                             BipedObjectSlot,
                             std::optional<BipedObjectSlot>)> &a_visit) const {
  if (!a_sourceArmorAddon) {
    return;
  }

  if (!a_resolution.ActiveVariant) {
    const auto initMask =
        FindOwnershipMaskForAddon(a_sourceContributionMap, a_sourceArmorAddon);
    const auto overrideMask =
        a_defaultArmor &&
                a_defaultArmor->bipedModelData.bipedObjectSlots.get() != initMask
            ? std::optional{initMask}
            : std::nullopt;
    a_visit(a_defaultArmor, a_sourceArmorAddon, initMask, overrideMask);
    return;
  }

  if (!a_resolution.ResolvedAddonList.has_value()) {
    return;
  }

  for (const auto &resolvedAddon : *a_resolution.ResolvedAddonList) {
    if (!resolvedAddon.ArmorAddon) {
      continue;
    }

    auto *resolvedArmor =
        resolvedAddon.Armor ? resolvedAddon.Armor : a_defaultArmor;
    auto initMask = resolvedAddon.ArmorAddon->bipedModelData.bipedObjectSlots.get();
    if (resolvedAddon.Armor) {
      const auto &replacementContributionMap =
          GetOrBuildArmorSlotContributionMap(resolvedAddon.Armor);
      initMask = FindOwnershipMaskForAddon(replacementContributionMap,
                                           resolvedAddon.ArmorAddon);
    }

    const auto overrideMask =
        resolvedArmor &&
                resolvedArmor->bipedModelData.bipedObjectSlots.get() != initMask
            ? std::optional{initMask}
            : std::nullopt;
    a_visit(resolvedArmor, resolvedAddon.ArmorAddon, initMask, overrideMask);
  }
}

auto DynamicArmorManager::FindOwnershipMaskForAddon(
    const ArmorSlotContributionMap &a_map,
    const RE::TESObjectARMA *a_armorAddon) -> BipedObjectSlot {
  if (!a_armorAddon) {
    return BipedObjectSlot::kNone;
  }

  for (const auto &source : a_map.Sources) {
    if (source.ArmorAddon == a_armorAddon) {
      return source.OwnershipMask;
    }
  }

  return ToBipedObjectSlot(
      a_armorAddon->bipedModelData.bipedObjectSlots.underlying() &
      ToSlotMask(a_map.ArmorMask));
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
  std::unordered_map<std::string, std::uint64_t> overridePriorityByName;
  if (const auto overrideIt = _variantOverrides.find(a_actor->GetFormID());
      overrideIt != _variantOverrides.end()) {
    overridePriorityByName = overrideIt->second;
  }

  const ArmorVariant *activeVariant = nullptr;
  const ArmorVariant::AddonList *stateAddonList = nullptr;
  const ArmorVariant::AddonList *linkedAddonList = nullptr;
  std::string activeVariantState;
  std::int64_t activePriority = (std::numeric_limits<std::int64_t>::min)();

  linkedAddonLists.clear();

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

    std::optional<std::int64_t> candidatePriority;
    if (const auto overrideIt = overridePriorityByName.find(name);
        overrideIt != overridePriorityByName.end()) {
      candidatePriority =
          (1ll << 62) + static_cast<std::int64_t>(overrideIt->second);
    } else if (IsVariantConditionLocked(a_actor, name)) {
      candidatePriority = BuildConditionPriority(variant);
    }

    if (!candidatePriority.has_value()) {
      continue;
    }

    if (*candidatePriority < activePriority) {
      continue;
    }

    activePriority = *candidatePriority;
    activeVariantState = name;
    activeVariant = std::addressof(variant);
    stateAddonList = addonList;
    if (const auto it = linkedAddonLists.find(activeVariantState);
        it != linkedAddonLists.end()) {
      linkedAddonList = it->second;
    } else {
      linkedAddonList = nullptr;
    }
  }

  if (activeVariant) {
    resolution.ActiveVariant = activeVariant;
    resolution.ResolvedAddonList = FilterAddonListForRace(
        linkedAddonList ? linkedAddonList : stateAddonList, a_actor->GetRace());
  }

  return resolution;
}

void DynamicArmorManager::ClearArmorAddonResolutionCache() const {
  _armorAddonResolutionCache_.Clear();
}

void DynamicArmorManager::ClearArmorAddonResolutionCache(
    const RE::FormID a_actorFormID) const {
  std::unique_lock lock(_stateMutex);
  ClearArmorAddonResolutionCacheLocked(a_actorFormID);
}

void DynamicArmorManager::ClearArmorAddonResolutionCacheLocked(
    const RE::FormID a_actorFormID) const {
  _armorAddonResolutionCache_.ClearActor(a_actorFormID);
}
