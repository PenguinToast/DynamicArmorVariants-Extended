#pragma once

#include "ArmorVariant.h"

namespace LogUtil {
auto GetFormName(const RE::TESForm *a_form) -> std::string;
auto FormatFormID(RE::FormID a_formID) -> std::string;
auto DescribeArmorAddon(const RE::TESObjectARMA *a_armorAddon) -> std::string;
auto DescribeArmor(const RE::TESObjectARMO *a_armor) -> std::string;
auto JoinDescriptions(const std::vector<std::string> &a_values) -> std::string;
auto DescribeOverrideOption(ArmorVariant::OverrideOption a_option)
    -> std::string_view;
void LogHookInstalled(std::string_view a_name,
                      std::string_view a_detail = {});
void LogHookSkipped(std::string_view a_name, std::string_view a_reason,
                    spdlog::level::level_enum a_level = spdlog::level::info);
} // namespace LogUtil
