#pragma once

namespace Patches {
struct SkillLevelingVisitor;
}

namespace SkillLeveling {
auto FixArmorCounts(RE::BipedAnim *a_biped,
                    Patches::SkillLevelingVisitor *a_visitor) -> bool;
} // namespace SkillLeveling
