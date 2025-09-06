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

#include "PlayerbotMageStrategy.h"
#include "PlayerbotMageActions.h"
#include "PlayerbotPlayerAI.h"
#include "Player.h"
#include "SpellAuraEffects.h"
#include "SpellHistory.h"
#include "Group.h"

PlayerbotMageStrategy::PlayerbotMageStrategy(PlayerbotPlayerAI* ai, std::string const& name)
    : PlayerbotStrategy(ai, name) {}

void PlayerbotMageStrategy::InitializeActions()
{
    RegisterCommonActions();
}

void PlayerbotMageStrategy::InitializeTriggers()
{
    RegisterCommonTriggers();
}

float PlayerbotMageStrategy::GetActionPriority(std::string const& actionName) const
{
    // Emergency actions
    if (actionName == "ice block" && GetManaPercent() < 10) return 1.0f;
    if (actionName == "counterspell" && ShouldUseCounterspell()) return 0.95f;
    
    // Buff maintenance
    if (actionName == "arcane intellect" && NeedsArcaneIntellect()) return 0.9f;
    if (actionName == "ice barrier" && NeedsMageArmor()) return 0.85f;
    
    // Basic combat
    if (actionName == "frostbolt" && isFrost()) return 0.5f;
    if (actionName == "fireball" && isFire()) return 0.5f;
    if (actionName == "arcane blast" && isArcane()) return 0.5f;
    
    return 0.3f;
}

void PlayerbotMageStrategy::RegisterCommonActions()
{
    RegisterAction("arcane intellect", std::make_unique<CastArcaneIntellect>(_ai));
    RegisterAction("counterspell", std::make_unique<CastCounterspell>(_ai));
    RegisterAction("dispel magic", std::make_unique<CastDispelMagic>(_ai));
    RegisterAction("remove curse", std::make_unique<CastRemoveCurse>(_ai));
    RegisterAction("blink", std::make_unique<CastBlink>(_ai));
    RegisterAction("mirror image", std::make_unique<CastMirrorImage>(_ai));
    RegisterAction("ice block", std::make_unique<CastIceBlock>(_ai));
    RegisterAction("invisibility", std::make_unique<CastInvisibility>(_ai));
    RegisterAction("time warp", std::make_unique<CastTimeWarp>(_ai));
}

void PlayerbotMageStrategy::RegisterCommonTriggers()
{
    // TODO: Implement when trigger system is available
}

bool PlayerbotMageStrategy::ShouldUseBlink() const
{
    Player* bot = _ai->GetBot();
    if (!bot || !bot->IsInCombat())
        return false;
        
    // Use blink to escape melee range
    Unit* target = FindBestTarget();
    return target && bot->GetDistance(target) < 8.0f;
}

bool PlayerbotMageStrategy::ShouldUseCounterspell() const
{
    Unit* target = FindBestTarget();
    return target && IsTargetCastingInterruptible(target);
}

bool PlayerbotMageStrategy::ShouldUseMirrorImage() const
{
    Player* bot = _ai->GetBot();
    if (!bot || !bot->IsInCombat())
        return false;
        
    return bot->GetHealthPct() < 50.0f && CountNearbyEnemies(15.0f) >= 2;
}

bool PlayerbotMageStrategy::ShouldUseIceBlock() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->GetHealthPct() < 15.0f;
}

bool PlayerbotMageStrategy::ShouldUseInvisibility() const
{
    Player* bot = _ai->GetBot();
    if (!bot || bot->IsInCombat())
        return false;
        
    return bot->GetHealthPct() < 30.0f;
}

bool PlayerbotMageStrategy::ShouldUseTimeWarp() const
{
    Player* bot = _ai->GetBot();
    if (!bot || !bot->IsInCombat() || HasTimeWarpActive())
        return false;
        
    // Use Time Warp in challenging situations
    return CountNearbyEnemies(15.0f) >= 3 || bot->GetHealthPct() < 40.0f;
}

bool PlayerbotMageStrategy::NeedsArcaneIntellect() const
{
    Player* bot = _ai->GetBot();
    return bot && !bot->HasAura(MageSpells::ARCANE_INTELLECT);
}

bool PlayerbotMageStrategy::NeedsMageArmor() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return false;
        
    // Check for any mage armor
    return !bot->HasAura(MageSpells::FROST_ARMOR) && 
           !bot->HasAura(MageSpells::ICE_BARRIER) &&
           !bot->HasAura(MageSpells::BLAZING_BARRIER);
}

uint32 PlayerbotMageStrategy::GetManaAmount() const
{
    Player* bot = _ai->GetBot();
    return bot ? bot->GetPower(POWER_MANA) : 0;
}

uint32 PlayerbotMageStrategy::GetManaPercent() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0;
    
    uint32 maxMana = bot->GetMaxPower(POWER_MANA);
    return maxMana > 0 ? (GetManaAmount() * 100) / maxMana : 0;
}

bool PlayerbotMageStrategy::ShouldConserveMana() const
{
    return GetManaPercent() < 20;
}

bool PlayerbotMageStrategy::ShouldEvocate() const
{
    Player* bot = _ai->GetBot();
    return bot && GetManaPercent() < 30 && 
           !bot->GetSpellHistory()->HasCooldown(MageSpells::EVOCATION);
}

bool PlayerbotMageStrategy::isArcane() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(MageSpells::ARCANE_BLAST);
}

bool PlayerbotMageStrategy::isFire() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(MageSpells::FIREBALL);
}

bool PlayerbotMageStrategy::isFrost() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasSpell(MageSpells::ICE_LANCE);
}

Unit* PlayerbotMageStrategy::FindBestTarget() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return nullptr;
    
    Unit* currentTarget = bot->GetSelectedUnit();
    if (currentTarget && currentTarget->IsAlive() && bot->IsWithinLOSInMap(currentTarget))
        return currentTarget;
        
    // Find nearby hostile target
    std::list<Unit*> targets;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck check(bot, bot, 30.0f);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(bot, targets, check);
    Cell::VisitAllObjects(bot, searcher, 30.0f);
    
    for (Unit* target : targets)
    {
        if (target->IsAlive() && bot->IsWithinLOSInMap(target))
            return target;
    }
    
    return nullptr;
}

bool PlayerbotMageStrategy::IsInCombat() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->IsInCombat();
}

uint32 PlayerbotMageStrategy::CountNearbyEnemies(float range) const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0;
    
    std::list<Unit*> targets;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck check(bot, bot, range);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(bot, targets, check);
    Cell::VisitAllObjects(bot, searcher, range);
    
    uint32 count = 0;
    for (Unit* target : targets)
    {
        if (target->IsAlive())
            count++;
    }
    
    return count;
}

bool PlayerbotMageStrategy::ShouldUseAoE() const
{
    return CountNearbyEnemies(8.0f) >= 3;
}

bool PlayerbotMageStrategy::IsTargetInRange(Unit* target, float range) const
{
    Player* bot = _ai->GetBot();
    return bot && target && bot->GetDistance(target) <= range;
}

bool PlayerbotMageStrategy::HasTimeWarpActive() const
{
    Player* bot = _ai->GetBot();
    return bot && (bot->HasAura(MageSpells::TIME_WARP) || bot->HasAura(2825)); // Bloodlust/Heroism
}

bool PlayerbotMageStrategy::IsTargetCastingInterruptible(Unit* target) const
{
    if (!target || !target->HasUnitState(UNIT_STATE_CASTING))
        return false;
        
    Spell* spell = target->GetCurrentSpell(CURRENT_GENERIC_SPELL);
    return spell && !spell->IsAutoRepeat() && spell->GetCastTime() > 1500;
}

bool PlayerbotMageStrategy::ShouldDispelMagic(Unit* target) const
{
    return target && target->HasAuraType(SPELL_AURA_MOD_INCREASE_SPEED);
}

bool PlayerbotMageStrategy::ShouldRemoveCurse(Unit* target) const
{
    return target && target->HasAuraType(SPELL_AURA_DUMMY); // Curse check
}

// ===== ARCANE MAGE STRATEGY =====

PlayerbotArcaneMageStrategy::PlayerbotArcaneMageStrategy(PlayerbotPlayerAI* ai)
    : PlayerbotMageStrategy(ai, "arcane") {}

void PlayerbotArcaneMageStrategy::InitializeActions()
{
    PlayerbotMageStrategy::InitializeActions();
    RegisterArcaneActions();
}

void PlayerbotArcaneMageStrategy::InitializeTriggers()
{
    PlayerbotMageStrategy::InitializeTriggers();
    RegisterArcaneTriggers();
}

void PlayerbotArcaneMageStrategy::RegisterArcaneActions()
{
    RegisterAction("arcane blast", std::make_unique<CastArcaneBlast>(_ai));
    RegisterAction("arcane barrage", std::make_unique<CastArcaneBarrage>(_ai));
    RegisterAction("arcane missile", std::make_unique<CastArcaneMissile>(_ai));
    RegisterAction("arcane orb", std::make_unique<CastArcaneOrb>(_ai));
    RegisterAction("arcane explosion", std::make_unique<CastArcaneExplosion>(_ai));
    RegisterAction("presence of mind", std::make_unique<CastPresenceOfMind>(_ai));
    RegisterAction("arcane power", std::make_unique<CastArcanePower>(_ai));
    RegisterAction("touch of the magi", std::make_unique<CastTouchOfTheMagi>(_ai));
    RegisterAction("supernova", std::make_unique<CastSupernova>(_ai));
    RegisterAction("evocation", std::make_unique<CastEvocation>(_ai));
}

void PlayerbotArcaneMageStrategy::RegisterArcaneTriggers()
{
    // TODO: Implement when trigger system is available
}

float PlayerbotArcaneMageStrategy::GetActionPriority(std::string const& actionName) const
{
    uint32 charges = GetArcaneCharges();
    bool inBurn = IsInBurnPhase();
    bool inConserve = IsInConservePhase();
    
    // Emergency and utility (highest priority)
    if (actionName == "evocation" && ShouldEvocate()) return 1.0f;
    if (actionName == "counterspell" && ShouldUseCounterspell()) return 0.95f;
    
    // Burst cooldowns
    if (actionName == "arcane power" && ShouldUseArcanePower()) return 0.9f;
    if (actionName == "touch of the magi" && ShouldUseTouchOfTheMagi()) return 0.85f;
    
    // Core rotation - Burn Phase
    if (inBurn)
    {
        if (actionName == "arcane blast" && charges < MAX_ARCANE_CHARGES) return 0.8f;
        if (actionName == "arcane barrage" && charges >= OPTIMAL_CHARGE_THRESHOLD) return 0.75f;
    }
    
    // Core rotation - Conserve Phase  
    if (inConserve)
    {
        if (actionName == "arcane barrage" && charges >= 2) return 0.7f;
        if (actionName == "arcane blast" && charges == 0) return 0.65f;
    }
    
    // AoE situations
    if (IsInAoESituation())
    {
        if (actionName == "arcane orb") return 0.6f;
        if (actionName == "arcane explosion") return 0.55f;
    }
    
    // Default filler
    if (actionName == "arcane blast") return 0.5f;
    
    return PlayerbotMageStrategy::GetActionPriority(actionName);
}

bool PlayerbotArcaneMageStrategy::ShouldUseArcaneBlast() const
{
    return CanAffordArcaneBlast() && (ShouldBuildCharges() || IsInBurnPhase());
}

bool PlayerbotArcaneMageStrategy::ShouldUseArcaneBarrage() const
{
    return ShouldSpendCharges() || GetManaPercent() < 15;
}

bool PlayerbotArcaneMageStrategy::ShouldUseArcaneMissiles() const
{
    Player* bot = _ai->GetBot();
    // Use on proc or when moving
    return bot && (bot->HasAura(79683) || !bot->HasUnitState(UNIT_STATE_CASTING));
}

bool PlayerbotArcaneMageStrategy::ShouldUseArcaneOrb() const
{
    return IsInAoESituation() && GetManaPercent() > 30;
}

bool PlayerbotArcaneMageStrategy::ShouldUseArcaneExplosion() const
{
    return IsInAoESituation() && CountNearbyEnemies(8.0f) >= 4;
}

bool PlayerbotArcaneMageStrategy::ShouldUsePresenceOfMind() const
{
    Player* bot = _ai->GetBot();
    return bot && IsInBurnPhase() && 
           !bot->GetSpellHistory()->HasCooldown(MageSpells::PRESENCE_OF_MIND);
}

bool PlayerbotArcaneMageStrategy::ShouldUseArcanePower() const
{
    Player* bot = _ai->GetBot();
    return bot && ShouldUseBurstCooldowns() && GetManaPercent() > 60 &&
           !bot->GetSpellHistory()->HasCooldown(MageSpells::ARCANE_POWER);
}

bool PlayerbotArcaneMageStrategy::ShouldUseTouchOfTheMagi() const
{
    Player* bot = _ai->GetBot();
    Unit* target = FindBestTarget();
    return bot && target && ShouldUseBurstCooldowns() &&
           !bot->GetSpellHistory()->HasCooldown(MageSpells::TOUCH_OF_THE_MAGI);
}

bool PlayerbotArcaneMageStrategy::ShouldUseSupernova() const
{
    Player* bot = _ai->GetBot();
    return bot && CountNearbyEnemies(8.0f) >= 2 &&
           !bot->GetSpellHistory()->HasCooldown(MageSpells::SUPERNOVA);
}

uint32 PlayerbotArcaneMageStrategy::GetArcaneCharges() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0;
        
    if (Aura* aura = bot->GetAura(36032)) // Arcane Charges aura
        return aura->GetStackAmount();
        
    return 0;
}

bool PlayerbotArcaneMageStrategy::HasMaxArcaneCharges() const
{
    return GetArcaneCharges() >= MAX_ARCANE_CHARGES;
}

bool PlayerbotArcaneMageStrategy::ShouldBuildCharges() const
{
    return GetArcaneCharges() < MAX_ARCANE_CHARGES && GetManaPercent() > 30;
}

bool PlayerbotArcaneMageStrategy::ShouldSpendCharges() const
{
    return GetArcaneCharges() >= OPTIMAL_CHARGE_THRESHOLD || GetManaPercent() < 25;
}

bool PlayerbotArcaneMageStrategy::IsInBurnPhase() const
{
    return GetManaPercent() > MANA_BURN_THRESHOLD;
}

bool PlayerbotArcaneMageStrategy::IsInConservePhase() const
{
    return GetManaPercent() < MANA_CONSERVE_THRESHOLD;
}

bool PlayerbotArcaneMageStrategy::ShouldUseBurstCooldowns() const
{
    Unit* target = FindBestTarget();
    return target && target->GetHealthPct() > BURST_COOLDOWN_HEALTH;
}

bool PlayerbotArcaneMageStrategy::IsInAoESituation() const
{
    return CountNearbyEnemies(8.0f) >= AOE_TARGET_COUNT;
}

bool PlayerbotArcaneMageStrategy::ShouldUseManaEfficiently() const
{
    return GetManaPercent() < 50;
}

bool PlayerbotArcaneMageStrategy::CanAffordArcaneBlast() const
{
    uint32 charges = GetArcaneCharges();
    uint32 baseCost = 250 + (charges * 75); // Increasing cost per charge
    return GetManaAmount() >= baseCost;
}

float PlayerbotArcaneMageStrategy::GetOptimalChargeCount() const
{
    if (IsInBurnPhase())
        return MAX_ARCANE_CHARGES;
    if (IsInConservePhase())
        return 1.0f;
    return OPTIMAL_CHARGE_THRESHOLD;
}

// ===== FIRE MAGE STRATEGY =====

PlayerbotFireMageStrategy::PlayerbotFireMageStrategy(PlayerbotPlayerAI* ai)
    : PlayerbotMageStrategy(ai, "fire") {}

void PlayerbotFireMageStrategy::InitializeActions()
{
    PlayerbotMageStrategy::InitializeActions();
    RegisterFireActions();
}

void PlayerbotFireMageStrategy::InitializeTriggers()
{
    PlayerbotMageStrategy::InitializeTriggers();
    RegisterFireTriggers();
}

void PlayerbotFireMageStrategy::RegisterFireActions()
{
    RegisterAction("fireball", std::make_unique<CastFireball>(_ai));
    RegisterAction("pyroblast", std::make_unique<CastPyroblast>(_ai));
    RegisterAction("fire blast", std::make_unique<CastFireBlast>(_ai));
    RegisterAction("phoenix flames", std::make_unique<CastPhoenixFlames>(_ai));
    RegisterAction("scorch", std::make_unique<CastScorch>(_ai));
    RegisterAction("flamestrike", std::make_unique<CastFlamestrike>(_ai));
    RegisterAction("combustion", std::make_unique<CastCombustion>(_ai));
    RegisterAction("meteor", std::make_unique<CastMeteor>(_ai));
    RegisterAction("dragon breath", std::make_unique<CastDragonBreath>(_ai));
    RegisterAction("living bomb", std::make_unique<CastLivingBomb>(_ai));
    RegisterAction("blazing barrier", std::make_unique<CastBlazingBarrier>(_ai));
}

void PlayerbotFireMageStrategy::RegisterFireTriggers()
{
    // TODO: Implement when trigger system is available
}

float PlayerbotFireMageStrategy::GetActionPriority(std::string const& actionName) const
{
    bool hasHotStreak = HasHotStreak();
    bool hasHeatingUp = HasHeatingUp();
    bool inCombustion = IsInCombustion();
    
    // Emergency and utility
    if (actionName == "blazing barrier" && NeedsMageArmor()) return 1.0f;
    if (actionName == "counterspell" && ShouldUseCounterspell()) return 0.95f;
    
    // Burst cooldowns
    if (actionName == "combustion" && ShouldUseCombustion()) return 0.9f;
    if (actionName == "meteor" && ShouldUseMeteor()) return 0.85f;
    
    // Hot Streak usage (highest DPS priority)
    if (actionName == "pyroblast" && hasHotStreak) return 0.8f;
    
    // Heating Up conversion
    if (actionName == "fire blast" && hasHeatingUp && HasPhoenixFlamesCharges()) return 0.75f;
    if (actionName == "phoenix flames" && hasHeatingUp) return 0.7f;
    
    // AoE situations
    if (ShouldUseAoERotation())
    {
        if (actionName == "flamestrike") return 0.65f;
        if (actionName == "dragon breath") return 0.6f;
    }
    
    // Single target rotation
    if (actionName == "scorch" && inCombustion) return 0.55f; // Fast cast during Combustion
    if (actionName == "fireball") return 0.5f; // Standard filler
    
    return PlayerbotMageStrategy::GetActionPriority(actionName);
}

bool PlayerbotFireMageStrategy::ShouldUseFireball() const
{
    return !HasHotStreak() && GetManaPercent() > 20;
}

bool PlayerbotFireMageStrategy::ShouldUsePyroblast() const
{
    return HasHotStreak() || IsInCombustion();
}

bool PlayerbotFireMageStrategy::ShouldUseFireBlast() const
{
    Player* bot = _ai->GetBot();
    return bot && (HasHeatingUp() || IsInCombustion()) && 
           !bot->GetSpellHistory()->HasCooldown(MageSpells::FIRE_BLAST);
}

bool PlayerbotFireMageStrategy::ShouldUsePhoenixFlames() const
{
    return HasPhoenixFlamesCharges() && (HasHeatingUp() || IsInCombustion());
}

bool PlayerbotFireMageStrategy::ShouldUseScorch() const
{
    Player* bot = _ai->GetBot();
    // Use Scorch when moving or during Combustion for fast casts
    return IsInCombustion() || (bot && !bot->HasUnitState(UNIT_STATE_CASTING));
}

bool PlayerbotFireMageStrategy::ShouldUseFlamestrike() const
{
    return ShouldUseAoERotation() && CountNearbyEnemies(8.0f) >= 3;
}

bool PlayerbotFireMageStrategy::ShouldUseCombustion() const
{
    Player* bot = _ai->GetBot();
    return bot && ShouldUseBurstCooldowns() && 
           !bot->GetSpellHistory()->HasCooldown(MageSpells::COMBUSTION);
}

bool PlayerbotFireMageStrategy::ShouldUseMeteor() const
{
    Player* bot = _ai->GetBot();
    return bot && ShouldUseBurstCooldowns() &&
           !bot->GetSpellHistory()->HasCooldown(MageSpells::METEOR);
}

bool PlayerbotFireMageStrategy::ShouldUseDragonBreath() const
{
    return CountNearbyEnemies(8.0f) >= 2;
}

bool PlayerbotFireMageStrategy::ShouldUseLivingBomb() const
{
    Unit* target = FindBestTarget();
    return target && !target->HasAura(MageSpells::LIVING_BOMB) && ShouldUseAoERotation();
}

bool PlayerbotFireMageStrategy::HasHotStreak() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(MageSpells::HOT_STREAK);
}

bool PlayerbotFireMageStrategy::HasHeatingUp() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(MageSpells::HEATING_UP);
}

bool PlayerbotFireMageStrategy::IsInCombustion() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(MageSpells::COMBUSTION);
}

bool PlayerbotFireMageStrategy::ShouldUseBurstCooldowns() const
{
    Unit* target = FindBestTarget();
    return target && target->GetHealthPct() > BURST_COOLDOWN_HEALTH;
}

bool PlayerbotFireMageStrategy::ShouldUseAoERotation() const
{
    return CountNearbyEnemies(8.0f) >= AOE_TARGET_COUNT;
}

bool PlayerbotFireMageStrategy::HasIgniteOnTarget(Unit* target) const
{
    return target && target->HasAura(MageSpells::IGNITE);
}

bool PlayerbotFireMageStrategy::ShouldRefreshIgnite(Unit* target) const
{
    if (!target)
        return false;
        
    if (Aura* aura = target->GetAura(MageSpells::IGNITE))
        return float(aura->GetDuration()) / 1000.0f < IGNITE_REFRESH_THRESHOLD;
        
    return true;
}

bool PlayerbotFireMageStrategy::ShouldUseInstantPyroblast() const
{
    return HasHotStreak();
}

bool PlayerbotFireMageStrategy::ShouldGenerateHotStreak() const
{
    return !HasHotStreak() && (HasHeatingUp() || IsInCombustion());
}

uint32 PlayerbotFireMageStrategy::GetPhoenixFlamesCharges() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0;
        
    // Check Phoenix Flames charges (spell charge system)
    return bot->GetSpellHistory()->GetChargeRecoveryTime(MageSpells::PHOENIX_FLAMES) > 0 ? 0 : 1;
}

bool PlayerbotFireMageStrategy::HasPhoenixFlamesCharges() const
{
    return GetPhoenixFlamesCharges() > 0;
}

bool PlayerbotFireMageStrategy::ShouldPrioritizeIgniteTarget() const
{
    Unit* target = FindBestTarget();
    return target && HasIgniteOnTarget(target) && !ShouldRefreshIgnite(target);
}

Unit* PlayerbotFireMageStrategy::FindBestIgniteTarget() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return nullptr;
    
    std::list<Unit*> targets;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck check(bot, bot, 30.0f);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(bot, targets, check);
    Cell::VisitAllObjects(bot, searcher, 30.0f);
    
    // Prioritize targets with Ignite that needs refreshing
    for (Unit* target : targets)
    {
        if (target->IsAlive() && HasIgniteOnTarget(target) && ShouldRefreshIgnite(target))
            return target;
    }
    
    return FindBestTarget();
}

// ===== FROST MAGE STRATEGY =====

PlayerbotFrostMageStrategy::PlayerbotFrostMageStrategy(PlayerbotPlayerAI* ai)
    : PlayerbotMageStrategy(ai, "frost") {}

void PlayerbotFrostMageStrategy::InitializeActions()
{
    PlayerbotMageStrategy::InitializeActions();
    RegisterFrostActions();
}

void PlayerbotFrostMageStrategy::InitializeTriggers()
{
    PlayerbotMageStrategy::InitializeTriggers();
    RegisterFrostTriggers();
}

void PlayerbotFrostMageStrategy::RegisterFrostActions()
{
    RegisterAction("frostbolt", std::make_unique<CastFrostbolt>(_ai));
    RegisterAction("ice lance", std::make_unique<CastIceLance>(_ai));
    RegisterAction("flurry", std::make_unique<CastFlurry>(_ai));
    RegisterAction("frozen orb", std::make_unique<CastFrozenOrb>(_ai));
    RegisterAction("comet storm", std::make_unique<CastCometStorm>(_ai));
    RegisterAction("glacial spike", std::make_unique<CastGlacialSpike>(_ai));
    RegisterAction("ray of frost", std::make_unique<CastRayOfFrost>(_ai));
    RegisterAction("blizzard", std::make_unique<CastBlizzard>(_ai));
    RegisterAction("frost nova", std::make_unique<CastFrostNova>(_ai));
    RegisterAction("cone of cold", std::make_unique<CastConeOfCold>(_ai));
    RegisterAction("ice barrier", std::make_unique<CastIceBarrier>(_ai));
}

void PlayerbotFrostMageStrategy::RegisterFrostTriggers()
{
    // TODO: Implement when trigger system is available
}

float PlayerbotFrostMageStrategy::GetActionPriority(std::string const& actionName) const
{
    bool hasBrainFreeze = HasBrainFreeze();
    bool hasFingers = HasFingersOfFrost();
    Unit* target = FindBestTarget();
    bool hasWintersChill = target && HasWintersChillOnTarget(target);
    
    // Emergency and utility
    if (actionName == "ice barrier" && NeedsMageArmor()) return 1.0f;
    if (actionName == "frost nova" && ShouldUseCrowdControl()) return 0.95f;
    if (actionName == "counterspell" && ShouldUseCounterspell()) return 0.9f;
    
    // Burst cooldowns
    if (actionName == "frozen orb" && ShouldUseFrozenOrb()) return 0.85f;
    if (actionName == "comet storm" && ShouldUseCometStorm()) return 0.8f;
    if (actionName == "ray of frost" && ShouldUseRayOfFrost()) return 0.75f;
    
    // Shatter combo priority
    if (actionName == "ice lance" && (hasFingers || hasWintersChill)) return 0.7f;
    if (actionName == "flurry" && hasBrainFreeze) return 0.65f;
    if (actionName == "glacial spike" && ShouldUseGlacialSpike()) return 0.6f;
    
    // AoE situations
    if (ShouldUseAoERotation())
    {
        if (actionName == "blizzard") return 0.55f;
        if (actionName == "cone of cold") return 0.5f;
    }
    
    // Standard rotation
    if (actionName == "frostbolt") return 0.45f; // Main filler
    
    return PlayerbotMageStrategy::GetActionPriority(actionName);
}

bool PlayerbotFrostMageStrategy::ShouldUseFrostbolt() const
{
    return !HasBrainFreeze() && !HasFingersOfFrost() && GetManaPercent() > 15;
}

bool PlayerbotFrostMageStrategy::ShouldUseIceLance() const
{
    Unit* target = FindBestTarget();
    return HasFingersOfFrost() || (target && IsTargetFrozen(target)) ||
           (target && HasWintersChillOnTarget(target));
}

bool PlayerbotFrostMageStrategy::ShouldUseFlurry() const
{
    return HasBrainFreeze() && !ShouldConserveFingersOfFrost();
}

bool PlayerbotFrostMageStrategy::ShouldUseFrozenOrb() const
{
    Player* bot = _ai->GetBot();
    return bot && (CountNearbyEnemies(15.0f) >= 2 || ShouldUseBurstCooldowns()) &&
           !bot->GetSpellHistory()->HasCooldown(MageSpells::FROZEN_ORB);
}

bool PlayerbotFrostMageStrategy::ShouldUseCometStorm() const
{
    Player* bot = _ai->GetBot();
    return bot && ShouldUseBurstCooldowns() &&
           !bot->GetSpellHistory()->HasCooldown(MageSpells::COMET_STORM);
}

bool PlayerbotFrostMageStrategy::ShouldUseGlacialSpike() const
{
    Player* bot = _ai->GetBot();
    Unit* target = FindBestTarget();
    // Use when target is frozen or has Winter's Chill
    return bot && target && (IsTargetFrozen(target) || HasWintersChillOnTarget(target)) &&
           !bot->GetSpellHistory()->HasCooldown(MageSpells::GLACIAL_SPIKE);
}

bool PlayerbotFrostMageStrategy::ShouldUseRayOfFrost() const
{
    Player* bot = _ai->GetBot();
    return bot && ShouldUseBurstCooldowns() &&
           !bot->GetSpellHistory()->HasCooldown(MageSpells::RAY_OF_FROST);
}

bool PlayerbotFrostMageStrategy::ShouldUseBlizzard() const
{
    return ShouldUseAoERotation() && CountNearbyEnemies(8.0f) >= 4;
}

bool PlayerbotFrostMageStrategy::ShouldUseFrostNova() const
{
    return CountNearbyEnemies(8.0f) >= 2 && ShouldUseCrowdControl();
}

bool PlayerbotFrostMageStrategy::ShouldUseConeOfCold() const
{
    return CountNearbyEnemies(8.0f) >= 2;
}

bool PlayerbotFrostMageStrategy::HasBrainFreeze() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(MageSpells::BRAIN_FREEZE);
}

bool PlayerbotFrostMageStrategy::HasFingersOfFrost() const
{
    Player* bot = _ai->GetBot();
    return bot && bot->HasAura(MageSpells::FINGERS_OF_FROST);
}

bool PlayerbotFrostMageStrategy::HasWintersChillOnTarget(Unit* target) const
{
    return target && target->HasAura(MageSpells::WINTERS_CHILL);
}

bool PlayerbotFrostMageStrategy::IsTargetFrozen(Unit* target) const
{
    if (!target)
        return false;
        
    return target->HasAuraType(SPELL_AURA_MOD_ROOT) || 
           target->HasAuraType(SPELL_AURA_MOD_STUN) ||
           target->HasAura(MageSpells::FROST_NOVA) ||
           target->HasAura(MageSpells::DEEP_FREEZE);
}

bool PlayerbotFrostMageStrategy::ShouldUseShatterCombo() const
{
    Unit* target = FindBestTarget();
    return target && (HasBrainFreeze() || HasFingersOfFrost());
}

bool PlayerbotFrostMageStrategy::ShouldUseBurstCooldowns() const
{
    Unit* target = FindBestTarget();
    return target && target->GetHealthPct() > BURST_COOLDOWN_HEALTH;
}

bool PlayerbotFrostMageStrategy::ShouldUseAoERotation() const
{
    return CountNearbyEnemies(8.0f) >= AOE_TARGET_COUNT;
}

bool PlayerbotFrostMageStrategy::ShouldUseInstantFlurry() const
{
    return HasBrainFreeze();
}

bool PlayerbotFrostMageStrategy::ShouldUseInstantIceLance() const
{
    Unit* target = FindBestTarget();
    return HasFingersOfFrost() || (target && HasWintersChillOnTarget(target));
}

uint32 PlayerbotFrostMageStrategy::GetFingersOfFrostStacks() const
{
    Player* bot = _ai->GetBot();
    if (!bot)
        return 0;
        
    if (Aura* aura = bot->GetAura(MageSpells::FINGERS_OF_FROST))
        return aura->GetStackAmount();
        
    return 0;
}

bool PlayerbotFrostMageStrategy::ShouldConserveFingersOfFrost() const
{
    return GetFingersOfFrostStacks() == 1 && !HasBrainFreeze();
}

bool PlayerbotFrostMageStrategy::ShouldUseMovementAbility() const
{
    return ShouldUseBlink() || NeedsToKite();
}

bool PlayerbotFrostMageStrategy::NeedsToKite() const
{
    Player* bot = _ai->GetBot();
    Unit* target = FindBestTarget();
    return bot && target && bot->GetDistance(target) < KITING_DISTANCE && 
           target->GetVictim() == bot;
}

bool PlayerbotFrostMageStrategy::ShouldUseCrowdControl() const
{
    Player* bot = _ai->GetBot();
    return bot && (bot->GetHealthPct() < 50.0f || CountNearbyEnemies(8.0f) >= 3);
}