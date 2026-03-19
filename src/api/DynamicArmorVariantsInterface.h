#pragma once

#include "DynamicArmorVariantsAPI.h"

class DynamicArmorVariantsInterface : public DynamicArmorVariantsAPI::IDynamicArmorVariantsInterface001
{
public:
	static auto GetSingleton() -> DynamicArmorVariantsInterface*
	{
		static DynamicArmorVariantsInterface singleton;
		return std::addressof(singleton);
	}

	bool IsReady() override;
	bool RegisterVariantJson(const char* a_name, const char* a_variantJson) override;
	bool DeleteVariant(const char* a_name) override;
	bool SetVariantConditionsJson(const char* a_name, const char* a_conditionsJson) override;
	bool RefreshActor(RE::Actor* a_actor) override;

	static void SetReady(bool a_ready);
	static void HandleInterfaceRequest(SKSE::MessagingInterface::Message* a_msg);

private:
	static void* GetApiFunction(unsigned int a_revisionNumber);
};
