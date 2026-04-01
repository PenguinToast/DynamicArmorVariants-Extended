#pragma once

struct Settings {
  // Startup policy: if settings are missing or unreadable, leave this false
  // and skip installing the equip conflict hook.
  bool installEquipConflictHook{false};

  static auto Load() -> Settings;
};
