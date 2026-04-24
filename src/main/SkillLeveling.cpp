#include "SkillLeveling.h"
#include "DynamicArmorManager.h"
#include "Ext/InventoryChanges.h"
#include "Patches.h"

#include <bit>

namespace {
using BipedObjectSlot = RE::BGSBipedObjectForm::BipedObjectSlot;

auto CountSkillArmorSlots(RE::TESObjectARMO *a_armor,
                          const BipedObjectSlot a_mask) -> std::uint32_t {
  if (!a_armor || a_armor->IsShield() ||
      static_cast<std::uint16_t>(a_armor->armorRating) == 0) {
    return 0;
  }

  const auto mask = static_cast<std::uint32_t>(util::to_underlying(a_mask));
  auto count = std::popcount(mask);
  if ((mask & util::to_underlying(BipedObjectSlot::kBody)) != 0) {
    ++count;
  }
  return count;
}

class ArmorVisitor final : public Ext::IItemChangeVisitor {
public:
  explicit ArmorVisitor(RE::Actor *a_actor) : actor_{a_actor} {}

  std::uint32_t Visit(RE::InventoryEntryData *a_entryData) override {
    if (!a_entryData || !a_entryData->IsWorn()) {
      return 1;
    }

    auto *armor = a_entryData->object
                      ? a_entryData->object->As<RE::TESObjectARMO>()
                      : nullptr;
    if (!armor) {
      return 1;
    }

    if (!hasActiveVariant_) {
      auto *manager = DynamicArmorManager::GetSingleton();
      if (manager->HasActiveArmorVariant(actor_, armor)) {
        hasActiveVariant_ = true;
      }
    }

    AddArmor(armor, armor->bipedModelData.bipedObjectSlots.get());
    return 1;
  }

  auto Commit(Patches::SkillLevelingVisitor &a_visitor) const -> bool {
    if (!hasActiveVariant_) {
      return false;
    }

    a_visitor.light = light_;
    a_visitor.heavy = heavy_;
    return true;
  }

private:
  void AddArmor(RE::TESObjectARMO *a_armor, const BipedObjectSlot a_mask) {
    if (!a_armor) {
      return;
    }

    const auto count = CountSkillArmorSlots(a_armor, a_mask);
    if (count == 0) {
      return;
    }

    if (a_armor->IsLightArmor()) {
      light_ += count;
    } else if (a_armor->IsHeavyArmor()) {
      heavy_ += count;
    }
  }

  RE::Actor *actor_{nullptr};
  std::uint32_t light_{0};
  std::uint32_t heavy_{0};
  bool hasActiveVariant_{false};
};
} // namespace

auto SkillLeveling::FixArmorCounts(
    RE::BipedAnim *a_biped, Patches::SkillLevelingVisitor *a_visitor) -> bool {
  if (!a_biped || !a_visitor) {
    return false;
  }

  const auto target = a_biped->actorRef.get();
  auto *actor = target ? target.get()->As<RE::Actor>() : nullptr;
  if (!actor) {
    return false;
  }

  auto *inventory = actor->GetInventoryChanges();
  if (!inventory) {
    return false;
  }

  ArmorVisitor visitor{actor};
  Ext::InventoryChanges::Accept(inventory, std::addressof(visitor));
  return visitor.Commit(*a_visitor);
}
