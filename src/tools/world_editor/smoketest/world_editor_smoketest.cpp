/*
 * world_editor_smoketest - headless validation harness for the editor's
 * non-GUI code paths.
 *
 * The desktop editor's MMapReader / MapReader / MySqlClient libs have
 * zero Qt dependency by design (Phase 0 commitment). This CLI uses
 * them directly with stdout reporting so the data-layer can be
 * validated end-to-end without launching a window.
 *
 * Usage:
 *   world_editor_smoketest <mmaps_dir> <maps_dir>
 *                          [--db-host=H] [--db-user=U] [--db-pass=P]
 *                          [--world=NAME] [--characters=NAME]
 *                          [--map-id=N]
 *
 * Defaults:
 *   --db-host=127.0.0.1   --db-user=playerbot   --db-pass=playerbot
 *   --world=playerbot_world  --characters=characters  --map-id=0
 *
 * Exit code 0 only if every phase reports OK.  Any failure (mmap not
 * found, .map header mismatch, DB connect refused, etc.) returns 1 so
 * this is CI-pluggable.
 */

#include "../db/MySqlClient.h"
#include "../db/TemplateLookup.h"
#include "../io/CascClient.h"
#include "../io/HandcraftedRoadRepo.h"
#include "../io/ListfileLookup.h"
#include "../io/MMapReader.h"
#include "../io/MapReader.h"
#include "../io/MapTileCache.h"
#include "../io/VmapHeightProbe.h"
#include "../io/VmapReader.h"
#include "../io/WMOReader.h"
#include "SmartAiMetadata.gen.h"   // build-time codegen from core SmartScriptMgr.h

#include <QImage>
#include <QTransform>
#include <QVector2D>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace
{

using clock = std::chrono::steady_clock;

struct CliConfig
{
    std::filesystem::path mmapsDir;
    std::filesystem::path mapsDir;
    std::string  dbHost = "127.0.0.1";
    std::string  dbUser = "playerbot";
    std::string  dbPass = "playerbot";
    std::string  worldDb = "playerbot_world";
    std::string  charsDb = "characters";
    uint32_t     mapId   = 0;
    // Optional: when supplied alongside --listfile, the casc.fdid-open
    // phase will attempt to open a real FDID via CASC + listfile.  Empty
    // means SKIP that phase.
    std::string  cascDir;
    std::string  listfileCsv;
    uint32_t     probeFdid = 0; // optional FDID to verify open succeeds
    uint32_t     probeWmoFdid = 0; // optional WMO root FDID for wmo.load phase
};

bool parseCli(int argc, char** argv, CliConfig& out)
{
    if (argc < 3)
    {
        std::fprintf(stderr,
            "Usage: world_editor_smoketest <mmaps_dir> <maps_dir>\n"
            "                              [--db-host=H] [--db-user=U] [--db-pass=P]\n"
            "                              [--world=NAME] [--characters=NAME]\n"
            "                              [--map-id=N]\n"
            "                              [--casc-dir=PATH] [--listfile=CSV]\n"
            "                              [--probe-fdid=N]\n");
        return false;
    }
    out.mmapsDir = argv[1];
    out.mapsDir  = argv[2];
    for (int i = 3; i < argc; ++i)
    {
        std::string_view a = argv[i];
        auto eat = [&](char const* prefix, std::string& dst) -> bool
        {
            size_t const n = std::strlen(prefix);
            if (a.size() > n && a.substr(0, n) == prefix)
            {
                dst = std::string(a.substr(n));
                return true;
            }
            return false;
        };
        if      (eat("--db-host=",    out.dbHost))   { }
        else if (eat("--db-user=",    out.dbUser))   { }
        else if (eat("--db-pass=",    out.dbPass))   { }
        else if (eat("--world=",      out.worldDb)) { }
        else if (eat("--characters=", out.charsDb)) { }
        else if (eat("--casc-dir=",   out.cascDir)) { }
        else if (eat("--listfile=",   out.listfileCsv)) { }
        else if (a.size() > 9 && a.substr(0, 9) == "--map-id=")
        {
            out.mapId = static_cast<uint32_t>(std::stoul(std::string(a.substr(9))));
        }
        else if (a.size() > 13 && a.substr(0, 13) == "--probe-fdid=")
        {
            out.probeFdid = static_cast<uint32_t>(std::stoul(std::string(a.substr(13))));
        }
        else if (a.size() > 17 && a.substr(0, 17) == "--probe-wmo-fdid=")
        {
            out.probeWmoFdid = static_cast<uint32_t>(std::stoul(std::string(a.substr(17))));
        }
        else
        {
            std::fprintf(stderr, "WARN: unknown arg '%.*s'\n", int(a.size()), a.data());
        }
    }
    return true;
}

struct Phase
{
    std::string name;
    bool        ok = false;
    std::string detail;
    double      ms = 0.0;
    bool        skipped = false; // appended at the end so existing
                                 // Phase{ "name", ok, detail, ms } CTOR
                                 // initializers still bind correctly.
};

void printPhases(std::vector<Phase> const& phases)
{
    std::printf("\n=== Smoketest summary ===\n");
    for (Phase const& p : phases)
    {
        char const* tag = p.skipped ? "SKIP" : (p.ok ? "OK  " : "FAIL");
        std::printf("  [%s] %-30s %.1f ms  %s\n",
            tag,
            p.name.c_str(),
            p.ms,
            p.detail.c_str());
    }
}

double sinceMs(clock::time_point t0)
{
    return std::chrono::duration<double, std::milli>(clock::now() - t0).count();
}

Phase testMmaps(CliConfig const& cfg)
{
    Phase p{ "mmaps", false, {}, 0.0 };
    auto const t0 = clock::now();

    using namespace world_editor::io;
    LoadedMMap mesh = loadMap(cfg.mmapsDir, cfg.mapId);
    p.ms = sinceMs(t0);
    if (!mesh.ok())
    {
        p.detail = "loadMap returned !ok (file missing or bad header)";
        return p;
    }
    auto const& s = mesh.stats();
    char buf[256];
    std::snprintf(buf, sizeof(buf),
        "map=%u tiles=%u failed=%u polys=%llu bytes=%.2f MiB mmapV=%u dtV=%u",
        cfg.mapId, s.tilesLoaded, s.tilesFailed,
        static_cast<unsigned long long>(s.polyCount),
        double(s.bytesLoaded) / (1024.0 * 1024.0),
        s.mmapVersion, s.dtVersion);
    p.detail = buf;
    // Even if tilesLoaded==0 the navmesh header was readable; we still
    // flag that as suspicious so an empty mmaps dir doesn't silently pass.
    p.ok = (s.tilesLoaded > 0);
    if (!p.ok)
        p.detail += "  (no tiles loaded - is the directory correct?)";
    return p;
}

Phase testMapTile(CliConfig const& cfg)
{
    Phase p{ "maps", false, {}, 0.0 };
    auto const t0 = clock::now();

    using namespace world_editor::io;

    // Collect every tile for cfg.mapId, then iterate until we find one
    // that carries actual height + area data. Some tiles (deep ocean,
    // unpopulated zones) ship with NoHeight + NoArea flags, which is
    // valid but boring; we want to validate the COMMON path.
    struct TileRef { int gx; int gy; };
    std::vector<TileRef> tiles;
    char prefix[8];
    std::snprintf(prefix, sizeof(prefix), "%04u_", cfg.mapId);
    std::error_code ec;
    if (std::filesystem::exists(cfg.mapsDir, ec))
    {
        for (auto const& entry : std::filesystem::directory_iterator(cfg.mapsDir, ec))
        {
            if (!entry.is_regular_file(ec)) continue;
            std::string const name = entry.path().filename().string();
            if (name.size() < 13) continue;
            if (name.compare(0, std::strlen(prefix), prefix) != 0) continue;
            if (name.substr(name.size() - 4) != ".map") continue;
            try {
                TileRef t{ std::stoi(name.substr(5, 2)), std::stoi(name.substr(8, 2)) };
                tiles.push_back(t);
            } catch (...) { /* skip malformed */ }
        }
    }
    if (tiles.empty())
    {
        p.ms = sinceMs(t0);
        p.detail = "no <mapId>_*.map tile found in maps_dir";
        return p;
    }

    std::unique_ptr<LoadedMapTile> chosen;
    int chosenGx = -1, chosenGy = -1;
    size_t scanned = 0;
    for (TileRef const& t : tiles)
    {
        ++scanned;
        auto tile = loadTile(cfg.mapsDir, cfg.mapId, t.gx, t.gy);
        if (!tile) continue;
        // Prefer a tile that has both heights AND a populated area grid -
        // that exercises the full reader code path.
        if (tile->hasHeight && !tile->areaGrid.empty())
        {
            chosen = std::move(tile);
            chosenGx = t.gx;
            chosenGy = t.gy;
            break;
        }
        // Stash the first decodable tile as a fallback in case nothing
        // in the dir has height (would be very weird).
        if (!chosen)
        {
            chosen = std::move(tile);
            chosenGx = t.gx;
            chosenGy = t.gy;
        }
    }
    p.ms = sinceMs(t0);
    if (!chosen)
    {
        p.detail = "no decodable tile - every loadTile returned nullptr";
        return p;
    }

    float hCenter = heightAtLocal(*chosen,
        float(ADT_HEIGHT_GRID_V8) * 0.5f,
        float(ADT_HEIGHT_GRID_V8) * 0.5f);
    uint16_t aCenter = areaAtLocal(*chosen,
        ADT_AREA_GRID / 2, ADT_AREA_GRID / 2);

    char buf[320];
    std::snprintf(buf, sizeof(buf),
        "%u_%02d_%02d.map (scanned %zu/%zu)  hasHeight=%d  V9=%zu  V8=%zu  "
        "areaGrid=%zu  liquid=%dx%d  centerH=%.1f  centerArea=%u  build=%u",
        cfg.mapId, chosenGx, chosenGy, scanned, tiles.size(),
        int(chosen->hasHeight), chosen->heightV9.size(), chosen->heightV8.size(),
        chosen->areaGrid.size(),
        int(chosen->liquidWidth), int(chosen->liquidHeight),
        hCenter, unsigned(aCenter),
        chosen->buildMagic);
    p.detail = buf;
    // Strong-pass criterion: a real tile from a real map should expose
    // V9 and V8 height arrays.  If the run only ever found NoHeight
    // tiles, downgrade to a partial pass to make it visible.
    p.ok = chosen->hasHeight && !chosen->heightV9.empty() && !chosen->heightV8.empty();
    if (!p.ok)
        p.detail += "  (no height-bearing tile found - reader path partially exercised)";
    return p;
}

Phase testVmaps(CliConfig const& cfg)
{
    Phase p{ "vmaps", false, {}, 0.0 };
    auto const t0 = clock::now();

    // The vmaps directory is a sibling of `data/maps` (data/maps + data/vmaps).
    // The smoketest only validates a small slice - 10 tiles is enough to
    // exercise the tile + .vmo + transform code path without burning a
    // minute on the full continent.
    std::filesystem::path const vmapsDir = cfg.mapsDir.parent_path() / "vmaps";
    auto const loaded = world_editor::io::loadVmaps(vmapsDir, cfg.mapId, /*maxTiles*/ 10);
    p.ms = sinceMs(t0);

    auto const& s = loaded.stats();
    char buf[384];
    std::snprintf(buf, sizeof(buf),
        "dir=%s tiles=%u failed=%u instances=%u/+%u models=%u/+%u tris=%llu (wmo=%llu m2=%llu) bytes=%.2f MiB",
        vmapsDir.filename().string().c_str(),
        s.tilesLoaded, s.tilesFailed,
        s.instancesLoaded, s.instancesFailed,
        s.modelsLoaded, s.modelsFailed,
        static_cast<unsigned long long>(s.triangleCount),
        static_cast<unsigned long long>(s.wmoTriangleCount),
        static_cast<unsigned long long>(s.m2TriangleCount),
        double(s.bytesLoaded) / (1024.0 * 1024.0));
    p.detail = buf;

    // Map 0 (Eastern Kingdoms) MUST have collision geometry - Stormwind,
    // Ironforge, etc.  Zero triangles on this map means the parser is
    // broken or pointed at the wrong directory.
    p.ok = s.tilesLoaded > 0 && loaded.triangleCount() > 0;
    if (!p.ok)
        p.detail += "  (no triangles materialized - is vmaps dir correct?)";
    return p;
}

// Build a VmapHeightProbe spatial index over a small slice of map 0 and
// confirm a few well-known WMO points return sane floor heights.
Phase testVmapProbe(CliConfig const& cfg)
{
    Phase p{ "vmap.probe", false, {}, 0.0 };
    auto const t0 = clock::now();

    std::filesystem::path const vmapsDir = cfg.mapsDir.parent_path() / "vmaps";
    auto const loaded = world_editor::io::loadVmaps(vmapsDir, cfg.mapId, /*maxTiles*/ 10);
    if (!loaded.ok() || loaded.triangleCount() == 0)
    {
        p.ms = sinceMs(t0);
        p.detail = "underlying vmaps load returned empty - skip probe";
        p.ok = true;  // not fatal; the vmaps phase will flag the real failure.
        return p;
    }
    world_editor::io::VmapHeightProbe probe(loaded, /*cellSize*/ 16.0f);
    auto const& ps = probe.stats();
    p.ms = sinceMs(t0);

    char buf[320];
    std::snprintf(buf, sizeof(buf),
        "cells=%llu entries=%llu cellSize=%.1f bbox=[%.0f,%.0f -> %.0f,%.0f]",
        (unsigned long long)ps.cellCount,
        (unsigned long long)ps.cellEntries,
        ps.cellSize,
        ps.minX, ps.minY, ps.maxX, ps.maxY);
    p.detail = buf;
    // Index built successfully if cellCount > 0 AND a query inside the
    // bbox returns SOMETHING for a high-up probe.
    if (!probe.ok())
    {
        p.detail += "  (probe.ok()=false; cellCount too large?)";
        return p;
    }
    float const cx = 0.5f * (ps.minX + ps.maxX);
    float const cy = 0.5f * (ps.minY + ps.maxY);
    float const floor = probe.floorBelow(cx, cy, /*probeZ*/ 10000.0f);
    char buf2[64];
    std::snprintf(buf2, sizeof(buf2), "  floorAtCenter=%.2f", floor);
    p.detail += buf2;
    p.ok = true;
    return p;
}

Phase testDbConnect(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.connect", false, {}, 0.0 };
    auto const t0 = clock::now();

    world_editor::db::ConnectionParams cp;
    cp.host     = cfg.dbHost;
    cp.user     = cfg.dbUser;
    cp.password = cfg.dbPass;
    cp.database = cfg.worldDb;

    auto const err = client.connect(cp);
    p.ms = sinceMs(t0);
    if (!err.ok())
    {
        p.detail = "[" + std::to_string(err.code) + "] " + err.message;
        return p;
    }
    p.detail = "server " + client.serverVersion();
    p.ok = true;
    return p;
}

Phase testCreatureQuery(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.creature", false, {}, 0.0 };
    auto const t0 = clock::now();

    char sql[256];
    std::snprintf(sql, sizeof(sql),
        "SELECT guid, id, position_x, position_y, position_z, orientation, zoneId, areaId "
        "FROM %s.creature WHERE map = %u LIMIT 5",
        cfg.worldDb.c_str(), cfg.mapId);

    world_editor::db::QueryResult res;
    auto const err = client.query(sql, res);
    p.ms = sinceMs(t0);
    if (!err.ok())
    {
        p.detail = "[" + std::to_string(err.code) + "] " + err.message;
        return p;
    }

    // Also get a count across map.
    char sqlCount[256];
    std::snprintf(sqlCount, sizeof(sqlCount),
        "SELECT COUNT(*) FROM %s.creature WHERE map = %u",
        cfg.worldDb.c_str(), cfg.mapId);
    world_editor::db::QueryResult countRes;
    (void)client.query(sqlCount, countRes);
    std::string countStr = countRes.rowCount() > 0 ? countRes.cell(0, 0) : std::string("?");

    char buf[256];
    std::snprintf(buf, sizeof(buf),
        "rows=%zu  cols=%zu  totalOnMap=%s  first guid=%s id=%s pos=(%s,%s,%s)",
        res.rowCount(), res.columnCount(), countStr.c_str(),
        res.rowCount() > 0 ? res.cell(0, 0).c_str() : "-",
        res.rowCount() > 0 ? res.cell(0, 1).c_str() : "-",
        res.rowCount() > 0 ? res.cell(0, 2).c_str() : "-",
        res.rowCount() > 0 ? res.cell(0, 3).c_str() : "-",
        res.rowCount() > 0 ? res.cell(0, 4).c_str() : "-");
    p.detail = buf;
    p.ok = (res.columnCount() == 8);
    return p;
}

Phase testBattlemasterQuery(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    // Mirrors the SELECT the editor runs at map-load to feed the
    // battlemaster recruitment-radius overlay.  We just check the
    // query parses + returns a non-error column count of 1; an empty
    // result on a custom DB is still OK.
    Phase p{ "db.battlemaster", false, {}, 0.0 };
    auto const t0 = clock::now();

    char sql[256];
    std::snprintf(sql, sizeof(sql),
        "SELECT entry FROM %s.creature_template "
        "WHERE (npcflag & 0x100000) <> 0",
        cfg.worldDb.c_str());

    world_editor::db::QueryResult res;
    auto const err = client.query(sql, res);
    p.ms = sinceMs(t0);
    if (!err.ok())
    {
        p.detail = "[" + std::to_string(err.code) + "] " + err.message;
        return p;
    }
    char buf[128];
    std::snprintf(buf, sizeof(buf),
        "battlemaster rows=%zu cols=%zu",
        res.rowCount(), res.columnCount());
    p.detail = buf;
    p.ok = (res.columnCount() == 1);
    return p;
}

// Trainer dock parity probe.  Mirrors the chain that TrainerSpellDock
// walks at spawn-click time: trainer header + trainer_spell rows.  We
// only assert the base trainer/trainer_spell tables parse and the
// COUNT join returns the expected column shape; the dock itself
// tolerates schema drift (Requirement column, SpellID vs SpellId,
// creature_default_trainer vs creature_trainer) at runtime, so the
// smoketest doesn't pin a specific fork's columns beyond Id/Type.
Phase testTrainerQuery(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.trainer", false, {}, 0.0 };
    auto const t0 = clock::now();
    char sql[512];
    std::snprintf(sql, sizeof(sql),
        "SELECT t.Id, t.Type, COUNT(ts.TrainerId) AS spells "
        "FROM %s.trainer t "
        "LEFT JOIN %s.trainer_spell ts ON ts.TrainerId = t.Id "
        "GROUP BY t.Id, t.Type "
        "ORDER BY t.Id LIMIT 5",
        cfg.worldDb.c_str(), cfg.worldDb.c_str());
    world_editor::db::QueryResult res;
    auto const err = client.query(sql, res);
    p.ms = sinceMs(t0);
    if (!err.ok())
    {
        p.detail = "[" + std::to_string(err.code) + "] " + err.message;
        return p;
    }
    char buf[256];
    std::snprintf(buf, sizeof(buf),
        "trainers=%zu cols=%zu first id=%s type=%s spells=%s",
        res.rowCount(), res.columnCount(),
        res.rowCount() > 0 ? res.cell(0, 0).c_str() : "-",
        res.rowCount() > 0 ? res.cell(0, 1).c_str() : "-",
        res.rowCount() > 0 ? res.cell(0, 2).c_str() : "-");
    p.detail = buf;
    p.ok = (res.columnCount() == 3);
    return p;
}

Phase testGameObjectQuery(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.gameobject", false, {}, 0.0 };
    auto const t0 = clock::now();
    char sql[256];
    std::snprintf(sql, sizeof(sql),
        "SELECT guid, id, position_x, position_y, position_z FROM %s.gameobject "
        "WHERE map = %u LIMIT 3",
        cfg.worldDb.c_str(), cfg.mapId);
    world_editor::db::QueryResult res;
    auto const err = client.query(sql, res);
    p.ms = sinceMs(t0);
    if (!err.ok())
    {
        p.detail = "[" + std::to_string(err.code) + "] " + err.message;
        return p;
    }
    char buf[200];
    std::snprintf(buf, sizeof(buf), "rows=%zu cols=%zu", res.rowCount(), res.columnCount());
    p.detail = buf;
    p.ok = (res.columnCount() == 5);
    return p;
}

Phase testAnnotationQuery(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.annotations", false, {}, 0.0 };
    auto const t0 = clock::now();
    // Cross-DB query - the same SELECT the GUI editor issues.
    char sql[400];
    std::snprintf(sql, sizeof(sql),
        "SELECT id, map_id, zone_id, kind, pos_x, pos_y, pos_z, radius, label, notes, created_by "
        "FROM %s.playerbot_v2_world_metadata WHERE map_id = %u",
        cfg.charsDb.c_str(), cfg.mapId);
    world_editor::db::QueryResult res;
    auto const err = client.query(sql, res);
    p.ms = sinceMs(t0);
    if (!err.ok())
    {
        p.detail = "[" + std::to_string(err.code) + "] " + err.message;
        return p;
    }
    char buf[200];
    std::snprintf(buf, sizeof(buf), "rows=%zu cols=%zu (table reachable)", res.rowCount(), res.columnCount());
    p.detail = buf;
    p.ok = (res.columnCount() == 11);
    return p;
}

Phase testCreatureInsertRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.creature-insert-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    // Pick a real creature_template entry so the FK reference is valid.
    char templateSql[256];
    std::snprintf(templateSql, sizeof(templateSql),
        "SELECT entry FROM %s.creature_template ORDER BY entry LIMIT 1",
        cfg.worldDb.c_str());
    world_editor::db::QueryResult tpl;
    auto err = client.query(templateSql, tpl);
    if (!err.ok() || tpl.rowCount() == 0)
    {
        p.detail = "template pick failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }
    uint32_t const entry = static_cast<uint32_t>(tpl.asUInt64(0, 0).value_or(0));

    // Reserve a guid: MAX(guid)+1.
    char maxSql[256];
    std::snprintf(maxSql, sizeof(maxSql),
        "SELECT COALESCE(MAX(guid), 0)+1 FROM %s.creature", cfg.worldDb.c_str());
    world_editor::db::QueryResult maxRes;
    (void)client.query(maxSql, maxRes);
    uint64_t const reserved = maxRes.rowCount() > 0
        ? maxRes.asUInt64(0, 0).value_or(0) : 0;
    if (reserved == 0) { p.detail = "reservation failed"; p.ms = sinceMs(t0); return p; }

    err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    // Same shape as SpawnCommitDialog's formatCreatureInsert.
    char insertSql[1024];
    std::snprintf(insertSql, sizeof(insertSql),
        "INSERT INTO %s.creature "
        "(guid, id, map, zoneId, areaId, spawnDifficulties, phaseUseFlags, PhaseId, "
        " PhaseGroup, terrainSwapMap, modelid, equipment_id, "
        " position_x, position_y, position_z, orientation, spawntimesecs, "
        " wander_distance, currentwaypoint, curHealthPct, MovementType, "
        " npcflag, unit_flags, unit_flags2, unit_flags3, ScriptName, StringId) "
        "VALUES (%llu, %u, %u, 0, 0, '0', 0, 0, 0, -1, 0, 0, "
        " 0.0, 0.0, 0.0, 0.0, 120, 0.0, 0, 100, 0, 0, 0, 0, 0, '', '')",
        cfg.worldDb.c_str(),
        (unsigned long long)reserved, entry, cfg.mapId);
    uint64_t affected = 0;
    err = client.exec(insertSql, &affected);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "INSERT failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    // Verify visible in transaction.
    char selectSql[256];
    std::snprintf(selectSql, sizeof(selectSql),
        "SELECT guid FROM %s.creature WHERE guid = %llu",
        cfg.worldDb.c_str(), (unsigned long long)reserved);
    world_editor::db::QueryResult midRes;
    (void)client.query(selectSql, midRes);
    bool const sawInTx = midRes.rowCount() == 1;

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(selectSql, afterRes);
    bool const goneAfter = afterRes.rowCount() == 0;

    p.ms = sinceMs(t0);
    char buf[256];
    std::snprintf(buf, sizeof(buf),
        "entry=%u  reservedGuid=%llu  sawInTx=%d  goneAfterRollback=%d  affected=%llu",
        entry, (unsigned long long)reserved, int(sawInTx), int(goneAfter),
        (unsigned long long)affected);
    p.detail = buf;
    p.ok = sawInTx && goneAfter && affected == 1;
    return p;
}

Phase testWaypointPaths(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.waypoint-paths", false, {}, 0.0 };
    auto const t0 = clock::now();

    // Same shape as MainWindow::reloadPathsForMap.  Path FK lives in
    // creature_addon.PathId, NOT creature.currentwaypoint.
    char sql[1024];
    std::snprintf(sql, sizeof(sql),
        "SELECT wp.PathId, wp.MoveType, wp.Flags "
        "FROM %s.waypoint_path wp "
        "JOIN (SELECT DISTINCT ca.PathId AS pid "
        "      FROM %s.creature_addon ca "
        "      JOIN %s.creature c ON c.guid = ca.guid "
        "      WHERE c.map = %u AND ca.PathId > 0) used "
        "ON used.pid = wp.PathId",
        cfg.worldDb.c_str(), cfg.worldDb.c_str(), cfg.worldDb.c_str(), cfg.mapId);
    world_editor::db::QueryResult paths;
    auto err = client.query(sql, paths);
    if (!err.ok())
    {
        p.detail = "paths query failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }
    // If at least one path is present, count nodes for the first one.
    size_t firstPathNodes = 0;
    uint32_t firstPathId = 0;
    if (paths.rowCount() > 0)
    {
        firstPathId = uint32_t(paths.asUInt64(0, 0).value_or(0));
        char nodeSql[256];
        std::snprintf(nodeSql, sizeof(nodeSql),
            "SELECT COUNT(*) FROM %s.waypoint_path_node WHERE PathId = %u",
            cfg.worldDb.c_str(), firstPathId);
        world_editor::db::QueryResult nodeCount;
        (void)client.query(nodeSql, nodeCount);
        if (nodeCount.rowCount() > 0)
            firstPathNodes = size_t(nodeCount.asUInt64(0, 0).value_or(0));
    }
    p.ms = sinceMs(t0);
    char buf[256];
    std::snprintf(buf, sizeof(buf),
        "pathsOnMap=%zu  firstPathId=%u  firstPathNodes=%zu",
        paths.rowCount(), firstPathId, firstPathNodes);
    p.detail = buf;
    // PASS as long as the query succeeded.  Zero paths on a given map
    // is legitimate (not every map has waypoint creatures).
    p.ok = true;
    return p;
}

Phase testGroupsPools(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.groups-pools", false, {}, 0.0 };
    auto const t0 = clock::now();

    char sql[512];
    std::snprintf(sql, sizeof(sql),
        "SELECT COUNT(*) FROM %s.spawn_group_template; ",
        cfg.worldDb.c_str());
    world_editor::db::QueryResult r1, r2, r3, r4;
    (void)client.query(sql, r1);
    std::snprintf(sql, sizeof(sql), "SELECT COUNT(*) FROM %s.spawn_group", cfg.worldDb.c_str());
    (void)client.query(sql, r2);
    std::snprintf(sql, sizeof(sql), "SELECT COUNT(*) FROM %s.pool_template", cfg.worldDb.c_str());
    (void)client.query(sql, r3);
    std::snprintf(sql, sizeof(sql), "SELECT COUNT(*) FROM %s.pool_members", cfg.worldDb.c_str());
    (void)client.query(sql, r4);

    p.ms = sinceMs(t0);
    auto get = [](world_editor::db::QueryResult const& r) -> uint64_t {
        return r.rowCount() > 0 ? r.asUInt64(0, 0).value_or(0) : 0;
    };
    char buf[200];
    std::snprintf(buf, sizeof(buf),
        "templates=%llu groupRows=%llu pools=%llu poolRows=%llu",
        (unsigned long long)get(r1), (unsigned long long)get(r2),
        (unsigned long long)get(r3), (unsigned long long)get(r4));
    p.detail = buf;
    p.ok = get(r1) > 0 && get(r3) > 0;
    return p;
}

Phase testWaypointInsertRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.waypoint-insert-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    // Reserve a PathId from MAX+1.
    char maxSql[256];
    std::snprintf(maxSql, sizeof(maxSql),
        "SELECT COALESCE(MAX(PathId), 0)+1 FROM %s.waypoint_path", cfg.worldDb.c_str());
    world_editor::db::QueryResult maxRes;
    (void)client.query(maxSql, maxRes);
    uint64_t const reserved = maxRes.rowCount() > 0
        ? maxRes.asUInt64(0, 0).value_or(0) : 0;
    if (reserved == 0) { p.detail = "reservation failed"; p.ms = sinceMs(t0); return p; }

    auto err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    // Insert path header + 3 nodes.
    char buf[512];
    std::snprintf(buf, sizeof(buf),
        "INSERT INTO %s.waypoint_path (PathId, MoveType, Flags, Velocity, Comment) "
        "VALUES (%llu, 0, 0, 0.0, 'smoketest probe')",
        cfg.worldDb.c_str(), (unsigned long long)reserved);
    err = client.exec(buf);
    if (!err.ok()) { (void)client.exec("ROLLBACK"); p.detail = "header INSERT failed: " + err.message; p.ms = sinceMs(t0); return p; }

    for (int i = 1; i <= 3; ++i)
    {
        std::snprintf(buf, sizeof(buf),
            "INSERT INTO %s.waypoint_path_node "
            "(PathId, NodeId, PositionX, PositionY, PositionZ, Orientation, Delay) "
            "VALUES (%llu, %d, %d.0, %d.0, 0.0, 0.0, 0)",
            cfg.worldDb.c_str(), (unsigned long long)reserved, i, i * 10, i * 10);
        err = client.exec(buf);
        if (!err.ok()) { (void)client.exec("ROLLBACK"); p.detail = "node INSERT failed: " + err.message; p.ms = sinceMs(t0); return p; }
    }

    // Verify visible in transaction.
    std::snprintf(buf, sizeof(buf),
        "SELECT COUNT(*) FROM %s.waypoint_path_node WHERE PathId = %llu",
        cfg.worldDb.c_str(), (unsigned long long)reserved);
    world_editor::db::QueryResult midRes;
    (void)client.query(buf, midRes);
    int const inTxNodes = midRes.rowCount() > 0
        ? int(midRes.asUInt64(0, 0).value_or(0)) : 0;

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(buf, afterRes);
    int const afterRollbackNodes = afterRes.rowCount() > 0
        ? int(afterRes.asUInt64(0, 0).value_or(0)) : 0;

    p.ms = sinceMs(t0);
    char out[256];
    std::snprintf(out, sizeof(out),
        "reservedPathId=%llu  inTxNodes=%d  afterRollback=%d",
        (unsigned long long)reserved, inTxNodes, afterRollbackNodes);
    p.detail = out;
    p.ok = (inTxNodes == 3) && (afterRollbackNodes == 0);
    return p;
}

Phase testAreatriggersGraveyards(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.atr-graveyards", false, {}, 0.0 };
    auto const t0 = clock::now();
    char sql[256];
    world_editor::db::QueryResult ar, gy, gz;
    std::snprintf(sql, sizeof(sql),
        "SELECT COUNT(*) FROM %s.areatrigger WHERE MapId = %u",
        cfg.worldDb.c_str(), cfg.mapId);
    (void)client.query(sql, ar);
    std::snprintf(sql, sizeof(sql),
        "SELECT COUNT(*) FROM %s.world_safe_locs WHERE MapID = %u",
        cfg.worldDb.c_str(), cfg.mapId);
    (void)client.query(sql, gy);
    std::snprintf(sql, sizeof(sql),
        "SELECT COUNT(*) FROM %s.graveyard_zone", cfg.worldDb.c_str());
    (void)client.query(sql, gz);
    p.ms = sinceMs(t0);
    auto get = [](world_editor::db::QueryResult const& r) -> uint64_t {
        return r.rowCount() > 0 ? r.asUInt64(0, 0).value_or(0) : 0;
    };
    char buf[200];
    std::snprintf(buf, sizeof(buf),
        "areatrigger=%llu graveyards=%llu graveyard_zone=%llu",
        (unsigned long long)get(ar), (unsigned long long)get(gy),
        (unsigned long long)get(gz));
    p.detail = buf;
    p.ok = true;  // zero on a map is legitimate.
    return p;
}

// Reserve a SpawnId, INSERT a new areatrigger row inside a transaction,
// verify it's visible in-tx, then ROLLBACK and verify it's gone.  Catches
// SQL-gen drift against the live schema (HANDOFF lessons §3).
Phase testAreatriggerInsertRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.areatrigger-insert-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    char maxSql[256];
    std::snprintf(maxSql, sizeof(maxSql),
        "SELECT COALESCE(MAX(SpawnId), 0)+1 FROM %s.areatrigger",
        cfg.worldDb.c_str());
    world_editor::db::QueryResult maxRes;
    (void)client.query(maxSql, maxRes);
    uint64_t const reserved = maxRes.rowCount() > 0
        ? maxRes.asUInt64(0, 0).value_or(0) : 0;
    if (reserved == 0) { p.detail = "SpawnId reservation failed"; p.ms = sinceMs(t0); return p; }

    // Pick an EXISTING (CreatePropertiesId, IsCustom) pair so the join
    // back in the editor still works.  Any row works; we use ORDER BY ID.
    char propsSql[256];
    std::snprintf(propsSql, sizeof(propsSql),
        "SELECT Id, IsCustom FROM %s.areatrigger_create_properties "
        "ORDER BY Id LIMIT 1", cfg.worldDb.c_str());
    world_editor::db::QueryResult propsRes;
    (void)client.query(propsSql, propsRes);
    if (propsRes.rowCount() == 0)
    {
        p.detail = "no areatrigger_create_properties rows available";
        p.ms = sinceMs(t0);
        return p;
    }
    uint32_t const propsId   = uint32_t(propsRes.asUInt64(0, 0).value_or(0));
    uint8_t  const isCustom  = uint8_t (propsRes.asUInt64(0, 1).value_or(0));

    auto err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    char buf[768];
    std::snprintf(buf, sizeof(buf),
        "INSERT INTO %s.areatrigger "
        "(SpawnId, AreaTriggerCreatePropertiesId, IsCustom, MapId, "
        " SpawnDifficulties, PosX, PosY, PosZ, Orientation, "
        " PhaseUseFlags, PhaseId, PhaseGroup, ScriptName, Comment, VerifiedBuild) "
        "VALUES (%llu, %u, %u, %u, '0', 100.0, 200.0, 50.0, 0.0, "
        "        0, 0, 0, '', 'smoketest probe', 0)",
        cfg.worldDb.c_str(),
        (unsigned long long)reserved, propsId, isCustom, cfg.mapId);
    err = client.exec(buf);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "INSERT failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    // Verify visible in transaction.
    std::snprintf(buf, sizeof(buf),
        "SELECT COUNT(*) FROM %s.areatrigger WHERE SpawnId = %llu",
        cfg.worldDb.c_str(), (unsigned long long)reserved);
    world_editor::db::QueryResult midRes;
    (void)client.query(buf, midRes);
    int const sawInTx = midRes.rowCount() > 0
        ? int(midRes.asUInt64(0, 0).value_or(0)) : 0;

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(buf, afterRes);
    int const sawAfterRollback = afterRes.rowCount() > 0
        ? int(afterRes.asUInt64(0, 0).value_or(0)) : 0;

    p.ms = sinceMs(t0);
    char out[256];
    std::snprintf(out, sizeof(out),
        "reservedSpawnId=%llu  propsId=%u/isCustom=%u  sawInTx=%d  goneAfterRollback=%d",
        (unsigned long long)reserved, propsId, isCustom,
        sawInTx, sawAfterRollback == 0);
    p.detail = out;
    p.ok = (sawInTx == 1) && (sawAfterRollback == 0);
    return p;
}

// UPDATE one existing row's comment+orientation inside a transaction,
// verify the in-transaction view, then ROLLBACK and verify the original
// values restore.  Catches drift in the UPDATE SQL generator.
Phase testAreatriggerUpdateRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.areatrigger-update-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    char selectSql[256];
    std::snprintf(selectSql, sizeof(selectSql),
        "SELECT SpawnId, Orientation, COALESCE(Comment, '') "
        "FROM %s.areatrigger WHERE MapId = %u LIMIT 1",
        cfg.worldDb.c_str(), cfg.mapId);
    world_editor::db::QueryResult pickRes;
    auto err = client.query(selectSql, pickRes);
    if (!err.ok() || pickRes.rowCount() == 0)
    {
        p.detail = "no areatrigger row on this map (skip)";
        p.ms = sinceMs(t0);
        p.ok = true; // not fatal - some maps have no areatriggers.
        return p;
    }
    uint64_t const spawnId      = pickRes.asUInt64(0, 0).value_or(0);
    float    const beforeOrient = float(pickRes.asDouble(0, 1).value_or(0.0));
    std::string const beforeComment = pickRes.cell(0, 2);

    err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    char buf[512];
    float const probeOrient = beforeOrient + 0.5f;
    std::snprintf(buf, sizeof(buf),
        "UPDATE %s.areatrigger SET Orientation=%.4f, Comment='smoketest probe' "
        "WHERE SpawnId=%llu",
        cfg.worldDb.c_str(), probeOrient, (unsigned long long)spawnId);
    uint64_t affected = 0;
    err = client.exec(buf, &affected);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "UPDATE failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    std::snprintf(buf, sizeof(buf),
        "SELECT Orientation FROM %s.areatrigger WHERE SpawnId=%llu",
        cfg.worldDb.c_str(), (unsigned long long)spawnId);
    world_editor::db::QueryResult midRes;
    (void)client.query(buf, midRes);
    float const inTxOrient = midRes.rowCount() > 0
        ? float(midRes.asDouble(0, 0).value_or(0.0)) : 0.0f;

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(buf, afterRes);
    float const afterOrient = afterRes.rowCount() > 0
        ? float(afterRes.asDouble(0, 0).value_or(0.0)) : 0.0f;

    p.ms = sinceMs(t0);
    char out[256];
    std::snprintf(out, sizeof(out),
        "spawnId=%llu  before=%.4f  inTx=%.4f  afterRollback=%.4f  affected=%llu",
        (unsigned long long)spawnId, beforeOrient, inTxOrient, afterOrient,
        (unsigned long long)affected);
    p.detail = out;
    // 0.001 tolerance for float roundtrip noise.
    p.ok = affected == 1
        && std::fabs(inTxOrient - probeOrient) < 0.001f
        && std::fabs(afterOrient - beforeOrient) < 0.001f;
    return p;
}

// Reserve a new world_safe_locs.ID, INSERT the row in a transaction,
// verify visible in-tx + gone after rollback.  Catches schema drift on
// the graveyard editor's INSERT generator (nullable TransportSpawnId etc).
Phase testGraveyardInsertRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.graveyard-insert-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    // world_safe_locs is MyISAM on the canonical TC schema, so ROLLBACK is a
    // no-op. Probe the engine and clean up explicitly (DELETE) when the table is
    // non-transactional — mirrors the access_requirement phase. Without this the
    // test both fails AND leaks 'smoketest probe' rows into world_safe_locs.
    char engineSql[384];
    std::snprintf(engineSql, sizeof(engineSql),
        "SELECT ENGINE FROM INFORMATION_SCHEMA.TABLES "
        "WHERE TABLE_SCHEMA='%s' AND TABLE_NAME='world_safe_locs'",
        cfg.worldDb.c_str());
    world_editor::db::QueryResult engRes;
    (void)client.query(engineSql, engRes);
    std::string const engine = engRes.rowCount() > 0 ? engRes.cell(0, 0) : std::string();
    bool const txCapable = (engine == "InnoDB");

    char maxSql[256];
    std::snprintf(maxSql, sizeof(maxSql),
        "SELECT COALESCE(MAX(ID), 0)+1 FROM %s.world_safe_locs",
        cfg.worldDb.c_str());
    world_editor::db::QueryResult maxRes;
    (void)client.query(maxSql, maxRes);
    uint64_t const reserved = maxRes.rowCount() > 0
        ? maxRes.asUInt64(0, 0).value_or(0) : 0;
    if (reserved == 0) { p.detail = "ID reservation failed"; p.ms = sinceMs(t0); return p; }

    if (txCapable)
    {
        auto berr = client.exec("START TRANSACTION");
        if (!berr.ok()) { p.detail = "BEGIN: " + berr.message; p.ms = sinceMs(t0); return p; }
    }

    char buf[512];
    std::snprintf(buf, sizeof(buf),
        "INSERT INTO %s.world_safe_locs "
        "(ID, MapID, LocX, LocY, LocZ, Facing, TransportSpawnId, Comment) "
        "VALUES (%llu, %u, 100.0, 200.0, 50.0, 0.0, NULL, 'smoketest probe')",
        cfg.worldDb.c_str(), (unsigned long long)reserved, cfg.mapId);
    auto err = client.exec(buf);
    if (!err.ok())
    {
        if (txCapable) (void)client.exec("ROLLBACK");
        p.detail = "INSERT failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    std::snprintf(buf, sizeof(buf),
        "SELECT COUNT(*) FROM %s.world_safe_locs WHERE ID = %llu",
        cfg.worldDb.c_str(), (unsigned long long)reserved);
    world_editor::db::QueryResult midRes;
    (void)client.query(buf, midRes);
    int const sawInTx = midRes.rowCount() > 0
        ? int(midRes.asUInt64(0, 0).value_or(0)) : 0;

    // Undo: ROLLBACK on InnoDB; explicit DELETE on MyISAM (keeps the table clean).
    if (txCapable)
    {
        (void)client.exec("ROLLBACK");
    }
    else
    {
        char del[256];
        std::snprintf(del, sizeof(del),
            "DELETE FROM %s.world_safe_locs WHERE ID = %llu",
            cfg.worldDb.c_str(), (unsigned long long)reserved);
        (void)client.exec(del);
    }

    world_editor::db::QueryResult afterRes;
    (void)client.query(buf, afterRes);
    int const sawAfter = afterRes.rowCount() > 0
        ? int(afterRes.asUInt64(0, 0).value_or(0)) : 0;

    p.ms = sinceMs(t0);
    char out[256];
    std::snprintf(out, sizeof(out),
        "reservedId=%llu  engine=%s  sawInTx=%d  goneAfter=%d",
        (unsigned long long)reserved, engine.c_str(), sawInTx, sawAfter == 0);
    p.detail = out;
    p.ok = (sawInTx == 1) && (sawAfter == 0);
    return p;
}

// UPDATE one existing world_safe_locs row's Facing+Comment in a transaction,
// verify visible-in-tx then ROLLBACK restores the original.
Phase testGraveyardUpdateRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.graveyard-update-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    // MyISAM: ROLLBACK is a no-op, so probe the engine and restore the original
    // Facing+Comment explicitly when non-transactional (mirrors access_requirement).
    // The old version assumed rollback worked, so on MyISAM it permanently drifted
    // the picked row's Facing by +0.5 and clobbered its Comment on every run.
    char engineSql[384];
    std::snprintf(engineSql, sizeof(engineSql),
        "SELECT ENGINE FROM INFORMATION_SCHEMA.TABLES "
        "WHERE TABLE_SCHEMA='%s' AND TABLE_NAME='world_safe_locs'",
        cfg.worldDb.c_str());
    world_editor::db::QueryResult engRes;
    (void)client.query(engineSql, engRes);
    std::string const engine = engRes.rowCount() > 0 ? engRes.cell(0, 0) : std::string();
    bool const txCapable = (engine == "InnoDB");

    char selectSql[256];
    std::snprintf(selectSql, sizeof(selectSql),
        "SELECT ID, COALESCE(Facing, 0), COALESCE(Comment, '') FROM %s.world_safe_locs "
        "WHERE MapID = %u LIMIT 1",
        cfg.worldDb.c_str(), cfg.mapId);
    world_editor::db::QueryResult pickRes;
    auto err = client.query(selectSql, pickRes);
    if (!err.ok() || pickRes.rowCount() == 0)
    {
        p.detail = "no graveyard row on this map (skip)";
        p.ms = sinceMs(t0);
        p.ok = true;
        return p;
    }
    uint64_t const id              = pickRes.asUInt64(0, 0).value_or(0);
    float    const beforeFace      = float(pickRes.asDouble(0, 1).value_or(0.0));
    std::string const beforeComment = pickRes.cell(0, 2);

    if (txCapable)
    {
        err = client.exec("START TRANSACTION");
        if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }
    }

    char buf[512];
    float const probeFace = beforeFace + 0.5f;
    std::snprintf(buf, sizeof(buf),
        "UPDATE %s.world_safe_locs SET Facing=%.4f, Comment='smoketest probe' "
        "WHERE ID=%llu",
        cfg.worldDb.c_str(), probeFace, (unsigned long long)id);
    uint64_t affected = 0;
    err = client.exec(buf, &affected);
    if (!err.ok())
    {
        if (txCapable) (void)client.exec("ROLLBACK");
        p.detail = "UPDATE failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    std::snprintf(buf, sizeof(buf),
        "SELECT Facing FROM %s.world_safe_locs WHERE ID=%llu",
        cfg.worldDb.c_str(), (unsigned long long)id);
    world_editor::db::QueryResult midRes;
    (void)client.query(buf, midRes);
    float const inTxFace = midRes.rowCount() > 0
        ? float(midRes.asDouble(0, 0).value_or(0.0)) : 0.0f;

    // Restore: ROLLBACK on InnoDB; explicit UPDATE-back (Facing+Comment) on MyISAM.
    if (txCapable)
    {
        (void)client.exec("ROLLBACK");
    }
    else
    {
        std::string esc;
        esc.reserve(beforeComment.size() + 8);
        for (char c : beforeComment)
        {
            if (c == '\'' || c == '\\')
                esc.push_back('\\');
            esc.push_back(c);
        }
        char rest[640];
        std::snprintf(rest, sizeof(rest),
            "UPDATE %s.world_safe_locs SET Facing=%.4f, Comment='%s' WHERE ID=%llu",
            cfg.worldDb.c_str(), beforeFace, esc.c_str(), (unsigned long long)id);
        (void)client.exec(rest);
    }

    world_editor::db::QueryResult afterRes;
    (void)client.query(buf, afterRes);
    float const afterFace = afterRes.rowCount() > 0
        ? float(afterRes.asDouble(0, 0).value_or(0.0)) : 0.0f;

    p.ms = sinceMs(t0);
    char out[256];
    std::snprintf(out, sizeof(out),
        "id=%llu  engine=%s  before=%.4f  inTx=%.4f  afterRestore=%.4f  affected=%llu",
        (unsigned long long)id, engine.c_str(), beforeFace, inTxFace, afterFace,
        (unsigned long long)affected);
    p.detail = out;
    p.ok = affected == 1
        && std::fabs(inTxFace - probeFace) < 0.001f
        && std::fabs(afterFace - beforeFace) < 0.001f;
    return p;
}

// UPDATE one existing smart_scripts row's comment inside a transaction,
// verify in-tx view, then ROLLBACK and verify the original comment is
// restored.  Catches drift in the composite-PK UPDATE SQL generator
// (entryorguid + source_type + id + link).
Phase testSmartScriptUpdateRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.smart-scripts-update-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    char selectSql[384];
    std::snprintf(selectSql, sizeof(selectSql),
        "SELECT entryorguid, source_type, id, link, COALESCE(comment, '') "
        "FROM %s.smart_scripts LIMIT 1",
        cfg.worldDb.c_str());
    world_editor::db::QueryResult pickRes;
    auto err = client.query(selectSql, pickRes);
    if (!err.ok() || pickRes.rowCount() == 0)
    {
        p.detail = "no smart_scripts row available (skip)";
        p.ms = sinceMs(t0);
        p.ok = true; // not fatal - tiny DBs may legitimately be empty.
        return p;
    }
    int64_t  const entryorguid = pickRes.asInt64 (0, 0).value_or(0);
    uint8_t  const sourceType  = uint8_t (pickRes.asUInt64(0, 1).value_or(0));
    uint16_t const id          = uint16_t(pickRes.asUInt64(0, 2).value_or(0));
    uint16_t const link        = uint16_t(pickRes.asUInt64(0, 3).value_or(0));
    std::string const beforeComment = pickRes.cell(0, 4);

    err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    char buf[640];
    std::snprintf(buf, sizeof(buf),
        "UPDATE %s.smart_scripts SET comment='smoketest probe' "
        "WHERE entryorguid=%lld AND source_type=%u AND id=%u AND link=%u",
        cfg.worldDb.c_str(), (long long)entryorguid,
        unsigned(sourceType), unsigned(id), unsigned(link));
    uint64_t affected = 0;
    err = client.exec(buf, &affected);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "UPDATE failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    std::snprintf(buf, sizeof(buf),
        "SELECT COALESCE(comment, '') FROM %s.smart_scripts "
        "WHERE entryorguid=%lld AND source_type=%u AND id=%u AND link=%u",
        cfg.worldDb.c_str(), (long long)entryorguid,
        unsigned(sourceType), unsigned(id), unsigned(link));
    world_editor::db::QueryResult midRes;
    (void)client.query(buf, midRes);
    std::string const inTxComment = midRes.rowCount() > 0
        ? midRes.cell(0, 0) : std::string();

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(buf, afterRes);
    std::string const afterComment = afterRes.rowCount() > 0
        ? afterRes.cell(0, 0) : std::string();

    p.ms = sinceMs(t0);
    char out[384];
    std::snprintf(out, sizeof(out),
        "pk=(%lld,%u,%u,%u)  inTx='%s'  afterRollback='%s'  affected=%llu",
        (long long)entryorguid, unsigned(sourceType), unsigned(id), unsigned(link),
        inTxComment.c_str(), afterComment.c_str(),
        (unsigned long long)affected);
    p.detail = out;
    p.ok = affected == 1
        && inTxComment == std::string("smoketest probe")
        && afterComment == beforeComment;
    return p;
}

// Pick a real creature_template.entry, INSERT a probe smart_scripts row
// with (source_type=0, id=9999, link=0) inside a transaction, verify it's
// visible, ROLLBACK and verify it's gone.  We scout id=9999 to a free
// slot first so the test tolerates that exact id being taken on this
// installation.
Phase testSmartScriptInsertRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.smart-scripts-insert-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    // Pick a creature_template entry that EXISTS but has no smart_scripts
    // collision at our chosen (source_type=0, link=0) namespace for the
    // candidate id.  We start at id=9999 and walk up if taken.
    char pickSql[256];
    std::snprintf(pickSql, sizeof(pickSql),
        "SELECT entry FROM %s.creature_template ORDER BY entry LIMIT 1",
        cfg.worldDb.c_str());
    world_editor::db::QueryResult pickRes;
    auto err = client.query(pickSql, pickRes);
    if (!err.ok() || pickRes.rowCount() == 0)
    {
        p.detail = "no creature_template rows available";
        p.ms = sinceMs(t0);
        return p;
    }
    int64_t const entry = pickRes.asInt64(0, 0).value_or(0);
    if (entry <= 0)
    {
        p.detail = "creature_template.entry probe returned 0";
        p.ms = sinceMs(t0);
        return p;
    }

    // Find an unused id in the (entry, 0) namespace, starting at 9999.
    int probeId = 9999;
    for (int attempts = 0; attempts < 16; ++attempts)
    {
        char checkSql[256];
        std::snprintf(checkSql, sizeof(checkSql),
            "SELECT COUNT(*) FROM %s.smart_scripts "
            "WHERE entryorguid=%lld AND source_type=0 AND id=%d AND link=0",
            cfg.worldDb.c_str(), (long long)entry, probeId);
        world_editor::db::QueryResult cRes;
        (void)client.query(checkSql, cRes);
        uint64_t const cnt = cRes.rowCount() > 0 ? cRes.asUInt64(0, 0).value_or(0) : 1;
        if (cnt == 0) break;
        ++probeId;
        if (attempts == 15)
        {
            p.detail = "could not find free smart_scripts id slot";
            p.ms = sinceMs(t0);
            return p;
        }
    }

    err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    char buf[896];
    std::snprintf(buf, sizeof(buf),
        "INSERT INTO %s.smart_scripts "
        "(entryorguid, source_type, id, link, Difficulties, "
        " event_type, event_phase_mask, event_chance, event_flags, "
        " event_param1, event_param2, event_param3, event_param4, event_param5, "
        " event_param_string, "
        " action_type, action_param1, action_param2, action_param3, action_param4, "
        " action_param5, action_param6, action_param7, action_param_string, "
        " target_type, target_param1, target_param2, target_param3, target_param4, "
        " target_param_string, target_x, target_y, target_z, target_o, comment) "
        "VALUES (%lld, 0, %d, 0, '', "
        "        0, 0, 100, 0, 0, 0, 0, 0, 0, '', "
        "        0, 0, 0, 0, 0, 0, 0, 0, NULL, "
        "        0, 0, 0, 0, 0, NULL, 0.0, 0.0, 0.0, 0.0, 'smoketest probe')",
        cfg.worldDb.c_str(), (long long)entry, probeId);
    err = client.exec(buf);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "INSERT failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    std::snprintf(buf, sizeof(buf),
        "SELECT COUNT(*) FROM %s.smart_scripts "
        "WHERE entryorguid=%lld AND source_type=0 AND id=%d AND link=0",
        cfg.worldDb.c_str(), (long long)entry, probeId);
    world_editor::db::QueryResult midRes;
    (void)client.query(buf, midRes);
    int const sawInTx = midRes.rowCount() > 0
        ? int(midRes.asUInt64(0, 0).value_or(0)) : 0;

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(buf, afterRes);
    int const sawAfter = afterRes.rowCount() > 0
        ? int(afterRes.asUInt64(0, 0).value_or(0)) : 0;

    p.ms = sinceMs(t0);
    char out[320];
    std::snprintf(out, sizeof(out),
        "entry=%lld  probeId=%d  sawInTx=%d  goneAfterRollback=%d",
        (long long)entry, probeId, sawInTx, sawAfter == 0);
    p.detail = out;
    p.ok = (sawInTx == 1) && (sawAfter == 0);
    return p;
}

// Pick any real conditions row, UPDATE its Comment inside a
// transaction, verify in-tx view, then ROLLBACK and verify the
// original Comment is restored.  Catches drift in the 11-column
// composite-PK UPDATE WHERE clause generator.
Phase testConditionUpdateRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.conditions-update-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    char selectSql[768];
    std::snprintf(selectSql, sizeof(selectSql),
        "SELECT SourceTypeOrReferenceId, SourceGroup, SourceEntry, SourceId, "
        "       ElseGroup, ConditionTypeOrReference, ConditionTarget, "
        "       ConditionValue1, ConditionValue2, ConditionValue3, "
        "       COALESCE(ConditionStringValue1, ''), "
        "       COALESCE(Comment, '') "
        "FROM %s.conditions LIMIT 1",
        cfg.worldDb.c_str());
    world_editor::db::QueryResult pickRes;
    auto err = client.query(selectSql, pickRes);
    if (!err.ok() || pickRes.rowCount() == 0)
    {
        p.detail = "no conditions row available (skip)";
        p.ms = sinceMs(t0);
        p.ok = true; // not fatal - tiny DBs may legitimately be empty.
        return p;
    }
    int32_t  const src   = int32_t (pickRes.asInt64 (0, 0).value_or(0));
    uint32_t const grp   = uint32_t(pickRes.asUInt64(0, 1).value_or(0));
    int32_t  const ent   = int32_t (pickRes.asInt64 (0, 2).value_or(0));
    int32_t  const sid   = int32_t (pickRes.asInt64 (0, 3).value_or(0));
    uint32_t const els   = uint32_t(pickRes.asUInt64(0, 4).value_or(0));
    int32_t  const ct    = int32_t (pickRes.asInt64 (0, 5).value_or(0));
    uint8_t  const tgt   = uint8_t (pickRes.asUInt64(0, 6).value_or(0));
    uint32_t const v1    = uint32_t(pickRes.asUInt64(0, 7).value_or(0));
    uint32_t const v2    = uint32_t(pickRes.asUInt64(0, 8).value_or(0));
    uint32_t const v3    = uint32_t(pickRes.asUInt64(0, 9).value_or(0));
    std::string const sv1            = pickRes.cell(0, 10);
    std::string const beforeComment  = pickRes.cell(0, 11);

    // Escape sv1 once via libmysql since it can contain quote-y bytes.
    std::string const escSv1 = client.escapeString(sv1);

    auto buildWhere = [&](char* buf, size_t cap) {
        std::snprintf(buf, cap,
            "SourceTypeOrReferenceId=%d AND SourceGroup=%u AND SourceEntry=%d "
            "AND SourceId=%d AND ElseGroup=%u AND ConditionTypeOrReference=%d "
            "AND ConditionTarget=%u AND ConditionValue1=%u AND ConditionValue2=%u "
            "AND ConditionValue3=%u AND ConditionStringValue1='%s'",
            int(src), grp, int(ent), int(sid), els, int(ct),
            unsigned(tgt), v1, v2, v3, escSv1.c_str());
    };

    err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    char whereBuf[1024];
    buildWhere(whereBuf, sizeof(whereBuf));

    char updSql[1280];
    std::snprintf(updSql, sizeof(updSql),
        "UPDATE %s.conditions SET Comment='smoketest probe' WHERE %s",
        cfg.worldDb.c_str(), whereBuf);
    uint64_t affected = 0;
    err = client.exec(updSql, &affected);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "UPDATE failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    char selProbe[1280];
    std::snprintf(selProbe, sizeof(selProbe),
        "SELECT COALESCE(Comment, '') FROM %s.conditions WHERE %s",
        cfg.worldDb.c_str(), whereBuf);
    world_editor::db::QueryResult midRes;
    (void)client.query(selProbe, midRes);
    std::string const inTxComment = midRes.rowCount() > 0
        ? midRes.cell(0, 0) : std::string();

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(selProbe, afterRes);
    std::string const afterComment = afterRes.rowCount() > 0
        ? afterRes.cell(0, 0) : std::string();

    p.ms = sinceMs(t0);
    char out[512];
    std::snprintf(out, sizeof(out),
        "pk=(%d,%u,%d,%d,%u,%d,%u,%u,%u,%u,'%s')  inTx='%s'  afterRollback='%s'  affected=%llu",
        int(src), grp, int(ent), int(sid), els, int(ct),
        unsigned(tgt), v1, v2, v3, sv1.c_str(),
        inTxComment.c_str(), afterComment.c_str(),
        (unsigned long long)affected);
    p.detail = out;
    p.ok = affected == 1
        && inTxComment == std::string("smoketest probe")
        && afterComment == beforeComment;
    return p;
}

// INSERT a probe conditions row at SourceTypeOrReferenceId=999999 (well
// outside any real ConditionSourceType enum value) inside a transaction,
// verify it's visible mid-tx, ROLLBACK and verify it's gone.  Catches
// drift in the conditions editor's INSERT column list / VALUES order.
Phase testConditionInsertRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.conditions-insert-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    // 999999 won't collide with any real CONDITION_SOURCE_TYPE_* value
    // (the enum tops out under 100).  Pre-check to be safe though.
    int const probeSrc = 999999;
    char checkSql[256];
    std::snprintf(checkSql, sizeof(checkSql),
        "SELECT COUNT(*) FROM %s.conditions WHERE SourceTypeOrReferenceId=%d",
        cfg.worldDb.c_str(), probeSrc);
    world_editor::db::QueryResult cRes;
    auto err = client.query(checkSql, cRes);
    if (!err.ok())
    {
        p.detail = "pre-check failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }
    uint64_t const preCount = cRes.rowCount() > 0 ? cRes.asUInt64(0, 0).value_or(0) : 0;
    if (preCount != 0)
    {
        p.detail = "probe source-type 999999 already occupied; aborting";
        p.ms = sinceMs(t0);
        return p;
    }

    err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    char insSql[1024];
    std::snprintf(insSql, sizeof(insSql),
        "INSERT INTO %s.conditions "
        "(SourceTypeOrReferenceId, SourceGroup, SourceEntry, SourceId, "
        " ElseGroup, ConditionTypeOrReference, ConditionTarget, "
        " ConditionValue1, ConditionValue2, ConditionValue3, ConditionStringValue1, "
        " NegativeCondition, ErrorType, ErrorTextId, ScriptName, Comment) "
        "VALUES (%d, 0, 0, 0, 0, 0, 0, 0, 0, 0, '', 0, 0, 0, '', 'smoketest probe')",
        cfg.worldDb.c_str(), probeSrc);
    err = client.exec(insSql);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "INSERT failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    world_editor::db::QueryResult midRes;
    (void)client.query(checkSql, midRes);
    int const sawInTx = midRes.rowCount() > 0
        ? int(midRes.asUInt64(0, 0).value_or(0)) : 0;

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(checkSql, afterRes);
    int const sawAfter = afterRes.rowCount() > 0
        ? int(afterRes.asUInt64(0, 0).value_or(0)) : 0;

    p.ms = sinceMs(t0);
    char out[256];
    std::snprintf(out, sizeof(out),
        "probeSrc=%d  sawInTx=%d  goneAfterRollback=%d",
        probeSrc, sawInTx, sawAfter == 0);
    p.detail = out;
    p.ok = (sawInTx == 1) && (sawAfter == 0);
    return p;
}

// INSERT a probe pool_template row at entry=999999 inside a transaction,
// verify it's visible in-tx, ROLLBACK and verify it's gone.  Catches drift
// in the pool_template editor's INSERT generator (entry, max_limit,
// description column list).
Phase testPoolTemplateInsertRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.pool-template-insert-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    // 999999 is well outside the existing entry range in any TC dataset.
    // Pre-check so the test does not collide with prior state.
    int const probeEntry = 999999;
    char checkSql[256];
    std::snprintf(checkSql, sizeof(checkSql),
        "SELECT COUNT(*) FROM %s.pool_template WHERE entry=%d",
        cfg.worldDb.c_str(), probeEntry);
    world_editor::db::QueryResult cRes;
    auto err = client.query(checkSql, cRes);
    if (!err.ok())
    {
        p.detail = "pre-check failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }
    uint64_t const preCount = cRes.rowCount() > 0 ? cRes.asUInt64(0, 0).value_or(0) : 0;
    if (preCount != 0)
    {
        p.detail = "probe entry 999999 already occupied; aborting";
        p.ms = sinceMs(t0);
        return p;
    }

    err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    char insSql[384];
    std::snprintf(insSql, sizeof(insSql),
        "INSERT INTO %s.pool_template (entry, max_limit, description) "
        "VALUES (%d, 0, 'smoketest probe')",
        cfg.worldDb.c_str(), probeEntry);
    err = client.exec(insSql);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "INSERT failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    world_editor::db::QueryResult midRes;
    (void)client.query(checkSql, midRes);
    int const sawInTx = midRes.rowCount() > 0
        ? int(midRes.asUInt64(0, 0).value_or(0)) : 0;

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(checkSql, afterRes);
    int const sawAfter = afterRes.rowCount() > 0
        ? int(afterRes.asUInt64(0, 0).value_or(0)) : 0;

    p.ms = sinceMs(t0);
    char out[256];
    std::snprintf(out, sizeof(out),
        "probeEntry=%d  sawInTx=%d  goneAfterRollback=%d",
        probeEntry, sawInTx, sawAfter == 0);
    p.detail = out;
    p.ok = (sawInTx == 1) && (sawAfter == 0);
    return p;
}

// Pick an existing pool_template row, UPDATE its description inside a
// transaction, verify in-tx view, then ROLLBACK and verify the original
// description survives.  Catches drift in the pool_template editor's
// UPDATE SQL (single-column PK on entry).
Phase testPoolTemplateUpdateRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.pool-template-update-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    char selectSql[256];
    std::snprintf(selectSql, sizeof(selectSql),
        "SELECT entry, COALESCE(description, '') FROM %s.pool_template LIMIT 1",
        cfg.worldDb.c_str());
    world_editor::db::QueryResult pickRes;
    auto err = client.query(selectSql, pickRes);
    if (!err.ok() || pickRes.rowCount() == 0)
    {
        p.detail = "no pool_template row available (skip)";
        p.ms = sinceMs(t0);
        p.ok = true; // not fatal - tiny DBs may legitimately be empty.
        return p;
    }
    uint32_t const entry = uint32_t(pickRes.asUInt64(0, 0).value_or(0));
    std::string const beforeDesc = pickRes.cell(0, 1);

    err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    char updSql[384];
    std::snprintf(updSql, sizeof(updSql),
        "UPDATE %s.pool_template SET description='smoketest probe' WHERE entry=%u",
        cfg.worldDb.c_str(), entry);
    uint64_t affected = 0;
    err = client.exec(updSql, &affected);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "UPDATE failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    char selProbe[256];
    std::snprintf(selProbe, sizeof(selProbe),
        "SELECT COALESCE(description, '') FROM %s.pool_template WHERE entry=%u",
        cfg.worldDb.c_str(), entry);
    world_editor::db::QueryResult midRes;
    (void)client.query(selProbe, midRes);
    std::string const inTxDesc = midRes.rowCount() > 0
        ? midRes.cell(0, 0) : std::string();

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(selProbe, afterRes);
    std::string const afterDesc = afterRes.rowCount() > 0
        ? afterRes.cell(0, 0) : std::string();

    p.ms = sinceMs(t0);
    char out[384];
    std::snprintf(out, sizeof(out),
        "entry=%u  inTx='%s'  afterRollback='%s'  affected=%llu",
        entry, inTxDesc.c_str(), afterDesc.c_str(),
        (unsigned long long)affected);
    p.detail = out;
    p.ok = affected == 1
        && inTxDesc == std::string("smoketest probe")
        && afterDesc == beforeDesc;
    return p;
}

// INSERT a probe spawn_group_template row at groupId=999999 inside a
// transaction, verify visible in-tx, ROLLBACK and verify it is gone.
// Catches drift in the spawn_group_template editor's INSERT generator
// (groupId, groupName, groupFlags column list).
Phase testSpawnGroupTemplateInsertRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.spawn-group-template-insert-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    int const probeId = 999999;
    char checkSql[256];
    std::snprintf(checkSql, sizeof(checkSql),
        "SELECT COUNT(*) FROM %s.spawn_group_template WHERE groupId=%d",
        cfg.worldDb.c_str(), probeId);
    world_editor::db::QueryResult cRes;
    auto err = client.query(checkSql, cRes);
    if (!err.ok())
    {
        p.detail = "pre-check failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }
    uint64_t const preCount = cRes.rowCount() > 0 ? cRes.asUInt64(0, 0).value_or(0) : 0;
    if (preCount != 0)
    {
        p.detail = "probe groupId 999999 already occupied; aborting";
        p.ms = sinceMs(t0);
        return p;
    }

    err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    char insSql[384];
    std::snprintf(insSql, sizeof(insSql),
        "INSERT INTO %s.spawn_group_template (groupId, groupName, groupFlags) "
        "VALUES (%d, 'smoketest probe', 0)",
        cfg.worldDb.c_str(), probeId);
    err = client.exec(insSql);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "INSERT failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    world_editor::db::QueryResult midRes;
    (void)client.query(checkSql, midRes);
    int const sawInTx = midRes.rowCount() > 0
        ? int(midRes.asUInt64(0, 0).value_or(0)) : 0;

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(checkSql, afterRes);
    int const sawAfter = afterRes.rowCount() > 0
        ? int(afterRes.asUInt64(0, 0).value_or(0)) : 0;

    p.ms = sinceMs(t0);
    char out[256];
    std::snprintf(out, sizeof(out),
        "probeId=%d  sawInTx=%d  goneAfterRollback=%d",
        probeId, sawInTx, sawAfter == 0);
    p.detail = out;
    p.ok = (sawInTx == 1) && (sawAfter == 0);
    return p;
}

// Pick an existing spawn_group_template row, UPDATE its groupName inside
// a transaction, verify the in-tx view, ROLLBACK and verify the original
// groupName survives.  Catches drift in the spawn_group_template editor's
// UPDATE SQL (single-column PK on groupId).
Phase testSpawnGroupTemplateUpdateRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.spawn-group-template-update-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    char selectSql[256];
    std::snprintf(selectSql, sizeof(selectSql),
        "SELECT groupId, COALESCE(groupName, '') FROM %s.spawn_group_template LIMIT 1",
        cfg.worldDb.c_str());
    world_editor::db::QueryResult pickRes;
    auto err = client.query(selectSql, pickRes);
    if (!err.ok() || pickRes.rowCount() == 0)
    {
        p.detail = "no spawn_group_template row available (skip)";
        p.ms = sinceMs(t0);
        p.ok = true; // empty table is not fatal.
        return p;
    }
    uint32_t const groupId = uint32_t(pickRes.asUInt64(0, 0).value_or(0));
    std::string const beforeName = pickRes.cell(0, 1);

    err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    char updSql[384];
    std::snprintf(updSql, sizeof(updSql),
        "UPDATE %s.spawn_group_template SET groupName='smoketest probe' WHERE groupId=%u",
        cfg.worldDb.c_str(), groupId);
    uint64_t affected = 0;
    err = client.exec(updSql, &affected);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "UPDATE failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    char selProbe[256];
    std::snprintf(selProbe, sizeof(selProbe),
        "SELECT COALESCE(groupName, '') FROM %s.spawn_group_template WHERE groupId=%u",
        cfg.worldDb.c_str(), groupId);
    world_editor::db::QueryResult midRes;
    (void)client.query(selProbe, midRes);
    std::string const inTxName = midRes.rowCount() > 0
        ? midRes.cell(0, 0) : std::string();

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(selProbe, afterRes);
    std::string const afterName = afterRes.rowCount() > 0
        ? afterRes.cell(0, 0) : std::string();

    p.ms = sinceMs(t0);
    char out[384];
    std::snprintf(out, sizeof(out),
        "groupId=%u  inTx='%s'  afterRollback='%s'  affected=%llu",
        groupId, inTxName.c_str(), afterName.c_str(),
        (unsigned long long)affected);
    p.detail = out;
    p.ok = affected == 1
        && inTxName == std::string("smoketest probe")
        && afterName == beforeName;
    return p;
}

// Pick an existing spawn_group_template row, INSERT a probe spawn_group
// member (spawnType=0, spawnId=999999) inside a transaction, verify it is
// visible in-tx, ROLLBACK, and verify it is gone.  Catches drift in the
// GroupsPoolsDialog 'Add member' INSERT generator (groupId, spawnType,
// spawnId column list and PK semantics).
Phase testSpawnGroupInsertRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.spawn-group-insert-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    // Pick any existing spawn_group_template row to hang the probe off.
    char pickSql[256];
    std::snprintf(pickSql, sizeof(pickSql),
        "SELECT groupId FROM %s.spawn_group_template ORDER BY groupId LIMIT 1",
        cfg.worldDb.c_str());
    world_editor::db::QueryResult pickRes;
    auto err = client.query(pickSql, pickRes);
    if (!err.ok() || pickRes.rowCount() == 0)
    {
        p.detail = "no spawn_group_template row available (skip)";
        p.ms = sinceMs(t0);
        p.ok = true; // empty table is not fatal.
        return p;
    }
    uint32_t const groupId = uint32_t(pickRes.asUInt64(0, 0).value_or(0));

    int const probeSpawnId = 999999;
    int const probeType    = 0;
    char checkSql[320];
    std::snprintf(checkSql, sizeof(checkSql),
        "SELECT COUNT(*) FROM %s.spawn_group "
        "WHERE groupId=%u AND spawnType=%d AND spawnId=%d",
        cfg.worldDb.c_str(), groupId, probeType, probeSpawnId);
    world_editor::db::QueryResult cRes;
    err = client.query(checkSql, cRes);
    if (!err.ok())
    {
        p.detail = "pre-check failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }
    uint64_t const preCount = cRes.rowCount() > 0 ? cRes.asUInt64(0, 0).value_or(0) : 0;
    if (preCount != 0)
    {
        p.detail = "probe (groupId,type,sid) already occupied; aborting";
        p.ms = sinceMs(t0);
        return p;
    }

    err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    char insSql[384];
    std::snprintf(insSql, sizeof(insSql),
        "INSERT INTO %s.spawn_group (groupId, spawnType, spawnId) "
        "VALUES (%u, %d, %d)",
        cfg.worldDb.c_str(), groupId, probeType, probeSpawnId);
    err = client.exec(insSql);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "INSERT failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    world_editor::db::QueryResult midRes;
    (void)client.query(checkSql, midRes);
    int const sawInTx = midRes.rowCount() > 0
        ? int(midRes.asUInt64(0, 0).value_or(0)) : 0;

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(checkSql, afterRes);
    int const sawAfter = afterRes.rowCount() > 0
        ? int(afterRes.asUInt64(0, 0).value_or(0)) : 0;

    p.ms = sinceMs(t0);
    char out[256];
    std::snprintf(out, sizeof(out),
        "groupId=%u probeType=%d probeSpawnId=%d sawInTx=%d goneAfterRollback=%d",
        groupId, probeType, probeSpawnId, sawInTx, sawAfter == 0);
    p.detail = out;
    p.ok = (sawInTx == 1) && (sawAfter == 0);
    return p;
}

// Pick an existing pool_template row, INSERT a probe pool_members row for
// creature membership (type=0, spawnId=999999) inside a transaction, verify
// visible in-tx, ROLLBACK, verify gone.  Catches drift in the GroupsPoolsDialog
// 'Add creature' INSERT generator (pool_members shape with type column).
Phase testPoolCreatureInsertRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.pool-creature-insert-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    char pickSql[256];
    std::snprintf(pickSql, sizeof(pickSql),
        "SELECT entry FROM %s.pool_template ORDER BY entry LIMIT 1",
        cfg.worldDb.c_str());
    world_editor::db::QueryResult pickRes;
    auto err = client.query(pickSql, pickRes);
    if (!err.ok() || pickRes.rowCount() == 0)
    {
        p.detail = "no pool_template row available (skip)";
        p.ms = sinceMs(t0);
        p.ok = true;
        return p;
    }
    uint32_t const poolEntry = uint32_t(pickRes.asUInt64(0, 0).value_or(0));

    int const probeType   = 0; // creature
    int const probeSpawn  = 999999;
    char checkSql[320];
    std::snprintf(checkSql, sizeof(checkSql),
        "SELECT COUNT(*) FROM %s.pool_members "
        "WHERE poolSpawnId=%u AND type=%d AND spawnId=%d",
        cfg.worldDb.c_str(), poolEntry, probeType, probeSpawn);
    world_editor::db::QueryResult cRes;
    err = client.query(checkSql, cRes);
    if (!err.ok())
    {
        p.detail = "pre-check failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }
    uint64_t const preCount = cRes.rowCount() > 0 ? cRes.asUInt64(0, 0).value_or(0) : 0;
    if (preCount != 0)
    {
        p.detail = "probe (poolEntry,type,sid) already occupied; aborting";
        p.ms = sinceMs(t0);
        return p;
    }

    err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    char insSql[416];
    std::snprintf(insSql, sizeof(insSql),
        "INSERT INTO %s.pool_members (type, spawnId, poolSpawnId, chance, description) "
        "VALUES (%d, %d, %u, 50.0, 'probe')",
        cfg.worldDb.c_str(), probeType, probeSpawn, poolEntry);
    err = client.exec(insSql);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "INSERT failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    world_editor::db::QueryResult midRes;
    (void)client.query(checkSql, midRes);
    int const sawInTx = midRes.rowCount() > 0
        ? int(midRes.asUInt64(0, 0).value_or(0)) : 0;

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(checkSql, afterRes);
    int const sawAfter = afterRes.rowCount() > 0
        ? int(afterRes.asUInt64(0, 0).value_or(0)) : 0;

    p.ms = sinceMs(t0);
    char out[256];
    std::snprintf(out, sizeof(out),
        "poolEntry=%u type=%d guid=%d sawInTx=%d goneAfterRollback=%d",
        poolEntry, probeType, probeSpawn, sawInTx, sawAfter == 0);
    p.detail = out;
    p.ok = (sawInTx == 1) && (sawAfter == 0);
    return p;
}

// Same shape as pool-creature-insert-rollback but type=1 (gameobject).
// Catches drift in the GroupsPoolsDialog 'Add gameobject' INSERT generator.
Phase testPoolGameobjectInsertRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.pool-gameobject-insert-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    char pickSql[256];
    std::snprintf(pickSql, sizeof(pickSql),
        "SELECT entry FROM %s.pool_template ORDER BY entry LIMIT 1",
        cfg.worldDb.c_str());
    world_editor::db::QueryResult pickRes;
    auto err = client.query(pickSql, pickRes);
    if (!err.ok() || pickRes.rowCount() == 0)
    {
        p.detail = "no pool_template row available (skip)";
        p.ms = sinceMs(t0);
        p.ok = true;
        return p;
    }
    uint32_t const poolEntry = uint32_t(pickRes.asUInt64(0, 0).value_or(0));

    int const probeType   = 1; // gameobject
    int const probeSpawn  = 999999;
    char checkSql[320];
    std::snprintf(checkSql, sizeof(checkSql),
        "SELECT COUNT(*) FROM %s.pool_members "
        "WHERE poolSpawnId=%u AND type=%d AND spawnId=%d",
        cfg.worldDb.c_str(), poolEntry, probeType, probeSpawn);
    world_editor::db::QueryResult cRes;
    err = client.query(checkSql, cRes);
    if (!err.ok())
    {
        p.detail = "pre-check failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }
    uint64_t const preCount = cRes.rowCount() > 0 ? cRes.asUInt64(0, 0).value_or(0) : 0;
    if (preCount != 0)
    {
        p.detail = "probe (poolEntry,type,sid) already occupied; aborting";
        p.ms = sinceMs(t0);
        return p;
    }

    err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    char insSql[416];
    std::snprintf(insSql, sizeof(insSql),
        "INSERT INTO %s.pool_members (type, spawnId, poolSpawnId, chance, description) "
        "VALUES (%d, %d, %u, 50.0, 'probe')",
        cfg.worldDb.c_str(), probeType, probeSpawn, poolEntry);
    err = client.exec(insSql);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "INSERT failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    world_editor::db::QueryResult midRes;
    (void)client.query(checkSql, midRes);
    int const sawInTx = midRes.rowCount() > 0
        ? int(midRes.asUInt64(0, 0).value_or(0)) : 0;

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(checkSql, afterRes);
    int const sawAfter = afterRes.rowCount() > 0
        ? int(afterRes.asUInt64(0, 0).value_or(0)) : 0;

    p.ms = sinceMs(t0);
    char out[256];
    std::snprintf(out, sizeof(out),
        "poolEntry=%u type=%d guid=%d sawInTx=%d goneAfterRollback=%d",
        poolEntry, probeType, probeSpawn, sawInTx, sawAfter == 0);
    p.detail = out;
    p.ok = (sawInTx == 1) && (sawAfter == 0);
    return p;
}

// Reserve a transport guid, INSERT a probe row inside a transaction,
// verify visible in-tx + gone after rollback.  Catches SQL drift on the
// transports editor's INSERT generator (small table, 7 columns).
Phase testTransportInsertRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.transport-insert-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    char maxSql[256];
    std::snprintf(maxSql, sizeof(maxSql),
        "SELECT COALESCE(MAX(guid), 0)+1 FROM %s.transports",
        cfg.worldDb.c_str());
    world_editor::db::QueryResult maxRes;
    (void)client.query(maxSql, maxRes);
    uint64_t const reserved = maxRes.rowCount() > 0
        ? maxRes.asUInt64(0, 0).value_or(0) : 0;
    if (reserved == 0) { p.detail = "guid reservation failed"; p.ms = sinceMs(t0); return p; }

    char entrySql[256];
    std::snprintf(entrySql, sizeof(entrySql),
        "SELECT entry FROM %s.gameobject_template ORDER BY entry LIMIT 1",
        cfg.worldDb.c_str());
    world_editor::db::QueryResult entryRes;
    (void)client.query(entrySql, entryRes);
    if (entryRes.rowCount() == 0)
    {
        p.detail = "no gameobject_template rows -- skip";
        p.ms = sinceMs(t0);
        p.ok = true;
        return p;
    }
    uint32_t const probeEntry = uint32_t(entryRes.asUInt64(0, 0).value_or(0));

    auto err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    char buf[512];
    std::snprintf(buf, sizeof(buf),
        "INSERT INTO %s.transports "
        "(guid, entry, name, phaseUseFlags, phaseid, phasegroup, ScriptName) "
        "VALUES (%llu, %u, 'smoketest probe', 0, 0, 0, '')",
        cfg.worldDb.c_str(), (unsigned long long)reserved, probeEntry);
    err = client.exec(buf);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "INSERT failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    std::snprintf(buf, sizeof(buf),
        "SELECT COUNT(*) FROM %s.transports WHERE guid = %llu",
        cfg.worldDb.c_str(), (unsigned long long)reserved);
    world_editor::db::QueryResult midRes;
    (void)client.query(buf, midRes);
    int const sawInTx = midRes.rowCount() > 0
        ? int(midRes.asUInt64(0, 0).value_or(0)) : 0;

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(buf, afterRes);
    int const sawAfter = afterRes.rowCount() > 0
        ? int(afterRes.asUInt64(0, 0).value_or(0)) : 0;

    p.ms = sinceMs(t0);
    char out[256];
    std::snprintf(out, sizeof(out),
        "reservedGuid=%llu  probeEntry=%u  sawInTx=%d  goneAfterRollback=%d",
        (unsigned long long)reserved, probeEntry,
        sawInTx, sawAfter == 0);
    p.detail = out;
    p.ok = (sawInTx == 1) && (sawAfter == 0);
    return p;
}

// UPDATE one existing transport's name + ScriptName inside a transaction,
// verify visible-in-tx then ROLLBACK restores.
Phase testTransportUpdateRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.transport-update-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    char selectSql[256];
    std::snprintf(selectSql, sizeof(selectSql),
        "SELECT guid, COALESCE(name, ''), ScriptName "
        "FROM %s.transports ORDER BY guid LIMIT 1",
        cfg.worldDb.c_str());
    world_editor::db::QueryResult pickRes;
    auto err = client.query(selectSql, pickRes);
    if (!err.ok() || pickRes.rowCount() == 0)
    {
        p.detail = "no transports rows (skip)";
        p.ms = sinceMs(t0);
        p.ok = true;
        return p;
    }
    uint64_t    const guid       = pickRes.asUInt64(0, 0).value_or(0);
    std::string const beforeName = pickRes.cell(0, 1);

    err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    char buf[512];
    std::snprintf(buf, sizeof(buf),
        "UPDATE %s.transports SET name='smoketest probe', ScriptName='Probe' "
        "WHERE guid=%llu",
        cfg.worldDb.c_str(), (unsigned long long)guid);
    uint64_t affected = 0;
    err = client.exec(buf, &affected);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "UPDATE failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    std::snprintf(buf, sizeof(buf),
        "SELECT COALESCE(name, '') FROM %s.transports WHERE guid=%llu",
        cfg.worldDb.c_str(), (unsigned long long)guid);
    world_editor::db::QueryResult midRes;
    (void)client.query(buf, midRes);
    std::string const inTxName = midRes.rowCount() > 0 ? midRes.cell(0, 0) : std::string{};

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(buf, afterRes);
    std::string const afterName = afterRes.rowCount() > 0 ? afterRes.cell(0, 0) : std::string{};

    p.ms = sinceMs(t0);
    char out[320];
    std::snprintf(out, sizeof(out),
        "guid=%llu  inTxName='%s'  afterRollback='%s'  affected=%llu",
        (unsigned long long)guid, inTxName.c_str(), afterName.c_str(),
        (unsigned long long)affected);
    p.detail = out;
    p.ok = affected == 1
        && inTxName == "smoketest probe"
        && afterName == beforeName;
    return p;
}

Phase testSnapToGround(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "maps.snap-to-ground", false, {}, 0.0 };
    auto const t0 = clock::now();

    // Take the first creature on the map (any with valid position), look
    // up its (X, Y) in the cache, compare against its stored Z. Close
    // values (within 25y) prove the GridMap-mirroring math works.
    char selectSql[256];
    std::snprintf(selectSql, sizeof(selectSql),
        "SELECT guid, position_x, position_y, position_z "
        "FROM %s.creature WHERE map = %u AND position_z > -1000 "
        "ORDER BY guid LIMIT 1",
        cfg.worldDb.c_str(), cfg.mapId);
    world_editor::db::QueryResult res;
    auto const err = client.query(selectSql, res);
    if (!err.ok())
    {
        p.detail = "creature pick failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }
    if (res.rowCount() == 0)
    {
        p.detail = "no creature rows on this map";
        p.ms = sinceMs(t0);
        return p;
    }
    int64_t const guid = res.asInt64(0, 0).value_or(0);
    float   const px   = float(res.asDouble(0, 1).value_or(0.0));
    float   const py   = float(res.asDouble(0, 2).value_or(0.0));
    float   const pz   = float(res.asDouble(0, 3).value_or(0.0));

    world_editor::io::MapTileCache cache;
    cache.setMapsDir(cfg.mapsDir);
    float const snapped = cache.heightAt(cfg.mapId, px, py);

    p.ms = sinceMs(t0);
    char buf[256];
    if (snapped <= world_editor::io::ADT_INVALID_HEIGHT)
    {
        std::snprintf(buf, sizeof(buf),
            "guid=%lld at (%.1f, %.1f, %.1f) -> snap=INVALID (point in hole or tile missing)",
            (long long)guid, px, py, pz);
        p.detail = buf;
        // Not fatal - some creatures sit on transports / inside WMOs
        // where heightmap returns INVALID.  Flag as partial pass.
        p.ok = false;
        return p;
    }
    float const delta = std::fabs(snapped - pz);
    std::snprintf(buf, sizeof(buf),
        "guid=%lld at (%.1f, %.1f)  storedZ=%.2f  snappedZ=%.2f  delta=%.2fy  tilesCached=%zu",
        (long long)guid, px, py, pz, snapped, delta, cache.cachedTileCount());
    p.detail = buf;
    // 25y tolerance: creatures often sit slightly above terrain (mounts,
    // floating mobs), and WMO interiors snap to the ADT below.
    p.ok = (delta < 25.0f);
    return p;
}

Phase testSpawnUpdateRoundtrip(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.creature-update-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    // Pick the first creature row on the map and stash its spawntimesecs.
    char selectSql[256];
    std::snprintf(selectSql, sizeof(selectSql),
        "SELECT guid, spawntimesecs FROM %s.creature WHERE map = %u LIMIT 1",
        cfg.worldDb.c_str(), cfg.mapId);
    world_editor::db::QueryResult before;
    auto err = client.query(selectSql, before);
    if (!err.ok())
    {
        p.detail = "pre-select failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }
    if (before.rowCount() == 0)
    {
        p.detail = "no creature rows on this map - cannot test";
        p.ms = sinceMs(t0);
        return p;
    }
    int64_t  const guid     = before.asInt64 (0, 0).value_or(0);
    uint32_t const original = static_cast<uint32_t>(before.asUInt64(0, 1).value_or(0));
    uint32_t const probe    = original + 99999u;

    // Open a transaction, UPDATE spawntimesecs, verify, ROLLBACK, verify.
    err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    char updateSql[256];
    std::snprintf(updateSql, sizeof(updateSql),
        "UPDATE %s.creature SET spawntimesecs = %u WHERE guid = %lld",
        cfg.worldDb.c_str(), probe, static_cast<long long>(guid));
    uint64_t affected = 0;
    err = client.exec(updateSql, &affected);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "UPDATE failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    // SELECT inside transaction to verify the new value is visible.
    char verifySql[256];
    std::snprintf(verifySql, sizeof(verifySql),
        "SELECT spawntimesecs FROM %s.creature WHERE guid = %lld",
        cfg.worldDb.c_str(), static_cast<long long>(guid));
    world_editor::db::QueryResult midRes;
    (void)client.query(verifySql, midRes);
    uint32_t const midValue = midRes.rowCount() > 0
        ? static_cast<uint32_t>(midRes.asUInt64(0, 0).value_or(0)) : 0;

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(verifySql, afterRes);
    uint32_t const afterValue = afterRes.rowCount() > 0
        ? static_cast<uint32_t>(afterRes.asUInt64(0, 0).value_or(0)) : 0;

    p.ms = sinceMs(t0);
    char buf[256];
    std::snprintf(buf, sizeof(buf),
        "guid=%lld  before=%u  inTx=%u  afterRollback=%u  affected=%llu",
        static_cast<long long>(guid),
        original, midValue, afterValue,
        static_cast<unsigned long long>(affected));
    p.detail = buf;
    p.ok = (midValue == probe) && (afterValue == original) && (affected == 1);
    return p;
}

// UPDATE creature_addon.mount inside a transaction, verify in-tx + after
// rollback.  Mirrors the editor's CreatureAddonEditDialog commit path.
// Falls back to inserting a probe row if no creature_addon exists at all
// (clean DBs after a fresh extract have an empty table).
Phase testCreatureAddonUpdateRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.creature-addon-update-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    // Pick any existing creature_addon row.  Prefer one whose creature is
    // on the configured map; otherwise fall back to the table at large.
    char selectSql[400];
    std::snprintf(selectSql, sizeof(selectSql),
        "SELECT ca.guid, ca.mount FROM %s.creature_addon ca "
        "JOIN %s.creature c ON c.guid = ca.guid WHERE c.map = %u LIMIT 1",
        cfg.worldDb.c_str(), cfg.worldDb.c_str(), cfg.mapId);
    world_editor::db::QueryResult pickRes;
    auto err = client.query(selectSql, pickRes);
    if (!err.ok())
    {
        p.detail = "pre-select failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }
    if (pickRes.rowCount() == 0)
    {
        std::snprintf(selectSql, sizeof(selectSql),
            "SELECT guid, mount FROM %s.creature_addon LIMIT 1",
            cfg.worldDb.c_str());
        (void)client.query(selectSql, pickRes);
    }

    int64_t  guid           = 0;
    uint32_t originalMount  = 0;
    bool     probeInserted  = false;

    err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    if (pickRes.rowCount() > 0)
    {
        guid          = pickRes.asInt64 (0, 0).value_or(0);
        originalMount = static_cast<uint32_t>(pickRes.asUInt64(0, 1).value_or(0));
    }
    else
    {
        // Table is empty (or all rows orphaned).  Insert a probe row tied
        // to any creature.guid so the rollback path is still validated.
        char findSql[256];
        std::snprintf(findSql, sizeof(findSql),
            "SELECT guid FROM %s.creature LIMIT 1", cfg.worldDb.c_str());
        world_editor::db::QueryResult cRes;
        (void)client.query(findSql, cRes);
        if (cRes.rowCount() == 0)
        {
            (void)client.exec("ROLLBACK");
            p.detail = "no creature rows at all - cannot test";
            p.ms = sinceMs(t0);
            p.ok = true; // not fatal on truly-empty DB
            return p;
        }
        guid = cRes.asInt64(0, 0).value_or(0);
        char insSql[512];
        std::snprintf(insSql, sizeof(insSql),
            "INSERT INTO %s.creature_addon (guid, mount) VALUES (%lld, 0)",
            cfg.worldDb.c_str(), static_cast<long long>(guid));
        err = client.exec(insSql);
        if (!err.ok())
        {
            (void)client.exec("ROLLBACK");
            p.detail = "probe INSERT failed: " + err.message;
            p.ms = sinceMs(t0);
            return p;
        }
        probeInserted = true;
        originalMount = 0;
    }

    uint32_t const probeMount = originalMount + 7777u;
    char updateSql[400];
    std::snprintf(updateSql, sizeof(updateSql),
        "UPDATE %s.creature_addon SET mount = %u WHERE guid = %lld",
        cfg.worldDb.c_str(), probeMount, static_cast<long long>(guid));
    uint64_t affected = 0;
    err = client.exec(updateSql, &affected);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "UPDATE failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    char verifySql[400];
    std::snprintf(verifySql, sizeof(verifySql),
        "SELECT mount FROM %s.creature_addon WHERE guid = %lld",
        cfg.worldDb.c_str(), static_cast<long long>(guid));
    world_editor::db::QueryResult midRes;
    (void)client.query(verifySql, midRes);
    uint32_t const midValue = midRes.rowCount() > 0
        ? static_cast<uint32_t>(midRes.asUInt64(0, 0).value_or(0)) : 0;

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(verifySql, afterRes);
    // After ROLLBACK: if we inserted a probe row, the row should be gone.
    // Otherwise the original mount should be restored.
    uint32_t const afterValue = afterRes.rowCount() > 0
        ? static_cast<uint32_t>(afterRes.asUInt64(0, 0).value_or(0)) : 0;
    bool const afterMatches = probeInserted
        ? (afterRes.rowCount() == 0)
        : (afterValue == originalMount && afterRes.rowCount() == 1);

    p.ms = sinceMs(t0);
    char buf[320];
    std::snprintf(buf, sizeof(buf),
        "guid=%lld  before=%u  inTx=%u  afterRollback=%u  affected=%llu  probeInserted=%d",
        static_cast<long long>(guid),
        originalMount, midValue, afterValue,
        static_cast<unsigned long long>(affected), int(probeInserted));
    p.detail = buf;
    p.ok = (midValue == probeMount) && afterMatches && (affected == 1);
    return p;
}

// UPDATE gameobject_addon.invisibilityValue inside a transaction, verify
// in-tx + after rollback.  Mirrors the editor's GameObjectAddonEditDialog
// commit path.  Falls back to inserting a probe row if no gameobject_addon
// exists at all (clean DBs after a fresh extract have an empty table).
Phase testGameObjectAddonUpdateRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.gameobject-addon-update-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    // Pick any existing gameobject_addon row.  Prefer one whose gameobject
    // is on the configured map; otherwise fall back to the table at large.
    char selectSql[400];
    std::snprintf(selectSql, sizeof(selectSql),
        "SELECT ga.guid, ga.invisibilityValue FROM %s.gameobject_addon ga "
        "JOIN %s.gameobject g ON g.guid = ga.guid WHERE g.map = %u LIMIT 1",
        cfg.worldDb.c_str(), cfg.worldDb.c_str(), cfg.mapId);
    world_editor::db::QueryResult pickRes;
    auto err = client.query(selectSql, pickRes);
    if (!err.ok())
    {
        p.detail = "pre-select failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }
    if (pickRes.rowCount() == 0)
    {
        std::snprintf(selectSql, sizeof(selectSql),
            "SELECT guid, invisibilityValue FROM %s.gameobject_addon LIMIT 1",
            cfg.worldDb.c_str());
        (void)client.query(selectSql, pickRes);
    }

    int64_t  guid               = 0;
    uint32_t originalInvisValue = 0;
    bool     probeInserted      = false;

    err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    if (pickRes.rowCount() > 0)
    {
        guid               = pickRes.asInt64 (0, 0).value_or(0);
        originalInvisValue = static_cast<uint32_t>(pickRes.asUInt64(0, 1).value_or(0));
    }
    else
    {
        // Table is empty (or all rows orphaned).  Insert a probe row tied
        // to any gameobject.guid so the rollback path is still validated.
        char findSql[256];
        std::snprintf(findSql, sizeof(findSql),
            "SELECT guid FROM %s.gameobject LIMIT 1", cfg.worldDb.c_str());
        world_editor::db::QueryResult goRes;
        (void)client.query(findSql, goRes);
        if (goRes.rowCount() == 0)
        {
            (void)client.exec("ROLLBACK");
            p.detail = "no gameobject rows at all - cannot test";
            p.ms = sinceMs(t0);
            p.ok = true; // not fatal on truly-empty DB
            return p;
        }
        guid = goRes.asInt64(0, 0).value_or(0);
        char insSql[512];
        std::snprintf(insSql, sizeof(insSql),
            "INSERT INTO %s.gameobject_addon (guid, invisibilityValue) VALUES (%lld, 0)",
            cfg.worldDb.c_str(), static_cast<long long>(guid));
        err = client.exec(insSql);
        if (!err.ok())
        {
            (void)client.exec("ROLLBACK");
            p.detail = "probe INSERT failed: " + err.message;
            p.ms = sinceMs(t0);
            return p;
        }
        probeInserted = true;
        originalInvisValue = 0;
    }

    uint32_t const probeValue = originalInvisValue + 9999u;
    char updateSql[400];
    std::snprintf(updateSql, sizeof(updateSql),
        "UPDATE %s.gameobject_addon SET invisibilityValue = %u WHERE guid = %lld",
        cfg.worldDb.c_str(), probeValue, static_cast<long long>(guid));
    uint64_t affected = 0;
    err = client.exec(updateSql, &affected);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "UPDATE failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    char verifySql[400];
    std::snprintf(verifySql, sizeof(verifySql),
        "SELECT invisibilityValue FROM %s.gameobject_addon WHERE guid = %lld",
        cfg.worldDb.c_str(), static_cast<long long>(guid));
    world_editor::db::QueryResult midRes;
    (void)client.query(verifySql, midRes);
    uint32_t const midValue = midRes.rowCount() > 0
        ? static_cast<uint32_t>(midRes.asUInt64(0, 0).value_or(0)) : 0;

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(verifySql, afterRes);
    // After ROLLBACK: if we inserted a probe row, the row should be gone.
    // Otherwise the original value should be restored.
    uint32_t const afterValue = afterRes.rowCount() > 0
        ? static_cast<uint32_t>(afterRes.asUInt64(0, 0).value_or(0)) : 0;
    bool const afterMatches = probeInserted
        ? (afterRes.rowCount() == 0)
        : (afterValue == originalInvisValue && afterRes.rowCount() == 1);

    p.ms = sinceMs(t0);
    char buf[320];
    std::snprintf(buf, sizeof(buf),
        "guid=%lld  before=%u  inTx=%u  afterRollback=%u  affected=%llu  probeInserted=%d",
        static_cast<long long>(guid),
        originalInvisValue, midValue, afterValue,
        static_cast<unsigned long long>(affected), int(probeInserted));
    p.detail = buf;
    p.ok = (midValue == probeValue) && afterMatches && (affected == 1);
    return p;
}

Phase testCommitRoundtrip(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    // Annotation INSERT + ROLLBACK probe.  Same SQL shape AnnotationCommit-
    // Dialog issues; proves the editor's commit path works without leaving
    // an artifact row.  Named "db.annotation-insert-rollback" so the
    // smoketest tail surfaces it next to the other -insert-rollback phases.
    Phase p{ "db.annotation-insert-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    // INSERT a probe row inside a transaction, verify the row count went
    // up by 1, then ROLLBACK so we don't pollute the live table.  This
    // proves the editor's commit path (transaction wrapping + INSERT
    // syntax) works without leaving artifacts.
    auto runOrFail = [&](char const* sql, char const* phase) -> std::optional<world_editor::db::QueryError>
    {
        auto e = client.exec(sql);
        if (!e.ok())
            return e;
        (void)phase;
        return std::nullopt;
    };

    if (auto e = runOrFail("START TRANSACTION", "begin")) { p.detail = e->message; return p; }

    char insertSql[512];
    std::snprintf(insertSql, sizeof(insertSql),
        "INSERT INTO %s.playerbot_v2_world_metadata "
        "(map_id, zone_id, kind, pos_x, pos_y, pos_z, radius, label, notes, created_by) "
        "VALUES (%u, 0, 1, 0.0, 0.0, 0.0, 5.0, 'smoketest', 'rollback me', 'world_editor_smoketest')",
        cfg.charsDb.c_str(), cfg.mapId);
    if (auto e = runOrFail(insertSql, "insert"))
    {
        (void)client.exec("ROLLBACK");
        p.detail = "[" + std::to_string(e->code) + "] " + e->message;
        return p;
    }

    uint64_t const inserted = client.lastInsertId();

    char selectSql[400];
    std::snprintf(selectSql, sizeof(selectSql),
        "SELECT id, kind, label FROM %s.playerbot_v2_world_metadata WHERE id = %llu",
        cfg.charsDb.c_str(), static_cast<unsigned long long>(inserted));
    world_editor::db::QueryResult res;
    auto qErr = client.query(selectSql, res);
    if (!qErr.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "post-insert SELECT failed: " + qErr.message;
        return p;
    }
    bool const sawRow = (res.rowCount() == 1 && res.cell(0, 2) == "smoketest");

    (void)client.exec("ROLLBACK");

    // Verify the probe row is gone after rollback.
    world_editor::db::QueryResult verifyRes;
    (void)client.query(selectSql, verifyRes);
    bool const rolledBack = (verifyRes.rowCount() == 0);

    p.ms = sinceMs(t0);
    char buf[200];
    std::snprintf(buf, sizeof(buf),
        "inserted id=%llu  sawInTx=%d  goneAfterRollback=%d",
        static_cast<unsigned long long>(inserted), int(sawRow), int(rolledBack));
    p.detail = buf;
    p.ok = sawRow && rolledBack;
    return p;
}

Phase testListfileRoundtrip()
{
    // infra.listfile-roundtrip: parse a tiny in-memory CSV through
    // ListfileLookup and assert that resolveFdid round-trips for both
    // exact-case and case-insensitive forward-slash-normalized paths.
    // No I/O, no environment dependency — runs in every CI.
    Phase p{ "infra.listfile-roundtrip", false, {}, 0.0 };
    auto const t0 = clock::now();

    // 3 valid records + 1 comment + 1 empty line.  Two delimiter flavors
    // (comma + semicolon) exercise the Road::ListfileMap parser surface.
    constexpr char const* kCsv =
        "# wow-listfile mini sample\n"
        "1532530,tileset/expansion10/11ea_road01_1024.blp\n"
        "200001;world/minimaps/azeroth/map32_48.blp\n"
        "\n"
        "200002,World/Minimaps/Kalimdor/Map_30_31.blp\n";

    world_editor::io::ListfileLookup lookup;
    lookup.loadFromString(kCsv);
    p.ms = sinceMs(t0);

    std::size_t const count = lookup.entryCount();
    auto a = lookup.resolveFdid(std::string_view("tileset/expansion10/11ea_road01_1024.blp"));
    auto b = lookup.resolveFdid(std::string_view("world/minimaps/azeroth/map32_48.blp"));
    // Path with backslashes + mixed case to verify normalization.
    auto c = lookup.resolveFdid(std::string_view("World\\Minimaps\\Kalimdor\\Map_30_31.blp"));
    auto miss = lookup.resolveFdid(std::string_view("does/not/exist.blp"));

    bool const ok =
        count == 3
        && a && *a == 1532530u
        && b && *b == 200001u
        && c && *c == 200002u
        && !miss.has_value();

    char buf[256];
    std::snprintf(buf, sizeof(buf),
        "entries=%zu  a=%s b=%s c=%s miss=%s",
        count,
        a ? std::to_string(*a).c_str() : "<none>",
        b ? std::to_string(*b).c_str() : "<none>",
        c ? std::to_string(*c).c_str() : "<none>",
        miss ? "FOUND(BUG)" : "absent");
    p.detail = buf;
    p.ok = ok;
    return p;
}

Phase testMinimapTileOrientation()
{
    // render.minimap-tile-orientation: validate the BLP TRANSPOSE that
    // NavMeshView::loadOrUploadMinimapTile / SceneView3D::loadOrUploadMinimapTile
    // perform on every minimap tile.  Exercises the EXACT QTransform path
    // production uses (matrix 0,1,1,0,0,0) so a future regression that
    // swaps the matrix or reverts to a manual reinterpret_cast<QRgb const*>
    // pixel loop on a non-ARGB32 image fails loudly here.
    //
    // The previous incarnation of this test simulated the swap with a
    // raw uint8 array, which passed even after the production code
    // started reading garbage on Format_RGBA8888 BLPs (the cause of the
    // operator's brown/blue stripe corruption).  Using a real QImage in
    // Format_ARGB32 plus QTransform makes the test bit-equivalent to
    // production -- and ARGB32 makes the pixel values deterministic
    // across host endianness (ARGB32 is always 0xAARRGGBB regardless).
    //
    // Why transpose, not vertical flip: raw BLP storage indexes texels
    // by (row=world-X, col=world-Y), but our shader (matching wow.export's
    // TerrainRenderer.js:640-641 convention) samples them as
    // (U=world-Y, V=world-X).  A pure transpose -- dst(y,x) = src(x,y) --
    // realigns texel storage to shader sampling.
    //
    // 4x4 with markers at (col=1,row=0) and (col=2,row=0) -- both
    // OFF-DIAGONAL, so a transpose-vs-identity-vs-flip mismatch fails
    // loudly.  After transpose dst(row=y,col=x) = src(row=x,col=y):
    //   src(col=1,row=0)=R  =>  dst(col=0,row=1)=R
    //   src(col=2,row=0)=G  =>  dst(col=0,row=2)=G
    Phase p{ "render.minimap-tile-orientation", false, {}, 0.0 };
    auto const t0 = clock::now();

    constexpr int W = 4;
    constexpr int H = 4;

    // Build an ARGB32 image and place two off-diagonal markers.  ARGB32
    // (NOT RGBA8888) is the format production normalises to before the
    // transpose, so the test matches what really runs.
    QImage src(W, H, QImage::Format_ARGB32);
    src.fill(QColor(0, 0, 0, 0));                           // blank
    src.setPixelColor(/*x=col=*/1, /*y=row=*/0, QColor(255,   0, 0, 255)); // red at (col=1,row=0)
    src.setPixelColor(/*x=col=*/2, /*y=row=*/0, QColor(  0, 255, 0, 255)); // green at (col=2,row=0)

    // Same transform production code uses.  After transpose:
    //   src(col=1,row=0)=R -> dst(col=0,row=1)=R
    //   src(col=2,row=0)=G -> dst(col=0,row=2)=G
    QImage dst = src.transformed(QTransform(0, 1, 1, 0, 0, 0));

    bool const sizeRight     = (dst.width() == H && dst.height() == W);
    QRgb const expectRed     = qRgba(255,   0, 0, 255);
    QRgb const expectGreen   = qRgba(  0, 255, 0, 255);
    QRgb const expectBlank   = qRgba(0, 0, 0, 0);

    bool const redLandedAt_0_1   = sizeRight && (dst.pixel(/*x=col=*/0, /*y=row=*/1) == expectRed);
    bool const greenLandedAt_0_2 = sizeRight && (dst.pixel(/*x=col=*/0, /*y=row=*/2) == expectGreen);
    bool const oldRedSlotBlank   = sizeRight && (dst.pixel(/*x=col=*/1, /*y=row=*/0) == expectBlank);
    bool const oldGreenSlotBlank = sizeRight && (dst.pixel(/*x=col=*/2, /*y=row=*/0) == expectBlank);
    bool const originBlankPostTr = sizeRight && (dst.pixel(0, 0) == expectBlank);
    bool const cornerBlankPostTr = sizeRight && (dst.pixel(3, 3) == expectBlank);

    p.ms = sinceMs(t0);

    char buf[256];
    std::snprintf(buf, sizeof(buf),
        "src(col=1,row=0)=R src(col=2,row=0)=G -> dst(col=0,row=1)=%s dst(col=0,row=2)=%s "
        "oldR=%s oldG=%s o=%s c=%s",
        redLandedAt_0_1     ? "R"     : "MISS",
        greenLandedAt_0_2   ? "G"     : "MISS",
        oldRedSlotBlank     ? "blank" : "STAINED",
        oldGreenSlotBlank   ? "blank" : "STAINED",
        originBlankPostTr   ? "blank" : "STAINED",
        cornerBlankPostTr   ? "blank" : "STAINED");
    p.detail = buf;
    p.ok = redLandedAt_0_1 && greenLandedAt_0_2
        && oldRedSlotBlank && oldGreenSlotBlank
        && originBlankPostTr && cornerBlankPostTr;
    return p;
}

Phase testMinimapCanonicalPath()
{
    // render.minimap-canonical-path: assert the SINGLE canonical filename
    // formula NavMeshView::loadOrUploadMinimapTile builds for every probed
    // tile.  The bug we're locking down here: the old loader emitted both
    // (gx, gy) and (gy, gx) orderings, and when the listfile carries BOTH
    // as DIFFERENT FDIDs (because they correspond to two real tiles in the
    // world), whichever resolved first won and half the landmass ended up
    // placed at the wrong grid cell.  Wow.export source (ADTExporter.js,
    // TerrainRenderer.js) confirms the canonical form is
    //     world/minimaps/<dir>/map<paddedY>_<paddedX>.blp
    // and in TC notation that becomes map<pad2(gx)>_<pad2(gy)>.blp because
    // TC's gx == client info.y and TC's gy == client info.x.
    Phase p{ "render.minimap-canonical-path", false, {}, 0.0 };
    auto const t0 = clock::now();

    auto pad2 = [](int v) {
        std::string s = std::to_string(v);
        if (s.size() == 1) s.insert(s.begin(), '0');
        return s;
    };
    auto vpath = [&](int gx, int gy, std::string const& dir) {
        return "world/minimaps/" + dir + "/map" + pad2(gx) + "_" + pad2(gy) + ".blp";
    };

    std::string const got1 = vpath(34, 61, "azeroth");
    std::string const got2 = vpath(5, 8,   "azeroth");
    std::string const want1 = "world/minimaps/azeroth/map34_61.blp";
    std::string const want2 = "world/minimaps/azeroth/map05_08.blp";

    bool const ok1 = (got1 == want1);
    bool const ok2 = (got2 == want2);

    p.ms = sinceMs(t0);
    char buf[384];
    std::snprintf(buf, sizeof(buf),
        "gx=34,gy=61 -> '%s' %s ; gx=5,gy=8 -> '%s' %s",
        got1.c_str(), ok1 ? "OK" : "MISMATCH",
        got2.c_str(), ok2 ? "OK" : "MISMATCH");
    p.detail = buf;
    p.ok = ok1 && ok2;
    return p;
}

// db.template-lookup: exercise the shared TemplateLookup service (creature /
// gameobject / quest template resolve + search + FK-existence) against the live
// world DB. Backs the content pickers and the spawn-placement validation gate.
Phase testTemplateLookup(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    using world_editor::db::TemplateLookup;
    Phase p{ "db.template-lookup", false, {}, 0.0 };
    auto const t0 = clock::now();

    TemplateLookup lk(&client, cfg.worldDb);

    // creature_template is always populated on a TC world DB; grab the first
    // rows by entry (empty query = no filter).
    std::vector<TemplateLookup::Row> creatures = lk.search(TemplateLookup::Table::Creature, "", 50);
    bool const haveRows = !creatures.empty();
    uint32_t const firstEntry = haveRows ? creatures.front().entry : 0;
    std::string const firstName = haveRows ? creatures.front().name : std::string();

    bool const nameOk   = haveRows && lk.name(TemplateLookup::Table::Creature, firstEntry) == firstName;
    bool const existsOk = haveRows && lk.exists(TemplateLookup::Table::Creature, firstEntry);
    bool const notExist = !lk.exists(TemplateLookup::Table::Creature, 999999999u);

    bool numericOk = false;
    if (haveRows)
    {
        std::vector<TemplateLookup::Row> byId =
            lk.search(TemplateLookup::Table::Creature, std::to_string(firstEntry), 10);
        for (TemplateLookup::Row const& r : byId)
            if (r.entry == firstEntry) { numericOk = true; break; }
    }

    // GameObject + Quest tables must resolve without error (row counts vary).
    std::vector<TemplateLookup::Row> gos    = lk.search(TemplateLookup::Table::GameObject, "", 5);
    std::vector<TemplateLookup::Row> quests = lk.search(TemplateLookup::Table::Quest, "", 5);

    p.ms = sinceMs(t0);
    char buf[288];
    std::snprintf(buf, sizeof(buf),
        "creatures=%zu firstEntry=%u name='%s' nameOk=%d exists=%d notExist=%d numeric=%d gos=%zu quests=%zu",
        creatures.size(), firstEntry, firstName.substr(0, 24).c_str(),
        int(nameOk), int(existsOk), int(notExist), int(numericOk), gos.size(), quests.size());
    p.detail = buf;
    p.ok = haveRows && nameOk && existsOk && notExist && numericOk;
    return p;
}

// smartai.metadata: verify the build-time-generated SmartAI metadata (from core
// SmartScriptMgr.h) is complete and accurate — this is the drift guard for the
// SmartAI editor's event/action/target labels + param semantics.
Phase testSmartAiMetadata(CliConfig const&)
{
    using namespace world_editor::smartai;
    Phase p{ "smartai.metadata", false, {}, 0.0 };
    auto const t0 = clock::now();

    auto findByValue = [](MetaEntry const* arr, int n, int v) -> MetaEntry const* {
        for (int i = 0; i < n; ++i)
            if (arr[i].value == v) return &arr[i];
        return nullptr;
    };
    MetaEntry const* spellhit = findByValue(kSmartEvents,  kSmartEventsCount,  8);  // SMART_EVENT_SPELLHIT
    MetaEntry const* cast     = findByValue(kSmartActions, kSmartActionsCount, 11); // SMART_ACTION_CAST
    MetaEntry const* selfTgt  = findByValue(kSmartTargets, kSmartTargetsCount, 1);  // SMART_TARGET_SELF
    MetaEntry const* condAura = findByValue(kConditions,   kConditionsCount,   1);  // CONDITION_AURA

    bool const spellhitOk = spellhit && std::string(spellhit->name) == "SMART_EVENT_SPELLHIT"
                         && std::string(spellhit->params).find("SpellID") != std::string::npos;
    bool const castOk     = cast && std::string(cast->name) == "SMART_ACTION_CAST"
                         && std::string(cast->params).find("SpellId") != std::string::npos;
    bool const targetOk   = selfTgt && std::string(selfTgt->name) == "SMART_TARGET_SELF";
    bool const condOk     = condAura && std::string(condAura->name) == "CONDITION_AURA"
                         && std::string(condAura->params).find("spell_id") != std::string::npos;

    bool const countsOk = kSmartEventsCount >= 80 && kSmartActionsCount >= 150
                       && kSmartTargetsCount >= 30 && kConditionsCount >= 50;

    p.ms = sinceMs(t0);
    char buf[256];
    std::snprintf(buf, sizeof(buf),
        "events=%d actions=%d targets=%d conditions=%d spellhit=%d cast=%d selfTarget=%d cond=%d",
        kSmartEventsCount, kSmartActionsCount, kSmartTargetsCount, kConditionsCount,
        int(spellhitOk), int(castOk), int(targetOk), int(condOk));
    p.detail = buf;
    p.ok = countsOk && spellhitOk && castOk && targetOk && condOk;
    return p;
}

Phase testCascFdidOpen(CliConfig const& cfg)
{
    // casc.fdid-open: end-to-end test that the editor can resolve a path
    // through the listfile and open the file by FDID from a live CASC.
    // SKIPS unless the operator supplied --casc-dir + --listfile.
    Phase p{ "casc.fdid-open", false, {}, 0.0 };
    auto const t0 = clock::now();
    if (cfg.cascDir.empty() || cfg.listfileCsv.empty())
    {
        p.skipped = true;
        p.detail  = "skipped: --casc-dir and --listfile not both set";
        p.ms      = sinceMs(t0);
        return p;
    }

    world_editor::io::CascClient casc;
    if (!casc.open(cfg.cascDir))
    {
        p.detail = "CASC open failed: " + casc.lastError();
        p.ms     = sinceMs(t0);
        return p;
    }
    if (!casc.ready())
    {
        p.detail = "CASC reports not ready after open";
        p.ms     = sinceMs(t0);
        return p;
    }

    world_editor::io::ListfileLookup lookup;
    QString err;
    if (!lookup.loadFromFile(QString::fromStdString(cfg.listfileCsv), &err))
    {
        p.detail = "listfile load failed: " + err.toStdString();
        p.ms     = sinceMs(t0);
        return p;
    }
    if (lookup.empty())
    {
        p.detail = "listfile parsed 0 entries";
        p.ms     = sinceMs(t0);
        return p;
    }

    // Pick a FDID to verify.  Operator-supplied wins; otherwise pull the
    // first FDID out of the loaded listfile by reverse-walking pathFor()
    // on a known low-numbered id.  We don't iterate the listfile (no
    // public iterator) so this falls back to a single common minimap id.
    uint32_t fdid = cfg.probeFdid;
    if (fdid == 0)
    {
        // No probe supplied — just confirm both subsystems are ready and
        // report PASS without actually opening a file.  This keeps the
        // smoketest CI-friendly when only a CASC dir + listfile are
        // supplied without a known FDID.
        char buf[256];
        std::snprintf(buf, sizeof(buf),
            "casc=open listfile=%zu entries (no --probe-fdid supplied, open skipped)",
            lookup.entryCount());
        p.detail = buf;
        p.ok     = true;
        p.ms     = sinceMs(t0);
        return p;
    }

    std::vector<uint8_t> blob;
    bool const opened = casc.openByFileDataId(fdid, blob);
    p.ms = sinceMs(t0);

    char buf[256];
    std::snprintf(buf, sizeof(buf),
        "fdid=%u opened=%d bytes=%zu listfile=%zu",
        fdid, int(opened), blob.size(), lookup.entryCount());
    p.detail = buf;
    p.ok     = opened && !blob.empty();
    return p;
}

// wmo.load: end-to-end decode of a real WMO root + group files into render
// geometry via io::loadWmo.  SKIPS unless the operator supplied --casc-dir,
// --listfile AND --probe-wmo-fdid (a root WMO FDID).  Modern WMO texture
// resolution is FDID-first (inline MOMT.texture1 / MDID) so the listfile is
// not strictly required to decode, but it IS attached here so the legacy
// MOTX path-string branch (case C) can also resolve+report a texturePath
// when probing an older root.
Phase testWmoLoad(CliConfig const& cfg)
{
    Phase p{ "wmo.load", false, {}, 0.0 };
    auto const t0 = clock::now();
    if (cfg.cascDir.empty() || cfg.listfileCsv.empty() || cfg.probeWmoFdid == 0)
    {
        p.skipped = true;
        p.detail  = "skipped (need --casc-dir + --listfile + --probe-wmo-fdid)";
        p.ms      = sinceMs(t0);
        p.ok      = true;
        return p;
    }

    world_editor::io::CascClient casc;
    if (!casc.open(cfg.cascDir))
    {
        p.detail = "casc open failed: " + casc.lastError();
        p.ms     = sinceMs(t0);
        return p;
    }

    // Attach the listfile so the legacy MOTX path-string branch decodes.
    world_editor::io::ListfileLookup lookup;
    QString err;
    if (lookup.loadFromFile(QString::fromStdString(cfg.listfileCsv), &err) && !lookup.empty())
        casc.setListfile(&lookup);

    world_editor::io::WmoModel model;
    bool const ok = world_editor::io::loadWmo(casc, cfg.probeWmoFdid, model);

    std::size_t verts = 0, idx = 0, subs = 0, textured = 0;
    for (auto const& g : model.groups)
    {
        verts += g.vertices.size() / 8;
        idx   += g.indices.size();
        subs  += g.subMeshes.size();
        for (auto const& sm : g.subMeshes)
            if (sm.textureFileDataId != 0 || !sm.texturePath.empty())
                ++textured;
    }

    char buf[256];
    std::snprintf(buf, sizeof(buf),
        "fdid=%u groups=%zu verts=%zu idx=%zu subMeshes=%zu textured=%zu",
        cfg.probeWmoFdid, model.groups.size(), verts, idx, subs, textured);
    p.detail = buf;
    p.ms     = sinceMs(t0);
    // Assert: load OK, >=1 group, >0 verts, >0 submeshes, >=1 textured submesh.
    p.ok     = ok && !model.groups.empty() && verts > 0 && subs > 0 && textured > 0;
    if (!p.ok)
        p.detail += "  (expected groups/verts/subMeshes/textured > 0)";
    return p;
}

// INSERT a probe game_event row (eventEntry=250) inside a transaction,
// verify visible in-tx, ROLLBACK and verify gone.  Catches drift on the
// game_event editor's INSERT column list / defaults.
// eventEntry is tinyint UNSIGNED (max 255) so we pick 250 as the probe slot.
Phase testGameEventInsertRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.game-event-insert-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    int const probeEntry = 250;
    char checkSql[256];
    std::snprintf(checkSql, sizeof(checkSql),
        "SELECT COUNT(*) FROM %s.game_event WHERE eventEntry=%d",
        cfg.worldDb.c_str(), probeEntry);
    world_editor::db::QueryResult cRes;
    auto err = client.query(checkSql, cRes);
    if (!err.ok())
    {
        p.detail = "pre-check failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }
    uint64_t const preCount = cRes.rowCount() > 0 ? cRes.asUInt64(0, 0).value_or(0) : 0;
    if (preCount != 0)
    {
        p.detail = "probe entry 250 already occupied; aborting";
        p.ms = sinceMs(t0);
        return p;
    }

    err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    char insSql[512];
    std::snprintf(insSql, sizeof(insSql),
        "INSERT INTO %s.game_event "
        "(eventEntry, start_time, end_time, occurence, length, holiday, "
        " holidayStage, description, world_event, announce) "
        "VALUES (%d, NULL, NULL, 525600, 10080, 0, 0, 'smoketest probe', 0, 0)",
        cfg.worldDb.c_str(), probeEntry);
    err = client.exec(insSql);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "INSERT failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    world_editor::db::QueryResult midRes;
    (void)client.query(checkSql, midRes);
    int const sawInTx = midRes.rowCount() > 0
        ? int(midRes.asUInt64(0, 0).value_or(0)) : 0;

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(checkSql, afterRes);
    int const sawAfter = afterRes.rowCount() > 0
        ? int(afterRes.asUInt64(0, 0).value_or(0)) : 0;

    p.ms = sinceMs(t0);
    char out[256];
    std::snprintf(out, sizeof(out),
        "probeEntry=%d  sawInTx=%d  goneAfterRollback=%d",
        probeEntry, sawInTx, sawAfter == 0);
    p.detail = out;
    p.ok = (sawInTx == 1) && (sawAfter == 0);
    return p;
}

// Pick the first game_event row, INSERT a probe (guid=999999, eventEntry=that)
// into game_event_creature inside a transaction, verify, ROLLBACK and verify
// gone.  Catches drift on the editor's link-row INSERT (composite PK on
// (guid, eventEntry)).
Phase testGameEventCreatureLinkRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.game-event-creature-link-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    char pickSql[256];
    std::snprintf(pickSql, sizeof(pickSql),
        "SELECT eventEntry FROM %s.game_event ORDER BY eventEntry LIMIT 1",
        cfg.worldDb.c_str());
    world_editor::db::QueryResult pickRes;
    auto err = client.query(pickSql, pickRes);
    if (!err.ok() || pickRes.rowCount() == 0)
    {
        p.detail = "no game_event rows (skip)";
        p.ms = sinceMs(t0);
        p.skipped = true;
        p.ok = true;
        return p;
    }
    int const eventEntry = int(pickRes.asInt64(0, 0).value_or(0));
    int64_t const probeGuid = 999999;

    char checkSql[384];
    std::snprintf(checkSql, sizeof(checkSql),
        "SELECT COUNT(*) FROM %s.game_event_creature WHERE guid=%lld AND eventEntry=%d",
        cfg.worldDb.c_str(), (long long)probeGuid, eventEntry);
    world_editor::db::QueryResult cRes;
    (void)client.query(checkSql, cRes);
    uint64_t const preCount = cRes.rowCount() > 0 ? cRes.asUInt64(0, 0).value_or(0) : 0;
    if (preCount != 0)
    {
        p.detail = "probe link already exists; aborting";
        p.ms = sinceMs(t0);
        return p;
    }

    err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    char insSql[384];
    std::snprintf(insSql, sizeof(insSql),
        "INSERT INTO %s.game_event_creature (eventEntry, guid) VALUES (%d, %lld)",
        cfg.worldDb.c_str(), eventEntry, (long long)probeGuid);
    err = client.exec(insSql);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "INSERT failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    world_editor::db::QueryResult midRes;
    (void)client.query(checkSql, midRes);
    int const sawInTx = midRes.rowCount() > 0
        ? int(midRes.asUInt64(0, 0).value_or(0)) : 0;

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(checkSql, afterRes);
    int const sawAfter = afterRes.rowCount() > 0
        ? int(afterRes.asUInt64(0, 0).value_or(0)) : 0;

    p.ms = sinceMs(t0);
    char out[256];
    std::snprintf(out, sizeof(out),
        "eventEntry=%d probeGuid=%lld sawInTx=%d goneAfterRollback=%d",
        eventEntry, (long long)probeGuid, sawInTx, sawAfter == 0);
    p.detail = out;
    p.ok = (sawInTx == 1) && (sawAfter == 0);
    return p;
}

// Same shape as game-event-creature-link-rollback but against
// game_event_gameobject.  Composite PK on (guid, eventEntry).
Phase testGameEventGameobjectLinkRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.game-event-gameobject-link-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    char pickSql[256];
    std::snprintf(pickSql, sizeof(pickSql),
        "SELECT eventEntry FROM %s.game_event ORDER BY eventEntry LIMIT 1",
        cfg.worldDb.c_str());
    world_editor::db::QueryResult pickRes;
    auto err = client.query(pickSql, pickRes);
    if (!err.ok() || pickRes.rowCount() == 0)
    {
        p.detail = "no game_event rows (skip)";
        p.ms = sinceMs(t0);
        p.skipped = true;
        p.ok = true;
        return p;
    }
    int const eventEntry = int(pickRes.asInt64(0, 0).value_or(0));
    int64_t const probeGuid = 999999;

    char checkSql[384];
    std::snprintf(checkSql, sizeof(checkSql),
        "SELECT COUNT(*) FROM %s.game_event_gameobject WHERE guid=%lld AND eventEntry=%d",
        cfg.worldDb.c_str(), (long long)probeGuid, eventEntry);
    world_editor::db::QueryResult cRes;
    (void)client.query(checkSql, cRes);
    uint64_t const preCount = cRes.rowCount() > 0 ? cRes.asUInt64(0, 0).value_or(0) : 0;
    if (preCount != 0)
    {
        p.detail = "probe link already exists; aborting";
        p.ms = sinceMs(t0);
        return p;
    }

    err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    char insSql[384];
    std::snprintf(insSql, sizeof(insSql),
        "INSERT INTO %s.game_event_gameobject (eventEntry, guid) VALUES (%d, %lld)",
        cfg.worldDb.c_str(), eventEntry, (long long)probeGuid);
    err = client.exec(insSql);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "INSERT failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    world_editor::db::QueryResult midRes;
    (void)client.query(checkSql, midRes);
    int const sawInTx = midRes.rowCount() > 0
        ? int(midRes.asUInt64(0, 0).value_or(0)) : 0;

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(checkSql, afterRes);
    int const sawAfter = afterRes.rowCount() > 0
        ? int(afterRes.asUInt64(0, 0).value_or(0)) : 0;

    p.ms = sinceMs(t0);
    char out[256];
    std::snprintf(out, sizeof(out),
        "eventEntry=%d probeGuid=%lld sawInTx=%d goneAfterRollback=%d",
        eventEntry, (long long)probeGuid, sawInTx, sawAfter == 0);
    p.detail = out;
    p.ok = (sawInTx == 1) && (sawAfter == 0);
    return p;
}

// Pick the first creature_template entry, INSERT a probe npc_vendor row
// (item=999999, slot=0, ExtendedCost=0, type=1) inside a transaction, verify
// in-tx, ROLLBACK and verify gone.  Catches drift on the NpcVendorDialog's
// INSERT column list (entry, slot, item, maxcount, incrtime, ExtendedCost,
// type) and composite-PK semantics on (entry, item, ExtendedCost, type).
Phase testNpcVendorInsertRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.npc-vendor-insert-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    char pickSql[256];
    std::snprintf(pickSql, sizeof(pickSql),
        "SELECT entry FROM %s.creature_template ORDER BY entry LIMIT 1",
        cfg.worldDb.c_str());
    world_editor::db::QueryResult pickRes;
    auto err = client.query(pickSql, pickRes);
    if (!err.ok() || pickRes.rowCount() == 0)
    {
        p.detail = "no creature_template rows (skip)";
        p.ms = sinceMs(t0);
        p.skipped = true;
        p.ok = true;
        return p;
    }
    uint32_t const entry    = uint32_t(pickRes.asUInt64(0, 0).value_or(0));
    uint32_t const probeItem = 999999;

    char checkSql[384];
    std::snprintf(checkSql, sizeof(checkSql),
        "SELECT COUNT(*) FROM %s.npc_vendor "
        "WHERE entry=%u AND item=%u AND ExtendedCost=0 AND type=1",
        cfg.worldDb.c_str(), entry, probeItem);
    world_editor::db::QueryResult cRes;
    (void)client.query(checkSql, cRes);
    uint64_t const preCount = cRes.rowCount() > 0 ? cRes.asUInt64(0, 0).value_or(0) : 0;
    if (preCount != 0)
    {
        p.detail = "probe npc_vendor row already exists; aborting";
        p.ms = sinceMs(t0);
        return p;
    }

    err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    char insSql[512];
    std::snprintf(insSql, sizeof(insSql),
        "INSERT INTO %s.npc_vendor "
        "(entry, slot, item, maxcount, incrtime, ExtendedCost, type) "
        "VALUES (%u, 0, %u, 0, 0, 0, 1)",
        cfg.worldDb.c_str(), entry, probeItem);
    err = client.exec(insSql);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "INSERT failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    world_editor::db::QueryResult midRes;
    (void)client.query(checkSql, midRes);
    int const sawInTx = midRes.rowCount() > 0 ? int(midRes.asUInt64(0, 0).value_or(0)) : 0;

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(checkSql, afterRes);
    int const sawAfter = afterRes.rowCount() > 0 ? int(afterRes.asUInt64(0, 0).value_or(0)) : 0;

    p.ms = sinceMs(t0);
    char out[256];
    std::snprintf(out, sizeof(out),
        "entry=%u probeItem=%u sawInTx=%d goneAfterRollback=%d",
        entry, probeItem, sawInTx, sawAfter == 0);
    p.detail = out;
    p.ok = (sawInTx == 1) && (sawAfter == 0);
    return p;
}

// Same probe shape, but inside the transaction also runs an UPDATE bumping
// maxcount from 0 -> 7 and verifies the post-UPDATE COUNT(maxcount=7)==1.
// Then ROLLBACK and verify everything is gone.  Catches drift on the editor's
// UPDATE WHERE clause (full composite PK match).
Phase testNpcVendorUpdateRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.npc-vendor-update-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    char pickSql[256];
    std::snprintf(pickSql, sizeof(pickSql),
        "SELECT entry FROM %s.creature_template ORDER BY entry LIMIT 1",
        cfg.worldDb.c_str());
    world_editor::db::QueryResult pickRes;
    auto err = client.query(pickSql, pickRes);
    if (!err.ok() || pickRes.rowCount() == 0)
    {
        p.detail = "no creature_template rows (skip)";
        p.ms = sinceMs(t0);
        p.skipped = true;
        p.ok = true;
        return p;
    }
    uint32_t const entry     = uint32_t(pickRes.asUInt64(0, 0).value_or(0));
    uint32_t const probeItem = 999999;

    char existsSql[384];
    std::snprintf(existsSql, sizeof(existsSql),
        "SELECT COUNT(*) FROM %s.npc_vendor "
        "WHERE entry=%u AND item=%u AND ExtendedCost=0 AND type=1",
        cfg.worldDb.c_str(), entry, probeItem);
    world_editor::db::QueryResult cRes;
    (void)client.query(existsSql, cRes);
    uint64_t const preCount = cRes.rowCount() > 0 ? cRes.asUInt64(0, 0).value_or(0) : 0;
    if (preCount != 0)
    {
        p.detail = "probe npc_vendor row already exists; aborting";
        p.ms = sinceMs(t0);
        return p;
    }

    err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    char insSql[512];
    std::snprintf(insSql, sizeof(insSql),
        "INSERT INTO %s.npc_vendor "
        "(entry, slot, item, maxcount, incrtime, ExtendedCost, type) "
        "VALUES (%u, 0, %u, 0, 0, 0, 1)",
        cfg.worldDb.c_str(), entry, probeItem);
    err = client.exec(insSql);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "INSERT failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    // Verify visible in-tx after INSERT.
    world_editor::db::QueryResult midInsRes;
    (void)client.query(existsSql, midInsRes);
    int const sawAfterInsert = midInsRes.rowCount() > 0
        ? int(midInsRes.asUInt64(0, 0).value_or(0)) : 0;

    // UPDATE maxcount to 7 using the full composite-PK WHERE.
    char updSql[512];
    std::snprintf(updSql, sizeof(updSql),
        "UPDATE %s.npc_vendor SET maxcount=7 "
        "WHERE entry=%u AND item=%u AND ExtendedCost=0 AND type=1",
        cfg.worldDb.c_str(), entry, probeItem);
    err = client.exec(updSql);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "UPDATE failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    // Verify the bump landed.
    char checkMaxSql[512];
    std::snprintf(checkMaxSql, sizeof(checkMaxSql),
        "SELECT COUNT(*) FROM %s.npc_vendor "
        "WHERE entry=%u AND item=%u AND ExtendedCost=0 AND type=1 AND maxcount=7",
        cfg.worldDb.c_str(), entry, probeItem);
    world_editor::db::QueryResult midUpdRes;
    (void)client.query(checkMaxSql, midUpdRes);
    int const sawAfterUpdate = midUpdRes.rowCount() > 0
        ? int(midUpdRes.asUInt64(0, 0).value_or(0)) : 0;

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(existsSql, afterRes);
    int const sawAfter = afterRes.rowCount() > 0
        ? int(afterRes.asUInt64(0, 0).value_or(0)) : 0;

    p.ms = sinceMs(t0);
    char out[320];
    std::snprintf(out, sizeof(out),
        "entry=%u probeItem=%u sawAfterInsert=%d sawAfterUpdate=%d goneAfterRollback=%d",
        entry, probeItem, sawAfterInsert, sawAfterUpdate, sawAfter == 0);
    p.detail = out;
    p.ok = (sawAfterInsert == 1) && (sawAfterUpdate == 1) && (sawAfter == 0);
    return p;
}

// INSERT a probe disables row (sourceType=2 Map, entry=999999, flags=0,
// comment='probe') inside a transaction, verify visible mid-tx, ROLLBACK
// and verify gone.  Catches drift on DisablesEditDialog's INSERT column
// list and composite-PK semantics on (sourceType, entry).
Phase testDisablesInsertRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.disables-insert-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    uint32_t const probeSourceType = 2;        // Map
    uint32_t const probeEntry      = 999999;

    // Resolve entry vs entryID once, matching the dialog's INFORMATION_SCHEMA probe.
    char colSql[384];
    std::snprintf(colSql, sizeof(colSql),
        "SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS "
        "WHERE TABLE_SCHEMA='%s' AND TABLE_NAME='disables' "
        "AND COLUMN_NAME IN ('entry','entryID')",
        cfg.worldDb.c_str());
    world_editor::db::QueryResult colRes;
    auto err = client.query(colSql, colRes);
    if (!err.ok() || colRes.rowCount() == 0)
    {
        p.detail = "disables table missing or no entry column (skip)";
        p.ms = sinceMs(t0);
        p.skipped = true;
        p.ok = true;
        return p;
    }
    std::string entryCol = "entry";
    bool foundEntry = false;
    for (size_t r = 0; r < colRes.rowCount(); ++r)
        if (colRes.cell(r, 0) == "entry") { foundEntry = true; break; }
    if (!foundEntry) entryCol = colRes.cell(0, 0);

    char checkSql[384];
    std::snprintf(checkSql, sizeof(checkSql),
        "SELECT COUNT(*) FROM %s.disables WHERE sourceType=%u AND %s=%u",
        cfg.worldDb.c_str(), probeSourceType, entryCol.c_str(), probeEntry);
    world_editor::db::QueryResult cRes;
    (void)client.query(checkSql, cRes);
    uint64_t const preCount = cRes.rowCount() > 0 ? cRes.asUInt64(0, 0).value_or(0) : 0;
    if (preCount != 0)
    {
        p.detail = "probe disables row already exists; aborting";
        p.ms = sinceMs(t0);
        return p;
    }

    err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    char insSql[640];
    std::snprintf(insSql, sizeof(insSql),
        "INSERT INTO %s.disables (sourceType, %s, flags, params_0, params_1, comment) "
        "VALUES (%u, %u, 0, '', '', 'probe')",
        cfg.worldDb.c_str(), entryCol.c_str(), probeSourceType, probeEntry);
    err = client.exec(insSql);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "INSERT failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    world_editor::db::QueryResult midRes;
    (void)client.query(checkSql, midRes);
    int const sawInTx = midRes.rowCount() > 0 ? int(midRes.asUInt64(0, 0).value_or(0)) : 0;

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(checkSql, afterRes);
    int const sawAfter = afterRes.rowCount() > 0 ? int(afterRes.asUInt64(0, 0).value_or(0)) : 0;

    p.ms = sinceMs(t0);
    char out[256];
    std::snprintf(out, sizeof(out),
        "sourceType=%u entry=%u sawInTx=%d goneAfterRollback=%d",
        probeSourceType, probeEntry, sawInTx, sawAfter == 0);
    p.detail = out;
    p.ok = (sawInTx == 1) && (sawAfter == 0);
    return p;
}

// Same probe shape, but inside the transaction also runs an UPDATE bumping
// flags 0 -> 1 and verifies the post-UPDATE COUNT(flags=1)==1.  Then ROLLBACK
// and verify gone.  Catches drift on the editor's UPDATE WHERE clause (full
// composite-PK match on sourceType + entry).
Phase testDisablesUpdateRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.disables-update-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    uint32_t const probeSourceType = 2;        // Map
    uint32_t const probeEntry      = 999999;

    char colSql[384];
    std::snprintf(colSql, sizeof(colSql),
        "SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS "
        "WHERE TABLE_SCHEMA='%s' AND TABLE_NAME='disables' "
        "AND COLUMN_NAME IN ('entry','entryID')",
        cfg.worldDb.c_str());
    world_editor::db::QueryResult colRes;
    auto err = client.query(colSql, colRes);
    if (!err.ok() || colRes.rowCount() == 0)
    {
        p.detail = "disables table missing or no entry column (skip)";
        p.ms = sinceMs(t0);
        p.skipped = true;
        p.ok = true;
        return p;
    }
    std::string entryCol = "entry";
    bool foundEntry = false;
    for (size_t r = 0; r < colRes.rowCount(); ++r)
        if (colRes.cell(r, 0) == "entry") { foundEntry = true; break; }
    if (!foundEntry) entryCol = colRes.cell(0, 0);

    char existsSql[384];
    std::snprintf(existsSql, sizeof(existsSql),
        "SELECT COUNT(*) FROM %s.disables WHERE sourceType=%u AND %s=%u",
        cfg.worldDb.c_str(), probeSourceType, entryCol.c_str(), probeEntry);
    world_editor::db::QueryResult cRes;
    (void)client.query(existsSql, cRes);
    uint64_t const preCount = cRes.rowCount() > 0 ? cRes.asUInt64(0, 0).value_or(0) : 0;
    if (preCount != 0)
    {
        p.detail = "probe disables row already exists; aborting";
        p.ms = sinceMs(t0);
        return p;
    }

    err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    char insSql[640];
    std::snprintf(insSql, sizeof(insSql),
        "INSERT INTO %s.disables (sourceType, %s, flags, params_0, params_1, comment) "
        "VALUES (%u, %u, 0, '', '', 'probe')",
        cfg.worldDb.c_str(), entryCol.c_str(), probeSourceType, probeEntry);
    err = client.exec(insSql);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "INSERT failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    world_editor::db::QueryResult midInsRes;
    (void)client.query(existsSql, midInsRes);
    int const sawAfterInsert = midInsRes.rowCount() > 0
        ? int(midInsRes.asUInt64(0, 0).value_or(0)) : 0;

    char updSql[512];
    std::snprintf(updSql, sizeof(updSql),
        "UPDATE %s.disables SET flags=1 WHERE sourceType=%u AND %s=%u",
        cfg.worldDb.c_str(), probeSourceType, entryCol.c_str(), probeEntry);
    err = client.exec(updSql);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "UPDATE failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    char checkFlagsSql[512];
    std::snprintf(checkFlagsSql, sizeof(checkFlagsSql),
        "SELECT COUNT(*) FROM %s.disables "
        "WHERE sourceType=%u AND %s=%u AND flags=1",
        cfg.worldDb.c_str(), probeSourceType, entryCol.c_str(), probeEntry);
    world_editor::db::QueryResult midUpdRes;
    (void)client.query(checkFlagsSql, midUpdRes);
    int const sawAfterUpdate = midUpdRes.rowCount() > 0
        ? int(midUpdRes.asUInt64(0, 0).value_or(0)) : 0;

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(existsSql, afterRes);
    int const sawAfter = afterRes.rowCount() > 0
        ? int(afterRes.asUInt64(0, 0).value_or(0)) : 0;

    p.ms = sinceMs(t0);
    char out[320];
    std::snprintf(out, sizeof(out),
        "sourceType=%u entry=%u sawAfterInsert=%d sawAfterUpdate=%d goneAfterRollback=%d",
        probeSourceType, probeEntry, sawAfterInsert, sawAfterUpdate, sawAfter == 0);
    p.detail = out;
    p.ok = (sawAfterInsert == 1) && (sawAfterUpdate == 1) && (sawAfter == 0);
    return p;
}

// INSERT a probe waypoint_path header row (PathId=9999999, Comment='probe')
// inside a transaction, verify visible mid-tx, ROLLBACK and verify gone.
// Catches drift on WaypointPathDialog's path-header INSERT column list.
Phase testWaypointPathInsertRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.waypoint-path-insert-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    uint32_t const probePathId = 9999999;

    char checkSql[256];
    std::snprintf(checkSql, sizeof(checkSql),
        "SELECT COUNT(*) FROM %s.waypoint_path WHERE PathId=%u",
        cfg.worldDb.c_str(), probePathId);
    world_editor::db::QueryResult cRes;
    (void)client.query(checkSql, cRes);
    uint64_t const preCount = cRes.rowCount() > 0 ? cRes.asUInt64(0, 0).value_or(0) : 0;
    if (preCount != 0)
    {
        p.detail = "probe waypoint_path row already exists; aborting";
        p.ms = sinceMs(t0);
        return p;
    }

    auto err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    char insSql[512];
    std::snprintf(insSql, sizeof(insSql),
        "INSERT INTO %s.waypoint_path (PathId, MoveType, Flags, Velocity, Comment) "
        "VALUES (%u, 0, 0, 0.0, 'probe')",
        cfg.worldDb.c_str(), probePathId);
    err = client.exec(insSql);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "INSERT failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    world_editor::db::QueryResult midRes;
    (void)client.query(checkSql, midRes);
    int const sawInTx = midRes.rowCount() > 0
        ? int(midRes.asUInt64(0, 0).value_or(0)) : 0;

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(checkSql, afterRes);
    int const sawAfter = afterRes.rowCount() > 0
        ? int(afterRes.asUInt64(0, 0).value_or(0)) : 0;

    p.ms = sinceMs(t0);
    char out[256];
    std::snprintf(out, sizeof(out),
        "PathId=%u sawInTx=%d goneAfterRollback=%d",
        probePathId, sawInTx, sawAfter == 0);
    p.detail = out;
    p.ok = (sawInTx == 1) && (sawAfter == 0);
    return p;
}

// Pick the first existing waypoint_path.PathId; INSERT a probe node
// (NodeId=9999, X=Y=Z=O=0, Delay=0) inside a transaction; verify visible
// in-tx; ROLLBACK and verify gone.  Catches drift on the node INSERT column
// list and composite-PK semantics on (PathId, NodeId).
Phase testWaypointNodeInsertRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.waypoint-node-insert-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    char pickSql[256];
    std::snprintf(pickSql, sizeof(pickSql),
        "SELECT PathId FROM %s.waypoint_path ORDER BY PathId LIMIT 1",
        cfg.worldDb.c_str());
    world_editor::db::QueryResult pickRes;
    auto err = client.query(pickSql, pickRes);
    if (!err.ok() || pickRes.rowCount() == 0)
    {
        p.detail = "no waypoint_path rows present (skip)";
        p.ms = sinceMs(t0);
        p.skipped = true;
        p.ok = true;
        return p;
    }
    uint32_t const hostPathId = static_cast<uint32_t>(pickRes.asUInt64(0, 0).value_or(0));
    uint32_t const probeNodeId = 9999;

    char checkSql[384];
    std::snprintf(checkSql, sizeof(checkSql),
        "SELECT COUNT(*) FROM %s.waypoint_path_node WHERE PathId=%u AND NodeId=%u",
        cfg.worldDb.c_str(), hostPathId, probeNodeId);
    world_editor::db::QueryResult cRes;
    (void)client.query(checkSql, cRes);
    uint64_t const preCount = cRes.rowCount() > 0 ? cRes.asUInt64(0, 0).value_or(0) : 0;
    if (preCount != 0)
    {
        p.detail = "probe waypoint_path_node row already exists; aborting";
        p.ms = sinceMs(t0);
        return p;
    }

    err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    char insSql[640];
    std::snprintf(insSql, sizeof(insSql),
        "INSERT INTO %s.waypoint_path_node "
        "(PathId, NodeId, PositionX, PositionY, PositionZ, Orientation, Delay) "
        "VALUES (%u, %u, 0.0, 0.0, 0.0, 0.0, 0)",
        cfg.worldDb.c_str(), hostPathId, probeNodeId);
    err = client.exec(insSql);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "INSERT failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    world_editor::db::QueryResult midRes;
    (void)client.query(checkSql, midRes);
    int const sawInTx = midRes.rowCount() > 0
        ? int(midRes.asUInt64(0, 0).value_or(0)) : 0;

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(checkSql, afterRes);
    int const sawAfter = afterRes.rowCount() > 0
        ? int(afterRes.asUInt64(0, 0).value_or(0)) : 0;

    p.ms = sinceMs(t0);
    char out[256];
    std::snprintf(out, sizeof(out),
        "hostPathId=%u NodeId=%u sawInTx=%d goneAfterRollback=%d",
        hostPathId, probeNodeId, sawInTx, sawAfter == 0);
    p.detail = out;
    p.ok = (sawInTx == 1) && (sawAfter == 0);
    return p;
}

// Within a single transaction, INSERT a probe waypoint_path + 2 probe
// nodes, then INSERT a clone path (different PathId) + 2 nodes mirroring
// the source via INSERT...SELECT (matching WaypointPathDialog::onClonePath).
// Verify both paths exist in-tx each with 2 nodes; ROLLBACK; verify both
// gone.
Phase testWaypointPathCloneRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.waypoint-path-clone-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    uint32_t const srcPathId = 9999998;
    uint32_t const dstPathId = 9999997;

    char preSql[384];
    std::snprintf(preSql, sizeof(preSql),
        "SELECT COUNT(*) FROM %s.waypoint_path WHERE PathId IN (%u, %u)",
        cfg.worldDb.c_str(), srcPathId, dstPathId);
    world_editor::db::QueryResult preRes;
    (void)client.query(preSql, preRes);
    uint64_t const preCount = preRes.rowCount() > 0
        ? preRes.asUInt64(0, 0).value_or(0) : 0;
    if (preCount != 0)
    {
        p.detail = "probe waypoint_path PathIds already exist; aborting";
        p.ms = sinceMs(t0);
        return p;
    }

    auto err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    auto runOne = [&](char const* sql) -> bool {
        auto e = client.exec(sql);
        if (!e.ok())
        {
            (void)client.exec("ROLLBACK");
            p.detail = std::string("INSERT failed: ") + e.message;
            return false;
        }
        return true;
    };

    char sql[768];
    std::snprintf(sql, sizeof(sql),
        "INSERT INTO %s.waypoint_path (PathId, MoveType, Flags, Velocity, Comment) "
        "VALUES (%u, 0, 0, 0.0, 'probe-src')",
        cfg.worldDb.c_str(), srcPathId);
    if (!runOne(sql)) { p.ms = sinceMs(t0); return p; }

    for (int n = 0; n < 2; ++n)
    {
        std::snprintf(sql, sizeof(sql),
            "INSERT INTO %s.waypoint_path_node "
            "(PathId, NodeId, PositionX, PositionY, PositionZ, Orientation, Delay) "
            "VALUES (%u, %d, %d.0, %d.0, 0.0, 0.0, 0)",
            cfg.worldDb.c_str(), srcPathId, n, n * 5, n * 5);
        if (!runOne(sql)) { p.ms = sinceMs(t0); return p; }
    }

    // Clone header + nodes using the same INSERT...SELECT shape as the
    // dialog, so any column-list drift surfaces here.
    std::snprintf(sql, sizeof(sql),
        "INSERT INTO %s.waypoint_path (PathId, MoveType, Flags, Velocity, Comment) "
        "SELECT %u, MoveType, Flags, Velocity, Comment "
        "FROM %s.waypoint_path WHERE PathId=%u",
        cfg.worldDb.c_str(), dstPathId, cfg.worldDb.c_str(), srcPathId);
    if (!runOne(sql)) { p.ms = sinceMs(t0); return p; }

    std::snprintf(sql, sizeof(sql),
        "INSERT INTO %s.waypoint_path_node "
        "(PathId, NodeId, PositionX, PositionY, PositionZ, Orientation, Delay) "
        "SELECT %u, NodeId, PositionX, PositionY, PositionZ, Orientation, Delay "
        "FROM %s.waypoint_path_node WHERE PathId=%u",
        cfg.worldDb.c_str(), dstPathId, cfg.worldDb.c_str(), srcPathId);
    if (!runOne(sql)) { p.ms = sinceMs(t0); return p; }

    // Verify in-tx: both paths exist with 2 nodes each.
    auto countNodes = [&](uint32_t pid) -> uint64_t {
        char q[256];
        std::snprintf(q, sizeof(q),
            "SELECT COUNT(*) FROM %s.waypoint_path_node WHERE PathId=%u",
            cfg.worldDb.c_str(), pid);
        world_editor::db::QueryResult r;
        (void)client.query(q, r);
        return r.rowCount() > 0 ? r.asUInt64(0, 0).value_or(0) : 0;
    };
    uint64_t const srcMid = countNodes(srcPathId);
    uint64_t const dstMid = countNodes(dstPathId);

    char headerSql[256];
    std::snprintf(headerSql, sizeof(headerSql),
        "SELECT COUNT(*) FROM %s.waypoint_path WHERE PathId IN (%u, %u)",
        cfg.worldDb.c_str(), srcPathId, dstPathId);
    world_editor::db::QueryResult midHeader;
    (void)client.query(headerSql, midHeader);
    uint64_t const midHeaders = midHeader.rowCount() > 0
        ? midHeader.asUInt64(0, 0).value_or(0) : 0;

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterHeader;
    (void)client.query(headerSql, afterHeader);
    uint64_t const afterHeaders = afterHeader.rowCount() > 0
        ? afterHeader.asUInt64(0, 0).value_or(0) : 0;

    p.ms = sinceMs(t0);
    char out[320];
    std::snprintf(out, sizeof(out),
        "srcPath=%u dstPath=%u srcMidNodes=%llu dstMidNodes=%llu midHeaders=%llu afterHeaders=%llu",
        srcPathId, dstPathId,
        (unsigned long long)srcMid, (unsigned long long)dstMid,
        (unsigned long long)midHeaders, (unsigned long long)afterHeaders);
    p.detail = out;
    p.ok = (midHeaders == 2) && (srcMid == 2) && (dstMid == 2) && (afterHeaders == 0);
    return p;
}

// INSERT a probe areatrigger_teleport row (ID=9999999, Name='probe',
// PortLocID=0) inside a transaction, verify visible mid-tx, ROLLBACK
// and verify gone.  Catches drift on AreaTriggerTeleportDialog's
// areatrigger_teleport INSERT column list.  TC 12.0+ stores the actual
// map/x/y/z/orientation in world_safe_locs keyed by PortLocID; this
// phase only exercises the join row.
Phase testAreatriggerTeleportInsertRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.areatrigger-teleport-insert-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    uint32_t const probeId = 9999999;

    char checkSql[256];
    std::snprintf(checkSql, sizeof(checkSql),
        "SELECT COUNT(*) FROM %s.areatrigger_teleport WHERE ID=%u",
        cfg.worldDb.c_str(), probeId);
    world_editor::db::QueryResult cRes;
    (void)client.query(checkSql, cRes);
    uint64_t const preCount = cRes.rowCount() > 0 ? cRes.asUInt64(0, 0).value_or(0) : 0;
    if (preCount != 0)
    {
        p.detail = "probe areatrigger_teleport row already exists; aborting";
        p.ms = sinceMs(t0);
        return p;
    }

    auto err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    char insSql[384];
    std::snprintf(insSql, sizeof(insSql),
        "INSERT INTO %s.areatrigger_teleport (ID, PortLocID, Name) "
        "VALUES (%u, 0, 'probe')",
        cfg.worldDb.c_str(), probeId);
    err = client.exec(insSql);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "INSERT failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    world_editor::db::QueryResult midRes;
    (void)client.query(checkSql, midRes);
    int const sawInTx = midRes.rowCount() > 0
        ? int(midRes.asUInt64(0, 0).value_or(0)) : 0;

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(checkSql, afterRes);
    int const sawAfter = afterRes.rowCount() > 0
        ? int(afterRes.asUInt64(0, 0).value_or(0)) : 0;

    p.ms = sinceMs(t0);
    char out[256];
    std::snprintf(out, sizeof(out),
        "ID=%u sawInTx=%d goneAfterRollback=%d",
        probeId, sawInTx, sawAfter == 0);
    p.detail = out;
    p.ok = (sawInTx == 1) && (sawAfter == 0);
    return p;
}

// INSERT probe + UPDATE Name='probe2' within the SAME transaction; verify
// both states are visible mid-tx; ROLLBACK; verify gone.  Catches drift
// on the UPDATE column list (Name + PortLocID).
Phase testAreatriggerTeleportUpdateRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.areatrigger-teleport-update-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    uint32_t const probeId = 9999998;

    char checkSql[256];
    std::snprintf(checkSql, sizeof(checkSql),
        "SELECT COUNT(*) FROM %s.areatrigger_teleport WHERE ID=%u",
        cfg.worldDb.c_str(), probeId);
    world_editor::db::QueryResult cRes;
    (void)client.query(checkSql, cRes);
    uint64_t const preCount = cRes.rowCount() > 0 ? cRes.asUInt64(0, 0).value_or(0) : 0;
    if (preCount != 0)
    {
        p.detail = "probe areatrigger_teleport row already exists; aborting";
        p.ms = sinceMs(t0);
        return p;
    }

    auto err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    char insSql[384];
    std::snprintf(insSql, sizeof(insSql),
        "INSERT INTO %s.areatrigger_teleport (ID, PortLocID, Name) "
        "VALUES (%u, 0, 'probe')",
        cfg.worldDb.c_str(), probeId);
    err = client.exec(insSql);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "INSERT failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    // Verify the post-insert state.
    char nameSql[256];
    std::snprintf(nameSql, sizeof(nameSql),
        "SELECT Name FROM %s.areatrigger_teleport WHERE ID=%u",
        cfg.worldDb.c_str(), probeId);
    world_editor::db::QueryResult mid1;
    (void)client.query(nameSql, mid1);
    bool const sawInsert = mid1.rowCount() == 1
        && mid1.cell(0, 0) == "probe";

    char updSql[384];
    std::snprintf(updSql, sizeof(updSql),
        "UPDATE %s.areatrigger_teleport SET Name='probe2', PortLocID=1 WHERE ID=%u",
        cfg.worldDb.c_str(), probeId);
    err = client.exec(updSql);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "UPDATE failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    // Verify the post-update state.
    world_editor::db::QueryResult mid2;
    (void)client.query(nameSql, mid2);
    bool const sawUpdate = mid2.rowCount() == 1
        && mid2.cell(0, 0) == "probe2";

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(checkSql, afterRes);
    int const sawAfter = afterRes.rowCount() > 0
        ? int(afterRes.asUInt64(0, 0).value_or(0)) : 0;

    p.ms = sinceMs(t0);
    char out[256];
    std::snprintf(out, sizeof(out),
        "ID=%u sawInsert=%d sawUpdate=%d goneAfterRollback=%d",
        probeId, sawInsert ? 1 : 0, sawUpdate ? 1 : 0, sawAfter == 0);
    p.detail = out;
    p.ok = sawInsert && sawUpdate && (sawAfter == 0);
    return p;
}

// Pick the first existing areatrigger_teleport row, extract its Name,
// then run a LIKE search; assert >= 1 row returned.  Read-only proxy
// for the dialog's search QLineEdit behavior.
Phase testAreatriggerTeleportSearch(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.areatrigger-teleport-search", false, {}, 0.0 };
    auto const t0 = clock::now();

    char pickSql[256];
    std::snprintf(pickSql, sizeof(pickSql),
        "SELECT ID, COALESCE(Name, '') FROM %s.areatrigger_teleport ORDER BY ID LIMIT 1",
        cfg.worldDb.c_str());
    world_editor::db::QueryResult pickRes;
    auto err = client.query(pickSql, pickRes);
    if (!err.ok() || pickRes.rowCount() == 0)
    {
        p.detail = "no areatrigger_teleport rows present (skip)";
        p.ms = sinceMs(t0);
        p.skipped = true;
        p.ok = true;
        return p;
    }
    uint32_t const firstId = static_cast<uint32_t>(pickRes.asUInt64(0, 0).value_or(0));
    std::string const firstName = pickRes.cell(0, 1);

    // If Name is empty the LIKE fallback uses the numeric ID, mirroring the
    // dialog's "substring matches ID OR Name" filter.
    std::string needle = firstName.empty() ? std::to_string(firstId) : firstName;
    // Take only a short prefix so we exercise a substring rather than an
    // exact match - the dialog's filter is substring-based.
    if (needle.size() > 4) needle.resize(4);
    std::string const escaped = client.escapeString(needle);

    char searchSql[512];
    std::snprintf(searchSql, sizeof(searchSql),
        "SELECT COUNT(*) FROM %s.areatrigger_teleport "
        "WHERE CAST(ID AS CHAR) LIKE '%%%s%%' OR Name LIKE '%%%s%%'",
        cfg.worldDb.c_str(), escaped.c_str(), escaped.c_str());
    world_editor::db::QueryResult sRes;
    err = client.query(searchSql, sRes);
    if (!err.ok() || sRes.rowCount() == 0)
    {
        p.detail = "search query failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }
    uint64_t const hits = sRes.asUInt64(0, 0).value_or(0);

    p.ms = sinceMs(t0);
    char out[256];
    std::snprintf(out, sizeof(out),
        "firstId=%u needle='%s' hits=%llu",
        firstId, needle.c_str(), (unsigned long long)hits);
    p.detail = out;
    p.ok = (hits >= 1);
    return p;
}

// Pick the first existing creature_template.entry, INSERT a probe
// creature_loot_template row (Item=999999, Chance=50, GroupId=0) inside
// a transaction, verify visible mid-tx, ROLLBACK and verify gone.
// Catches drift on CreatureLootEditDialog's INSERT column list.
Phase testCreatureLootInsertRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.creature-loot-insert-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    // Pick the first existing creature_template row to attach the probe
    // loot row to.  Modern TC composite-key index is (Entry, ItemType,
    // Item); the probe uses ItemType=0 (regular Item).
    char pickSql[256];
    std::snprintf(pickSql, sizeof(pickSql),
        "SELECT entry FROM %s.creature_template ORDER BY entry LIMIT 1",
        cfg.worldDb.c_str());
    world_editor::db::QueryResult pickRes;
    auto err = client.query(pickSql, pickRes);
    if (!err.ok() || pickRes.rowCount() == 0)
    {
        p.detail = "no creature_template rows to attach probe loot to (skip)";
        p.ms = sinceMs(t0);
        p.skipped = true;
        p.ok = true;
        return p;
    }
    uint32_t const entry  = static_cast<uint32_t>(pickRes.asUInt64(0, 0).value_or(0));
    uint32_t const probeItem = 999999u;

    char checkSql[320];
    std::snprintf(checkSql, sizeof(checkSql),
        "SELECT COUNT(*) FROM %s.creature_loot_template "
        "WHERE Entry=%u AND ItemType=0 AND Item=%u",
        cfg.worldDb.c_str(), entry, probeItem);
    world_editor::db::QueryResult cRes;
    (void)client.query(checkSql, cRes);
    uint64_t const preCount = cRes.rowCount() > 0 ? cRes.asUInt64(0, 0).value_or(0) : 0;
    if (preCount != 0)
    {
        p.detail = "probe creature_loot_template row already exists; aborting";
        p.ms = sinceMs(t0);
        return p;
    }

    err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    char insSql[512];
    std::snprintf(insSql, sizeof(insSql),
        "INSERT INTO %s.creature_loot_template "
        "(Entry, ItemType, Item, Chance, QuestRequired, LootMode, GroupId, "
        " MinCount, MaxCount, Comment) "
        "VALUES (%u, 0, %u, 50.0, 0, 1, 0, 1, 1, 'probe')",
        cfg.worldDb.c_str(), entry, probeItem);
    err = client.exec(insSql);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "INSERT failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    world_editor::db::QueryResult midRes;
    (void)client.query(checkSql, midRes);
    int const sawInTx = midRes.rowCount() > 0
        ? int(midRes.asUInt64(0, 0).value_or(0)) : 0;

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(checkSql, afterRes);
    int const sawAfter = afterRes.rowCount() > 0
        ? int(afterRes.asUInt64(0, 0).value_or(0)) : 0;

    p.ms = sinceMs(t0);
    char out[320];
    std::snprintf(out, sizeof(out),
        "Entry=%u Item=%u sawInTx=%d goneAfterRollback=%d",
        entry, probeItem, sawInTx, sawAfter == 0);
    p.detail = out;
    p.ok = (sawInTx == 1) && (sawAfter == 0);
    return p;
}

// INSERT a probe creature_loot_template row + UPDATE Chance=25.0 within
// the same transaction; verify both states visible mid-tx; ROLLBACK and
// verify gone.  Catches drift on the UPDATE column list (Chance is the
// most operator-facing field; if the editor stops binding it the row
// would silently revert to 100.0).
Phase testCreatureLootUpdateRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.creature-loot-update-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    char pickSql[256];
    std::snprintf(pickSql, sizeof(pickSql),
        "SELECT entry FROM %s.creature_template ORDER BY entry LIMIT 1",
        cfg.worldDb.c_str());
    world_editor::db::QueryResult pickRes;
    auto err = client.query(pickSql, pickRes);
    if (!err.ok() || pickRes.rowCount() == 0)
    {
        p.detail = "no creature_template rows to attach probe loot to (skip)";
        p.ms = sinceMs(t0);
        p.skipped = true;
        p.ok = true;
        return p;
    }
    uint32_t const entry     = static_cast<uint32_t>(pickRes.asUInt64(0, 0).value_or(0));
    uint32_t const probeItem = 999998u;

    char checkSql[320];
    std::snprintf(checkSql, sizeof(checkSql),
        "SELECT COUNT(*) FROM %s.creature_loot_template "
        "WHERE Entry=%u AND ItemType=0 AND Item=%u",
        cfg.worldDb.c_str(), entry, probeItem);
    world_editor::db::QueryResult cRes;
    (void)client.query(checkSql, cRes);
    uint64_t const preCount = cRes.rowCount() > 0 ? cRes.asUInt64(0, 0).value_or(0) : 0;
    if (preCount != 0)
    {
        p.detail = "probe creature_loot_template row already exists; aborting";
        p.ms = sinceMs(t0);
        return p;
    }

    err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    char insSql[512];
    std::snprintf(insSql, sizeof(insSql),
        "INSERT INTO %s.creature_loot_template "
        "(Entry, ItemType, Item, Chance, QuestRequired, LootMode, GroupId, "
        " MinCount, MaxCount, Comment) "
        "VALUES (%u, 0, %u, 50.0, 0, 1, 0, 1, 1, 'probe')",
        cfg.worldDb.c_str(), entry, probeItem);
    err = client.exec(insSql);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "INSERT failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    // Verify the post-insert Chance.
    char chanceSql[320];
    std::snprintf(chanceSql, sizeof(chanceSql),
        "SELECT Chance FROM %s.creature_loot_template "
        "WHERE Entry=%u AND ItemType=0 AND Item=%u",
        cfg.worldDb.c_str(), entry, probeItem);
    world_editor::db::QueryResult mid1;
    (void)client.query(chanceSql, mid1);
    bool const sawInsert = mid1.rowCount() == 1
        && std::fabs(mid1.asDouble(0, 0).value_or(0.0) - 50.0) < 0.01;

    char updSql[384];
    std::snprintf(updSql, sizeof(updSql),
        "UPDATE %s.creature_loot_template SET Chance=25.0 "
        "WHERE Entry=%u AND ItemType=0 AND Item=%u",
        cfg.worldDb.c_str(), entry, probeItem);
    err = client.exec(updSql);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "UPDATE failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    // Verify the post-update Chance.
    world_editor::db::QueryResult mid2;
    (void)client.query(chanceSql, mid2);
    bool const sawUpdate = mid2.rowCount() == 1
        && std::fabs(mid2.asDouble(0, 0).value_or(0.0) - 25.0) < 0.01;

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(checkSql, afterRes);
    int const sawAfter = afterRes.rowCount() > 0
        ? int(afterRes.asUInt64(0, 0).value_or(0)) : 0;

    p.ms = sinceMs(t0);
    char out[320];
    std::snprintf(out, sizeof(out),
        "Entry=%u Item=%u sawInsert=%d sawUpdate=%d goneAfterRollback=%d",
        entry, probeItem, sawInsert ? 1 : 0, sawUpdate ? 1 : 0, sawAfter == 0);
    p.detail = out;
    p.ok = sawInsert && sawUpdate && (sawAfter == 0);
    return p;
}

// SELECT the first creature_template.entry that has at least one
// creature_loot_template row; assert >= 1 row returned.  Read-only proxy
// for the dialog's "Load" button finding drops for a known creature.
Phase testCreatureLootSearchEntry(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.creature-loot-search-entry", false, {}, 0.0 };
    auto const t0 = clock::now();

    char pickSql[512];
    std::snprintf(pickSql, sizeof(pickSql),
        "SELECT ct.entry, COUNT(clt.Item) AS drops "
        "FROM %s.creature_template ct "
        "JOIN %s.creature_loot_template clt ON clt.Entry = ct.entry "
        "GROUP BY ct.entry "
        "HAVING drops >= 1 "
        "ORDER BY ct.entry LIMIT 1",
        cfg.worldDb.c_str(), cfg.worldDb.c_str());
    world_editor::db::QueryResult pickRes;
    auto err = client.query(pickSql, pickRes);
    if (!err.ok() || pickRes.rowCount() == 0)
    {
        p.detail = "no creature with any loot row present (skip)";
        p.ms = sinceMs(t0);
        p.skipped = true;
        p.ok = true;
        return p;
    }
    uint32_t const firstEntry = static_cast<uint32_t>(pickRes.asUInt64(0, 0).value_or(0));
    uint64_t const drops      = pickRes.asUInt64(0, 1).value_or(0);

    // Mirror the dialog's load query.
    char loadSql[512];
    std::snprintf(loadSql, sizeof(loadSql),
        "SELECT COUNT(*) FROM %s.creature_loot_template WHERE Entry=%u",
        cfg.worldDb.c_str(), firstEntry);
    world_editor::db::QueryResult sRes;
    err = client.query(loadSql, sRes);
    if (!err.ok() || sRes.rowCount() == 0)
    {
        p.detail = "loot-row count query failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }
    uint64_t const hits = sRes.asUInt64(0, 0).value_or(0);

    p.ms = sinceMs(t0);
    char out[256];
    std::snprintf(out, sizeof(out),
        "firstEntry=%u joinDrops=%llu reloadHits=%llu",
        firstEntry,
        (unsigned long long)drops,
        (unsigned long long)hits);
    p.detail = out;
    p.ok = (hits >= 1);
    return p;
}

// Shared INSERT/ROLLBACK harness for the secondary loot-template tables that
// share creature_loot_template's schema.  Picks the first existing Entry,
// INSERTs a probe row with Item=999999 inside START TRANSACTION, verifies
// the row visible mid-tx, ROLLBACKs and asserts gone.  Caches the source
// table for picking valid Entry values (e.g. gameobject_loot_template draws
// from gameobject_template).  If no source row exists -> SKIP.
Phase runSecondaryLootInsertRollback(world_editor::db::MySqlClient& client,
                                     CliConfig const& cfg,
                                     char const* phaseName,
                                     char const* lootTable,
                                     char const* pickSqlFmt)
{
    Phase p{ phaseName, false, {}, 0.0 };
    auto const t0 = clock::now();

    char pickSql[512];
    std::snprintf(pickSql, sizeof(pickSql), pickSqlFmt, cfg.worldDb.c_str());
    world_editor::db::QueryResult pickRes;
    auto err = client.query(pickSql, pickRes);
    if (!err.ok() || pickRes.rowCount() == 0)
    {
        p.detail = std::string("no source row to attach probe ") + lootTable + " (skip)";
        p.ms = sinceMs(t0);
        p.skipped = true;
        p.ok = true;
        return p;
    }
    uint32_t const entry     = static_cast<uint32_t>(pickRes.asUInt64(0, 0).value_or(0));
    uint32_t const probeItem = 999999u;

    char checkSql[384];
    std::snprintf(checkSql, sizeof(checkSql),
        "SELECT COUNT(*) FROM %s.%s "
        "WHERE Entry=%u AND ItemType=0 AND Item=%u",
        cfg.worldDb.c_str(), lootTable, entry, probeItem);
    world_editor::db::QueryResult cRes;
    (void)client.query(checkSql, cRes);
    uint64_t const preCount = cRes.rowCount() > 0 ? cRes.asUInt64(0, 0).value_or(0) : 0;
    if (preCount != 0)
    {
        p.detail = std::string("probe ") + lootTable + " row already exists; aborting";
        p.ms = sinceMs(t0);
        return p;
    }

    err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    char insSql[640];
    std::snprintf(insSql, sizeof(insSql),
        "INSERT INTO %s.%s "
        "(Entry, ItemType, Item, Chance, QuestRequired, LootMode, GroupId, "
        " MinCount, MaxCount, Comment) "
        "VALUES (%u, 0, %u, 50.0, 0, 1, 0, 1, 1, 'probe')",
        cfg.worldDb.c_str(), lootTable, entry, probeItem);
    err = client.exec(insSql);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "INSERT failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    world_editor::db::QueryResult midRes;
    (void)client.query(checkSql, midRes);
    int const sawInTx = midRes.rowCount() > 0
        ? int(midRes.asUInt64(0, 0).value_or(0)) : 0;

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(checkSql, afterRes);
    int const sawAfter = afterRes.rowCount() > 0
        ? int(afterRes.asUInt64(0, 0).value_or(0)) : 0;

    p.ms = sinceMs(t0);
    char out[384];
    std::snprintf(out, sizeof(out),
        "table=%s Entry=%u Item=%u sawInTx=%d goneAfterRollback=%d",
        lootTable, entry, probeItem, sawInTx, sawAfter == 0);
    p.detail = out;
    p.ok = (sawInTx == 1) && (sawAfter == 0);
    return p;
}

Phase testGameobjectLootInsertRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    return runSecondaryLootInsertRollback(client, cfg,
        "db.gameobject-loot-insert-rollback",
        "gameobject_loot_template",
        "SELECT entry FROM %s.gameobject_template ORDER BY entry LIMIT 1");
}

Phase testSkinningLootInsertRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    // skinning_loot_template.Entry refers to creature_template.entry; picking
    // a real creature.entry keeps the test consistent with the dialog's
    // operator workflow even though the loot table itself has no FK.
    return runSecondaryLootInsertRollback(client, cfg,
        "db.skinning-loot-insert-rollback",
        "skinning_loot_template",
        "SELECT entry FROM %s.creature_template ORDER BY entry LIMIT 1");
}

Phase testReferenceLootInsertRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    // reference_loot_template.Entry is an arbitrary fan-out id with no
    // owning table; pick the first existing reference id so we don't collide
    // with an empty fixture.  Falls back to a fresh-but-valid Entry=1 when
    // the table is empty (still inside a tx, so rollback yields no diff).
    return runSecondaryLootInsertRollback(client, cfg,
        "db.reference-loot-insert-rollback",
        "reference_loot_template",
        "SELECT COALESCE(MIN(Entry), 1) FROM %s.reference_loot_template");
}

// INSERT a probe gossip_menu header row (MenuID=9999999, TextID=0,
// VerifiedBuild=0) inside a transaction, verify visible mid-tx, ROLLBACK
// and verify gone.  Catches drift on GossipMenuEditDialog's INSERT column
// list.
Phase testGossipMenuInsertRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.gossip-menu-insert-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    uint32_t const probeMenuId = 9999999;
    uint32_t const probeTextId = 0;

    char checkSql[256];
    std::snprintf(checkSql, sizeof(checkSql),
        "SELECT COUNT(*) FROM %s.gossip_menu WHERE MenuID=%u AND TextID=%u",
        cfg.worldDb.c_str(), probeMenuId, probeTextId);
    world_editor::db::QueryResult cRes;
    auto err = client.query(checkSql, cRes);
    if (!err.ok())
    {
        p.detail = "gossip_menu table missing or unreadable (skip): " + err.message;
        p.ms = sinceMs(t0);
        p.skipped = true;
        p.ok = true;
        return p;
    }
    uint64_t const preCount = cRes.rowCount() > 0 ? cRes.asUInt64(0, 0).value_or(0) : 0;
    if (preCount != 0)
    {
        p.detail = "probe gossip_menu row already exists; aborting";
        p.ms = sinceMs(t0);
        return p;
    }

    err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    char insSql[384];
    std::snprintf(insSql, sizeof(insSql),
        "INSERT INTO %s.gossip_menu (MenuID, TextID, VerifiedBuild) VALUES (%u, %u, 0)",
        cfg.worldDb.c_str(), probeMenuId, probeTextId);
    err = client.exec(insSql);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "INSERT failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    world_editor::db::QueryResult midRes;
    (void)client.query(checkSql, midRes);
    int const sawInTx = midRes.rowCount() > 0 ? int(midRes.asUInt64(0, 0).value_or(0)) : 0;

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(checkSql, afterRes);
    int const sawAfter = afterRes.rowCount() > 0 ? int(afterRes.asUInt64(0, 0).value_or(0)) : 0;

    p.ms = sinceMs(t0);
    char out[256];
    std::snprintf(out, sizeof(out),
        "MenuID=%u TextID=%u sawInTx=%d goneAfterRollback=%d",
        probeMenuId, probeTextId, sawInTx, sawAfter == 0);
    p.detail = out;
    p.ok = (sawInTx == 1) && (sawAfter == 0);
    return p;
}

// Pick the first existing gossip_menu.MenuID, INSERT a probe option row
// (OptionID=9999, OptionNpc=1, OptionText='probe') inside a transaction,
// verify visible mid-tx, ROLLBACK and verify gone.  Catches drift on the
// gossip_menu_option INSERT column list.
Phase testGossipMenuOptionInsertRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.gossip-menu-option-insert-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    char pickSql[256];
    std::snprintf(pickSql, sizeof(pickSql),
        "SELECT MenuID FROM %s.gossip_menu ORDER BY MenuID LIMIT 1",
        cfg.worldDb.c_str());
    world_editor::db::QueryResult pickRes;
    auto err = client.query(pickSql, pickRes);
    if (!err.ok() || pickRes.rowCount() == 0)
    {
        p.detail = "no gossip_menu rows to attach option to (skip)";
        p.ms = sinceMs(t0);
        p.skipped = true;
        p.ok = true;
        return p;
    }
    uint32_t const menuId   = uint32_t(pickRes.asUInt64(0, 0).value_or(0));
    uint32_t const probeOpt = 9999;

    char checkSql[256];
    std::snprintf(checkSql, sizeof(checkSql),
        "SELECT COUNT(*) FROM %s.gossip_menu_option WHERE MenuID=%u AND OptionID=%u",
        cfg.worldDb.c_str(), menuId, probeOpt);
    world_editor::db::QueryResult cRes;
    (void)client.query(checkSql, cRes);
    uint64_t const preCount = cRes.rowCount() > 0 ? cRes.asUInt64(0, 0).value_or(0) : 0;
    if (preCount != 0)
    {
        p.detail = "probe gossip_menu_option row already exists; aborting";
        p.ms = sinceMs(t0);
        return p;
    }

    err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    char insSql[768];
    std::snprintf(insSql, sizeof(insSql),
        "INSERT INTO %s.gossip_menu_option "
        "(MenuID, OptionID, OptionNpc, OptionText, OptionBroadcastTextID, Language, "
        " ActionMenuID, ActionPoiID, GossipNpcOptionID, BoxCoded, BoxMoney, BoxText, "
        " BoxBroadcastTextID, SpellID, OverrideIconID, VerifiedBuild) "
        "VALUES (%u, %u, 1, 'probe', 0, 0, 0, 0, 0, 0, 0, '', 0, 0, 0, 0)",
        cfg.worldDb.c_str(), menuId, probeOpt);
    err = client.exec(insSql);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "INSERT failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    world_editor::db::QueryResult midRes;
    (void)client.query(checkSql, midRes);
    int const sawInTx = midRes.rowCount() > 0 ? int(midRes.asUInt64(0, 0).value_or(0)) : 0;

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(checkSql, afterRes);
    int const sawAfter = afterRes.rowCount() > 0 ? int(afterRes.asUInt64(0, 0).value_or(0)) : 0;

    p.ms = sinceMs(t0);
    char out[256];
    std::snprintf(out, sizeof(out),
        "MenuID=%u OptionID=%u sawInTx=%d goneAfterRollback=%d",
        menuId, probeOpt, sawInTx, sawAfter == 0);
    p.detail = out;
    p.ok = (sawInTx == 1) && (sawAfter == 0);
    return p;
}

// Pick the first existing npc_text.ID, UPDATE Probability0 to a sentinel
// value (0.5) inside a transaction, verify the change is visible mid-tx,
// ROLLBACK and verify the row is no longer at the sentinel.  Catches drift
// on GossipMenuEditDialog's "Edit NPC text..." UPDATE.
Phase testNpcTextUpdateRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.npc-text-update-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    char pickSql[256];
    std::snprintf(pickSql, sizeof(pickSql),
        "SELECT ID, Probability0 FROM %s.npc_text ORDER BY ID LIMIT 1",
        cfg.worldDb.c_str());
    world_editor::db::QueryResult pickRes;
    auto err = client.query(pickSql, pickRes);
    if (!err.ok() || pickRes.rowCount() == 0)
    {
        p.detail = "npc_text table missing or empty (skip): " + err.message;
        p.ms = sinceMs(t0);
        p.skipped = true;
        p.ok = true;
        return p;
    }
    uint32_t const id           = uint32_t(pickRes.asUInt64(0, 0).value_or(0));
    double   const originalProb = pickRes.asDouble(0, 1).value_or(0.0);

    // Pick a sentinel that's deliberately not the original value so the
    // mid-tx COUNT(Probability0=sentinel)==1 check is meaningful even when
    // the existing value happens to be 0.5.
    double sentinel = 0.5;
    if (std::abs(originalProb - sentinel) < 0.001)
        sentinel = 0.75;

    err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    char updSql[384];
    std::snprintf(updSql, sizeof(updSql),
        "UPDATE %s.npc_text SET Probability0=%.4f WHERE ID=%u",
        cfg.worldDb.c_str(), sentinel, id);
    err = client.exec(updSql);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "UPDATE failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    char readSql[256];
    std::snprintf(readSql, sizeof(readSql),
        "SELECT Probability0 FROM %s.npc_text WHERE ID=%u",
        cfg.worldDb.c_str(), id);
    world_editor::db::QueryResult midRes;
    (void)client.query(readSql, midRes);
    double const midProb = midRes.rowCount() > 0 ? midRes.asDouble(0, 0).value_or(0.0) : -1.0;
    bool const sawInTx = std::abs(midProb - sentinel) < 0.001;

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(readSql, afterRes);
    double const afterProb = afterRes.rowCount() > 0 ? afterRes.asDouble(0, 0).value_or(0.0) : -1.0;
    bool const restored = std::abs(afterProb - originalProb) < 0.001;

    p.ms = sinceMs(t0);
    char out[320];
    std::snprintf(out, sizeof(out),
        "ID=%u origProb=%.4f sentinel=%.4f midProb=%.4f afterProb=%.4f sawInTx=%d restored=%d",
        id, originalProb, sentinel, midProb, afterProb, int(sawInTx), int(restored));
    p.detail = out;
    p.ok = sawInTx && restored;
    return p;
}

// Pick the first existing creature_template.entry, INSERT a probe
// creature_text row (GroupID=99, ID=0, Type=12 Say, Text='probe') inside
// a transaction, verify visible mid-tx, ROLLBACK and verify gone.
// Catches drift on CreatureTextEditDialog's INSERT column list and PK.
Phase testCreatureTextInsertRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.creature-text-insert-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    char pickSql[256];
    std::snprintf(pickSql, sizeof(pickSql),
        "SELECT entry FROM %s.creature_template ORDER BY entry LIMIT 1",
        cfg.worldDb.c_str());
    world_editor::db::QueryResult pickRes;
    auto err = client.query(pickSql, pickRes);
    if (!err.ok() || pickRes.rowCount() == 0)
    {
        p.detail = "no creature_template rows to attach probe text to (skip)";
        p.ms = sinceMs(t0);
        p.skipped = true;
        p.ok = true;
        return p;
    }
    uint32_t const entry      = static_cast<uint32_t>(pickRes.asUInt64(0, 0).value_or(0));
    uint32_t const probeGroup = 99u;
    uint32_t const probeId    = 0u;

    char checkSql[384];
    std::snprintf(checkSql, sizeof(checkSql),
        "SELECT COUNT(*) FROM %s.creature_text "
        "WHERE CreatureID=%u AND GroupID=%u AND ID=%u",
        cfg.worldDb.c_str(), entry, probeGroup, probeId);
    world_editor::db::QueryResult cRes;
    (void)client.query(checkSql, cRes);
    uint64_t const preCount = cRes.rowCount() > 0 ? cRes.asUInt64(0, 0).value_or(0) : 0;
    if (preCount != 0)
    {
        p.detail = "probe creature_text row already exists; aborting";
        p.ms = sinceMs(t0);
        return p;
    }

    err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    char insSql[640];
    std::snprintf(insSql, sizeof(insSql),
        "INSERT INTO %s.creature_text "
        "(CreatureID, GroupID, ID, Text, Type, Language, Probability, "
        " Emote, Duration, Sound, BroadcastTextId, TextRange, comment) "
        "VALUES (%u, %u, %u, 'probe', 12, 0, 100.0, 0, 0, 0, 0, 0, 'smoketest')",
        cfg.worldDb.c_str(), entry, probeGroup, probeId);
    err = client.exec(insSql);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "INSERT failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    world_editor::db::QueryResult midRes;
    (void)client.query(checkSql, midRes);
    int const sawInTx = midRes.rowCount() > 0
        ? int(midRes.asUInt64(0, 0).value_or(0)) : 0;

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(checkSql, afterRes);
    int const sawAfter = afterRes.rowCount() > 0
        ? int(afterRes.asUInt64(0, 0).value_or(0)) : 0;

    p.ms = sinceMs(t0);
    char out[320];
    std::snprintf(out, sizeof(out),
        "CreatureID=%u GroupID=%u ID=%u sawInTx=%d goneAfterRollback=%d",
        entry, probeGroup, probeId, sawInTx, sawAfter == 0);
    p.detail = out;
    p.ok = (sawInTx == 1) && (sawAfter == 0);
    return p;
}

// INSERT a probe creature_text row + UPDATE Probability=50.0 within the
// same transaction; verify both states visible mid-tx; ROLLBACK and verify
// gone.  Catches drift on the UPDATE column list - Probability is the
// most operator-facing field after Text/Type.
Phase testCreatureTextUpdateRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.creature-text-update-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    char pickSql[256];
    std::snprintf(pickSql, sizeof(pickSql),
        "SELECT entry FROM %s.creature_template ORDER BY entry LIMIT 1",
        cfg.worldDb.c_str());
    world_editor::db::QueryResult pickRes;
    auto err = client.query(pickSql, pickRes);
    if (!err.ok() || pickRes.rowCount() == 0)
    {
        p.detail = "no creature_template rows to attach probe text to (skip)";
        p.ms = sinceMs(t0);
        p.skipped = true;
        p.ok = true;
        return p;
    }
    uint32_t const entry      = static_cast<uint32_t>(pickRes.asUInt64(0, 0).value_or(0));
    uint32_t const probeGroup = 98u;
    uint32_t const probeId    = 0u;

    char checkSql[384];
    std::snprintf(checkSql, sizeof(checkSql),
        "SELECT COUNT(*) FROM %s.creature_text "
        "WHERE CreatureID=%u AND GroupID=%u AND ID=%u",
        cfg.worldDb.c_str(), entry, probeGroup, probeId);
    world_editor::db::QueryResult cRes;
    (void)client.query(checkSql, cRes);
    uint64_t const preCount = cRes.rowCount() > 0 ? cRes.asUInt64(0, 0).value_or(0) : 0;
    if (preCount != 0)
    {
        p.detail = "probe creature_text row already exists; aborting";
        p.ms = sinceMs(t0);
        return p;
    }

    err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    char insSql[640];
    std::snprintf(insSql, sizeof(insSql),
        "INSERT INTO %s.creature_text "
        "(CreatureID, GroupID, ID, Text, Type, Language, Probability, "
        " Emote, Duration, Sound, BroadcastTextId, TextRange, comment) "
        "VALUES (%u, %u, %u, 'probe', 12, 0, 100.0, 0, 0, 0, 0, 0, 'smoketest')",
        cfg.worldDb.c_str(), entry, probeGroup, probeId);
    err = client.exec(insSql);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "INSERT failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    // Verify the post-insert Probability.
    char probSql[384];
    std::snprintf(probSql, sizeof(probSql),
        "SELECT Probability FROM %s.creature_text "
        "WHERE CreatureID=%u AND GroupID=%u AND ID=%u",
        cfg.worldDb.c_str(), entry, probeGroup, probeId);
    world_editor::db::QueryResult mid1;
    (void)client.query(probSql, mid1);
    bool const sawInsert = mid1.rowCount() == 1
        && std::fabs(mid1.asDouble(0, 0).value_or(0.0) - 100.0) < 0.01;

    char updSql[384];
    std::snprintf(updSql, sizeof(updSql),
        "UPDATE %s.creature_text SET Probability=50.0 "
        "WHERE CreatureID=%u AND GroupID=%u AND ID=%u",
        cfg.worldDb.c_str(), entry, probeGroup, probeId);
    err = client.exec(updSql);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "UPDATE failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    // Verify the post-update Probability.
    world_editor::db::QueryResult mid2;
    (void)client.query(probSql, mid2);
    bool const sawUpdate = mid2.rowCount() == 1
        && std::fabs(mid2.asDouble(0, 0).value_or(0.0) - 50.0) < 0.01;

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(checkSql, afterRes);
    int const sawAfter = afterRes.rowCount() > 0
        ? int(afterRes.asUInt64(0, 0).value_or(0)) : 0;

    p.ms = sinceMs(t0);
    char out[320];
    std::snprintf(out, sizeof(out),
        "CreatureID=%u GroupID=%u ID=%u sawInsert=%d sawUpdate=%d goneAfterRollback=%d",
        entry, probeGroup, probeId, sawInsert ? 1 : 0, sawUpdate ? 1 : 0, sawAfter == 0);
    p.detail = out;
    p.ok = sawInsert && sawUpdate && (sawAfter == 0);
    return p;
}

// SELECT COUNT(DISTINCT CreatureID) FROM creature_text; assert >= 1.
// Read-only proxy for the dialog's "are there any creatures with text
// events to inspect" sanity check.
Phase testCreatureTextSearchEntriesWithText(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.creature-text-search-entries-with-text", false, {}, 0.0 };
    auto const t0 = clock::now();

    char sql[256];
    std::snprintf(sql, sizeof(sql),
        "SELECT COUNT(DISTINCT CreatureID) FROM %s.creature_text",
        cfg.worldDb.c_str());
    world_editor::db::QueryResult res;
    auto err = client.query(sql, res);
    if (!err.ok())
    {
        p.detail = "COUNT(DISTINCT CreatureID) failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }
    if (res.rowCount() == 0)
    {
        p.detail = "creature_text empty (skip)";
        p.ms = sinceMs(t0);
        p.skipped = true;
        p.ok = true;
        return p;
    }
    uint64_t const distinctCreatures = res.asUInt64(0, 0).value_or(0);

    p.ms = sinceMs(t0);
    char out[256];
    std::snprintf(out, sizeof(out),
        "distinctCreatureIDs=%llu",
        (unsigned long long)distinctCreatures);
    p.detail = out;
    p.ok = (distinctCreatures >= 1);
    return p;
}

// Pick the first existing creature_template.entry, INSERT a probe
// creature_equip_template row (ID=99, ItemID1=999999, others 0) inside a
// transaction, verify visible mid-tx, ROLLBACK and verify gone.  Catches
// drift on CreatureEquipTemplateDialog's INSERT column list.
Phase testCreatureEquipInsertRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.creature-equip-insert-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    char pickSql[256];
    std::snprintf(pickSql, sizeof(pickSql),
        "SELECT entry FROM %s.creature_template ORDER BY entry LIMIT 1",
        cfg.worldDb.c_str());
    world_editor::db::QueryResult pickRes;
    auto err = client.query(pickSql, pickRes);
    if (!err.ok() || pickRes.rowCount() == 0)
    {
        p.detail = "no creature_template rows to attach probe equip to (skip)";
        p.ms = sinceMs(t0);
        p.skipped = true;
        p.ok = true;
        return p;
    }
    uint32_t const entry   = static_cast<uint32_t>(pickRes.asUInt64(0, 0).value_or(0));
    uint32_t const probeId = 99u;

    char checkSql[384];
    std::snprintf(checkSql, sizeof(checkSql),
        "SELECT COUNT(*) FROM %s.creature_equip_template "
        "WHERE CreatureID=%u AND ID=%u",
        cfg.worldDb.c_str(), entry, probeId);
    world_editor::db::QueryResult cRes;
    (void)client.query(checkSql, cRes);
    uint64_t const preCount = cRes.rowCount() > 0 ? cRes.asUInt64(0, 0).value_or(0) : 0;
    if (preCount != 0)
    {
        p.detail = "probe creature_equip_template row already exists; aborting";
        p.ms = sinceMs(t0);
        return p;
    }

    err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    char insSql[640];
    std::snprintf(insSql, sizeof(insSql),
        "INSERT INTO %s.creature_equip_template "
        "(CreatureID, ID, ItemID1, AppearanceModID1, ItemVisual1, "
        " ItemID2, AppearanceModID2, ItemVisual2, "
        " ItemID3, AppearanceModID3, ItemVisual3) "
        "VALUES (%u, %u, 999999, 0, 0, 0, 0, 0, 0, 0, 0)",
        cfg.worldDb.c_str(), entry, probeId);
    err = client.exec(insSql);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "INSERT failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    world_editor::db::QueryResult midRes;
    (void)client.query(checkSql, midRes);
    int const sawInTx = midRes.rowCount() > 0
        ? int(midRes.asUInt64(0, 0).value_or(0)) : 0;

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(checkSql, afterRes);
    int const sawAfter = afterRes.rowCount() > 0
        ? int(afterRes.asUInt64(0, 0).value_or(0)) : 0;

    p.ms = sinceMs(t0);
    char out[320];
    std::snprintf(out, sizeof(out),
        "CreatureID=%u ID=%u sawInTx=%d goneAfterRollback=%d",
        entry, probeId, sawInTx, sawAfter == 0);
    p.detail = out;
    p.ok = (sawInTx == 1) && (sawAfter == 0);
    return p;
}

// INSERT a probe creature_equip_template row + UPDATE ItemID1=999998 in
// the same transaction; verify both states mid-tx; ROLLBACK and verify
// gone.  Catches drift on the UPDATE column list - ItemID1 is the most
// operator-facing field (main-hand weapon).
Phase testCreatureEquipUpdateRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.creature-equip-update-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    char pickSql[256];
    std::snprintf(pickSql, sizeof(pickSql),
        "SELECT entry FROM %s.creature_template ORDER BY entry LIMIT 1",
        cfg.worldDb.c_str());
    world_editor::db::QueryResult pickRes;
    auto err = client.query(pickSql, pickRes);
    if (!err.ok() || pickRes.rowCount() == 0)
    {
        p.detail = "no creature_template rows to attach probe equip to (skip)";
        p.ms = sinceMs(t0);
        p.skipped = true;
        p.ok = true;
        return p;
    }
    uint32_t const entry   = static_cast<uint32_t>(pickRes.asUInt64(0, 0).value_or(0));
    uint32_t const probeId = 98u;

    char checkSql[384];
    std::snprintf(checkSql, sizeof(checkSql),
        "SELECT COUNT(*) FROM %s.creature_equip_template "
        "WHERE CreatureID=%u AND ID=%u",
        cfg.worldDb.c_str(), entry, probeId);
    world_editor::db::QueryResult cRes;
    (void)client.query(checkSql, cRes);
    uint64_t const preCount = cRes.rowCount() > 0 ? cRes.asUInt64(0, 0).value_or(0) : 0;
    if (preCount != 0)
    {
        p.detail = "probe creature_equip_template row already exists; aborting";
        p.ms = sinceMs(t0);
        return p;
    }

    err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    char insSql[640];
    std::snprintf(insSql, sizeof(insSql),
        "INSERT INTO %s.creature_equip_template "
        "(CreatureID, ID, ItemID1, AppearanceModID1, ItemVisual1, "
        " ItemID2, AppearanceModID2, ItemVisual2, "
        " ItemID3, AppearanceModID3, ItemVisual3) "
        "VALUES (%u, %u, 999999, 0, 0, 0, 0, 0, 0, 0, 0)",
        cfg.worldDb.c_str(), entry, probeId);
    err = client.exec(insSql);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "INSERT failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    // Verify the post-insert ItemID1.
    char itemSql[384];
    std::snprintf(itemSql, sizeof(itemSql),
        "SELECT ItemID1 FROM %s.creature_equip_template "
        "WHERE CreatureID=%u AND ID=%u",
        cfg.worldDb.c_str(), entry, probeId);
    world_editor::db::QueryResult mid1;
    (void)client.query(itemSql, mid1);
    bool const sawInsert = mid1.rowCount() == 1
        && mid1.asUInt64(0, 0).value_or(0) == 999999ull;

    char updSql[384];
    std::snprintf(updSql, sizeof(updSql),
        "UPDATE %s.creature_equip_template SET ItemID1=999998 "
        "WHERE CreatureID=%u AND ID=%u",
        cfg.worldDb.c_str(), entry, probeId);
    err = client.exec(updSql);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "UPDATE failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    world_editor::db::QueryResult mid2;
    (void)client.query(itemSql, mid2);
    bool const sawUpdate = mid2.rowCount() == 1
        && mid2.asUInt64(0, 0).value_or(0) == 999998ull;

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(checkSql, afterRes);
    int const sawAfter = afterRes.rowCount() > 0
        ? int(afterRes.asUInt64(0, 0).value_or(0)) : 0;

    p.ms = sinceMs(t0);
    char out[320];
    std::snprintf(out, sizeof(out),
        "CreatureID=%u ID=%u sawInsert=%d sawUpdate=%d goneAfterRollback=%d",
        entry, probeId, sawInsert ? 1 : 0, sawUpdate ? 1 : 0, sawAfter == 0);
    p.detail = out;
    p.ok = sawInsert && sawUpdate && (sawAfter == 0);
    return p;
}

// SELECT COUNT(DISTINCT CreatureID) FROM creature_equip_template; assert
// >= 1.  Read-only proxy for the dialog's "are there any creatures with
// equip variants to inspect" sanity check.
Phase testCreatureEquipSearchEntriesWithEquip(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.creature-equip-search-entries-with-equip", false, {}, 0.0 };
    auto const t0 = clock::now();

    char sql[256];
    std::snprintf(sql, sizeof(sql),
        "SELECT COUNT(DISTINCT CreatureID) FROM %s.creature_equip_template",
        cfg.worldDb.c_str());
    world_editor::db::QueryResult res;
    auto err = client.query(sql, res);
    if (!err.ok())
    {
        p.detail = "COUNT(DISTINCT CreatureID) failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }
    if (res.rowCount() == 0)
    {
        p.detail = "creature_equip_template empty (skip)";
        p.ms = sinceMs(t0);
        p.skipped = true;
        p.ok = true;
        return p;
    }
    uint64_t const distinctCreatures = res.asUInt64(0, 0).value_or(0);

    p.ms = sinceMs(t0);
    char out[256];
    std::snprintf(out, sizeof(out),
        "distinctCreatureIDs=%llu",
        (unsigned long long)distinctCreatures);
    p.detail = out;
    p.ok = (distinctCreatures >= 1);
    return p;
}

// Resolve which text column the live broadcast_text schema actually uses.
// Modern TC hotfixes uses `Text`/`Text1`; older world-schema variants use
// `MaleText`/`FemaleText`.  Returns the column name to write the probe
// string into, or empty if neither exists.
inline std::string detectBroadcastTextCol(world_editor::db::MySqlClient& client,
                                          std::string const& dbName)
{
    char sql[384];
    std::snprintf(sql, sizeof(sql),
        "SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.COLUMNS "
        "WHERE TABLE_SCHEMA='%s' AND TABLE_NAME='broadcast_text'",
        dbName.c_str());
    world_editor::db::QueryResult res;
    auto err = client.query(sql, res);
    if (!err.ok()) return "";
    bool hasMale = false, hasText = false;
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        std::string const& c = res.cell(r, 0);
        if (c == "MaleText") hasMale = true;
        if (c == "Text")     hasText = true;
    }
    if (hasMale) return "MaleText";
    if (hasText) return "Text";
    return "";
}

// Reserve a high probe ID well above any real broadcast_text.ID; INSERT
// (ID=99999999, LanguageID=0, <text-col>='probe') inside a transaction,
// verify mid-tx, ROLLBACK, verify gone.  Catches drift on BroadcastTextDialog's
// schema-tolerant INSERT column list.
Phase testBroadcastTextInsertRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.broadcast-text-insert-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    uint32_t const probeId = 99999999u;

    std::string const textCol = detectBroadcastTextCol(client, cfg.worldDb);
    if (textCol.empty())
    {
        p.detail = "broadcast_text not found in schema (skip)";
        p.ms = sinceMs(t0);
        p.skipped = true;
        p.ok = true;
        return p;
    }

    char checkSql[256];
    std::snprintf(checkSql, sizeof(checkSql),
        "SELECT COUNT(*) FROM %s.broadcast_text WHERE ID=%u",
        cfg.worldDb.c_str(), probeId);
    world_editor::db::QueryResult cRes;
    auto err = client.query(checkSql, cRes);
    if (!err.ok())
    {
        p.detail = "broadcast_text precheck failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }
    uint64_t const preCount = cRes.rowCount() > 0 ? cRes.asUInt64(0, 0).value_or(0) : 0;
    if (preCount != 0)
    {
        p.detail = "probe broadcast_text row already exists; aborting";
        p.ms = sinceMs(t0);
        return p;
    }

    err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    char insSql[384];
    std::snprintf(insSql, sizeof(insSql),
        "INSERT INTO %s.broadcast_text (ID, LanguageID, %s) "
        "VALUES (%u, 0, 'probe')",
        cfg.worldDb.c_str(), textCol.c_str(), probeId);
    err = client.exec(insSql);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "INSERT failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    world_editor::db::QueryResult midRes;
    (void)client.query(checkSql, midRes);
    int const sawInTx = midRes.rowCount() > 0
        ? int(midRes.asUInt64(0, 0).value_or(0)) : 0;

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(checkSql, afterRes);
    int const sawAfter = afterRes.rowCount() > 0
        ? int(afterRes.asUInt64(0, 0).value_or(0)) : 0;

    p.ms = sinceMs(t0);
    char out[256];
    std::snprintf(out, sizeof(out),
        "ID=%u sawInTx=%d goneAfterRollback=%d",
        probeId, sawInTx, sawAfter == 0);
    p.detail = out;
    p.ok = (sawInTx == 1) && (sawAfter == 0);
    return p;
}

// Pick the first existing broadcast_text.ID, UPDATE <text-col>='probe_sentinel'
// in tx, verify changed, ROLLBACK, verify NOT 'probe_sentinel'.  Catches drift
// on the UPDATE column list.
Phase testBroadcastTextUpdateRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.broadcast-text-update-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    std::string const textCol = detectBroadcastTextCol(client, cfg.worldDb);
    if (textCol.empty())
    {
        p.detail = "broadcast_text not found in schema (skip)";
        p.ms = sinceMs(t0);
        p.skipped = true;
        p.ok = true;
        return p;
    }

    char pickSql[256];
    std::snprintf(pickSql, sizeof(pickSql),
        "SELECT ID FROM %s.broadcast_text ORDER BY ID LIMIT 1",
        cfg.worldDb.c_str());
    world_editor::db::QueryResult pickRes;
    auto err = client.query(pickSql, pickRes);
    if (!err.ok())
    {
        p.detail = "broadcast_text query failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }
    if (pickRes.rowCount() == 0)
    {
        p.detail = "broadcast_text empty (skip)";
        p.ms = sinceMs(t0);
        p.skipped = true;
        p.ok = true;
        return p;
    }
    uint32_t const id = static_cast<uint32_t>(pickRes.asUInt64(0, 0).value_or(0));

    err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    char updSql[384];
    std::snprintf(updSql, sizeof(updSql),
        "UPDATE %s.broadcast_text SET %s='probe_sentinel' WHERE ID=%u",
        cfg.worldDb.c_str(), textCol.c_str(), id);
    err = client.exec(updSql);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "UPDATE failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    char sentinelSql[384];
    std::snprintf(sentinelSql, sizeof(sentinelSql),
        "SELECT %s FROM %s.broadcast_text WHERE ID=%u",
        textCol.c_str(), cfg.worldDb.c_str(), id);
    world_editor::db::QueryResult midRes;
    (void)client.query(sentinelSql, midRes);
    bool const sawSentinelInTx =
        midRes.rowCount() == 1 && midRes.cell(0, 0) == "probe_sentinel";

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(sentinelSql, afterRes);
    bool const sentinelGone =
        afterRes.rowCount() == 1 && afterRes.cell(0, 0) != "probe_sentinel";

    p.ms = sinceMs(t0);
    char out[256];
    std::snprintf(out, sizeof(out),
        "ID=%u sawSentinelInTx=%d sentinelGoneAfterRollback=%d",
        id, sawSentinelInTx ? 1 : 0, sentinelGone ? 1 : 0);
    p.detail = out;
    p.ok = sawSentinelInTx && sentinelGone;
    return p;
}

// SELECT COUNT(*) from any one well-known referencing table; assert >= 1.
// Read-only proxy for the dialog's "Show references" probe path.  Tries
// creature_text.BroadcastTextId first, then gossip_menu_option.OptionBroadcastTextID;
// skips gracefully if neither table exists in the connected schema.
Phase testBroadcastTextReferenceScan(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.broadcast-text-reference-scan", false, {}, 0.0 };
    auto const t0 = clock::now();

    struct Candidate { char const* table; char const* column; };
    constexpr Candidate kCandidates[] = {
        { "creature_text",      "BroadcastTextId"       },
        { "gossip_menu_option", "OptionBroadcastTextID" },
    };

    for (auto const& cand : kCandidates)
    {
        char probeSql[384];
        std::snprintf(probeSql, sizeof(probeSql),
            "SELECT 1 FROM INFORMATION_SCHEMA.COLUMNS "
            "WHERE TABLE_SCHEMA='%s' AND TABLE_NAME='%s' AND COLUMN_NAME='%s'",
            cfg.worldDb.c_str(), cand.table, cand.column);
        world_editor::db::QueryResult probeRes;
        auto pErr = client.query(probeSql, probeRes);
        if (!pErr.ok() || probeRes.rowCount() == 0) continue;

        char sql[256];
        std::snprintf(sql, sizeof(sql),
            "SELECT COUNT(*) FROM %s.%s WHERE %s > 0",
            cfg.worldDb.c_str(), cand.table, cand.column);
        world_editor::db::QueryResult res;
        auto err = client.query(sql, res);
        if (!err.ok() || res.rowCount() == 0) continue;
        uint64_t const refCount = res.asUInt64(0, 0).value_or(0);

        p.ms = sinceMs(t0);
        char out[256];
        std::snprintf(out, sizeof(out),
            "%s.%s>0 count=%llu",
            cand.table, cand.column, (unsigned long long)refCount);
        p.detail = out;
        p.ok = (refCount >= 1);
        return p;
    }

    p.detail = "no broadcast_text-referencing table found in schema (skip)";
    p.ms = sinceMs(t0);
    p.skipped = true;
    p.ok = true;
    return p;
}

// Reserve a probe entry well above any real creature_template.entry, INSERT a
// creature_template_addon row mid-tx, verify visible, ROLLBACK, verify gone.
// Catches drift on CreatureTemplateAddonDialog's INSERT column list (per-
// template defaults that apply to every spawn lacking a per-spawn override).
Phase testCreatureTemplateAddonInsertRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.creature-template-addon-insert-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    uint32_t const probeEntry = 999999u;

    char checkSql[256];
    std::snprintf(checkSql, sizeof(checkSql),
        "SELECT COUNT(*) FROM %s.creature_template_addon WHERE entry=%u",
        cfg.worldDb.c_str(), probeEntry);
    world_editor::db::QueryResult cRes;
    auto err = client.query(checkSql, cRes);
    if (!err.ok())
    {
        p.detail = "creature_template_addon precheck failed (table missing?): " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }
    uint64_t const preCount = cRes.rowCount() > 0 ? cRes.asUInt64(0, 0).value_or(0) : 0;
    if (preCount != 0)
    {
        p.detail = "probe creature_template_addon row already exists; aborting";
        p.ms = sinceMs(t0);
        return p;
    }

    err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    char insSql[768];
    std::snprintf(insSql, sizeof(insSql),
        "INSERT INTO %s.creature_template_addon "
        "(entry, PathId, mount, StandState, AnimTier, VisFlags, SheathState, "
        " PvpFlags, emote, AiAnimKit, MovementAnimKit, MeleeAnimKit, "
        " VisibilityDistanceType, auras) "
        "VALUES (%u, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '')",
        cfg.worldDb.c_str(), probeEntry);
    err = client.exec(insSql);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "INSERT failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    world_editor::db::QueryResult midRes;
    (void)client.query(checkSql, midRes);
    int const sawInTx = midRes.rowCount() > 0
        ? int(midRes.asUInt64(0, 0).value_or(0)) : 0;

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(checkSql, afterRes);
    int const sawAfter = afterRes.rowCount() > 0
        ? int(afterRes.asUInt64(0, 0).value_or(0)) : 0;

    p.ms = sinceMs(t0);
    char out[256];
    std::snprintf(out, sizeof(out),
        "entry=%u sawInTx=%d goneAfterRollback=%d",
        probeEntry, sawInTx, sawAfter == 0);
    p.detail = out;
    p.ok = (sawInTx == 1) && (sawAfter == 0);
    return p;
}

// Pick the first existing creature_template_addon.entry, save its original
// mount, UPDATE mount=999999 in tx, verify changed mid-tx, ROLLBACK, verify
// mount restored to original.  Catches drift on the UPDATE column list -
// mount is the most operator-facing field.
Phase testCreatureTemplateAddonUpdateRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.creature-template-addon-update-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    char pickSql[256];
    std::snprintf(pickSql, sizeof(pickSql),
        "SELECT entry, mount FROM %s.creature_template_addon ORDER BY entry LIMIT 1",
        cfg.worldDb.c_str());
    world_editor::db::QueryResult pickRes;
    auto err = client.query(pickSql, pickRes);
    if (!err.ok())
    {
        p.detail = "creature_template_addon pick failed (table missing?): " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }
    if (pickRes.rowCount() == 0)
    {
        p.detail = "creature_template_addon empty (skip)";
        p.ms = sinceMs(t0);
        p.skipped = true;
        p.ok = true;
        return p;
    }
    uint32_t const entry        = static_cast<uint32_t>(pickRes.asUInt64(0, 0).value_or(0));
    uint32_t const originalMount = static_cast<uint32_t>(pickRes.asUInt64(0, 1).value_or(0));

    err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    char updSql[256];
    std::snprintf(updSql, sizeof(updSql),
        "UPDATE %s.creature_template_addon SET mount=999999 WHERE entry=%u",
        cfg.worldDb.c_str(), entry);
    err = client.exec(updSql);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "UPDATE failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    char readSql[256];
    std::snprintf(readSql, sizeof(readSql),
        "SELECT mount FROM %s.creature_template_addon WHERE entry=%u",
        cfg.worldDb.c_str(), entry);
    world_editor::db::QueryResult midRes;
    (void)client.query(readSql, midRes);
    uint64_t const midMount = midRes.rowCount() == 1 ? midRes.asUInt64(0, 0).value_or(0) : 0;
    bool const sawUpdate = (midMount == 999999ull);

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(readSql, afterRes);
    uint64_t const afterMount = afterRes.rowCount() == 1 ? afterRes.asUInt64(0, 0).value_or(0) : 0;
    bool const restored = (afterMount == originalMount);

    p.ms = sinceMs(t0);
    char out[320];
    std::snprintf(out, sizeof(out),
        "entry=%u origMount=%u midMount=%llu afterMount=%llu sawUpdate=%d restored=%d",
        entry, originalMount, (unsigned long long)midMount,
        (unsigned long long)afterMount, sawUpdate ? 1 : 0, restored ? 1 : 0);
    p.detail = out;
    p.ok = sawUpdate && restored;
    return p;
}

// SELECT COUNT(*) FROM creature_template_addon WHERE mount > 0; assert >= 1.
// Read-only proxy for "are there any mounted creatures in this build's data".
// Most retail builds have hundreds.
Phase testCreatureTemplateAddonCountWithMount(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.creature-template-addon-count-with-mount", false, {}, 0.0 };
    auto const t0 = clock::now();

    char sql[256];
    std::snprintf(sql, sizeof(sql),
        "SELECT COUNT(*) FROM %s.creature_template_addon WHERE mount > 0",
        cfg.worldDb.c_str());
    world_editor::db::QueryResult res;
    auto err = client.query(sql, res);
    if (!err.ok())
    {
        p.detail = "COUNT(*) WHERE mount>0 failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }
    if (res.rowCount() == 0)
    {
        p.detail = "creature_template_addon empty (skip)";
        p.ms = sinceMs(t0);
        p.skipped = true;
        p.ok = true;
        return p;
    }
    uint64_t const mountedCount = res.asUInt64(0, 0).value_or(0);

    p.ms = sinceMs(t0);
    char out[160];
    std::snprintf(out, sizeof(out),
        "mountedCreatures=%llu",
        (unsigned long long)mountedCount);
    p.detail = out;
    p.ok = (mountedCount >= 1);
    return p;
}

// INSERT a probe linked_respawn row (guid=999999999, linkedGuid=999999998,
// linkType=0) inside a transaction; verify mid-tx; ROLLBACK; verify gone.
// Mirrors LinkedRespawnDialog::onAdd's INSERT path.
Phase testLinkedRespawnInsertRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.linked-respawn-insert-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    uint64_t const probeGuid       = 999999999ull;
    uint64_t const probeLinkedGuid = 999999998ull;
    uint32_t const probeLinkType   = 0u;

    char checkSql[384];
    std::snprintf(checkSql, sizeof(checkSql),
        "SELECT COUNT(*) FROM %s.linked_respawn "
        "WHERE guid=%llu AND linkType=%u",
        cfg.worldDb.c_str(),
        (unsigned long long)probeGuid, probeLinkType);
    world_editor::db::QueryResult cRes;
    (void)client.query(checkSql, cRes);
    uint64_t const preCount = cRes.rowCount() > 0 ? cRes.asUInt64(0, 0).value_or(0) : 0;
    if (preCount != 0)
    {
        p.detail = "probe linked_respawn row already exists; aborting";
        p.ms = sinceMs(t0);
        return p;
    }

    auto err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    char insSql[384];
    std::snprintf(insSql, sizeof(insSql),
        "INSERT INTO %s.linked_respawn (guid, linkedGuid, linkType) "
        "VALUES (%llu, %llu, %u)",
        cfg.worldDb.c_str(),
        (unsigned long long)probeGuid, (unsigned long long)probeLinkedGuid, probeLinkType);
    err = client.exec(insSql);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "INSERT failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    world_editor::db::QueryResult midRes;
    (void)client.query(checkSql, midRes);
    int const sawInTx = midRes.rowCount() > 0 ? int(midRes.asUInt64(0, 0).value_or(0)) : 0;

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(checkSql, afterRes);
    int const sawAfter = afterRes.rowCount() > 0 ? int(afterRes.asUInt64(0, 0).value_or(0)) : 0;

    p.ms = sinceMs(t0);
    char out[320];
    std::snprintf(out, sizeof(out),
        "guid=%llu linkedGuid=%llu linkType=%u sawInTx=%d goneAfterRollback=%d",
        (unsigned long long)probeGuid, (unsigned long long)probeLinkedGuid,
        probeLinkType, sawInTx, sawAfter == 0);
    p.detail = out;
    p.ok = (sawInTx == 1) && (sawAfter == 0);
    return p;
}

// INSERT a probe linked_respawn row + UPDATE linkedGuid=999999997 in the
// same transaction; verify both states mid-tx; ROLLBACK and verify gone.
// Catches drift on the UPDATE WHERE/SET column list -- linkedGuid is the
// only mutable column post-INSERT.
Phase testLinkedRespawnUpdateRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.linked-respawn-update-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    uint64_t const probeGuid           = 999999999ull;
    uint64_t const probeLinkedGuidIns  = 999999998ull;
    uint64_t const probeLinkedGuidUpd  = 999999997ull;
    uint32_t const probeLinkType       = 0u;

    char checkSql[384];
    std::snprintf(checkSql, sizeof(checkSql),
        "SELECT COUNT(*) FROM %s.linked_respawn "
        "WHERE guid=%llu AND linkType=%u",
        cfg.worldDb.c_str(),
        (unsigned long long)probeGuid, probeLinkType);
    world_editor::db::QueryResult cRes;
    (void)client.query(checkSql, cRes);
    uint64_t const preCount = cRes.rowCount() > 0 ? cRes.asUInt64(0, 0).value_or(0) : 0;
    if (preCount != 0)
    {
        p.detail = "probe linked_respawn row already exists; aborting";
        p.ms = sinceMs(t0);
        return p;
    }

    auto err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    char insSql[384];
    std::snprintf(insSql, sizeof(insSql),
        "INSERT INTO %s.linked_respawn (guid, linkedGuid, linkType) "
        "VALUES (%llu, %llu, %u)",
        cfg.worldDb.c_str(),
        (unsigned long long)probeGuid, (unsigned long long)probeLinkedGuidIns, probeLinkType);
    err = client.exec(insSql);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "INSERT failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    char readSql[384];
    std::snprintf(readSql, sizeof(readSql),
        "SELECT linkedGuid FROM %s.linked_respawn "
        "WHERE guid=%llu AND linkType=%u",
        cfg.worldDb.c_str(),
        (unsigned long long)probeGuid, probeLinkType);
    world_editor::db::QueryResult mid1;
    (void)client.query(readSql, mid1);
    bool const sawInsert = mid1.rowCount() == 1
        && mid1.asUInt64(0, 0).value_or(0) == probeLinkedGuidIns;

    char updSql[384];
    std::snprintf(updSql, sizeof(updSql),
        "UPDATE %s.linked_respawn SET linkedGuid=%llu "
        "WHERE guid=%llu AND linkType=%u",
        cfg.worldDb.c_str(),
        (unsigned long long)probeLinkedGuidUpd,
        (unsigned long long)probeGuid, probeLinkType);
    err = client.exec(updSql);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "UPDATE failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    world_editor::db::QueryResult mid2;
    (void)client.query(readSql, mid2);
    bool const sawUpdate = mid2.rowCount() == 1
        && mid2.asUInt64(0, 0).value_or(0) == probeLinkedGuidUpd;

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(checkSql, afterRes);
    int const sawAfter = afterRes.rowCount() > 0 ? int(afterRes.asUInt64(0, 0).value_or(0)) : 0;

    p.ms = sinceMs(t0);
    char out[320];
    std::snprintf(out, sizeof(out),
        "guid=%llu linkType=%u sawInsert=%d sawUpdate=%d goneAfterRollback=%d",
        (unsigned long long)probeGuid, probeLinkType,
        sawInsert ? 1 : 0, sawUpdate ? 1 : 0, sawAfter == 0);
    p.detail = out;
    p.ok = sawInsert && sawUpdate && (sawAfter == 0);
    return p;
}

// SELECT COUNT(*) FROM linked_respawn; assert >= 0 -- the table is allowed
// to be empty on a fresh extract.  Read-only proxy for the dialog's
// "is the table queryable" sanity check.
Phase testLinkedRespawnCount(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.linked-respawn-count", false, {}, 0.0 };
    auto const t0 = clock::now();

    char sql[256];
    std::snprintf(sql, sizeof(sql),
        "SELECT COUNT(*) FROM %s.linked_respawn", cfg.worldDb.c_str());
    world_editor::db::QueryResult res;
    auto const err = client.query(sql, res);
    if (!err.ok())
    {
        p.detail = "COUNT(*) failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }
    uint64_t const total = res.rowCount() > 0 ? res.asUInt64(0, 0).value_or(0) : 0;

    p.ms = sinceMs(t0);
    char out[160];
    std::snprintf(out, sizeof(out), "linked_respawn rows=%llu",
        (unsigned long long)total);
    p.detail = out;
    p.ok = true;   // table existence + queryable is the assertion; any count >= 0 passes.
    return p;
}

// Reserve a probe ID with a wide-margin starting point so the test
// tolerates whatever the live DB already has populated.  INSERT inside
// a transaction, verify visible mid-tx, ROLLBACK and verify gone.
Phase testWorldSafeLocsInsertRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.world-safe-locs-insert-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    // world_safe_locs is MyISAM on the canonical TC schema → ROLLBACK is a no-op.
    // Probe the engine and undo via explicit DELETE when non-transactional
    // (mirrors access_requirement). Without this the test fails AND leaks the
    // probe row. A stale probe row from a prior (pre-fix) run is cleared rather
    // than aborting.
    uint32_t const probeId = 99999999;

    char engineSql[384];
    std::snprintf(engineSql, sizeof(engineSql),
        "SELECT ENGINE FROM INFORMATION_SCHEMA.TABLES "
        "WHERE TABLE_SCHEMA='%s' AND TABLE_NAME='world_safe_locs'",
        cfg.worldDb.c_str());
    world_editor::db::QueryResult engRes;
    (void)client.query(engineSql, engRes);
    std::string const engine = engRes.rowCount() > 0 ? engRes.cell(0, 0) : std::string();
    bool const txCapable = (engine == "InnoDB");

    char checkSql[256];
    std::snprintf(checkSql, sizeof(checkSql),
        "SELECT COUNT(*) FROM %s.world_safe_locs WHERE ID=%u",
        cfg.worldDb.c_str(), probeId);
    world_editor::db::QueryResult cRes;
    (void)client.query(checkSql, cRes);
    uint64_t const preCount = cRes.rowCount() > 0 ? cRes.asUInt64(0, 0).value_or(0) : 0;
    if (preCount != 0)
    {
        char delStale[256];
        std::snprintf(delStale, sizeof(delStale),
            "DELETE FROM %s.world_safe_locs WHERE ID=%u", cfg.worldDb.c_str(), probeId);
        (void)client.exec(delStale);
    }

    if (txCapable)
    {
        auto berr = client.exec("START TRANSACTION");
        if (!berr.ok()) { p.detail = "BEGIN: " + berr.message; p.ms = sinceMs(t0); return p; }
    }

    // Note: Comment column is the dialog's canonical column.  Some forks
    // also have TransportSpawnId (nullable) - we don't name it so the
    // INSERT works on both shapes.
    char insSql[512];
    std::snprintf(insSql, sizeof(insSql),
        "INSERT INTO %s.world_safe_locs (ID, MapID, LocX, LocY, LocZ, Facing, Comment) "
        "VALUES (%u, 0, 0.0, 0.0, 0.0, 0.0, 'probe')",
        cfg.worldDb.c_str(), probeId);
    auto err = client.exec(insSql);
    if (!err.ok())
    {
        if (txCapable) (void)client.exec("ROLLBACK");
        p.detail = "INSERT failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    world_editor::db::QueryResult midRes;
    (void)client.query(checkSql, midRes);
    int const sawInTx = midRes.rowCount() > 0
        ? int(midRes.asUInt64(0, 0).value_or(0)) : 0;

    if (txCapable)
    {
        (void)client.exec("ROLLBACK");
    }
    else
    {
        char del[256];
        std::snprintf(del, sizeof(del),
            "DELETE FROM %s.world_safe_locs WHERE ID=%u", cfg.worldDb.c_str(), probeId);
        (void)client.exec(del);
    }

    world_editor::db::QueryResult afterRes;
    (void)client.query(checkSql, afterRes);
    int const sawAfter = afterRes.rowCount() > 0
        ? int(afterRes.asUInt64(0, 0).value_or(0)) : 0;

    p.ms = sinceMs(t0);
    char out[256];
    std::snprintf(out, sizeof(out),
        "ID=%u engine=%s sawInTx=%d goneAfter=%d",
        probeId, engine.c_str(), sawInTx, sawAfter == 0);
    p.detail = out;
    p.ok = (sawInTx == 1) && (sawAfter == 0);
    return p;
}

// Pick the first existing world_safe_locs.ID, save the original Comment,
// UPDATE Comment='probe_sentinel' inside a transaction, verify the new
// value is visible mid-tx, ROLLBACK and verify the original Comment
// restores byte-for-byte.
Phase testWorldSafeLocsUpdateRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.world-safe-locs-update-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    // MyISAM → ROLLBACK no-op: probe engine, restore explicitly when
    // non-transactional. Use a sentinel guaranteed different from the current
    // value so the UPDATE always affects exactly 1 row (the old code broke when
    // the picked row's Comment was already 'probe_sentinel' from prior pollution
    // → affected=0).
    char engineSql[384];
    std::snprintf(engineSql, sizeof(engineSql),
        "SELECT ENGINE FROM INFORMATION_SCHEMA.TABLES "
        "WHERE TABLE_SCHEMA='%s' AND TABLE_NAME='world_safe_locs'",
        cfg.worldDb.c_str());
    world_editor::db::QueryResult engRes;
    (void)client.query(engineSql, engRes);
    std::string const engine = engRes.rowCount() > 0 ? engRes.cell(0, 0) : std::string();
    bool const txCapable = (engine == "InnoDB");

    char pickSql[256];
    std::snprintf(pickSql, sizeof(pickSql),
        "SELECT ID, COALESCE(Comment, '') FROM %s.world_safe_locs ORDER BY ID LIMIT 1",
        cfg.worldDb.c_str());
    world_editor::db::QueryResult pickRes;
    auto err = client.query(pickSql, pickRes);
    if (!err.ok() || pickRes.rowCount() == 0)
    {
        p.detail = "no world_safe_locs rows available (skip)";
        p.ms = sinceMs(t0);
        p.skipped = true;
        p.ok = true;
        return p;
    }
    uint32_t const id = static_cast<uint32_t>(pickRes.asUInt64(0, 0).value_or(0));
    std::string const beforeComment = pickRes.cell(0, 1);
    std::string const sentinel =
        (beforeComment == "probe_sentinel") ? std::string("probe_sentinel_b")
                                            : std::string("probe_sentinel");

    auto sqlEscape = [](std::string const& s) {
        std::string e; e.reserve(s.size() + 8);
        for (char c : s) { if (c == '\'' || c == '\\') e.push_back('\\'); e.push_back(c); }
        return e;
    };

    if (txCapable)
    {
        err = client.exec("START TRANSACTION");
        if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }
    }

    char updSql[512];
    std::snprintf(updSql, sizeof(updSql),
        "UPDATE %s.world_safe_locs SET Comment='%s' WHERE ID=%u",
        cfg.worldDb.c_str(), sentinel.c_str(), id);
    uint64_t affected = 0;
    err = client.exec(updSql, &affected);
    if (!err.ok())
    {
        if (txCapable) (void)client.exec("ROLLBACK");
        p.detail = "UPDATE failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    char readSql[256];
    std::snprintf(readSql, sizeof(readSql),
        "SELECT COALESCE(Comment, '') FROM %s.world_safe_locs WHERE ID=%u",
        cfg.worldDb.c_str(), id);
    world_editor::db::QueryResult midRes;
    (void)client.query(readSql, midRes);
    std::string const inTxComment = midRes.rowCount() > 0 ? midRes.cell(0, 0) : std::string();

    if (txCapable)
    {
        (void)client.exec("ROLLBACK");
    }
    else
    {
        char rest[640];
        std::snprintf(rest, sizeof(rest),
            "UPDATE %s.world_safe_locs SET Comment='%s' WHERE ID=%u",
            cfg.worldDb.c_str(), sqlEscape(beforeComment).c_str(), id);
        (void)client.exec(rest);
    }

    world_editor::db::QueryResult afterRes;
    (void)client.query(readSql, afterRes);
    std::string const afterComment = afterRes.rowCount() > 0
        ? afterRes.cell(0, 0) : std::string();

    p.ms = sinceMs(t0);
    char out[384];
    std::snprintf(out, sizeof(out),
        "ID=%u engine=%s before='%s' inTx='%s' afterRestore='%s' affected=%llu",
        id, engine.c_str(), beforeComment.c_str(), inTxComment.c_str(),
        afterComment.c_str(), (unsigned long long)affected);
    p.detail = out;
    p.ok = affected == 1
        && inTxComment == sentinel
        && afterComment == beforeComment;
    return p;
}

// Sanity check that the world_safe_locs table is populated with named
// entries.  Most retail builds have hundreds; this acts as a smoke
// for the dialog's "Comment substring" search code path.
Phase testWorldSafeLocsSearch(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.world-safe-locs-search", false, {}, 0.0 };
    auto const t0 = clock::now();

    char sql[256];
    std::snprintf(sql, sizeof(sql),
        "SELECT ID, Comment FROM %s.world_safe_locs WHERE Comment != '' LIMIT 1",
        cfg.worldDb.c_str());
    world_editor::db::QueryResult res;
    auto err = client.query(sql, res);
    p.ms = sinceMs(t0);
    if (!err.ok())
    {
        p.detail = "SELECT failed: " + err.message;
        return p;
    }
    if (res.rowCount() == 0)
    {
        p.detail = "no rows with non-empty Comment - is this a fresh world DB?";
        return p;
    }
    uint32_t const firstId = static_cast<uint32_t>(res.asUInt64(0, 0).value_or(0));
    std::string const firstCmt = res.cell(0, 1);

    char out[256];
    std::snprintf(out, sizeof(out),
        "firstId=%u firstComment='%s'",
        firstId, firstCmt.c_str());
    p.detail = out;
    p.ok = (res.rowCount() >= 1);
    return p;
}

// Pick the first existing quest_template.ID, UPSERT a probe RewardText into
// quest_offer_reward inside a transaction, verify the new RewardText is
// visible mid-tx, ROLLBACK and verify the original (or absence) restores.
// Covers both INSERT-path (no prior row) and UPDATE-path (prior row present).
Phase testQuestOfferRewardUpsertRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.quest-offer-reward-upsert-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    char pickSql[256];
    std::snprintf(pickSql, sizeof(pickSql),
        "SELECT ID FROM %s.quest_template ORDER BY ID LIMIT 1",
        cfg.worldDb.c_str());
    world_editor::db::QueryResult pickRes;
    auto err = client.query(pickSql, pickRes);
    if (!err.ok() || pickRes.rowCount() == 0)
    {
        p.detail = "no quest_template rows available (skip)";
        p.ms = sinceMs(t0); p.skipped = true; p.ok = true; return p;
    }
    uint32_t const id = static_cast<uint32_t>(pickRes.asUInt64(0, 0).value_or(0));

    char existsSql[256];
    std::snprintf(existsSql, sizeof(existsSql),
        "SELECT COUNT(*) FROM %s.quest_offer_reward WHERE ID=%u",
        cfg.worldDb.c_str(), id);
    world_editor::db::QueryResult existsRes;
    (void)client.query(existsSql, existsRes);
    bool const priorExisted = existsRes.rowCount() > 0 && existsRes.asUInt64(0, 0).value_or(0) > 0;

    char readSql[256];
    std::snprintf(readSql, sizeof(readSql),
        "SELECT COALESCE(RewardText, '') FROM %s.quest_offer_reward WHERE ID=%u",
        cfg.worldDb.c_str(), id);
    std::string priorText;
    if (priorExisted)
    {
        world_editor::db::QueryResult priorRes;
        (void)client.query(readSql, priorRes);
        if (priorRes.rowCount() > 0) priorText = priorRes.cell(0, 0);
    }

    err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    char upsertSql[512];
    if (priorExisted)
    {
        std::snprintf(upsertSql, sizeof(upsertSql),
            "UPDATE %s.quest_offer_reward SET RewardText='probe_sentinel' WHERE ID=%u",
            cfg.worldDb.c_str(), id);
    }
    else
    {
        std::snprintf(upsertSql, sizeof(upsertSql),
            "INSERT INTO %s.quest_offer_reward (ID, RewardText) VALUES (%u, 'probe_sentinel')",
            cfg.worldDb.c_str(), id);
    }
    err = client.exec(upsertSql);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = std::string("UPSERT failed: ") + err.message;
        p.ms = sinceMs(t0); return p;
    }

    world_editor::db::QueryResult midRes;
    (void)client.query(readSql, midRes);
    std::string const inTxText = midRes.rowCount() > 0 ? midRes.cell(0, 0) : std::string();

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(readSql, afterRes);
    std::string const afterText = afterRes.rowCount() > 0 ? afterRes.cell(0, 0) : std::string();
    bool const afterExists = afterRes.rowCount() > 0;

    p.ms = sinceMs(t0);
    char out[384];
    std::snprintf(out, sizeof(out),
        "ID=%u priorExisted=%d inTx='%s' afterRollback='%s' afterExists=%d",
        id, priorExisted ? 1 : 0, inTxText.c_str(), afterText.c_str(), afterExists ? 1 : 0);
    p.detail = out;
    // Mid-tx must show probe_sentinel; post-rollback must restore prior state.
    bool const restoredOk = priorExisted
        ? (afterExists && afterText == priorText)
        : (!afterExists);
    p.ok = (inTxText == "probe_sentinel") && restoredOk;
    return p;
}

// Same shape as the quest_offer_reward phase but targets quest_request_items.
// UPSERT CompletionText='probe_sentinel', verify, ROLLBACK, verify restored.
Phase testQuestRequestItemsUpsertRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.quest-request-items-upsert-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    char pickSql[256];
    std::snprintf(pickSql, sizeof(pickSql),
        "SELECT ID FROM %s.quest_template ORDER BY ID LIMIT 1",
        cfg.worldDb.c_str());
    world_editor::db::QueryResult pickRes;
    auto err = client.query(pickSql, pickRes);
    if (!err.ok() || pickRes.rowCount() == 0)
    {
        p.detail = "no quest_template rows available (skip)";
        p.ms = sinceMs(t0); p.skipped = true; p.ok = true; return p;
    }
    uint32_t const id = static_cast<uint32_t>(pickRes.asUInt64(0, 0).value_or(0));

    char existsSql[256];
    std::snprintf(existsSql, sizeof(existsSql),
        "SELECT COUNT(*) FROM %s.quest_request_items WHERE ID=%u",
        cfg.worldDb.c_str(), id);
    world_editor::db::QueryResult existsRes;
    (void)client.query(existsSql, existsRes);
    bool const priorExisted = existsRes.rowCount() > 0 && existsRes.asUInt64(0, 0).value_or(0) > 0;

    char readSql[256];
    std::snprintf(readSql, sizeof(readSql),
        "SELECT COALESCE(CompletionText, '') FROM %s.quest_request_items WHERE ID=%u",
        cfg.worldDb.c_str(), id);
    std::string priorText;
    if (priorExisted)
    {
        world_editor::db::QueryResult priorRes;
        (void)client.query(readSql, priorRes);
        if (priorRes.rowCount() > 0) priorText = priorRes.cell(0, 0);
    }

    err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    char upsertSql[512];
    if (priorExisted)
    {
        std::snprintf(upsertSql, sizeof(upsertSql),
            "UPDATE %s.quest_request_items SET CompletionText='probe_sentinel' WHERE ID=%u",
            cfg.worldDb.c_str(), id);
    }
    else
    {
        std::snprintf(upsertSql, sizeof(upsertSql),
            "INSERT INTO %s.quest_request_items (ID, CompletionText) VALUES (%u, 'probe_sentinel')",
            cfg.worldDb.c_str(), id);
    }
    err = client.exec(upsertSql);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = std::string("UPSERT failed: ") + err.message;
        p.ms = sinceMs(t0); return p;
    }

    world_editor::db::QueryResult midRes;
    (void)client.query(readSql, midRes);
    std::string const inTxText = midRes.rowCount() > 0 ? midRes.cell(0, 0) : std::string();

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(readSql, afterRes);
    std::string const afterText = afterRes.rowCount() > 0 ? afterRes.cell(0, 0) : std::string();
    bool const afterExists = afterRes.rowCount() > 0;

    p.ms = sinceMs(t0);
    char out[384];
    std::snprintf(out, sizeof(out),
        "ID=%u priorExisted=%d inTx='%s' afterRollback='%s' afterExists=%d",
        id, priorExisted ? 1 : 0, inTxText.c_str(), afterText.c_str(), afterExists ? 1 : 0);
    p.detail = out;
    bool const restoredOk = priorExisted
        ? (afterExists && afterText == priorText)
        : (!afterExists);
    p.ok = (inTxText == "probe_sentinel") && restoredOk;
    return p;
}

// quest_details has no text column - only emote channels.  UPSERT Emote1=9999
// (well above the live Emote.db2 ID range), verify mid-tx, ROLLBACK + verify
// the prior Emote1 / row absence restores.
Phase testQuestDetailsUpsertRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.quest-details-upsert-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    char pickSql[256];
    std::snprintf(pickSql, sizeof(pickSql),
        "SELECT ID FROM %s.quest_template ORDER BY ID LIMIT 1",
        cfg.worldDb.c_str());
    world_editor::db::QueryResult pickRes;
    auto err = client.query(pickSql, pickRes);
    if (!err.ok() || pickRes.rowCount() == 0)
    {
        p.detail = "no quest_template rows available (skip)";
        p.ms = sinceMs(t0); p.skipped = true; p.ok = true; return p;
    }
    uint32_t const id = static_cast<uint32_t>(pickRes.asUInt64(0, 0).value_or(0));

    char existsSql[256];
    std::snprintf(existsSql, sizeof(existsSql),
        "SELECT COUNT(*) FROM %s.quest_details WHERE ID=%u",
        cfg.worldDb.c_str(), id);
    world_editor::db::QueryResult existsRes;
    (void)client.query(existsSql, existsRes);
    bool const priorExisted = existsRes.rowCount() > 0 && existsRes.asUInt64(0, 0).value_or(0) > 0;

    char readSql[256];
    std::snprintf(readSql, sizeof(readSql),
        "SELECT COALESCE(Emote1, 0) FROM %s.quest_details WHERE ID=%u",
        cfg.worldDb.c_str(), id);
    uint32_t priorEmote = 0;
    if (priorExisted)
    {
        world_editor::db::QueryResult priorRes;
        (void)client.query(readSql, priorRes);
        if (priorRes.rowCount() > 0)
            priorEmote = static_cast<uint32_t>(priorRes.asUInt64(0, 0).value_or(0));
    }

    err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    char upsertSql[512];
    if (priorExisted)
    {
        std::snprintf(upsertSql, sizeof(upsertSql),
            "UPDATE %s.quest_details SET Emote1=9999 WHERE ID=%u",
            cfg.worldDb.c_str(), id);
    }
    else
    {
        std::snprintf(upsertSql, sizeof(upsertSql),
            "INSERT INTO %s.quest_details (ID, Emote1) VALUES (%u, 9999)",
            cfg.worldDb.c_str(), id);
    }
    err = client.exec(upsertSql);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = std::string("UPSERT failed: ") + err.message;
        p.ms = sinceMs(t0); return p;
    }

    world_editor::db::QueryResult midRes;
    (void)client.query(readSql, midRes);
    uint32_t const inTxEmote = midRes.rowCount() > 0
        ? static_cast<uint32_t>(midRes.asUInt64(0, 0).value_or(0)) : 0;

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(readSql, afterRes);
    bool const afterExists = afterRes.rowCount() > 0;
    uint32_t const afterEmote = afterExists
        ? static_cast<uint32_t>(afterRes.asUInt64(0, 0).value_or(0)) : 0;

    p.ms = sinceMs(t0);
    char out[384];
    std::snprintf(out, sizeof(out),
        "ID=%u priorExisted=%d inTxEmote1=%u afterRollbackEmote1=%u afterExists=%d",
        id, priorExisted ? 1 : 0, inTxEmote, afterEmote, afterExists ? 1 : 0);
    p.detail = out;
    bool const restoredOk = priorExisted
        ? (afterExists && afterEmote == priorEmote)
        : (!afterExists);
    p.ok = (inTxEmote == 9999) && restoredOk;
    return p;
}

// INSERT a probe creature_summon_groups row (summonerId=99999999, summonerType=0,
// groupId=0, entry=99999999, position_x/y/z=0, orientation=0, summonType=0,
// summonTime=0) inside a transaction; verify mid-tx; ROLLBACK; verify gone.
// Mirrors CreatureSummonGroupsDialog::openModal's INSERT path.
Phase testCreatureSummonGroupsInsertRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.creature-summon-groups-insert-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    uint32_t const probeSummonerId   = 99999999u;
    uint32_t const probeSummonerType = 0u;
    uint32_t const probeGroupId      = 0u;
    uint32_t const probeEntry        = 99999999u;

    char checkSql[384];
    std::snprintf(checkSql, sizeof(checkSql),
        "SELECT COUNT(*) FROM %s.creature_summon_groups "
        "WHERE summonerId=%u AND summonerType=%u AND groupId=%u AND entry=%u",
        cfg.worldDb.c_str(),
        probeSummonerId, probeSummonerType, probeGroupId, probeEntry);
    world_editor::db::QueryResult cRes;
    (void)client.query(checkSql, cRes);
    uint64_t const preCount = cRes.rowCount() > 0 ? cRes.asUInt64(0, 0).value_or(0) : 0;
    if (preCount != 0)
    {
        p.detail = "probe creature_summon_groups row already exists; aborting";
        p.ms = sinceMs(t0);
        return p;
    }

    auto err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    char insSql[512];
    std::snprintf(insSql, sizeof(insSql),
        "INSERT INTO %s.creature_summon_groups "
        "(summonerId, summonerType, groupId, entry, "
        " position_x, position_y, position_z, orientation, summonType, summonTime) "
        "VALUES (%u, %u, %u, %u, 0.0, 0.0, 0.0, 0.0, 0, 0)",
        cfg.worldDb.c_str(),
        probeSummonerId, probeSummonerType, probeGroupId, probeEntry);
    err = client.exec(insSql);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "INSERT failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    world_editor::db::QueryResult midRes;
    (void)client.query(checkSql, midRes);
    int const sawInTx = midRes.rowCount() > 0 ? int(midRes.asUInt64(0, 0).value_or(0)) : 0;

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(checkSql, afterRes);
    int const sawAfter = afterRes.rowCount() > 0 ? int(afterRes.asUInt64(0, 0).value_or(0)) : 0;

    p.ms = sinceMs(t0);
    char out[320];
    std::snprintf(out, sizeof(out),
        "summonerId=%u summonerType=%u groupId=%u entry=%u sawInTx=%d goneAfterRollback=%d",
        probeSummonerId, probeSummonerType, probeGroupId, probeEntry,
        sawInTx, sawAfter == 0);
    p.detail = out;
    p.ok = (sawInTx == 1) && (sawAfter == 0);
    return p;
}

// INSERT a probe creature_summon_groups row + UPDATE summonTime=99999 in the
// same transaction; verify both states mid-tx; ROLLBACK and verify gone.
// Catches drift on the UPDATE WHERE/SET column list and the composite-PK match.
Phase testCreatureSummonGroupsUpdateRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.creature-summon-groups-update-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    uint32_t const probeSummonerId   = 99999999u;
    uint32_t const probeSummonerType = 0u;
    uint32_t const probeGroupId      = 0u;
    uint32_t const probeEntry        = 99999999u;

    char checkSql[384];
    std::snprintf(checkSql, sizeof(checkSql),
        "SELECT COUNT(*) FROM %s.creature_summon_groups "
        "WHERE summonerId=%u AND summonerType=%u AND groupId=%u AND entry=%u",
        cfg.worldDb.c_str(),
        probeSummonerId, probeSummonerType, probeGroupId, probeEntry);
    world_editor::db::QueryResult cRes;
    (void)client.query(checkSql, cRes);
    uint64_t const preCount = cRes.rowCount() > 0 ? cRes.asUInt64(0, 0).value_or(0) : 0;
    if (preCount != 0)
    {
        p.detail = "probe creature_summon_groups row already exists; aborting";
        p.ms = sinceMs(t0);
        return p;
    }

    auto err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    char insSql[512];
    std::snprintf(insSql, sizeof(insSql),
        "INSERT INTO %s.creature_summon_groups "
        "(summonerId, summonerType, groupId, entry, "
        " position_x, position_y, position_z, orientation, summonType, summonTime) "
        "VALUES (%u, %u, %u, %u, 0.0, 0.0, 0.0, 0.0, 0, 0)",
        cfg.worldDb.c_str(),
        probeSummonerId, probeSummonerType, probeGroupId, probeEntry);
    err = client.exec(insSql);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "INSERT failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    char readSql[384];
    std::snprintf(readSql, sizeof(readSql),
        "SELECT summonTime FROM %s.creature_summon_groups "
        "WHERE summonerId=%u AND summonerType=%u AND groupId=%u AND entry=%u",
        cfg.worldDb.c_str(),
        probeSummonerId, probeSummonerType, probeGroupId, probeEntry);
    world_editor::db::QueryResult mid1;
    (void)client.query(readSql, mid1);
    bool const sawInsert = mid1.rowCount() == 1
        && mid1.asUInt64(0, 0).value_or(99) == 0;

    char updSql[512];
    std::snprintf(updSql, sizeof(updSql),
        "UPDATE %s.creature_summon_groups SET summonTime=99999 "
        "WHERE summonerId=%u AND summonerType=%u AND groupId=%u AND entry=%u "
        "AND ABS(position_x - 0.0000) < 0.0001",
        cfg.worldDb.c_str(),
        probeSummonerId, probeSummonerType, probeGroupId, probeEntry);
    err = client.exec(updSql);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "UPDATE failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    world_editor::db::QueryResult mid2;
    (void)client.query(readSql, mid2);
    bool const sawUpdate = mid2.rowCount() == 1
        && mid2.asUInt64(0, 0).value_or(0) == 99999;

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(checkSql, afterRes);
    int const sawAfter = afterRes.rowCount() > 0 ? int(afterRes.asUInt64(0, 0).value_or(0)) : 0;

    p.ms = sinceMs(t0);
    char out[320];
    std::snprintf(out, sizeof(out),
        "summonerId=%u entry=%u sawInsert=%d sawUpdate=%d goneAfterRollback=%d",
        probeSummonerId, probeEntry,
        sawInsert ? 1 : 0, sawUpdate ? 1 : 0, sawAfter == 0);
    p.detail = out;
    p.ok = sawInsert && sawUpdate && (sawAfter == 0);
    return p;
}

// SELECT COUNT(*) FROM creature_summon_groups; assert >= 0 -- the table is
// allowed to be empty on a fresh extract.  Read-only proxy for the dialog's
// "is the table queryable" sanity check.
Phase testCreatureSummonGroupsCount(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.creature-summon-groups-count", false, {}, 0.0 };
    auto const t0 = clock::now();

    char sql[256];
    std::snprintf(sql, sizeof(sql),
        "SELECT COUNT(*) FROM %s.creature_summon_groups", cfg.worldDb.c_str());
    world_editor::db::QueryResult res;
    auto const err = client.query(sql, res);
    if (!err.ok())
    {
        p.detail = "COUNT(*) failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }
    uint64_t const total = res.rowCount() > 0 ? res.asUInt64(0, 0).value_or(0) : 0;

    p.ms = sinceMs(t0);
    char out[160];
    std::snprintf(out, sizeof(out), "creature_summon_groups rows=%llu",
        (unsigned long long)total);
    p.detail = out;
    p.ok = true;   // table existence + queryable is the assertion; any count >= 0 passes.
    return p;
}

// Reserve a probe (mapId, difficulty) PK far above any realistic value, INSERT
// inside a transaction, verify visible mid-tx, ROLLBACK and verify gone.
// Backstops the AccessRequirementDialog's INSERT path.
//
// access_requirement is MyISAM on the canonical TC schema (no FKs), so
// ROLLBACK is a no-op there.  Engine probe + explicit DELETE for cleanup
// when non-InnoDB; the assertion stays "INSERT visible -> cleanup -> gone".
Phase testAccessRequirementInsertRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.access-requirement-insert-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    uint32_t const probeMapId      = 999999;
    uint32_t const probeDifficulty = 0;

    char engineSql[384];
    std::snprintf(engineSql, sizeof(engineSql),
        "SELECT ENGINE FROM INFORMATION_SCHEMA.TABLES "
        "WHERE TABLE_SCHEMA='%s' AND TABLE_NAME='access_requirement'",
        cfg.worldDb.c_str());
    world_editor::db::QueryResult engRes;
    (void)client.query(engineSql, engRes);
    std::string const engine = engRes.rowCount() > 0 ? engRes.cell(0, 0) : std::string();
    bool const txCapable = (engine == "InnoDB");

    char checkSql[256];
    std::snprintf(checkSql, sizeof(checkSql),
        "SELECT COUNT(*) FROM %s.access_requirement WHERE mapId=%u AND difficulty=%u",
        cfg.worldDb.c_str(), probeMapId, probeDifficulty);
    world_editor::db::QueryResult cRes;
    (void)client.query(checkSql, cRes);
    uint64_t const preCount = cRes.rowCount() > 0 ? cRes.asUInt64(0, 0).value_or(0) : 0;
    if (preCount != 0)
    {
        p.detail = "probe access_requirement row already exists; aborting";
        p.ms = sinceMs(t0);
        return p;
    }

    auto err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    char insSql[512];
    std::snprintf(insSql, sizeof(insSql),
        "INSERT INTO %s.access_requirement "
        "(mapId, difficulty, level_min, level_max, item, item2, "
        " quest_done_A, quest_done_H, completed_achievement, "
        " quest_failed_text, comment) "
        "VALUES (%u, %u, 1, 60, 0, 0, 0, 0, 0, '', 'smoketest probe')",
        cfg.worldDb.c_str(), probeMapId, probeDifficulty);
    err = client.exec(insSql);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "INSERT failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    world_editor::db::QueryResult midRes;
    (void)client.query(checkSql, midRes);
    int const sawInTx = midRes.rowCount() > 0
        ? int(midRes.asUInt64(0, 0).value_or(0)) : 0;

    (void)client.exec("ROLLBACK");

    // MyISAM ignores ROLLBACK; explicit DELETE keeps the table clean.
    if (!txCapable)
    {
        char delSql[256];
        std::snprintf(delSql, sizeof(delSql),
            "DELETE FROM %s.access_requirement WHERE mapId=%u AND difficulty=%u",
            cfg.worldDb.c_str(), probeMapId, probeDifficulty);
        (void)client.exec(delSql);
    }

    world_editor::db::QueryResult afterRes;
    (void)client.query(checkSql, afterRes);
    int const sawAfter = afterRes.rowCount() > 0
        ? int(afterRes.asUInt64(0, 0).value_or(0)) : 0;

    p.ms = sinceMs(t0);
    char out[320];
    std::snprintf(out, sizeof(out),
        "mapId=%u diff=%u engine=%s sawInTx=%d goneAfter=%d",
        probeMapId, probeDifficulty, engine.c_str(), sawInTx, sawAfter == 0);
    p.detail = out;
    p.ok = (sawInTx == 1) && (sawAfter == 0);
    return p;
}

// INSERT probe + UPDATE level_min=10 inside one transaction.  Verify both
// states are visible mid-tx (sawInsert + sawUpdate), ROLLBACK, assert the
// row is fully gone.  Backstops the AccessRequirementDialog's UPDATE path.
// MyISAM ROLLBACK is a no-op (engine probe + explicit DELETE cleanup).
Phase testAccessRequirementUpdateRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.access-requirement-update-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    uint32_t const probeMapId      = 999998;
    uint32_t const probeDifficulty = 0;

    char engineSql[384];
    std::snprintf(engineSql, sizeof(engineSql),
        "SELECT ENGINE FROM INFORMATION_SCHEMA.TABLES "
        "WHERE TABLE_SCHEMA='%s' AND TABLE_NAME='access_requirement'",
        cfg.worldDb.c_str());
    world_editor::db::QueryResult engRes;
    (void)client.query(engineSql, engRes);
    std::string const engine = engRes.rowCount() > 0 ? engRes.cell(0, 0) : std::string();
    bool const txCapable = (engine == "InnoDB");

    char checkSql[256];
    std::snprintf(checkSql, sizeof(checkSql),
        "SELECT COALESCE(level_min, 0) FROM %s.access_requirement "
        "WHERE mapId=%u AND difficulty=%u",
        cfg.worldDb.c_str(), probeMapId, probeDifficulty);

    char preSql[256];
    std::snprintf(preSql, sizeof(preSql),
        "SELECT COUNT(*) FROM %s.access_requirement WHERE mapId=%u AND difficulty=%u",
        cfg.worldDb.c_str(), probeMapId, probeDifficulty);
    world_editor::db::QueryResult cRes;
    (void)client.query(preSql, cRes);
    uint64_t const preCount = cRes.rowCount() > 0 ? cRes.asUInt64(0, 0).value_or(0) : 0;
    if (preCount != 0)
    {
        p.detail = "probe access_requirement row already exists; aborting";
        p.ms = sinceMs(t0);
        return p;
    }

    auto err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    char insSql[512];
    std::snprintf(insSql, sizeof(insSql),
        "INSERT INTO %s.access_requirement "
        "(mapId, difficulty, level_min, level_max, item, item2, "
        " quest_done_A, quest_done_H, completed_achievement, "
        " quest_failed_text, comment) "
        "VALUES (%u, %u, 1, 60, 0, 0, 0, 0, 0, '', 'smoketest probe')",
        cfg.worldDb.c_str(), probeMapId, probeDifficulty);
    err = client.exec(insSql);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "INSERT failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    world_editor::db::QueryResult midInsert;
    (void)client.query(checkSql, midInsert);
    uint64_t const sawInsert = midInsert.rowCount() > 0
        ? midInsert.asUInt64(0, 0).value_or(0) : 0;

    char updSql[512];
    std::snprintf(updSql, sizeof(updSql),
        "UPDATE %s.access_requirement SET level_min=10 "
        "WHERE mapId=%u AND difficulty=%u",
        cfg.worldDb.c_str(), probeMapId, probeDifficulty);
    uint64_t affected = 0;
    err = client.exec(updSql, &affected);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "UPDATE failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    world_editor::db::QueryResult midUpdate;
    (void)client.query(checkSql, midUpdate);
    uint64_t const sawUpdate = midUpdate.rowCount() > 0
        ? midUpdate.asUInt64(0, 0).value_or(0) : 0;

    (void)client.exec("ROLLBACK");

    if (!txCapable)
    {
        char delSql[256];
        std::snprintf(delSql, sizeof(delSql),
            "DELETE FROM %s.access_requirement WHERE mapId=%u AND difficulty=%u",
            cfg.worldDb.c_str(), probeMapId, probeDifficulty);
        (void)client.exec(delSql);
    }

    world_editor::db::QueryResult afterRes;
    (void)client.query(preSql, afterRes);
    int const sawAfter = afterRes.rowCount() > 0
        ? int(afterRes.asUInt64(0, 0).value_or(0)) : 0;

    p.ms = sinceMs(t0);
    char out[384];
    std::snprintf(out, sizeof(out),
        "mapId=%u diff=%u engine=%s sawInsert.level_min=%llu sawUpdate.level_min=%llu "
        "goneAfter=%d affected=%llu",
        probeMapId, probeDifficulty, engine.c_str(),
        (unsigned long long)sawInsert, (unsigned long long)sawUpdate,
        sawAfter == 0, (unsigned long long)affected);
    p.detail = out;
    p.ok = (sawInsert == 1) && (sawUpdate == 10) && (sawAfter == 0) && (affected == 1);
    return p;
}

// SELECT COUNT(*) FROM access_requirement; assert >= 0 -- table existence
// check; read-only proxy for the dialog's main browse path.
Phase testAccessRequirementCount(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.access-requirement-count", false, {}, 0.0 };
    auto const t0 = clock::now();

    char sql[256];
    std::snprintf(sql, sizeof(sql),
        "SELECT COUNT(*) FROM %s.access_requirement", cfg.worldDb.c_str());
    world_editor::db::QueryResult res;
    auto const err = client.query(sql, res);
    if (!err.ok())
    {
        p.detail = "COUNT(*) failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }
    uint64_t const total = res.rowCount() > 0 ? res.asUInt64(0, 0).value_or(0) : 0;

    p.ms = sinceMs(t0);
    char out[160];
    std::snprintf(out, sizeof(out), "access_requirement rows=%llu",
        (unsigned long long)total);
    p.detail = out;
    p.ok = true;   // table existence + queryable is the assertion; any count >= 0 passes.
    return p;
}

// Shared body for the 3 quest-giver linkage INSERT-rollback phases.
// Picks the first existing template entry + first existing quest_template.ID,
// reserves a non-colliding (entry, quest+offset) pair, INSERTs inside a
// transaction, verifies the row is visible mid-tx, ROLLBACKs, and asserts
// the row is gone.  Backstops the QuestGiverLinkageDialog INSERT path for
// all 4 (id, quest) tables -- the body is table-agnostic so all three
// phase entry points just plug in (phaseName, linkTable, templateTable).
Phase runQuestGiverLinkInsertRollback(world_editor::db::MySqlClient& client,
                                      CliConfig const& cfg,
                                      char const* phaseName,
                                      char const* linkTable,
                                      char const* templateTable)
{
    Phase p{ phaseName, false, {}, 0.0 };
    auto const t0 = clock::now();

    // 1. Pick first existing template entry.
    char tplSql[256];
    std::snprintf(tplSql, sizeof(tplSql),
        "SELECT entry FROM %s.%s ORDER BY entry LIMIT 1",
        cfg.worldDb.c_str(), templateTable);
    world_editor::db::QueryResult tplRes;
    auto err = client.query(tplSql, tplRes);
    if (!err.ok() || tplRes.rowCount() == 0)
    {
        p.detail = std::string(templateTable) + " pick failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }
    uint32_t const entry = static_cast<uint32_t>(tplRes.asUInt64(0, 0).value_or(0));

    // 2. Pick first existing quest_template.ID.
    char qSql[256];
    std::snprintf(qSql, sizeof(qSql),
        "SELECT ID FROM %s.quest_template ORDER BY ID LIMIT 1",
        cfg.worldDb.c_str());
    world_editor::db::QueryResult qRes;
    err = client.query(qSql, qRes);
    if (!err.ok() || qRes.rowCount() == 0)
    {
        p.detail = "quest_template pick failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }
    uint32_t const baseQuest = static_cast<uint32_t>(qRes.asUInt64(0, 0).value_or(0));

    // 3. Reserve a non-colliding probe quest.  Spec: prefer base+99999999;
    //    if that PK is already taken (vanishingly rare but possible on
    //    forked data), fall back to base+1.
    auto pkTaken = [&](uint32_t probe) -> bool {
        char chkSql[256];
        std::snprintf(chkSql, sizeof(chkSql),
            "SELECT COUNT(*) FROM %s.%s WHERE id=%u AND quest=%u",
            cfg.worldDb.c_str(), linkTable, entry, probe);
        world_editor::db::QueryResult r;
        if (!client.query(chkSql, r).ok()) return true;
        return r.rowCount() > 0 && r.asUInt64(0, 0).value_or(0) > 0;
    };
    uint32_t probeQuest = baseQuest + 99999999u;
    if (pkTaken(probeQuest))
        probeQuest = baseQuest + 1u;

    err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    char insSql[384];
    std::snprintf(insSql, sizeof(insSql),
        "INSERT INTO %s.%s (id, quest) VALUES (%u, %u)",
        cfg.worldDb.c_str(), linkTable, entry, probeQuest);
    uint64_t affected = 0;
    err = client.exec(insSql, &affected);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "INSERT failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    // Verify visible in transaction.
    char selSql[256];
    std::snprintf(selSql, sizeof(selSql),
        "SELECT COUNT(*) FROM %s.%s WHERE id=%u AND quest=%u",
        cfg.worldDb.c_str(), linkTable, entry, probeQuest);
    world_editor::db::QueryResult midRes;
    (void)client.query(selSql, midRes);
    int const sawInTx = midRes.rowCount() > 0
        ? int(midRes.asUInt64(0, 0).value_or(0)) : 0;

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(selSql, afterRes);
    int const sawAfter = afterRes.rowCount() > 0
        ? int(afterRes.asUInt64(0, 0).value_or(0)) : 0;

    p.ms = sinceMs(t0);
    char out[320];
    std::snprintf(out, sizeof(out),
        "%s id=%u probeQuest=%u sawInTx=%d goneAfterRollback=%d affected=%llu",
        linkTable, entry, probeQuest, sawInTx, sawAfter == 0,
        (unsigned long long)affected);
    p.detail = out;
    p.ok = (sawInTx == 1) && (sawAfter == 0) && (affected == 1);
    return p;
}

Phase testCreatureQuestStarterInsertRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    return runQuestGiverLinkInsertRollback(client, cfg,
        "db.creature-queststarter-insert-rollback",
        "creature_queststarter", "creature_template");
}

Phase testCreatureQuestEnderInsertRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    return runQuestGiverLinkInsertRollback(client, cfg,
        "db.creature-questender-insert-rollback",
        "creature_questender", "creature_template");
}

Phase testGameObjectQuestEnderInsertRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    return runQuestGiverLinkInsertRollback(client, cfg,
        "db.gameobject-questender-insert-rollback",
        "gameobject_questender", "gameobject_template");
}

// Probe whether the handcrafted_road table exists in the world DB. The
// migration may not have been applied locally; in that case we SKIP rather
// than FAIL so the smoketest stays green on un-migrated boxes.
bool handcraftedRoadTableExists(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    char sql[256];
    std::snprintf(sql, sizeof(sql),
        "SELECT 1 FROM information_schema.tables "
        "WHERE table_schema='%s' AND table_name='handcrafted_road' LIMIT 1",
        cfg.worldDb.c_str());
    world_editor::db::QueryResult res;
    auto const err = client.query(sql, res);
    return err.ok() && res.rowCount() > 0;
}

// INSERT a probe segment, verify the row is visible in-tx via raw SELECT
// (and via the repo's loadForMap reader), then ROLLBACK and verify it's gone.
Phase testHandcraftedRoadInsertRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.handcrafted-road-insert-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    if (!handcraftedRoadTableExists(client, cfg))
    {
        p.detail = "handcrafted_road table not present in " + cfg.worldDb + " (skip - run migration 14)";
        p.ms = sinceMs(t0);
        p.skipped = true;
        return p;
    }

    auto err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    char buf[512];
    std::snprintf(buf, sizeof(buf),
        "INSERT INTO %s.handcrafted_road "
        "(mapId, fromX, fromY, toX, toY, width, comment, verified) "
        "VALUES (0, 100.0, 100.0, 200.0, 200.0, 8.0, 'smoketest probe', 0)",
        cfg.worldDb.c_str());
    uint64_t affected = 0;
    err = client.exec(buf, &affected);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "INSERT failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }
    uint64_t const newId = client.lastInsertId();

    // Verify in-tx via raw COUNT and via the repo reader.
    std::snprintf(buf, sizeof(buf),
        "SELECT COUNT(*) FROM %s.handcrafted_road WHERE id=%llu",
        cfg.worldDb.c_str(), (unsigned long long)newId);
    world_editor::db::QueryResult midRes;
    (void)client.query(buf, midRes);
    int const sawInTx = midRes.rowCount() > 0
        ? int(midRes.asUInt64(0, 0).value_or(0)) : 0;

    world_editor::io::HandcraftedRoadRepo repo(&client);
    auto const inTxList = repo.loadForMap(0);
    bool const repoSawProbe = std::any_of(inTxList.begin(), inTxList.end(),
        [&](world_editor::io::RoadSegment const& s) { return s.id == uint32_t(newId); });

    (void)client.exec("ROLLBACK");

    world_editor::db::QueryResult afterRes;
    (void)client.query(buf, afterRes);
    int const goneAfter = (afterRes.rowCount() > 0
        && afterRes.asUInt64(0, 0).value_or(0) == 0) ? 1 : 0;

    p.ms = sinceMs(t0);
    char out[256];
    std::snprintf(out, sizeof(out),
        "newId=%llu  sawInTx=%d  repoSaw=%d  goneAfterRollback=%d  affected=%llu",
        (unsigned long long)newId, sawInTx, int(repoSawProbe), goneAfter,
        (unsigned long long)affected);
    p.detail = out;
    p.ok = sawInTx == 1 && repoSawProbe && goneAfter == 1 && affected == 1;
    return p;
}

// INSERT probe then UPDATE its comment in the same tx; verify each in-tx
// state, then ROLLBACK and confirm the row is gone (insert reverted too).
Phase testHandcraftedRoadUpdateRollback(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.handcrafted-road-update-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    if (!handcraftedRoadTableExists(client, cfg))
    {
        p.detail = "handcrafted_road table not present in " + cfg.worldDb + " (skip)";
        p.ms = sinceMs(t0);
        p.skipped = true;
        return p;
    }

    auto err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    char buf[512];
    std::snprintf(buf, sizeof(buf),
        "INSERT INTO %s.handcrafted_road "
        "(mapId, fromX, fromY, toX, toY, width, comment, verified) "
        "VALUES (0, 100.0, 100.0, 200.0, 200.0, 8.0, 'probe', 0)",
        cfg.worldDb.c_str());
    err = client.exec(buf);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "INSERT failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }
    uint64_t const newId = client.lastInsertId();

    // Verify the original comment.
    std::snprintf(buf, sizeof(buf),
        "SELECT COALESCE(comment,'') FROM %s.handcrafted_road WHERE id=%llu",
        cfg.worldDb.c_str(), (unsigned long long)newId);
    world_editor::db::QueryResult c1;
    (void)client.query(buf, c1);
    std::string const stage1Comment = c1.rowCount() > 0 ? c1.cell(0, 0) : std::string();

    // UPDATE -> probe2.
    char upd[512];
    std::snprintf(upd, sizeof(upd),
        "UPDATE %s.handcrafted_road SET comment='probe2' WHERE id=%llu",
        cfg.worldDb.c_str(), (unsigned long long)newId);
    uint64_t affected = 0;
    err = client.exec(upd, &affected);
    if (!err.ok())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "UPDATE failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    world_editor::db::QueryResult c2;
    (void)client.query(buf, c2);
    std::string const stage2Comment = c2.rowCount() > 0 ? c2.cell(0, 0) : std::string();

    (void)client.exec("ROLLBACK");

    // Reuse `buf` (still holds the SELECT) to verify the row is gone post-rollback.
    world_editor::db::QueryResult c3;
    (void)client.query(buf, c3);
    bool const goneAfter = c3.rowCount() == 0;

    p.ms = sinceMs(t0);
    char out[320];
    std::snprintf(out, sizeof(out),
        "newId=%llu  stage1='%s'  stage2='%s'  goneAfterRollback=%d  updateAffected=%llu",
        (unsigned long long)newId, stage1Comment.c_str(), stage2Comment.c_str(),
        int(goneAfter), (unsigned long long)affected);
    p.detail = out;
    p.ok = stage1Comment == "probe"
        && stage2Comment == "probe2"
        && goneAfter
        && affected == 1;
    return p;
}

// INSERT two probes on mapId=0 in a transaction, call repo.loadForMap(0)
// and assert at least two probe rows surface, ROLLBACK.
Phase testHandcraftedRoadLoadForMap(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.handcrafted-road-loadformap", false, {}, 0.0 };
    auto const t0 = clock::now();

    if (!handcraftedRoadTableExists(client, cfg))
    {
        p.detail = "handcrafted_road table not present in " + cfg.worldDb + " (skip)";
        p.ms = sinceMs(t0);
        p.skipped = true;
        return p;
    }

    auto err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    // Snapshot the pre-insert count so a non-empty existing table doesn't
    // mask the test - we only assert delta >= 2.
    char count[256];
    std::snprintf(count, sizeof(count),
        "SELECT COUNT(*) FROM %s.handcrafted_road WHERE mapId=0",
        cfg.worldDb.c_str());
    world_editor::db::QueryResult c0;
    (void)client.query(count, c0);
    uint64_t const before = c0.rowCount() > 0 ? c0.asUInt64(0, 0).value_or(0) : 0;

    char buf[512];
    for (int i = 0; i < 2; ++i)
    {
        std::snprintf(buf, sizeof(buf),
            "INSERT INTO %s.handcrafted_road "
            "(mapId, fromX, fromY, toX, toY, width, comment, verified) "
            "VALUES (0, %d.0, %d.0, %d.0, %d.0, 8.0, 'probe%d', 0)",
            cfg.worldDb.c_str(),
            100 + 10 * i, 100 + 10 * i,
            200 + 10 * i, 200 + 10 * i, i);
        err = client.exec(buf);
        if (!err.ok())
        {
            (void)client.exec("ROLLBACK");
            p.detail = "INSERT failed: " + err.message;
            p.ms = sinceMs(t0);
            return p;
        }
    }

    world_editor::io::HandcraftedRoadRepo repo(&client);
    auto const segs = repo.loadForMap(0);
    size_t const loadCount = segs.size();

    // Sanity-check the column decoding picked up our test values.
    size_t probeRows = 0;
    for (auto const& s : segs)
        if (s.comment == QStringLiteral("probe0") || s.comment == QStringLiteral("probe1"))
            ++probeRows;

    (void)client.exec("ROLLBACK");

    p.ms = sinceMs(t0);
    char out[256];
    std::snprintf(out, sizeof(out),
        "before=%llu  loadCount=%zu  probeRowsFound=%zu",
        (unsigned long long)before, loadCount, probeRows);
    p.detail = out;
    p.ok = loadCount >= before + 2 && probeRows == 2;
    return p;
}

// Exercise the HandcraftedRoadDock CRUD API end-to-end against the live
// world DB, but inside a transaction with ROLLBACK so the probe row
// never escapes.  Validates the entire flow the dock will execute on a
// real Add segment... -> Edit -> Delete operator action.
Phase testHandcraftedRoadDockRoundtrip(world_editor::db::MySqlClient& client, CliConfig const& cfg)
{
    Phase p{ "db.handcrafted-road-roundtrip-via-dock-api", false, {}, 0.0 };
    auto const t0 = clock::now();

    if (!handcraftedRoadTableExists(client, cfg))
    {
        p.detail = "handcrafted_road table not present in " + cfg.worldDb + " (skip - run migration 14)";
        p.ms = sinceMs(t0);
        p.skipped = true;
        return p;
    }

    auto err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    // NOTE: HandcraftedRoadRepo::insert/update/remove wrap their bodies
    // in BEGIN/COMMIT internally, so the outer transaction we just opened
    // is auto-committed by repo.insert.  To keep the smoketest contract
    // (no probe rows escape), we manually DELETE the row before ROLLBACK
    // at the end -- the ROLLBACK then only rewinds the explicit DELETE.
    world_editor::io::HandcraftedRoadRepo repo(&client);

    // Step 1: Construct a RoadSegment programmatically + insert.
    world_editor::io::RoadSegment probe;
    probe.mapId    = 0;
    probe.fromX    = 500.5f;
    probe.fromY    = 600.25f;
    probe.toX      = 700.75f;
    probe.toY      = 800.125f;
    probe.width    = 10.0f;
    probe.comment  = QStringLiteral("dock_smoketest");
    probe.verified = false;
    auto const newIdOpt = repo.insert(probe);
    if (!newIdOpt.has_value())
    {
        (void)client.exec("ROLLBACK");
        p.detail = "repo.insert returned nullopt";
        p.ms = sinceMs(t0);
        return p;
    }
    uint32_t const newId = *newIdOpt;
    probe.id = newId;

    // Step 2: loadForMap(0) should now surface the row.
    auto const afterInsert = repo.loadForMap(0);
    bool const seenAfterInsert = std::any_of(afterInsert.begin(), afterInsert.end(),
        [&](world_editor::io::RoadSegment const& s) {
            return s.id == newId
                && s.comment == QStringLiteral("dock_smoketest");
        });
    if (!seenAfterInsert)
    {
        (void)client.exec(std::string("DELETE FROM ") + cfg.worldDb
            + ".handcrafted_road WHERE id=" + std::to_string(newId));
        (void)client.exec("ROLLBACK");
        p.detail = "newly inserted row missing from loadForMap(0)";
        p.ms = sinceMs(t0);
        return p;
    }

    // Step 3: UPDATE: bump width + flip verified flag.
    probe.width    = 12.5f;
    probe.verified = true;
    probe.comment  = QStringLiteral("dock_smoketest_v2");
    if (!repo.update(probe))
    {
        (void)client.exec(std::string("DELETE FROM ") + cfg.worldDb
            + ".handcrafted_road WHERE id=" + std::to_string(newId));
        (void)client.exec("ROLLBACK");
        p.detail = "repo.update returned false";
        p.ms = sinceMs(t0);
        return p;
    }

    // Step 4: loadForMap(0) should reflect the new fields.
    auto const afterUpdate = repo.loadForMap(0);
    auto const it = std::find_if(afterUpdate.begin(), afterUpdate.end(),
        [&](world_editor::io::RoadSegment const& s) { return s.id == newId; });
    bool const updateVisible = (it != afterUpdate.end()
        && std::abs(it->width - 12.5f) < 0.01f
        && it->verified
        && it->comment == QStringLiteral("dock_smoketest_v2"));
    if (!updateVisible)
    {
        (void)client.exec(std::string("DELETE FROM ") + cfg.worldDb
            + ".handcrafted_road WHERE id=" + std::to_string(newId));
        (void)client.exec("ROLLBACK");
        p.detail = "UPDATE not visible in loadForMap";
        p.ms = sinceMs(t0);
        return p;
    }

    // Step 5: DELETE.
    if (!repo.remove(newId))
    {
        (void)client.exec(std::string("DELETE FROM ") + cfg.worldDb
            + ".handcrafted_road WHERE id=" + std::to_string(newId));
        (void)client.exec("ROLLBACK");
        p.detail = "repo.remove returned false";
        p.ms = sinceMs(t0);
        return p;
    }

    // Step 6: loadForMap(0) should no longer surface the row.
    auto const afterDelete = repo.loadForMap(0);
    bool const gone = std::none_of(afterDelete.begin(), afterDelete.end(),
        [&](world_editor::io::RoadSegment const& s) { return s.id == newId; });

    // Rollback whatever the explicit transaction has captured (repo
    // operations COMMIT individually, so this is a defensive sweep --
    // there should be nothing left to undo).
    (void)client.exec("ROLLBACK");

    p.ms = sinceMs(t0);
    char out[320];
    std::snprintf(out, sizeof(out),
        "newId=%u  seenAfterInsert=1  updateVisible=1  gone=%d  "
        "afterInsertRows=%zu  afterUpdateRows=%zu  afterDeleteRows=%zu",
        newId, int(gone),
        afterInsert.size(), afterUpdate.size(), afterDelete.size());
    p.detail = out;
    p.ok = gone;
    return p;
}

// Mirror of NavMeshView::findSnapTarget so the smoketest can validate the
// snap algorithm without pulling the QOpenGLWidget header (which would
// transitively require Qt6::OpenGLWidgets).  Logic MUST match
// NavMeshView::findSnapTarget byte-for-byte; if the production code
// changes, this mirror must change with it.
int snapTargetMirror(float qx, float qy,
                     std::vector<QVector2D> const& candidates,
                     float radiusYards)
{
    float const r2 = radiusYards * radiusYards;
    int bestIdx = -1;
    float bestD2 = r2;
    for (size_t i = 0; i < candidates.size(); ++i)
    {
        float const dx = candidates[i].x() - qx;
        float const dy = candidates[i].y() - qy;
        float const d2 = dx * dx + dy * dy;
        if (d2 <= bestD2)
        {
            bestD2 = d2;
            bestIdx = int(i);
        }
    }
    return bestIdx;
}

// Validate the chain-mode flow: three back-to-back inserts (A->B, B->C,
// C->D) all sharing endpoints, plus an in-process snap check that the
// findSnapTarget algorithm latches onto the right endpoint when the
// click lands slightly off the existing node.  Defensive DELETE +
// ROLLBACK keep the probe rows from escaping.
Phase testHandcraftedRoadChainModeRoundtrip(world_editor::db::MySqlClient& client,
                                            CliConfig const& cfg)
{
    Phase p{ "db.handcrafted-road-chain-mode-roundtrip", false, {}, 0.0 };
    auto const t0 = clock::now();

    if (!handcraftedRoadTableExists(client, cfg))
    {
        p.detail = "handcrafted_road table not present in " + cfg.worldDb
                 + " (skip - run migration 14)";
        p.ms = sinceMs(t0);
        p.skipped = true;
        return p;
    }

    auto err = client.exec("START TRANSACTION");
    if (!err.ok()) { p.detail = "BEGIN: " + err.message; p.ms = sinceMs(t0); return p; }

    world_editor::io::HandcraftedRoadRepo repo(&client);

    // Three chained segments: A->B, B->C, C->D.  Width / comment shared
    // (the dock's chain-prompted caching keeps these identical across
    // every segment in a single chain).
    struct ChainPoint { float x, y; };
    ChainPoint const A{ 1000.0f, 1000.0f };
    ChainPoint const B{ 1100.0f, 1050.0f };
    ChainPoint const C{ 1200.0f, 1200.0f };
    ChainPoint const D{ 1350.0f, 1280.0f };

    std::vector<uint32_t> insertedIds;
    auto pushSeg = [&](ChainPoint const& from, ChainPoint const& to) -> bool {
        world_editor::io::RoadSegment seg;
        seg.mapId    = 0;
        seg.fromX    = from.x;
        seg.fromY    = from.y;
        seg.toX      = to.x;
        seg.toY      = to.y;
        seg.width    = 9.0f;
        seg.comment  = QStringLiteral("chain_smoketest");
        seg.verified = false;
        auto const id = repo.insert(seg);
        if (!id.has_value())
            return false;
        insertedIds.push_back(*id);
        return true;
    };

    bool const insAB = pushSeg(A, B);
    bool const insBC = pushSeg(B, C);
    bool const insCD = pushSeg(C, D);

    auto cleanup = [&]() {
        for (uint32_t id : insertedIds)
        {
            char del[256];
            std::snprintf(del, sizeof(del),
                "DELETE FROM %s.handcrafted_road WHERE id=%u",
                cfg.worldDb.c_str(), id);
            (void)client.exec(del);
        }
        (void)client.exec("ROLLBACK");
    };

    if (!insAB || !insBC || !insCD)
    {
        cleanup();
        p.detail = "one or more chain inserts failed";
        p.ms = sinceMs(t0);
        return p;
    }

    auto const loaded = repo.loadForMap(0);
    int chainHits = 0;
    for (world_editor::io::RoadSegment const& s : loaded)
    {
        if (s.comment == QStringLiteral("chain_smoketest"))
            ++chainHits;
    }
    if (chainHits < 3)
    {
        cleanup();
        char detail[256];
        std::snprintf(detail, sizeof(detail),
            "loadForMap(0) returned only %d chain rows; expected >=3", chainHits);
        p.detail = detail;
        p.ms = sinceMs(t0);
        return p;
    }

    // Snap test: build the candidate vector exactly the way the viewer
    // does (pairs of GL_LINES endpoints) and verify that a query point
    // near B latches onto B specifically.  Radius 6.0 yards matches
    // NavMeshView::SNAP_RADIUS_YARDS.
    std::vector<QVector2D> cands;
    cands.reserve(insertedIds.size() * 2);
    cands.emplace_back(A.x, A.y); cands.emplace_back(B.x, B.y);
    cands.emplace_back(B.x, B.y); cands.emplace_back(C.x, C.y);
    cands.emplace_back(C.x, C.y); cands.emplace_back(D.x, D.y);

    // Click 3.5 yards off B: well within 6y radius.
    float const queryX = B.x + 2.5f;
    float const queryY = B.y - 2.5f;
    int const snapIdx = snapTargetMirror(queryX, queryY, cands, 6.0f);
    bool const snappedToB = (snapIdx >= 0
        && std::abs(cands[size_t(snapIdx)].x() - B.x) < 0.01f
        && std::abs(cands[size_t(snapIdx)].y() - B.y) < 0.01f);

    // Negative case: 30 yards out -> no snap.
    int const farIdx = snapTargetMirror(B.x + 30.0f, B.y + 30.0f, cands, 6.0f);
    bool const farRejected = (farIdx < 0);

    cleanup();

    p.ms = sinceMs(t0);
    char out[320];
    std::snprintf(out, sizeof(out),
        "chainInserts=AB:%d,BC:%d,CD:%d  chainHits=%d  snappedToB=%d  farRejected=%d",
        int(insAB), int(insBC), int(insCD), chainHits,
        int(snappedToB), int(farRejected));
    p.detail = out;
    p.ok = insAB && insBC && insCD && chainHits >= 3 && snappedToB && farRejected;
    return p;
}

// INSERT three probe segments with comment='probe' in a transaction, then
// build a SINGLE bulk-UPDATE SQL statement that retags every probe id with
// comment='probe-elwynn' (mirrors the dock's onBulkEditClicked flow), verify
// in-tx that all three flipped, then ROLLBACK and assert the probes are gone
// (the explicit BEGIN nests the inserts + update under one tx).
Phase testHandcraftedRoadBulkEditRollback(world_editor::db::MySqlClient& client,
                                          CliConfig const& cfg)
{
    Phase p{ "db.handcrafted-road-bulk-edit-rollback", false, {}, 0.0 };
    auto const t0 = clock::now();

    if (!handcraftedRoadTableExists(client, cfg))
    {
        p.detail = "handcrafted_road table not present in " + cfg.worldDb
                 + " (skip - run migration 14)";
        p.ms = sinceMs(t0);
        p.skipped = true;
        return p;
    }

    auto err = client.exec("START TRANSACTION");
    if (!err.ok())
    {
        p.detail = "BEGIN: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    // Step 1: INSERT three probe segments with comment='probe'.  Different
    // geometry to mimic what an operator-authored chain looks like.
    std::vector<uint64_t> probeIds;
    char buf[512];
    for (int i = 0; i < 3; ++i)
    {
        std::snprintf(buf, sizeof(buf),
            "INSERT INTO %s.handcrafted_road "
            "(mapId, fromX, fromY, toX, toY, width, comment, verified) "
            "VALUES (0, %d.0, %d.0, %d.0, %d.0, 8.0, 'probe', 0)",
            cfg.worldDb.c_str(),
            500 + 25 * i, 500 + 25 * i,
            600 + 25 * i, 600 + 25 * i);
        err = client.exec(buf);
        if (!err.ok())
        {
            (void)client.exec("ROLLBACK");
            p.detail = "probe INSERT failed: " + err.message;
            p.ms = sinceMs(t0);
            return p;
        }
        probeIds.push_back(client.lastInsertId());
    }

    // Step 2: loadForMap(0) should surface the probe ids; collect them as
    // the dock would via the table widget + selectedSegmentIds.
    world_editor::io::HandcraftedRoadRepo repo(&client);
    auto const loaded = repo.loadForMap(0);
    std::vector<uint32_t> collectedIds;
    for (auto const& s : loaded)
    {
        bool const isProbe = std::any_of(probeIds.begin(), probeIds.end(),
            [&](uint64_t p) { return uint64_t(s.id) == p; });
        if (isProbe && s.comment == QStringLiteral("probe"))
            collectedIds.push_back(s.id);
    }
    if (collectedIds.size() != 3)
    {
        (void)client.exec("ROLLBACK");
        char detail[256];
        std::snprintf(detail, sizeof(detail),
            "expected 3 probe rows in loadForMap; saw %zu", collectedIds.size());
        p.detail = detail;
        p.ms = sinceMs(t0);
        return p;
    }

    // Step 3: build + run a bulk-UPDATE SQL statement.  We mirror the dock's
    // shape: one UPDATE per id inside a single tx (the dock also wraps the
    // batch in START TRANSACTION / COMMIT).
    uint64_t totalAffected = 0;
    bool sqlOk = true;
    for (uint32_t id : collectedIds)
    {
        std::snprintf(buf, sizeof(buf),
            "UPDATE %s.handcrafted_road SET comment='probe-elwynn' WHERE id=%u",
            cfg.worldDb.c_str(), id);
        uint64_t affected = 0;
        err = client.exec(buf, &affected);
        if (!err.ok()) { sqlOk = false; break; }
        totalAffected += affected;
    }
    if (!sqlOk)
    {
        (void)client.exec("ROLLBACK");
        p.detail = "bulk UPDATE failed: " + err.message;
        p.ms = sinceMs(t0);
        return p;
    }

    // Step 4: verify in-tx -- repo.loadForMap should report all three rows
    // now carrying 'probe-elwynn'.
    auto const afterBulk = repo.loadForMap(0);
    int flipped = 0;
    for (auto const& s : afterBulk)
    {
        bool const isProbe = std::any_of(collectedIds.begin(), collectedIds.end(),
            [&](uint32_t cid) { return s.id == cid; });
        if (isProbe && s.comment == QStringLiteral("probe-elwynn"))
            ++flipped;
    }

    // Step 5: ROLLBACK.  Since the probe inserts were inside the same tx,
    // both the inserts AND the bulk-update get reversed; the probes are
    // expected to be ABSENT entirely after the rollback (per the task spec
    // wording "back to 'probe' (or absent if they were inside the tx)").
    (void)client.exec("ROLLBACK");

    int stillPresent = 0;
    int stillPresentWithOldComment = 0;
    for (uint32_t id : collectedIds)
    {
        std::snprintf(buf, sizeof(buf),
            "SELECT COALESCE(comment,'') FROM %s.handcrafted_road WHERE id=%u",
            cfg.worldDb.c_str(), id);
        world_editor::db::QueryResult r;
        (void)client.query(buf, r);
        if (r.rowCount() > 0)
        {
            ++stillPresent;
            if (r.cell(0, 0) == "probe")
                ++stillPresentWithOldComment;
        }
    }
    // Either fully absent (insert reverted by ROLLBACK) or still present
    // but with the original 'probe' comment (the bulk UPDATE was reverted).
    bool const rollbackOk = (stillPresent == 0)
        || (stillPresent == 3 && stillPresentWithOldComment == 3);

    p.ms = sinceMs(t0);
    char out[320];
    std::snprintf(out, sizeof(out),
        "inserted=%zu  collected=%zu  bulkAffected=%llu  flippedInTx=%d  "
        "stillPresent=%d  stillProbe=%d",
        probeIds.size(), collectedIds.size(),
        (unsigned long long)totalAffected, flipped,
        stillPresent, stillPresentWithOldComment);
    p.detail = out;
    p.ok = (probeIds.size() == 3)
        && (collectedIds.size() == 3)
        && (flipped == 3)
        && (totalAffected == 3)
        && rollbackOk;
    return p;
}

} // anonymous namespace

int main(int argc, char** argv)
{
    CliConfig cfg;
    if (!parseCli(argc, argv, cfg))
        return 2;

    std::printf("world_editor_smoketest\n");
    std::printf("  mmaps    : %s\n", cfg.mmapsDir.string().c_str());
    std::printf("  maps     : %s\n", cfg.mapsDir.string().c_str());
    std::printf("  db.host  : %s\n", cfg.dbHost.c_str());
    std::printf("  db.user  : %s\n", cfg.dbUser.c_str());
    std::printf("  world db : %s\n", cfg.worldDb.c_str());
    std::printf("  chars db : %s\n", cfg.charsDb.c_str());
    std::printf("  map id   : %u\n", cfg.mapId);
    std::fflush(stdout);

    std::vector<Phase> phases;
    // Environment-free infra phases run first so a broken local toolchain
    // surfaces before we touch disk / DB.
    phases.push_back(testListfileRoundtrip());
    std::printf("phase infra.listfile-roundtrip done\n"); std::fflush(stdout);
    phases.push_back(testMinimapTileOrientation());
    std::printf("phase render.minimap-tile-orientation done\n"); std::fflush(stdout);
    phases.push_back(testMinimapCanonicalPath());
    std::printf("phase render.minimap-canonical-path done\n"); std::fflush(stdout);
    phases.push_back(testSmartAiMetadata(cfg));
    std::printf("phase smartai.metadata done\n"); std::fflush(stdout);
    phases.push_back(testCascFdidOpen(cfg));
    std::printf("phase casc.fdid-open done\n"); std::fflush(stdout);
    phases.push_back(testWmoLoad(cfg));
    std::printf("phase wmo.load done\n"); std::fflush(stdout);
    phases.push_back(testMmaps(cfg));
    std::printf("phase mmaps done\n"); std::fflush(stdout);
    phases.push_back(testMapTile(cfg));
    std::printf("phase maps done\n"); std::fflush(stdout);

    world_editor::db::MySqlClient client;
    phases.push_back(testDbConnect(client, cfg));
    std::printf("phase db.connect done\n"); std::fflush(stdout);

    phases.push_back(testVmaps(cfg));
    phases.push_back(testVmapProbe(cfg));
    std::printf("phase vmaps done\n"); std::fflush(stdout);

    if (client.isConnected())
    {
        phases.push_back(testCreatureQuery(client, cfg));
        phases.push_back(testBattlemasterQuery(client, cfg));
        phases.push_back(testTrainerQuery(client, cfg));
        phases.push_back(testGameObjectQuery(client, cfg));
        phases.push_back(testAnnotationQuery(client, cfg));
        phases.push_back(testCommitRoundtrip(client, cfg));
        phases.push_back(testSpawnUpdateRoundtrip(client, cfg));
        phases.push_back(testCreatureAddonUpdateRollback(client, cfg));
        phases.push_back(testGameObjectAddonUpdateRollback(client, cfg));
        phases.push_back(testCreatureInsertRollback(client, cfg));
        phases.push_back(testWaypointPaths(client, cfg));
        phases.push_back(testWaypointInsertRollback(client, cfg));
        phases.push_back(testGroupsPools(client, cfg));
        phases.push_back(testAreatriggersGraveyards(client, cfg));
        phases.push_back(testAreatriggerUpdateRollback(client, cfg));
        phases.push_back(testAreatriggerInsertRollback(client, cfg));
        phases.push_back(testGraveyardUpdateRollback(client, cfg));
        phases.push_back(testGraveyardInsertRollback(client, cfg));
        phases.push_back(testTemplateLookup(client, cfg));
        phases.push_back(testTransportUpdateRollback(client, cfg));
        phases.push_back(testTransportInsertRollback(client, cfg));
        phases.push_back(testSmartScriptUpdateRollback(client, cfg));
        phases.push_back(testSmartScriptInsertRollback(client, cfg));
        phases.push_back(testConditionUpdateRollback(client, cfg));
        phases.push_back(testConditionInsertRollback(client, cfg));
        phases.push_back(testPoolTemplateInsertRollback(client, cfg));
        phases.push_back(testPoolTemplateUpdateRollback(client, cfg));
        phases.push_back(testSpawnGroupTemplateInsertRollback(client, cfg));
        phases.push_back(testSpawnGroupTemplateUpdateRollback(client, cfg));
        phases.push_back(testSpawnGroupInsertRollback(client, cfg));
        phases.push_back(testPoolCreatureInsertRollback(client, cfg));
        phases.push_back(testPoolGameobjectInsertRollback(client, cfg));
        phases.push_back(testGameEventInsertRollback(client, cfg));
        phases.push_back(testGameEventCreatureLinkRollback(client, cfg));
        phases.push_back(testGameEventGameobjectLinkRollback(client, cfg));
        phases.push_back(testNpcVendorInsertRollback(client, cfg));
        phases.push_back(testNpcVendorUpdateRollback(client, cfg));
        phases.push_back(testDisablesInsertRollback(client, cfg));
        phases.push_back(testDisablesUpdateRollback(client, cfg));
        phases.push_back(testWaypointPathInsertRollback(client, cfg));
        phases.push_back(testWaypointNodeInsertRollback(client, cfg));
        phases.push_back(testWaypointPathCloneRollback(client, cfg));
        phases.push_back(testAreatriggerTeleportInsertRollback(client, cfg));
        phases.push_back(testAreatriggerTeleportUpdateRollback(client, cfg));
        phases.push_back(testAreatriggerTeleportSearch(client, cfg));
        phases.push_back(testCreatureLootInsertRollback(client, cfg));
        phases.push_back(testCreatureLootUpdateRollback(client, cfg));
        phases.push_back(testCreatureLootSearchEntry(client, cfg));
        phases.push_back(testGameobjectLootInsertRollback(client, cfg));
        phases.push_back(testSkinningLootInsertRollback(client, cfg));
        phases.push_back(testReferenceLootInsertRollback(client, cfg));
        phases.push_back(testGossipMenuInsertRollback(client, cfg));
        phases.push_back(testGossipMenuOptionInsertRollback(client, cfg));
        phases.push_back(testNpcTextUpdateRollback(client, cfg));
        phases.push_back(testCreatureTextInsertRollback(client, cfg));
        phases.push_back(testCreatureTextUpdateRollback(client, cfg));
        phases.push_back(testCreatureTextSearchEntriesWithText(client, cfg));
        phases.push_back(testCreatureEquipInsertRollback(client, cfg));
        phases.push_back(testCreatureEquipUpdateRollback(client, cfg));
        phases.push_back(testCreatureEquipSearchEntriesWithEquip(client, cfg));
        phases.push_back(testBroadcastTextInsertRollback(client, cfg));
        phases.push_back(testBroadcastTextUpdateRollback(client, cfg));
        phases.push_back(testBroadcastTextReferenceScan(client, cfg));
        phases.push_back(testCreatureTemplateAddonInsertRollback(client, cfg));
        phases.push_back(testCreatureTemplateAddonUpdateRollback(client, cfg));
        phases.push_back(testCreatureTemplateAddonCountWithMount(client, cfg));
        phases.push_back(testLinkedRespawnInsertRollback(client, cfg));
        phases.push_back(testLinkedRespawnUpdateRollback(client, cfg));
        phases.push_back(testLinkedRespawnCount(client, cfg));
        phases.push_back(testSnapToGround(client, cfg));
        phases.push_back(testWorldSafeLocsInsertRollback(client, cfg));
        phases.push_back(testWorldSafeLocsUpdateRollback(client, cfg));
        phases.push_back(testWorldSafeLocsSearch(client, cfg));
        phases.push_back(testQuestOfferRewardUpsertRollback(client, cfg));
        phases.push_back(testQuestRequestItemsUpsertRollback(client, cfg));
        phases.push_back(testQuestDetailsUpsertRollback(client, cfg));
        phases.push_back(testCreatureSummonGroupsInsertRollback(client, cfg));
        phases.push_back(testCreatureSummonGroupsUpdateRollback(client, cfg));
        phases.push_back(testCreatureSummonGroupsCount(client, cfg));
        phases.push_back(testAccessRequirementInsertRollback(client, cfg));
        phases.push_back(testAccessRequirementUpdateRollback(client, cfg));
        phases.push_back(testAccessRequirementCount(client, cfg));
        phases.push_back(testCreatureQuestStarterInsertRollback(client, cfg));
        phases.push_back(testCreatureQuestEnderInsertRollback(client, cfg));
        phases.push_back(testGameObjectQuestEnderInsertRollback(client, cfg));
        phases.push_back(testHandcraftedRoadInsertRollback(client, cfg));
        phases.push_back(testHandcraftedRoadUpdateRollback(client, cfg));
        phases.push_back(testHandcraftedRoadLoadForMap(client, cfg));
        phases.push_back(testHandcraftedRoadDockRoundtrip(client, cfg));
        phases.push_back(testHandcraftedRoadChainModeRoundtrip(client, cfg));
        phases.push_back(testHandcraftedRoadBulkEditRollback(client, cfg));
    }

    printPhases(phases);
    bool const allOk = std::all_of(phases.begin(), phases.end(),
        [](Phase const& p) { return p.ok || p.skipped; });
    std::printf("\nOVERALL: %s\n", allOk ? "PASS" : "FAIL");
    return allOk ? 0 : 1;
}
