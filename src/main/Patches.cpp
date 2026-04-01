#include "Patches.h"
#include "RE/Offset.Ext.h"

namespace {
constexpr std::uint8_t kSEFixEquipConflictItemStackOffset = 0x80;
constexpr std::uint8_t kAEFixEquipConflictItemStackOffset = 0x88;

auto GetFixEquipConflictItemStackOffset() -> std::uint8_t {
  return REL::Module::IsAE() ? kAEFixEquipConflictItemStackOffset
                             : kSEFixEquipConflictItemStackOffset;
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
