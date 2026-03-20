#include "ConfigLoader.h"

#include "DynamicArmorManager.h"

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
auto ConfigLoader::RegisterVariantJson(std::string_view a_name,
                                       std::string_view a_variantJson) -> bool {
  Json::Value root;
  if (!ParseJson(a_variantJson, root) || !root.isObject()) {
    return false;
  }

  auto variantName = std::string(a_name);
  if (variantName.empty()) {
    variantName = root["name"].asString();
  }

  if (variantName.empty()) {
    logger::error("RegisterVariantJson requires a variant name"sv);
    return false;
  }

  ArmorVariant armorVariant{};
  if (!BuildArmorVariant(root, armorVariant)) {
    return false;
  }

  DynamicArmorManager::GetSingleton()->ReplaceArmorVariant(
      variantName, std::move(armorVariant));
  return true;
}

auto ConfigLoader::DeleteVariant(std::string_view a_name) -> bool {
  if (a_name.empty()) {
    return false;
  }

  return DynamicArmorManager::GetSingleton()->DeleteArmorVariant(a_name);
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
auto ConfigLoader::SetVariantConditionsJson(std::string_view a_variant,
                                            std::string_view a_conditionsJson)
    -> bool {
  if (a_variant.empty()) {
    logger::error("SetVariantConditionsJson requires a variant name"sv);
    return false;
  }

  Json::Value root;
  if (!ParseJson(a_conditionsJson, root)) {
    return false;
  }

  Json::Value conditions;
  Json::Value refs;
  if (!NormalizeConditionsPayload(root, conditions, refs)) {
    return false;
  }

  DynamicArmorManager::GetSingleton()->ClearCondition(a_variant);
  LoadConditions(a_variant, conditions, BuildRefMap(refs));
  return true;
}

void ConfigLoader::LoadConfigs() {
  const auto dataHandler = RE::TESDataHandler::GetSingleton();
  if (!dataHandler) {
    return;
  }

  for (auto &file : dataHandler->files) {
    if (!file) {
      continue;
    }

    auto fileName = fs::path(file->fileName);
    fileName.replace_extension("json"sv);
    auto directory = fs::path("SKSE/Plugins/DynamicArmorVariants");
    auto dynamicArmorFile = directory / fileName;

    LoadConfig(dynamicArmorFile);
  }
}

void ConfigLoader::LoadConfig(const fs::path &a_path) {
  RE::BSResourceNiBinaryStream fileStream{a_path.string()};

  if (!fileStream.good()) {
    return;
  }

  Json::Value root;
  try {
    fileStream >> root;
  } catch (...) {
    logger::error("Parse errors in file: {}"sv, a_path.filename().string());
  }

  if (!root.isObject()) {
    return;
  }

  Json::Value variants = root["variants"];
  if (variants.isArray()) {
    for (auto &variant : variants) {
      auto name = variant["name"].asString();
      LoadVariant(name, variant);
    }
  }

  Json::Value states = root["states"];
  if (states.isArray()) {
    for (auto &state : states) {
      auto variant = state["variant"].asString();
      LoadConditions(variant, state["conditions"], BuildRefMap(state["refs"]));
    }
  }
}

void ConfigLoader::LoadVariant(std::string_view a_name,
                               const Json::Value &a_variant) {
  ArmorVariant armorVariant{};
  if (!BuildArmorVariant(a_variant, armorVariant)) {
    return;
  }

  DynamicArmorManager::GetSingleton()->RegisterArmorVariant(
      a_name, std::move(armorVariant));
}
