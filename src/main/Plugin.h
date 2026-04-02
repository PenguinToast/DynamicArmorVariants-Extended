#pragma once

#ifndef DAVE_VERSION_MAJOR
#error "DAVE_VERSION_MAJOR must be provided by the build system"
#endif

#ifndef DAVE_VERSION_MINOR
#error "DAVE_VERSION_MINOR must be provided by the build system"
#endif

#ifndef DAVE_VERSION_PATCH
#error "DAVE_VERSION_PATCH must be provided by the build system"
#endif

#ifndef DAVE_VERSION_STRING
#error "DAVE_VERSION_STRING must be provided by the build system"
#endif

namespace Plugin {
inline constexpr auto NAME = "DynamicArmorVariants"sv;
inline constexpr REL::Version VERSION{DAVE_VERSION_MAJOR, DAVE_VERSION_MINOR,
                                      DAVE_VERSION_PATCH, 0};
inline constexpr std::string_view VERSION_STRING{DAVE_VERSION_STRING};
inline constexpr auto AUTHOR = "Parapets, PenguinToast"sv;
} // namespace Plugin
