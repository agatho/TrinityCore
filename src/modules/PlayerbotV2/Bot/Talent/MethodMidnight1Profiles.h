// Generated from method.gg class guides for WoW Midnight (12.0.5).
// Source pattern: https://www.method.gg/guides/<spec>-<class>/talents
//
// Coverage: 14 specs simc/midnight does not publish profiles for —
//   8 healers + Augmentation Evoker (no simc support for healing/support)
//   6 Hunter/DK specs whose simc profiles encode older 12.0.1 trait IDs and
//   no longer align against current 12.0.5 trait_data.inc.
//
// Each entry's `talents` is the in-game Blizzard import string (same simc
// loadout encoding) and decodes via the existing SimcLoadoutDecoder against
// current 12.0.5 trait_data.

#pragma once

#include <cstdint>
#include <string_view>

namespace Playerbot::V2::Talent {

struct MethodRaidProfile
{
    uint8_t          class_id; // ChrClasses.db2 id
    uint16_t         spec_id;  // ChrSpecialization.db2 id
    std::string_view label;    // human-readable label, includes hero spec
    std::string_view talents;  // Blizzard import string (base64)
};

inline constexpr MethodRaidProfile kMethodMidnight1Profiles[] = {
    // Healers + Augmentation (8) — not in simc
    { 2,   65, "method.gg Holy Paladin (Herald of the Sun)",          "CEEAAAAAAAAAAAAAAAAAAAAAAAAAAYBAMDAAsMmZGzYmZ2YMGzyYbmZxMNxwYmZYY2yAwAwGYjlZMzysNzMbNAAAALgB2MMmxAAAMzwMGjGA"},
    { 5,  256, "method.gg Discipline Priest (Oracle)",                "CAQAAAAAAAAAAAAAAAAAAAAAAADsAz2MzMYmhZbmtZmZmhZAAAAAAAAAAMDLzgZmZYGmBmpZamBYmFMEGzyAMGsAAAjxMjBzAMzMaGG"},
    { 5,  257, "method.gg Holy Priest (Oracle)",                      "CEQAAAAAAAAAAAAAAAAAAAAAAwYAAAAAAAGjZmtZmZMzMDzMDLzwMAAAAmhlZYmZmhZYGAzUDgZWwQYMLDwYgFGzGgmxYMGmZAmZmBG"},
    { 7,  264, "method.gg Restoration Shaman (Totemic Downpour)",     "CgQAAAAAAAAAAAAAAAAAAAAAAAAAAgBAAAAzMzMLLbDzwYmZmZGzYB2gZsox2AyMwGjhZsNGz0stMzwMmFWMzMjZYWGAAYAzMDmZAgBD"},
    {10,  270, "method.gg Mistweaver Monk (Conduit of the Celestials)","C4QAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAghx2MwmFzYmZbGbYmZYmlttZGLMjmxMgBDGWmZmZYWGMYxEAAAAABYxyMLz2MDAAMgBYGwYYsMZMDA"},
    {11,  105, "method.gg Restoration Druid (Wildstalker)",           "CkGAAAAAAAAAAAAAAAAAAAAAAMjxMbz2MmZGz2wDwMzmxCzAAAAAAAAAAgNoZzMmmZgxsMzMzMMMDAAAAAAAAAgAAAmtZWa2mZzGjZmhZGY0MAAzMAMA"},
    {13, 1468, "method.gg Preservation Evoker (Chronowarden Raid)",   "CwbBAAAAAAAAAAAAAAAAAAAAAAAAAAAYmZ2WmHADzMmNjZmZWmxAAAzYGDmxMyMzAAAAMzMTmxMjZbmZAwAjZsxCMwMaoBsAjZGgxA"},
    {13, 1473, "method.gg Augmentation Evoker (Chronowarden ST+Cleave)","CEcBAAAAAAAAAAAAAAAAAAAAAMmZmZbmZmxyAzsMjxwMAAAAAgBAAzMDMYM1YmZGAAAAMjZmxMzyYmBmZzAjZswCMwMM0IWwMjZGAYA"},

    // Hunter + DK (6) — replace stale simc 12.0.1 strings; method.gg builds
    // are current 12.0.5 and align cleanly against 12.0.5 trait_data.inc.
    { 3,  253, "method.gg Beast Mastery Hunter (Pack Leader Single Target)",
                                                                       "C0PAAAAAAAAAAAAAAAAAAAAAAAMmxwCsAzwQDbAAYG2GzsNzwMmZYYmxYmxMzYGzwMzYGzgx0MAAAAAmBAAgxMzMgZ2Q2gZBsNA"},
    { 3,  254, "method.gg Marksmanship Hunter (Sentinel Cleave)",     "C4PAAAAAAAAAAAAAAAAAAAAAAwCMwMGNWGAzgNAAAAAAAAgZMzMDzYmZMDGTzYwstxMzYmZmZmZhZWGmZAAAjZmZAYmpNwAsxMzM"},
    { 3,  255, "method.gg Survival Hunter (Pack Leader Single Target)","C8PAAAAAAAAAAAAAAAAAAAAAAMgxMGWIbwMM0gFjZmZmxyAAAAAAgZMzMDz4BMjZwYaGAAAAAAjllZmZxMzMzYmxAmZDwsMjxM2MA"},
    { 6,  250, "method.gg Blood Death Knight (San'layn Single Target)","CoPAAAAAAAAAAAAAAAAAAAAAAwYWGzMmxMzMMLzMz0MLGzMmxAAAAAzMzMzMzMDzYMAgZmZGAAADMwM20YZDklBsBYGmBAAmZghB"},
    { 6,  251, "method.gg Frost Death Knight (Breath of Sindragosa)", "CsPAAAAAAAAAAAAAAAAAAAAAAMDwMjZMDY2mZmZmZZmZkZMmZYGGPgZGMzMzMDAAAAAAAAAjZbgBsAWGmQGLYmxMzAzAYYmBYmBD"},
    { 6,  252, "method.gg Unholy Death Knight (Riders Single Target)","CwPAAAAAAAAAAAAAAAAAAAAAAAwMjZMDDz2MzMTzmZmZMjBAAAAAAAgZGmZAw2MmZ2mZGjZAbmFDDZgZjhGLAYGAGzMjZAmZmxYA"},
};

inline constexpr size_t kMethodMidnight1ProfileCount =
    sizeof(kMethodMidnight1Profiles) / sizeof(kMethodMidnight1Profiles[0]);

} // namespace Playerbot::V2::Talent
