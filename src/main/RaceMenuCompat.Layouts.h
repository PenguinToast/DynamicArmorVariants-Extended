#pragma once

#include "RaceMenuCompatInternal.h"

#include <optional>
#include <string_view>
#include <utility>

namespace RaceMenuCompat::detail {
auto FindLoadedSkeeModule() -> std::pair<HMODULE, const wchar_t *>;
auto DescribeModuleName(const wchar_t *a_moduleName) -> std::string_view;
auto ReadModuleVersion(HMODULE a_module) -> std::optional<REL::Version>;
auto FindHookLayout(const wchar_t *a_moduleName, const REL::Version &a_version,
                    std::uint32_t a_timeDateStamp) -> const HookLayout *;
} // namespace RaceMenuCompat::detail
