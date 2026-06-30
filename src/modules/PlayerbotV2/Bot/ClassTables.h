// ClassTables - Per-(class,spec) lookup tables shared between auto-rules
// (State_Idle) and owner-driven whisper commands. Defines:
//   - ClassSelfBuff(cls)            → 1hr+ raid-frame buff spell id
//   - ClassOocHeal(cls, spec)       → spec basic single-target heal
//   - FriendlyDispel(cls, spec)     → friendly dispel spell + types
// All return 0 (or empty struct) when the (class,spec) has no offering —
// callers gate on the zero value.

#pragma once

#include "BotTypes.h"

namespace Playerbot {

uint32 ClassSelfBuff(uint8 cls);

uint32 ClassOocHeal(uint8 cls, uint32 spec);

struct ClassDispelSpell
{
    uint32 spell_id;
    bool   magic;
    bool   curse;
    bool   disease;
    bool   poison;
};
ClassDispelSpell FriendlyDispel(uint8 cls, uint32 spec);

// Single-target hard interrupt per (class, spec). Used by the
// /interrupt squad command — owner targets a casting enemy and the
// addressed bot kicks. Returns 0 when the class/spec lacks a
// reliable interrupt (Holy Priest, fresh sub-level chars without
// the spell yet). Caller checks knows_spell() before emitting.
uint32 ClassInterrupt(uint8 cls, uint32 spec);

// Single-target crowd-control (CC) per class. Owner uses /cc to
// take a non-priority enemy out of the fight; the resolver picks
// the canonical CC: Polymorph for mages, Sap for rogues, Hex for
// shaman, etc. Returns 0 when the class lacks a usable CC at
// current spec.
uint32 ClassCC(uint8 cls, uint32 spec);

// Per-class tank-taunt spell id. Used by the M+ tank-swap rule
// (advice.tank_swap_on_spells) — off-tank casts this on the boss
// when the active tank's debuff stacks call for a swap. Returns 0
// for non-tank-capable classes; caller checks knows_spell()/is_ready().
// Warrior=355 (Taunt), Paladin=62124 (Hand of Reckoning),
// DeathKnight=49576 (Death Grip), Druid=6795 (Growl — bear-form
// only, caller must verify form), Monk=115546 (Provoke),
// DemonHunter=185245 (Torment).
uint32 ClassTaunt(uint8 cls);

// ---- Role ↔ spec helpers (shared between BotQueueFiller and
//      State_Idle's idle:dual_spec_switch rule) ----
// Returns true when (cls, spec) IS the canonical tank/healer spec
// for that class (e.g., Warrior+73=Prot, Druid+105=Resto). Note
// Priest accepts both 256 Disc and 257 Holy for healing.
bool IsTankSpec(uint8 cls, uint16 spec);
bool IsHealerSpec(uint8 cls, uint16 spec);
// Class-side tank/healer SPEC ID for autonomous spec switching.
// Returns 0 for classes that can't fill that role (e.g., Hunter→Tank
// is 0 since Survival's tank stance was removed). Used by
// BotQueueFiller's "convert hybrid DPS to tank/healer before
// queueing" path AND idle:dual_spec_switch when owner /setrole
// pinned a role the current spec doesn't satisfy.
uint32 TankSpecForClass(uint8 cls);
uint32 HealerSpecForClass(uint8 cls);

// Returns true when (cls, spec) is a ranged DPS / ranged-style spec —
// drives the BG kiting rule: ranged classes back away from melee
// attackers; melee classes don't (they'd just lose uptime). Casters
// (Mage / Warlock / Shadow Priest / Balance Druid / Elemental Shaman /
// Hunter MM+BM / Evoker DPS specs) all return true. Survival Hunter is
// melee in modern WoW and returns false. Healers also return false —
// they kite via Healer-specific logic (LoS / instant heals).
bool IsRangedSpec(uint8 cls, uint16 spec);

// Class mobility / disengage spell — Blink / Disengage / Sprint /
// Vengeful Retreat / Wraith Walk etc. Used by the retreat-outnumbered
// rule to actually get away rather than slow-walking. Returns 0
// when the class lacks one. Spell IDs are 12.0+ canonical.
uint32 ClassMobilityEscape(uint8 cls, uint16 spec);

// Group "Bloodlust" buff. Shaman casts Bloodlust (Horde) / Heroism
// (Alliance), Mage Time Warp, Hunter Primal Rage (pet), Evoker Fury
// of the Aspects. 5-min CD, applies a 30% haste raid buff for 40s.
// Returns 0 for non-lust classes. The spell handles faction routing
// for Shaman; the call site picks based on bot's race/team if needed.
uint32 ClassLustSpell(uint8 cls);

// Class offensive burst cooldown — Recklessness / Avenging Wrath /
// Combustion / Bestial Wrath / Pillar of Frost / Stormkeeper /
// Demonic Power / Metamorphosis / Voidform / Berserk / Adrenaline
// Rush / Storm Earth Fire / Dragonrage. Fired when an enemy target
// is sub-30% HP for the kill. Returns 0 for tank/healer specs and
// classes without a clear single-button burst CD. 1.5-3min CD.
uint32 ClassOffensiveBurst(uint8 cls, uint16 spec);

} // namespace Playerbot
