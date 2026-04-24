#include "Settings.h"

#include "Json.h"

#include <fstream>
#include <limits>
#include <optional>
#include <sstream>

namespace {
Settings g_settings{};

auto ParseLogLevel(const std::string_view a_value)
    -> std::optional<spdlog::level::level_enum> {
  if (a_value == "trace"sv) {
    return spdlog::level::trace;
  }
  if (a_value == "debug"sv) {
    return spdlog::level::debug;
  }
  if (a_value == "info"sv) {
    return spdlog::level::info;
  }
  if (a_value == "warn"sv || a_value == "warning"sv) {
    return spdlog::level::warn;
  }
  if (a_value == "error"sv || a_value == "err"sv) {
    return spdlog::level::err;
  }
  if (a_value == "critical"sv) {
    return spdlog::level::critical;
  }
  if (a_value == "off"sv) {
    return spdlog::level::off;
  }

  return std::nullopt;
}

auto DescribeLogLevel(const spdlog::level::level_enum a_level)
    -> std::string_view {
  return spdlog::level::to_string_view(a_level);
}

auto GetSettingsPath() -> fs::path {
  return fs::current_path() / "Data" / "SKSE" / "Plugins" /
         "DynamicArmorVariants" / "settings.json";
}
} // namespace

auto Settings::Load() -> Settings {
  Settings settings{};
  try {
    const auto path = GetSettingsPath();

    std::ifstream stream(path);
    if (!stream.is_open()) {
      logger::warn("Failed to open settings file {}; optional settings will "
                   "stay at defaults"sv,
                   path.string());
      g_settings = settings;
      return g_settings;
    }

    std::ostringstream buffer;
    buffer << stream.rdbuf();

    Json::CharReaderBuilder builder;
    const auto reader =
        std::unique_ptr<Json::CharReader>(builder.newCharReader());

    Json::Value root;
    std::string errors;
    const auto json = buffer.str();
    if (!reader->parse(json.data(), json.data() + json.size(),
                       std::addressof(root), std::addressof(errors))) {
      logger::error("Failed to parse settings file {}; optional settings will "
                    "stay at defaults: {}"sv,
                    path.string(), errors);
      g_settings = settings;
      return g_settings;
    }

    settings.installEquipConflictHook =
        root.get("installEquipConflictHook", false).asBool();
    settings.useOwnershipBasedArmorMasks =
        root.get("useOwnershipBasedArmorMasks", false).asBool();
    settings.installRaceMenuCompatHooks =
        root.get("installRaceMenuCompatHooks", false).asBool();
    settings.installSkillLevelingHook =
        root.get("installSkillLevelingHook", false).asBool();
    if (const auto logLevelValue = root["logLevel"];
        logLevelValue.isString()) {
      if (const auto parsedLevel = ParseLogLevel(logLevelValue.asString());
          parsedLevel.has_value()) {
        settings.logLevel = *parsedLevel;
      } else {
        logger::warn("Unknown logLevel {}; ignoring"sv,
                     logLevelValue.asString());
      }
    }
    if (const auto capacityValue =
            root.get("refreshVariantCacheCapacity",
                     static_cast<Json::UInt64>(
                         Settings::DefaultRefreshVariantCacheCapacity));
        capacityValue.isUInt64()) {
      const auto rawCapacity = capacityValue.asUInt64();
      const auto maxCapacity =
          static_cast<Json::UInt64>((std::numeric_limits<std::size_t>::max)());
      settings.refreshVariantCacheCapacity =
          rawCapacity == 0
              ? Settings::DefaultRefreshVariantCacheCapacity
              : static_cast<std::size_t>(rawCapacity > maxCapacity
                                             ? maxCapacity
                                             : rawCapacity);
    }
    if (const auto ttlValue =
            root.get("refreshVariantCacheTtlMs",
                     static_cast<Json::UInt64>(
                         Settings::DefaultRefreshVariantCacheTtl.count()));
        ttlValue.isUInt64()) {
      const auto rawTtl = ttlValue.asUInt64();
      const auto maxTtl = static_cast<Json::UInt64>(
          (std::numeric_limits<std::int64_t>::max)());
      settings.refreshVariantCacheTtl =
          rawTtl == 0
              ? Settings::DefaultRefreshVariantCacheTtl
              : std::chrono::milliseconds(static_cast<std::int64_t>(
                    rawTtl > maxTtl ? maxTtl : rawTtl));
    }
    logger::info("Loaded settings from {} (installEquipConflictHook={}, "
                 "useOwnershipBasedArmorMasks={}, "
                 "installRaceMenuCompatHooks={}, "
                 "installSkillLevelingHook={}, "
                 "logLevel={}, "
                 "refreshVariantCacheCapacity={}, "
                 "refreshVariantCacheTtlMs={})"sv,
                 path.string(), settings.installEquipConflictHook,
                 settings.useOwnershipBasedArmorMasks,
                 settings.installRaceMenuCompatHooks,
                 settings.installSkillLevelingHook,
                 settings.logLevel.has_value()
                     ? DescribeLogLevel(*settings.logLevel)
                     : "default"sv,
                 settings.refreshVariantCacheCapacity,
                 settings.refreshVariantCacheTtl.count());
    g_settings = settings;
    return g_settings;
  } catch (const std::exception &a_error) {
    logger::error("Failed to load settings; optional settings will stay at "
                  "defaults: {}"sv,
                  a_error.what());
    g_settings = settings;
    return g_settings;
  } catch (...) {
    logger::error(
        "Failed to load settings; optional settings will stay at defaults"sv);
    g_settings = settings;
    return g_settings;
  }
}

auto Settings::Get() -> const Settings & { return g_settings; }
