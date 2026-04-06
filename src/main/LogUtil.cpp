#include "LogUtil.h"

namespace LogUtil {
auto GetFormName(const RE::TESForm *a_form) -> std::string {
  if (!a_form) {
    return "<null>"s;
  }

  if (const auto *name = a_form->GetName(); name && name[0] != '\0') {
    return name;
  }

  if (const auto *editorID = a_form->GetFormEditorID();
      editorID && editorID[0] != '\0') {
    return editorID;
  }

  return "<unnamed>"s;
}

auto FormatFormID(RE::FormID a_formID) -> std::string {
  char buffer[16]{};
  std::snprintf(buffer, sizeof(buffer), "%08X", a_formID);
  return buffer;
}

auto DescribeArmorAddon(const RE::TESObjectARMA *a_armorAddon) -> std::string {
  if (!a_armorAddon) {
    return "<null armor addon>"s;
  }

  return GetFormName(a_armorAddon) + " [" +
         FormatFormID(a_armorAddon->GetFormID()) + "] slots=" +
         FormatFormID(static_cast<RE::FormID>(
             a_armorAddon->bipedModelData.bipedObjectSlots.underlying()));
}

auto DescribeArmor(const RE::TESObjectARMO *a_armor) -> std::string {
  if (!a_armor) {
    return "<null armor>"s;
  }

  return GetFormName(a_armor) + " [" + FormatFormID(a_armor->GetFormID()) +
         "] slots=" +
         FormatFormID(static_cast<RE::FormID>(
             a_armor->bipedModelData.bipedObjectSlots.underlying()));
}

auto JoinDescriptions(const std::vector<std::string> &a_values) -> std::string {
  std::string result;
  for (const auto &value : a_values) {
    if (!result.empty()) {
      result += "; ";
    }
    result += value;
  }
  return result;
}

auto DescribeOverrideOption(ArmorVariant::OverrideOption a_option)
    -> std::string_view {
  switch (a_option) {
  case ArmorVariant::OverrideOption::Undefined:
    return "Undefined"sv;
  case ArmorVariant::OverrideOption::None:
    return "None"sv;
  case ArmorVariant::OverrideOption::ShowAll:
    return "ShowAll"sv;
  case ArmorVariant::OverrideOption::ShowHead:
    return "ShowHead"sv;
  case ArmorVariant::OverrideOption::HideHair:
    return "HideHair"sv;
  case ArmorVariant::OverrideOption::HideAll:
    return "HideAll"sv;
  }

  return "Unknown"sv;
}

void LogHookInstalled(const std::string_view a_name,
                      const std::string_view a_detail) {
  if (a_detail.empty()) {
    logger::info("Hook install: {}: installed"sv, a_name);
    return;
  }

  logger::info("Hook install: {}: installed ({})"sv, a_name, a_detail);
}

void LogHookSkipped(const std::string_view a_name,
                    const std::string_view a_reason,
                    const spdlog::level::level_enum a_level) {
  switch (a_level) {
  case spdlog::level::warn:
    logger::warn("Hook install: {}: skipped ({})"sv, a_name, a_reason);
    return;
  case spdlog::level::err:
  case spdlog::level::critical:
    logger::error("Hook install: {}: skipped ({})"sv, a_name, a_reason);
    return;
  default:
    logger::info("Hook install: {}: skipped ({})"sv, a_name, a_reason);
    return;
  }
}
} // namespace LogUtil
