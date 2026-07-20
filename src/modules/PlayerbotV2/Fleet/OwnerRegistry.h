// OwnerRegistry — bot-to-owner binding. The "owner" of a bot is a
// specific player account (and optionally a specific character on that
// account). Authority for whisper / squad commands derives from this
// binding: only the owner can issue control commands, and the binding
// survives logout, group disband, and server restart.
//
// Storage: persisted in playerbot_v2_character.owner_account_id /
// owner_player_guid (migration 0002). In-memory cache is a hash map
// guarded by shared_mutex. Writes are immediate (not debounced) — owner
// changes are infrequent and command authority depends on durability.
//
// World-thread mutations (mark / adopt / disown). Reads from any thread
// (whisper handler, AI workers, snapshot builder).

#pragma once

#include "Bot/BotTypes.h"
#include <cstdint>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace Playerbot {

struct OwnerBinding
{
    // Account that owns the bot. 0 = unowned (legacy / pre-migration).
    uint32 account_id = 0;
    // Specific character GUID-low on that account that owns the bot. 0
    // means "any character on the account is the owner". Used to support
    // alts: by default any of the player's characters can command, but
    // an owner can bind a bot to a specific main if they want.
    uint64 player_guid = 0;
};

class OwnerRegistry
{
public:
    // Loads bindings from the DB. Returns the count.
    size_t LoadFromDb();

    // Returns the bot's binding. {0, 0} when unowned.
    OwnerBinding GetOwner(BotId bot) const;

    // Sets the bot's owner. Writes immediately to the DB; the in-memory
    // cache is updated on success. account_id == 0 + player_guid == 0
    // = clear ownership.
    void SetOwner(BotId bot, uint32 account_id, uint64 player_guid);

    // Convenience: clears any binding.
    void ClearOwner(BotId bot) { SetOwner(bot, 0, 0); }

    // Authority predicate. Returns true when (sender_account, sender_guid)
    // is allowed to command `bot` per the binding rules:
    //   - If bot is unowned: any sender is allowed (legacy behaviour;
    //     command parser then layers a group-membership check for those).
    //   - If bot is owned and player_guid != 0: only that exact char.
    //   - If bot is owned and player_guid == 0: any char on the account.
    bool IsOwner(BotId bot, uint32 sender_account, uint64 sender_guid) const;

    // Returns the bot ids currently owned by `account_id`. Empty if none.
    // O(N) in the registry; only used by GM diagnostic commands and the
    // /squad addressing — not in any per-tick path.
    std::vector<BotId> BotsOwnedBy(uint32 account_id) const;

    // Per-bot squad state persisted alongside the owner binding.
    // Migration 0002 adds these columns. Loaded on bot register;
    // saved when /formation, /follow_distance, /slot, /verbose mutate.
    struct SquadState
    {
        uint8 formation_type = 0;     // FormationType (Free=0)
        uint8 formation_slot = 0;
        float follow_distance = 5.0f;
        bool  owner_verbose  = false;
    };

    // Pull the persisted squad state for a bot. Used at bot register
    // time so the in-memory BotAI starts with the owner's saved
    // preferences instead of defaulting every relog.
    SquadState LoadSquadState(BotId bot) const;

    // Persist the in-memory squad state back to the DB. Called from
    // mutation commands (/formation, /follow_distance, etc). Single
    // UPDATE statement; safe to call frequently — no debouncing
    // necessary for a fleet under ~500 bots.
    void SaveSquadState(BotId bot, SquadState const& s) const;

    // Squad-preset save/load. A "squad" is the owner's currently-owned
    // set of bots; a preset is a snapshot of each bot's
    // (formation_type, formation_slot, follow_distance) under a
    // user-named label. Stored in playerbot_v2_squad_preset, keyed by
    // (owner_account_id, preset_name).
    //
    // Save walks the owner's bots, gathers each bot's current squad
    // state, serialises as a compact string (`bot:type:slot:fd|...`),
    // upserts the preset row.
    //
    // Load reads the row, parses, applies SquadState to each named bot
    // (if still owned + registered). Bots no longer owned by the
    // account are skipped silently.
    //
    // Both return the count affected. 0 from Save = the owner has no
    // bots; 0 from Load = preset name not found or all bots gone.
    size_t SaveSquadPreset(uint32 owner_account, std::string const& preset_name) const;
    size_t LoadSquadPreset(uint32 owner_account, std::string const& preset_name) const;

private:
    mutable std::shared_mutex                mtx_;
    std::unordered_map<BotId, OwnerBinding>  owners_;
};

} // namespace Playerbot
