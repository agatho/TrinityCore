#include "ClassTables.h"
#include "SharedDefines.h"

namespace Playerbot {

uint32 ClassSelfBuff(uint8 cls)
{
    switch (cls)
    {
        case CLASS_MAGE:    return 1459;     // Arcane Intellect
        case CLASS_PRIEST:  return 21562;    // Power Word: Fortitude
        case CLASS_DRUID:   return 1126;     // Mark of the Wild
        case CLASS_WARRIOR: return 6673;     // Battle Shout
        case CLASS_PALADIN: return 0;        // Devotion Aura is passive in modern WoW
        case CLASS_DEATH_KNIGHT: return 0;   // Horn of Winter is Frost-only resource gen
        case CLASS_SHAMAN:  return 462854;   // Skyfury (haste raid buff)
        case CLASS_EVOKER:  return 364342;   // Blessing of the Bronze
        default:            return 0;
    }
}

uint32 ClassOocHeal(uint8 cls, uint32 spec)
{
    switch (cls)
    {
        case CLASS_PRIEST:
            // Disc 256, Holy 257 — Flash Heal is the fast topup for both.
            if (spec == 256 || spec == 257) return 2061;
            break;
        case CLASS_PALADIN:
            if (spec == 65)   return 19750;     // Flash of Light
            break;
        case CLASS_DRUID:
            if (spec == 105)  return 8936;      // Regrowth
            break;
        case CLASS_SHAMAN:
            if (spec == 264)  return 77472;     // Healing Wave
            break;
        case CLASS_MONK:
            if (spec == 270)  return 116670;    // Vivify
            break;
        case CLASS_EVOKER:
            if (spec == 1468) return 361469;    // Living Flame
            break;
        default: break;
    }
    return 0;
}

ClassDispelSpell FriendlyDispel(uint8 cls, uint32 spec)
{
    constexpr uint32 SPEC_PRIEST_DISC = 256, SPEC_PRIEST_HOLY = 257;
    constexpr uint32 SPEC_PALA_HOLY = 65;
    constexpr uint32 SPEC_DRUID_RESTO = 105;
    constexpr uint32 SPEC_SHAMAN_RESTO = 264;
    constexpr uint32 SPEC_MONK_MW = 270;
    constexpr uint32 SPEC_EVOKER_PRES = 1468;
    switch (cls)
    {
        case CLASS_PRIEST:
            if (spec == SPEC_PRIEST_DISC || spec == SPEC_PRIEST_HOLY)
                return {527, /*magic*/ true, /*curse*/ false, /*disease*/ true, /*poison*/ false};
            return {0,0,0,0,0};
        case CLASS_PALADIN:
            if (spec == SPEC_PALA_HOLY)
                return {4987, true, false, true, true};
            return {213644, false, false, true, true};
        case CLASS_DRUID:
            if (spec == SPEC_DRUID_RESTO)
                return {88423, true, true, false, true};
            return {2782, false, true, false, true};
        case CLASS_SHAMAN:
            if (spec == SPEC_SHAMAN_RESTO)
                return {77130, true, true, false, false};
            return {51886, false, true, false, false};
        case CLASS_MONK:
            if (spec == SPEC_MONK_MW)
                return {388874, true, false, true, true};
            return {218164, false, false, true, true};
        case CLASS_MAGE:
            return {475, false, true, false, false};
        case CLASS_EVOKER:
            if (spec == SPEC_EVOKER_PRES)
                return {360823, true, false, false, true};
            return {365585, false, false, false, true};
        default:
            return {0,0,0,0,0};
    }
}

uint32 ClassInterrupt(uint8 cls, uint32 spec)
{
    // Single canonical interrupt per (class, [spec]). Confirmed against
    // Wowhead retail data; spell ids stable since DF.
    switch (cls)
    {
        case CLASS_WARRIOR:      return 6552;     // Pummel
        case CLASS_PALADIN:      return 96231;    // Rebuke
        case CLASS_HUNTER:
            // Counter Shot for BM/MM (253/254); Muzzle for Survival (255).
            if (spec == 255) return 187707;       // Muzzle
            return 147362;                         // Counter Shot
        case CLASS_ROGUE:        return 1766;     // Kick
        case CLASS_PRIEST:
            // Only Shadow has a hard interrupt (Silence 15487) — pure
            // Discipline / Holy have no kick.
            if (spec == 258) return 15487;        // Silence
            return 0;
        case CLASS_DEATH_KNIGHT: return 47528;    // Mind Freeze
        case CLASS_SHAMAN:       return 57994;    // Wind Shear
        case CLASS_MAGE:         return 2139;     // Counterspell
        case CLASS_WARLOCK:
            // Felhunter pet's Spell Lock — needs the pet out and the
            // command spell (19647). Bot-side this becomes a pet_cast,
            // but as a plain "interrupt" the player-side cast is 19647
            // when felhunter is active. Fallback 0 if no Felhunter.
            return 19647;
        case CLASS_MONK:         return 116705;   // Spear Hand Strike
        case CLASS_DRUID:
            // Skull Bash for Feral/Guardian (103/104); Solar Beam (AoE)
            // for Balance (102). Resto has no interrupt.
            if (spec == 103 || spec == 104) return 106839;
            if (spec == 102) return 78675;
            return 0;
        case CLASS_DEMON_HUNTER: return 183752;   // Disrupt
        case CLASS_EVOKER:       return 351338;   // Quell
        default:                 return 0;
    }
}

uint32 ClassCC(uint8 cls, uint32 spec)
{
    // Single canonical CC per class. Many classes have multiple CCs
    // (Mage Poly + Frost Nova; Hunter Trap + Sleep Dart) — we pick
    // the longest reliable single-target one usable at any spec.
    switch (cls)
    {
        case CLASS_MAGE:         return 118;      // Polymorph
        case CLASS_WARLOCK:      return 5782;     // Fear
        case CLASS_ROGUE:        return 6770;     // Sap
        case CLASS_HUNTER:       return 187650;   // Freezing Trap
        case CLASS_DRUID:        return 2637;     // Hibernate (beasts/dragonkin only — caller checks target type)
        case CLASS_SHAMAN:       return 51514;    // Hex
        case CLASS_PRIEST:       return 605;      // Mind Control (32yd)
        case CLASS_PALADIN:
            // Repentance (Ret talent, 20066). Holy/Prot don't have it
            // baseline; fall back to Hammer of Justice (853) — short
            // stun but works for any spec.
            if (spec == 70) return 20066;
            return 853;
        case CLASS_DEATH_KNIGHT: return 47476;    // Strangulate (PvP talent, but core CC)
        case CLASS_MONK:         return 115078;   // Paralysis
        case CLASS_DEMON_HUNTER: return 217832;   // Imprison
        case CLASS_EVOKER:       return 360806;   // Sleep Walk
        case CLASS_WARRIOR:      return 0;        // No reliable single-target CC
        default:                 return 0;
    }
}

// ---- Role / spec helpers (canonical source — referenced by
//      BotQueueFiller and idle:dual_spec_switch alike) ----
bool IsTankSpec(uint8 cls, uint16 spec)
{
    if (cls == CLASS_WARRIOR && spec == 73)  return true;       // Protection
    if (cls == CLASS_PALADIN && spec == 66)  return true;       // Protection
    if (cls == CLASS_DEATH_KNIGHT && spec == 250) return true;  // Blood
    if (cls == CLASS_DRUID && spec == 104)   return true;       // Guardian
    if (cls == CLASS_MONK && spec == 268)    return true;       // Brewmaster
    if (cls == CLASS_DEMON_HUNTER && spec == 581) return true;  // Vengeance
    return false;
}

bool IsHealerSpec(uint8 cls, uint16 spec)
{
    if (cls == CLASS_PALADIN && spec == 65)  return true;       // Holy
    if (cls == CLASS_PRIEST && (spec == 256 || spec == 257)) return true;  // Disc/Holy
    if (cls == CLASS_DRUID && spec == 105)   return true;       // Restoration
    if (cls == CLASS_SHAMAN && spec == 264)  return true;       // Restoration
    if (cls == CLASS_MONK && spec == 270)    return true;       // Mistweaver
    if (cls == CLASS_EVOKER && spec == 1468) return true;       // Preservation
    return false;
}

uint32 TankSpecForClass(uint8 cls)
{
    switch (cls)
    {
        case CLASS_WARRIOR:      return 73;   // Protection
        case CLASS_PALADIN:      return 66;   // Protection
        case CLASS_DEATH_KNIGHT: return 250;  // Blood
        case CLASS_DRUID:        return 104;  // Guardian
        case CLASS_MONK:         return 268;  // Brewmaster
        case CLASS_DEMON_HUNTER: return 581;  // Vengeance
        default:                 return 0;
    }
}

uint32 ClassTaunt(uint8 cls)
{
    switch (cls)
    {
        case CLASS_WARRIOR:      return 355;     // Taunt
        case CLASS_PALADIN:      return 62124;   // Hand of Reckoning (taunt on cast)
        case CLASS_DEATH_KNIGHT: return 56222;   // Dark Command (single-target taunt)
        case CLASS_DRUID:        return 6795;    // Growl (bear-form required; caller checks)
        case CLASS_MONK:         return 115546;  // Provoke
        case CLASS_DEMON_HUNTER: return 185245;  // Torment
        default:                 return 0;
    }
}

uint32 HealerSpecForClass(uint8 cls)
{
    switch (cls)
    {
        case CLASS_PALADIN: return 65;    // Holy
        case CLASS_PRIEST:  return 257;   // Holy (Disc 256 also valid; we pick one canonical)
        case CLASS_DRUID:   return 105;   // Restoration
        case CLASS_SHAMAN:  return 264;   // Restoration
        case CLASS_MONK:    return 270;   // Mistweaver
        case CLASS_EVOKER:  return 1468;  // Preservation
        default:            return 0;
    }
}

bool IsRangedSpec(uint8 cls, uint16 spec)
{
    switch (cls)
    {
        case CLASS_MAGE:                 return true;          // all specs
        case CLASS_WARLOCK:              return true;          // all specs
        case CLASS_PRIEST:                                    // Shadow only
            return spec == 258;
        case CLASS_HUNTER:                                    // BM(253) + MM(254);
            return spec == 253 || spec == 254;                // Survival(255) is melee
        case CLASS_DRUID:                                     // Balance only
            return spec == 102;
        case CLASS_SHAMAN:                                    // Elemental only
            return spec == 262;
        case CLASS_EVOKER:                                    // Devastation(1467) + Augmentation(1473)
            return spec == 1467 || spec == 1473;
        default:                         return false;
    }
}

uint32 ClassMobilityEscape(uint8 cls, uint16 spec)
{
    switch (cls)
    {
        case CLASS_MAGE:         return 1953;     // Blink — 20y forward
        case CLASS_HUNTER:       return 781;      // Disengage — 13y back-flip
        case CLASS_ROGUE:        return 2983;     // Sprint — 70% MS 8s
        case CLASS_DEMON_HUNTER: return 198793;   // Vengeful Retreat
        case CLASS_DEATH_KNIGHT: return 212552;   // Wraith Walk (talented)
        case CLASS_DRUID:                         // Dash for Feral, Stampeding Roar for others
            return spec == 103 ? 1850u : 77764u;
        case CLASS_MONK:         return 109132;   // Roll
        case CLASS_PALADIN:      return 190784;   // Divine Steed
        case CLASS_WARRIOR:      return 6544;     // Heroic Leap
        case CLASS_WARLOCK:      return 48020;    // Demonic Circle: Teleport
        case CLASS_SHAMAN:       return 58875;    // Spirit Walk (Enh) / Ghost Wolf others
        case CLASS_PRIEST:       return spec == 258 ? 109964u : 0u; // Spirit Shell only Shadow has mobility
        case CLASS_EVOKER:       return 358267;   // Hover (instant)
        default:                 return 0;
    }
}

uint32 ClassOffensiveBurst(uint8 cls, uint16 spec)
{
    switch (cls)
    {
        case CLASS_WARRIOR:
            // Arms 71 / Fury 72 use Recklessness; Prot 73 has it but defensive
            if (spec == 73) return 0;             // Prot tank — no burst CD
            return 1719;                          // Recklessness
        case CLASS_PALADIN:
            if (spec == 65)  return 0;            // Holy — heal, no burst
            if (spec == 66)  return 0;            // Prot — defensive cooldowns
            return 31884;                         // Avenging Wrath (Ret 70)
        case CLASS_HUNTER:
            return 19574;                         // Bestial Wrath (BM 253) / others use it too via talent
        case CLASS_ROGUE:
            if (spec == 259) return 121471;       // Shadow Blades (Assassination)
            if (spec == 260) return 13750;        // Adrenaline Rush (Outlaw)
            if (spec == 261) return 185313;       // Shadow Dance (Subtlety)
            return 0;
        case CLASS_PRIEST:
            if (spec == 258) return 228260;       // Void Eruption (Shadow)
            return 0;                             // Disc/Holy — heal
        case CLASS_DEATH_KNIGHT:
            if (spec == 250) return 0;            // Blood — defensive cooldowns
            if (spec == 251) return 51271;        // Pillar of Frost (Frost)
            if (spec == 252) return 47568;        // Empower Rune Weapon (Unholy / shared)
            return 0;
        case CLASS_SHAMAN:
            if (spec == 264) return 0;            // Resto — heal
            if (spec == 262) return 191634;       // Stormkeeper (Elemental)
            if (spec == 263) return 51533;        // Feral Spirit (Enhancement)
            return 0;
        case CLASS_MAGE:
            if (spec == 62)  return 12042;        // Arcane Power
            if (spec == 63)  return 190319;       // Combustion (Fire)
            if (spec == 64)  return 12472;        // Icy Veins (Frost)
            return 0;
        case CLASS_WARLOCK:
            if (spec == 265) return 205180;       // Summon Darkglare (Affliction)
            if (spec == 266) return 265187;       // Summon Demonic Tyrant (Demonology)
            if (spec == 267) return 1122;         // Summon Infernal (Destruction)
            return 0;
        case CLASS_MONK:
            if (spec == 269) return 137639;       // Storm, Earth, and Fire (Windwalker)
            return 0;                             // Brewmaster/Mistweaver — no PvP burst
        case CLASS_DRUID:
            if (spec == 102) return 194223;       // Celestial Alignment (Balance)
            if (spec == 103) return 106951;       // Berserk (Feral)
            return 0;
        case CLASS_DEMON_HUNTER:
            if (spec == 577) return 191427;       // Metamorphosis (Havoc)
            return 0;                             // Vengeance — defensive
        case CLASS_EVOKER:
            if (spec == 1467) return 375087;      // Dragonrage (Devastation)
            if (spec == 1473) return 395152;      // Ebon Might (Augmentation)
            return 0;
        default:
            return 0;
    }
}

uint32 ClassLustSpell(uint8 cls)
{
    switch (cls)
    {
        case CLASS_SHAMAN:  return 2825;     // Bloodlust (game routes to Heroism for Alliance via Player::CastSpell faction-aware)
        case CLASS_MAGE:    return 80353;    // Time Warp
        case CLASS_HUNTER:  return 264667;   // Primal Rage (pet)
        case CLASS_EVOKER:  return 390386;   // Fury of the Aspects
        default:            return 0;
    }
}

} // namespace Playerbot
