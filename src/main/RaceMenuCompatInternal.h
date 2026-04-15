#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace RaceMenuCompat::detail {
struct HookSite {
  std::string_view Name;
  std::uintptr_t Rva{0};
  std::size_t PatchSize{0};
  std::span<const std::uint8_t> Prologue;
};

struct HookLayout {
  std::string_view Label;
  const wchar_t *ModuleName;
  REL::Version Version;
  std::uint32_t TimeDateStamp{0};
  HookSite GetSkinFormSite;
  HookSite VisitAllWornItemsSite;
  HookSite VisitAllWornItemsSlotMatchSite;
  std::uint8_t VisitAllWornItemsSlotMatchMaskStackOffset{0};
  enum class EntryRegister : std::uint8_t {
    Rsi = 1,
    Rdi = 2,
    R15 = 3,
  } VisitAllWornItemsSlotMatchEntryRegister{};
  HookSite VisitArmorAddonSite;
};

using VisitArmorAddonFunc =
    void(RE::Actor *, RE::TESObjectARMO *, RE::TESObjectARMA *,
         std::function<void(bool, RE::NiNode *, RE::NiAVObject *)>);
using VisitAllWornItemsFunc = void(
    RE::Actor *, std::uint32_t, std::function<void(RE::InventoryEntryData *)>);
using EffectiveArmorMaskCacheEntry =
    std::pair<RE::TESObjectARMO *, std::uint32_t>;
using ResolvedArmorAddonPair =
    std::pair<RE::TESObjectARMO *, RE::TESObjectARMA *>;

extern bool g_installed;
extern VisitAllWornItemsFunc *g_originalVisitAllWornItems;
extern thread_local RE::Actor *g_currentVisitAllWornItemsActor;
extern thread_local std::vector<EffectiveArmorMaskCacheEntry>
    g_currentVisitAllWornItemsEffectiveMaskCache;

struct ScopedVisitAllWornItemsContext final {
  explicit ScopedVisitAllWornItemsContext(RE::Actor *a_actor);
  ~ScopedVisitAllWornItemsContext();

  ScopedVisitAllWornItemsContext(const ScopedVisitAllWornItemsContext &) =
      delete;
  auto operator=(const ScopedVisitAllWornItemsContext &)
      -> ScopedVisitAllWornItemsContext & = delete;

  RE::Actor *previousActor{nullptr};
  std::vector<EffectiveArmorMaskCacheEntry> previousEffectiveMaskCache;
};

auto Hook_GetSkinForm(RE::Actor *a_actor, std::uint32_t a_mask)
    -> RE::TESForm *;
void Hook_VisitAllWornItems(
    RE::Actor *a_actor, std::uint32_t a_mask,
    std::function<void(RE::InventoryEntryData *)> a_functor);
void Hook_VisitArmorAddon(
    RE::Actor *a_actor, RE::TESObjectARMO *a_armor,
    RE::TESObjectARMA *a_armorAddon,
    std::function<void(bool, RE::NiNode *, RE::NiAVObject *)> a_functor);
auto DoesCurrentVisitItemMatchSlotMask(std::uint32_t a_mask,
                                       RE::InventoryEntryData *a_entryData)
    -> bool;
} // namespace RaceMenuCompat::detail
