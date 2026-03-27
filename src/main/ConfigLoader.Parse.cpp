#include "ConfigLoader.h"

#include "DynamicArmorManager.h"
#include "FormUtil.h"

#include <array>

auto ConfigLoader::ParseJson(std::string_view a_jsonText, Json::Value &a_root)
    -> bool {
  Json::CharReaderBuilder builder;
  const auto reader =
      std::unique_ptr<Json::CharReader>(builder.newCharReader());

  std::string errors;
  if (!reader->parse(a_jsonText.data(), a_jsonText.data() + a_jsonText.size(),
                     std::addressof(a_root), std::addressof(errors))) {
    logger::error("Failed to parse JSON payload: {}"sv, errors);
    return false;
  }

  return true;
}

auto ConfigLoader::BuildRefMap(const Json::Value &a_refs)
    -> ConditionParser::RefMap {
  ConditionParser::RefMap refMap;
  refMap["PLAYER"s] = RE::PlayerCharacter::GetSingleton();

  if (!a_refs.isObject()) {
    return refMap;
  }

  for (auto &name : a_refs.getMemberNames()) {
    auto identifier = a_refs[name].asString();

    if (auto form = FormUtil::LookupByIdentifier(identifier)) {
      refMap[util::str_toupper(name)] = form;
    }
  }

  return refMap;
}

#include <algorithm>

namespace {
constexpr std::int32_t kMinVariantPriority = -1000000;
constexpr std::int32_t kMaxVariantPriority = 1000000;
}

auto ConfigLoader::ParseOverrideOption(const Json::Value &a_overrideHead)
    -> ArmorVariant::OverrideOption {
  if (!a_overrideHead.isString()) {
    return ArmorVariant::OverrideOption::Undefined;
  }

  if (a_overrideHead == "none"s) {
    return ArmorVariant::OverrideOption::None;
  }
  if (a_overrideHead == "showAll"s) {
    return ArmorVariant::OverrideOption::ShowAll;
  }
  if (a_overrideHead == "showHead"s) {
    return ArmorVariant::OverrideOption::ShowHead;
  }
  if (a_overrideHead == "hideHair"s) {
    return ArmorVariant::OverrideOption::HideHair;
  }
  if (a_overrideHead == "hideAll"s) {
    return ArmorVariant::OverrideOption::HideAll;
  }

  return ArmorVariant::OverrideOption::Undefined;
}

auto ConfigLoader::BuildArmorVariant(const Json::Value &a_variantJson,
                                     ArmorVariant &a_variant) -> bool {
  if (!a_variantJson.isObject()) {
    return false;
  }

  a_variant.Linked = a_variantJson["linkTo"].asString();
  a_variant.DisplayName = a_variantJson["displayName"].asString();
  a_variant.Priority =
      std::clamp(a_variantJson.get("priority", 0).asInt(),
                 kMinVariantPriority, kMaxVariantPriority);
  a_variant.HasExplicitPriority = a_variantJson["priority"].isInt();
  a_variant.OverrideHead = ParseOverrideOption(a_variantJson["overrideHead"]);

  LoadFormMap(a_variantJson["replaceByForm"], a_variant.ReplaceByForm);
  LoadSlotMap(a_variantJson["replaceBySlot"], a_variant.ReplaceBySlot);
  return true;
}

auto ConfigLoader::NormalizeConditionsPayload(const Json::Value &a_root,
                                              Json::Value &a_conditions,
                                              Json::Value &a_refs) -> bool {
  if (a_root.isArray()) {
    a_conditions = a_root;
    return true;
  }

  if (a_root.isObject()) {
    a_conditions = a_root["conditions"];
    a_refs = a_root["refs"];
    if (a_conditions.isArray()) {
      return true;
    }

    logger::error("Condition JSON must contain a conditions array"sv);
    return false;
  }

  logger::error("Condition JSON must be an array or object"sv);
  return false;
}

auto ConfigLoader::ParseReplacementAddon(std::string_view a_identifier)
    -> std::optional<ArmorVariant::ReplacementAddon> {
  std::array<std::string, 4> parts;
  std::size_t partCount = 0;
  std::size_t start = 0;

  while (true) {
    const auto separator = a_identifier.find('|', start);
    if (separator == std::string_view::npos) {
      if (partCount >= parts.size()) {
        logger::warn("Could not resolve replacement form: {}"sv, a_identifier);
        return std::nullopt;
      }

      parts[partCount++] = std::string(a_identifier.substr(start));
      break;
    }

    if (partCount >= parts.size()) {
      logger::warn("Could not resolve replacement form: {}"sv, a_identifier);
      return std::nullopt;
    }

    parts[partCount++] =
        std::string(a_identifier.substr(start, separator - start));
    start = separator + 1;
  }

  if (partCount == 2) {
    auto *armorAddon = FormUtil::LookupByIdentifier<RE::TESObjectARMA>(
        std::string(a_identifier));
    if (!armorAddon) {
      logger::warn("Could not resolve form: {}"sv, a_identifier);
      return std::nullopt;
    }

    return ArmorVariant::ReplacementAddon{
        .Armor = nullptr,
        .ArmorAddon = armorAddon,
    };
  }

  if (partCount == 4) {
    const auto armorIdentifier = parts[0] + "|" + parts[1];
    const auto armorAddonIdentifier = parts[2] + "|" + parts[3];
    auto *armor =
        FormUtil::LookupByIdentifier<RE::TESObjectARMO>(armorIdentifier);
    auto *armorAddon =
        FormUtil::LookupByIdentifier<RE::TESObjectARMA>(armorAddonIdentifier);
    if (!armor || !armorAddon) {
      logger::warn("Could not resolve replacement form: {}"sv, a_identifier);
      return std::nullopt;
    }

    return ArmorVariant::ReplacementAddon{
        .Armor = armor,
        .ArmorAddon = armorAddon,
    };
  }

  logger::warn("Could not resolve replacement form: {}"sv, a_identifier);
  return std::nullopt;
}

void ConfigLoader::LoadConditions(std::string_view a_variant,
                                  const Json::Value &a_conditions,
                                  const ConditionParser::RefMap &a_refs) {
  if (!a_conditions.isArray()) {
    return;
  }

  auto condition = std::make_shared<RE::TESCondition>();
  RE::TESConditionItem **head = std::addressof(condition->head);

  for (auto &item : a_conditions) {
    auto text = item.asString();

    if (text.empty()) {
      continue;
    }

    if (auto conditionItem = ConditionParser::Parse(text, a_refs)) {
      *head = conditionItem;
      head = std::addressof(conditionItem->next);
    } else {
      logger::info("Aborting condition parsing"sv);
      return;
    }
  }

  DynamicArmorManager::GetSingleton()->SetCondition(a_variant, condition);
}

void ConfigLoader::LoadFormMap(Json::Value a_replaceByForm,
                               ArmorVariant::FormMap &a_formMap) {
  if (!a_replaceByForm.isObject()) {
    return;
  }

  for (auto &formIdentifier : a_replaceByForm.getMemberNames()) {
    auto addon =
        FormUtil::LookupByIdentifier<RE::TESObjectARMA>(formIdentifier);
    if (!addon) {
      continue;
    }

    ArmorVariant::AddonList replacementForms;

    Json::Value addons = a_replaceByForm[formIdentifier];
    if (addons.isString()) {
      if (auto replacement = ParseReplacementAddon(addons.asString())) {
        replacementForms.push_back(*replacement);
      }
    } else if (addons.isArray()) {
      for (auto &identifier : addons) {
        if (auto replacement = ParseReplacementAddon(identifier.asString())) {
          replacementForms.push_back(*replacement);
        }
      }
    }

    if (addons.isArray() || !replacementForms.empty()) {
      a_formMap.emplace(addon, std::move(replacementForms));
    } else {
      logger::warn("Replacements for {} are not valid, ignoring"sv,
                   formIdentifier);
    }
  }
}

void ConfigLoader::LoadSlotMap(Json::Value a_replaceBySlot,
                               ArmorVariant::SlotMap &a_slotMap) {
  if (!a_replaceBySlot.isObject()) {
    return;
  }

  for (auto &slotIndex : a_replaceBySlot.getMemberNames()) {
    auto bipedObject = static_cast<BipedObject>(std::stoi(slotIndex) - 30);

    if (bipedObject > 31) {
      continue;
    }

    a_slotMap.emplace(bipedObject, ArmorVariant::AddonList{});

    Json::Value addons = a_replaceBySlot[slotIndex];
    if (addons.isString()) {
      if (auto replacement = ParseReplacementAddon(addons.asString())) {
        a_slotMap[bipedObject].push_back(*replacement);
      }
    } else if (addons.isArray()) {
      for (auto &identifier : addons) {
        if (auto replacement = ParseReplacementAddon(identifier.asString())) {
          a_slotMap[bipedObject].push_back(*replacement);
        }
      }
    } else {
      a_slotMap.erase(bipedObject);
    }
  }
}
