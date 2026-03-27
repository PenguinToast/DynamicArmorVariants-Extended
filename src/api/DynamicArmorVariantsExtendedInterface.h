#pragma once

#include "DynamicArmorVariantsExtendedAPI.h"

class DynamicArmorVariantsExtendedInterface
    : public DynamicArmorVariantsExtendedAPI::
          IDynamicArmorVariantsExtendedInterface001 {
public:
  static auto GetSingleton() -> DynamicArmorVariantsExtendedInterface * {
    static DynamicArmorVariantsExtendedInterface singleton;
    return std::addressof(singleton);
  }

  bool IsReady() override;
  bool RegisterVariantJson(const char *a_name,
                           const char *a_variantJson) override;
  bool DeleteVariant(const char *a_name) override;
  bool SetVariantConditionsJson(const char *a_name,
                                const char *a_conditionsJson) override;
  bool SetCondition(
      const char *a_name,
      const std::shared_ptr<RE::TESCondition> &a_condition) override;
  bool RefreshActor(RE::Actor *a_actor) override;
  bool ApplyVariantOverride(RE::Actor *a_actor, const char *a_variant,
                            bool a_keepExistingOverrides = false) override;
  bool RemoveVariantOverride(RE::Actor *a_actor,
                             const char *a_variant) override;

  static void SetReady(bool a_ready);
  static void HandleInterfaceRequest(SKSE::MessagingInterface::Message *a_msg);

private:
  static void *GetApiFunction(unsigned int a_revisionNumber);
};
