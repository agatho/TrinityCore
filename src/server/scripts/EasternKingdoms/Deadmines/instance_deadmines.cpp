/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "ScriptMgr.h"
#include "deadmines.h"
#include "InstanceScript.h"
#include "Creature.h"
#include "Log.h"

static constexpr ObjectData creatureData[] =
{
    { NPC_GLUBTOK,              BOSS_GLUBTOK            },
    { NPC_HELIX_GEARBREAKER,    BOSS_HELIX_GEARBREAKER  },
    { NPC_FOE_REAPER_5000,      BOSS_FOE_REAPER_5000    },
    { NPC_ADMIRAL_RIPSNARL,     BOSS_ADMIRAL_RIPSNARL   },
    { NPC_CAPTAIN_COOKIE,       BOSS_CAPTAIN_COOKIE     },
    { NPC_VANESSA_VAN_CLEEF,    BOSS_VANESSA_VANCLEEF   },
};

static constexpr DoorData doorData[] =
{
    { GO_FACTORY_DOOR,      BOSS_GLUBTOK,           EncounterDoorBehavior::OpenWhenDone             },
    { GO_MAST_ROOM_DOOR,    BOSS_HELIX_GEARBREAKER, EncounterDoorBehavior::OpenWhenDone             },
    { GO_HEAVY_DOOR,        BOSS_HELIX_GEARBREAKER, EncounterDoorBehavior::OpenWhenNotInProgress    },
    { GO_FOUNDRY_DOOR,      BOSS_FOE_REAPER_5000,   EncounterDoorBehavior::OpenWhenDone             },
};

// In-instance entrance graveyard (world_safe_locs.ID 3598, "Deadmines Entrance
// Target" @ map 36 -14.6,-385.5,62.5). Declared here so RepopAtGraveyard() routes
// a dying group member to the SAME-MAP instance graveyard instead of falling
// through to GetClosestGraveyard() -> Westfall (cross-map eject). The base
// InstanceScript only learns the entrance from a *restored* instance lock
// (Map.cpp early-returns before SetEntranceLocation for a fresh instance), so a
// freshly created instance — every LFG run — would otherwise leave _entranceId=0.
// Setting it in the ctor is clobber-safe: GetEntranceLocationForCompletedEncounters
// returns _entranceId when non-zero, so the lock persists 3598 and restore feeds
// it straight back. Combined with IsNonRaidDungeon()->shouldResurrect, the group
// revives together at the entrance and can regroup/re-attempt after a wipe.
static constexpr uint32 DEADMINES_ENTRANCE_WORLDSAFELOC = 3598;

// Static "decoy" creatures that an unscripted Deadmines leaves behind as permanent
// HOSTILE false-combat sources. NONE of these are real kill targets — the credited
// bosses are in creatureData[] (47162/47296/43778/47626/47739/49541); these are
// event doubles, floor-fire tiles, and trigger stalkers that, in a fully-scripted
// encounter, a boss AI would summon and clean up. This dataset ships no such AI, so
// they sit forever as attackable-but-unwinnable hostiles: bots (and real players)
// that wander into them get held InCombat with a target they can neither out-DPS
// (boss-HP, pack-healed) nor even select (UNINTERACTIBLE), with no encounter to end
// it. That false-combat both pins the group (no re-advance) and — for the firewall
// platters — chases them ~160y back to the zone-in. Neutralize each at spawn
// (passive + immune-to-all + non-selectable) so it never pulls anyone into combat,
// fixing the lock at the SOURCE rather than adapting bot AI around it. A future real
// boss script can re-arm them. (See OnCreatureCreate.)
static constexpr uint32 DecoyNeutralizeEntries[] =
{
    // Glubtok floor-fire "Firewall Platter" tiles — static, inert (no aura/SAI/
    // damage), 180s-respawning hostiles clustered around the Glubtok room.
    48974, 48975, 48976, 49039, 49040, 49041, 49042,
    // Boss cutscene/event DOUBLES — immune (boss-HP, pack-healed), faction-hostile,
    // and NOT the credited boss entries. 49671 confirmed live pinning the group at
    // the Helix foundry (2026-06-28); 49682 sits on the Ripsnarl harbor approach.
    49670 /*Glubtok*/, 49674 /*Helix Gearbreaker*/, 49681 /*Foe Reaper 5000*/,
    49682 /*Ripsnarl*/, 49671 /*Vanessa VanCleef*/, 49429 /*Vanessa VanCleef*/,
    // Untargetable hostile lightning triggers — UNINTERACTIBLE, so unkillable and
    // unselectable, yet they flood the attacker list (49521: 56 spawns) and hold
    // the group InCombat near the harbor.
    49521 /*Vanessa Lightning Stalker*/, 53488 /*Summon Enabler Stalker*/,
    // Foe Reaper 5000 arena junkyard PROPS — ambient mining-yard clutter left with
    // NO protective flags in wc_world (unit_flags=0), so they spawn attackable on a
    // hostile faction. A tank crossing the arena (e.g. a post-death catch-up path)
    // aggros one, then out-ranges it onto an unreachable navmesh pocket behind the
    // hulk — held InCombat with victim empty, the false-combat escape's any_seedable
    // test counts the targetable-but-unreachable prop as a live target so it never
    // fires (observed live 2026-06-28: tank pinned 826s at -200,-505 on a Mining
    // Monkey, run frozen 3/6). Their sibling props (Drink Tray 48340, Goblin Cocktail
    // 48341-43) already carry IMMUNE+UNINTERACTIBLE in DB; these just missed it. No
    // quest kill-credit, ambient — safe to make passive+immune+uninteractible too.
    48278, 48441, 48442 /*Mining Monkey*/, 48284 /*Mining Powder*/,
};

static constexpr DungeonEncounterData encounters[] =
{
    { BOSS_GLUBTOK,             {{ 2976, 2981 }}  },
    { BOSS_HELIX_GEARBREAKER,   {{ 2977, 2982 }}  },
    { BOSS_FOE_REAPER_5000,     {{ 2975, 2980 }}  },
    { BOSS_ADMIRAL_RIPSNARL,    {{ 2974, 2979 }}  },
    { BOSS_CAPTAIN_COOKIE,      {{ 2973, 2978 }}  },
    { BOSS_VANESSA_VANCLEEF,    {{ 1081 }}  }
};

class instance_deadmines : public InstanceMapScript
{
public:
    instance_deadmines() : InstanceMapScript(DMScriptName, 36) {  }

    struct instance_deadmines_InstanceMapScript : public InstanceScript
    {
        instance_deadmines_InstanceMapScript(InstanceMap* map) : InstanceScript(map)
        {
            SetHeaders(DataHeader);
            SetBossNumber(EncounterCount);
            LoadObjectData(creatureData, {});
            LoadDoorData(doorData);
            LoadDungeonEncounterData(encounters);
            SetEntranceLocation(DEADMINES_ENTRANCE_WORLDSAFELOC);
        }

        // Neutralize the static decoy creatures (DecoyNeutralizeEntries) at spawn so
        // they never pull anyone into unwinnable/untargetable false-combat. See the
        // table's comment for the full rationale. Order matters: set REACT_PASSIVE
        // first so Creature::SetImmuneToAll keeps it out of combat (keepCombat reads
        // the react state). SetUninteractible makes the unkillable ones unselectable.
        void OnCreatureCreate(Creature* creature) override
        {
            InstanceScript::OnCreatureCreate(creature);

            uint32 const entry = creature->GetEntry();
            for (uint32 decoy : DecoyNeutralizeEntries)
            {
                if (decoy != entry)
                    continue;
                creature->SetReactState(REACT_PASSIVE);
                creature->SetImmuneToAll(true);
                creature->SetUninteractible(true);
                break;
            }
        }

        // Credit boss kills to the encounter/door system on death.
        //
        // The Cata Deadmines bosses (Glubtok 47162, Helix 47296, Ripsnarl 47626,
        // "Captain" Cookie 47739) ship with NO creature AI in this dataset
        // (creature_template.AIName/ScriptName empty; Foe Reaper 47296 and Vanessa
        // are SmartAI but no script sets instance state), so none of them ever call
        // the usual BossAI _JustDied() -> instance->SetBossState(DONE). Without that
        // credit the EncounterDoorBehavior::OpenWhenDone doors never open — most
        // notably GO_FOUNDRY_DOOR (gated on BOSS_FOE_REAPER_5000), which walls off
        // the harbor and makes Admiral Ripsnarl navmesh-unreachable after Foe Reaper
        // is dead. The earlier Factory/Mast Room doors were only bypassed because an
        // off-mesh bridge routes around them; the Foundry Door has no such bypass.
        // It also left dungeon_exec.bosses_done_count stuck at 0/6 (full-clear /
        // LFG-reward gate). Fix at the InstanceScript layer (independent of any per-
        // boss AI): ZoneScript::OnUnitDeath fires for every unit death in the
        // instance, so map the dead creature's entry to its boss DATA index via the
        // existing creatureData[] table and set DONE. SetBossState updates the linked
        // doors, so the OpenWhenDone doors open the moment their boss dies. Benefits
        // real players too (their kills would hit the same un-credited path).
        void OnUnitDeath(Unit* unit) override
        {
            Creature* creature = unit->ToCreature();
            if (!creature)
                return;

            for (ObjectData const& data : creatureData)
            {
                if (data.entry != creature->GetEntry())
                    continue;
                if (GetBossState(data.type) != DONE)
                {
                    SetBossState(data.type, DONE);
                    // TEMP DIAG (remove after door-credit confirmed): prove the
                    // AI-less boss kill credits the encounter + opens its door.
                    TC_LOG_INFO("playerbot.v2",
                        "[dm_credit] entry={} data_type={} new_state={}",
                        creature->GetEntry(), uint32(data.type),
                        uint32(GetBossState(data.type)));
                }
                break;
            }
        }
    };

    InstanceScript* GetInstanceScript(InstanceMap* map) const override
    {
        return new instance_deadmines_InstanceMapScript(map);
    }
};

void AddSC_instance_deadmines()
{
    new instance_deadmines();
}
