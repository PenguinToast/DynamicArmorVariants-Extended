#include "RaceMenuCompat.h"

#include "DynamicArmorManager.h"
#include "Ext/IItemChangeVisitor.h"
#include "Ext/InventoryChanges.h"
#include "Ext/TESObjectARMA.h"
#include "REL/Version.h"
#include "REX/W32/VERSION.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include <intrin.h>

namespace {
using namespace std::literals;

constexpr auto kSkee64ModuleName = L"skee64.dll";
constexpr auto kSkeeVRModuleName = L"skeevr.dll";

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
  HookSite VisitAllWornItemsMatchSite;
  std::uintptr_t VisitAllWornItemsMatchContinueRva{0};
  std::uintptr_t VisitAllWornItemsMatchMissRva{0};
  std::uint8_t VisitAllWornItemsMatchMaskStackOffset{0};
  enum class EntryRegister : std::uint8_t {
    Rsi = 1,
    Rdi = 2,
    R15 = 3,
  } VisitAllWornItemsMatchEntryRegister{};
  HookSite VisitArmorAddonSite;
};

constexpr std::array<std::uint8_t, 13> kGetSkinFormPrologue_0419_15{
    0x48, 0x89, 0x5C, 0x24, 0x20, 0x55, 0x48,
    0x83, 0xEC, 0x60, 0x48, 0x8B, 0xE9};
constexpr std::array<std::uint8_t, 14> kGetSkinFormPrologue_0419_10{
    0x48, 0x89, 0x74, 0x24, 0x18, 0x57, 0x41,
    0x56, 0x41, 0x57, 0x48, 0x83, 0xEC, 0x50};
constexpr std::array<std::uint8_t, 13> kGetSkinFormPrologue_0419_9{
    0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x48,
    0x83, 0xEC, 0x60, 0x4C, 0x8B, 0xE9};
constexpr std::array<std::uint8_t, 20> kGetSkinFormPrologue_0419_9_Audited{
    0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x6C, 0x24, 0x10,
    0x48, 0x89, 0x74, 0x24, 0x18, 0x48, 0x89, 0x7C, 0x24, 0x20};
constexpr std::array<std::uint8_t, 13> kGetSkinFormPrologue_0419_10_Audited{
    0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48,
    0x83, 0xEC, 0x30, 0x48, 0x8B, 0xF9};
constexpr std::array<std::uint8_t, 15> kGetSkinFormPrologue_0345_SE{
    0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x6C,
    0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18};
constexpr std::array<std::uint8_t, 13> kGetSkinFormPrologue_0345_VR{
    0x48, 0x89, 0x5C, 0x24, 0x18, 0x55, 0x48,
    0x83, 0xEC, 0x60, 0x48, 0x8B, 0xE9};

constexpr std::array<std::uint8_t, 14> kVisitAllWornItemsPrologue_0345_VR{
    0x48, 0x89, 0x5C, 0x24, 0x10, 0x55, 0x56,
    0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56};
constexpr std::array<std::uint8_t, 12> kVisitAllWornItemsPrologue_0345_SE{
    0x48, 0x89, 0x5C, 0x24, 0x10, 0x48,
    0x89, 0x6C, 0x24, 0x20, 0x56, 0x57};
constexpr std::array<std::uint8_t, 12> kVisitAllWornItemsPrologue_0419_9{
    0x48, 0x89, 0x5C, 0x24, 0x10, 0x48,
    0x89, 0x6C, 0x24, 0x20, 0x56, 0x57};
constexpr std::array<std::uint8_t, 13> kVisitAllWornItemsPrologue_0419_13_AE{
    0x41, 0x57, 0x48, 0x83, 0xEC, 0x50, 0x48,
    0x8B, 0x05, 0x2B, 0xE1, 0x14, 0x00};
constexpr std::array<std::uint8_t, 13> kVisitAllWornItemsPrologue_0419_13_GOG{
    0x41, 0x57, 0x48, 0x83, 0xEC, 0x50, 0x48,
    0x8B, 0x05, 0x4B, 0xE1, 0x14, 0x00};
constexpr std::array<std::uint8_t, 13> kVisitAllWornItemsPrologue_0419_14_GOG{
    0x41, 0x57, 0x48, 0x83, 0xEC, 0x50, 0x48,
    0x8B, 0x05, 0x4B, 0xE0, 0x14, 0x00};
constexpr std::array<std::uint8_t, 12> kVisitAllWornItemsPrologue_0419_15{
    0x48, 0x89, 0x5C, 0x24, 0x10, 0x55,
    0x56, 0x57, 0x41, 0x54, 0x41, 0x55};

constexpr std::array<std::uint8_t, 19> kVisitAllWornItemsMatchPrologue_0345_VR{
    0x48, 0x8B, 0x17, 0x48, 0x8D, 0x4C, 0x24, 0x40, 0x48, 0x8B,
    0x44, 0x24, 0x40, 0xFF, 0x10, 0x84, 0xC0, 0x74, 0x43};
constexpr std::array<std::uint8_t, 23> kVisitAllWornItemsMatchPrologue_0345_SE{
    0x49, 0x8B, 0x17, 0x48, 0x8D, 0x4C, 0x24, 0x28, 0x48, 0x8B,
    0x44, 0x24, 0x28, 0xFF, 0x10, 0x84, 0xC0, 0x0F, 0x84, 0xD5,
    0x00, 0x00, 0x00};
constexpr std::array<std::uint8_t, 23> kVisitAllWornItemsMatchPrologue_0419_9{
    0x49, 0x8B, 0x17, 0x48, 0x8D, 0x4C, 0x24, 0x28, 0x48, 0x8B,
    0x44, 0x24, 0x28, 0xFF, 0x10, 0x84, 0xC0, 0x0F, 0x84, 0xD5,
    0x00, 0x00, 0x00};
constexpr std::array<std::uint8_t, 23> kVisitAllWornItemsMatchPrologue_0419_13{
    0x48, 0x8B, 0x44, 0x24, 0x28, 0x49, 0x8B, 0x17, 0x48, 0x8D,
    0x4C, 0x24, 0x28, 0xFF, 0x10, 0x84, 0xC0, 0x0F, 0x84, 0xD5,
    0x00, 0x00, 0x00};
constexpr std::array<std::uint8_t, 19> kVisitAllWornItemsMatchPrologue_0419_15{
    0x48, 0x8B, 0x44, 0x24, 0x48, 0x48, 0x8B, 0x16, 0x48, 0x8D,
    0x4C, 0x24, 0x48, 0xFF, 0x10, 0x84, 0xC0, 0x74, 0x50};

constexpr std::array<std::uint8_t, 21> kVisitArmorAddonPrologue_0419_9{
    0x40, 0x55, 0x53, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56,
    0x41, 0x57, 0x48, 0x8D, 0xAC, 0x24, 0x98, 0xFE, 0xFF, 0xFF};
constexpr std::array<std::uint8_t, 19> kVisitArmorAddonPrologue_New_0419_13{
    0x40, 0x55, 0x53, 0x56, 0x57, 0x41, 0x54,
    0x41, 0x56, 0x41, 0x57, 0x48, 0x8D, 0xAC,
    0x24, 0xA0, 0xFE, 0xFF, 0xFF};
constexpr std::array<std::uint8_t, 19> kVisitArmorAddonPrologue_New_0419_15{
    0x40, 0x55, 0x53, 0x56, 0x57, 0x41, 0x54,
    0x41, 0x56, 0x41, 0x57, 0x48, 0x8D, 0xAC,
    0x24, 0xC0, 0xFE, 0xFF, 0xFF};
constexpr std::array<std::uint8_t, 12> kVisitArmorAddonPrologue_Old{
    0x48, 0x89, 0x5C, 0x24, 0x20, 0x55,
    0x56, 0x57, 0x48, 0x83, 0xEC, 0x60};
constexpr std::array<std::uint8_t, 20> kVisitArmorAddonPrologue_SE_0416{
    0x40, 0x53, 0x55, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41,
    0x56, 0x41, 0x57, 0x48, 0x81, 0xEC, 0xC8, 0x01, 0x00, 0x00};
constexpr auto MakeSite(std::string_view a_name, std::uintptr_t a_rva,
                        std::size_t a_patchSize,
                        std::span<const std::uint8_t> a_prologue) -> HookSite {
  return HookSite{
      .Name = a_name,
      .Rva = a_rva,
      .PatchSize = a_patchSize,
      .Prologue = a_prologue,
  };
}

constexpr std::array kHookLayouts{
    HookLayout{
        .Label = "RaceMenu VR 0.4.14"sv,
        .ModuleName = kSkeeVRModuleName,
        .Version = REL::Version{3, 4, 5, 0},
        .TimeDateStamp = 0x5E71AF1F,
        .GetSkinFormSite = MakeSite("NifUtils::GetSkinForm", 0x000CE2E0, 13,
                                    kGetSkinFormPrologue_0345_VR),
        .VisitAllWornItemsSite =
            MakeSite("NifUtils::VisitAllWornItems", 0x000CE5C0, 14,
                     kVisitAllWornItemsPrologue_0345_VR),
        .VisitAllWornItemsMatchSite =
            MakeSite("NifUtils::VisitAllWornItems::MatchBySlot", 0x000CE6E5, 19,
                     kVisitAllWornItemsMatchPrologue_0345_VR),
        .VisitAllWornItemsMatchContinueRva = 0x000CE6F8,
        .VisitAllWornItemsMatchMissRva = 0x000CE73B,
        .VisitAllWornItemsMatchMaskStackOffset = 0x48,
        .VisitAllWornItemsMatchEntryRegister = HookLayout::EntryRegister::Rdi,
        .VisitArmorAddonSite =
            MakeSite("OverrideInterface::VisitArmorAddon", 0x000CE900, 20,
                     kVisitArmorAddonPrologue_SE_0416),
    },
    HookLayout{
        .Label = "RaceMenu SE 0.4.16"sv,
        .ModuleName = kSkee64ModuleName,
        .Version = REL::Version{3, 4, 5, 0},
        .TimeDateStamp = 0x5F799CB1,
        .GetSkinFormSite = MakeSite("NifUtils::GetSkinForm", 0x000D1C90, 15,
                                    kGetSkinFormPrologue_0345_SE),
        .VisitAllWornItemsSite =
            MakeSite("NifUtils::VisitAllWornItems", 0x000D1EB0, 12,
                     kVisitAllWornItemsPrologue_0345_SE),
        .VisitAllWornItemsMatchSite =
            MakeSite("NifUtils::VisitAllWornItems::MatchBySlot", 0x000D1F39, 23,
                     kVisitAllWornItemsMatchPrologue_0345_SE),
        .VisitAllWornItemsMatchContinueRva = 0x000D1F50,
        .VisitAllWornItemsMatchMissRva = 0x000D2025,
        .VisitAllWornItemsMatchMaskStackOffset = 0x30,
        .VisitAllWornItemsMatchEntryRegister = HookLayout::EntryRegister::R15,
        .VisitArmorAddonSite =
            MakeSite("OverrideInterface::VisitArmorAddon", 0x000D2410, 20,
                     kVisitArmorAddonPrologue_SE_0416),
    },
    HookLayout{
        .Label = "RaceMenu AE 0.4.19.9"sv,
        .ModuleName = kSkee64ModuleName,
        .Version = REL::Version{0, 4, 19, 9},
        .TimeDateStamp = 0x61D8DC73,
        .GetSkinFormSite = MakeSite("NifUtils::GetSkinForm", 0x0007A120, 20,
                                    kGetSkinFormPrologue_0419_9_Audited),
        .VisitAllWornItemsSite =
            MakeSite("NifUtils::VisitAllWornItems", 0x0007A340, 12,
                     kVisitAllWornItemsPrologue_0419_9),
        .VisitAllWornItemsMatchSite =
            MakeSite("NifUtils::VisitAllWornItems::MatchBySlot", 0x0007A3C9, 23,
                     kVisitAllWornItemsMatchPrologue_0419_9),
        .VisitAllWornItemsMatchContinueRva = 0x0007A3E0,
        .VisitAllWornItemsMatchMissRva = 0x0007A4B5,
        .VisitAllWornItemsMatchMaskStackOffset = 0x30,
        .VisitAllWornItemsMatchEntryRegister = HookLayout::EntryRegister::R15,
        .VisitArmorAddonSite =
            MakeSite("OverrideInterface::VisitArmorAddon", 0x0007A830, 21,
                     kVisitArmorAddonPrologue_0419_9),
    },
    HookLayout{
        .Label = "RaceMenu AE 0.4.19.10"sv,
        .ModuleName = kSkee64ModuleName,
        .Version = REL::Version{0, 4, 19, 10},
        .TimeDateStamp = 0x62B5048B,
        .GetSkinFormSite = MakeSite("NifUtils::GetSkinForm", 0x0007B060, 13,
                                    kGetSkinFormPrologue_0419_10_Audited),
        .VisitAllWornItemsSite =
            MakeSite("NifUtils::VisitAllWornItems", 0x0007B2A0, 12,
                     kVisitAllWornItemsPrologue_0419_9),
        .VisitAllWornItemsMatchSite =
            MakeSite("NifUtils::VisitAllWornItems::MatchBySlot", 0x0007B329, 23,
                     kVisitAllWornItemsMatchPrologue_0419_9),
        .VisitAllWornItemsMatchContinueRva = 0x0007B340,
        .VisitAllWornItemsMatchMissRva = 0x0007B415,
        .VisitAllWornItemsMatchMaskStackOffset = 0x30,
        .VisitAllWornItemsMatchEntryRegister = HookLayout::EntryRegister::R15,
        .VisitArmorAddonSite =
            MakeSite("OverrideInterface::VisitArmorAddon", 0x0007B9A0, 19,
                     kVisitArmorAddonPrologue_New_0419_13),
    },
    HookLayout{
        .Label = "RaceMenu AE 0.4.19.11"sv,
        .ModuleName = kSkee64ModuleName,
        .Version = REL::Version{0, 4, 19, 11},
        .TimeDateStamp = 0x62C5F183,
        .GetSkinFormSite = MakeSite("NifUtils::GetSkinForm", 0x0007B0E0, 13,
                                    kGetSkinFormPrologue_0419_10_Audited),
        .VisitAllWornItemsSite =
            MakeSite("NifUtils::VisitAllWornItems", 0x0007B320, 12,
                     kVisitAllWornItemsPrologue_0419_9),
        .VisitAllWornItemsMatchSite =
            MakeSite("NifUtils::VisitAllWornItems::MatchBySlot", 0x0007B3A9, 23,
                     kVisitAllWornItemsMatchPrologue_0419_9),
        .VisitAllWornItemsMatchContinueRva = 0x0007B3C0,
        .VisitAllWornItemsMatchMissRva = 0x0007B495,
        .VisitAllWornItemsMatchMaskStackOffset = 0x30,
        .VisitAllWornItemsMatchEntryRegister = HookLayout::EntryRegister::R15,
        .VisitArmorAddonSite =
            MakeSite("OverrideInterface::VisitArmorAddon", 0x0007BA20, 19,
                     kVisitArmorAddonPrologue_New_0419_13),
    },
    HookLayout{
        .Label = "RaceMenu AE 0.4.19.13"sv,
        .ModuleName = kSkee64ModuleName,
        .Version = REL::Version{0, 4, 19, 13},
        .TimeDateStamp = 0x63323917,
        .GetSkinFormSite = MakeSite("NifUtils::GetSkinForm", 0x0007E7B0, 13,
                                    kGetSkinFormPrologue_0419_10_Audited),
        .VisitAllWornItemsSite =
            MakeSite("NifUtils::VisitAllWornItems", 0x0007EA00, 13,
                     kVisitAllWornItemsPrologue_0419_13_AE),
        .VisitAllWornItemsMatchSite =
            MakeSite("NifUtils::VisitAllWornItems::MatchBySlot", 0x0007EA79, 23,
                     kVisitAllWornItemsMatchPrologue_0419_13),
        .VisitAllWornItemsMatchContinueRva = 0x0007EA90,
        .VisitAllWornItemsMatchMissRva = 0x0007EB65,
        .VisitAllWornItemsMatchMaskStackOffset = 0x30,
        .VisitAllWornItemsMatchEntryRegister = HookLayout::EntryRegister::R15,
        .VisitArmorAddonSite =
            MakeSite("OverrideInterface::VisitArmorAddon", 0x0007F0F0, 19,
                     kVisitArmorAddonPrologue_New_0419_13),
    },
    HookLayout{
        .Label = "RaceMenu GOG 0.4.19.13"sv,
        .ModuleName = kSkee64ModuleName,
        .Version = REL::Version{0, 4, 19, 13},
        .TimeDateStamp = 0x634CAD62,
        .GetSkinFormSite = MakeSite("NifUtils::GetSkinForm", 0x0007E790, 13,
                                    kGetSkinFormPrologue_0419_10_Audited),
        .VisitAllWornItemsSite =
            MakeSite("NifUtils::VisitAllWornItems", 0x0007E9E0, 13,
                     kVisitAllWornItemsPrologue_0419_13_GOG),
        .VisitAllWornItemsMatchSite =
            MakeSite("NifUtils::VisitAllWornItems::MatchBySlot", 0x0007EA59, 23,
                     kVisitAllWornItemsMatchPrologue_0419_13),
        .VisitAllWornItemsMatchContinueRva = 0x0007EA70,
        .VisitAllWornItemsMatchMissRva = 0x0007EB45,
        .VisitAllWornItemsMatchMaskStackOffset = 0x30,
        .VisitAllWornItemsMatchEntryRegister = HookLayout::EntryRegister::R15,
        .VisitArmorAddonSite =
            MakeSite("OverrideInterface::VisitArmorAddon", 0x0007F0D0, 19,
                     kVisitArmorAddonPrologue_New_0419_13),
    },
    HookLayout{
        .Label = "RaceMenu AE 0.4.19.14"sv,
        .ModuleName = kSkee64ModuleName,
        .Version = REL::Version{0, 4, 19, 14},
        .TimeDateStamp = 0x635C6697,
        .GetSkinFormSite = MakeSite("NifUtils::GetSkinForm", 0x0007E790, 13,
                                    kGetSkinFormPrologue_0419_10_Audited),
        .VisitAllWornItemsSite =
            MakeSite("NifUtils::VisitAllWornItems", 0x0007E9E0, 13,
                     kVisitAllWornItemsPrologue_0419_13_GOG),
        .VisitAllWornItemsMatchSite =
            MakeSite("NifUtils::VisitAllWornItems::MatchBySlot", 0x0007EA59, 23,
                     kVisitAllWornItemsMatchPrologue_0419_13),
        .VisitAllWornItemsMatchContinueRva = 0x0007EA70,
        .VisitAllWornItemsMatchMissRva = 0x0007EB45,
        .VisitAllWornItemsMatchMaskStackOffset = 0x30,
        .VisitAllWornItemsMatchEntryRegister = HookLayout::EntryRegister::R15,
        .VisitArmorAddonSite =
            MakeSite("OverrideInterface::VisitArmorAddon", 0x0007F0D0, 19,
                     kVisitArmorAddonPrologue_New_0419_13),
    },
    HookLayout{
        .Label = "RaceMenu GOG 0.4.19.14"sv,
        .ModuleName = kSkee64ModuleName,
        .Version = REL::Version{0, 4, 19, 14},
        .TimeDateStamp = 0x63B34B30,
        .GetSkinFormSite = MakeSite("NifUtils::GetSkinForm", 0x0007E890, 13,
                                    kGetSkinFormPrologue_0419_10_Audited),
        .VisitAllWornItemsSite =
            MakeSite("NifUtils::VisitAllWornItems", 0x0007EAE0, 13,
                     kVisitAllWornItemsPrologue_0419_14_GOG),
        .VisitAllWornItemsMatchSite =
            MakeSite("NifUtils::VisitAllWornItems::MatchBySlot", 0x0007EB59, 23,
                     kVisitAllWornItemsMatchPrologue_0419_13),
        .VisitAllWornItemsMatchContinueRva = 0x0007EB70,
        .VisitAllWornItemsMatchMissRva = 0x0007EC45,
        .VisitAllWornItemsMatchMaskStackOffset = 0x30,
        .VisitAllWornItemsMatchEntryRegister = HookLayout::EntryRegister::R15,
        .VisitArmorAddonSite =
            MakeSite("OverrideInterface::VisitArmorAddon", 0x0007F1D0, 19,
                     kVisitArmorAddonPrologue_New_0419_13),
    },
    HookLayout{
        .Label = "RaceMenu AE 0.4.19.15"sv,
        .ModuleName = kSkee64ModuleName,
        .Version = REL::Version{0, 4, 19, 15},
        .TimeDateStamp = 0x65739632,
        .GetSkinFormSite = MakeSite("NifUtils::GetSkinForm", 0x000C27C0, 13,
                                    kGetSkinFormPrologue_0419_15),
        .VisitAllWornItemsSite =
            MakeSite("NifUtils::VisitAllWornItems", 0x000C2B10, 12,
                     kVisitAllWornItemsPrologue_0419_15),
        .VisitAllWornItemsMatchSite =
            MakeSite("NifUtils::VisitAllWornItems::MatchBySlot", 0x000C2C31, 19,
                     kVisitAllWornItemsMatchPrologue_0419_15),
        .VisitAllWornItemsMatchContinueRva = 0x000C2C44,
        .VisitAllWornItemsMatchMissRva = 0x000C2C94,
        .VisitAllWornItemsMatchMaskStackOffset = 0x50,
        .VisitAllWornItemsMatchEntryRegister = HookLayout::EntryRegister::Rsi,
        .VisitArmorAddonSite =
            MakeSite("OverrideInterface::VisitArmorAddon", 0x000C2F60, 19,
                     kVisitArmorAddonPrologue_New_0419_15),
    },
    HookLayout{
        .Label = "RaceMenu AE 0.4.19.16"sv,
        .ModuleName = kSkee64ModuleName,
        .Version = REL::Version{0, 4, 19, 16},
        .TimeDateStamp = 0x65B4092C,
        .GetSkinFormSite = MakeSite("NifUtils::GetSkinForm", 0x000C27C0, 13,
                                    kGetSkinFormPrologue_0419_15),
        .VisitAllWornItemsSite =
            MakeSite("NifUtils::VisitAllWornItems", 0x000C2B10, 12,
                     kVisitAllWornItemsPrologue_0419_15),
        .VisitAllWornItemsMatchSite =
            MakeSite("NifUtils::VisitAllWornItems::MatchBySlot", 0x000C2C31, 19,
                     kVisitAllWornItemsMatchPrologue_0419_15),
        .VisitAllWornItemsMatchContinueRva = 0x000C2C44,
        .VisitAllWornItemsMatchMissRva = 0x000C2C94,
        .VisitAllWornItemsMatchMaskStackOffset = 0x50,
        .VisitAllWornItemsMatchEntryRegister = HookLayout::EntryRegister::Rsi,
        .VisitArmorAddonSite =
            MakeSite("OverrideInterface::VisitArmorAddon", 0x000C2F60, 19,
                     kVisitArmorAddonPrologue_New_0419_15),
    },
};

using VisitArmorAddonFunc =
    void(RE::Actor *, RE::TESObjectARMO *, RE::TESObjectARMA *,
         std::function<void(bool, RE::NiNode *, RE::NiAVObject *)>);
using VisitAllWornItemsFunc =
    void(RE::Actor *, std::uint32_t,
         std::function<void(RE::InventoryEntryData *)>);
using EffectiveArmorMaskCacheEntry =
    std::pair<RE::TESObjectARMO *, std::uint32_t>;
using ResolvedArmorAddonPair =
    std::pair<RE::TESObjectARMO *, RE::TESObjectARMA *>;

bool g_installed{false};
VisitArmorAddonFunc *g_originalVisitArmorAddon{nullptr};
VisitAllWornItemsFunc *g_originalVisitAllWornItems{nullptr};
thread_local RE::Actor *g_currentVisitAllWornItemsActor{nullptr};
thread_local std::vector<EffectiveArmorMaskCacheEntry>
    g_currentVisitAllWornItemsEffectiveMaskCache;

struct ScopedVisitAllWornItemsContext final {
  explicit ScopedVisitAllWornItemsContext(RE::Actor *a_actor)
      : previousActor(g_currentVisitAllWornItemsActor),
        previousEffectiveMaskCache(
            std::move(g_currentVisitAllWornItemsEffectiveMaskCache)) {
    g_currentVisitAllWornItemsActor = a_actor;
    g_currentVisitAllWornItemsEffectiveMaskCache.clear();
  }

  ~ScopedVisitAllWornItemsContext() {
    g_currentVisitAllWornItemsActor = previousActor;
    g_currentVisitAllWornItemsEffectiveMaskCache =
        std::move(previousEffectiveMaskCache);
  }

  ScopedVisitAllWornItemsContext(const ScopedVisitAllWornItemsContext &) = delete;
  auto operator=(const ScopedVisitAllWornItemsContext &)
      -> ScopedVisitAllWornItemsContext & = delete;

  RE::Actor *previousActor{nullptr};
  std::vector<EffectiveArmorMaskCacheEntry> previousEffectiveMaskCache;
};

void Hook_VisitAllWornItems(
    RE::Actor *a_actor, std::uint32_t a_mask,
    std::function<void(RE::InventoryEntryData *)> a_functor);
void Hook_VisitArmorAddon(
    RE::Actor *a_actor, RE::TESObjectARMO *a_armor,
    RE::TESObjectARMA *a_armorAddon,
    std::function<void(bool, RE::NiNode *, RE::NiAVObject *)> a_functor);
auto DoesCurrentVisitWornItemMatchSlotMask(
    std::uint32_t a_mask, RE::InventoryEntryData *a_entryData) -> bool;
auto CollectResolvedArmorAddonPairs(RE::Actor *a_actor, RE::TESObjectARMO *a_armor,
                                    RE::TESObjectARMA *a_armorAddon)
    -> std::vector<ResolvedArmorAddonPair>;

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

auto GetEffectiveArmorSlotMask(RE::Actor *a_actor, RE::TESObjectARMO *a_armor)
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

auto GetCachedEffectiveArmorSlotMaskForCurrentVisit(RE::TESObjectARMO *a_armor)
    -> std::optional<std::uint32_t> {
  if (!g_currentVisitAllWornItemsActor || !a_armor) {
    return std::nullopt;
  }

  const auto cacheIt = std::ranges::find(
      g_currentVisitAllWornItemsEffectiveMaskCache, a_armor,
      &EffectiveArmorMaskCacheEntry::first);
  if (cacheIt != g_currentVisitAllWornItemsEffectiveMaskCache.end()) {
    return cacheIt->second;
  }

  const auto effectiveMask =
      GetEffectiveArmorSlotMask(g_currentVisitAllWornItemsActor, a_armor);
  if (effectiveMask) {
    g_currentVisitAllWornItemsEffectiveMaskCache.emplace_back(a_armor,
                                                              *effectiveMask);
  }

  return effectiveMask;
}

auto MatchesMask(const std::uint32_t a_slotMask, const std::uint32_t a_mask)
    -> bool {
  return (a_slotMask & a_mask) != 0;
}

auto GetOriginalSlotMask(RE::TESForm *a_form) -> std::optional<std::uint32_t> {
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

auto DoesWornItemMatchSlotMask(
    RE::InventoryEntryData *a_entryData, const std::uint32_t a_mask,
    const auto &a_getEffectiveMask) -> bool {
  if (!a_entryData || !a_entryData->object) {
    return false;
  }

  if (auto *armor = a_entryData->object->As<RE::TESObjectARMO>()) {
    if (const auto effectiveMask = a_getEffectiveMask(armor)) {
      return MatchesMask(*effectiveMask, a_mask);
    }
  }

  if (const auto originalMask = GetOriginalSlotMask(a_entryData->object)) {
    return MatchesMask(*originalMask, a_mask);
  }

  return false;
}

auto DoesCurrentVisitWornItemMatchSlotMask(
    const std::uint32_t a_mask, RE::InventoryEntryData *a_entryData) -> bool {
  return DoesWornItemMatchSlotMask(
      a_entryData, a_mask,
      [](RE::TESObjectARMO *a_armor) {
        return GetCachedEffectiveArmorSlotMaskForCurrentVisit(a_armor);
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
                              DynamicArmorManager *a_manager,
                              VisitFn &a_visit)
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

        manager->VisitArmorAddons(actor, armor, armorAddon,
                                  [this, a_entryData](const auto &a_visit) {
                                    if (done ||
                                        (std::to_underlying(
                                             a_visit.EffectiveMask) &
                                         mask) == 0) {
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

  if (auto *effectiveOwner = ResolveEffectiveArmorOwnerForSlot(a_actor, a_mask)) {
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

  const ScopedVisitAllWornItemsContext actorScope{a_actor};
  g_originalVisitAllWornItems(a_actor, a_mask, std::move(a_functor));
}

auto CollectResolvedArmorAddonPairs(RE::Actor *a_actor, RE::TESObjectARMO *a_armor,
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
        if (std::find(resolved.begin(), resolved.end(), pair) == resolved.end()) {
          resolved.push_back(pair);
        }
      });

  return resolved;
}

void Hook_VisitArmorAddon(
    RE::Actor *a_actor, RE::TESObjectARMO *a_armor,
    RE::TESObjectARMA *a_armorAddon,
    std::function<void(bool, RE::NiNode *, RE::NiAVObject *)> a_functor) {
  if (!g_originalVisitArmorAddon || !a_actor || !a_functor) {
    return;
  }

  // RaceMenu's OverrideInterface::Impl_SetSkinProperties still gates source
  // addons with IsSlotMatch(...) before it reaches VisitArmorAddon(...). The
  // current hooks are enough for the Wet Function path we verified, but a niche
  // future case could still be blocked if the source ARMA mask does not match
  // while the resolved DAVE visual ARMA should. If that ever matters, patch
  // that internal IsSlotMatch decision directly too.
  auto resolvedPairs =
      CollectResolvedArmorAddonPairs(a_actor, a_armor, a_armorAddon);

  if (resolvedPairs.empty()) {
    g_originalVisitArmorAddon(a_actor, a_armor, a_armorAddon,
                              std::move(a_functor));
    return;
  }

  for (auto &[resolvedArmor, resolvedAddon] : resolvedPairs) {
    g_originalVisitArmorAddon(a_actor, resolvedArmor, resolvedAddon, a_functor);
  }
}

auto ResolveHookAddress(const std::uintptr_t a_moduleBase, const HookSite &a_site)
    -> std::uintptr_t {
  return a_moduleBase + a_site.Rva;
}

auto MatchesPrologue(const std::uintptr_t a_moduleBase, const HookSite &a_site)
    -> bool {
  const auto address = ResolveHookAddress(a_moduleBase, a_site);
  return std::memcmp(reinterpret_cast<const void *>(address), a_site.Prologue.data(),
                     a_site.Prologue.size()) == 0;
}

void WriteAbsoluteJump(const std::uintptr_t a_address,
                       const std::uintptr_t a_target,
                       const std::size_t a_patchSize,
                       std::span<const std::uint8_t> a_expected) {
  struct AbsoluteJumpPatch final : Xbyak::CodeGenerator {
    explicit AbsoluteJumpPatch(const std::uintptr_t a_target) {
      mov(rax, a_target);
      jmp(rax);
    }
  };

  AbsoluteJumpPatch patch{a_target};
  patch.ready();

  if (patch.getSize() > a_patchSize) {
    util::report_and_fail("RaceMenu compat patch size too small"sv);
  }

  std::vector<std::uint8_t> patchBytes(a_patchSize, REL::NOP);
  std::memcpy(patchBytes.data(), patch.getCode(), patch.getSize());
  if (!REL::safe_write(a_address, patchBytes.data(), patchBytes.size(),
                       a_expected.data(), a_expected.size())) {
    util::report_and_fail("RaceMenu compat patch verification failed"sv);
  }
}

auto CreateOriginalTrampoline(const std::uintptr_t a_address,
                              const std::size_t a_patchSize)
    -> std::uintptr_t {
  constexpr std::size_t kAbsoluteJumpSize = 12;
  auto *memory = static_cast<std::uint8_t *>(
      SKSE::GetTrampoline().allocate(a_patchSize + kAbsoluteJumpSize));
  std::memcpy(memory, reinterpret_cast<const void *>(a_address), a_patchSize);

  const auto returnAddress = a_address + a_patchSize;
  std::array<std::uint8_t, kAbsoluteJumpSize> jump{
      0x48, 0xB8, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xE0};
  std::memcpy(jump.data() + 2, &returnAddress, sizeof(returnAddress));
  std::memcpy(memory + a_patchSize, jump.data(), jump.size());
  return reinterpret_cast<std::uintptr_t>(memory);
}

void InstallVisitAllWornItemsMatchPatch(const std::uintptr_t a_moduleBase,
                                        const HookLayout &a_layout) {
  if (a_layout.VisitAllWornItemsMatchSite.Rva == 0 ||
      a_layout.VisitAllWornItemsMatchContinueRva == 0 ||
      a_layout.VisitAllWornItemsMatchMissRva == 0) {
    return;
  }

  const auto siteAddress =
      ResolveHookAddress(a_moduleBase, a_layout.VisitAllWornItemsMatchSite);
  const auto continueAddress =
      a_moduleBase + a_layout.VisitAllWornItemsMatchContinueRva;
  const auto missAddress = a_moduleBase + a_layout.VisitAllWornItemsMatchMissRva;
  const auto maskStackOffset = a_layout.VisitAllWornItemsMatchMaskStackOffset;
  const auto entryRegister = a_layout.VisitAllWornItemsMatchEntryRegister;

  struct VisitAllWornItemsMatchPatch final : Xbyak::CodeGenerator {
    VisitAllWornItemsMatchPatch(const std::uintptr_t a_helper,
                                const std::uint8_t a_maskStackOffset,
                                const HookLayout::EntryRegister a_entryRegister,
                                const std::uintptr_t a_continueAddress,
                                const std::uintptr_t a_missAddress) {
      mov(ecx, dword[rsp + a_maskStackOffset]);

      switch (a_entryRegister) {
      case HookLayout::EntryRegister::Rsi:
        mov(rdx, rsi);
        break;
      case HookLayout::EntryRegister::Rdi:
        mov(rdx, rdi);
        break;
      case HookLayout::EntryRegister::R15:
        mov(rdx, r15);
        break;
      default:
        util::report_and_fail(
            "RaceMenu compat unsupported VisitAllWornItems entry register"sv);
      }

      sub(rsp, 0x20);
      mov(rax, a_helper);
      call(rax);
      add(rsp, 0x20);
      test(al, al);

      Xbyak::Label miss;
      je(miss);

      mov(rax, a_continueAddress);
      jmp(rax);

      L(miss);
      mov(rax, a_missAddress);
      jmp(rax);
    }
  };

  static std::vector<std::unique_ptr<VisitAllWornItemsMatchPatch>> s_patches;
  auto patch = std::make_unique<VisitAllWornItemsMatchPatch>(
      reinterpret_cast<std::uintptr_t>(&DoesCurrentVisitWornItemMatchSlotMask),
      maskStackOffset, entryRegister, continueAddress, missAddress);
  patch->ready();

  WriteAbsoluteJump(siteAddress, reinterpret_cast<std::uintptr_t>(patch->getCode()),
                    a_layout.VisitAllWornItemsMatchSite.PatchSize,
                    a_layout.VisitAllWornItemsMatchSite.Prologue);
  s_patches.push_back(std::move(patch));
}

auto ValidateHookSite(const std::uintptr_t a_moduleBase, const HookSite &a_site)
    -> bool {
  if (a_site.Rva == 0) {
    return true;
  }

  if (MatchesPrologue(a_moduleBase, a_site)) {
    return true;
  }

  logger::warn("RaceMenu compat skipped; {} prologue mismatch at {:X}"sv,
               a_site.Name, ResolveHookAddress(a_moduleBase, a_site));
  return false;
}

auto ReadModuleTimeDateStamp(const std::uintptr_t a_moduleBase)
    -> std::optional<std::uint32_t> {
  const auto *dosHeader =
      reinterpret_cast<const IMAGE_DOS_HEADER *>(a_moduleBase);
  if (!dosHeader || dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
    return std::nullopt;
  }

  const auto ntHeadersAddress = a_moduleBase + dosHeader->e_lfanew;
  const auto *ntHeaders =
      reinterpret_cast<const IMAGE_NT_HEADERS64 *>(ntHeadersAddress);
  if (!ntHeaders || ntHeaders->Signature != IMAGE_NT_SIGNATURE) {
    return std::nullopt;
  }

  return ntHeaders->FileHeader.TimeDateStamp;
}

auto FindLoadedSkeeModule() -> std::pair<HMODULE, const wchar_t *> {
  if (const auto module = ::GetModuleHandleW(kSkeeVRModuleName)) {
    return {module, kSkeeVRModuleName};
  }

  if (const auto module = ::GetModuleHandleW(kSkee64ModuleName)) {
    return {module, kSkee64ModuleName};
  }

  return {nullptr, nullptr};
}

auto DescribeModuleName(const wchar_t *a_moduleName) -> std::string_view {
  if (a_moduleName && std::wcscmp(a_moduleName, kSkee64ModuleName) == 0) {
    return "skee64.dll"sv;
  }

  if (a_moduleName && std::wcscmp(a_moduleName, kSkeeVRModuleName) == 0) {
    return "skeevr.dll"sv;
  }

  return "<unknown>"sv;
}

auto ReadModuleVersion(const HMODULE a_module) -> std::optional<REL::Version> {
  std::array<wchar_t, MAX_PATH> modulePath{};
  const auto length = ::GetModuleFileNameW(
      a_module, modulePath.data(), static_cast<DWORD>(modulePath.size()));
  if (length == 0 || length >= modulePath.size()) {
    return std::nullopt;
  }

  std::uint32_t ignoredHandle{0};
  std::vector<char> versionData(REX::W32::GetFileVersionInfoSizeW(
      modulePath.data(), std::addressof(ignoredHandle)));
  if (versionData.empty()) {
    return std::nullopt;
  }

  if (!REX::W32::GetFileVersionInfoW(modulePath.data(), 0,
                                     static_cast<std::uint32_t>(
                                         versionData.size()),
                                     versionData.data())) {
    return std::nullopt;
  }

  void *versionBlock{nullptr};
  std::uint32_t versionBlockSize{0};
  if (!REX::W32::VerQueryValueW(versionData.data(), L"\\",
                                std::addressof(versionBlock),
                                std::addressof(versionBlockSize)) ||
      !versionBlock ||
      versionBlockSize < sizeof(VS_FIXEDFILEINFO)) {
    return std::nullopt;
  }

  const auto *fixedInfo =
      static_cast<const VS_FIXEDFILEINFO *>(versionBlock);
  return REL::Version{
      static_cast<std::uint16_t>((fixedInfo->dwFileVersionMS >> 16) & 0xFFFF),
      static_cast<std::uint16_t>(fixedInfo->dwFileVersionMS & 0xFFFF),
      static_cast<std::uint16_t>((fixedInfo->dwFileVersionLS >> 16) & 0xFFFF),
      static_cast<std::uint16_t>(fixedInfo->dwFileVersionLS & 0xFFFF),
  };
}

auto FindHookLayout(const wchar_t *a_moduleName, const REL::Version &a_version,
                    const std::uint32_t a_timeDateStamp) -> const HookLayout * {
  const auto matchesModule = [a_moduleName](const HookLayout &a_layout) {
    return std::wcscmp(a_layout.ModuleName, a_moduleName) == 0;
  };

  const auto matchesVersion = [&a_version](const HookLayout &a_layout) {
    return a_layout.Version == a_version;
  };

  std::vector<const HookLayout *> candidates;
  candidates.reserve(kHookLayouts.size());
  for (const auto &layout : kHookLayouts) {
    if (matchesModule(layout) && matchesVersion(layout)) {
      candidates.push_back(std::addressof(layout));
    }
  }

  if (candidates.empty()) {
    return nullptr;
  }

  if (candidates.size() == 1) {
    return candidates.front();
  }

  const auto timestampIt = std::ranges::find_if(
      candidates, [a_timeDateStamp](const HookLayout *a_layout) {
        return a_layout->TimeDateStamp == a_timeDateStamp;
      });
  return timestampIt != candidates.end() ? *timestampIt : nullptr;
}

void InstallAbsoluteJumpHook(const std::uintptr_t a_moduleBase,
                             const HookSite &a_site, const void *a_replacement) {
  WriteAbsoluteJump(ResolveHookAddress(a_moduleBase, a_site),
                    reinterpret_cast<std::uintptr_t>(a_replacement),
                    a_site.PatchSize, a_site.Prologue);
}

template <class Fn>
auto InstallTrampolineHook(const std::uintptr_t a_moduleBase,
                           const HookSite &a_site, const void *a_replacement)
    -> Fn * {
  const auto address = ResolveHookAddress(a_moduleBase, a_site);
  auto *original = reinterpret_cast<Fn *>(
      CreateOriginalTrampoline(address, a_site.PatchSize));
  WriteAbsoluteJump(address, reinterpret_cast<std::uintptr_t>(a_replacement),
                    a_site.PatchSize, a_site.Prologue);
  return original;
}
} // namespace

void RaceMenuCompat::Install() {
  if (g_installed) {
    return;
  }

  const auto [module, moduleName] = FindLoadedSkeeModule();
  if (!module) {
    logger::info("RaceMenu compat skipped; RaceMenu module not loaded"sv);
    return;
  }

  const auto moduleBase = reinterpret_cast<std::uintptr_t>(module);
  const auto version = ReadModuleVersion(module);
  if (!version) {
    logger::warn("RaceMenu compat skipped; failed to read version from {}"sv,
                 DescribeModuleName(moduleName));
    return;
  }

  const auto timeDateStamp = ReadModuleTimeDateStamp(moduleBase);
  if (!timeDateStamp) {
    logger::warn(
        "RaceMenu compat skipped; failed to read PE timestamp from {}"sv,
        DescribeModuleName(moduleName));
    return;
  }

  const auto *layout = FindHookLayout(moduleName, *version, *timeDateStamp);
  if (!layout) {
    logger::warn(
        "RaceMenu compat skipped; unsupported {} version {} timestamp {:08X}"sv,
        DescribeModuleName(moduleName), version->string("."sv), *timeDateStamp);
    return;
  }

  if (!ValidateHookSite(moduleBase, layout->GetSkinFormSite) ||
      !ValidateHookSite(moduleBase, layout->VisitAllWornItemsSite) ||
      !ValidateHookSite(moduleBase, layout->VisitAllWornItemsMatchSite) ||
      !ValidateHookSite(moduleBase, layout->VisitArmorAddonSite)) {
    return;
  }

  InstallAbsoluteJumpHook(moduleBase, layout->GetSkinFormSite, &Hook_GetSkinForm);
  g_originalVisitAllWornItems = InstallTrampolineHook<VisitAllWornItemsFunc>(
      moduleBase, layout->VisitAllWornItemsSite, &Hook_VisitAllWornItems);
  InstallVisitAllWornItemsMatchPatch(moduleBase, *layout);
  g_originalVisitArmorAddon = InstallTrampolineHook<VisitArmorAddonFunc>(
      moduleBase, layout->VisitArmorAddonSite, &Hook_VisitArmorAddon);
  g_installed = true;

  logger::info("Installed RaceMenu compat hooks for {} ({} {:08X})"sv,
               layout->Label, version->string("."sv), layout->TimeDateStamp);
}
