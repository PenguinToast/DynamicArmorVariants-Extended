#include "DynamicArmorVariantsInterface.h"

#include "ConfigLoader.h"
#include "Ext/Actor.h"

namespace
{
	std::atomic<bool> g_apiReady{ false };
}

bool DynamicArmorVariantsInterface::IsReady()
{
	return g_apiReady.load();
}

bool DynamicArmorVariantsInterface::RegisterVariantJson(const char* a_name, const char* a_variantJson)
{
	if (!IsReady() || !a_variantJson) {
		return false;
	}

	return ConfigLoader::RegisterVariantJson(a_name ? a_name : ""sv, a_variantJson);
}

bool DynamicArmorVariantsInterface::DeleteVariant(const char* a_name)
{
	if (!IsReady() || !a_name) {
		return false;
	}

	return ConfigLoader::DeleteVariant(a_name);
}

bool DynamicArmorVariantsInterface::SetVariantConditionsJson(const char* a_name, const char* a_conditionsJson)
{
	if (!IsReady() || !a_name || !a_conditionsJson) {
		return false;
	}

	return ConfigLoader::SetVariantConditionsJson(a_name, a_conditionsJson);
}

bool DynamicArmorVariantsInterface::RefreshActor(RE::Actor* a_actor)
{
	if (!IsReady() || !a_actor) {
		return false;
	}

	Ext::Actor::Update3DSafe(a_actor);
	return true;
}

void DynamicArmorVariantsInterface::SetReady(bool a_ready)
{
	g_apiReady.store(a_ready);
}

void* DynamicArmorVariantsInterface::GetApiFunction(unsigned int a_revisionNumber)
{
	switch (a_revisionNumber) {
	case 1:
		return static_cast<DynamicArmorVariantsAPI::IDynamicArmorVariantsInterface001*>(GetSingleton());
	default:
		return nullptr;
	}
}

void DynamicArmorVariantsInterface::HandleInterfaceRequest(SKSE::MessagingInterface::Message* a_msg)
{
	if (a_msg->type != DynamicArmorVariantsAPI::DynamicArmorVariantsMessage::kMessage_QueryInterface) {
		return;
	}

	auto* request = static_cast<DynamicArmorVariantsAPI::DynamicArmorVariantsMessage*>(a_msg->data);
	request->GetApiFunction = GetApiFunction;
	logger::info("Provided DynamicArmorVariants API interface to {}"sv, a_msg->sender);
}
