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

#include "tc_catch2.h"

#include "ListfileMap.h"

#include <string>
#include <vector>

using namespace Road;

// =============================================================================
// Basic operations
// =============================================================================

TEST_CASE("ListfileMap - default-constructed is empty", "[ListfileMap]")
{
    ListfileMap m;
    REQUIRE(m.Empty());
    REQUIRE(m.Size() == 0);
    REQUIRE_FALSE(m.Lookup(0).has_value());
    REQUIRE_FALSE(m.Lookup(123456).has_value());
}

TEST_CASE("ListfileMap - Insert + Lookup round-trip", "[ListfileMap]")
{
    ListfileMap m;
    m.Insert(1532530, "tileset/expansion10/11ea_road01_1024.blp");
    m.Insert(0, "");

    REQUIRE(m.Size() == 2);
    REQUIRE(m.Lookup(1532530).has_value());
    REQUIRE(*m.Lookup(1532530) == "tileset/expansion10/11ea_road01_1024.blp");
    REQUIRE(m.Lookup(0).has_value());
    REQUIRE(*m.Lookup(0) == "");
}

TEST_CASE("ListfileMap - Insert with same id overwrites", "[ListfileMap]")
{
    ListfileMap m;
    m.Insert(42, "old.blp");
    m.Insert(42, "new.blp");
    REQUIRE(m.Size() == 1);
    REQUIRE(*m.Lookup(42) == "new.blp");
}

TEST_CASE("ListfileMap - Clear empties the table", "[ListfileMap]")
{
    ListfileMap m;
    m.Insert(1, "a");
    m.Insert(2, "b");
    REQUIRE(m.Size() == 2);
    m.Clear();
    REQUIRE(m.Empty());
}

// =============================================================================
// CSV parsing
// =============================================================================

TEST_CASE("ListfileMap - parse simple comma-separated lines", "[ListfileMap]")
{
    ListfileMap m;
    std::string content =
        "100,tileset/elwynn/cobble.blp\n"
        "200,tileset/durotar/road.blp\n"
        "300,tileset/expansion10/11ea_road01_1024.blp\n";
    std::vector<std::string> failed;
    REQUIRE(m.ParseFromString(content, &failed) == 3);
    REQUIRE(failed.empty());
    REQUIRE(*m.Lookup(100) == "tileset/elwynn/cobble.blp");
    REQUIRE(*m.Lookup(200) == "tileset/durotar/road.blp");
    REQUIRE(*m.Lookup(300) == "tileset/expansion10/11ea_road01_1024.blp");
}

TEST_CASE("ListfileMap - parse semicolon-separated lines", "[ListfileMap]")
{
    ListfileMap m;
    std::string content =
        "100;tileset/a.blp\n"
        "200;tileset/b.blp\n";
    REQUIRE(m.ParseFromString(content) == 2);
    REQUIRE(*m.Lookup(100) == "tileset/a.blp");
    REQUIRE(*m.Lookup(200) == "tileset/b.blp");
}

TEST_CASE("ListfileMap - mixed comma + semicolon in same file",
          "[ListfileMap]")
{
    ListfileMap m;
    std::string content =
        "100,tileset/a.blp\n"
        "200;tileset/b.blp\n"
        "300,tileset/c.blp\n";
    REQUIRE(m.ParseFromString(content) == 3);
    REQUIRE(*m.Lookup(100) == "tileset/a.blp");
    REQUIRE(*m.Lookup(200) == "tileset/b.blp");
    REQUIRE(*m.Lookup(300) == "tileset/c.blp");
}

TEST_CASE("ListfileMap - skip empty lines + comments", "[ListfileMap]")
{
    ListfileMap m;
    std::string content =
        "# Community listfile, generated 2026-05-20\n"
        "\n"
        "100,tileset/a.blp\n"
        "\n"
        "# Another comment\n"
        "200,tileset/b.blp\n"
        "\n";
    std::vector<std::string> failed;
    REQUIRE(m.ParseFromString(content, &failed) == 2);
    REQUIRE(failed.empty());
}

TEST_CASE("ListfileMap - whitespace trimmed around fields", "[ListfileMap]")
{
    ListfileMap m;
    std::string content =
        "  100  ,   tileset/a.blp   \n"
        "\t200\t;\ttileset/b.blp\t\n";
    REQUIRE(m.ParseFromString(content) == 2);
    REQUIRE(*m.Lookup(100) == "tileset/a.blp");
    REQUIRE(*m.Lookup(200) == "tileset/b.blp");
}

TEST_CASE("ListfileMap - CRLF line endings tolerated", "[ListfileMap]")
{
    ListfileMap m;
    std::string content =
        "100,tileset/a.blp\r\n"
        "200,tileset/b.blp\r\n";
    REQUIRE(m.ParseFromString(content) == 2);
    REQUIRE(*m.Lookup(100) == "tileset/a.blp");
    REQUIRE(*m.Lookup(200) == "tileset/b.blp");
}

TEST_CASE("ListfileMap - malformed rows recorded as failures",
          "[ListfileMap]")
{
    ListfileMap m;
    std::string content =
        "100,tileset/a.blp\n"
        "no_separator_here\n"
        "abc,not_a_number_id_first_field\n"
        "200,tileset/b.blp\n"
        ",empty_id\n"
        "300,\n";   // empty path
    std::vector<std::string> failed;
    REQUIRE(m.ParseFromString(content, &failed) == 2);
    REQUIRE(failed.size() == 4);
    REQUIRE(*m.Lookup(100) == "tileset/a.blp");
    REQUIRE(*m.Lookup(200) == "tileset/b.blp");
}

TEST_CASE("ListfileMap - path may contain commas (greedy split on first sep)",
          "[ListfileMap]")
{
    ListfileMap m;
    // Path with internal commas — our parser only splits on the FIRST
    // separator, so commas later in the path are preserved.
    std::string content = "100,tileset/some,weird,path.blp\n";
    REQUIRE(m.ParseFromString(content) == 1);
    REQUIRE(*m.Lookup(100) == "tileset/some,weird,path.blp");
}

TEST_CASE("ListfileMap - large IDs (32-bit boundary)", "[ListfileMap]")
{
    ListfileMap m;
    std::string content =
        "4294967295,tileset/max.blp\n"   // UINT32_MAX
        "0,tileset/zero.blp\n";
    REQUIRE(m.ParseFromString(content) == 2);
    REQUIRE(*m.Lookup(4294967295u) == "tileset/max.blp");
    REQUIRE(*m.Lookup(0) == "tileset/zero.blp");
}

TEST_CASE("ListfileMap - id overflow rejected", "[ListfileMap]")
{
    ListfileMap m;
    std::string content = "4294967296,tileset/over.blp\n";  // > UINT32_MAX
    std::vector<std::string> failed;
    REQUIRE(m.ParseFromString(content, &failed) == 0);
    REQUIRE(failed.size() == 1);
}

TEST_CASE("ListfileMap - LoadFromFile returns false on missing file",
          "[ListfileMap]")
{
    ListfileMap m;
    std::vector<std::string> failed;
    REQUIRE_FALSE(m.LoadFromFile("/nonexistent/path/xxxxxxxxxxxx.csv", &failed));
}

TEST_CASE("ListfileMap - last-write-wins on duplicate IDs in same file",
          "[ListfileMap]")
{
    ListfileMap m;
    std::string content =
        "100,tileset/first.blp\n"
        "100,tileset/second.blp\n"
        "100,tileset/third.blp\n";
    REQUIRE(m.ParseFromString(content) == 3);   // all parsed
    REQUIRE(m.Size() == 1);                      // but only one entry
    REQUIRE(*m.Lookup(100) == "tileset/third.blp");
}
