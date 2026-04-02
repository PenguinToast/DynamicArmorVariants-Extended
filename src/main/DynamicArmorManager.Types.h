#pragma once

#include <functional>
#include <optional>

struct DynamicArmorResolvedAddonVisit {
  RE::TESObjectARMO *Armor{nullptr};
  RE::TESObjectARMA *ArmorAddon{nullptr};
  BipedObjectSlot EffectiveMask{BipedObjectSlot::kNone};
  std::optional<BipedObjectSlot> InitOverrideMask{std::nullopt};
};

using DynamicArmorResolvedAddonVisitor =
    std::function<void(const DynamicArmorResolvedAddonVisit &)>;
