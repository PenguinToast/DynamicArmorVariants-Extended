#pragma once

#ifndef DAVE_VERSION_MAJOR
#define DAVE_VERSION_MAJOR 1
#endif

#ifndef DAVE_VERSION_MINOR
#define DAVE_VERSION_MINOR 1
#endif

#ifndef DAVE_VERSION_PATCH
#define DAVE_VERSION_PATCH 0
#endif

#ifndef DAVE_VERSION_STRING
#define DAVE_VERSION_STRING "1.1.0-dev"
#endif

namespace Plugin {
inline constexpr auto NAME = "DynamicArmorVariants"sv;
inline constexpr REL::Version VERSION{DAVE_VERSION_MAJOR, DAVE_VERSION_MINOR,
                                      DAVE_VERSION_PATCH, 0};
inline constexpr std::string_view VERSION_STRING{DAVE_VERSION_STRING};
inline constexpr auto AUTHOR = "Parapets"sv;
} // namespace Plugin
