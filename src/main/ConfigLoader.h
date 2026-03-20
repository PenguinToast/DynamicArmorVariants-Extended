#pragma once

#include "ArmorVariant.h"
#include "ConditionParser.h"
#include "Json.h"

class ConfigLoader {
public:
  ConfigLoader() = delete;

  static void LoadConfigs();
  static auto RegisterVariantJson(std::string_view a_name,
                                  std::string_view a_variantJson) -> bool;
  static auto DeleteVariant(std::string_view a_name) -> bool;
  static auto SetVariantConditionsJson(std::string_view a_variant,
                                       std::string_view a_conditionsJson)
      -> bool;

private:
  static auto ParseJson(std::string_view a_jsonText, Json::Value &a_root)
      -> bool;
  static auto BuildRefMap(const Json::Value &a_refs) -> ConditionParser::RefMap;
  static auto ParseOverrideOption(const Json::Value &a_overrideHead)
      -> ArmorVariant::OverrideOption;
  static auto BuildArmorVariant(const Json::Value &a_variantJson,
                                ArmorVariant &a_variant) -> bool;
  static auto NormalizeConditionsPayload(const Json::Value &a_root,
                                         Json::Value &a_conditions,
                                         Json::Value &a_refs) -> bool;
  static void LoadConfig(const fs::path &a_path);

  static void LoadVariant(std::string_view a_name,
                          const Json::Value &a_variant);

  static void LoadConditions(std::string_view a_variant,
                             const Json::Value &a_conditions,
                             const ConditionParser::RefMap &a_refs);

  static void LoadFormMap(Json::Value a_replaceByForm,
                          ArmorVariant::FormMap &a_formMap);

  static void LoadSlotMap(Json::Value a_replaceBySlot,
                          ArmorVariant::SlotMap &a_slotMap);
};
