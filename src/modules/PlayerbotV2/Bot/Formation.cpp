#include "Formation.h"
#include <cmath>
#include <cstring>

namespace Playerbot {

namespace {
constexpr float kPi = 3.14159265358979323846f;
}

FormationOffset ComputeFormationOffset(
    FormationType type, uint8_t slot, float default_distance)
{
    FormationOffset out;
    out.distance = default_distance;
    switch (type)
    {
        case FormationType::Free:
            // No offset — caller emits a plain follow.
            out.angle_radians = 0.0f;
            return out;

        case FormationType::Tight:
            // Stack on leader. Tiny distance so collision boxes don't
            // visibly clip but bots are essentially "on" the leader.
            out.distance      = 1.5f;
            out.angle_radians = 0.0f;
            return out;

        case FormationType::Spread:
        {
            // Ring 8y around leader; slots evenly spaced. With 8 slots
            // each gets 45°. Modulo 16 so >16 bots wrap (rare).
            out.distance = 8.0f;
            out.angle_radians = kPi * 2.0f * float(slot % 16) / 16.0f;
            return out;
        }

        case FormationType::Line:
        {
            // Behind leader, lateral fan. Slot 0 = directly behind;
            // odd slots go right, even slots go left of slot 0.
            // Result: behind-centre with lateral spread of ~4y/slot.
            const int s = static_cast<int>(slot);
            const int side = (s % 2 == 1) ? 1 : -1;
            const int rank = (s + 1) / 2;
            // Offset vector in leader-local frame: (-distance, ±rank*4)
            const float dx = -default_distance;
            const float dy = static_cast<float>(side) * static_cast<float>(rank) * 4.0f;
            out.distance      = std::sqrt(dx * dx + dy * dy);
            out.angle_radians = std::atan2(dy, dx);
            // Normalise to [0, 2pi) so callers can rely on positive.
            if (out.angle_radians < 0.0f) out.angle_radians += 2.0f * kPi;
            return out;
        }

        case FormationType::Column:
        {
            // Behind leader, single file. Slot 0 immediately behind,
            // slot N is N*4y further back.
            out.distance      = default_distance + static_cast<float>(slot) * 4.0f;
            out.angle_radians = 0.0f;     // directly behind
            return out;
        }

        case FormationType::Wedge:
        {
            // V-shape behind leader, slot 0 = primary point of the V
            // (closest), then alternating flanks fanning out + back.
            const int s = static_cast<int>(slot);
            if (s == 0)
            {
                out.distance      = default_distance;
                out.angle_radians = 0.0f;
                return out;
            }
            const int side = (s % 2 == 1) ? 1 : -1;
            const int rank = (s + 1) / 2;
            const float dx = -default_distance - static_cast<float>(rank) * 2.0f;
            const float dy = static_cast<float>(side) * static_cast<float>(rank) * 3.0f;
            out.distance      = std::sqrt(dx * dx + dy * dy);
            out.angle_radians = std::atan2(dy, dx);
            if (out.angle_radians < 0.0f) out.angle_radians += 2.0f * kPi;
            return out;
        }

        case FormationType::Circle:
        {
            // Ring 12y around leader, evenly distributed across 12 slots.
            out.distance      = 12.0f;
            out.angle_radians = kPi * 2.0f * float(slot % 12) / 12.0f;
            return out;
        }
    }
    return out;
}

FormationType ParseFormationType(char const* token)
{
    if (!token) return FormationType::Free;
    auto eq = [token](char const* s) { return std::strcmp(token, s) == 0; };
    if (eq("free"))    return FormationType::Free;
    if (eq("tight"))   return FormationType::Tight;
    if (eq("stack"))   return FormationType::Tight;     // alias
    if (eq("spread"))  return FormationType::Spread;
    if (eq("line"))    return FormationType::Line;
    if (eq("column"))  return FormationType::Column;
    if (eq("col"))     return FormationType::Column;    // alias
    if (eq("wedge"))   return FormationType::Wedge;
    if (eq("v"))       return FormationType::Wedge;     // alias
    if (eq("circle"))  return FormationType::Circle;
    if (eq("ring"))    return FormationType::Circle;    // alias
    return FormationType::Free;
}

char const* FormationTypeName(FormationType t)
{
    switch (t)
    {
        case FormationType::Free:   return "free";
        case FormationType::Tight:  return "tight";
        case FormationType::Spread: return "spread";
        case FormationType::Line:   return "line";
        case FormationType::Column: return "column";
        case FormationType::Wedge:  return "wedge";
        case FormationType::Circle: return "circle";
    }
    return "?";
}

} // namespace Playerbot
