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

#ifndef TRINITY_PLAYERBOTEVOKER_ACTIONS_H
#define TRINITY_PLAYERBOTEVOKER_ACTIONS_H

#ifdef WITH_PLAYERBOTS

#include "PlayerbotCommon.h"

namespace EvokerSpells
{
    // Devastation (DPS) Spells - TWW 11.2
    static constexpr uint32 AZURE_STRIKE = 362969;
    static constexpr uint32 LIVING_FLAME = 361469;
    static constexpr uint32 LIVING_FLAME_DOT = 361500;
    static constexpr uint32 DISINTEGRATE = 356995;
    static constexpr uint32 FIRE_BREATH = 357208;
    static constexpr uint32 ETERNITY_SURGE = 359073;
    static constexpr uint32 PYRE = 357211;
    static constexpr uint32 SHATTERSHARD = 370452;
    static constexpr uint32 DRAGONRAGE = 375087;
    static constexpr uint32 TIP_THE_SCALES = 370553;
    static constexpr uint32 TIME_SKIP = 404977;
    static constexpr uint32 SNAPFIRE = 370713;
    static constexpr uint32 FIRESTORM = 368847;
    static constexpr uint32 ENGULF = 375020;
    static constexpr uint32 ONYX_LEGACY = 386348;
    static constexpr uint32 BURNOUT = 375802;
    static constexpr uint32 CHARGED_BLAST = 370454;
    static constexpr uint32 IRIDESCENCE_RED = 386353;
    static constexpr uint32 IRIDESCENCE_BLUE = 386399;
    static constexpr uint32 ESSENCE_BURST = 369297;
    static constexpr uint32 LEAPING_FLAMES = 369939;

    // Preservation (Healing) Spells - TWW 11.2
    static constexpr uint32 EMERALD_BLOSSOM = 355913;
    static constexpr uint32 VERDANT_EMBRACE = 360995;
    static constexpr uint32 DREAM_BREATH = 355936;
    static constexpr uint32 SPIRITBLOOM = 367226;
    static constexpr uint32 LIFE_GIVERS_FLAME = 361509;
    static constexpr uint32 REVERSION = 366155;
    static constexpr uint32 ECHO = 364343;
    static constexpr uint32 TEMPORAL_ANOMALY = 373862;
    static constexpr uint32 STASIS = 370537;
    static constexpr uint32 REWIND = 363534;
    static constexpr uint32 TIME_SKIP_HEALING = 404977;
    static constexpr uint32 BLESSING_OF_THE_BRONZE = 364342;
    static constexpr uint32 ZEPHYR = 374348;
    static constexpr uint32 CHRONO = 375234;
    static constexpr uint32 FLOW_STATE = 385696;
    static constexpr uint32 FIELD_OF_DREAMS = 370062;
    static constexpr uint32 CALL_OF_YSERA = 373834;
    static constexpr uint32 EMERALD_TRANCE = 371877;

    // Utility and Common Spells - TWW 11.2
    static constexpr uint32 QUELL = 351338;
    static constexpr uint32 TAIL_SWIPE = 368970;
    static constexpr uint32 WING_BUFFET = 357214;
    static constexpr uint32 EXPUNGE = 365585;
    static constexpr uint32 NATURALIZE = 360823;
    static constexpr uint32 SLEEP_WALK = 360806;
    static constexpr uint32 RESCUE = 370665;
    static constexpr uint32 TIME_SPIRAL = 374968;
    static constexpr uint32 HOVER = 358267;
    static constexpr uint32 LANDSLIDE = 358385;
    static constexpr uint32 DEEP_BREATH = 357210;
    static constexpr uint32 SOAR = 369536;
    static constexpr uint32 ERUPTION = 395160; // Augmentation signature spell
}

/**
 * @brief Base class for all Evoker actions
 * 
 * Provides common functionality for Essence resource management,
 * empowered spell mechanics, and utility methods shared across
 * all Evoker specializations.
 */
class TC_GAME_API PlayerbotEvokerAction : public PlayerbotSpellAction
{
public:
    PlayerbotEvokerAction(PlayerbotPlayerAI* ai, std::string const& name, uint32 spellId = 0);

protected:
    // Resource management
    uint32 GetEssenceAmount() const;
    uint32 GetEssencePercent() const;
    bool HasEnoughEssence(uint32 cost) const;
    bool ShouldConserveEssence() const;
    
    // Specialization detection
    bool isDevastation() const;
    bool isPreservation() const;
    bool isAugmentation() const;
    
    // Empowered spell mechanics
    bool ShouldChargeEmpoweredSpell() const;
    bool HasTimeForEmpower() const;
    uint32 GetOptimalEmpowerLevel() const;
    bool IsChannelingEmpoweredSpell() const;
    
    // Buff and debuff management
    bool HasDragonrage() const;
    bool HasSnapfire() const;
    bool HasBurnout() const;
    bool HasChargedBlast() const;
    bool HasIridescenceRed() const;
    bool HasIridescenceBlue() const;
    bool HasEssenceBurst() const;
    bool HasLeapingFlames(Unit* target = nullptr) const;
    bool HasCallOfYsera() const;
    bool HasEmeraldTrance() const;
    
    // Utility methods
    Unit* FindBestDpsTarget() const;
    Unit* FindBestHealTarget() const;
    Unit* FindLowestHealthAlly() const;
    Unit* FindTankTarget() const;
    std::vector<Unit*> FindHealingTargets(uint32 maxTargets = 5) const;
    uint32 CountNearbyEnemies(float range = 25.0f) const;
    uint32 CountNearbyAllies(float range = 30.0f) const;
    uint32 CountInjuredAllies(float healthPercent = 80.0f, float range = 30.0f) const;
    bool IsInOptimalRange(Unit* target, float range = 25.0f) const;
    bool ShouldUseAoE() const;
    
    // Combat state evaluation
    bool IsInCombat() const override;
    bool ShouldUseBurstCooldowns() const;
    bool IsInDragonrageWindow() const;
    bool IsInEmergencyMode() const;
    bool NeedsHealing() const;
};

// ===========================================
// Devastation Evoker Actions (DPS)
// ===========================================

class TC_GAME_API PlayerbotAzureStrikeAction : public PlayerbotEvokerAction
{
public:
    PlayerbotAzureStrikeAction(PlayerbotPlayerAI* ai);
    bool Execute(Unit* target) override;
    bool isUseful() override;
};

class TC_GAME_API PlayerbotLivingFlameAction : public PlayerbotEvokerAction
{
public:
    PlayerbotLivingFlameAction(PlayerbotPlayerAI* ai);
    bool Execute(Unit* target) override;
    bool isUseful() override;
private:
    bool HasLivingFlameDoT(Unit* target) const;
    bool ShouldRefreshDoT(Unit* target) const;
};

class TC_GAME_API PlayerbotDisintegrateAction : public PlayerbotEvokerAction
{
public:
    PlayerbotDisintegrateAction(PlayerbotPlayerAI* ai);
    bool Execute(Unit* target) override;
    bool isUseful() override;
};

class TC_GAME_API PlayerbotFireBreathAction : public PlayerbotEvokerAction
{
public:
    PlayerbotFireBreathAction(PlayerbotPlayerAI* ai);
    bool Execute(Unit* target) override;
    bool isUseful() override;
};

class TC_GAME_API PlayerbotEternitySurgeAction : public PlayerbotEvokerAction
{
public:
    PlayerbotEternitySurgeAction(PlayerbotPlayerAI* ai);
    bool Execute(Unit* target) override;
    bool isUseful() override;
};

class TC_GAME_API PlayerbotPyreAction : public PlayerbotEvokerAction
{
public:
    PlayerbotPyreAction(PlayerbotPlayerAI* ai);
    bool Execute(Unit* target) override;
    bool isUseful() override;
};

class TC_GAME_API PlayerbotDragonrageAction : public PlayerbotEvokerAction
{
public:
    PlayerbotDragonrageAction(PlayerbotPlayerAI* ai);
    bool Execute(Unit* target) override;
    bool isUseful() override;
};

class TC_GAME_API PlayerbotTipTheScalesAction : public PlayerbotEvokerAction
{
public:
    PlayerbotTipTheScalesAction(PlayerbotPlayerAI* ai);
    bool Execute(Unit* target) override;
    bool isUseful() override;
};

class TC_GAME_API PlayerbotTimeSkipAction : public PlayerbotEvokerAction
{
public:
    PlayerbotTimeSkipAction(PlayerbotPlayerAI* ai);
    bool Execute(Unit* target) override;
    bool isUseful() override;
};

// ===========================================
// Preservation Evoker Actions (Healing)
// ===========================================

class TC_GAME_API PlayerbotEmeraldBlossomAction : public PlayerbotEvokerAction
{
public:
    PlayerbotEmeraldBlossomAction(PlayerbotPlayerAI* ai);
    bool Execute(Unit* target) override;
    bool isUseful() override;
};

class TC_GAME_API PlayerbotVerdantEmbraceAction : public PlayerbotEvokerAction
{
public:
    PlayerbotVerdantEmbraceAction(PlayerbotPlayerAI* ai);
    bool Execute(Unit* target) override;
    bool isUseful() override;
};

class TC_GAME_API PlayerbotDreamBreathAction : public PlayerbotEvokerAction
{
public:
    PlayerbotDreamBreathAction(PlayerbotPlayerAI* ai);
    bool Execute(Unit* target) override;
    bool isUseful() override;
};

class TC_GAME_API PlayerbotSpiritbloomAction : public PlayerbotEvokerAction
{
public:
    PlayerbotSpiritbloomAction(PlayerbotPlayerAI* ai);
    bool Execute(Unit* target) override;
    bool isUseful() override;
};

class TC_GAME_API PlayerbotLifeGiversFlameAction : public PlayerbotEvokerAction
{
public:
    PlayerbotLifeGiversFlameAction(PlayerbotPlayerAI* ai);
    bool Execute(Unit* target) override;
    bool isUseful() override;
};

class TC_GAME_API PlayerbotReversionAction : public PlayerbotEvokerAction
{
public:
    PlayerbotReversionAction(PlayerbotPlayerAI* ai);
    bool Execute(Unit* target) override;
    bool isUseful() override;
private:
    bool HasReversionHoT(Unit* target) const;
};

class TC_GAME_API PlayerbotEchoAction : public PlayerbotEvokerAction
{
public:
    PlayerbotEchoAction(PlayerbotPlayerAI* ai);
    bool Execute(Unit* target) override;
    bool isUseful() override;
private:
    bool HasEchoEffect(Unit* target) const;
};

class TC_GAME_API PlayerbotStasisAction : public PlayerbotEvokerAction
{
public:
    PlayerbotStasisAction(PlayerbotPlayerAI* ai);
    bool Execute(Unit* target) override;
    bool isUseful() override;
};

class TC_GAME_API PlayerbotRewindAction : public PlayerbotEvokerAction
{
public:
    PlayerbotRewindAction(PlayerbotPlayerAI* ai);
    bool Execute(Unit* target) override;
    bool isUseful() override;
};

// ===========================================
// Utility Evoker Actions
// ===========================================

class TC_GAME_API PlayerbotQuellAction : public PlayerbotEvokerAction
{
public:
    PlayerbotQuellAction(PlayerbotPlayerAI* ai);
    bool Execute(Unit* target) override;
    bool isUseful() override;
};

class TC_GAME_API PlayerbotRescueAction : public PlayerbotEvokerAction
{
public:
    PlayerbotRescueAction(PlayerbotPlayerAI* ai);
    bool Execute(Unit* target) override;
    bool isUseful() override;
};

class TC_GAME_API PlayerbotExpungeAction : public PlayerbotEvokerAction
{
public:
    PlayerbotExpungeAction(PlayerbotPlayerAI* ai);
    bool Execute(Unit* target) override;
    bool isUseful() override;
};

class TC_GAME_API PlayerbotNaturalizeAction : public PlayerbotEvokerAction
{
public:
    PlayerbotNaturalizeAction(PlayerbotPlayerAI* ai);
    bool Execute(Unit* target) override;
    bool isUseful() override;
};

class TC_GAME_API PlayerbotHoverAction : public PlayerbotEvokerAction
{
public:
    PlayerbotHoverAction(PlayerbotPlayerAI* ai);
    bool Execute(Unit* target) override;
    bool isUseful() override;
};

class TC_GAME_API PlayerbotDeepBreathAction : public PlayerbotEvokerAction
{
public:
    PlayerbotDeepBreathAction(PlayerbotPlayerAI* ai);
    bool Execute(Unit* target) override;
    bool isUseful() override;
};

#endif // WITH_PLAYERBOTS

#endif // TRINITY_PLAYERBOTEVOKER_ACTIONS_H