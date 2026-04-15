#include "RaceMenuCompatInternal.h"

#include "DynamicArmorManager.h"
#include "Ext/IItemChangeVisitor.h"
#include "Ext/InventoryChanges.h"
#include "Ext/TESObjectARMA.h"

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <unordered_set>
#include <utility>

namespace RaceMenuCompat::detail {
namespace {
constexpr std::size_t kNodeNameBufferSize = 260;

using VisitArmorAddonCallback =
    std::function<void(bool, RE::NiNode *, RE::NiAVObject *)>;

auto GetArmorAddonNodeName(RE::Actor *a_actor, RE::TESObjectARMO *a_armor,
                           RE::TESObjectARMA *a_armorAddon)
    -> RE::BSFixedString {
  std::array<char, kNodeNameBufferSize> nodeName{};
  a_armorAddon->GetNodeName(nodeName.data(), a_actor, a_armor, -1.0f);
  return RE::BSFixedString{nodeName.data()};
}

auto IsMatchingBipedEntry(RE::TESObjectARMO *a_armor,
                          RE::TESObjectARMA *a_armorAddon,
                          const RE::BSFixedString &a_addonNodeName,
                          RE::TESForm *a_bipedArmor,
                          RE::TESObjectARMA *a_bipedArmorAddon,
                          RE::NiAVObject *a_object) -> bool {
  const auto armorMatch = a_bipedArmor &&
                          a_bipedArmor->GetFormType() == RE::FormType::Armor &&
                          a_bipedArmor->GetFormID() == a_armor->GetFormID();
  const auto addonMatch = a_bipedArmorAddon && a_bipedArmorAddon->GetFormID() ==
                                                   a_armorAddon->GetFormID();
  const auto directAddonMatch =
      a_bipedArmor && a_bipedArmor->GetFormType() == RE::FormType::Armature &&
      a_bipedArmor->GetFormID() == a_armorAddon->GetFormID();
  const auto nodeNameMatch = a_object && a_object->name == a_addonNodeName;

  return (armorMatch && addonMatch) || directAddonMatch || nodeNameMatch;
}

void InvokeArmorAddonCallback(VisitArmorAddonCallback &a_functor,
                              const bool a_isFirstPerson,
                              const RE::NiPointer<RE::NiNode> &a_rootNode,
                              const RE::NiPointer<RE::NiAVObject> &a_object) {
  if (!a_rootNode || !a_object) {
    return;
  }

  a_functor(a_isFirstPerson, a_rootNode.get(), a_object.get());
}

template <class Fn> void VisitBipedNodesSafe(RE::Actor *a_actor, Fn &&a_visit) {
  if (!a_actor) {
    return;
  }

  for (std::uint32_t i = 0; i < 2; ++i) {
    const auto isFirstPerson = i == 1;
    const auto &biped = a_actor->GetBiped(isFirstPerson);
    auto *weightModel = biped.get();
    if (!weightModel) {
      continue;
    }

    // RaceMenu pairs biped objects with a separate GetNiRootNode(k) lookup.
    // Keep the root and part clones from the same live biped snapshot instead.
    RE::NiPointer<RE::NiNode> rootHolder{weightModel->root};
    auto visitObject = [&](RE::BIPOBJECT &a_objectData) {
      const auto &objectHolder = a_objectData.partClone;
      if (!objectHolder) {
        return;
      }

      a_visit(isFirstPerson, rootHolder, a_objectData.item, a_objectData.addon,
              objectHolder);
    };

    for (auto &objectData : weightModel->objects) {
      visitObject(objectData);
    }
    for (auto &objectData : weightModel->bufferedObjects) {
      visitObject(objectData);
    }
  }
}

template <class Fn>
void VisitSkeletalRootsSafe(RE::Actor *a_actor, Fn &&a_visit) {
  if (!a_actor) {
    return;
  }

  // CommonLib actor code treats the third/first-person skeleton roots as the
  // current Get3D(false/true) scene roots.
  std::array<RE::NiPointer<RE::NiNode>, 2> roots{};
  for (std::uint32_t i = 0; i < roots.size(); ++i) {
    if (auto *rootObject = a_actor->Get3D(i == 1); rootObject) {
      roots[i] = RE::NiPointer<RE::NiNode>{rootObject->AsNode()};
    }
  }

  if (roots[0] == roots[1]) {
    roots[1].reset();
  }

  for (std::uint32_t i = 0; i < roots.size(); ++i) {
    if (roots[i]) {
      a_visit(roots[i], i == 1);
    }
  }
}

void SafeVisitArmorAddon(RE::Actor *a_actor, RE::TESObjectARMO *a_armor,
                         RE::TESObjectARMA *a_armorAddon,
                         VisitArmorAddonCallback &a_functor) {
  if (!a_actor || !a_armor || !a_armorAddon || !a_functor) {
    return;
  }

  const auto addonNodeName =
      GetArmorAddonNodeName(a_actor, a_armor, a_armorAddon);
  std::unordered_set<RE::NiAVObject *> touched;

  VisitBipedNodesSafe(
      a_actor,
      [&](const bool a_isFirstPerson,
          const RE::NiPointer<RE::NiNode> &a_rootNode,
          RE::TESForm *a_bipedArmor, RE::TESObjectARMA *a_bipedArmorAddon,
          const RE::NiPointer<RE::NiAVObject> &a_object) {
        if (!a_rootNode || !a_object ||
            !IsMatchingBipedEntry(a_armor, a_armorAddon, addonNodeName,
                                  a_bipedArmor, a_bipedArmorAddon,
                                  a_object.get()) ||
            touched.contains(a_object.get())) {
          return;
        }

        touched.emplace(a_object.get());
        InvokeArmorAddonCallback(a_functor, a_isFirstPerson, a_rootNode,
                                 a_object);
      });

  VisitSkeletalRootsSafe(
      a_actor, [&](const RE::NiPointer<RE::NiNode> &a_rootNode,
                   const bool a_isFirstPerson) {
        RE::NiPointer<RE::NiAVObject> armorNode{
            a_rootNode ? a_rootNode->GetObjectByName(addonNodeName) : nullptr};
        if (!armorNode || !armorNode->parent) {
          return;
        }

        RE::NiPointer<RE::NiNode> parentHolder{armorNode->parent};
        if (!parentHolder) {
          return;
        }

        for (auto &childHolder : parentHolder->GetChildren()) {
          auto *child = childHolder.get();
          if (!child || child->name != addonNodeName ||
              touched.contains(child)) {
            continue;
          }

          touched.emplace(child);
          InvokeArmorAddonCallback(a_functor, a_isFirstPerson, a_rootNode,
                                   childHolder);
        }
      });
}
} // namespace
bool g_installed{false};
VisitAllWornItemsFunc *g_originalVisitAllWornItems{nullptr};
thread_local RE::Actor *g_currentVisitAllWornItemsActor{nullptr};
// The matcher patch only needs a small per-call cache, so a linear container is
// fine here and avoids heap-heavy hashing in a hot path.
thread_local std::vector<EffectiveArmorMaskCacheEntry>
    g_currentVisitAllWornItemsEffectiveMaskCache;

ScopedVisitAllWornItemsContext::ScopedVisitAllWornItemsContext(
    RE::Actor *a_actor)
    : previousActor(g_currentVisitAllWornItemsActor),
      previousEffectiveMaskCache(
          std::move(g_currentVisitAllWornItemsEffectiveMaskCache)) {
  g_currentVisitAllWornItemsActor = a_actor;
  g_currentVisitAllWornItemsEffectiveMaskCache.clear();
}

ScopedVisitAllWornItemsContext::~ScopedVisitAllWornItemsContext() {
  g_currentVisitAllWornItemsActor = previousActor;
  g_currentVisitAllWornItemsEffectiveMaskCache =
      std::move(previousEffectiveMaskCache);
}

auto GetWornExtraData(RE::InventoryEntryData *a_entryData)
    -> RE::ExtraDataList * {
  if (!a_entryData || !a_entryData->extraLists) {
    return nullptr;
  }

  for (auto *extraData : *a_entryData->extraLists) {
    if (!extraData) {
      continue;
    }

    if (extraData->HasType<RE::ExtraWorn>() ||
        extraData->HasType<RE::ExtraWornLeft>()) {
      return extraData;
    }
  }

  return nullptr;
}

auto TryGetEffectiveArmorSlotMask(RE::Actor *a_actor,
                                  RE::TESObjectARMO *a_armor)
    -> std::optional<std::uint32_t> {
  if (!a_actor || !a_armor) {
    return std::nullopt;
  }

  auto *manager = DynamicArmorManager::GetSingleton();
  if (!manager) {
    return std::nullopt;
  }

  return std::to_underlying(manager->GetBipedObjectSlots(a_actor, a_armor));
}

auto TryGetCachedEffectiveArmorSlotMaskForCurrentVisit(
    RE::TESObjectARMO *a_armor) -> std::optional<std::uint32_t> {
  if (!g_currentVisitAllWornItemsActor || !a_armor) {
    return std::nullopt;
  }

  const auto cacheIt =
      std::ranges::find(g_currentVisitAllWornItemsEffectiveMaskCache, a_armor,
                        &EffectiveArmorMaskCacheEntry::first);
  if (cacheIt != g_currentVisitAllWornItemsEffectiveMaskCache.end()) {
    return cacheIt->second;
  }

  const auto effectiveMask =
      TryGetEffectiveArmorSlotMask(g_currentVisitAllWornItemsActor, a_armor);
  if (effectiveMask) {
    g_currentVisitAllWornItemsEffectiveMaskCache.emplace_back(a_armor,
                                                              *effectiveMask);
  }

  return effectiveMask;
}

auto HasSlotMatch(const std::uint32_t a_slotMask, const std::uint32_t a_mask)
    -> bool {
  return (a_slotMask & a_mask) != 0;
}

auto TryGetOriginalSlotMask(RE::TESForm *a_form)
    -> std::optional<std::uint32_t> {
  if (!a_form) {
    return std::nullopt;
  }

  switch (a_form->GetFormType()) {
  case RE::FormType::Armor:
    return static_cast<const RE::TESObjectARMO *>(a_form)
        ->GetSlotMask()
        .underlying();
  case RE::FormType::Armature:
    return static_cast<const RE::TESObjectARMA *>(a_form)
        ->GetSlotMask()
        .underlying();
  default:
    return std::nullopt;
  }
}

auto DoesWornItemMatchSlotMask(RE::InventoryEntryData *a_entryData,
                               const std::uint32_t a_mask,
                               const auto &a_getEffectiveMask) -> bool {
  if (!a_entryData || !a_entryData->object) {
    return false;
  }

  if (auto *armor = a_entryData->object->As<RE::TESObjectARMO>()) {
    if (const auto effectiveMask = a_getEffectiveMask(armor)) {
      return HasSlotMatch(*effectiveMask, a_mask);
    }
  }

  if (const auto originalMask = TryGetOriginalSlotMask(a_entryData->object)) {
    return HasSlotMatch(*originalMask, a_mask);
  }

  return false;
}

auto DoesCurrentVisitItemMatchSlotMask(const std::uint32_t a_mask,
                                       RE::InventoryEntryData *a_entryData)
    -> bool {
  return DoesWornItemMatchSlotMask(
      a_entryData, a_mask, [](RE::TESObjectARMO *a_armor) {
        return TryGetCachedEffectiveArmorSlotMaskForCurrentVisit(a_armor);
      });
}

auto GetWornArmor(RE::InventoryEntryData *a_entryData) -> RE::TESObjectARMO * {
  if (!a_entryData || !GetWornExtraData(a_entryData)) {
    return nullptr;
  }

  return a_entryData->object ? a_entryData->object->As<RE::TESObjectARMO>()
                             : nullptr;
}

template <class Fn>
void VisitEffectiveWornArmorsForSlot(RE::Actor *a_actor, std::uint32_t a_mask,
                                     Fn &&a_visit) {
  if (!a_actor || a_mask == 0) {
    return;
  }

  auto *inventory = a_actor->GetInventoryChanges();
  auto *manager = DynamicArmorManager::GetSingleton();
  auto *race = a_actor->GetRace();
  if (!inventory || !manager || !race) {
    return;
  }

  using VisitFn = std::decay_t<Fn>;

  class EffectiveWornArmorVisitor final : public Ext::IItemChangeVisitor {
  public:
    EffectiveWornArmorVisitor(RE::Actor *a_actor, std::uint32_t a_mask,
                              RE::TESRace *a_race,
                              DynamicArmorManager *a_manager, VisitFn &a_visit)
        : actor(a_actor), mask(a_mask), race(a_race), manager(a_manager),
          visit(a_visit) {}

    std::uint32_t Visit(RE::InventoryEntryData *a_entryData) override {
      if (done) {
        return 1;
      }

      auto *armor = GetWornArmor(a_entryData);
      if (!armor) {
        return 1;
      }

      for (auto *armorAddon : armor->armorAddons) {
        if (!armorAddon || !Ext::TESObjectARMA::HasRace(armorAddon, race)) {
          continue;
        }

        manager->VisitArmorAddons(
            actor, armor, armorAddon, [this, a_entryData](const auto &a_visit) {
              if (done ||
                  (std::to_underlying(a_visit.EffectiveMask) & mask) == 0) {
                return;
              }

              done = !visit(a_entryData, a_visit);
            });
        if (done) {
          break;
        }
      }

      return 1;
    }

    RE::Actor *actor{nullptr};
    std::uint32_t mask{0};
    RE::TESRace *race{nullptr};
    DynamicArmorManager *manager{nullptr};
    VisitFn &visit;
    bool done{false};
  };

  EffectiveWornArmorVisitor visitor{a_actor, a_mask, race, manager, a_visit};
  Ext::InventoryChanges::Accept(inventory, std::addressof(visitor));
}

auto GetActorSkin(RE::Actor *a_actor) -> RE::TESForm * {
  if (!a_actor) {
    return nullptr;
  }

  auto *actorBase = a_actor->GetActorBase();
  auto *skin = actorBase ? actorBase->skin : nullptr;
  if (skin) {
    return skin;
  }

  auto *race = a_actor->GetRace();
  if (!race && actorBase) {
    race = actorBase->race;
  }

  return race ? race->skin : nullptr;
}

auto ResolveEffectiveArmorOwnerForSlot(RE::Actor *a_actor, std::uint32_t a_mask)
    -> RE::TESObjectARMO * {
  RE::TESObjectARMO *effectiveOwner{nullptr};
  VisitEffectiveWornArmorsForSlot(
      a_actor, a_mask,
      [&effectiveOwner](RE::InventoryEntryData *a_entryData,
                        const DynamicArmorResolvedAddonVisit &) {
        effectiveOwner = GetWornArmor(a_entryData);
        return false;
      });
  return effectiveOwner;
}

auto Hook_GetSkinForm(RE::Actor *a_actor, std::uint32_t a_mask)
    -> RE::TESForm * {
  if (!a_actor) {
    return nullptr;
  }

  if (auto *effectiveOwner =
          ResolveEffectiveArmorOwnerForSlot(a_actor, a_mask)) {
    return effectiveOwner;
  }

  return GetActorSkin(a_actor);
}

void Hook_VisitAllWornItems(
    RE::Actor *a_actor, std::uint32_t a_mask,
    std::function<void(RE::InventoryEntryData *)> a_functor) {
  if (!g_originalVisitAllWornItems) {
    return;
  }

  // Keep RaceMenu's own VisitAllWornItems implementation and callback ABI
  // intact; we only provide actor context for the internal MatchBySlot patch.
  const ScopedVisitAllWornItemsContext actorScope{a_actor};
  g_originalVisitAllWornItems(a_actor, a_mask, std::move(a_functor));
}

auto CollectResolvedVisualPairs(RE::Actor *a_actor, RE::TESObjectARMO *a_armor,
                                RE::TESObjectARMA *a_armorAddon)
    -> std::vector<ResolvedArmorAddonPair> {
  std::vector<ResolvedArmorAddonPair> resolved;
  resolved.reserve(4);

  auto *manager = DynamicArmorManager::GetSingleton();
  if (!manager) {
    return resolved;
  }

  manager->VisitArmorAddons(
      a_actor, a_armor, a_armorAddon,
      [&resolved](const DynamicArmorResolvedAddonVisit &a_visit) {
        if (!a_visit.Armor || !a_visit.ArmorAddon) {
          return;
        }

        const ResolvedArmorAddonPair pair{a_visit.Armor, a_visit.ArmorAddon};
        if (std::find(resolved.begin(), resolved.end(), pair) ==
            resolved.end()) {
          resolved.push_back(pair);
        }
      });

  return resolved;
}

void Hook_VisitArmorAddon(
    RE::Actor *a_actor, RE::TESObjectARMO *a_armor,
    RE::TESObjectARMA *a_armorAddon,
    std::function<void(bool, RE::NiNode *, RE::NiAVObject *)> a_functor) {
  if (!a_actor || !a_functor) {
    return;
  }

  // Keep the exact callback signature from RaceMenu's hook site. On Win64 the
  // hook and the original function both use the platform's standard x64 ABI,
  // and the shipped skee64.dll passes the callback payload through the
  // std::function invoke stub using ordinary pointer arguments.

  // RaceMenu's OverrideInterface::Impl_SetSkinProperties still gates source
  // addons with IsSlotMatch(...) before it reaches VisitArmorAddon(...). The
  // current hooks are enough for the Wet Function path we verified, but a niche
  // future case could still be blocked if the source ARMA mask does not match
  // while the resolved DAVE visual ARMA should. If that ever matters, patch
  // that internal IsSlotMatch decision directly too.
  auto resolvedPairs =
      CollectResolvedVisualPairs(a_actor, a_armor, a_armorAddon);

  if (resolvedPairs.empty()) {
    SafeVisitArmorAddon(a_actor, a_armor, a_armorAddon, a_functor);
    return;
  }

  for (auto &[resolvedArmor, resolvedAddon] : resolvedPairs) {
    SafeVisitArmorAddon(a_actor, resolvedArmor, resolvedAddon, a_functor);
  }
}
} // namespace RaceMenuCompat::detail
