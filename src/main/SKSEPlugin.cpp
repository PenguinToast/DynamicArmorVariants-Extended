#include "api/DynamicArmorVariantsInterface.h"
#include "ConfigLoader.h"
#include "Hooks.h"
#include "Papyrus/Papyrus.h"
#include "Serialization.h"
#include "WornFormUpdater.h"

namespace
{
	void InitializeLog()
	{
#ifndef NDEBUG
		auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
		auto path = logger::log_directory();
		if (!path) {
			util::report_and_fail("Failed to find standard logging directory"sv);
		}

		*path /= std::string(Plugin::NAME) + ".log";
		auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif

#ifndef NDEBUG
		const auto level = spdlog::level::trace;
#else
		const auto level = spdlog::level::info;
#endif

		auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));
		log->set_level(level);
		log->flush_on(level);

		spdlog::set_default_logger(std::move(log));
		spdlog::set_pattern("%s(%#): [%^%l%$] %v"s);
	}
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	InitializeLog();
	logger::info("{} v{}"sv, Plugin::NAME, Plugin::VERSION.string());

	SKSE::Init(a_skse);
	SKSE::GetMessagingInterface()->RegisterListener(nullptr, [](SKSE::MessagingInterface::Message* a_message) {
		DynamicArmorVariantsInterface::HandleInterfaceRequest(a_message);
	});

	Hooks::Install();

	SKSE::GetPapyrusInterface()->Register(Papyrus::RegisterFuncs);

	if (auto serialization = SKSE::GetSerializationInterface()) {
		serialization->SetUniqueID(Serialization::ID);
		serialization->SetSaveCallback(&Serialization::SaveCallback);
		serialization->SetLoadCallback(&Serialization::LoadCallback);
		serialization->SetRevertCallback(&Serialization::RevertCallback);
	}

	SKSE::GetMessagingInterface()->RegisterListener(
		[](auto a_msg)
		{
			switch (a_msg->type) {
			case SKSE::MessagingInterface::kDataLoaded:
				ConfigLoader::LoadConfigs();
				DynamicArmorVariantsInterface::SetReady(true);
				WornFormUpdater::Install();
				break;
			}
		});

	return true;
}
