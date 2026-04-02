#include "DynamicArmorManager.Internal.h"

#include "Ext/TESObjectARMA.h"

#include <optional>

namespace {
using SlotMask = std::uint32_t;

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

auto dave::detail::BuildResolvedCoverageMask(
    DynamicArmorManagerState &a_state, RE::Actor *a_actor,
    RE::TESObjectARMO *a_armor, bool *a_hasActiveVariant,
    ArmorVariant::OverrideOption *a_overrideOption) -> BipedObjectSlot {
  SlotMask resolvedMask = 0;
  auto hasActiveVariant = false;
  auto overrideOption = ArmorVariant::OverrideOption::None;
  const auto &contributionMap = GetOrBuildArmorSlotContributionMap(a_state, a_armor);
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
        GetOrBuildArmorAddonResolution(a_state, a_actor, source.ArmorAddon);

    if (resolution.ActiveVariant) {
      hasActiveVariant = true;
      if (util::to_underlying(resolution.ActiveVariant->OverrideHead) >
          util::to_underlying(overrideOption)) {
        overrideOption = resolution.ActiveVariant->OverrideHead;
      }
    }

    VisitResolvedArmorAddons(
        a_state, a_armor, source.ArmorAddon, contributionMap, resolution,
        [&resolvedMask](const DynamicArmorResolvedAddonVisit &a_visit) {
          resolvedMask |= ToSlotMask(a_visit.EffectiveMask);
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
    const ArmorSlotContributionMap &a_sourceContributionMap,
    const ArmorAddonResolutionCache::Value &a_resolution,
    const DynamicArmorResolvedAddonVisitor &a_visit) {
  if (!a_sourceArmorAddon) {
    return;
  }

  if (!a_resolution.ActiveVariant) {
    const auto effectiveMask =
        FindOwnershipMaskForAddon(a_sourceContributionMap, a_sourceArmorAddon);
    const auto overrideMask =
        a_defaultArmor &&
                a_defaultArmor->bipedModelData.bipedObjectSlots.get() !=
                    effectiveMask
            ? std::optional{effectiveMask}
            : std::nullopt;
    a_visit(DynamicArmorResolvedAddonVisit{
        .Armor = a_defaultArmor,
        .ArmorAddon = a_sourceArmorAddon,
        .EffectiveMask = effectiveMask,
        .InitOverrideMask = overrideMask,
    });
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
    auto effectiveMask = resolvedAddon.ArmorAddon->bipedModelData.bipedObjectSlots.get();
    if (resolvedAddon.Armor) {
      const auto &replacementContributionMap =
          GetOrBuildArmorSlotContributionMap(a_state, resolvedAddon.Armor);
      effectiveMask = FindOwnershipMaskForAddon(replacementContributionMap,
                                                resolvedAddon.ArmorAddon);
    }

    const auto overrideMask =
        resolvedArmor &&
                resolvedArmor->bipedModelData.bipedObjectSlots.get() !=
                    effectiveMask
            ? std::optional{effectiveMask}
            : std::nullopt;
    a_visit(DynamicArmorResolvedAddonVisit{
        .Armor = resolvedArmor,
        .ArmorAddon = resolvedAddon.ArmorAddon,
        .EffectiveMask = effectiveMask,
        .InitOverrideMask = overrideMask,
    });
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
