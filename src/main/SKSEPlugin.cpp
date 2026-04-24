#include "ConfigLoader.h"
#include "DynamicArmorManager.h"
#include "Hooks.h"
#include "LogUtil.h"
#include "Papyrus/Papyrus.h"
#include "RaceMenuCompat.h"
#include "Serialization.h"
#include "Settings.h"
#include "WornFormUpdater.h"
#include "api/DynamicArmorVariantsExtendedInterface.h"

namespace {
auto GetDefaultLogLevel() -> spdlog::level::level_enum {
#ifndef NDEBUG
  return spdlog::level::trace;
#else
  return spdlog::level::info;
#endif
}

void InitializeLog() {
#ifndef NDEBUG
  auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
  auto path = logger::log_directory();
  if (!path) {
    util::report_and_fail("Failed to find standard logging directory"sv);
  }

  *path /= std::string(Plugin::NAME) + ".log";
  auto sink =
      std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif

  const auto level = GetDefaultLogLevel();

  auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));
  log->set_level(level);
  log->flush_on(level);

  spdlog::set_default_logger(std::move(log));
  spdlog::set_pattern("%s(%#): [%^%l%$] %v"s);
}

void ApplyConfiguredLogLevel() {
  const auto level = Settings::Get().logLevel.value_or(GetDefaultLogLevel());
  spdlog::set_level(level);
  spdlog::default_logger()->flush_on(level);
  logger::info("Logger level set to {}"sv, spdlog::level::to_string_view(level));
}

} // namespace

extern "C" DLLEXPORT bool SKSEAPI
SKSEPlugin_Load(const SKSE::LoadInterface *a_skse) {
  InitializeLog();

  SKSE::Init(a_skse);
  SKSE::AllocTrampoline(1 << 10);
  logger::info("{} build {}"sv, Plugin::NAME, Plugin::VERSION_STRING);
  auto *messaging = SKSE::GetMessagingInterface();
  Settings::Load();
  ApplyConfiguredLogLevel();

  Hooks::Install();

  SKSE::GetPapyrusInterface()->Register(Papyrus::RegisterFuncs);

  if (auto serialization = SKSE::GetSerializationInterface()) {
    serialization->SetUniqueID(Serialization::ID);
    serialization->SetSaveCallback(&Serialization::SaveCallback);
    serialization->SetLoadCallback(&Serialization::LoadCallback);
    serialization->SetRevertCallback(&Serialization::RevertCallback);
  }

  messaging->RegisterListener([](auto a_msg) {
    switch (a_msg->type) {
    case SKSE::MessagingInterface::kPostLoad:
      if (Settings::Get().installRaceMenuCompatHooks) {
        RaceMenuCompat::Install();
      } else {
        LogUtil::LogHookSkipped("RaceMenu compat"sv, "disabled by settings"sv);
      }
      SKSE::GetMessagingInterface()->RegisterListener(
          nullptr, [](SKSE::MessagingInterface::Message *a_message) {
            DynamicArmorVariantsExtendedInterface::HandleInterfaceRequest(
                a_message);
          });
      break;
    case SKSE::MessagingInterface::kDataLoaded:
      ConfigLoader::LoadConfigs();
      DynamicArmorVariantsExtendedInterface::SetReady(true);
      WornFormUpdater::Install();
      break;
    }
  });

  return true;
}
