#include "DynamicArmorVariantsExtendedInterface.h"

#include "ConfigLoader.h"
#include "Ext/Actor.h"
#include "main/DynamicArmorManager.h"

namespace {
std::atomic<bool> g_apiReady{false};
}

bool DynamicArmorVariantsExtendedInterface::IsReady() {
  return g_apiReady.load();
}

bool DynamicArmorVariantsExtendedInterface::RegisterVariantJson(
    const char *a_name, const char *a_variantJson) {
  if (!IsReady() || !a_variantJson) {
    return false;
  }

  return ConfigLoader::RegisterVariantJson(a_name ? a_name : ""sv,
                                           a_variantJson);
}

bool DynamicArmorVariantsExtendedInterface::DeleteVariant(const char *a_name) {
  if (!IsReady() || !a_name) {
    return false;
  }

  return ConfigLoader::DeleteVariant(a_name);
}

bool DynamicArmorVariantsExtendedInterface::SetVariantConditionsJson(
    const char *a_name, const char *a_conditionsJson) {
  if (!IsReady() || !a_name || !a_conditionsJson) {
    return false;
  }

  return ConfigLoader::SetVariantConditionsJson(a_name, a_conditionsJson);
}

bool DynamicArmorVariantsExtendedInterface::SetCondition(
    const char *a_name,
    const std::shared_ptr<RE::TESCondition> &a_condition) {
  if (!IsReady() || !a_name || !a_condition) {
    return false;
  }

  DynamicArmorManager::GetSingleton()->SetCondition(std::string(a_name),
                                                    a_condition);
  return true;
}

bool DynamicArmorVariantsExtendedInterface::RefreshActor(RE::Actor *a_actor) {
  if (!IsReady() || !a_actor) {
    return false;
  }

  DynamicArmorManager::GetSingleton()->ClearArmorAddonResolutionCache(
      a_actor->GetFormID());
  Ext::Actor::Update3DSafe(a_actor);
  return true;
}

bool DynamicArmorVariantsExtendedInterface::ApplyVariantOverride(
    RE::Actor *a_actor, const char *a_variant, bool a_keepExistingOverrides) {
  if (!IsReady() || !a_actor || !a_variant) {
    return false;
  }

  DynamicArmorManager::GetSingleton()->ApplyVariant(
      a_actor, std::string(a_variant), a_keepExistingOverrides);
  return true;
}

bool DynamicArmorVariantsExtendedInterface::RemoveVariantOverride(
    RE::Actor *a_actor, const char *a_variant) {
  if (!IsReady() || !a_actor || !a_variant) {
    return false;
  }

  DynamicArmorManager::GetSingleton()->RemoveVariantOverride(
      a_actor, std::string(a_variant));
  return true;
}

void DynamicArmorVariantsExtendedInterface::SetReady(bool a_ready) {
  g_apiReady.store(a_ready);
}

void *DynamicArmorVariantsExtendedInterface::GetApiFunction(
    unsigned int a_revisionNumber) {
  switch (a_revisionNumber) {
  case 1:
    return static_cast<DynamicArmorVariantsExtendedAPI::
                           IDynamicArmorVariantsExtendedInterface001 *>(
        GetSingleton());
  default:
    return nullptr;
  }
}

void DynamicArmorVariantsExtendedInterface::HandleInterfaceRequest(
    SKSE::MessagingInterface::Message *a_msg) {
  if (a_msg->type !=
      DynamicArmorVariantsExtendedAPI::DynamicArmorVariantsExtendedMessage::
          kMessage_QueryInterface) {
    return;
  }

  auto *request = static_cast<
      DynamicArmorVariantsExtendedAPI::DynamicArmorVariantsExtendedMessage *>(
      a_msg->data);
  request->GetApiFunction = GetApiFunction;
  logger::info("Provided DynamicArmorVariantsExtended API interface to {}"sv,
               a_msg->sender);
}
