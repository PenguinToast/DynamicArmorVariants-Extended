#pragma once

struct ArmorVariant {
  struct ReplacementAddon {
    RE::TESObjectARMO *Armor{nullptr};
    RE::TESObjectARMA *ArmorAddon{nullptr};
  };

  using AddonList = std::vector<ReplacementAddon>;
  using FormMap = std::unordered_map<const RE::TESObjectARMA *, AddonList>;
  using SlotMap = std::map<BipedObject, AddonList>;

  enum class OverrideOption {
    Undefined,
    None,
    ShowAll,
    ShowHead,
    HideHair,
    HideAll,
  };

  ArmorVariant() = default;

  auto HasSlotOverrides() const -> bool {
    return OverrideHead != OverrideOption::Undefined &&
           OverrideHead != OverrideOption::None;
  }

  auto GetAddonList(const RE::TESObjectARMA *a_armorAddon) const
      -> const AddonList *;
  auto WouldReplace(const RE::TESObjectARMA *a_armorAddon) const -> bool;
  auto WouldReplaceAny(const RE::TESObjectARMO *a_armor) const -> bool;

  std::string Linked;
  std::string DisplayName;
  std::int32_t Priority{0};
  bool HasExplicitPriority{false};
  OverrideOption OverrideHead;
  FormMap ReplaceByForm;
  SlotMap ReplaceBySlot;
};
