#include "RaceMenuCompat.h"

#include "RaceMenuCompat.Layouts.h"
#include "RaceMenuCompatInternal.h"

#include <array>
#include <cstring>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace {
using namespace std::literals;
using namespace RaceMenuCompat::detail;

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

void InstallVisitAllWornItemsSlotMatchPatch(const std::uintptr_t a_moduleBase,
                                            const HookLayout &a_layout) {
  if (a_layout.VisitAllWornItemsSlotMatchSite.Rva == 0 ||
      a_layout.VisitAllWornItemsSlotMatchContinueRva == 0 ||
      a_layout.VisitAllWornItemsSlotMatchMissRva == 0) {
    return;
  }

  // Patch only the slot-match decision inside RaceMenu's VisitAllWornItems so
  // the original function body and std::function callback invocation stay in
  // skee, while DAVE controls the effective slot test.
  const auto siteAddress =
      ResolveHookAddress(a_moduleBase, a_layout.VisitAllWornItemsSlotMatchSite);
  const auto continueAddress =
      a_moduleBase + a_layout.VisitAllWornItemsSlotMatchContinueRva;
  const auto missAddress =
      a_moduleBase + a_layout.VisitAllWornItemsSlotMatchMissRva;
  const auto maskStackOffset =
      a_layout.VisitAllWornItemsSlotMatchMaskStackOffset;
  const auto entryRegister =
      a_layout.VisitAllWornItemsSlotMatchEntryRegister;

  struct VisitAllWornItemsSlotMatchPatch final : Xbyak::CodeGenerator {
    VisitAllWornItemsSlotMatchPatch(const std::uintptr_t a_helper,
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

  static std::vector<std::unique_ptr<VisitAllWornItemsSlotMatchPatch>> s_patches;
  auto patch = std::make_unique<VisitAllWornItemsSlotMatchPatch>(
      reinterpret_cast<std::uintptr_t>(&DoesCurrentVisitItemMatchSlotMask),
      maskStackOffset, entryRegister, continueAddress, missAddress);
  patch->ready();

  WriteAbsoluteJump(siteAddress, reinterpret_cast<std::uintptr_t>(patch->getCode()),
                    a_layout.VisitAllWornItemsSlotMatchSite.PatchSize,
                    a_layout.VisitAllWornItemsSlotMatchSite.Prologue);
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
  if (detail::g_installed) {
    return;
  }

  const auto [module, moduleName] = detail::FindLoadedSkeeModule();
  if (!module) {
    logger::info("RaceMenu compat skipped; RaceMenu module not loaded"sv);
    return;
  }

  const auto moduleBase = reinterpret_cast<std::uintptr_t>(module);
  const auto version = detail::ReadModuleVersion(module);
  if (!version) {
    logger::warn("RaceMenu compat skipped; failed to read version from {}"sv,
                 detail::DescribeModuleName(moduleName));
    return;
  }

  const auto timeDateStamp = ReadModuleTimeDateStamp(moduleBase);
  if (!timeDateStamp) {
    logger::warn(
        "RaceMenu compat skipped; failed to read PE timestamp from {}"sv,
        detail::DescribeModuleName(moduleName));
    return;
  }

  const auto *layout =
      detail::FindHookLayout(moduleName, *version, *timeDateStamp);
  if (!layout) {
    logger::warn(
        "RaceMenu compat skipped; unsupported {} version {} timestamp {:08X}"sv,
        detail::DescribeModuleName(moduleName), version->string("."sv),
        *timeDateStamp);
    return;
  }

  if (!ValidateHookSite(moduleBase, layout->GetSkinFormSite) ||
      !ValidateHookSite(moduleBase, layout->VisitAllWornItemsSite) ||
      !ValidateHookSite(moduleBase, layout->VisitAllWornItemsSlotMatchSite) ||
      !ValidateHookSite(moduleBase, layout->VisitArmorAddonSite)) {
    return;
  }

  InstallAbsoluteJumpHook(moduleBase, layout->GetSkinFormSite,
                          &detail::Hook_GetSkinForm);
  detail::g_originalVisitAllWornItems =
      InstallTrampolineHook<VisitAllWornItemsFunc>(
          moduleBase, layout->VisitAllWornItemsSite,
          &detail::Hook_VisitAllWornItems);
  InstallVisitAllWornItemsSlotMatchPatch(moduleBase, *layout);
  detail::g_originalVisitArmorAddon = InstallTrampolineHook<VisitArmorAddonFunc>(
      moduleBase, layout->VisitArmorAddonSite, &detail::Hook_VisitArmorAddon);
  detail::g_installed = true;

  logger::info("Installed RaceMenu compat hooks for {} ({} {:08X})"sv,
               layout->Label, version->string("."sv), layout->TimeDateStamp);
}
