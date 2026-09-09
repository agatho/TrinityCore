// MailRules - Refactor #3 pass 3. Migrates the two mailbox-related idle
// rules — `idle:mail_drain` (take money/items from delivered mail with
// data-loss guard + thank whisper) and `idle:mat_share_to_owner` (mail
// surplus trade goods to the bot's registered owner).

#include "Bot/IdleRule.h"
#include "Group/GroupSnapshot.h"
#include "Bot/BotAI.h"
#include "Bot/BotSnapshotView.h"
#include "Bot/BotIntentEmitter.h"
#include "Bot/BotIntent.h"
#include "Bot/BotPersonality.h"
#include "CharacterCache.h"
#include "ObjectGuid.h"
#include "World/WorldMetadata.h"
#include <cfloat>
#include <cmath>

namespace Playerbot {

namespace {

constexpr float kInteractSq = 5.0f * 5.0f;

BotSnapshot::NearbyObject const* InRangeMailbox(BotSnapshotView const& s)
{
    auto const* mb = s.nearest_object_of_type(/*MAILBOX*/ 19);
    if (!mb) return nullptr;
    float bx, by, bz; s.position(bx, by, bz);
    const float dx = mb->x - bx, dy = mb->y - by, dz = mb->z - bz;
    return (dx*dx + dy*dy + dz*dz <= kInteractSq) ? mb : nullptr;
}

// ---------- idle:mail_drain ----------
bool MailDrainGate(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&, uint32 now_ms)
{
    // Quest-first (2026-06-16): yield to a reachable quest action.
    if (s.has_actionable_quest()) return false;
    if (!s.has_drainable_mail() || InRangeMailbox(s) == nullptr)
    {
        if (ai.pending_mail_open_at_ms() != 0)
            ai.set_pending_mail_open_at_ms(0);
        return false;
    }
    // Open-the-mailbox hesitation: 1.5–4s before the first take packet
    // lands. Subsequent mails in the same batch drain at their own
    // per-mail action_recently_tried lockout (~30s). Only the FIRST
    // arrival at the mailbox waits.
    uint32 ready_at = ai.pending_mail_open_at_ms();
    if (ready_at == 0)
    {
        const uint32 jitter = 1500u + (uint32(s.bot_id()) * 2654435761u) % 2500u;
        ai.set_pending_mail_open_at_ms(now_ms + jitter);
        return false;
    }
    return now_ms >= ready_at;
}

bool MailDrainFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,BotIntentEmitter& emit, uint32 now_ms)
{
    auto const* mailbox = InRangeMailbox(s);
    if (!mailbox) return false;

    auto const* m = s.next_drainable_mail();
    if (!m) return false;

    // L-P1a: never auto-pay Cash-On-Delivery in autonomous drain — a
    // hostile sender could otherwise drain the bot's gold by mailing a
    // COD item. The selector already filters cod>0, but guard here too in
    // case a COD mail is reached by any other path.
    if (m->cod > 0)
        return false;

    const uint64 mailbox_low = mailbox->guid.GetCounter();
    const uint64 mail_key =
        (mailbox_low & 0x00FFFFFFFFFFFFFFULL) ^ uint64(m->message_id);
    if (ai.action_recently_tried(BotAI::ActionKind::MailDrain, mail_key, now_ms))
        return false;

    if (uint32(m->item_guid_lows.size()) > uint32(s.bag_free_slots()))
    {
        // Bag full — note retry and bail; let bag-management clear space.
        ai.note_action_retry(BotAI::ActionKind::MailDrain, mail_key, now_ms);
        return false;
    }

    if (m->money > 0)
        emit.mail_take_money(mailbox->guid, m->message_id);
    for (uint64 item_low : m->item_guid_lows)
        emit.mail_take_item(mailbox->guid, m->message_id, item_low);
    ai.note_action_retry(BotAI::ActionKind::MailDrain, mail_key, now_ms);

    if (m->message_type == 0 && m->sender_low != 0 &&
        (m->money > 0 || !m->item_guid_lows.empty()) &&
        ai.personality().verbosity != Verbosity::Silent)
    {
        const uint64 thank_key = (uint64(0xA1) << 56) | uint64(m->sender_low);
        if (!ai.action_recently_tried(BotAI::ActionKind::MailDrain, thank_key, now_ms))
        {
            ObjectGuid sender_g = ObjectGuid::Create<HighGuid::Player>(m->sender_low);
            std::string sender_name;
            if (sCharacterCache->GetCharacterNameByGuid(sender_g, sender_name) &&
                !sender_name.empty())
            {
                emit.whisper(sender_name, "ty for the mail!");
                ai.note_action_retry(BotAI::ActionKind::MailDrain, thank_key, now_ms);
            }
        }
    }
    ai.set_last_rule_fired("idle:mail_drain");
    return true;
}

// ---------- idle:mat_share_to_owner ----------
bool MatShareGate(BotSnapshotView const& s, BotAI&, GroupSnapshotView const&,uint32)
{
    if (!s.has_owner_character()) return false;
    // Quest-first (2026-06-16): yield to a reachable quest action.
    if (s.has_actionable_quest()) return false;
    if (s.raw().inventory.bag_items.empty()) return false;
    return InRangeMailbox(s) != nullptr;
}

bool MatShareFire(BotSnapshotView const& s, BotAI& ai, GroupSnapshotView const&,BotIntentEmitter& emit, uint32 now_ms)
{
    auto const* mailbox = InRangeMailbox(s);
    if (!mailbox) return false;

    for (auto const& it : s.raw().inventory.bag_items)
    {
        if (it.guid.IsEmpty()) continue;
        if (it.is_quest_item) continue;
        if (it.count < 60) continue;
        if (it.item_class != 7) continue;
        if (it.item_subclass != 5 && it.item_subclass != 6 &&
            it.item_subclass != 7 && it.item_subclass != 9 &&
            it.item_subclass != 10)
            continue;
        const uint64 mat_key = uint64(it.entry);
        if (ai.action_recently_tried(BotAI::ActionKind::MatShare, mat_key, now_ms))
            continue;
        if (it.stats.is_soulbound || it.stats.bonding == 1) continue;

        constexpr uint16 kKeepBuffer = 40;
        const uint32 send_count = uint32(it.count) - kKeepBuffer;
        emit.emit(MailIntent{MailSendItemIntent{
            s.owner_name(), it.guid,
            /*count=*/send_count,
            /*copper=*/0,
            /*cod=*/0,
            std::string("Surplus mats"),
            std::string("From your bot — bags overflowing.")
        }});
        ai.note_action_retry(BotAI::ActionKind::MatShare, mat_key, now_ms);
        ai.set_last_rule_fired("idle:mat_share_to_owner");
        return true;
    }
    return false;
}

} // anonymous namespace

// ---------- idle:walk_to_known_mailbox ----------
//
// When the bot has drainable mail but no mailbox in snapshot scan range
// (~30y), check the operator-curated WorldMetadata for a Mailbox
// annotation on this map. If one is within 600y (a reasonable "go
// pick up your mail" walk), walk toward it; once within the 30y
// snapshot scan radius the existing idle:mail_drain rule takes over
// the close approach + actual mail intent.
//
// Why this exists: the snapshot's nearby_objects only contains GOs
// within ~30y of the bot. A bot in the wilds with mail but no mailbox
// in range would otherwise wait passively or just wander — even
// though the operator has annotated mailbox locations as
// WorldMetadataKind::Mailbox. This rule turns those annotations into
// purposeful movement.
bool WalkToKnownMailboxGate(BotSnapshotView const& s, BotAI& ai,
                            GroupSnapshotView const&, uint32 /*now_ms*/)
{
    (void)ai;
    if (s.in_combat() || s.is_casting() || s.raw().movement.is_mounted)
        return false;
    // Quest-first (2026-06-16): yield to a reachable quest action.
    if (s.has_actionable_quest()) return false;
    if (!s.has_drainable_mail()) return false;
    // Already in snapshot mailbox range — let the regular drain rule
    // handle it.
    for (auto const& o : s.raw().world_objects.nearby_objects)
        if (o.go_type == /*MAILBOX*/ 19)
            return false;
    // Need at least one Mailbox annotation on this map.
    using ::Playerbot::V2::World::WorldMetadataKind;
    return s.any_metadata_within(uint32(WorldMetadataKind::Mailbox), 600.0f);
}

bool WalkToKnownMailboxFire(BotSnapshotView const& s, BotAI& ai,
                            GroupSnapshotView const&,
                            BotIntentEmitter& emit, uint32 /*now_ms*/)
{
    using ::Playerbot::V2::World::WorldMetadataStore;
    using ::Playerbot::V2::World::WorldMetadataKind;
    auto rows = WorldMetadataStore::Instance().RecordsForMapAndKind(
        s.map_id(), WorldMetadataKind::Mailbox);
    if (rows.empty()) return false;
    float bx, by, bz; s.position(bx, by, bz);
    float best_dsq = std::numeric_limits<float>::infinity();
    float bestx = 0.f, besty = 0.f, bestz = 0.f;
    for (auto const& r : rows)
    {
        const float dx = r.x - bx;
        const float dy = r.y - by;
        const float dsq = dx*dx + dy*dy;
        if (dsq < best_dsq)
        { best_dsq = dsq; bestx = r.x; besty = r.y; bestz = r.z; }
    }
    if (best_dsq <= 30.0f * 30.0f) return false;
    // Step toward the target. Personality risk-tolerance picks stride
    // (matches walk_to_quest_ender / walk_to_innkeeper convention).
    const float kStep =
        ai.personality().risk_tolerance == RiskTolerance::Cautious ? 25.0f :
        ai.personality().risk_tolerance == RiskTolerance::Reckless ? 50.0f :
                                                                       35.0f;
    const float dist = std::sqrt(best_dsq);
    const float scale = std::min(kStep, dist) / dist;
    const float tx = bx + (bestx - bx) * scale;
    const float ty = by + (besty - by) * scale;
    if (NearbyUnit const* threat = s.path_threat(
            tx, ty,
            /*max_forward*/ std::min(kStep, 35.0f),
            /*half_width*/  10.0f))
    {
        if (emit.start_attack(threat->guid))
        {
            ai.set_last_rule_fired("idle:walk_mailbox_pull_threat");
            return true;
        }
    }
    emit.move_to(tx, ty, bestz, /*run*/ true);
    ai.set_last_rule_fired("idle:walk_to_known_mailbox");
    return true;
}

void RegisterMailRules(IdleRuleRegistry& r)
{
    {
        IdleRule rule;
        rule.name     = "idle:walk_to_known_mailbox";
        rule.priority = 475;            // just below mail_drain (480)
        rule.gate     = &WalkToKnownMailboxGate;
        rule.fire     = &WalkToKnownMailboxFire;
        // 5s gate throttle — we re-evaluate destination every 5s but
        // the actual walk continues via the existing motion master
        // spline in between. Mailbox positions don't move.
        rule.min_interval_ms = 5000;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:mail_drain";
        rule.priority = 480;
        rule.gate     = &MailDrainGate;
        rule.fire     = &MailDrainFire;
        // Mail arrives sparsely; 2s gate-throttle is well below the
        // human-perceptible threshold for "bot picked up its mail".
        rule.min_interval_ms = 2000;
        r.register_rule(std::move(rule));
    }
    {
        IdleRule rule;
        rule.name     = "idle:mat_share_to_owner";
        rule.priority = 470;
        rule.gate     = &MatShareGate;
        rule.fire     = &MatShareFire;
        // Mat-share decisions don't change between snapshots faster than
        // the bag is actually mutated by loot/craft.
        rule.min_interval_ms = 2000;
        r.register_rule(std::move(rule));
    }
}

} // namespace Playerbot
