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

#ifndef TRINITYCORE_BNET_DISCORD_LINK_STORE_H
#define TRINITYCORE_BNET_DISCORD_LINK_STORE_H

#include "Define.h"
#include "Optional.h"
#include <mutex>
#include <string>
#include <unordered_map>

// bnetserver Discord account-link seam (spec section A).
//
// RE finding (12.1.0.69299): there is NO dedicated Discord BNet RPC service. The complete embedded
// proto-descriptor set was enumerated and contains only `DiscordClientTelemetry.proto` (telemetry,
// not RPC). The account link is transported over EXISTING Battle.net plumbing:
//   * bgs.protocol.presence.v2.PresenceExternalIdentity {unique_id, display_id}
//         -> Discord user id + display name published as an external identity on BNet presence
//   * OPTIN_TYPE_ID_GAME_DATA_SHARING_CONSENT_DISCORD  -> the account opt-in that gates it
//   * the client server/channel list rides the generic GameUtilities attribute channel
//         -> the exact attribute keys are PAST THE OFFLINE CEILING (the client Discord manager is
//            un-instantiated in the dump, so the naming call-sites are unreachable; with no Discord
//            .proto there is nothing to decode). They must come from a real 12.1 bnet capture.
//
// Accordingly this store is a clean holder for per-account Discord identity that a future 12.1 wire
// implementation reads from when it publishes the presence external identity / answers the attribute
// queries. It is config-gated and empty by default (returns "not linked"). No wire is invented here.

namespace Battlenet
{
    // Mirrors the client Enum.DiscordAccountType (registrar RVA 0xE3F030): Normal=0, Provisional=1.
    enum class DiscordAccountType : uint8
    {
        Normal      = 0,
        Provisional = 1
    };

    struct DiscordLink
    {
        uint64 DiscordUserId = 0;                                // OpaqueDiscordUserID (64-bit)
        std::string DiscordUserName;
        DiscordAccountType AccountType = DiscordAccountType::Normal;
    };

    class DiscordLinkStore
    {
    public:
        static DiscordLinkStore* instance();

        // Reads BattlenetDiscord.* config. Logic stays inert while disabled.
        void LoadConfig();

        bool IsEnabled() const { return _enabled; }

        // Returns the linked Discord identity for a BNet account, if any. Empty by default until an
        // external linker populates it via SetLink (the presence-external-identity seam).
        Optional<DiscordLink> GetLinkForAccount(uint32 bnetAccountId) const;

        // Seam an external account-linker uses to register/refresh a Discord identity. This is the
        // server-state source the presence v2 ExternalIdentity publish path would read from.
        void SetLink(uint32 bnetAccountId, DiscordLink link);
        void RemoveLink(uint32 bnetAccountId);

    private:
        DiscordLinkStore() = default;

        bool _enabled = false;                                  // BattlenetDiscord.Enabled
        mutable std::mutex _mutex;
        mutable std::unordered_map<uint32, DiscordLink> _links;
    };
}

#define sDiscordLinkStore Battlenet::DiscordLinkStore::instance()

#endif // TRINITYCORE_BNET_DISCORD_LINK_STORE_H
