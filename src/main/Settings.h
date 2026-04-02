#pragma once

struct Settings {
  // Startup policy: if settings are missing or unreadable, leave this false
  // and skip installing the equip conflict hook.
  bool installEquipConflictHook{false};
  // Startup policy: if settings are missing or unreadable, leave this false
  // and keep the legacy ARMO/addon mask aggregation behavior.
  bool useOwnershipBasedArmorMasks{false};

  static auto Load() -> Settings;
  static auto Get() -> const Settings &;
};
