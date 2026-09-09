// PlayerbotHooks.cpp
// Implementations of the hook surface declared in PlayerbotHooks.h.
// Compiled only when BUILD_PLAYERBOT_V2 is ON.

#include "PlayerbotHooks.h"

#if TRINITY_PLAYERBOT_V2

#include "../../../modules/PlayerbotV2/PlayerbotV2.h"

namespace Playerbot::Hooks {

void OnPlayerLogin(Player* p) { V2::Module::instance().OnPlayerLogin(p); }
void OnPlayerLogout(Player* p) { V2::Module::instance().OnPlayerLogout(p); }
void OnLevelUp(Player* p, uint8 new_level) { V2::Module::instance().OnLevelUp(p, new_level); }
void OnDeath(Unit* victim, Unit* killer) { V2::Module::instance().OnDeath(victim, killer); }
void OnResurrect(Player* p) { V2::Module::instance().OnResurrect(p); }
void OnSpecChanged(Player* p, uint8 new_spec) { V2::Module::instance().OnSpecChanged(p, new_spec); }

void OnDamageDealt(Unit* attacker, Unit* victim, int32 amount, uint32 spell_id)
{
    V2::Module::instance().OnDamageDealt(attacker, victim, amount, spell_id);
}

void OnDamageTaken(Unit* attacker, Unit* victim, int32 amount, uint32 spell_id)
{
    V2::Module::instance().OnDamageTaken(attacker, victim, amount, spell_id);
}

void OnHealReceived(Unit* healer, Unit* target, int32 amount, uint32 spell_id)
{
    V2::Module::instance().OnHealReceived(healer, target, amount, spell_id);
}

void OnAuraApplied(Unit* target, Aura* aura) { V2::Module::instance().OnAuraApplied(target, aura); }
void OnAuraRemoved(Unit* target, Aura* aura) { V2::Module::instance().OnAuraRemoved(target, aura); }

void OnGroupMemberJoined(Group* g, Player* p) { V2::Module::instance().OnGroupMemberJoined(g, p); }
void OnGroupMemberLeft(Group* g, Player* p) { V2::Module::instance().OnGroupMemberLeft(g, p); }
void OnWhisperReceived(Player* sender, Player* receiver, std::string const& msg)
{
    V2::Module::instance().OnWhisperReceived(sender, receiver, msg);
}

void OnPartyChat(Player* sender, Group* group, std::string const& msg)
{
    V2::Module::instance().OnPartyChat(sender, group, msg);
}

void OnGuildChat(Player* sender, uint64 guild_id, std::string const& msg)
{
    V2::Module::instance().OnGuildChat(sender, guild_id, msg);
}

void OnSayChat(Player* sender, std::string const& msg)
{
    V2::Module::instance().OnSayChat(sender, msg);
}

void OnYellChat(Player* sender, std::string const& msg)
{
    V2::Module::instance().OnYellChat(sender, msg);
}

void OnTextEmote(Player* sender, uint32 emote_id, ObjectGuid target)
{
    V2::Module::instance().OnTextEmote(sender, emote_id, target);
}

void OnGuildMemberAdded(uint64 guild_id, ObjectGuid joiner_guid, std::string const& joiner_name)
{
    V2::Module::instance().OnGuildMemberAdded(guild_id, joiner_guid, joiner_name);
}

void OnPlayerJoinedBgQueue(Player* player, uint32 bg_type_id, uint8 bracket)
{
    V2::Module::instance().OnPlayerJoinedBgQueue(player, bg_type_id, bracket);
}

void OnPlayerJoinedLfg(Player* player, uint32 dungeon_id, uint8 role_mask)
{
    V2::Module::instance().OnPlayerJoinedLfg(player, dungeon_id, role_mask);
}

void OnBGInvitationReceived(Player* player, uint32 bg_instance_id, uint32 bg_type_id)
{
    V2::Module::instance().OnBGInvitationReceived(player, bg_instance_id, bg_type_id);
}

void OnLfgProposalReceived(Player* player, uint32 proposal_id)
{
    V2::Module::instance().OnLfgProposalReceived(player, proposal_id);
}

void OnBGPortFailed(Player* player, uint8 reason_code, uint32 bg_instance_id)
{
    V2::Module::instance().OnBGPortFailed(player, reason_code, bg_instance_id);
}

void OnPathOutcome(uint8 outcome)
{
    V2::Module::instance().OnPathOutcome(outcome);
}

} // namespace Playerbot::Hooks

#endif // TRINITY_PLAYERBOT_V2
