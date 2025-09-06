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

#ifndef TRINITY_PLAYERBOTMONKACTIONS_H
#define TRINITY_PLAYERBOTMONKACTIONS_H

#ifdef WITH_PLAYERBOTS

#include "PlayerbotCommon.h"

/**
 * @brief Spell IDs for Monk abilities - The War Within 11.2
 */
namespace MonkSpells
{
    // Common Monk Spells
    constexpr uint32 TIGER_PALM = 100780;
    constexpr uint32 BLACKOUT_KICK = 100784;
    constexpr uint32 ROLL = 109132;
    constexpr uint32 CHI_TORPEDO = 115008;
    constexpr uint32 TRANSCENDENCE = 101643;
    constexpr uint32 TRANSCENDENCE_TRANSFER = 119996;
    constexpr uint32 PARALYSIS = 115078;
    constexpr uint32 SPEAR_HAND_STRIKE = 116705;
    constexpr uint32 LEG_SWEEP = 119381;
    constexpr uint32 FORTIFYING_BREW = 115203;
    constexpr uint32 TOUCH_OF_DEATH = 322109;
    constexpr uint32 VIVIFY = 116670;
    constexpr uint32 DETOX = 218164;
    constexpr uint32 RING_OF_PEACE = 116844;
    
    // Brewmaster Spells
    constexpr uint32 KEG_SMASH = 121253;
    constexpr uint32 BREATH_OF_FIRE = 115181;
    constexpr uint32 SPINNING_CRANE_KICK_BM = 322729;
    constexpr uint32 GUARD = 115295;
    constexpr uint32 IRONSKIN_BREW = 115308;
    constexpr uint32 PURIFYING_BREW = 119582;
    constexpr uint32 CELESTIAL_BREW = 322507;
    constexpr uint32 ZEN_MEDITATION = 115176;
    constexpr uint32 INVOKE_NIUZAO = 132578;
    constexpr uint32 RUSHING_JADE_WIND_BM = 116847;
    constexpr uint32 BLACK_OX_BREW = 115399;
    constexpr uint32 EXPLODING_KEG = 325153;
    constexpr uint32 BONEDUST_BREW = 386276;
    constexpr uint32 WEAPONS_OF_ORDER_BM = 387184;
    constexpr uint32 STAGGER = 115069;
    constexpr uint32 SHUFFLE = 215479;
    
    // Mistweaver Spells
    constexpr uint32 SOOTHING_MIST = 115175;
    constexpr uint32 ENVELOPING_MIST = 124682;
    constexpr uint32 ESSENCE_FONT = 191837;
    constexpr uint32 RENEWING_MIST = 115151;
    constexpr uint32 LIFE_COCOON = 116849;
    constexpr uint32 REVIVAL = 115310;
    constexpr uint32 THUNDER_FOCUS_TEA = 116680;
    constexpr uint32 MANA_TEA = 197908;
    constexpr uint32 INVOKE_YU_LON = 123904;
    constexpr uint32 INVOKE_CHI_JI = 325197;
    constexpr uint32 CRACKLING_JADE_LIGHTNING = 117952;
    constexpr uint32 RISING_SUN_KICK_MW = 107428;
    constexpr uint32 SPINNING_CRANE_KICK_MW = 101546;
    constexpr uint32 RUSHING_JADE_WIND_MW = 116847;
    constexpr uint32 SHEILUNS_GIFT = 399491;
    constexpr uint32 FAELINE_STOMP_MW = 388193;
    constexpr uint32 FALLEN_MONK_KETHIEL = 389565;
    constexpr uint32 TEACHINGS_OF_THE_MONASTERY = 116645;
    
    // Windwalker Spells
    constexpr uint32 FISTS_OF_FURY = 113656;
    constexpr uint32 RISING_SUN_KICK_WW = 107428;
    constexpr uint32 SPINNING_CRANE_KICK_WW = 101546;
    constexpr uint32 WHIRLING_DRAGON_PUNCH = 152175;
    constexpr uint32 STRIKE_OF_THE_WINDLORD = 392983;
    constexpr uint32 INVOKE_XUEN = 123904;
    constexpr uint32 STORM_EARTH_AND_FIRE = 137639;
    constexpr uint32 SERENITY = 152173;
    constexpr uint32 ENERGIZING_ELIXIR = 115288;
    constexpr uint32 RUSHING_JADE_WIND_WW = 116847;
    constexpr uint32 FLYING_SERPENT_KICK = 101545;
    constexpr uint32 CHI_WAVE = 115098;
    constexpr uint32 CHI_BURST = 123986;
    constexpr uint32 FAELINE_STOMP_WW = 388193;
    constexpr uint32 WEAPONS_OF_ORDER_WW = 387184;
    constexpr uint32 DANCE_OF_CHI_JI = 325201;
    constexpr uint32 COMBO_BREAKER = 137384;
    constexpr uint32 MARK_OF_THE_CRANE = 228287;
    constexpr uint32 HIT_COMBO = 196740;
}

/**
 * @brief Base class for all Monk actions
 */
class TC_GAME_API PlayerbotMonkAction : public PlayerbotSpellAction
{
public:
    PlayerbotMonkAction(PlayerbotPlayerAI* ai, std::string const& name, uint32 spellId = 0)
        : PlayerbotSpellAction(ai, name, spellId) {}
    
    // Common Monk utility methods
    uint32 GetEnergyAmount() const;
    uint32 GetEnergyPercent() const;
    uint32 GetChi() const;
    bool HasEnoughEnergy(uint32 cost) const;
    bool HasEnoughChi(uint32 cost) const;
    bool IsInCombat() const;
    
    // Specialization detection
    bool isBrewmaster() const;
    bool isMistweaver() const;
    bool isWindwalker() const;
    
    // Target evaluation
    Unit* FindBestTarget() const;
    Unit* FindBestHealingTarget() const;
    uint32 CountNearbyEnemies(float range = 8.0f) const;
    uint32 CountInjuredGroupMembers(float threshold = 0.8f) const;
    bool ShouldUseAoE() const;
    bool ShouldUseAoEHealing() const;
    
    // Buff and debuff management
    bool HasShuffle() const;
    bool HasStagger() const;
    float GetStaggerDamage() const;
    bool HasTeachingsOfTheMonastery() const;
    uint32 GetTeachingsStacks() const;
    bool HasDanceOfChiJi() const;
    bool HasComboBreaker() const;
    bool HasMarkOfTheCrane(Unit* target) const;
    uint32 GetMarkOfTheCraneCount() const;
    bool HasHitCombo() const;
    
    // Mobility and positioning
    bool IsInMeleeRange(Unit* target) const;
    bool ShouldUseRoll() const;
    bool ShouldUseTranscendence() const;
    
protected:
    // Resource validation for different specs
    bool CanCastBrewmasterSpell() const;
    bool CanCastMistweaverSpell() const;
    bool CanCastWindwalkerSpell() const;
};

// Brewmaster Monk Actions
class TC_GAME_API CastKegSmash : public PlayerbotMonkAction
{
public:
    CastKegSmash(PlayerbotPlayerAI* ai) : PlayerbotMonkAction(ai, "keg smash", MonkSpells::KEG_SMASH) {}
};

class TC_GAME_API CastBreathOfFire : public PlayerbotMonkAction
{
public:
    CastBreathOfFire(PlayerbotPlayerAI* ai) : PlayerbotMonkAction(ai, "breath of fire", MonkSpells::BREATH_OF_FIRE) {}
};

class TC_GAME_API CastIronskinBrew : public PlayerbotMonkAction
{
public:
    CastIronskinBrew(PlayerbotPlayerAI* ai) : PlayerbotMonkAction(ai, "ironskin brew", MonkSpells::IRONSKIN_BREW) {}
};

class TC_GAME_API CastPurifyingBrew : public PlayerbotMonkAction
{
public:
    CastPurifyingBrew(PlayerbotPlayerAI* ai) : PlayerbotMonkAction(ai, "purifying brew", MonkSpells::PURIFYING_BREW) {}
};

class TC_GAME_API CastCelestialBrew : public PlayerbotMonkAction
{
public:
    CastCelestialBrew(PlayerbotPlayerAI* ai) : PlayerbotMonkAction(ai, "celestial brew", MonkSpells::CELESTIAL_BREW) {}
};

class TC_GAME_API CastZenMeditation : public PlayerbotMonkAction
{
public:
    CastZenMeditation(PlayerbotPlayerAI* ai) : PlayerbotMonkAction(ai, "zen meditation", MonkSpells::ZEN_MEDITATION) {}
};

class TC_GAME_API CastInvokeNiuzao : public PlayerbotMonkAction
{
public:
    CastInvokeNiuzao(PlayerbotPlayerAI* ai) : PlayerbotMonkAction(ai, "invoke niuzao", MonkSpells::INVOKE_NIUZAO) {}
};

class TC_GAME_API CastBlackOxBrew : public PlayerbotMonkAction
{
public:
    CastBlackOxBrew(PlayerbotPlayerAI* ai) : PlayerbotMonkAction(ai, "black ox brew", MonkSpells::BLACK_OX_BREW) {}
};

class TC_GAME_API CastExplodingKeg : public PlayerbotMonkAction
{
public:
    CastExplodingKeg(PlayerbotPlayerAI* ai) : PlayerbotMonkAction(ai, "exploding keg", MonkSpells::EXPLODING_KEG) {}
};

class TC_GAME_API CastBonedustBrew : public PlayerbotMonkAction
{
public:
    CastBonedustBrew(PlayerbotPlayerAI* ai) : PlayerbotMonkAction(ai, "bonedust brew", MonkSpells::BONEDUST_BREW) {}
};

class TC_GAME_API CastWeaponsOfOrderBM : public PlayerbotMonkAction
{
public:
    CastWeaponsOfOrderBM(PlayerbotPlayerAI* ai) : PlayerbotMonkAction(ai, "weapons of order", MonkSpells::WEAPONS_OF_ORDER_BM) {}
};

// Mistweaver Monk Actions
class TC_GAME_API CastSoothingMist : public PlayerbotMonkAction
{
public:
    CastSoothingMist(PlayerbotPlayerAI* ai) : PlayerbotMonkAction(ai, "soothing mist", MonkSpells::SOOTHING_MIST) {}
};

class TC_GAME_API CastEnvelopingMist : public PlayerbotMonkAction
{
public:
    CastEnvelopingMist(PlayerbotPlayerAI* ai) : PlayerbotMonkAction(ai, "enveloping mist", MonkSpells::ENVELOPING_MIST) {}
};

class TC_GAME_API CastEssenceFont : public PlayerbotMonkAction
{
public:
    CastEssenceFont(PlayerbotPlayerAI* ai) : PlayerbotMonkAction(ai, "essence font", MonkSpells::ESSENCE_FONT) {}
};

class TC_GAME_API CastRenewingMist : public PlayerbotMonkAction
{
public:
    CastRenewingMist(PlayerbotPlayerAI* ai) : PlayerbotMonkAction(ai, "renewing mist", MonkSpells::RENEWING_MIST) {}
};

class TC_GAME_API CastLifeCocoon : public PlayerbotMonkAction
{
public:
    CastLifeCocoon(PlayerbotPlayerAI* ai) : PlayerbotMonkAction(ai, "life cocoon", MonkSpells::LIFE_COCOON) {}
};

class TC_GAME_API CastRevival : public PlayerbotMonkAction
{
public:
    CastRevival(PlayerbotPlayerAI* ai) : PlayerbotMonkAction(ai, "revival", MonkSpells::REVIVAL) {}
};

class TC_GAME_API CastThunderFocusTea : public PlayerbotMonkAction
{
public:
    CastThunderFocusTea(PlayerbotPlayerAI* ai) : PlayerbotMonkAction(ai, "thunder focus tea", MonkSpells::THUNDER_FOCUS_TEA) {}
};

class TC_GAME_API CastManaTea : public PlayerbotMonkAction
{
public:
    CastManaTea(PlayerbotPlayerAI* ai) : PlayerbotMonkAction(ai, "mana tea", MonkSpells::MANA_TEA) {}
};

class TC_GAME_API CastInvokeYuLon : public PlayerbotMonkAction
{
public:
    CastInvokeYuLon(PlayerbotPlayerAI* ai) : PlayerbotMonkAction(ai, "invoke yu'lon", MonkSpells::INVOKE_YU_LON) {}
};

class TC_GAME_API CastInvokeChiJi : public PlayerbotMonkAction
{
public:
    CastInvokeChiJi(PlayerbotPlayerAI* ai) : PlayerbotMonkAction(ai, "invoke chi-ji", MonkSpells::INVOKE_CHI_JI) {}
};

class TC_GAME_API CastCracklingJadeLightning : public PlayerbotMonkAction
{
public:
    CastCracklingJadeLightning(PlayerbotPlayerAI* ai) : PlayerbotMonkAction(ai, "crackling jade lightning", MonkSpells::CRACKLING_JADE_LIGHTNING) {}
};

class TC_GAME_API CastSheilunsGift : public PlayerbotMonkAction
{
public:
    CastSheilunsGift(PlayerbotPlayerAI* ai) : PlayerbotMonkAction(ai, "sheilun's gift", MonkSpells::SHEILUNS_GIFT) {}
};

class TC_GAME_API CastFaelineStompMW : public PlayerbotMonkAction
{
public:
    CastFaelineStompMW(PlayerbotPlayerAI* ai) : PlayerbotMonkAction(ai, "faeline stomp", MonkSpells::FAELINE_STOMP_MW) {}
};

// Windwalker Monk Actions
class TC_GAME_API CastFistsOfFury : public PlayerbotMonkAction
{
public:
    CastFistsOfFury(PlayerbotPlayerAI* ai) : PlayerbotMonkAction(ai, "fists of fury", MonkSpells::FISTS_OF_FURY) {}
};

class TC_GAME_API CastRisingSunKickWW : public PlayerbotMonkAction
{
public:
    CastRisingSunKickWW(PlayerbotPlayerAI* ai) : PlayerbotMonkAction(ai, "rising sun kick", MonkSpells::RISING_SUN_KICK_WW) {}
};

class TC_GAME_API CastWhirlingDragonPunch : public PlayerbotMonkAction
{
public:
    CastWhirlingDragonPunch(PlayerbotPlayerAI* ai) : PlayerbotMonkAction(ai, "whirling dragon punch", MonkSpells::WHIRLING_DRAGON_PUNCH) {}
};

class TC_GAME_API CastStrikeOfTheWindlord : public PlayerbotMonkAction
{
public:
    CastStrikeOfTheWindlord(PlayerbotPlayerAI* ai) : PlayerbotMonkAction(ai, "strike of the windlord", MonkSpells::STRIKE_OF_THE_WINDLORD) {}
};

class TC_GAME_API CastInvokeXuen : public PlayerbotMonkAction
{
public:
    CastInvokeXuen(PlayerbotPlayerAI* ai) : PlayerbotMonkAction(ai, "invoke xuen", MonkSpells::INVOKE_XUEN) {}
};

class TC_GAME_API CastStormEarthAndFire : public PlayerbotMonkAction
{
public:
    CastStormEarthAndFire(PlayerbotPlayerAI* ai) : PlayerbotMonkAction(ai, "storm earth and fire", MonkSpells::STORM_EARTH_AND_FIRE) {}
};

class TC_GAME_API CastSerenity : public PlayerbotMonkAction
{
public:
    CastSerenity(PlayerbotPlayerAI* ai) : PlayerbotMonkAction(ai, "serenity", MonkSpells::SERENITY) {}
};

class TC_GAME_API CastEnergizingElixir : public PlayerbotMonkAction
{
public:
    CastEnergizingElixir(PlayerbotPlayerAI* ai) : PlayerbotMonkAction(ai, "energizing elixir", MonkSpells::ENERGIZING_ELIXIR) {}
};

class TC_GAME_API CastFlyingSerpentKick : public PlayerbotMonkAction
{
public:
    CastFlyingSerpentKick(PlayerbotPlayerAI* ai) : PlayerbotMonkAction(ai, "flying serpent kick", MonkSpells::FLYING_SERPENT_KICK) {}
};

class TC_GAME_API CastChiWave : public PlayerbotMonkAction
{
public:
    CastChiWave(PlayerbotPlayerAI* ai) : PlayerbotMonkAction(ai, "chi wave", MonkSpells::CHI_WAVE) {}
};

class TC_GAME_API CastChiBurst : public PlayerbotMonkAction
{
public:
    CastChiBurst(PlayerbotPlayerAI* ai) : PlayerbotMonkAction(ai, "chi burst", MonkSpells::CHI_BURST) {}
};

class TC_GAME_API CastFaelineStompWW : public PlayerbotMonkAction
{
public:
    CastFaelineStompWW(PlayerbotPlayerAI* ai) : PlayerbotMonkAction(ai, "faeline stomp", MonkSpells::FAELINE_STOMP_WW) {}
};

class TC_GAME_API CastWeaponsOfOrderWW : public PlayerbotMonkAction
{
public:
    CastWeaponsOfOrderWW(PlayerbotPlayerAI* ai) : PlayerbotMonkAction(ai, "weapons of order", MonkSpells::WEAPONS_OF_ORDER_WW) {}
};

// Common Monk Actions
class TC_GAME_API CastTigerPalm : public PlayerbotMonkAction
{
public:
    CastTigerPalm(PlayerbotPlayerAI* ai) : PlayerbotMonkAction(ai, "tiger palm", MonkSpells::TIGER_PALM) {}
};

class TC_GAME_API CastBlackoutKick : public PlayerbotMonkAction
{
public:
    CastBlackoutKick(PlayerbotPlayerAI* ai) : PlayerbotMonkAction(ai, "blackout kick", MonkSpells::BLACKOUT_KICK) {}
};

class TC_GAME_API CastSpinningCraneKick : public PlayerbotMonkAction
{
public:
    CastSpinningCraneKick(PlayerbotPlayerAI* ai) : PlayerbotMonkAction(ai, "spinning crane kick", MonkSpells::SPINNING_CRANE_KICK_WW) {}
};

class TC_GAME_API CastRushingJadeWind : public PlayerbotMonkAction
{
public:
    CastRushingJadeWind(PlayerbotPlayerAI* ai) : PlayerbotMonkAction(ai, "rushing jade wind", MonkSpells::RUSHING_JADE_WIND_WW) {}
};

class TC_GAME_API CastRoll : public PlayerbotMonkAction
{
public:
    CastRoll(PlayerbotPlayerAI* ai) : PlayerbotMonkAction(ai, "roll", MonkSpells::ROLL) {}
};

class TC_GAME_API CastChiTorpedo : public PlayerbotMonkAction
{
public:
    CastChiTorpedo(PlayerbotPlayerAI* ai) : PlayerbotMonkAction(ai, "chi torpedo", MonkSpells::CHI_TORPEDO) {}
};

class TC_GAME_API CastTranscendence : public PlayerbotMonkAction
{
public:
    CastTranscendence(PlayerbotPlayerAI* ai) : PlayerbotMonkAction(ai, "transcendence", MonkSpells::TRANSCENDENCE) {}
};

class TC_GAME_API CastParalysis : public PlayerbotMonkAction
{
public:
    CastParalysis(PlayerbotPlayerAI* ai) : PlayerbotMonkAction(ai, "paralysis", MonkSpells::PARALYSIS) {}
};

class TC_GAME_API CastSpearHandStrike : public PlayerbotMonkAction
{
public:
    CastSpearHandStrike(PlayerbotPlayerAI* ai) : PlayerbotMonkAction(ai, "spear hand strike", MonkSpells::SPEAR_HAND_STRIKE) {}
};

class TC_GAME_API CastLegSweep : public PlayerbotMonkAction
{
public:
    CastLegSweep(PlayerbotPlayerAI* ai) : PlayerbotMonkAction(ai, "leg sweep", MonkSpells::LEG_SWEEP) {}
};

class TC_GAME_API CastFortifyingBrew : public PlayerbotMonkAction
{
public:
    CastFortifyingBrew(PlayerbotPlayerAI* ai) : PlayerbotMonkAction(ai, "fortifying brew", MonkSpells::FORTIFYING_BREW) {}
};

class TC_GAME_API CastTouchOfDeath : public PlayerbotMonkAction
{
public:
    CastTouchOfDeath(PlayerbotPlayerAI* ai) : PlayerbotMonkAction(ai, "touch of death", MonkSpells::TOUCH_OF_DEATH) {}
};

class TC_GAME_API CastVivify : public PlayerbotMonkAction
{
public:
    CastVivify(PlayerbotPlayerAI* ai) : PlayerbotMonkAction(ai, "vivify", MonkSpells::VIVIFY) {}
};

class TC_GAME_API CastDetox : public PlayerbotMonkAction
{
public:
    CastDetox(PlayerbotPlayerAI* ai) : PlayerbotMonkAction(ai, "detox", MonkSpells::DETOX) {}
};

class TC_GAME_API CastRingOfPeace : public PlayerbotMonkAction
{
public:
    CastRingOfPeace(PlayerbotPlayerAI* ai) : PlayerbotMonkAction(ai, "ring of peace", MonkSpells::RING_OF_PEACE) {}
};

#endif // WITH_PLAYERBOTS

#endif // TRINITY_PLAYERBOTMONKACTIONS_H