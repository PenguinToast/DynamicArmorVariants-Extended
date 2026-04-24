#include "Patches.h"
#include "RE/Offset.Ext.h"

namespace {
constexpr std::uint8_t kSEFixEquipConflictItemStackOffset = 0x80;
constexpr std::uint8_t kAEFixEquipConflictItemStackOffset = 0x88;
constexpr std::ptrdiff_t kSESkillLevelingHookOffset = 0x57;
constexpr std::ptrdiff_t kAESkillLevelingHookOffset = 0x3EE;
constexpr std::size_t kSESkillLevelingHookSize = 0x8;
constexpr std::size_t kAESkillLevelingHookSize = 0x5;
constexpr auto kAEBipedFromLoopEndOffset =
    static_cast<std::ptrdiff_t>(offsetof(RE::BipedAnim, bufferedObjects));

auto GetFixEquipConflictItemStackOffset() -> std::uint8_t {
  // Skyrim VR 1.4.15 matches the pre-AE/SE layout here and uses the same
  // owning-armor stack slot as SE.
  return REL::Module::IsAE() ? kAEFixEquipConflictItemStackOffset
                             : kSEFixEquipConflictItemStackOffset;
}

auto GetFixSkillLevelingHookOffset() -> std::ptrdiff_t {
  return REL::Module::IsAE() ? kAESkillLevelingHookOffset
                             : kSESkillLevelingHookOffset;
}
} // namespace

void Patches::WriteInitWornPatch(InitWornArmorFunc *a_func) {
  auto hook = util::MakeHook(RE::Offset::TESNPC::InitWornForm, 0x2F0);

  struct Patch : public Xbyak::CodeGenerator {
    Patch(std::uintptr_t a_funcAddr) {
      mov(rdx, r13);
      mov(rcx, rbp);
      mov(rax, a_funcAddr);
      call(rax);
    }
  };

  Patch patch{reinterpret_cast<std::uintptr_t>(a_func)};
  patch.ready();

  if (patch.getSize() > 0x17) {
    util::report_and_fail("Patch was too large, failed to install"sv);
  }

  REL::safe_fill(hook.address(), REL::NOP, 0x17);
  REL::safe_write(hook.address(), patch.getCode(), patch.getSize());
}

void Patches::WriteGetWornMaskPatch(GetWornMaskFunc *a_func) {
  auto hook = util::MakeHook(RE::Offset::InventoryChanges::GetWornMask);

  struct Patch : public Xbyak::CodeGenerator {
    Patch(std::uintptr_t a_funcAddr) {
      sub(rsp, 0x8);
      mov(rax, a_funcAddr);
      call(rax);
      add(rsp, 0x8);
      ret();
    }
  };

  Patch patch{reinterpret_cast<std::uintptr_t>(a_func)};
  patch.ready();

  if (patch.getSize() > 0x40) {
    util::report_and_fail("Patch was too large, failed to install"sv);
  }

  REL::safe_fill(hook.address(), REL::INT3, 0x40);
  REL::safe_write(hook.address(), patch.getCode(), patch.getSize());
}

void Patches::WriteTestBodyPartByIndexPatch(TestBodyPartByIndexFunc *a_func) {
  auto hook =
      util::MakeHook(RE::Offset::BGSBipedObjectForm::TestBodyPartByIndex);

  struct Patch : public Xbyak::CodeGenerator {
    Patch(std::uintptr_t a_funcAddr) {
      mov(rax, a_funcAddr);
      jmp(rax);
    }
  };

  Patch patch{reinterpret_cast<std::uintptr_t>(a_func)};
  patch.ready();

  if (patch.getSize() > 0x12) {
    util::report_and_fail("Patch was too large, failed to install"sv);
  }

  REL::safe_fill(hook.address(), REL::INT3, 0x12);
  REL::safe_write(hook.address(), patch.getCode(), patch.getSize());
}

void Patches::WriteFixEquipConflictPatch(FixEquipConflictCheckFunc *a_func) {
  auto &trampoline = SKSE::GetTrampoline();
  auto hook = util::MakeHook(RE::Offset::Actor::FixEquipConflictCheck, 0x97);
  const auto testBodyPartAddr =
      RE::Offset::BGSBipedObjectForm::TestBodyPartByIndex.address();
  // At FixEquipConflictCheck + 0x97, the owning TESObjectARMO* lives in
  // different stack slots across runtimes. The rest of the callback ABI is
  // shared, so normalize that difference here and keep the hook logic runtime-
  // agnostic.
  const auto itemStackOffset = GetFixEquipConflictItemStackOffset();

  struct Patch : public Xbyak::CodeGenerator {
    Patch(std::uintptr_t a_funcAddr, std::uintptr_t a_testBodyPartAddr,
          std::uint8_t a_itemStackOffset) {
      Xbyak::Label func;
      Xbyak::Label testBodyPart;
      Xbyak::Label exit;

      sub(rsp, 0x68);
      mov(ptr[rsp + 0x40], rcx);

      call(ptr[rip + testBodyPart]);
      test(al, al);
      jz(exit);

      mov(rcx, ptr[rsp + a_itemStackOffset]);
      mov(edx, ebx);
      mov(r8, rdi);
      call(ptr[rip + func]);

      L(exit);
      add(rsp, 0x68);
      ret();

      L(func);
      dq(a_funcAddr);

      L(testBodyPart);
      dq(a_testBodyPartAddr);
    }
  };

  Patch patch{reinterpret_cast<std::uintptr_t>(a_func), testBodyPartAddr,
              itemStackOffset};
  patch.ready();

  auto *code = trampoline.allocate(patch);
  trampoline.write_call<5>(hook.address(),
                           reinterpret_cast<std::uintptr_t>(code));
}

void Patches::WriteFixSkillLevelingPatch(FixSkillLevelingFunc *a_func) {
  auto &trampoline = SKSE::GetTrampoline();
  auto hook = util::MakeHook(RE::Offset::SkillLeveling::SkillMutationHook,
                             GetFixSkillLevelingHookOffset());
  struct SEPatch : public Xbyak::CodeGenerator {
    SEPatch(std::uintptr_t a_funcAddr, std::uintptr_t a_resumeAddr) {
      Xbyak::Label func;

      mov(rcx, ptr[rdi]);
      lea(rdx, ptr[rsp + 0x20]);
      sub(rsp, 0x20);
      call(ptr[rip + func]);
      add(rsp, 0x20);

      mov(edi, ptr[rsp + 0x34]);
      mov(esi, ptr[rsp + 0x30]);
      jmp(ptr[rip]);
      dq(a_resumeAddr);

      L(func);
      dq(a_funcAddr);
    }
  };

  struct AEPatch : public Xbyak::CodeGenerator {
    AEPatch(std::uintptr_t a_funcAddr, std::uintptr_t a_resumeAddr) {
      Xbyak::Label func;

      sub(rsp, 0x40);
      mov(ptr[rsp + 0x20], r13);
      mov(ptr[rsp + 0x28], rbx);
      mov(ptr[rsp + 0x30], r15d);
      mov(ptr[rsp + 0x34], ebp);
      lea(rcx, ptr[r14 - kAEBipedFromLoopEndOffset]);
      lea(rdx, ptr[rsp + 0x20]);
      call(ptr[rip + func]);
      mov(r15d, ptr[rsp + 0x30]);
      mov(ebp, ptr[rsp + 0x34]);
      add(rsp, 0x40);

      lea(esi, ptr[r12 - 0x1]);
      jmp(ptr[rip]);
      dq(a_resumeAddr);

      L(func);
      dq(a_funcAddr);
    }
  };

  auto *code = [&]() {
    const auto funcAddr = reinterpret_cast<std::uintptr_t>(a_func);
    if (REL::Module::IsAE()) {
      const auto resumeAddr = hook.address() + kAESkillLevelingHookSize;
      AEPatch patch{funcAddr, resumeAddr};
      patch.ready();
      return trampoline.allocate(patch);
    }

    const auto resumeAddr = hook.address() + kSESkillLevelingHookSize;
    SEPatch patch{funcAddr, resumeAddr};
    patch.ready();
    return trampoline.allocate(patch);
  }();

  if (!REL::Module::IsAE()) {
    REL::safe_fill(hook.address(), REL::NOP, kSESkillLevelingHookSize);
  }
  trampoline.write_branch<5>(hook.address(),
                             reinterpret_cast<std::uintptr_t>(code));
}
