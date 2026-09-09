// BotAddressResolver — turns squad-address prefixes ("all:", "tank:",
// "Areon:", "mage:") into a vector of bot Player*s the sender is
// authorised to command. Used by the whisper command parser at the
// top of Dispatch, so a single whisper to one bot can apply to many.
//
// Resolution is bounded by the sender's OwnerRegistry binding: a
// prefix never resolves to a bot the sender doesn't own (or, for
// unowned bots, to one outside the sender's group). This means
// owners can never accidentally command someone else's bot via
// `all: <command>`, and griefers can't drive other players' fleets.
//
// Returned in resolution order: the prefix-matched set, in arbitrary
// stable order. The whispered "primary" bot is INCLUDED in the set
// when it matches the prefix (so /w Areon "all: come" still moves
// Areon if the player owns him); the broadcast helper is responsible
// for de-duplicating per-bot intent emits if the primary is also in
// the resolved set.

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

class Player;

namespace Playerbot {

struct ResolvedAddress
{
    enum class Kind : uint8_t
    {
        Single,        // No prefix — just the whispered bot.
        All,           // Every owned bot, online, any map.
        Squad,         // Owned + currently in sender's group.
        Here,          // Owned + on sender's map within 60y.
        Role_Tank,     // Owned + tank-spec.
        Role_Healer,   // Owned + healer-spec.
        Role_Dps,      // Owned + dps-spec.
        Class,         // Owned + matching class id.
        Spec,          // Owned + matching spec id.
        Marked,        // Owned + carries an in-game raid marker.
        Name,          // Owned + matching character name (case-insensitive).
    };

    Kind         kind = Kind::Single;
    // Stripped command text — prefix removed, leading whitespace trimmed.
    std::string  command;
    // Filter parameter for Class/Spec/Name kinds. Empty otherwise.
    std::string  filter;
    // The actual bots resolved by the prefix. Populated by Resolve().
    std::vector<Player*> bots;
};

class BotAddressResolver
{
public:
    // Parse the leading prefix off `whisper_text` and resolve it to bots
    // the `sender` is allowed to command. The whispered "primary" bot
    // is passed as fallback for the no-prefix case (Kind::Single resolves
    // to {primary}).
    //
    // Returns Kind::Single + bots = {primary} when no prefix is found.
    // Returns the resolved kind/filter/bots when a prefix matches.
    static ResolvedAddress Resolve(
        Player const* sender,
        Player*       primary,
        std::string_view whisper_text);

    // Lower-level: parse just the prefix without resolving. Useful for
    // diagnostics / dry-runs. Returns kind / filter / stripped command;
    // bots field stays empty.
    static ResolvedAddress ParsePrefix(std::string_view whisper_text);
};

} // namespace Playerbot
