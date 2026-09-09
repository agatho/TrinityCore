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

#include "ListfileMap.h"

#include <charconv>
#include <fstream>
#include <sstream>

namespace Road
{
    void ListfileMap::Clear()
    {
        _entries.clear();
    }

    void ListfileMap::Insert(uint32 fileDataId, std::string path)
    {
        _entries[fileDataId] = std::move(path);
    }

    std::optional<std::string_view> ListfileMap::Lookup(uint32 fileDataId) const
    {
        auto it = _entries.find(fileDataId);
        if (it == _entries.end())
            return std::nullopt;
        return std::string_view(it->second);
    }

    namespace
    {
        // Trim ASCII whitespace + \r from both ends in place.
        std::string_view Trim(std::string_view s)
        {
            std::size_t b = 0;
            while (b < s.size() &&
                   (s[b] == ' ' || s[b] == '\t' || s[b] == '\r' || s[b] == '\n'))
                ++b;
            std::size_t e = s.size();
            while (e > b &&
                   (s[e - 1] == ' ' || s[e - 1] == '\t' ||
                    s[e - 1] == '\r' || s[e - 1] == '\n'))
                --e;
            return s.substr(b, e - b);
        }

        // Parse a single line. Accepts comma OR semicolon delimiter. Returns
        // false if the line is empty, a comment, or malformed.
        bool ParseLine(std::string_view rawLine, uint32& outId,
                       std::string& outPath)
        {
            std::string_view line = Trim(rawLine);
            if (line.empty() || line.front() == '#')
                return false;

            // Find first separator (',' or ';').
            std::size_t sep = std::string_view::npos;
            for (std::size_t i = 0; i < line.size(); ++i)
            {
                if (line[i] == ',' || line[i] == ';')
                {
                    sep = i;
                    break;
                }
            }
            if (sep == std::string_view::npos)
                return false;

            std::string_view idStr = Trim(line.substr(0, sep));
            std::string_view path  = Trim(line.substr(sep + 1));
            if (idStr.empty() || path.empty())
                return false;

            uint32 id = 0;
            auto* first = idStr.data();
            auto* last  = idStr.data() + idStr.size();
            auto r = std::from_chars(first, last, id);
            if (r.ec != std::errc{} || r.ptr != last)
                return false;

            outId = id;
            outPath = path;
            return true;
        }
    }

    std::size_t ListfileMap::ParseFromString(std::string_view content,
                                              std::vector<std::string>* failedLines)
    {
        std::size_t ok = 0;
        std::size_t lineNo = 0;
        std::size_t pos = 0;
        while (pos < content.size())
        {
            ++lineNo;
            std::size_t end = content.find('\n', pos);
            std::string_view rawLine = (end == std::string_view::npos)
                ? content.substr(pos)
                : content.substr(pos, end - pos);
            pos = (end == std::string_view::npos) ? content.size() : end + 1;

            uint32 id;
            std::string path;
            if (ParseLine(rawLine, id, path))
            {
                _entries[id] = std::move(path);
                ++ok;
            }
            else
            {
                // Empty/comment lines are silent; only structural failures
                // record into failedLines.
                std::string_view trimmed = Trim(rawLine);
                if (!trimmed.empty() && trimmed.front() != '#' && failedLines)
                {
                    failedLines->push_back(
                        "line " + std::to_string(lineNo) + ": '" +
                        std::string(trimmed) + "'");
                }
            }
        }
        return ok;
    }

    bool ListfileMap::LoadFromFile(std::string const& path,
                                    std::vector<std::string>* failedLines)
    {
        std::ifstream f(path, std::ios::binary);
        if (!f)
            return false;
        std::stringstream ss;
        ss << f.rdbuf();
        std::string content = ss.str();
        ParseFromString(content, failedLines);
        return true;
    }
}
