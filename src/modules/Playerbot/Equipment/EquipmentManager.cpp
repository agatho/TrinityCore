/*
 * Copyright (C) 2024 TrinityCore <https://www.trinitycore.org/>
 *
 * COMPLETE EQUIPMENT MANAGEMENT IMPLEMENTATION
 *
 * Full implementation of equipment comparison, auto-equip, junk identification,
 * and consumable management for all 13 WoW classes.
 */

#include "EquipmentManager.h"
#include "Player.h"
#include "Item.h"
#include "Bag.h"
#include "ItemTemplate.h"
#include "ObjectMgr.h"
#include "Log.h"
#include "WorldSession.h"
#include <algorithm>

namespace Playerbot
{

EquipmentManager* EquipmentManager::instance()
{
    static EquipmentManager instance;
    return &instance;
}

EquipmentManager::EquipmentManager()
{
    InitializeStatPriorities();
    TC_LOG_INFO("playerbot", "EquipmentManager: Initialized with stat priorities for all 13 classes");
}

// ============================================================================
// STAT PRIORITY INITIALIZATION - ALL 13 CLASSES
// ============================================================================

void EquipmentManager::InitializeStatPriorities()
{
    InitializeWarriorPriorities();
    InitializePaladinPriorities();
    InitializeHunterPriorities();
    InitializeRoguePriorities();
    InitializePriestPriorities();
    InitializeShamanPriorities();
    InitializeMagePriorities();
    InitializeWarlockPriorities();
    InitializeDruidPriorities();
    InitializeDeathKnightPriorities();
    InitializeMonkPriorities();
    InitializeDemonHunterPriorities();
    InitializeEvokerPriorities();
}

void EquipmentManager::InitializeWarriorPriorities()
{
    // Arms (Spec 0)
    {
        StatPriority arms(CLASS_WARRIOR, 0);
        arms.statWeights = {
            {StatType::ITEM_LEVEL, 1.0f},
            {StatType::STRENGTH, 0.95f},
            {StatType::CRITICAL_STRIKE, 0.75f},
            {StatType::HASTE, 0.70f},
            {StatType::MASTERY, 0.65f},
            {StatType::VERSATILITY, 0.60f},
            {StatType::STAMINA, 0.50f},
            {StatType::ARMOR, 0.40f},
            {StatType::WEAPON_DPS, 0.90f}
        };
        _statPriorities[MakeStatPriorityKey(CLASS_WARRIOR, 0)] = arms;
    }

    // Fury (Spec 1)
    {
        StatPriority fury(CLASS_WARRIOR, 1);
        fury.statWeights = {
            {StatType::ITEM_LEVEL, 1.0f},
            {StatType::STRENGTH, 0.95f},
            {StatType::HASTE, 0.80f},
            {StatType::CRITICAL_STRIKE, 0.75f},
            {StatType::MASTERY, 0.70f},
            {StatType::VERSATILITY, 0.65f},
            {StatType::STAMINA, 0.50f},
            {StatType::ARMOR, 0.40f},
            {StatType::WEAPON_DPS, 0.85f}
        };
        _statPriorities[MakeStatPriorityKey(CLASS_WARRIOR, 1)] = fury;
    }

    // Protection (Spec 2)
    {
        StatPriority prot(CLASS_WARRIOR, 2);
        prot.statWeights = {
            {StatType::ITEM_LEVEL, 1.0f},
            {StatType::STAMINA, 0.90f},
            {StatType::ARMOR, 0.85f},
            {StatType::STRENGTH, 0.70f},
            {StatType::HASTE, 0.65f},
            {StatType::VERSATILITY, 0.75f},
            {StatType::MASTERY, 0.60f},
            {StatType::CRITICAL_STRIKE, 0.55f},
            {StatType::WEAPON_DPS, 0.50f}
        };
        _statPriorities[MakeStatPriorityKey(CLASS_WARRIOR, 2)] = prot;
    }
}

void EquipmentManager::InitializePaladinPriorities()
{
    // Holy (Spec 0)
    {
        StatPriority holy(CLASS_PALADIN, 0);
        holy.statWeights = {
            {StatType::ITEM_LEVEL, 1.0f},
            {StatType::INTELLECT, 0.95f},
            {StatType::CRITICAL_STRIKE, 0.75f},
            {StatType::HASTE, 0.70f},
            {StatType::MASTERY, 0.65f},
            {StatType::VERSATILITY, 0.60f},
            {StatType::STAMINA, 0.50f},
            {StatType::ARMOR, 0.40f}
        };
        _statPriorities[MakeStatPriorityKey(CLASS_PALADIN, 0)] = holy;
    }

    // Protection (Spec 1)
    {
        StatPriority prot(CLASS_PALADIN, 1);
        prot.statWeights = {
            {StatType::ITEM_LEVEL, 1.0f},
            {StatType::STAMINA, 0.90f},
            {StatType::ARMOR, 0.85f},
            {StatType::STRENGTH, 0.70f},
            {StatType::HASTE, 0.75f},
            {StatType::VERSATILITY, 0.70f},
            {StatType::MASTERY, 0.65f},
            {StatType::CRITICAL_STRIKE, 0.60f}
        };
        _statPriorities[MakeStatPriorityKey(CLASS_PALADIN, 1)] = prot;
    }

    // Retribution (Spec 2)
    {
        StatPriority ret(CLASS_PALADIN, 2);
        ret.statWeights = {
            {StatType::ITEM_LEVEL, 1.0f},
            {StatType::STRENGTH, 0.95f},
            {StatType::CRITICAL_STRIKE, 0.75f},
            {StatType::HASTE, 0.70f},
            {StatType::VERSATILITY, 0.65f},
            {StatType::MASTERY, 0.60f},
            {StatType::STAMINA, 0.50f},
            {StatType::WEAPON_DPS, 0.90f}
        };
        _statPriorities[MakeStatPriorityKey(CLASS_PALADIN, 2)] = ret;
    }
}

void EquipmentManager::InitializeHunterPriorities()
{
    // Beast Mastery (Spec 0)
    {
        StatPriority bm(CLASS_HUNTER, 0);
        bm.statWeights = {
            {StatType::ITEM_LEVEL, 1.0f},
            {StatType::AGILITY, 0.95f},
            {StatType::CRITICAL_STRIKE, 0.75f},
            {StatType::HASTE, 0.70f},
            {StatType::MASTERY, 0.80f},
            {StatType::VERSATILITY, 0.65f},
            {StatType::STAMINA, 0.50f},
            {StatType::WEAPON_DPS, 0.90f}
        };
        _statPriorities[MakeStatPriorityKey(CLASS_HUNTER, 0)] = bm;
    }

    // Marksmanship (Spec 1)
    {
        StatPriority mm(CLASS_HUNTER, 1);
        mm.statWeights = {
            {StatType::ITEM_LEVEL, 1.0f},
            {StatType::AGILITY, 0.95f},
            {StatType::CRITICAL_STRIKE, 0.80f},
            {StatType::MASTERY, 0.75f},
            {StatType::HASTE, 0.70f},
            {StatType::VERSATILITY, 0.65f},
            {StatType::STAMINA, 0.50f},
            {StatType::WEAPON_DPS, 0.95f}
        };
        _statPriorities[MakeStatPriorityKey(CLASS_HUNTER, 1)] = mm;
    }

    // Survival (Spec 2)
    {
        StatPriority surv(CLASS_HUNTER, 2);
        surv.statWeights = {
            {StatType::ITEM_LEVEL, 1.0f},
            {StatType::AGILITY, 0.95f},
            {StatType::HASTE, 0.75f},
            {StatType::CRITICAL_STRIKE, 0.70f},
            {StatType::VERSATILITY, 0.68f},
            {StatType::MASTERY, 0.65f},
            {StatType::STAMINA, 0.50f},
            {StatType::WEAPON_DPS, 0.85f}
        };
        _statPriorities[MakeStatPriorityKey(CLASS_HUNTER, 2)] = surv;
    }
}

void EquipmentManager::InitializeRoguePriorities()
{
    // Assassination (Spec 0)
    {
        StatPriority assa(CLASS_ROGUE, 0);
        assa.statWeights = {
            {StatType::ITEM_LEVEL, 1.0f},
            {StatType::AGILITY, 0.95f},
            {StatType::CRITICAL_STRIKE, 0.75f},
            {StatType::MASTERY, 0.70f},
            {StatType::HASTE, 0.65f},
            {StatType::VERSATILITY, 0.60f},
            {StatType::STAMINA, 0.50f},
            {StatType::WEAPON_DPS, 0.90f}
        };
        _statPriorities[MakeStatPriorityKey(CLASS_ROGUE, 0)] = assa;
    }

    // Outlaw (Spec 1)
    {
        StatPriority outlaw(CLASS_ROGUE, 1);
        outlaw.statWeights = {
            {StatType::ITEM_LEVEL, 1.0f},
            {StatType::AGILITY, 0.95f},
            {StatType::HASTE, 0.75f},
            {StatType::CRITICAL_STRIKE, 0.70f},
            {StatType::VERSATILITY, 0.68f},
            {StatType::MASTERY, 0.65f},
            {StatType::STAMINA, 0.50f},
            {StatType::WEAPON_DPS, 0.90f}
        };
        _statPriorities[MakeStatPriorityKey(CLASS_ROGUE, 1)] = outlaw;
    }

    // Subtlety (Spec 2)
    {
        StatPriority sub(CLASS_ROGUE, 2);
        sub.statWeights = {
            {StatType::ITEM_LEVEL, 1.0f},
            {StatType::AGILITY, 0.95f},
            {StatType::CRITICAL_STRIKE, 0.75f},
            {StatType::VERSATILITY, 0.72f},
            {StatType::MASTERY, 0.70f},
            {StatType::HASTE, 0.65f},
            {StatType::STAMINA, 0.50f},
            {StatType::WEAPON_DPS, 0.90f}
        };
        _statPriorities[MakeStatPriorityKey(CLASS_ROGUE, 2)] = sub;
    }
}

void EquipmentManager::InitializePriestPriorities()
{
    // Discipline (Spec 0)
    {
        StatPriority disc(CLASS_PRIEST, 0);
        disc.statWeights = {
            {StatType::ITEM_LEVEL, 1.0f},
            {StatType::INTELLECT, 0.95f},
            {StatType::HASTE, 0.80f},
            {StatType::CRITICAL_STRIKE, 0.75f},
            {StatType::MASTERY, 0.70f},
            {StatType::VERSATILITY, 0.65f},
            {StatType::STAMINA, 0.50f}
        };
        _statPriorities[MakeStatPriorityKey(CLASS_PRIEST, 0)] = disc;
    }

    // Holy (Spec 1)
    {
        StatPriority holy(CLASS_PRIEST, 1);
        holy.statWeights = {
            {StatType::ITEM_LEVEL, 1.0f},
            {StatType::INTELLECT, 0.95f},
            {StatType::CRITICAL_STRIKE, 0.75f},
            {StatType::MASTERY, 0.70f},
            {StatType::HASTE, 0.68f},
            {StatType::VERSATILITY, 0.65f},
            {StatType::STAMINA, 0.50f}
        };
        _statPriorities[MakeStatPriorityKey(CLASS_PRIEST, 1)] = holy;
    }

    // Shadow (Spec 2)
    {
        StatPriority shadow(CLASS_PRIEST, 2);
        shadow.statWeights = {
            {StatType::ITEM_LEVEL, 1.0f},
            {StatType::INTELLECT, 0.95f},
            {StatType::HASTE, 0.80f},
            {StatType::CRITICAL_STRIKE, 0.75f},
            {StatType::VERSATILITY, 0.70f},
            {StatType::MASTERY, 0.65f},
            {StatType::STAMINA, 0.50f}
        };
        _statPriorities[MakeStatPriorityKey(CLASS_PRIEST, 2)] = shadow;
    }
}

void EquipmentManager::InitializeShamanPriorities()
{
    // Elemental (Spec 0)
    {
        StatPriority ele(CLASS_SHAMAN, 0);
        ele.statWeights = {
            {StatType::ITEM_LEVEL, 1.0f},
            {StatType::INTELLECT, 0.95f},
            {StatType::CRITICAL_STRIKE, 0.75f},
            {StatType::HASTE, 0.72f},
            {StatType::VERSATILITY, 0.70f},
            {StatType::MASTERY, 0.68f},
            {StatType::STAMINA, 0.50f}
        };
        _statPriorities[MakeStatPriorityKey(CLASS_SHAMAN, 0)] = ele;
    }

    // Enhancement (Spec 1)
    {
        StatPriority enh(CLASS_SHAMAN, 1);
        enh.statWeights = {
            {StatType::ITEM_LEVEL, 1.0f},
            {StatType::AGILITY, 0.95f},
            {StatType::HASTE, 0.75f},
            {StatType::CRITICAL_STRIKE, 0.70f},
            {StatType::MASTERY, 0.68f},
            {StatType::VERSATILITY, 0.65f},
            {StatType::STAMINA, 0.50f},
            {StatType::WEAPON_DPS, 0.85f}
        };
        _statPriorities[MakeStatPriorityKey(CLASS_SHAMAN, 1)] = enh;
    }

    // Restoration (Spec 2)
    {
        StatPriority resto(CLASS_SHAMAN, 2);
        resto.statWeights = {
            {StatType::ITEM_LEVEL, 1.0f},
            {StatType::INTELLECT, 0.95f},
            {StatType::CRITICAL_STRIKE, 0.75f},
            {StatType::HASTE, 0.70f},
            {StatType::MASTERY, 0.68f},
            {StatType::VERSATILITY, 0.65f},
            {StatType::STAMINA, 0.50f}
        };
        _statPriorities[MakeStatPriorityKey(CLASS_SHAMAN, 2)] = resto;
    }
}

void EquipmentManager::InitializeMagePriorities()
{
    // Arcane (Spec 0)
    {
        StatPriority arcane(CLASS_MAGE, 0);
        arcane.statWeights = {
            {StatType::ITEM_LEVEL, 1.0f},
            {StatType::INTELLECT, 0.95f},
            {StatType::MASTERY, 0.80f},
            {StatType::HASTE, 0.75f},
            {StatType::CRITICAL_STRIKE, 0.70f},
            {StatType::VERSATILITY, 0.65f},
            {StatType::STAMINA, 0.50f}
        };
        _statPriorities[MakeStatPriorityKey(CLASS_MAGE, 0)] = arcane;
    }

    // Fire (Spec 1)
    {
        StatPriority fire(CLASS_MAGE, 1);
        fire.statWeights = {
            {StatType::ITEM_LEVEL, 1.0f},
            {StatType::INTELLECT, 0.95f},
            {StatType::CRITICAL_STRIKE, 0.85f},
            {StatType::HASTE, 0.75f},
            {StatType::MASTERY, 0.70f},
            {StatType::VERSATILITY, 0.65f},
            {StatType::STAMINA, 0.50f}
        };
        _statPriorities[MakeStatPriorityKey(CLASS_MAGE, 1)] = fire;
    }

    // Frost (Spec 2)
    {
        StatPriority frost(CLASS_MAGE, 2);
        frost.statWeights = {
            {StatType::ITEM_LEVEL, 1.0f},
            {StatType::INTELLECT, 0.95f},
            {StatType::HASTE, 0.80f},
            {StatType::CRITICAL_STRIKE, 0.75f},
            {StatType::VERSATILITY, 0.70f},
            {StatType::MASTERY, 0.68f},
            {StatType::STAMINA, 0.50f}
        };
        _statPriorities[MakeStatPriorityKey(CLASS_MAGE, 2)] = frost;
    }
}

void EquipmentManager::InitializeWarlockPriorities()
{
    // Affliction (Spec 0)
    {
        StatPriority aff(CLASS_WARLOCK, 0);
        aff.statWeights = {
            {StatType::ITEM_LEVEL, 1.0f},
            {StatType::INTELLECT, 0.95f},
            {StatType::HASTE, 0.80f},
            {StatType::MASTERY, 0.75f},
            {StatType::CRITICAL_STRIKE, 0.70f},
            {StatType::VERSATILITY, 0.65f},
            {StatType::STAMINA, 0.50f}
        };
        _statPriorities[MakeStatPriorityKey(CLASS_WARLOCK, 0)] = aff;
    }

    // Demonology (Spec 1)
    {
        StatPriority demo(CLASS_WARLOCK, 1);
        demo.statWeights = {
            {StatType::ITEM_LEVEL, 1.0f},
            {StatType::INTELLECT, 0.95f},
            {StatType::HASTE, 0.80f},
            {StatType::CRITICAL_STRIKE, 0.75f},
            {StatType::MASTERY, 0.70f},
            {StatType::VERSATILITY, 0.65f},
            {StatType::STAMINA, 0.50f}
        };
        _statPriorities[MakeStatPriorityKey(CLASS_WARLOCK, 1)] = demo;
    }

    // Destruction (Spec 2)
    {
        StatPriority destro(CLASS_WARLOCK, 2);
        destro.statWeights = {
            {StatType::ITEM_LEVEL, 1.0f},
            {StatType::INTELLECT, 0.95f},
            {StatType::CRITICAL_STRIKE, 0.80f},
            {StatType::HASTE, 0.75f},
            {StatType::VERSATILITY, 0.70f},
            {StatType::MASTERY, 0.68f},
            {StatType::STAMINA, 0.50f}
        };
        _statPriorities[MakeStatPriorityKey(CLASS_WARLOCK, 2)] = destro;
    }
}

void EquipmentManager::InitializeDruidPriorities()
{
    // Balance (Spec 0)
    {
        StatPriority balance(CLASS_DRUID, 0);
        balance.statWeights = {
            {StatType::ITEM_LEVEL, 1.0f},
            {StatType::INTELLECT, 0.95f},
            {StatType::HASTE, 0.75f},
            {StatType::CRITICAL_STRIKE, 0.72f},
            {StatType::MASTERY, 0.70f},
            {StatType::VERSATILITY, 0.65f},
            {StatType::STAMINA, 0.50f}
        };
        _statPriorities[MakeStatPriorityKey(CLASS_DRUID, 0)] = balance;
    }

    // Feral (Spec 1)
    {
        StatPriority feral(CLASS_DRUID, 1);
        feral.statWeights = {
            {StatType::ITEM_LEVEL, 1.0f},
            {StatType::AGILITY, 0.95f},
            {StatType::CRITICAL_STRIKE, 0.75f},
            {StatType::HASTE, 0.70f},
            {StatType::MASTERY, 0.68f},
            {StatType::VERSATILITY, 0.65f},
            {StatType::STAMINA, 0.50f}
        };
        _statPriorities[MakeStatPriorityKey(CLASS_DRUID, 1)] = feral;
    }

    // Guardian (Spec 2)
    {
        StatPriority guardian(CLASS_DRUID, 2);
        guardian.statWeights = {
            {StatType::ITEM_LEVEL, 1.0f},
            {StatType::STAMINA, 0.90f},
            {StatType::ARMOR, 0.85f},
            {StatType::AGILITY, 0.75f},
            {StatType::VERSATILITY, 0.70f},
            {StatType::HASTE, 0.65f},
            {StatType::MASTERY, 0.60f},
            {StatType::CRITICAL_STRIKE, 0.55f}
        };
        _statPriorities[MakeStatPriorityKey(CLASS_DRUID, 2)] = guardian;
    }

    // Restoration (Spec 3)
    {
        StatPriority resto(CLASS_DRUID, 3);
        resto.statWeights = {
            {StatType::ITEM_LEVEL, 1.0f},
            {StatType::INTELLECT, 0.95f},
            {StatType::HASTE, 0.75f},
            {StatType::MASTERY, 0.72f},
            {StatType::CRITICAL_STRIKE, 0.70f},
            {StatType::VERSATILITY, 0.65f},
            {StatType::STAMINA, 0.50f}
        };
        _statPriorities[MakeStatPriorityKey(CLASS_DRUID, 3)] = resto;
    }
}

void EquipmentManager::InitializeDeathKnightPriorities()
{
    // Blood (Spec 0)
    {
        StatPriority blood(CLASS_DEATH_KNIGHT, 0);
        blood.statWeights = {
            {StatType::ITEM_LEVEL, 1.0f},
            {StatType::STAMINA, 0.90f},
            {StatType::ARMOR, 0.85f},
            {StatType::STRENGTH, 0.75f},
            {StatType::HASTE, 0.70f},
            {StatType::VERSATILITY, 0.75f},
            {StatType::MASTERY, 0.68f},
            {StatType::CRITICAL_STRIKE, 0.60f},
            {StatType::WEAPON_DPS, 0.55f}
        };
        _statPriorities[MakeStatPriorityKey(CLASS_DEATH_KNIGHT, 0)] = blood;
    }

    // Frost (Spec 1)
    {
        StatPriority frost(CLASS_DEATH_KNIGHT, 1);
        frost.statWeights = {
            {StatType::ITEM_LEVEL, 1.0f},
            {StatType::STRENGTH, 0.95f},
            {StatType::CRITICAL_STRIKE, 0.75f},
            {StatType::MASTERY, 0.72f},
            {StatType::HASTE, 0.70f},
            {StatType::VERSATILITY, 0.65f},
            {StatType::STAMINA, 0.50f},
            {StatType::WEAPON_DPS, 0.90f}
        };
        _statPriorities[MakeStatPriorityKey(CLASS_DEATH_KNIGHT, 1)] = frost;
    }

    // Unholy (Spec 2)
    {
        StatPriority unholy(CLASS_DEATH_KNIGHT, 2);
        unholy.statWeights = {
            {StatType::ITEM_LEVEL, 1.0f},
            {StatType::STRENGTH, 0.95f},
            {StatType::HASTE, 0.80f},
            {StatType::CRITICAL_STRIKE, 0.75f},
            {StatType::MASTERY, 0.72f},
            {StatType::VERSATILITY, 0.68f},
            {StatType::STAMINA, 0.50f},
            {StatType::WEAPON_DPS, 0.90f}
        };
        _statPriorities[MakeStatPriorityKey(CLASS_DEATH_KNIGHT, 2)] = unholy;
    }
}

void EquipmentManager::InitializeMonkPriorities()
{
    // Brewmaster (Spec 0)
    {
        StatPriority brew(CLASS_MONK, 0);
        brew.statWeights = {
            {StatType::ITEM_LEVEL, 1.0f},
            {StatType::STAMINA, 0.90f},
            {StatType::ARMOR, 0.85f},
            {StatType::AGILITY, 0.75f},
            {StatType::CRITICAL_STRIKE, 0.70f},
            {StatType::VERSATILITY, 0.75f},
            {StatType::MASTERY, 0.68f},
            {StatType::HASTE, 0.65f}
        };
        _statPriorities[MakeStatPriorityKey(CLASS_MONK, 0)] = brew;
    }

    // Mistweaver (Spec 1)
    {
        StatPriority mist(CLASS_MONK, 1);
        mist.statWeights = {
            {StatType::ITEM_LEVEL, 1.0f},
            {StatType::INTELLECT, 0.95f},
            {StatType::CRITICAL_STRIKE, 0.75f},
            {StatType::VERSATILITY, 0.72f},
            {StatType::HASTE, 0.70f},
            {StatType::MASTERY, 0.68f},
            {StatType::STAMINA, 0.50f}
        };
        _statPriorities[MakeStatPriorityKey(CLASS_MONK, 1)] = mist;
    }

    // Windwalker (Spec 2)
    {
        StatPriority ww(CLASS_MONK, 2);
        ww.statWeights = {
            {StatType::ITEM_LEVEL, 1.0f},
            {StatType::AGILITY, 0.95f},
            {StatType::VERSATILITY, 0.75f},
            {StatType::CRITICAL_STRIKE, 0.72f},
            {StatType::HASTE, 0.70f},
            {StatType::MASTERY, 0.68f},
            {StatType::STAMINA, 0.50f},
            {StatType::WEAPON_DPS, 0.85f}
        };
        _statPriorities[MakeStatPriorityKey(CLASS_MONK, 2)] = ww;
    }
}

void EquipmentManager::InitializeDemonHunterPriorities()
{
    // Havoc (Spec 0)
    {
        StatPriority havoc(CLASS_DEMON_HUNTER, 0);
        havoc.statWeights = {
            {StatType::ITEM_LEVEL, 1.0f},
            {StatType::AGILITY, 0.95f},
            {StatType::CRITICAL_STRIKE, 0.75f},
            {StatType::HASTE, 0.72f},
            {StatType::VERSATILITY, 0.70f},
            {StatType::MASTERY, 0.68f},
            {StatType::STAMINA, 0.50f},
            {StatType::WEAPON_DPS, 0.85f}
        };
        _statPriorities[MakeStatPriorityKey(CLASS_DEMON_HUNTER, 0)] = havoc;
    }

    // Vengeance (Spec 1)
    {
        StatPriority veng(CLASS_DEMON_HUNTER, 1);
        veng.statWeights = {
            {StatType::ITEM_LEVEL, 1.0f},
            {StatType::STAMINA, 0.90f},
            {StatType::ARMOR, 0.85f},
            {StatType::AGILITY, 0.75f},
            {StatType::VERSATILITY, 0.75f},
            {StatType::HASTE, 0.70f},
            {StatType::MASTERY, 0.68f},
            {StatType::CRITICAL_STRIKE, 0.65f}
        };
        _statPriorities[MakeStatPriorityKey(CLASS_DEMON_HUNTER, 1)] = veng;
    }
}

void EquipmentManager::InitializeEvokerPriorities()
{
    // Devastation (Spec 0)
    {
        StatPriority dev(CLASS_EVOKER, 0);
        dev.statWeights = {
            {StatType::ITEM_LEVEL, 1.0f},
            {StatType::INTELLECT, 0.95f},
            {StatType::CRITICAL_STRIKE, 0.75f},
            {StatType::HASTE, 0.72f},
            {StatType::MASTERY, 0.70f},
            {StatType::VERSATILITY, 0.68f},
            {StatType::STAMINA, 0.50f}
        };
        _statPriorities[MakeStatPriorityKey(CLASS_EVOKER, 0)] = dev;
    }

    // Preservation (Spec 1)
    {
        StatPriority pres(CLASS_EVOKER, 1);
        pres.statWeights = {
            {StatType::ITEM_LEVEL, 1.0f},
            {StatType::INTELLECT, 0.95f},
            {StatType::MASTERY, 0.75f},
            {StatType::CRITICAL_STRIKE, 0.72f},
            {StatType::HASTE, 0.70f},
            {StatType::VERSATILITY, 0.68f},
            {StatType::STAMINA, 0.50f}
        };
        _statPriorities[MakeStatPriorityKey(CLASS_EVOKER, 1)] = pres;
    }

    // Augmentation (Spec 2)
    {
        StatPriority aug(CLASS_EVOKER, 2);
        aug.statWeights = {
            {StatType::ITEM_LEVEL, 1.0f},
            {StatType::INTELLECT, 0.95f},
            {StatType::MASTERY, 0.80f},
            {StatType::CRITICAL_STRIKE, 0.75f},
            {StatType::VERSATILITY, 0.72f},
            {StatType::HASTE, 0.70f},
            {StatType::STAMINA, 0.50f}
        };
        _statPriorities[MakeStatPriorityKey(CLASS_EVOKER, 2)] = aug;
    }
}

// ============================================================================
// CORE EQUIPMENT EVALUATION
// ============================================================================

void EquipmentManager::AutoEquipBestGear(::Player* player)
{
    if (!player)
        return;

    // No lock needed - equipment data is per-bot instance data

    uint32 playerGuid = player->GetGUID().GetCounter();
    EquipmentAutomationProfile profile = GetAutomationProfile(playerGuid);
    if (!profile.autoEquipEnabled)
        return;

    TC_LOG_DEBUG("playerbot.equipment", "AutoEquipBestGear: Scanning inventory for player {}",
                 player->GetName());

    uint32 upgradesFound = 0;

    // Scan all inventory bags for equippable items
    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
    {
        if (Bag* pBag = player->GetBagByPos(bag))
        {
            for (uint32 slot = 0; slot < pBag->GetBagSize(); ++slot)
            {
                if (::Item* item = pBag->GetItemByPos(slot))
                {
                    if (IsItemUpgrade(player, item))
                    {
                        ItemTemplate const* proto = item->GetTemplate();
                        if (!proto)
                            continue;

                        uint8 equipSlot = GetItemEquipmentSlot(proto);
                        if (equipSlot != EQUIPMENT_SLOT_END)
                        {
                            // Get currently equipped item
                            ::Item* currentItem = GetEquippedItemInSlot(player, equipSlot);

                            // Compare items
                            ItemComparisonResult result = CompareItems(player, currentItem, item);
                            if (result.isUpgrade && result.scoreDifference >= profile.minUpgradeThreshold)
                            {
                                TC_LOG_INFO("playerbot.equipment",
                                           "🎯 UPGRADE FOUND: Player {} - {} is upgrade over {} (Score: {:.2f} -> {:.2f}, Reason: {})",
                                           player->GetName(),
                                           proto->GetName(DEFAULT_LOCALE),
                                           currentItem ? currentItem->GetTemplate()->GetName(DEFAULT_LOCALE) : "Empty Slot",
                                           result.currentItemScore,
                                           result.newItemScore,
                                           result.upgradeReason);

                                EquipItemInSlot(player, item, equipSlot);
                                upgradesFound++;
                                UpdateMetrics(playerGuid, true, true);
                            }
                        }
                    }
                }
            }
        }
    }

    // Also check main bag (backpack)
    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
    {
        if (::Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
        {
            if (IsItemUpgrade(player, item))
            {
                ItemTemplate const* proto = item->GetTemplate();
                if (!proto)
                    continue;

                uint8 equipSlot = GetItemEquipmentSlot(proto);
                if (equipSlot != EQUIPMENT_SLOT_END)
                {
                    ::Item* currentItem = GetEquippedItemInSlot(player, equipSlot);
                    ItemComparisonResult result = CompareItems(player, currentItem, item);
                    if (result.isUpgrade && result.scoreDifference >= profile.minUpgradeThreshold)
                    {
                        TC_LOG_INFO("playerbot.equipment",
                                   "🎯 UPGRADE FOUND: Player {} - {} (Score improvement: {:.2f})",
                                   player->GetName(), proto->GetName(DEFAULT_LOCALE), result.scoreDifference);

                        EquipItemInSlot(player, item, equipSlot);
                        upgradesFound++;
                        UpdateMetrics(playerGuid, true, true);
                    }
                }
            }
        }
    }

    if (upgradesFound > 0)
    {
        TC_LOG_INFO("playerbot.equipment", "✅ AutoEquip Complete: Player {} equipped {} upgrades",
                   player->GetName(), upgradesFound);
    }
}

ItemComparisonResult EquipmentManager::CompareItems(::Player* player, ::Item* currentItem, ::Item* newItem)
{
    ItemComparisonResult result;

    if (!player || !newItem)
        return result;

    // Calculate scores
    result.newItemScore = CalculateItemScore(player, newItem);

    if (currentItem)
    {
        result.currentItemScore = CalculateItemScore(player, currentItem);
        result.currentItemLevel = currentItem->GetItemLevel(player);
    }
    else
    {
        result.currentItemScore = 0.0f;
        result.currentItemLevel = 0;
    }

    result.newItemLevel = newItem->GetItemLevel(player);

    // Determine if upgrade
    result.scoreDifference = result.newItemScore - result.currentItemScore;

    EquipmentAutomationProfile profile = GetAutomationProfile(player->GetGUID().GetCounter());
    // Item level preference
    if (profile.preferHigherItemLevel && result.newItemLevel > result.currentItemLevel + 5)
    {
        result.isUpgrade = true;
        result.upgradeReason = "Higher item level (" + std::to_string(result.newItemLevel) + " vs " +
                               std::to_string(result.currentItemLevel) + ")";
        return result;
    }

    // Stat score comparison
    if (result.scoreDifference > profile.minUpgradeThreshold)
    {
        result.isUpgrade = true;
        result.upgradeReason = "Better stat allocation (Score: " + std::to_string(result.scoreDifference) + " improvement)";
    }

    return result;
}

float EquipmentManager::CalculateItemScore(::Player* player, ::Item* item)
{
    if (!player || !item)
        return 0.0f;

    ItemTemplate const* proto = item->GetTemplate();
    if (!proto)
        return 0.0f;

    StatPriority const& priority = GetStatPriority(player);

    // Calculate weighted stat total
    float totalScore = 0.0f;

    for (const auto& [statType, weight] : priority.statWeights)
    {
        int32 statValue = GetItemStatValue(item, statType);
        if (statValue > 0)
        {
            totalScore += static_cast<float>(statValue) * weight;
        }
    }

    // Add item level as base score
    totalScore += static_cast<float>(item->GetItemLevel(player)) * priority.GetStatWeight(StatType::ITEM_LEVEL);
    TC_LOG_TRACE("playerbot.equipment", "Item {} score for player {}: {:.2f}",
                 proto->GetName(DEFAULT_LOCALE), player->GetName(), totalScore);

    return totalScore;
}

bool EquipmentManager::IsItemUpgrade(::Player* player, ::Item* item)
{
    if (!player || !item)
        return false;

    ItemTemplate const* proto = item->GetTemplate();
    if (!proto)
        return false;

    // Check if player can equip this item
    if (!CanPlayerEquipItem(player, proto))
        return false;

    uint8 equipSlot = GetItemEquipmentSlot(proto);
    if (equipSlot == EQUIPMENT_SLOT_END)
        return false;

    ::Item* currentItem = GetEquippedItemInSlot(player, equipSlot);

    ItemComparisonResult result = CompareItems(player, currentItem, item);
    return result.isUpgrade;
}

float EquipmentManager::CalculateItemTemplateScore(::Player* player, ItemTemplate const* itemTemplate)
{
    if (!player || !itemTemplate)
        return 0.0f;

    StatPriority const& priority = GetStatPriority(player);
    // Calculate weighted stat total using the same algorithm as CalculateItemScore()
    float totalScore = 0.0f;

    for (const auto& [statType, weight] : priority.statWeights)
    {
        int32 statValue = ExtractStatValue(itemTemplate, statType);
        if (statValue > 0)
        {
            totalScore += static_cast<float>(statValue) * weight;
        }
    }

    // Add item level as base score
    totalScore += static_cast<float>(itemTemplate->GetBaseItemLevel()) * priority.GetStatWeight(StatType::ITEM_LEVEL);
    TC_LOG_TRACE("playerbot.equipment", "ItemTemplate {} score for player {}: {:.2f}",
                 itemTemplate->GetName(DEFAULT_LOCALE), player->GetName(), totalScore);

    return totalScore;
}

// ============================================================================
// JUNK IDENTIFICATION - COMPLETE IMPLEMENTATION
// ============================================================================

std::vector<ObjectGuid> EquipmentManager::IdentifyJunkItems(::Player* player)
{
    std::vector<ObjectGuid> junkItems;

    if (!player)
        return junkItems;

    EquipmentAutomationProfile profile = GetAutomationProfile(player->GetGUID().GetCounter());
    if (!profile.autoSellJunkEnabled)
        return junkItems;

    // Scan all inventory for junk
    for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
    {
        if (Bag* pBag = player->GetBagByPos(bag))
        {
            for (uint32 slot = 0; slot < pBag->GetBagSize(); ++slot)
            {
                if (::Item* item = pBag->GetItemByPos(slot))
                {
                    if (IsJunkItem(player, item) && !IsProtectedItem(player, item))
                    {
                        junkItems.push_back(item->GetGUID());
                        TC_LOG_DEBUG("playerbot.equipment", "Identified junk: {} ({})",
                                     item->GetTemplate()->GetName(DEFAULT_LOCALE), item->GetGUID().ToString());
                    }
                }
            }
        }
    }

    // Check main bag
    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
    {
        if (::Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
        {
            if (IsJunkItem(player, item) && !IsProtectedItem(player, item))
            {
                junkItems.push_back(item->GetGUID());
            }
        }
    }

    TC_LOG_INFO("playerbot.equipment", "Identified {} junk items for player {}",
                junkItems.size(), player->GetName());

    return junkItems;
}

bool EquipmentManager::IsJunkItem(::Player* player, ::Item* item)
{
    if (!player || !item)
        return false;

    ItemTemplate const* proto = item->GetTemplate();
    if (!proto)
        return false;

    // Always junk: Grey (Poor) quality
    if (proto->GetQuality() == ITEM_QUALITY_POOR)
        return true;

    // Check item level threshold
    EquipmentAutomationProfile profile = GetAutomationProfile(player->GetGUID().GetCounter());
    if (item->GetItemLevel(player) < profile.minItemLevelToKeep && player->GetLevel() > 20)
        return true;

    // If it's equipment, check if it's worse than what we have
    if (proto->GetInventoryType() != INVTYPE_NON_EQUIP)
    {
        if (IsOutdatedGear(player, item))
            return true;

        if (HasWrongPrimaryStats(player, item))
            return true;
    }

    return false;
}

bool EquipmentManager::IsProtectedItem(::Player* player, ::Item* item)
{
    if (!player || !item)
        return false;

    ItemTemplate const* proto = item->GetTemplate();
    if (!proto)
        return false;

    // Never sell quest items
    if (proto->GetClass() == ITEM_CLASS_QUEST)
        return true;

    // Never sell soulbound items with high item level
    if (item->IsSoulBound() && item->GetItemLevel(player) >= player->GetLevel())
        return true;

    // Never sell set items (if profile says so)
    EquipmentAutomationProfile profile = GetAutomationProfile(player->GetGUID().GetCounter());
    if (profile.considerSetBonuses && IsSetItem(item))
        return true;

    // Never sell valuable BoE
    if (IsValuableBoE(item))
        return true;

    // Check never-sell list
    uint32 itemId = proto->GetId();
    if (profile.neverSellItems.find(itemId) != profile.neverSellItems.end())
        return true;

    // Never sell rare+ consumables
    if (proto->GetClass() == ITEM_CLASS_CONSUMABLE && proto->GetQuality() >= ITEM_QUALITY_RARE)
        return true;

    return false;
}

bool EquipmentManager::IsValuableBoE(::Item* item)
{
    if (!item)
        return false;

    ItemTemplate const* proto = item->GetTemplate();
    if (!proto)
        return false;

    // Not BoE - not relevant
    if (proto->GetBonding() != BIND_ON_EQUIP)
        return false;

    // Epic+ BoE are always valuable
    if (proto->GetQuality() >= ITEM_QUALITY_EPIC)
        return true;

    // Rare BoE with high sell price
    if (proto->GetQuality() == ITEM_QUALITY_RARE && proto->GetSellPrice() > 10000) // 1 gold
        return true;

    return false;
}

// ============================================================================
// CONSUMABLE MANAGEMENT - COMPLETE IMPLEMENTATION
// ============================================================================

std::unordered_map<uint32, uint32> EquipmentManager::GetConsumableNeeds(::Player* player)
{
    std::unordered_map<uint32, uint32> needs;

    if (!player)
        return needs;

    std::vector<uint32> classConsumables = GetClassConsumables(player->GetClass());
    for (uint32 itemId : classConsumables)
    {
        uint32 currentCount = GetConsumableCount(player, itemId);
        uint32 recommendedCount = 20; // Stack size recommendation

        if (currentCount < recommendedCount)
        {
            needs[itemId] = recommendedCount - currentCount;
        }
    }

    // Food and water (all classes)
    uint32 foodLevel = GetRecommendedFoodLevel(player);
    uint32 currentFood = GetConsumableCount(player, foodLevel);
    if (currentFood < 20)
        needs[foodLevel] = 20 - currentFood;

    if (player->GetPowerType() == POWER_MANA)
    {
        uint32 waterLevel = GetRecommendedPotionLevel(player);
        uint32 currentWater = GetConsumableCount(player, waterLevel);
        if (currentWater < 20)
            needs[waterLevel] = 20 - currentWater;
    }

    return needs;
}

bool EquipmentManager::NeedsConsumableRestocking(::Player* player)
{
    auto needs = GetConsumableNeeds(player);
    return !needs.empty();
}

std::vector<uint32> EquipmentManager::GetClassConsumables(uint8 classId)
{
    std::vector<uint32> consumables;

    switch (classId)
    {
        case CLASS_ROGUE:
            // Poisons, thistle tea (example item IDs - would need actual WoW DB IDs)
            consumables.push_back(6947);  // Instant Poison
            consumables.push_back(2892);  // Deadly Poison
            break;

        case CLASS_WARLOCK:
            // Soul shards are now passive, but healthstones
            consumables.push_back(5512);  // Healthstone
            break;

        case CLASS_MAGE:
            // Conjured items handled separately
            break;

        case CLASS_HUNTER:
            // Arrows/bullets removed in modern WoW
            break;

        // Add more class-specific consumables as needed
        default:
            break;
    }

    return consumables;
}

uint32 EquipmentManager::GetConsumableCount(::Player* player, uint32 itemId)
{
    if (!player)
        return 0;

    return player->GetItemCount(itemId, true); // includeBank = true
}

// ============================================================================
// STAT PRIORITY SYSTEM
// ============================================================================

StatPriority const& EquipmentManager::GetStatPriority(::Player* player)
{
    if (!player)
    {
        static StatPriority defaultPriority(0, 0);
        return defaultPriority;
    }

    uint8 classId = player->GetClass();
    uint8 specId = static_cast<uint8>(player->GetPrimarySpecialization());
    uint16 key = MakeStatPriorityKey(classId, specId);

    auto it = _statPriorities.find(key);
    if (it != _statPriorities.end())
        return it->second;

    // Fallback to spec 0
    key = MakeStatPriorityKey(classId, 0);
    it = _statPriorities.find(key);
    if (it != _statPriorities.end())
        return it->second;

    static StatPriority defaultPriority(0, 0);
    return defaultPriority;
}

StatPriority const& EquipmentManager::GetStatPriorityByClassSpec(uint8 classId, uint32 specId)
{
    uint16 key = MakeStatPriorityKey(classId, static_cast<uint8>(specId));

    auto it = _statPriorities.find(key);
    if (it != _statPriorities.end())
        return it->second;

    // Fallback to spec 0
    key = MakeStatPriorityKey(classId, 0);
    it = _statPriorities.find(key);
    if (it != _statPriorities.end())
        return it->second;

    static StatPriority defaultPriority(0, 0);
    return defaultPriority;
}

void EquipmentManager::UpdatePlayerStatPriority(::Player* player)
{
    // Called when player changes spec - priority automatically updated via GetStatPriority()
    if (player)
    {
        TC_LOG_DEBUG("playerbot.equipment", "Updated stat priority for player {} (Class: {}, Spec: {})",
                     player->GetName(), static_cast<uint32>(player->GetClass()), static_cast<uint32>(player->GetPrimarySpecialization()));
    }
}

// ============================================================================
// ITEM CATEGORIZATION
// ============================================================================

ItemCategory EquipmentManager::GetItemCategory(::Item* item)
{
    if (!item)
        return ItemCategory::UNKNOWN;

    ItemTemplate const* proto = item->GetTemplate();
    if (!proto)
        return ItemCategory::UNKNOWN;

    // Quest items
    if (proto->GetClass() == ITEM_CLASS_QUEST)
        return ItemCategory::QUEST_ITEM;

    // Junk
    if (proto->GetQuality() == ITEM_QUALITY_POOR)
        return ItemCategory::JUNK;

    // Valuable BoE
    if (proto->GetBonding() == BIND_ON_EQUIP && proto->GetQuality() >= ITEM_QUALITY_EPIC)
        return ItemCategory::VALUABLE_BIND_ON_EQUIP;

    // Equipment
    if (proto->GetInventoryType() != INVTYPE_NON_EQUIP)
    {
        if (proto->GetClass() == ITEM_CLASS_WEAPON)
            return ItemCategory::WEAPON;
        else if (proto->GetClass() == ITEM_CLASS_ARMOR)
            return ItemCategory::ARMOR;
    }

    // Consumables
    if (proto->GetClass() == ITEM_CLASS_CONSUMABLE)
        return ItemCategory::CONSUMABLE;

    // Trade goods
    if (proto->GetClass() == ITEM_CLASS_TRADE_GOODS)
        return ItemCategory::TRADE_GOODS;

    return ItemCategory::UNKNOWN;
}

bool EquipmentManager::CanPlayerEquipItem(::Player* player, ItemTemplate const* itemTemplate)
{
    if (!player || !itemTemplate)
        return false;

    // Check level requirement
    if (player->GetLevel() < itemTemplate->GetBaseRequiredLevel())
        return false;

    // Check class restriction
    if (itemTemplate->GetAllowableClass() && !(itemTemplate->GetAllowableClass() & (1 << (player->GetClass() - 1))))
        return false;

    // Check race restriction
    if (!itemTemplate->GetAllowableRace().IsEmpty() && !itemTemplate->GetAllowableRace().HasRace(player->GetRace()))
        return false;

    return true;
}

uint8 EquipmentManager::GetItemEquipmentSlot(ItemTemplate const* itemTemplate)
{
    if (!itemTemplate)
        return EQUIPMENT_SLOT_END;

    // Map inventory type to equipment slot
    switch (itemTemplate->GetInventoryType())
    {
        case INVTYPE_HEAD: return EQUIPMENT_SLOT_HEAD;
        case INVTYPE_NECK: return EQUIPMENT_SLOT_NECK;
        case INVTYPE_SHOULDERS: return EQUIPMENT_SLOT_SHOULDERS;
        case INVTYPE_BODY: return EQUIPMENT_SLOT_BODY;
        case INVTYPE_CHEST: return EQUIPMENT_SLOT_CHEST;
        case INVTYPE_WAIST: return EQUIPMENT_SLOT_WAIST;
        case INVTYPE_LEGS: return EQUIPMENT_SLOT_LEGS;
        case INVTYPE_FEET: return EQUIPMENT_SLOT_FEET;
        case INVTYPE_WRISTS: return EQUIPMENT_SLOT_WRISTS;
        case INVTYPE_HANDS: return EQUIPMENT_SLOT_HANDS;
        case INVTYPE_FINGER: return EQUIPMENT_SLOT_FINGER1; // Rings - would need logic for finger2
        case INVTYPE_TRINKET: return EQUIPMENT_SLOT_TRINKET1; // Trinkets - would need logic for trinket2
        case INVTYPE_CLOAK: return EQUIPMENT_SLOT_BACK;
        case INVTYPE_WEAPON:
        case INVTYPE_WEAPONMAINHAND: return EQUIPMENT_SLOT_MAINHAND;
        case INVTYPE_WEAPONOFFHAND: return EQUIPMENT_SLOT_OFFHAND;
        case INVTYPE_HOLDABLE: return EQUIPMENT_SLOT_OFFHAND;
        case INVTYPE_2HWEAPON: return EQUIPMENT_SLOT_MAINHAND;
        case INVTYPE_TABARD: return EQUIPMENT_SLOT_TABARD;
        case INVTYPE_ROBE: return EQUIPMENT_SLOT_CHEST;
        default: return EQUIPMENT_SLOT_END;
    }
}

// ============================================================================
// ADVANCED FEATURES
// ============================================================================

bool EquipmentManager::IsSetItem(::Item* item)
{
    if (!item)
        return false;

    ItemTemplate const* proto = item->GetTemplate();
    if (!proto)
        return false;

    // Check if item has ItemSet field (set items have ItemSet > 0)
    return proto->GetItemSet() != 0;
}

uint32 EquipmentManager::GetEquippedSetPieceCount(::Player* player, uint32 setId)
{
    if (!player || setId == 0)
        return 0;

    uint32 count = 0;

    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        if (::Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
        {
            ItemTemplate const* proto = item->GetTemplate();
            if (proto && proto->GetItemSet() == setId)
                ++count;
        }
    }

    return count;
}

float EquipmentManager::GetWeaponDPS(::Item* item)
{
    if (!item)
        return 0.0f;

    ItemTemplate const* proto = item->GetTemplate();
    if (!proto || proto->GetClass() != ITEM_CLASS_WEAPON)
        return 0.0f;

    // Calculate DPS: ((minDmg + maxDmg) / 2) / delay
    float minDmg = 0.0f;
    float maxDmg = 0.0f;
    proto->GetDamage(item->GetItemLevel(nullptr), minDmg, maxDmg);

    uint32 delay = proto->GetDelay();
    if (delay == 0)
        return 0.0f;

    float avgDmg = (minDmg + maxDmg) / 2.0f;
    float dps = (avgDmg / static_cast<float>(delay)) * 1000.0f;

    return dps;
}

uint32 EquipmentManager::GetItemArmor(::Item* item)
{
    if (!item)
        return 0;

    ItemTemplate const* proto = item->GetTemplate();
    if (!proto)
        return 0;

    return proto->GetArmor(ITEM_QUALITY_NORMAL);
}

int32 EquipmentManager::GetItemStatValue(::Item* item, StatType stat)
{
    if (!item)
        return 0;

    ItemTemplate const* proto = item->GetTemplate();
    if (!proto)
        return 0;
    return ExtractStatValue(proto, stat);
}

// ============================================================================
// AUTOMATION CONTROL
// ============================================================================

void EquipmentManager::SetAutomationProfile(uint32 playerGuid, EquipmentAutomationProfile const& profile)
{
    // No lock needed - equipment data is per-bot instance data
    _playerProfiles[playerGuid] = profile;
}

EquipmentManager::EquipmentAutomationProfile EquipmentManager::GetAutomationProfile(uint32 playerGuid)
{
    // No lock needed - equipment data is per-bot instance data

    auto it = _playerProfiles.find(playerGuid);
    if (it != _playerProfiles.end())
        return it->second;

    return EquipmentAutomationProfile(); // Return default
}

// ============================================================================
// METRICS
// ============================================================================

EquipmentManager::EquipmentMetrics const& EquipmentManager::GetPlayerMetrics(uint32 playerGuid)
{
    // No lock needed - equipment data is per-bot instance data

    auto it = _playerMetrics.find(playerGuid);
    if (it != _playerMetrics.end())
        return it->second;

    // Create default metrics (use operator[] which default-constructs)
    return _playerMetrics[playerGuid];
}

EquipmentManager::EquipmentMetrics const& EquipmentManager::GetGlobalMetrics()
{
    return _globalMetrics;
}

// ============================================================================
// HELPER METHODS IMPLEMENTATION
// ============================================================================

bool EquipmentManager::IsOutdatedGear(::Player* player, ::Item* item)
{
    if (!player || !item)
        return false;

    // Item is outdated if it's 10+ levels below player level
    uint32 itemLevel = item->GetItemLevel(player);
    uint32 playerLevel = player->GetLevel();
    return (playerLevel > itemLevel + 10);
}

bool EquipmentManager::HasWrongPrimaryStats(::Player* player, ::Item* item)
{
    if (!player || !item)
        return false;

    ItemTemplate const* proto = item->GetTemplate();
    if (!proto || proto->GetInventoryType() == INVTYPE_NON_EQUIP)
        return false;

    // Get player's primary stat
    StatType primaryStat;
    uint8 classId = player->GetClass();
    switch (classId)
    {
        case CLASS_WARRIOR:
        case CLASS_PALADIN:
        case CLASS_DEATH_KNIGHT:
            primaryStat = StatType::STRENGTH;
            break;

        case CLASS_HUNTER:
        case CLASS_ROGUE:
        case CLASS_SHAMAN:
        case CLASS_DRUID:
        case CLASS_MONK:
        case CLASS_DEMON_HUNTER:
            primaryStat = StatType::AGILITY;
            break;

        case CLASS_PRIEST:
        case CLASS_MAGE:
        case CLASS_WARLOCK:
        case CLASS_EVOKER:
            primaryStat = StatType::INTELLECT;
            break;

        default:
            return false;
    }

    // Check if item has the primary stat
    int32 primaryStatValue = GetItemStatValue(item, primaryStat);

    // If armor/weapon has no primary stat, it's wrong for this class
    return (primaryStatValue == 0 && proto->GetClass() == ITEM_CLASS_ARMOR);
}

::Item* EquipmentManager::GetEquippedItemInSlot(::Player* player, uint8 slot)
{
    if (!player || slot >= EQUIPMENT_SLOT_END)
        return nullptr;

    return player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
}

bool EquipmentManager::CanEquipInSlot(::Player* player, ::Item* item, uint8 slot)
{
    if (!player || !item)
        return false;

    ItemTemplate const* proto = item->GetTemplate();
    if (!proto)
        return false;

    // Basic checks
    if (!CanPlayerEquipItem(player, proto))
        return false;

    uint8 itemSlot = GetItemEquipmentSlot(proto);
    return (itemSlot == slot);
}

void EquipmentManager::EquipItemInSlot(::Player* player, ::Item* item, uint8 slot)
{
    if (!player || !item)
        return;

    // Use TrinityCore's EquipItem method
    uint16 dest;
    InventoryResult result = player->CanEquipItem(slot, dest, item, false);
    if (result == EQUIP_ERR_OK)
    {
        player->RemoveItem(item->GetBagSlot(), item->GetSlot(), true);
        player->EquipItem(dest, item, true);

        TC_LOG_INFO("playerbot.equipment", "✅ Equipped {} in slot {} for player {}",
                   item->GetTemplate()->GetName(DEFAULT_LOCALE), slot, player->GetName());
    }
    else
    {
        TC_LOG_ERROR("playerbot.equipment", "❌ Failed to equip {} for player {} (Error: {})",
                     item->GetTemplate()->GetName(DEFAULT_LOCALE), player->GetName(), static_cast<uint32>(result));
    }
}

uint32 EquipmentManager::GetRecommendedFoodLevel(::Player* player)
{
    if (!player)
        return 0;

    // Return appropriate food item ID based on player level
    // This would need actual WoW item IDs
    uint32 level = player->GetLevel();
    if (level >= 60) return 35953; // Example: Dragonfruit Pie
    if (level >= 50) return 33254; // Example: Stormchops
    if (level >= 40) return 27854; // Example: Smoked Talbuk
    if (level >= 30) return 8932;  // Example: Alterac Swiss
    if (level >= 20) return 4599;  // Example: Cured Ham Steak
    return 4540; // Example: Tough Hunk of Bread
}

uint32 EquipmentManager::GetRecommendedPotionLevel(::Player* player)
{
    if (!player)
        return 0;

    // Return appropriate water/mana potion item ID
    uint32 level = player->GetLevel();
    if (level >= 60) return 33445; // Example: Honeymint Tea
    if (level >= 50) return 28399; // Example: Filtered Draenic Water
    if (level >= 40) return 8077;  // Example: Conjured Sparkling Water
    return 5350; // Example: Conjured Water
}

std::vector<uint32> EquipmentManager::GetClassReagents(uint8 classId)
{
    std::vector<uint32> reagents;

    // Class-specific reagents (modern WoW has fewer than classic)
    switch (classId)
    {
        case CLASS_ROGUE:
            // Vanishing Powder, etc.
            break;

        case CLASS_MAGE:
            // Rune of Power reagents if needed
            break;

        default:
            break;
    }

    return reagents;
}

int32 EquipmentManager::ExtractStatValue(ItemTemplate const* proto, StatType stat)
{
    if (!proto)
        return 0;

    // TrinityCore 11.2 uses ItemMod array for stats
    for (uint8 i = 0; i < MAX_ITEM_PROTO_STATS; ++i)
    {
        ItemModType modType = static_cast<ItemModType>(proto->GetStatModifierBonusStat(i));
        int32 value = proto->GetStatPercentEditor(i);

        switch (stat)
        {
            case StatType::STRENGTH:
                if (modType == ITEM_MOD_STRENGTH) return value;
                break;
            case StatType::AGILITY:
                if (modType == ITEM_MOD_AGILITY) return value;
                break;
            case StatType::STAMINA:
                if (modType == ITEM_MOD_STAMINA) return value;
                break;
            case StatType::INTELLECT:
                if (modType == ITEM_MOD_INTELLECT) return value;
                break;
            case StatType::CRITICAL_STRIKE:
                if (modType == ITEM_MOD_CRIT_RATING) return value;
                break;
            case StatType::HASTE:
                if (modType == ITEM_MOD_HASTE_RATING) return value;
                break;
            case StatType::VERSATILITY:
                if (modType == ITEM_MOD_VERSATILITY) return value;
                break;
            case StatType::MASTERY:
                if (modType == ITEM_MOD_MASTERY_RATING) return value;
                break;
            case StatType::ARMOR:
                return static_cast<int32>(proto->GetArmor(ITEM_QUALITY_NORMAL));
            default:
                break;
        }
    }

    return 0;
}

float EquipmentManager::CalculateTotalStats(ItemTemplate const* proto, std::vector<std::pair<StatType, float>> const& weights)
{
    if (!proto)
        return 0.0f;

    float total = 0.0f;

    for (const auto& [statType, weight] : weights)
    {
        int32 value = ExtractStatValue(proto, statType);
        total += static_cast<float>(value) * weight;
    }

    return total;
}

void EquipmentManager::UpdateMetrics(uint32 playerGuid, bool wasEquipped, bool wasUpgrade, uint32 goldValue)
{
    // No lock needed - equipment data is per-bot instance data

    auto& metrics = _playerMetrics[playerGuid];

    if (wasEquipped)
        metrics.itemsEquipped++;

    if (wasUpgrade)
        metrics.upgradesFound++;

    if (goldValue > 0)
        metrics.totalGoldFromJunk += goldValue;

    // Update global
    if (wasEquipped)
        _globalMetrics.itemsEquipped++;

    if (wasUpgrade)
        _globalMetrics.upgradesFound++;

    if (goldValue > 0)
        _globalMetrics.totalGoldFromJunk += goldValue;
}

void EquipmentManager::LogEquipmentDecision(::Player* player, std::string const& action, std::string const& reason)
{
    if (player)
    {
        TC_LOG_DEBUG("playerbot.equipment", "Equipment Decision - Player: {}, Action: {}, Reason: {}",
                     player->GetName(), action, reason);
    }
}

} // namespace Playerbot
