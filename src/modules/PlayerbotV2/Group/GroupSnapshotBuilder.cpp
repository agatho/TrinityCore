#include "GroupSnapshotBuilder.h"
#include "../Bot/ClassTables.h"  // canonical IsTankSpec / IsHealerSpec
#include "../Services.h"                    // Lifecycle() accessor
#include "../Fleet/BotIdentityRegistry.h"   // is_bot definition (Services.h fwd-declares)
#include "Player.h"
#include "Group.h"
#include "Unit.h"
#include "ObjectAccessor.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "Spell.h"
#include "LFG.h"
#include "SharedDefines.h"

namespace Playerbot {

namespace {

// Same dispel-id translation table as BotSnapshotBuilder (DRY-violating
// duplicate kept here so the Group module doesn't depend on Bot internals).
DispelType TranslateDispelType(uint32 core)
{
    switch (core)
    {
        case 1:  return DispelType::Magic;
        case 2:  return DispelType::Curse;
        case 3:  return DispelType::Disease;
        case 4:  return DispelType::Poison;
        case 9:  return DispelType::Enrage;
        default: return DispelType::None;
    }
}

// Long-duration raid buffs the State_Idle group-buff maintainer cares about.
// Kept tiny on purpose — every entry costs N members * sizeof(AuraEntry) per
// snapshot. Add an id here only when a class-buff rule needs it. Order
// doesn't matter; the lookup is linear.
constexpr uint32 kTrackedRaidBuffs[] = {
    21562,    // Power Word: Fortitude (Priest)
    1126,     // Mark of the Wild       (Druid)
    6673,     // Battle Shout           (Warrior)
    1459,     // Arcane Intellect       (Mage)
    364342,   // Blessing of the Bronze (Evoker)
    462854,   // Skyfury                (Shaman)
};

bool IsTrackedRaidBuff(uint32 spell_id)
{
    for (uint32 id : kTrackedRaidBuffs)
        if (id == spell_id) return true;
    return false;
}

// Single-walk filler for both tracked raid buffs and harmful debuffs on
// `u`. Replaces a prior two-pass design that walked GetAppliedAuras
// twice; at 40-member raids × N grouped bots × snapshot cadence this
// adds up. Each applied aura is classified once (positive=buff path,
// otherwise debuff path) and pushed to the matching out-vector.
void CopyMemberAuras(Unit const* u,
                     std::vector<AuraEntry>& buffs_out,
                     std::vector<AuraEntry>& debuffs_out)
{
    if (!u) return;
    auto const& applied = u->GetAppliedAuras();
    debuffs_out.reserve(applied.size());
    for (auto const& [id, app] : applied)
    {
        if (!app) continue;
        Aura const* base = app->GetBase();
        if (!base) continue;
        SpellInfo const* si = base->GetSpellInfo();
        if (!si) continue;

        const bool positive = app->IsPositive();
        if (positive)
        {
            // Buffs we only care about a small fixed whitelist — the
            // long-duration raid buffs the State_Idle group-buff
            // maintainer needs to avoid double-casting.
            if (!IsTrackedRaidBuff(si->Id)) continue;
            AuraEntry e{};
            e.spell_id    = si->Id;
            e.stacks      = base->GetStackAmount();
            const int32 dur = base->GetDuration();
            e.remaining   = Ms{dur > 0 ? dur : 0};
            e.caster      = base->GetCasterGUID();
            e.dispel_type = DispelType::None;
            e.mechanic    = si->Mechanic;
            e.is_harmful  = false;
            e.is_stealable = false;
            buffs_out.push_back(e);
        }
        else
        {
            // Healers query this list to find dispel candidates per
            // group member.
            AuraEntry e{};
            e.spell_id    = si->Id;
            e.stacks      = base->GetStackAmount();
            const int32 dur = base->GetDuration();
            e.remaining   = Ms{dur > 0 ? dur : 0};
            e.caster      = base->GetCasterGUID();
            e.dispel_type = TranslateDispelType(si->Dispel);
            e.mechanic    = si->Mechanic;
            // Bleed promotion mirrors BotSnapshotBuilder — let
            // bleed-clearing helpers find the debuff via DispelType::Bleed.
            if (e.dispel_type == DispelType::None && si->Mechanic == MECHANIC_BLEED)
                e.dispel_type = DispelType::Bleed;
            e.is_harmful  = true;
            e.is_stealable = false;
            debuffs_out.push_back(e);
        }
    }
}

// Translate a MemberSlot's role flags into our compact Role enum. The LFG
// role bits cover tank/healer/damage; main-tank flag (set by ` /mt` or RAID
// commands) takes precedence when present. When neither flag is set (typical
// for player-led groups that didn't queue via LFG), fall back to class+spec
// inference so healers/tanks are still identifiable. Spec → role mapping
// lives in Bot/ClassTables.cpp — single source for all callers.
Role InferRoleFromSpec(uint8 cls, uint32 spec)
{
    if (IsTankSpec(cls, uint16(spec)))   return Role::Tank;
    if (IsHealerSpec(cls, uint16(spec))) return Role::Healer;
    return cls != 0 ? Role::Dps : Role::Unknown;
}

Role TranslateRole(uint8 lfg_role_bits, uint8 group_flags, uint8 cls, uint32 spec)
{
    if (group_flags & MEMBER_FLAG_MAINTANK)      return Role::Tank;
    if (lfg_role_bits & lfg::PLAYER_ROLE_TANK)   return Role::Tank;
    if (lfg_role_bits & lfg::PLAYER_ROLE_HEALER) return Role::Healer;
    if (lfg_role_bits & lfg::PLAYER_ROLE_DAMAGE) return Role::Dps;
    return InferRoleFromSpec(cls, spec);
}

} // anonymous

std::shared_ptr<GroupSnapshot const> GroupSnapshotBuilder::Build(Player* p, SnapshotVer version)
{
    if (!p) return {};
    Group* g = p->GetGroup();
    if (!g) return {};

    auto snap = std::make_shared<GroupSnapshot>();
    snap->version        = version;
    snap->group_guid     = g->GetGUID();
    snap->leader         = g->GetLeaderGUID();
    snap->is_raid        = g->isRaidGroup();
    snap->is_lfg         = g->isLFGGroup();
    snap->loot_method    = static_cast<uint8>(g->GetLootMethod());
    snap->ready_check_active = g->IsReadyCheckStarted();
    // Raid marks (Skull/Cross/etc.) — used by future kill-priority and CC
    // assignment rules. Empty for parties since GroupBuilder clears them.
    for (uint8 i = 0; i < snap->raid_marks.size(); ++i)
        snap->raid_marks[i] = g->GetTargetIcon(i);

    snap->members.reserve(g->GetMembersCount());
    for (auto const& slot : g->GetMemberSlots())
    {
        GroupMemberSummary m{};
        m.guid    = slot.guid;
        m.name    = slot.name;
        m.cls     = slot._class;
        m.is_bot  = Services::Initialized() &&
                    Services::Lifecycle().is_bot(slot.guid.GetCounter());

        // Live state from the actual Player if they're online & in-world.
        if (Player* mp = ObjectAccessor::FindConnectedPlayer(slot.guid))
        {
            m.online       = true;
            m.spec         = static_cast<uint32>(mp->GetPrimarySpecialization());
            m.level        = mp->GetLevel();
            m.hp           = static_cast<int32>(mp->GetHealth());
            m.max_hp       = static_cast<int32>(mp->GetMaxHealth());
            m.mana         = static_cast<int32>(mp->GetPower(POWER_MANA));
            m.max_mana     = static_cast<int32>(mp->GetMaxPower(POWER_MANA));
            m.in_combat    = mp->IsInCombat();
            m.is_mounted   = mp->IsMounted();
            m.is_alive     = mp->IsAlive();
            if (Unit* v = mp->GetVictim())
                m.victim   = v->GetGUID();
            // Fallback to the SELECTED target when there's no melee victim yet.
            // A member chasing an out-of-melee-range mob (the tank StuckChasing a
            // harbor 47403 Mechanical Reaper it can't path to, or a ranged class
            // mid-engage) has GetVictim()==null but GetTarget()==the mob, so
            // without this the GROUP snapshot shows an EMPTY victim and the
            // DPS-assist / harbor-dps-open rules can't focus what the tank is
            // actually engaging — the group stalls (live 2026-06-29: tank stuck on
            // a 47403, member.victim empty, idle:dungeon_harbor_dps_open fired 0x).
            // Mirrors the bot's OWN-snapshot victim seeding (BotSnapshotBuilder
            // ~2442): in-combat + a live, legally-attackable hostile selection only
            // (never seed a friendly heal-target a healer tab'd).
            if (m.victim.IsEmpty() && mp->IsInCombat())
                if (ObjectGuid t = mp->GetTarget(); !t.IsEmpty() && t != mp->GetGUID())
                    if (Unit* sel = ObjectAccessor::GetUnit(*mp, t);
                        sel && sel->IsAlive() && mp->IsValidAttackTarget(sel))
                        m.victim = sel->GetGUID();
            m.x            = mp->GetPositionX();
            m.y            = mp->GetPositionY();
            m.z            = mp->GetPositionZ();
            m.map_id       = mp->GetMapId();
            // Mirror what bot's own snapshot tracks for itself, so dispatchers
            // can reason about teammates the same way (e.g. interrupt-coord
            // when a healer is mid-cast, or skip Mass Dispel if everyone's CC
            // already broke).
            for (uint32 slot_t : { CURRENT_GENERIC_SPELL, CURRENT_CHANNELED_SPELL })
            {
                if (Spell* casting = mp->GetCurrentSpell(static_cast<CurrentSpellTypes>(slot_t)))
                {
                    m.is_casting    = true;
                    m.casting_spell = casting->m_spellInfo->Id;
                    if (Unit* tgt = casting->m_targets.GetUnitTarget())
                        m.casting_target = tgt->GetGUID();
                    break;
                }
            }
            CopyMemberAuras(mp, m.buffs, m.debuffs);
        }
        else
        {
            m.online       = false;
        }

        // Resolve role last so spec inference (when needed) sees populated cls/spec.
        m.role = TranslateRole(slot.roles, slot.flags, m.cls, m.spec);

        snap->members.push_back(std::move(m));
    }

    return snap;
}

} // namespace Playerbot
