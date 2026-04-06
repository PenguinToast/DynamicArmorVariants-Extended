#pragma once

#include "ArmorVariant.h"

#define DAV_LOG_DEBUG_LAZY(...)                                                \
  do {                                                                         \
    auto *const davLog = spdlog::default_logger_raw();                         \
    if (davLog != nullptr && davLog->should_log(spdlog::level::debug)) {       \
      logger::debug(__VA_ARGS__);                                              \
    }                                                                          \
  } while (false)

namespace LogUtil {
auto GetFormName(const RE::TESForm *a_form) -> std::string;
auto FormatFormID(RE::FormID a_formID) -> std::string;
auto DescribeActor(const RE::Actor *a_actor) -> std::string;
auto DescribeArmorAddon(const RE::TESObjectARMA *a_armorAddon) -> std::string;
auto DescribeArmor(const RE::TESObjectARMO *a_armor) -> std::string;
auto DescribeStringArg(std::string_view a_value,
                       std::size_t a_maxLength = 160) -> std::string;
auto JoinDescriptions(const std::vector<std::string> &a_values) -> std::string;
auto DescribeOverrideOption(ArmorVariant::OverrideOption a_option)
    -> std::string_view;
void LogHookInstalled(std::string_view a_name,
                      std::string_view a_detail = {});
void LogHookSkipped(std::string_view a_name, std::string_view a_reason,
                    spdlog::level::level_enum a_level = spdlog::level::info);
} // namespace LogUtil
