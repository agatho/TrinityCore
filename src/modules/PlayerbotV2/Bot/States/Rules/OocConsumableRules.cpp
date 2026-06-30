// OocConsumableRules - Refactor #3 pass 14. Migrates the out-of-combat
// recovery + auto-vote + auto-equip rules out of State_Idle's inline tail:
//   - idle:cannibalize        (Undead racial HP regen from a nearby corpse)
//   - idle:ooc_potion         (emergency HP potion when food/bandage skipped)
//   - idle:ooc_eat_food       (sit-eat-drink between pulls)
//   - idle:ooc_bandage        (self-bandage when low HP and no food works)
//   - idle:loot_roll          (auto-vote on group loot rolls; reports
//                              loot_roll / loot_roll_quest / loot_roll_de)
//   - idle:equip_upgrade      (stat-priority + ilvl scored equip swap)
// Priorities sit just below the high-pri preemption band (600..670) so
// they fire from the bottom-of-tick dispatch in priority order, after
// the still-inline MaintainOoc* family.

#include "Bot/IdleRule.h"
#include "Group/GroupSnapshot.h"
#include "Bot/BotAI.h"
#include "Bot/BotSnapshotView.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotIntent.h"
#include "SharedDefines.h"
#include "RaceMask.h"        // RACE_UNDEAD_PLAYER
#include "GameTime.h"

#include <array>

namespace Playerbot {

namespace {

// Per-bot HP / mana thresholds for OOC consumable use; duplicated from
// State_Idle.cpp::GetIdleConsumeThresholds (anonymous-namespace helper
// in the legacy TU). Cheap and small — keeping a copy avoids exposing
// the helper publicly.
struct IdleConsumeThresholds
{
    int32 food_floor;
    int32 bandage_below;
    int32 potion_below;
};

IdleConsumeThresholds GetIdleConsumeThresholds(BotAI const& ai)
{
    switch (ai.personality().risk_tolerance)
    {
        case RiskTolerance::Cautious: return {90, 70, 50};
        case RiskTolerance::Careful:  return {75, 60, 40};
        case RiskTolerance::Normal:   return {60, 50, 30};
        case RiskTolerance::Reckless: return {40, 30, 15};
    }
    return {60, 50, 30};
}

// Item score for equip-upgrade ranking; duplicated from State_Idle.cpp's
// anonymous-namespace ScoreItem helper (single arithmetic expression —
// duplication is cheaper than exposing a header).
inline float ScoreItem(ItemStatBlock const& stats,
                       uint16 item_level,
                       std::array<float, 12> const& weights,
                       float dps_weight)
{
    float score = static_cast<float>(item_level);
    for (size_t i = 0; i < weights.size(); ++i)
        score += static_cast<float>(stats.stats[i]) * weights[i];
    score += static_cast<float>(stats.weapon_dps_x10) * dps_weight;
    return score;
}

// ---------- idle:cannibalize ----------
bool CannibalizeGate(BotSnapshotView const& s, BotAI&, GroupSnapshotView const&, uint32)
{
    if (s.cls() == 0) return false;
    if (s.race() != RACE_UNDEAD_PLAYER) return false;
    if (s.in_combat() || s.is_casting()) return false;
    // Combat-flicker gate: Cannibalize is a 10s channel on a corpse,
    // breaks on damage. Don't start it in the brief single-tick OOC
    // window between trash pulls. 5s matches sibling food/bandage gates.
    if (s.recently_in_combat(5000)) return false;
    if (s.hp_pct() >= 70) return false;
    constexpr uint32 CANNIBALIZE = 20577;
    return s.knows_spell(CANNIBALIZE) && s.is_ready(CANNIBALIZE);
}

bool CannibalizeFire(BotSnapshotView const& s, BotAI& ai,
                     GroupSnapshotView const&,
                     BotIntentEmitter& emit, uint32)
{
    constexpr uint32 CANNIBALIZE = 20577;
    float bx, by, bz; s.position(bx, by, bz);
    for (auto const& u : s.raw().combat.nearby_enemies)
    {
        if (u.hp > 0) continue;
        const float dx = u.x - bx, dy = u.y - by, dz = u.z - bz;
        if (dx*dx + dy*dy + dz*dz <= 5.0f * 5.0f)
        {
            if (emit.cast(CANNIBALIZE))
            {
                ai.set_last_rule_fired("idle:cannibalize");
                return true;
            }
        }
    }
    return false;
}

// ---------- idle:ooc_potion ----------
bool OocPotionGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, uint32)
{
    if (s.in_combat() || s.is_casting()) return false;
    if (ai.effective_role(s) == Role::Healer) return false;
    const auto thr = GetIdleConsumeThresholds(ai);
    return s.hp_pct() < thr.potion_below;
}

bool OocPotionFire(BotSnapshotView const& s, BotAI& ai,
                   GroupSnapshotView const&,
                   BotIntentEmitter& emit, uint32)
{
    constexpr uint8 ITEM_CLASS_CONSUMABLE_LOCAL = 0;
    constexpr uint8 ITEM_SUBCLASS_POTION_LOCAL = 1;
    uint32 best_potion = 0;
    for (auto const& it : s.raw().inventory.bag_items)
    {
        if (it.item_class != ITEM_CLASS_CONSUMABLE_LOCAL) continue;
        if (it.item_subclass != ITEM_SUBCLASS_POTION_LOCAL) continue;
        if (it.entry > best_potion) best_potion = it.entry;
    }
    if (!best_potion) return false;
    const uint32 pot_now_ms = GameTime::GetGameTimeMS();
    if (ai.action_recently_tried(BotAI::ActionKind::OocPotion,
                                  uint64(best_potion), pot_now_ms))
        return false;
    emit.emit(UseItemByEntryIntent{best_potion, s.raw().guid});
    ai.note_action_retry(BotAI::ActionKind::OocPotion,
                         uint64(best_potion), pot_now_ms);
    ai.set_last_rule_fired("idle:ooc_potion");
    return true;
}

// ---------- idle:ic_potion (emergency in-combat) ----------
// Real players chug a Healing Potion when low-HP in active combat
// — the OocPotion rule above only fires when OUT of combat, so
// bots solo-questing through a tough pull eat damage past dangerous
// thresholds without consuming the potion right in their bag.
//
// Gates:
//   - in_combat (this is the whole point)
//   - HP <= 25% (emergency threshold — well below the 35-45% Ooc
//     trigger so we don't burn the per-fight potion CD on normal
//     mid-fight dips)
//   - has potion in bag
//   - not on action-retry lockout (5min default keyed by item entry;
//     prevents re-emit during the server-side Potion Sickness debuff)
//
// Healers exempt — they have their own emergency heals (PW: Life,
// Renewal, Holy Word: Salvation etc.) that scale better than a
// fixed potion charge.
bool IcPotionGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, uint32)
{
    if (!s.in_combat()) return false;
    if (!s.is_alive()) return false;
    if (ai.effective_role(s) == Role::Healer) return false;
    // Fixed 25% threshold — risk-tolerance doesn't drive this. The
    // potion is one-per-fight; reserve it for genuine emergencies.
    if (s.hp_pct() >= 25 || s.hp_pct() <= 0) return false;
    return true;
}

bool IcPotionFire(BotSnapshotView const& s, BotAI& ai,
                  GroupSnapshotView const&,
                  BotIntentEmitter& emit, uint32)
{
    constexpr uint8 ITEM_CLASS_CONSUMABLE_LOCAL = 0;
    constexpr uint8 ITEM_SUBCLASS_POTION_LOCAL = 1;
    uint32 best_potion = 0;
    for (auto const& it : s.raw().inventory.bag_items)
    {
        if (it.item_class != ITEM_CLASS_CONSUMABLE_LOCAL) continue;
        if (it.item_subclass != ITEM_SUBCLASS_POTION_LOCAL) continue;
        if (it.entry > best_potion) best_potion = it.entry;
    }
    if (!best_potion) return false;
    const uint32 pot_now_ms = GameTime::GetGameTimeMS();
    // Share lockout with OocPotion — Potion Sickness debuff means
    // we can only use one per fight regardless of which rule fired.
    if (ai.action_recently_tried(BotAI::ActionKind::OocPotion,
                                  uint64(best_potion), pot_now_ms))
        return false;
    emit.emit(UseItemByEntryIntent{best_potion, s.raw().guid});
    ai.note_action_retry(BotAI::ActionKind::OocPotion,
                         uint64(best_potion), pot_now_ms);
    ai.set_last_rule_fired("idle:ic_potion");
    return true;
}

// ---------- idle:ooc_eat_food ----------
bool OocEatFoodGate(BotSnapshotView const& s, BotAI& ai,
                    GroupSnapshotView const& g, uint32)
{
    if (s.in_combat() || s.is_casting()) return false;
    // Combat-flicker gate (see MaintainOocFood for full rationale).
    // Eating/drinking applies a Sit aura; first hit cancels it and
    // wastes the consumable.
    if (s.recently_in_combat(5000)) return false;
    if (s.raw().movement.is_moving || s.raw().movement.is_mounted) return false;
    if (s.is_encounter_in_progress()) return false;
    if (ai.dungeon_active()) return false;

    const bool caster_class =
        s.raw().identity.cls == 5 || s.raw().identity.cls == 8 || s.raw().identity.cls == 9 ||
        s.raw().identity.cls == 11 || s.raw().identity.cls == 7 || s.raw().identity.cls == 13;
    const int32 mana = caster_class ? s.power_pct(0) : 100;
    const bool need_food  = s.hp_pct() < 90;
    const bool need_drink = caster_class && mana < 70;
    if (!need_food && !need_drink) return false;

    if (g.exists())
    {
        if (auto const* mems_food = g.members())
            for (auto const& m : *mems_food)
                if (m.online && m.in_combat)
                    return false;
    }
    float bxe, bye, bze; s.position(bxe, bye, bze);
    for (auto const& u : s.raw().combat.nearby_enemies)
    {
        if (u.hp <= 0) continue;
        const float dx = u.x - bxe, dy = u.y - bye;
        if (dx * dx + dy * dy < 30.0f * 30.0f)
            return false;
    }
    return true;
}

bool OocEatFoodFire(BotSnapshotView const& s, BotAI& ai,
                    GroupSnapshotView const&,
                    BotIntentEmitter& emit, uint32)
{
    constexpr uint8 ITEM_CLASS_CONSUMABLE_LOCAL = 0;
    constexpr uint8 ITEM_SUBCLASS_FOOD_LOCAL    = 5;
    constexpr uint8 ITEM_SUBCLASS_DRINK_LOCAL   = 6;
    uint32 best_consumable = 0;
    for (auto const& it : s.raw().inventory.bag_items)
    {
        if (it.item_class != ITEM_CLASS_CONSUMABLE_LOCAL) continue;
        if (it.item_subclass != ITEM_SUBCLASS_FOOD_LOCAL &&
            it.item_subclass != ITEM_SUBCLASS_DRINK_LOCAL) continue;
        if (it.entry > best_consumable) best_consumable = it.entry;
    }
    if (!best_consumable) return false;
    const uint32 food_now_ms = GameTime::GetGameTimeMS();
    if (ai.action_recently_tried(BotAI::ActionKind::OocFood,
                                  uint64(best_consumable), food_now_ms))
        return false;
    emit.emit(UseItemByEntryIntent{best_consumable, s.raw().guid});
    ai.note_action_retry(BotAI::ActionKind::OocFood,
                         uint64(best_consumable), food_now_ms);
    ai.set_last_rule_fired("idle:ooc_eat_food");
    return true;
}

// ---------- idle:ooc_bandage ----------
bool OocBandageGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, uint32)
{
    if (s.in_combat() || s.is_casting()) return false;
    // Combat-flicker gate: bandage is 6-8s channel, breaks on damage.
    // 5s matches sibling food/cannibalize/conjure gates.
    if (s.recently_in_combat(5000)) return false;
    if (s.hp_pct() >= 50) return false;
    return ai.effective_role(s) != Role::Healer;
}

bool OocBandageFire(BotSnapshotView const& s, BotAI& ai,
                    GroupSnapshotView const&,
                    BotIntentEmitter& emit, uint32)
{
    constexpr uint8 ITEM_CLASS_CONSUMABLE_LOCAL = 0;
    constexpr uint8 ITEM_SUBCLASS_BANDAGE_LOCAL = 7;
    uint32 best_bandage = 0;
    for (auto const& it : s.raw().inventory.bag_items)
    {
        if (it.item_class != ITEM_CLASS_CONSUMABLE_LOCAL) continue;
        if (it.item_subclass != ITEM_SUBCLASS_BANDAGE_LOCAL) continue;
        if (it.entry > best_bandage) best_bandage = it.entry;
    }
    if (!best_bandage) return false;
    const uint32 ban_now_ms = GameTime::GetGameTimeMS();
    if (ai.action_recently_tried(BotAI::ActionKind::OocBandage,
                                  uint64(best_bandage), ban_now_ms))
        return false;
    emit.emit(UseItemByEntryIntent{best_bandage, s.raw().guid});
    ai.note_action_retry(BotAI::ActionKind::OocBandage,
                         uint64(best_bandage), ban_now_ms);
    ai.set_last_rule_fired("idle:ooc_bandage");
    return true;
}

// ---------- idle:loot_roll ----------
bool LootRollGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, uint32)
{
    if (s.raw().loot.loot_rolls.empty())
    {
        // Roll-list drained — reset the decide-at slot so the next batch
        // (e.g. the next boss kill) gets a fresh 1–4s hesitation.
        ai.set_next_loot_roll_fire_ms(0);
        return false;
    }
    const uint32 now_ms = s.published_at_ms();
    uint32 fire_at = ai.next_loot_roll_fire_ms();
    if (fire_at == 0)
    {
        // First tick we see a roll waiting — schedule the vote for
        // 800–4500ms from now, jittered per-bot so a raid doesn't roll
        // in lockstep. ~3.7s spread covers the entire "looking at the
        // popup" window humans show.
        const uint32 jitter = 800u + (uint32(s.bot_id()) * 2246822519u) % 3700u;
        ai.set_next_loot_roll_fire_ms(now_ms + jitter);
        return false;
    }
    return now_ms >= fire_at;
}

bool LootRollFire(BotSnapshotView const& s, BotAI& ai,
                  GroupSnapshotView const&,
                  BotIntentEmitter& emit, uint32)
{
    for (auto const& r : s.raw().loot.loot_rolls)
    {
        uint8 vote = 0;
        char const* rule_name = "idle:loot_roll";
        if (r.is_quest_item && (r.vote_mask & 0x02) != 0)
        {
            vote = 1;
            rule_name = "idle:loot_roll_quest";
        }
        else if (r.is_upgrade && (r.vote_mask & 0x02) != 0)
        {
            vote = 1;
        }
        // L-P1b: only Disenchant when the bot actually has Enchanting
        // (SKILL_ENCHANTING = 333). Without the skill a DE vote is wasted
        // (the item vanishes and the bot gets nothing usable), so fall
        // through to GREED instead.
        else if ((r.vote_mask & 0x08) != 0 && s.has_skill(333))
        {
            vote = 3;
            rule_name = "idle:loot_roll_de";
        }
        else if ((r.vote_mask & 0x04) != 0) vote = 2;  // GREED
        // L-P3a: TRANSMOG (0x10) — bots don't collect appearances; treat
        // as GREED if offered alongside greed, else PASS below.
        else if ((r.vote_mask & 0x10) != 0)
        {
            vote = 0;  // PASS on transmog-only rolls
            rule_name = "idle:loot_roll_pass";
        }
        else if ((r.vote_mask & 0x01) != 0) vote = 0;
        // L-P3a: final PASS fallback so every roll resolves even when only
        // an unhandled/unexpected vote bit is set. An unresolved roll
        // would otherwise hang the loot UI until it times out.
        else
        {
            vote = 0;
            rule_name = "idle:loot_roll_pass";
        }
        emit.loot_roll(r.loot_object, r.loot_list_id, vote);
        ai.set_last_rule_fired(rule_name);
        // Inter-roll spacing: after firing one vote, schedule the next
        // one 400–1500ms out so a 3-item boss drop doesn't auto-resolve
        // in two ticks. Re-jitter on bot_id+published_at so consecutive
        // rolls from the same bot don't fall on the same offset.
        const uint32 now_ms = s.published_at_ms();
        const uint32 spacing =
            400u + ((uint32(s.bot_id()) ^ now_ms) * 2654435761u) % 1100u;
        ai.set_next_loot_roll_fire_ms(now_ms + spacing);
        return true;
    }
    return false;
}

// ---------- idle:equip_upgrade ----------
bool EquipUpgradeGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, uint32 now_ms)
{
    if (s.in_combat() || s.is_casting())
    {
        if (ai.pending_equip_upgrade_at_ms() != 0)
            ai.set_pending_equip_upgrade_at_ms(0);
        return false;
    }
    // L-P1c: previously a blanket is_in_dungeon()||in_battleground() block
    // meant bots never equipped upgrades during instanced content. Removed.
    // We now rely on the existing !in_combat && !casting window plus a short
    // post-combat settle so the swap can't land mid-pull (the 1-tick OOC
    // gap between trash packs). recently_in_combat(3000) == true means
    // combat ended within the last 3s — defer until that window passes.
    if (s.recently_in_combat(3000))
    {
        if (ai.pending_equip_upgrade_at_ms() != 0)
            ai.set_pending_equip_upgrade_at_ms(0);
        return false;
    }
    // Quest-first (2026-06-16): a bot with a reachable quest action finishes the
    // quest instead of stopping to swap gear — this gate ignoring quest state was
    // the #1 GoalUnreachable wedge (bot AT its objective running equip_upgrade).
    // FULLY yield: there is NO bag-full carve-out because equipping does not free
    // bag slots (API::equip_item swaps the replaced item back into the bag); the
    // real full-bag blocker is idle:vendor_sell_trash, which is quest-aware.
    if (s.has_actionable_quest())
    {
        if (ai.pending_equip_upgrade_at_ms() != 0)
            ai.set_pending_equip_upgrade_at_ms(0);
        return false;
    }
    // Compare-tooltips hesitation: 2–6s before the first equip click
    // lands. We don't know inside the gate whether there IS an upgrade
    // candidate (that needs the full ScoreItem walk in Fire). So we
    // arm the slot unconditionally on first observation of "able to
    // equip" state and let Fire's own equip_recently_tried per-entry
    // throttle dedup subsequent ticks. Cost: every bot eligible for
    // a swap waits a one-time 2–6s window after entering town; cheap
    // for the realism payoff.
    uint32 ready_at = ai.pending_equip_upgrade_at_ms();
    if (ready_at == 0)
    {
        const uint32 jitter = 2000u + (uint32(s.bot_id()) * 2654435761u) % 4000u;
        ai.set_pending_equip_upgrade_at_ms(now_ms + jitter);
        return false;
    }
    return now_ms >= ready_at;
}

bool EquipUpgradeFire(BotSnapshotView const& s, BotAI& ai,
                     GroupSnapshotView const&,
                     BotIntentEmitter& emit, uint32)
{
    std::array<float, 12> weights = s.spec_stat_weights();
    if (s.in_battleground())
        weights[8] = weights[8] * 1.5f;
    const float dps_w   = s.spec_weapon_dps_weight();
    const uint32 equip_now_ms = GameTime::GetGameTimeMS();
    for (auto const& it : s.raw().inventory.bag_items)
    {
        if (it.equip_slot == 0xFF) continue;
        if (it.item_level == 0) continue;
        if (it.quality == 0) continue;
        if (ai.equip_recently_tried(it.entry, equip_now_ms)) continue;
        EquippedItem const& cur = s.equipped(it.equip_slot);
        const float score_new = ScoreItem(it.stats, it.item_level, weights, dps_w);
        const float score_cur = (cur.entry != 0)
            ? ScoreItem(cur.stats, cur.item_level, weights, dps_w)
            : 0.0f;
        // Swap margin scales with the CURRENT item's score. The old flat
        // max(50, 5%) floor demanded a ~+50-score jump on a scale where
        // leveling-range strict upgrades are worth +5..25 — so low/mid-level
        // bots NEVER equipped anything via this rule and armor piled up in
        // bags (audit C10: L34 healer at item level 7 with 6 pending
        // upgrades). 5% of current score (min 2) keeps swap-churn hysteresis
        // meaningful at endgame scores while letting real leveling upgrades
        // through.
        const float margin = std::max(2.0f, score_cur * 0.05f);
        if (score_new <= score_cur + margin) continue;
        emit.equip_item(it.bag, it.slot, it.equip_slot);
        ai.note_equip_try(it.entry, equip_now_ms);
        ai.set_last_rule_fired("idle:equip_upgrade");
        return true;
    }
    return false;
}

} // anonymous namespace

void RegisterOocConsumableRules(IdleRuleRegistry& r)
{
    {
        IdleRule rule;
        rule.name     = "idle:cannibalize";
        rule.priority = 670;
        rule.gate     = &CannibalizeGate;
        rule.fire     = &CannibalizeFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:ooc_potion";
        rule.priority = 668;
        rule.gate     = &OocPotionGate;
        rule.fire     = &OocPotionFire;
        r.register_rule(std::move(rule));
    }
    // In-combat emergency potion sits HIGHER than survival-tier
    // rules (875) — using a potion at 15% HP is the most life-
    // critical decision the bot makes mid-fight, above interrupt /
    // assist priorities. Shares lockout with idle:ooc_potion so the
    // one-per-fight Potion Sickness debuff is respected.
    {
        IdleRule rule;
        rule.name     = "idle:ic_potion";
        rule.priority = 875;
        rule.gate     = &IcPotionGate;
        rule.fire     = &IcPotionFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:ooc_eat_food";
        rule.priority = 666;
        rule.gate     = &OocEatFoodGate;
        rule.fire     = &OocEatFoodFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:ooc_bandage";
        rule.priority = 664;
        rule.gate     = &OocBandageGate;
        rule.fire     = &OocBandageFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:loot_roll";
        rule.priority = 660;
        rule.gate     = &LootRollGate;
        rule.fire     = &LootRollFire;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:equip_upgrade";
        rule.priority = 600;
        rule.gate     = &EquipUpgradeGate;
        rule.fire     = &EquipUpgradeFire;
        r.register_rule(std::move(rule));
    }
}

} // namespace Playerbot
