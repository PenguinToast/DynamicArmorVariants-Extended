#include "Settings.h"

#include "Json.h"

#include <fstream>
#include <limits>
#include <sstream>

namespace {
Settings g_settings{};

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
      logger::warn(
          "Failed to open settings file {}; equip conflict hook will stay "
          "disabled"sv,
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
      logger::error(
          "Failed to parse settings file {}; equip conflict hook will stay "
          "disabled: {}"sv,
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
                 "refreshVariantCacheCapacity={}, "
                 "refreshVariantCacheTtlMs={})"sv,
                 path.string(), settings.installEquipConflictHook,
                 settings.useOwnershipBasedArmorMasks,
                 settings.installRaceMenuCompatHooks,
                 settings.refreshVariantCacheCapacity,
                 settings.refreshVariantCacheTtl.count());
    g_settings = settings;
    return g_settings;
  } catch (const std::exception &a_error) {
    logger::error(
        "Failed to load settings; equip conflict hook and partial-slot "
        "resolution will stay disabled, and RaceMenu compat hooks will stay "
        "disabled: {}"sv,
        a_error.what());
    g_settings = settings;
    return g_settings;
  } catch (...) {
    logger::error(
        "Failed to load settings; equip conflict hook and partial-slot "
        "resolution will stay disabled, and RaceMenu compat hooks will stay "
        "disabled"sv);
    g_settings = settings;
    return g_settings;
  }
}

auto Settings::Get() -> const Settings & { return g_settings; }
