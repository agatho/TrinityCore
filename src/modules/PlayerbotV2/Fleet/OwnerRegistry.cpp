#include "OwnerRegistry.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include <cstdio>
#include <stdexcept>
#include <unordered_set>

namespace Playerbot {

namespace {
auto& Db() { return CharacterDatabase; }
}

size_t OwnerRegistry::LoadFromDb()
{
    auto result = Db().Query(
        "SELECT character_guid_low, owner_account_id, owner_player_guid "
        "FROM playerbot_v2_character "
        "WHERE owner_account_id != 0");
    std::unique_lock lk(mtx_);
    owners_.clear();
    if (!result)
    {
        TC_LOG_INFO("playerbot.v2", "[PlayerbotV2 Owners] No owner bindings yet.");
        return 0;
    }
    do {
        Field* f = result->Fetch();
        OwnerBinding b{};
        const BotId bot = f[0].GetUInt64();
        b.account_id    = f[1].GetUInt32();
        b.player_guid   = f[2].GetUInt64();
        owners_[bot] = b;
    } while (result->NextRow());
    TC_LOG_INFO("playerbot.v2",
        "[PlayerbotV2 Owners] Loaded {} owner bindings.", owners_.size());
    return owners_.size();
}

OwnerBinding OwnerRegistry::GetOwner(BotId bot) const
{
    std::shared_lock lk(mtx_);
    auto it = owners_.find(bot);
    return it == owners_.end() ? OwnerBinding{} : it->second;
}

void OwnerRegistry::SetOwner(BotId bot, uint32 account_id, uint64 player_guid)
{
    {
        std::unique_lock lk(mtx_);
        if (account_id == 0 && player_guid == 0)
            owners_.erase(bot);
        else
            owners_[bot] = OwnerBinding{account_id, player_guid};
    }
    // Persist. UPDATE only — the bot row must already exist (it's
    // created by mark_as_bot via BotIdentityRegistry). If the row is
    // missing the UPDATE is a no-op and the in-memory cache is the
    // only state, which is fine for the rare race of a just-spawned
    // bot — next mutation will succeed.
    Db().DirectPExecute(
        "UPDATE playerbot_v2_character "
        "SET owner_account_id = {}, owner_player_guid = {} "
        "WHERE character_guid_low = {}",
        account_id, player_guid, bot);
}

bool OwnerRegistry::IsOwner(BotId bot, uint32 sender_account, uint64 sender_guid) const
{
    OwnerBinding const b = GetOwner(bot);
    if (b.account_id == 0) return false;          // unowned
    if (b.account_id != sender_account) return false;
    if (b.player_guid != 0 && b.player_guid != sender_guid) return false;
    return true;
}

OwnerRegistry::SquadState OwnerRegistry::LoadSquadState(BotId bot) const
{
    SquadState out;
    auto result = Db().PQuery(
        "SELECT formation_type, formation_slot, follow_distance_yd, owner_verbose "
        "FROM playerbot_v2_character WHERE character_guid_low = {}",
        bot);
    if (!result) return out;
    Field* f = result->Fetch();
    out.formation_type   = f[0].GetUInt8();
    out.formation_slot   = f[1].GetUInt8();
    out.follow_distance  = f[2].GetFloat();
    out.owner_verbose    = f[3].GetBool();
    return out;
}

void OwnerRegistry::SaveSquadState(BotId bot, SquadState const& s) const
{
    Db().DirectPExecute(
        "UPDATE playerbot_v2_character "
        "SET formation_type = {}, formation_slot = {}, "
        "    follow_distance_yd = {:.2f}, owner_verbose = {} "
        "WHERE character_guid_low = {}",
        s.formation_type, s.formation_slot,
        s.follow_distance, s.owner_verbose ? 1 : 0,
        bot);
}

size_t OwnerRegistry::SaveSquadPreset(uint32 owner_account,
                                       std::string const& preset_name) const
{
    if (owner_account == 0 || preset_name.empty()) return 0;
    std::vector<BotId> const owned = BotsOwnedBy(owner_account);
    if (owned.empty()) return 0;
    // Compose the payload: `bot:type:slot:fd|bot:type:slot:fd|...`.
    // No JSON parser dependency; the format is fixed and round-trips
    // exactly via std::stoull / std::stof on Load.
    std::string payload;
    payload.reserve(owned.size() * 24);
    for (BotId bot : owned)
    {
        SquadState const s = LoadSquadState(bot);
        if (!payload.empty()) payload.push_back('|');
        char buf[64];
        const int n = snprintf(buf, sizeof(buf),
            "%llu:%u:%u:%.2f",
            static_cast<unsigned long long>(bot),
            unsigned(s.formation_type),
            unsigned(s.formation_slot),
            double(s.follow_distance));
        payload.append(buf, buf + (n > 0 ? n : 0));
    }
    // Upsert (REPLACE semantics — owner may overwrite a preset).
    // EscapeString on preset_name: owner-supplied input that may contain
    // apostrophes. payload is `bot:type:slot:fd|...` format — digits +
    // colons + pipes only, no escape needed.
    std::string esc_name = preset_name;
    Db().EscapeString(esc_name);
    Db().DirectPExecute(
        "REPLACE INTO playerbot_v2_squad_preset "
        "(owner_account_id, preset_name, payload_json) "
        "VALUES ({}, '{}', '{}')",
        owner_account, esc_name, payload);
    return owned.size();
}

size_t OwnerRegistry::LoadSquadPreset(uint32 owner_account,
                                       std::string const& preset_name) const
{
    if (owner_account == 0 || preset_name.empty()) return 0;
    std::string esc_name = preset_name;
    Db().EscapeString(esc_name);
    auto result = Db().PQuery(
        "SELECT payload_json FROM playerbot_v2_squad_preset "
        "WHERE owner_account_id = {} AND preset_name = '{}'",
        owner_account, esc_name);
    if (!result) return 0;
    Field* f = result->Fetch();
    std::string const payload = f[0].GetString();
    if (payload.empty()) return 0;
    // Parse: split by '|', then ':' inside each. Skip entries whose
    // bot is no longer owned by this account (owner may have
    // disowned mid-preset; loading would otherwise stomp another
    // owner's bots).
    std::unordered_set<BotId> still_owned;
    for (BotId b : BotsOwnedBy(owner_account)) still_owned.insert(b);

    size_t applied = 0;
    size_t pos = 0;
    while (pos < payload.size())
    {
        size_t end = payload.find('|', pos);
        if (end == std::string::npos) end = payload.size();
        std::string const part = payload.substr(pos, end - pos);
        pos = end + 1;
        // Parse bot:type:slot:fd
        size_t a = part.find(':');
        if (a == std::string::npos) continue;
        size_t b = part.find(':', a + 1);
        if (b == std::string::npos) continue;
        size_t c = part.find(':', b + 1);
        if (c == std::string::npos) continue;
        try
        {
            BotId bot = std::stoull(part.substr(0, a));
            uint8 ftype = static_cast<uint8>(std::stoul(part.substr(a + 1, b - a - 1)));
            uint8 fslot = static_cast<uint8>(std::stoul(part.substr(b + 1, c - b - 1)));
            float fd    = std::stof(part.substr(c + 1));
            if (!still_owned.count(bot)) continue;
            SquadState s;
            s.formation_type   = ftype;
            s.formation_slot   = fslot;
            s.follow_distance  = fd;
            s.owner_verbose    = LoadSquadState(bot).owner_verbose; // preserve verbose
            SaveSquadState(bot, s);
            ++applied;
        }
        catch (std::exception const&) { /* malformed entry — skip */ }
    }
    return applied;
}

std::vector<BotId> OwnerRegistry::BotsOwnedBy(uint32 account_id) const
{
    std::vector<BotId> out;
    if (account_id == 0) return out;
    std::shared_lock lk(mtx_);
    out.reserve(owners_.size() / 16 + 1);
    for (auto const& [bot, binding] : owners_)
        if (binding.account_id == account_id)
            out.push_back(bot);
    return out;
}

} // namespace Playerbot
