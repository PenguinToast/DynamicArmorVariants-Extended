#include "DynamicArmor.h"
#include "main/DynamicArmorManager.h"
#include "main/LogUtil.h"

#define REGISTER(a_vm, a_function)                                             \
  a_vm->RegisterFunction(#a_function##sv, "DynamicArmor"sv, &(a_function))

namespace Papyrus {
auto DynamicArmor::GetAPIVersion(RE::StaticFunctionTag *) -> std::int32_t {
  DAV_LOG_DEBUG_LAZY("DAVE API papyrus: GetAPIVersion()"sv);
  return 1;
}

auto DynamicArmor::GetVariants(RE::StaticFunctionTag *,
                               RE::TESObjectARMO *a_armor)
    -> std::vector<std::string> {
  DAV_LOG_DEBUG_LAZY("DAVE API papyrus: GetVariants(armor={})"sv,
                     LogUtil::DescribeArmor(a_armor));
  return DynamicArmorManager::GetSingleton()->GetVariants(a_armor);
}

auto DynamicArmor::GetEquippedArmorsWithVariants(RE::StaticFunctionTag *,
                                                 RE::Actor *a_actor)
    -> std::vector<RE::TESObjectARMO *> {
  DAV_LOG_DEBUG_LAZY(
      "DAVE API papyrus: GetEquippedArmorsWithVariants(actor={})"sv,
      LogUtil::DescribeActor(a_actor));
  return DynamicArmorManager::GetSingleton()->GetEquippedArmorsWithVariants(
      a_actor);
}

auto DynamicArmor::GetDisplayName(RE::StaticFunctionTag *,
                                  std::string a_variant) -> std::string {
  DAV_LOG_DEBUG_LAZY("DAVE API papyrus: GetDisplayName(variant={})"sv,
                     LogUtil::DescribeStringArg(a_variant));
  return DynamicArmorManager::GetSingleton()->GetDisplayName(a_variant);
}

void DynamicArmor::ApplyVariant(RE::StaticFunctionTag *, RE::Actor *a_actor,
                                std::string a_variant) {
  DAV_LOG_DEBUG_LAZY("DAVE API papyrus: ApplyVariant(actor={}, variant={})"sv,
                     LogUtil::DescribeActor(a_actor),
                     LogUtil::DescribeStringArg(a_variant));
  DynamicArmorManager::GetSingleton()->ApplyVariant(a_actor, a_variant);
}

void DynamicArmor::ResetVariant(RE::StaticFunctionTag *, RE::Actor *a_actor,
                                RE::TESObjectARMO *a_armor) {
  DAV_LOG_DEBUG_LAZY("DAVE API papyrus: ResetVariant(actor={}, armor={})"sv,
                     LogUtil::DescribeActor(a_actor),
                     LogUtil::DescribeArmor(a_armor));
  DynamicArmorManager::GetSingleton()->ResetVariant(a_actor, a_armor);
}

void DynamicArmor::ResetAllVariants(RE::StaticFunctionTag *,
                                    RE::Actor *a_actor) {
  DAV_LOG_DEBUG_LAZY("DAVE API papyrus: ResetAllVariants(actor={})"sv,
                     LogUtil::DescribeActor(a_actor));
  DynamicArmorManager::GetSingleton()->ResetAllVariants(a_actor);
}

bool DynamicArmor::RegisterFuncs(RE::BSScript::IVirtualMachine *a_vm) {
  REGISTER(a_vm, GetAPIVersion);
  REGISTER(a_vm, GetVariants);
  REGISTER(a_vm, GetEquippedArmorsWithVariants);
  REGISTER(a_vm, GetDisplayName);
  REGISTER(a_vm, ApplyVariant);
  REGISTER(a_vm, ResetVariant);
  REGISTER(a_vm, ResetAllVariants);

  return true;
}
} // namespace Papyrus
