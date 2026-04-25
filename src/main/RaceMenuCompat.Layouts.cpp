#include "RaceMenuCompat.Layouts.h"

#include "REL/Version.h"
#include "REX/W32/VERSION.h"

#include <array>
#include <string_view>
#include <vector>

namespace {
using namespace std::literals;
using namespace RaceMenuCompat::detail;

constexpr auto kSkee64ModuleName = L"skee64.dll";
constexpr auto kSkeeVRModuleName = L"skeevr.dll";

constexpr std::array<std::uint8_t, 13> kGetSkinFormPrologue_0419_15{
    0x48, 0x89, 0x5C, 0x24, 0x20, 0x55, 0x48,
    0x83, 0xEC, 0x60, 0x48, 0x8B, 0xE9};
constexpr std::array<std::uint8_t, 20> kGetSkinFormPrologue_0419_9{
    0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x6C, 0x24, 0x10,
    0x48, 0x89, 0x74, 0x24, 0x18, 0x48, 0x89, 0x7C, 0x24, 0x20};
constexpr std::array<std::uint8_t, 13> kGetSkinFormPrologue_0419_10{
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
    0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x6C, 0x24, 0x20, 0x56, 0x57};
constexpr std::array<std::uint8_t, 12> kVisitAllWornItemsPrologue_0419_9{
    0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x6C, 0x24, 0x20, 0x56, 0x57};
constexpr std::array<std::uint8_t, 13> kVisitAllWornItemsPrologue_0419_13_AE{
    0x41, 0x57, 0x48, 0x83, 0xEC, 0x50, 0x48,
    0x8B, 0x05, 0x2B, 0xE1, 0x14, 0x00};
constexpr std::array<std::uint8_t, 13> kVisitAllWornItemsPrologue_0419_13_GOG{
    0x41, 0x57, 0x48, 0x83, 0xEC, 0x50, 0x48,
    0x8B, 0x05, 0x4B, 0xE1, 0x14, 0x00};
constexpr std::array<std::uint8_t, 13> kVisitAllWornItemsPrologue_0419_14_GOG{
    0x41, 0x57, 0x48, 0x83, 0xEC, 0x50, 0x48,
    0x8B, 0x05, 0x4B, 0xE0, 0x14, 0x00};
constexpr std::array<std::uint8_t, 22> kVisitAllWornItemsPrologue_0419_13_Entry{
    0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x6C, 0x24, 0x20, 0x56,
    0x57, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x48, 0x83, 0xEC, 0x50};
constexpr std::array<std::uint8_t, 12> kVisitAllWornItemsPrologue_0419_15{
    0x48, 0x89, 0x5C, 0x24, 0x10, 0x55, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55};

constexpr std::array<std::uint8_t, 15>
    kVisitAllWornItemsSlotMatchPrologue_0345_VR{0x48, 0x8B, 0x17, 0x48, 0x8D,
                                                0x4C, 0x24, 0x40, 0x48, 0x8B,
                                                0x44, 0x24, 0x40, 0xFF, 0x10};
constexpr std::array<std::uint8_t, 15>
    kVisitAllWornItemsSlotMatchPrologue_0345_SE{0x49, 0x8B, 0x17, 0x48, 0x8D,
                                                0x4C, 0x24, 0x28, 0x48, 0x8B,
                                                0x44, 0x24, 0x28, 0xFF, 0x10};
constexpr std::array<std::uint8_t, 15>
    kVisitAllWornItemsSlotMatchPrologue_0419_9{0x49, 0x8B, 0x17, 0x48, 0x8D,
                                               0x4C, 0x24, 0x28, 0x48, 0x8B,
                                               0x44, 0x24, 0x28, 0xFF, 0x10};
constexpr std::array<std::uint8_t, 15>
    kVisitAllWornItemsSlotMatchPrologue_0419_13{0x48, 0x8B, 0x44, 0x24, 0x28,
                                                0x49, 0x8B, 0x17, 0x48, 0x8D,
                                                0x4C, 0x24, 0x28, 0xFF, 0x10};
constexpr std::array<std::uint8_t, 15>
    kVisitAllWornItemsSlotMatchPrologue_0419_15{0x48, 0x8B, 0x44, 0x24, 0x48,
                                                0x48, 0x8B, 0x16, 0x48, 0x8D,
                                                0x4C, 0x24, 0x48, 0xFF, 0x10};

constexpr std::array<std::uint8_t, 21> kVisitArmorAddonPrologue_0419_9{
    0x40, 0x55, 0x53, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56,
    0x41, 0x57, 0x48, 0x8D, 0xAC, 0x24, 0x98, 0xFE, 0xFF, 0xFF};
constexpr std::array<std::uint8_t, 19> kVisitArmorAddonPrologue_0419_13{
    0x40, 0x55, 0x53, 0x56, 0x57, 0x41, 0x54, 0x41, 0x56, 0x41,
    0x57, 0x48, 0x8D, 0xAC, 0x24, 0xA0, 0xFE, 0xFF, 0xFF};
constexpr std::array<std::uint8_t, 19> kVisitArmorAddonPrologue_0419_15{
    0x40, 0x55, 0x53, 0x56, 0x57, 0x41, 0x54, 0x41, 0x56, 0x41,
    0x57, 0x48, 0x8D, 0xAC, 0x24, 0xC0, 0xFE, 0xFF, 0xFF};
constexpr std::array<std::uint8_t, 19> kVisitArmorAddonPrologue_UBE_07{
    0x40, 0x55, 0x53, 0x56, 0x57, 0x41, 0x54, 0x41, 0x56, 0x41,
    0x57, 0x48, 0x8D, 0xAC, 0x24, 0xD0, 0xFE, 0xFF, 0xFF};
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
        .VisitAllWornItemsSlotMatchSite =
            MakeSite("NifUtils::VisitAllWornItems::MatchBySlot", 0x000CE6E5, 15,
                     kVisitAllWornItemsSlotMatchPrologue_0345_VR),
        .VisitAllWornItemsSlotMatchMaskStackOffset = 0x48,
        .VisitAllWornItemsSlotMatchEntryRegister =
            HookLayout::EntryRegister::Rdi,
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
        .VisitAllWornItemsSlotMatchSite =
            MakeSite("NifUtils::VisitAllWornItems::MatchBySlot", 0x000D1F39, 15,
                     kVisitAllWornItemsSlotMatchPrologue_0345_SE),
        .VisitAllWornItemsSlotMatchMaskStackOffset = 0x30,
        .VisitAllWornItemsSlotMatchEntryRegister =
            HookLayout::EntryRegister::R15,
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
                                    kGetSkinFormPrologue_0419_9),
        .VisitAllWornItemsSite =
            MakeSite("NifUtils::VisitAllWornItems", 0x0007A340, 12,
                     kVisitAllWornItemsPrologue_0419_9),
        .VisitAllWornItemsSlotMatchSite =
            MakeSite("NifUtils::VisitAllWornItems::MatchBySlot", 0x0007A3C9, 15,
                     kVisitAllWornItemsSlotMatchPrologue_0419_9),
        .VisitAllWornItemsSlotMatchMaskStackOffset = 0x30,
        .VisitAllWornItemsSlotMatchEntryRegister =
            HookLayout::EntryRegister::R15,
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
                                    kGetSkinFormPrologue_0419_10),
        .VisitAllWornItemsSite =
            MakeSite("NifUtils::VisitAllWornItems", 0x0007B2A0, 12,
                     kVisitAllWornItemsPrologue_0419_9),
        .VisitAllWornItemsSlotMatchSite =
            MakeSite("NifUtils::VisitAllWornItems::MatchBySlot", 0x0007B329, 15,
                     kVisitAllWornItemsSlotMatchPrologue_0419_9),
        .VisitAllWornItemsSlotMatchMaskStackOffset = 0x30,
        .VisitAllWornItemsSlotMatchEntryRegister =
            HookLayout::EntryRegister::R15,
        .VisitArmorAddonSite =
            MakeSite("OverrideInterface::VisitArmorAddon", 0x0007B9A0, 19,
                     kVisitArmorAddonPrologue_0419_13),
    },
    HookLayout{
        .Label = "RaceMenu AE 0.4.19.11"sv,
        .ModuleName = kSkee64ModuleName,
        .Version = REL::Version{0, 4, 19, 11},
        .TimeDateStamp = 0x62C5F183,
        .GetSkinFormSite = MakeSite("NifUtils::GetSkinForm", 0x0007B0E0, 13,
                                    kGetSkinFormPrologue_0419_10),
        .VisitAllWornItemsSite =
            MakeSite("NifUtils::VisitAllWornItems", 0x0007B320, 12,
                     kVisitAllWornItemsPrologue_0419_9),
        .VisitAllWornItemsSlotMatchSite =
            MakeSite("NifUtils::VisitAllWornItems::MatchBySlot", 0x0007B3A9, 15,
                     kVisitAllWornItemsSlotMatchPrologue_0419_9),
        .VisitAllWornItemsSlotMatchMaskStackOffset = 0x30,
        .VisitAllWornItemsSlotMatchEntryRegister =
            HookLayout::EntryRegister::R15,
        .VisitArmorAddonSite =
            MakeSite("OverrideInterface::VisitArmorAddon", 0x0007BA20, 19,
                     kVisitArmorAddonPrologue_0419_13),
    },
    HookLayout{
        .Label = "RaceMenu AE 0.4.19.13"sv,
        .ModuleName = kSkee64ModuleName,
        .Version = REL::Version{0, 4, 19, 13},
        .TimeDateStamp = 0x63323917,
        .GetSkinFormSite = MakeSite("NifUtils::GetSkinForm", 0x0007E7B0, 13,
                                    kGetSkinFormPrologue_0419_10),
        .VisitAllWornItemsSite =
            MakeSite("NifUtils::VisitAllWornItems", 0x0007E9F0, 22,
                     kVisitAllWornItemsPrologue_0419_13_Entry),
        .VisitAllWornItemsSlotMatchSite =
            MakeSite("NifUtils::VisitAllWornItems::MatchBySlot", 0x0007EA79, 15,
                     kVisitAllWornItemsSlotMatchPrologue_0419_13),
        .VisitAllWornItemsSlotMatchMaskStackOffset = 0x30,
        .VisitAllWornItemsSlotMatchEntryRegister =
            HookLayout::EntryRegister::R15,
        .VisitArmorAddonSite =
            MakeSite("OverrideInterface::VisitArmorAddon", 0x0007F0F0, 19,
                     kVisitArmorAddonPrologue_0419_13),
    },
    HookLayout{
        .Label = "RaceMenu GOG 0.4.19.13"sv,
        .ModuleName = kSkee64ModuleName,
        .Version = REL::Version{0, 4, 19, 13},
        .TimeDateStamp = 0x634CAD62,
        .GetSkinFormSite = MakeSite("NifUtils::GetSkinForm", 0x0007E790, 13,
                                    kGetSkinFormPrologue_0419_10),
        .VisitAllWornItemsSite =
            MakeSite("NifUtils::VisitAllWornItems", 0x0007E9D0, 22,
                     kVisitAllWornItemsPrologue_0419_13_Entry),
        .VisitAllWornItemsSlotMatchSite =
            MakeSite("NifUtils::VisitAllWornItems::MatchBySlot", 0x0007EA59, 15,
                     kVisitAllWornItemsSlotMatchPrologue_0419_13),
        .VisitAllWornItemsSlotMatchMaskStackOffset = 0x30,
        .VisitAllWornItemsSlotMatchEntryRegister =
            HookLayout::EntryRegister::R15,
        .VisitArmorAddonSite =
            MakeSite("OverrideInterface::VisitArmorAddon", 0x0007F0D0, 19,
                     kVisitArmorAddonPrologue_0419_13),
    },
    HookLayout{
        .Label = "RaceMenu AE 0.4.19.14"sv,
        .ModuleName = kSkee64ModuleName,
        .Version = REL::Version{0, 4, 19, 14},
        .TimeDateStamp = 0x635C6697,
        .GetSkinFormSite = MakeSite("NifUtils::GetSkinForm", 0x0007E790, 13,
                                    kGetSkinFormPrologue_0419_10),
        .VisitAllWornItemsSite =
            MakeSite("NifUtils::VisitAllWornItems", 0x0007E9D0, 22,
                     kVisitAllWornItemsPrologue_0419_13_Entry),
        .VisitAllWornItemsSlotMatchSite =
            MakeSite("NifUtils::VisitAllWornItems::MatchBySlot", 0x0007EA59, 15,
                     kVisitAllWornItemsSlotMatchPrologue_0419_13),
        .VisitAllWornItemsSlotMatchMaskStackOffset = 0x30,
        .VisitAllWornItemsSlotMatchEntryRegister =
            HookLayout::EntryRegister::R15,
        .VisitArmorAddonSite =
            MakeSite("OverrideInterface::VisitArmorAddon", 0x0007F0D0, 19,
                     kVisitArmorAddonPrologue_0419_13),
    },
    HookLayout{
        .Label = "RaceMenu GOG 0.4.19.14"sv,
        .ModuleName = kSkee64ModuleName,
        .Version = REL::Version{0, 4, 19, 14},
        .TimeDateStamp = 0x63B34B30,
        .GetSkinFormSite = MakeSite("NifUtils::GetSkinForm", 0x0007E890, 13,
                                    kGetSkinFormPrologue_0419_10),
        .VisitAllWornItemsSite =
            MakeSite("NifUtils::VisitAllWornItems", 0x0007EAD0, 22,
                     kVisitAllWornItemsPrologue_0419_13_Entry),
        .VisitAllWornItemsSlotMatchSite =
            MakeSite("NifUtils::VisitAllWornItems::MatchBySlot", 0x0007EB59, 15,
                     kVisitAllWornItemsSlotMatchPrologue_0419_13),
        .VisitAllWornItemsSlotMatchMaskStackOffset = 0x30,
        .VisitAllWornItemsSlotMatchEntryRegister =
            HookLayout::EntryRegister::R15,
        .VisitArmorAddonSite =
            MakeSite("OverrideInterface::VisitArmorAddon", 0x0007F1D0, 19,
                     kVisitArmorAddonPrologue_0419_13),
    },
    // Custom skee64.dll shipped by the UBE mod. This layout is currently
    // derived from static analysis only and remains untested in-game.
    HookLayout{
        .Label = "UBE 2.0 U 0.7 custom skee64"sv,
        .ModuleName = kSkee64ModuleName,
        .Version = REL::Version{4, 0, 0, 0},
        .TimeDateStamp = 0x61ECAF64,
        .GetSkinFormSite = MakeSite("NifUtils::GetSkinForm", 0x00077DE0, 13,
                                    kGetSkinFormPrologue_0419_10),
        .VisitAllWornItemsSite =
            MakeSite("NifUtils::VisitAllWornItems", 0x00078020, 22,
                     kVisitAllWornItemsPrologue_0419_13_Entry),
        .VisitAllWornItemsSlotMatchSite =
            MakeSite("NifUtils::VisitAllWornItems::MatchBySlot", 0x000780A9, 15,
                     kVisitAllWornItemsSlotMatchPrologue_0419_9),
        .VisitAllWornItemsSlotMatchMaskStackOffset = 0x30,
        .VisitAllWornItemsSlotMatchEntryRegister =
            HookLayout::EntryRegister::R15,
        .VisitArmorAddonSite =
            MakeSite("OverrideInterface::VisitArmorAddon", 0x000786B0, 19,
                     kVisitArmorAddonPrologue_UBE_07),
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
        .VisitAllWornItemsSlotMatchSite =
            MakeSite("NifUtils::VisitAllWornItems::MatchBySlot", 0x000C2C31, 15,
                     kVisitAllWornItemsSlotMatchPrologue_0419_15),
        .VisitAllWornItemsSlotMatchMaskStackOffset = 0x50,
        .VisitAllWornItemsSlotMatchEntryRegister =
            HookLayout::EntryRegister::Rsi,
        .VisitArmorAddonSite =
            MakeSite("OverrideInterface::VisitArmorAddon", 0x000C2F60, 19,
                     kVisitArmorAddonPrologue_0419_15),
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
        .VisitAllWornItemsSlotMatchSite =
            MakeSite("NifUtils::VisitAllWornItems::MatchBySlot", 0x000C2C31, 15,
                     kVisitAllWornItemsSlotMatchPrologue_0419_15),
        .VisitAllWornItemsSlotMatchMaskStackOffset = 0x50,
        .VisitAllWornItemsSlotMatchEntryRegister =
            HookLayout::EntryRegister::Rsi,
        .VisitArmorAddonSite =
            MakeSite("OverrideInterface::VisitArmorAddon", 0x000C2F60, 19,
                     kVisitArmorAddonPrologue_0419_15),
    },
    HookLayout{
        .Label = "RaceMenu AE 0.4.20.0"sv,
        .ModuleName = kSkee64ModuleName,
        .Version = REL::Version{0, 4, 20, 0},
        .TimeDateStamp = 0x69E40A71,
        .GetSkinFormSite = MakeSite("NifUtils::GetSkinForm", 0x000C6410, 13,
                                    kGetSkinFormPrologue_0419_15),
        .VisitAllWornItemsSite =
            MakeSite("NifUtils::VisitAllWornItems", 0x000C6760, 12,
                     kVisitAllWornItemsPrologue_0419_15),
        .VisitAllWornItemsSlotMatchSite =
            MakeSite("NifUtils::VisitAllWornItems::MatchBySlot", 0x000C6881,
                     15, kVisitAllWornItemsSlotMatchPrologue_0419_15),
        .VisitAllWornItemsSlotMatchMaskStackOffset = 0x50,
        .VisitAllWornItemsSlotMatchEntryRegister =
            HookLayout::EntryRegister::Rsi,
        .VisitArmorAddonSite =
            MakeSite("OverrideInterface::VisitArmorAddon", 0x000C6BB0, 19,
                     kVisitArmorAddonPrologue_0419_15),
    },
};
} // namespace

namespace RaceMenuCompat::detail {
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

  if (!REX::W32::GetFileVersionInfoW(
          modulePath.data(), 0, static_cast<std::uint32_t>(versionData.size()),
          versionData.data())) {
    return std::nullopt;
  }

  void *versionBlock{nullptr};
  std::uint32_t versionBlockSize{0};
  if (!REX::W32::VerQueryValueW(versionData.data(), L"\\",
                                std::addressof(versionBlock),
                                std::addressof(versionBlockSize)) ||
      !versionBlock || versionBlockSize < sizeof(VS_FIXEDFILEINFO)) {
    return std::nullopt;
  }

  const auto *fixedInfo = static_cast<const VS_FIXEDFILEINFO *>(versionBlock);
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
} // namespace RaceMenuCompat::detail
