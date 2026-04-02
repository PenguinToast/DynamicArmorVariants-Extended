#include "DynamicArmorManager.Internal.h"
#include "Settings.h"

#include "Ext/TESObjectARMA.h"

#include <optional>

namespace {
using SlotMask = std::uint32_t;
enum class BranchMaskMode {
  LegacyResolvedMask,
  OwnershipMask,
};

auto GetBranchMaskMode() -> BranchMaskMode {
  return Settings::Get().useOwnershipBasedArmorMasks
             ? BranchMaskMode::OwnershipMask
             : BranchMaskMode::LegacyResolvedMask;
}

auto UsesOwnershipMasks(const BranchMaskMode a_mode) -> bool {
  return a_mode == BranchMaskMode::OwnershipMask;
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

auto BuildResolvedBranchVisit(
    dave::detail::DynamicArmorManagerState &a_state,
    RE::TESObjectARMO *a_defaultArmor, RE::TESObjectARMA *a_sourceArmorAddon,
    const dave::detail::ArmorSlotContributionMap *a_sourceContributionMap,
    const ArmorVariant::ReplacementAddon &a_resolvedAddon,
    const BranchMaskMode a_mode) -> DynamicArmorResolvedAddonVisit {
  auto *resolvedArmor =
      a_resolvedAddon.Armor ? a_resolvedAddon.Armor : a_defaultArmor;
  auto effectiveMask =
      a_resolvedAddon.ArmorAddon->bipedModelData.bipedObjectSlots.get();

  if (UsesOwnershipMasks(a_mode)) {
    if (a_resolvedAddon.Armor) {
      const auto &replacementContributionMap =
          dave::detail::GetOrBuildArmorSlotContributionMap(a_state,
                                                           a_resolvedAddon.Armor);
      effectiveMask = dave::detail::FindOwnershipMaskForAddon(
          replacementContributionMap, a_resolvedAddon.ArmorAddon);
    } else if (a_sourceContributionMap) {
      effectiveMask = dave::detail::FindOwnershipMaskForAddon(
          *a_sourceContributionMap, a_sourceArmorAddon);
    }
  } else if (a_resolvedAddon.Armor) {
    effectiveMask = a_resolvedAddon.Armor->bipedModelData.bipedObjectSlots.get();
  }

  const auto overrideMask = UsesOwnershipMasks(a_mode) && resolvedArmor &&
                                    resolvedArmor->bipedModelData
                                            .bipedObjectSlots.get() !=
                                        effectiveMask
                                ? std::optional{effectiveMask}
                                : std::nullopt;

  return DynamicArmorResolvedAddonVisit{
      .Armor = resolvedArmor,
      .ArmorAddon = a_resolvedAddon.ArmorAddon,
      .EffectiveMask = effectiveMask,
      .InitOverrideMask = overrideMask,
  };
}

} // namespace

auto dave::detail::BuildResolvedCoverageMask(
    DynamicArmorManagerState &a_state, RE::Actor *a_actor,
    RE::TESObjectARMO *a_armor, bool *a_hasActiveVariant,
    ArmorVariant::OverrideOption *a_overrideOption) -> BipedObjectSlot {
  const auto branchMaskMode = GetBranchMaskMode();
  SlotMask resolvedMask = 0;
  auto hasActiveVariant = false;
  auto overrideOption = ArmorVariant::OverrideOption::None;
  const auto *contributionMap =
      UsesOwnershipMasks(branchMaskMode)
          ? std::addressof(GetOrBuildArmorSlotContributionMap(a_state, a_armor))
          : nullptr;
  const auto race = a_actor ? a_actor->GetRace() : nullptr;

  const auto visitSourceArmorAddon = [&](RE::TESObjectARMA *a_sourceArmorAddon) {
    // Worn-mask coverage must only include source addons that can actually be
    // selected for this actor. Armors often carry multiple race-specific ARMA
    // records for the same visual branch; if we aggregate every source addon,
    // inactive race branches can incorrectly keep head/hair bits occupied.
    if (race && a_sourceArmorAddon &&
        !Ext::TESObjectARMA::HasRace(a_sourceArmorAddon, race)) {
      return;
    }

    const auto &resolution =
        GetOrBuildArmorAddonResolution(a_state, a_actor, a_sourceArmorAddon);

    if (resolution.ActiveVariant) {
      hasActiveVariant = true;
      if (util::to_underlying(resolution.ActiveVariant->OverrideHead) >
          util::to_underlying(overrideOption)) {
        overrideOption = resolution.ActiveVariant->OverrideHead;
      }
    }

    VisitResolvedArmorAddons(
        a_state, a_armor, a_sourceArmorAddon, contributionMap, resolution,
        [&resolvedMask](const DynamicArmorResolvedAddonVisit &a_visit) {
          resolvedMask |= ToSlotMask(a_visit.EffectiveMask);
        });
  };

  if (UsesOwnershipMasks(branchMaskMode)) {
    for (const auto &source : contributionMap->Sources) {
      visitSourceArmorAddon(source.ArmorAddon);
    }
  } else {
    for (auto *armorAddon : a_armor->armorAddons) {
      visitSourceArmorAddon(armorAddon);
    }
  }

  if (a_hasActiveVariant) {
    *a_hasActiveVariant = hasActiveVariant;
  }
  if (a_overrideOption) {
    *a_overrideOption = overrideOption;
  }
  return ToBipedObjectSlot(resolvedMask);
}

auto dave::detail::BuildArmorSlotContributionMap(RE::TESObjectARMO *a_armor)
    -> ArmorSlotContributionMap {
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
    const auto sourceMask =
        armorAddon->bipedModelData.bipedObjectSlots.underlying();
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

void dave::detail::VisitResolvedArmorAddons(
    DynamicArmorManagerState &a_state, RE::TESObjectARMO *a_defaultArmor,
    RE::TESObjectARMA *a_sourceArmorAddon,
    const ArmorSlotContributionMap *a_sourceContributionMap,
    const ArmorAddonResolutionCache::Value &a_resolution,
    const DynamicArmorResolvedAddonVisitor &a_visit) {
  const auto branchMaskMode = GetBranchMaskMode();

  if (!a_sourceArmorAddon) {
    return;
  }

  if (!a_resolution.ActiveVariant) {
    a_visit(BuildResolvedBranchVisit(
        a_state, a_defaultArmor, a_sourceArmorAddon, a_sourceContributionMap,
        ArmorVariant::ReplacementAddon{.Armor = nullptr,
                                       .ArmorAddon = a_sourceArmorAddon},
        branchMaskMode));
    return;
  }

  if (!a_resolution.ResolvedAddonList.has_value()) {
    return;
  }

  for (const auto &resolvedAddon : *a_resolution.ResolvedAddonList) {
    if (!resolvedAddon.ArmorAddon) {
      continue;
    }

    a_visit(BuildResolvedBranchVisit(a_state, a_defaultArmor, a_sourceArmorAddon,
                                     a_sourceContributionMap, resolvedAddon,
                                     branchMaskMode));
  }
}

auto dave::detail::FindOwnershipMaskForAddon(
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
