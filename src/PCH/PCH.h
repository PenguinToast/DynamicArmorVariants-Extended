#pragma once

#pragma warning(disable : 5105)
#pragma warning(push, 0)
#include <tsl/ordered_map.h>
#include <tsl/ordered_set.h>

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#ifdef NDEBUG
#include <spdlog/sinks/basic_file_sink.h>
#else
#include <spdlog/sinks/msvc_sink.h>
#endif

#include <regex>
#include <xbyak/xbyak.h>
#pragma warning(pop)

using namespace std::literals;

namespace fs = std::filesystem;

namespace logger = SKSE::log;

using BipedObject = RE::BIPED_OBJECTS::BIPED_OBJECT;
using BipedObjectSlot = RE::BIPED_MODEL::BipedObjectSlot;

namespace util {
using SKSE::stl::enumeration;
using SKSE::stl::report_and_fail;
using std::to_underlying;

inline std::string str_toupper(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
    return static_cast<char>(std::toupper(c));
  });
  return s;
}

inline auto MakeHook(REL::ID a_id, std::ptrdiff_t a_offset = 0) {
  return REL::Relocation<std::uintptr_t>(a_id, a_offset);
}

inline auto MakeHook(REL::VariantID a_id, std::ptrdiff_t a_offset = 0) {
  return REL::Relocation<std::uintptr_t>(a_id.address() + a_offset);
}

inline auto MakeHook(REL::Offset a_address, std::ptrdiff_t a_offset = 0) {
  return REL::Relocation<std::uintptr_t>(a_address.address() + a_offset);
}

inline auto MakeHook(REL::VariantOffset a_address,
                     std::ptrdiff_t a_offset = 0) {
  return REL::Relocation<std::uintptr_t>(a_address.address() + a_offset);
}
} // namespace util

#define MAKE_OFFSET(a_idSE, a_offsetVR)                                        \
  REL::VariantID((a_idSE), (a_idSE), (a_offsetVR))

#define DLLEXPORT __declspec(dllexport)

#include "Plugin.h"
