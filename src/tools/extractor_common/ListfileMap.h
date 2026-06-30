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

#ifndef TRINITYCORE_LISTFILE_MAP_H
#define TRINITYCORE_LISTFILE_MAP_H

#include "Define.h"

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Road
{
    // FileDataID → BLP/file path lookup, sourced from the community
    // `wow-listfile` project (https://github.com/wowdev/wow-listfile).
    //
    // Modern WoW client data (Legion+) references textures in ADT files via
    // FileDataIDs (MDID chunk) rather than filename strings (the older MTEX
    // chunk). Without a listfile, our classifier sees `[FDID:N]` strings
    // and rejects every road texture in modern zones — silently producing
    // empty road masks for everything after Cataclysm.
    //
    // The listfile is text — CSV with either `,` or `;` delimiter,
    // one record per line:
    //
    //     <fileDataId><sep><path>
    //
    // Example:
    //     1532530,tileset/expansion10/11ea_road01_1024.blp
    //
    // Comments (`#`) and empty lines are ignored. Paths are NOT validated
    // — whatever's in the listfile is what we return.
    //
    // Memory cost: the community listfile has ~2.17M entries; loading the
    // whole thing into a hash table is ~150 MB. For the road workflow we
    // recommend filtering to just `tileset/` entries before loading
    // (~8K entries, ~1 MB). The parser doesn't do that filtering — it's
    // the caller's responsibility to provide a pre-filtered file.
    class ListfileMap
    {
    public:
        ListfileMap() = default;

        // Reset the table to empty.
        void Clear();

        // Total entry count.
        std::size_t Size() const { return _entries.size(); }
        bool Empty() const { return _entries.empty(); }

        // Insert/update a single mapping. Used by the parser + tests.
        void Insert(uint32 fileDataId, std::string path);

        // Look up a FileDataID. Returns nullopt if not present.
        std::optional<std::string_view> Lookup(uint32 fileDataId) const;

        // Parse a listfile from a string of CSV/SSV-like content.
        // Records `failedLines` warnings in the optional output vector for
        // any malformed rows. Returns count of successfully-parsed entries.
        std::size_t ParseFromString(std::string_view content,
                                     std::vector<std::string>* failedLines = nullptr);

        // Load + parse a file from disk. Returns false on open failure.
        // Parse-failures are reported via `failedLines` (if provided) and
        // do NOT cause the function to fail — partial loads succeed.
        bool LoadFromFile(std::string const& path,
                           std::vector<std::string>* failedLines = nullptr);

    private:
        std::unordered_map<uint32, std::string> _entries;
    };
}

#endif // TRINITYCORE_LISTFILE_MAP_H
