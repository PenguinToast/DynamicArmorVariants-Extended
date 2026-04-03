#pragma once

#include <chrono>

struct Settings {
  static constexpr std::size_t DefaultRefreshVariantCacheCapacity = 25000;
  static constexpr auto DefaultRefreshVariantCacheTtl =
      std::chrono::milliseconds(5000);

  // Startup policy: if settings are missing or unreadable, leave this false
  // and skip installing the equip conflict hook.
  bool installEquipConflictHook{false};
  // Startup policy: if settings are missing or unreadable, leave this false
  // and keep the legacy ARMO/addon mask aggregation behavior.
  bool useOwnershipBasedArmorMasks{false};
  // Startup policy: if settings are missing or unreadable, leave this false
  // and skip installing optional RaceMenu/NiOverride compatibility hooks.
  bool installRaceMenuCompatHooks{false};
  // Bound for the refresh-time armor resolution caches. Larger values trade
  // memory for fewer evictions during variant refresh checks.
  std::size_t refreshVariantCacheCapacity{DefaultRefreshVariantCacheCapacity};
  // TTL for the refresh-time armor resolution caches. Higher values reduce
  // recomputation but keep stale entries resident longer.
  std::chrono::milliseconds refreshVariantCacheTtl{
      DefaultRefreshVariantCacheTtl};

  static auto Load() -> Settings;
  static auto Get() -> const Settings &;
};
