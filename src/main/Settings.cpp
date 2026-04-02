#include "Settings.h"

#include "Json.h"

#include <fstream>
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
    logger::info(
        "Loaded settings from {} (installEquipConflictHook={}, "
        "useOwnershipBasedArmorMasks={})"sv,
        path.string(), settings.installEquipConflictHook,
        settings.useOwnershipBasedArmorMasks);
    g_settings = settings;
    return g_settings;
  } catch (const std::exception &a_error) {
    logger::error(
        "Failed to load settings; equip conflict hook and partial-slot "
        "resolution will stay disabled: {}"sv,
        a_error.what());
    g_settings = settings;
    return g_settings;
  } catch (...) {
    logger::error(
        "Failed to load settings; equip conflict hook and partial-slot "
        "resolution will stay disabled"sv);
    g_settings = settings;
    return g_settings;
  }
}

auto Settings::Get() -> const Settings & { return g_settings; }
