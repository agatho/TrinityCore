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

#ifndef TRINITY_PLAYERBOTDEMONHUNTER_ACTIONS_H
#define TRINITY_PLAYERBOTDEMONHUNTER_ACTIONS_H

#ifdef WITH_PLAYERBOTS

#include "PlayerbotSpellAction.h"

namespace DemonHunterSpells
{
    // Shared Abilities
    constexpr uint32 CONSUME_MAGIC = 183752;
    constexpr uint32 DISRUPT = 183752;
    constexpr uint32 THROW_GLAIVE = 185123;
    constexpr uint32 SPECTRAL_SIGHT = 188501;
    constexpr uint32 TORMENT = 185245;
    constexpr uint32 IMPRISON = 217832;
    constexpr uint32 BLUR = 198589;
    constexpr uint32 DARKNESS = 196718;
    
    // Mobility
    constexpr uint32 GLIDE = 131347;
    constexpr uint32 DOUBLE_JUMP = 196055;
    constexpr uint32 FEL_RUSH = 195072;
    constexpr uint32 VENGEFUL_RETREAT = 198793;
    constexpr uint32 INFERNAL_STRIKE = 189110;
    
    // Havoc DPS Spells
    constexpr uint32 DEMONS_BITE = 162243;
    constexpr uint32 CHAOS_STRIKE = 162794;
    constexpr uint32 ANNIHILATION = 201427;
    constexpr uint32 BLADE_DANCE = 188499;
    constexpr uint32 DEATH_SWEEP = 210152;
    constexpr uint32 EYE_BEAM = 198013;
    constexpr uint32 IMMOLATION_AURA = 258920;
    constexpr uint32 METAMORPHOSIS_HAVOC = 191427;
    constexpr uint32 ESSENCE_BREAK = 258860;
    constexpr uint32 THE_HUNT = 370965;
    constexpr uint32 GLAIVE_TEMPEST = 342817;
    constexpr uint32 ELYSIAN_DECREE = 306830;
    constexpr uint32 SINFUL_BRAND = 317009;
    constexpr uint32 FEL_BARRAGE = 258925;
    
    // Havoc Buffs/Debuffs
    constexpr uint32 PREPARED = 203551;
    constexpr uint32 MOMENTUM = 208628;
    constexpr uint32 DEMON_SOUL = 347765;
    constexpr uint32 TACTICAL_ADVANTAGE = 258862;
    constexpr uint32 BURNING_BLADES = 346278;
    constexpr uint32 INNER_DEMON = 337313;
    
    // Vengeance Tank Spells  
    constexpr uint32 DEMON_SPIKES = 203720;
    constexpr uint32 IMMOLATION_AURA_TANK = 178740;
    constexpr uint32 INFERNAL_STRIKE_TANK = 189110;
    constexpr uint32 SHEAR = 203782;
    constexpr uint32 FRACTURE = 263642;
    constexpr uint32 SOUL_CLEAVE = 228477;
    constexpr uint32 SPIRIT_BOMB = 247454;
    constexpr uint32 SIGIL_OF_FLAME = 204596;
    constexpr uint32 SIGIL_OF_SILENCE = 202137;
    constexpr uint32 SIGIL_OF_MISERY = 207684;
    constexpr uint32 SIGIL_OF_CHAINS = 202138;
    constexpr uint32 FIERY_BRAND = 204021;
    constexpr uint32 METAMORPHOSIS_TANK = 187827;
    constexpr uint32 FEL_DEVASTATION = 212084;
    constexpr uint32 SOUL_BARRIER = 263648;
    constexpr uint32 LAST_RESORT = 209258;
    constexpr uint32 BULK_EXTRACTION = 320341;
    constexpr uint32 THE_HUNT_TANK = 370965;
    constexpr uint32 ELYSIAN_DECREE_TANK = 306830;
    
    // Vengeance Buffs/Debuffs
    constexpr uint32 SOUL_FRAGMENTS = 203981;
    constexpr uint32 PAINBRINGER = 207387;
    constexpr uint32 FEAST_OF_SOULS = 207697;
    constexpr uint32 CALCIFIED_SPIKES = 391171;
    constexpr uint32 METAMORPHOSIS_ARMOR = 187827;
    constexpr uint32 FRAILTY = 389958;
}

class TC_GAME_API PlayerbotDemonHunterAction : public PlayerbotSpellAction
{
public:
    PlayerbotDemonHunterAction(PlayerbotPlayerAI* ai, std::string const& name, uint32 spellId = 0)
        : PlayerbotSpellAction(ai, name, spellId) {}

protected:
    // Resource management
    uint32 GetFuryAmount() const;
    uint32 GetFuryPercent() const;
    uint32 GetPainAmount() const;
    uint32 GetPainPercent() const;
    bool HasEnoughFury(uint32 cost) const;
    bool HasEnoughPain(uint32 cost) const;
    
    // Specialization detection
    bool isHavoc() const;
    bool isVengeance() const;
    
    // Target evaluation and utility
    Unit* FindBestTarget() const;
    bool IsInCombat() const;
    bool IsInMeleeRange(Unit* target) const;
    uint32 CountNearbyEnemies(float range = 8.0f) const;
    uint32 CountNearbyAllies(float range = 30.0f) const;
    bool ShouldUseAoE() const;
    
    // Buff and debuff management
    bool HasMetamorphosis() const;
    bool HasDemonSpikes() const;
    bool HasImmolationAura() const;
    bool HasFieryBrand(Unit* target = nullptr) const;
    bool HasSigil(uint32 sigilSpell) const;
    bool HasPrepared() const;
    bool HasMomentum() const;
    uint32 GetSoulFragments() const;
    bool ShouldConsumeSoulFragments() const;
    
    // Havoc-specific utility
    bool ShouldUseEssenceBreak() const;
    bool ShouldUseBladeDance() const;
    bool ShouldUseChaosStrike() const;
    bool ShouldUseEyeBeam() const;
    bool IsInDemonicWindow() const;
    bool ShouldUseBurstCooldowns() const;
    
    // Vengeance-specific utility
    bool ShouldUseDemonSpikes() const;
    bool ShouldUseInfernalStrike() const;
    bool ShouldUseSigilOfFlame() const;
    bool ShouldUseFieryBrand() const;
    bool NeedsThreat() const;
    bool ShouldUseSoulCleave() const;
    bool ShouldUseSpiritBomb() const;
    
    // Defensive abilities
    bool ShouldUseBlur() const;
    bool ShouldUseDarkness() const;
    bool ShouldUseLastResort() const;
    bool ShouldUseSoulBarrier() const;
    
    // Utility abilities
    bool ShouldUseConsumeMagic() const;
    bool ShouldUseDisrupt() const;
    bool ShouldUseThrowGlaive() const;
    bool ShouldUseTorment() const;
    bool ShouldUseImprison() const;
};

// Havoc Demon Hunter Actions
class TC_GAME_API CastDemonsBite : public PlayerbotDemonHunterAction
{
public:
    CastDemonsBite(PlayerbotPlayerAI* ai) : PlayerbotDemonHunterAction(ai, "demons bite", DemonHunterSpells::DEMONS_BITE) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastChaosStrike : public PlayerbotDemonHunterAction
{
public:
    CastChaosStrike(PlayerbotPlayerAI* ai) : PlayerbotDemonHunterAction(ai, "chaos strike", DemonHunterSpells::CHAOS_STRIKE) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastAnnihilation : public PlayerbotDemonHunterAction
{
public:
    CastAnnihilation(PlayerbotPlayerAI* ai) : PlayerbotDemonHunterAction(ai, "annihilation", DemonHunterSpells::ANNIHILATION) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastBladeDance : public PlayerbotDemonHunterAction
{
public:
    CastBladeDance(PlayerbotPlayerAI* ai) : PlayerbotDemonHunterAction(ai, "blade dance", DemonHunterSpells::BLADE_DANCE) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastDeathSweep : public PlayerbotDemonHunterAction
{
public:
    CastDeathSweep(PlayerbotPlayerAI* ai) : PlayerbotDemonHunterAction(ai, "death sweep", DemonHunterSpells::DEATH_SWEEP) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastEyeBeam : public PlayerbotDemonHunterAction
{
public:
    CastEyeBeam(PlayerbotPlayerAI* ai) : PlayerbotDemonHunterAction(ai, "eye beam", DemonHunterSpells::EYE_BEAM) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastImmolationAuraHavoc : public PlayerbotDemonHunterAction
{
public:
    CastImmolationAuraHavoc(PlayerbotPlayerAI* ai) : PlayerbotDemonHunterAction(ai, "immolation aura havoc", DemonHunterSpells::IMMOLATION_AURA) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastMetamorphosisHavoc : public PlayerbotDemonHunterAction
{
public:
    CastMetamorphosisHavoc(PlayerbotPlayerAI* ai) : PlayerbotDemonHunterAction(ai, "metamorphosis havoc", DemonHunterSpells::METAMORPHOSIS_HAVOC) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastEssenceBreak : public PlayerbotDemonHunterAction
{
public:
    CastEssenceBreak(PlayerbotPlayerAI* ai) : PlayerbotDemonHunterAction(ai, "essence break", DemonHunterSpells::ESSENCE_BREAK) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastTheHuntHavoc : public PlayerbotDemonHunterAction
{
public:
    CastTheHuntHavoc(PlayerbotPlayerAI* ai) : PlayerbotDemonHunterAction(ai, "the hunt havoc", DemonHunterSpells::THE_HUNT) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastGlaiveTempest : public PlayerbotDemonHunterAction
{
public:
    CastGlaiveTempest(PlayerbotPlayerAI* ai) : PlayerbotDemonHunterAction(ai, "glaive tempest", DemonHunterSpells::GLAIVE_TEMPEST) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastElysianDecreeHavoc : public PlayerbotDemonHunterAction
{
public:
    CastElysianDecreeHavoc(PlayerbotPlayerAI* ai) : PlayerbotDemonHunterAction(ai, "elysian decree havoc", DemonHunterSpells::ELYSIAN_DECREE) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastFelBarrage : public PlayerbotDemonHunterAction
{
public:
    CastFelBarrage(PlayerbotPlayerAI* ai) : PlayerbotDemonHunterAction(ai, "fel barrage", DemonHunterSpells::FEL_BARRAGE) {}
protected:
    Unit* GetSpellTarget() const override;
};

// Vengeance Demon Hunter Actions
class TC_GAME_API CastDemonSpikes : public PlayerbotDemonHunterAction
{
public:
    CastDemonSpikes(PlayerbotPlayerAI* ai) : PlayerbotDemonHunterAction(ai, "demon spikes", DemonHunterSpells::DEMON_SPIKES) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastImmolationAuraTank : public PlayerbotDemonHunterAction
{
public:
    CastImmolationAuraTank(PlayerbotPlayerAI* ai) : PlayerbotDemonHunterAction(ai, "immolation aura tank", DemonHunterSpells::IMMOLATION_AURA_TANK) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastInfernalStrike : public PlayerbotDemonHunterAction
{
public:
    CastInfernalStrike(PlayerbotPlayerAI* ai) : PlayerbotDemonHunterAction(ai, "infernal strike", DemonHunterSpells::INFERNAL_STRIKE_TANK) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastShear : public PlayerbotDemonHunterAction
{
public:
    CastShear(PlayerbotPlayerAI* ai) : PlayerbotDemonHunterAction(ai, "shear", DemonHunterSpells::SHEAR) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastFracture : public PlayerbotDemonHunterAction
{
public:
    CastFracture(PlayerbotPlayerAI* ai) : PlayerbotDemonHunterAction(ai, "fracture", DemonHunterSpells::FRACTURE) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastSoulCleave : public PlayerbotDemonHunterAction
{
public:
    CastSoulCleave(PlayerbotPlayerAI* ai) : PlayerbotDemonHunterAction(ai, "soul cleave", DemonHunterSpells::SOUL_CLEAVE) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastSpiritBomb : public PlayerbotDemonHunterAction
{
public:
    CastSpiritBomb(PlayerbotPlayerAI* ai) : PlayerbotDemonHunterAction(ai, "spirit bomb", DemonHunterSpells::SPIRIT_BOMB) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastSigilOfFlame : public PlayerbotDemonHunterAction
{
public:
    CastSigilOfFlame(PlayerbotPlayerAI* ai) : PlayerbotDemonHunterAction(ai, "sigil of flame", DemonHunterSpells::SIGIL_OF_FLAME) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastSigilOfSilence : public PlayerbotDemonHunterAction
{
public:
    CastSigilOfSilence(PlayerbotPlayerAI* ai) : PlayerbotDemonHunterAction(ai, "sigil of silence", DemonHunterSpells::SIGIL_OF_SILENCE) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastSigilOfMisery : public PlayerbotDemonHunterAction
{
public:
    CastSigilOfMisery(PlayerbotPlayerAI* ai) : PlayerbotDemonHunterAction(ai, "sigil of misery", DemonHunterSpells::SIGIL_OF_MISERY) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastSigilOfChains : public PlayerbotDemonHunterAction
{
public:
    CastSigilOfChains(PlayerbotPlayerAI* ai) : PlayerbotDemonHunterAction(ai, "sigil of chains", DemonHunterSpells::SIGIL_OF_CHAINS) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastFieryBrand : public PlayerbotDemonHunterAction
{
public:
    CastFieryBrand(PlayerbotPlayerAI* ai) : PlayerbotDemonHunterAction(ai, "fiery brand", DemonHunterSpells::FIERY_BRAND) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastMetamorphosisTank : public PlayerbotDemonHunterAction
{
public:
    CastMetamorphosisTank(PlayerbotPlayerAI* ai) : PlayerbotDemonHunterAction(ai, "metamorphosis tank", DemonHunterSpells::METAMORPHOSIS_TANK) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastFelDevastation : public PlayerbotDemonHunterAction
{
public:
    CastFelDevastation(PlayerbotPlayerAI* ai) : PlayerbotDemonHunterAction(ai, "fel devastation", DemonHunterSpells::FEL_DEVASTATION) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastSoulBarrier : public PlayerbotDemonHunterAction
{
public:
    CastSoulBarrier(PlayerbotPlayerAI* ai) : PlayerbotDemonHunterAction(ai, "soul barrier", DemonHunterSpells::SOUL_BARRIER) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastLastResort : public PlayerbotDemonHunterAction
{
public:
    CastLastResort(PlayerbotPlayerAI* ai) : PlayerbotDemonHunterAction(ai, "last resort", DemonHunterSpells::LAST_RESORT) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastBulkExtraction : public PlayerbotDemonHunterAction
{
public:
    CastBulkExtraction(PlayerbotPlayerAI* ai) : PlayerbotDemonHunterAction(ai, "bulk extraction", DemonHunterSpells::BULK_EXTRACTION) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastTheHuntTank : public PlayerbotDemonHunterAction
{
public:
    CastTheHuntTank(PlayerbotPlayerAI* ai) : PlayerbotDemonHunterAction(ai, "the hunt tank", DemonHunterSpells::THE_HUNT_TANK) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastElysianDecreeTank : public PlayerbotDemonHunterAction
{
public:
    CastElysianDecreeTank(PlayerbotPlayerAI* ai) : PlayerbotDemonHunterAction(ai, "elysian decree tank", DemonHunterSpells::ELYSIAN_DECREE_TANK) {}
protected:
    Unit* GetSpellTarget() const override;
};

// Utility Actions
class TC_GAME_API CastFelRush : public PlayerbotDemonHunterAction
{
public:
    CastFelRush(PlayerbotPlayerAI* ai) : PlayerbotDemonHunterAction(ai, "fel rush", DemonHunterSpells::FEL_RUSH) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastVengefulRetreat : public PlayerbotDemonHunterAction
{
public:
    CastVengefulRetreat(PlayerbotPlayerAI* ai) : PlayerbotDemonHunterAction(ai, "vengeful retreat", DemonHunterSpells::VENGEFUL_RETREAT) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastThrowGlaive : public PlayerbotDemonHunterAction
{
public:
    CastThrowGlaive(PlayerbotPlayerAI* ai) : PlayerbotDemonHunterAction(ai, "throw glaive", DemonHunterSpells::THROW_GLAIVE) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastConsumeMagic : public PlayerbotDemonHunterAction
{
public:
    CastConsumeMagic(PlayerbotPlayerAI* ai) : PlayerbotDemonHunterAction(ai, "consume magic", DemonHunterSpells::CONSUME_MAGIC) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastDisrupt : public PlayerbotDemonHunterAction
{
public:
    CastDisrupt(PlayerbotPlayerAI* ai) : PlayerbotDemonHunterAction(ai, "disrupt", DemonHunterSpells::DISRUPT) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastTorment : public PlayerbotDemonHunterAction
{
public:
    CastTorment(PlayerbotPlayerAI* ai) : PlayerbotDemonHunterAction(ai, "torment", DemonHunterSpells::TORMENT) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastImprison : public PlayerbotDemonHunterAction
{
public:
    CastImprison(PlayerbotPlayerAI* ai) : PlayerbotDemonHunterAction(ai, "imprison", DemonHunterSpells::IMPRISON) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastBlur : public PlayerbotDemonHunterAction
{
public:
    CastBlur(PlayerbotPlayerAI* ai) : PlayerbotDemonHunterAction(ai, "blur", DemonHunterSpells::BLUR) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastDarkness : public PlayerbotDemonHunterAction
{
public:
    CastDarkness(PlayerbotPlayerAI* ai) : PlayerbotDemonHunterAction(ai, "darkness", DemonHunterSpells::DARKNESS) {}
protected:
    Unit* GetSpellTarget() const override;
};

class TC_GAME_API CastSpectralSight : public PlayerbotDemonHunterAction
{
public:
    CastSpectralSight(PlayerbotPlayerAI* ai) : PlayerbotDemonHunterAction(ai, "spectral sight", DemonHunterSpells::SPECTRAL_SIGHT) {}
protected:
    Unit* GetSpellTarget() const override;
};

#endif // WITH_PLAYERBOTS

#endif // TRINITY_PLAYERBOTDEMONHUNTER_ACTIONS_H