#pragma once

namespace RE {
namespace Offset {
namespace Actor {
// SkyrimSE 1.6.353: 0x2A69D0
inline constexpr auto Update3D = MAKE_RELOCATION(19316, 19743);
// SkyrimSE 1.6.353: 0x635740
// SkyrimVR uses the pre-AE function family for this hook, but the current VR
// address library release does not expose ID 36979 yet, so keep using the
// verified raw VR offset until the database catches up.
inline constexpr auto FixEquipConflictCheck =
    MAKE_OFFSET(36979, 38004, 0x6155F0);
} // namespace Actor

namespace InventoryChanges {
// SkyrimSE 1.6.353: 0x1E4AA0
inline constexpr auto GetWornMask = MAKE_RELOCATION(15806, 16044);
// SkyrimSE 1.6.353: 0x1F08A0
inline constexpr auto Accept = MAKE_RELOCATION(15856, 16096);
} // namespace InventoryChanges

namespace InventoryUtils {
namespace GetWornMaskVisitor {
// SkyrimSE 1.6.353: 0x1FB510
inline constexpr auto Visit = MAKE_RELOCATION(15991, 16233);
} // namespace GetWornMaskVisitor
} // namespace InventoryUtils

namespace TESNPC {
// SkyrimSE 1.6.353: 0x37B430
inline constexpr auto InitWornForm = MAKE_RELOCATION(24232, 24736);
} // namespace TESNPC

namespace TESObjectARMA {
// SkyrimSE 1.6.353: 0x235A00
inline constexpr auto HasRace = MAKE_RELOCATION(17359, 17757);
// SkyrimSE 1.6.353: 0x235D10
inline constexpr auto InitWornArmorAddon = MAKE_RELOCATION(17361, 17759);
} // namespace TESObjectARMA

namespace BGSBipedObjectForm {
// SkyrimSE 1.6.353: 0x18B130
// The verified VR mapping exists, but the current VR address library release
// does not expose ID 14026 yet, so keep using the raw VR offset for now.
inline constexpr auto TestBodyPartByIndex = MAKE_OFFSET(14026, 14119, 0x191E20);
} // namespace BGSBipedObjectForm
} // namespace Offset
} // namespace RE
