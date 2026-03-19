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
} // namespace LogUtil
