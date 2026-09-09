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
#include "shadowfang_keep.h"
#include "Creature.h"
#include "GameObject.h"
#include "InstanceScript.h"
#include "Unit.h"

static constexpr ObjectData creatureData[] =
{
    { NPC_BARON_ASHBURY,        BOSS_BARON_ASHBURY          },
    { NPC_BARON_SILVERLAINE,    BOSS_BARON_SILVERLAINE      },
    { NPC_COMMANDER_SPRINGVALE, BOSS_COMMANDER_SPRINGVALE   },
    { NPC_LORD_WALDEN,          BOSS_LORD_WALDEN            },
    { NPC_LORD_GODFREY,         BOSS_LORD_GODFREY           },
};

static constexpr ObjectData gameobjectData[] =
{
    { GO_COURTYARD_DOOR,    DATA_COURTYARD_DOOR },
    { GO_SORCERERS_DOOR,    DATA_SORCERER_GATE  },
    { GO_ARUGALS_LAIR,      DATA_ARUGAL_DOOR    },
};

static constexpr DungeonEncounterData encounters[] =
{
    { BOSS_BARON_ASHBURY,           {{ 1069 }}  },
    { BOSS_BARON_SILVERLAINE,       {{ 1070 }}  },
    { BOSS_COMMANDER_SPRINGVALE,    {{ 1071 }}  },
    { BOSS_LORD_WALDEN,             {{ 1073 }}  },
    { BOSS_LORD_GODFREY,            {{ 1072 }}  },
    { BOSS_APOTHECARY_HUMMEL,       {{ 2879 }}  }
};

class instance_shadowfang_keep : public InstanceMapScript
{
public:
    instance_shadowfang_keep() : InstanceMapScript(SFKScriptName, 33) { }

    struct instance_shadowfang_keep_InstanceMapScript : public InstanceScript
    {
        instance_shadowfang_keep_InstanceMapScript(InstanceMap* map) : InstanceScript(map)
        {
            SetHeaders(DataHeader);
            SetBossNumber(EncounterCount);
            LoadObjectData(creatureData, gameobjectData);
            LoadDungeonEncounterData(encounters);
        }

        // Open a progression door tracked via ObjectData. HandleGameObject
        // tolerates a null GO (falls back to the guid, here Empty = no-op),
        // so this is safe even before the GO has spawned.
        void OpenDoor(uint32 type)
        {
            if (GameObject* go = GetGameObject(type))
                HandleGameObject(ObjectGuid::Empty, /*open=*/true, go);
        }

        // The five Cataclysm-revamp SFK bosses (Ashbury, Silverlaine,
        // Springvale, Walden, Godfrey) run entirely on SmartAI and have no
        // C++ BossAI, so nothing ever calls _JustDied()/SetBossState() when
        // they are killed — the encounter stays NOT_STARTED and the instance
        // never reads as progressed (pure-bot runs killed Baron Ashbury but
        // stayed stuck 0/6 forever), AND the boss-gated progression doors this
        // instance registers (Courtyard Door, Sorcerer's Gate, Arugal's Lair)
        // never open, sealing the group in the entrance courtyard (live
        // 2026-07-25: after Ashbury died the group could not reach Baron
        // Silverlaine — the tank wedged AT the closed Courtyard Door at z90.6
        // and pathing to Silverlaine returned Incomplete). Credit the encounter
        // AND open the door that boss gated when a registered boss dies.
        // Apothecary Hummel (index 5) has a real BossAI (boss_apothecary_hummel)
        // that self-credits, so it is deliberately not handled here. Cheap
        // entry switch; the default branch is a no-op for the flood of trash/
        // pet deaths on the map. Door->boss gating: Courtyard Door after Ashbury
        // is VERIFIED (the boss-2 blocker); the two upper gates are opened on
        // their inferred preceding boss (Sorcerer's Gate after Springvale ->
        // Walden's area, Arugal's Lair after Walden -> Godfrey's lair) — refine
        // if a later run wedges at one of them.
        void OnUnitDeath(Unit* unit) override
        {
            Creature* creature = unit->ToCreature();
            if (!creature)
                return;

            switch (creature->GetEntry())
            {
                case NPC_BARON_ASHBURY:
                    SetBossState(BOSS_BARON_ASHBURY, DONE);
                    OpenDoor(DATA_COURTYARD_DOOR);
                    break;
                case NPC_BARON_SILVERLAINE:
                    SetBossState(BOSS_BARON_SILVERLAINE, DONE);
                    break;
                case NPC_COMMANDER_SPRINGVALE:
                    SetBossState(BOSS_COMMANDER_SPRINGVALE, DONE);
                    OpenDoor(DATA_SORCERER_GATE);
                    break;
                case NPC_LORD_WALDEN:
                    SetBossState(BOSS_LORD_WALDEN, DONE);
                    OpenDoor(DATA_ARUGAL_DOOR);
                    break;
                case NPC_LORD_GODFREY:
                    SetBossState(BOSS_LORD_GODFREY, DONE);
                    break;
                default: break;
            }
        }
    };

    InstanceScript* GetInstanceScript(InstanceMap* map) const override
    {
        return new instance_shadowfang_keep_InstanceMapScript(map);
    }
};

void AddSC_instance_shadowfang_keep()
{
    new instance_shadowfang_keep();
}
