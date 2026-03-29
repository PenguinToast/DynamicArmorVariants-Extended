#pragma once

namespace RE {
namespace Offset {
namespace Actor {
// SkyrimSE 1.6.353: 0x2A69D0
inline constexpr auto Update3D = MAKE_OFFSET(19316, 19743, 0x2A5AC0);
} // namespace Actor

namespace InventoryChanges {
// SkyrimSE 1.6.353: 0x1E4AA0
inline constexpr auto GetWornMask = MAKE_OFFSET(15806, 16044, 0x1E9BE0);
// SkyrimSE 1.6.353: 0x1F08A0
inline constexpr auto Accept = MAKE_OFFSET(15856, 16096, 0x1F5D90);
} // namespace InventoryChanges

namespace InventoryUtils {
namespace GetWornMaskVisitor {
// SkyrimSE 1.6.353: 0x1FB510
inline constexpr auto Visit = MAKE_OFFSET(15991, 16233, 0x2009A0);
} // namespace GetWornMaskVisitor
} // namespace InventoryUtils

namespace TESNPC {
// SkyrimSE 1.6.353: 0x37B430
inline constexpr auto InitWornForm = MAKE_OFFSET(24232, 24736, 0x373CB0);
} // namespace TESNPC

namespace TESObjectARMA {
// SkyrimSE 1.6.353: 0x235A00
inline constexpr auto HasRace = MAKE_OFFSET(17359, 17757, 0x2380A0);
// SkyrimSE 1.6.353: 0x235D10
inline constexpr auto InitWornArmorAddon = MAKE_OFFSET(17361, 17759, 0x2383A0);
} // namespace TESObjectARMA
} // namespace Offset
} // namespace RE
