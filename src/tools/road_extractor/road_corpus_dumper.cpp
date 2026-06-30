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

// road_corpus_dumper
//
// Reads a sampling manifest JSON, opens the WoW CASC storage, and for each
// listed (map_id, adt_x, adt_y) dumps one CSV row per MCNK with non-zero
// nLayers. The output CSV uses the same schema road_validator consumes
// (CORPUS_BUILDING_GUIDE.md), with the `label` column left empty for a
// human labeler to fill in.
//
// Usage:
//   road_corpus_dumper --client <path> --manifest <json> --output <csv>
//                      [--locale <code>] [--product <name>]
//
// Required:
//   --client    path to WoW installation root (the directory containing
//               ".build.info"). map_extractor uses the same root.
//   --manifest  path to road_sampling_manifest.json
//   --output    path to write the candidate corpus CSV
//
// Optional:
//   --locale    enUS (default), deDE, frFR, ...
//   --product   "wow" (default), "wow_classic"

#include "AdtTextureReader.h"
#include "CascHandles.h"
#include "DB2CascFileSource.h"
#include "DB2FileLoader.h"
#include "ExtractorDB2LoadInfo.h"
#include "ListfileMap.h"

#include <CascLib.h>

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace
{
    struct Options
    {
        std::string clientPath;
        std::string manifestPath;
        std::string outputPath;
        std::string listfilePath;
        std::string locale  = "enUS";
        std::string product = "wow";
    };

    [[noreturn]] void PrintUsageAndExit(int exitCode)
    {
        std::fprintf(stderr,
            "road_corpus_dumper — read sampling manifest, dump candidate CSV\n"
            "\n"
            "Usage:\n"
            "  road_corpus_dumper --client <path> --manifest <json>\n"
            "                     --output <csv> [--locale enUS]\n"
            "                     [--product wow]\n"
            "\n"
            "Outputs one CSV row per MCNK with non-empty layer count.\n"
            "The `label` column is left empty for human labeling.\n");
        std::exit(exitCode);
    }

    Options ParseArgs(int argc, char** argv)
    {
        Options o;
        for (int i = 1; i < argc; ++i)
        {
            std::string_view a = argv[i];
            auto wantVal = [&](char const* flag) -> std::string {
                if (i + 1 >= argc)
                {
                    std::fprintf(stderr, "%s requires a value\n", flag);
                    PrintUsageAndExit(1);
                }
                return argv[++i];
            };
            if (a == "--help" || a == "-h") PrintUsageAndExit(0);
            else if (a == "--client")    o.clientPath  = wantVal("--client");
            else if (a == "--manifest")  o.manifestPath = wantVal("--manifest");
            else if (a == "--output")    o.outputPath  = wantVal("--output");
            else if (a == "--listfile")  o.listfilePath = wantVal("--listfile");
            else if (a == "--locale")    o.locale      = wantVal("--locale");
            else if (a == "--product")   o.product     = wantVal("--product");
            else
            {
                std::fprintf(stderr, "Unknown arg: %s\n",
                             std::string(a).c_str());
                PrintUsageAndExit(1);
            }
        }
        if (o.clientPath.empty() || o.manifestPath.empty() || o.outputPath.empty())
            PrintUsageAndExit(1);
        return o;
    }

    uint32 LocaleToCascMask(std::string const& locale)
    {
        // Mirror System.cpp's WowLocaleToCascLocaleFlags. Default-broad mask
        // for "enUS" or unknown.
        if (locale == "enUS") return CASC_LOCALE_ENUS;
        if (locale == "koKR") return CASC_LOCALE_KOKR;
        if (locale == "frFR") return CASC_LOCALE_FRFR;
        if (locale == "deDE") return CASC_LOCALE_DEDE;
        if (locale == "zhCN") return CASC_LOCALE_ZHCN;
        if (locale == "esES") return CASC_LOCALE_ESES;
        if (locale == "zhTW") return CASC_LOCALE_ZHTW;
        if (locale == "esMX") return CASC_LOCALE_ESMX;
        if (locale == "ruRU") return CASC_LOCALE_RURU;
        if (locale == "ptBR") return CASC_LOCALE_PTBR;
        if (locale == "itIT") return CASC_LOCALE_ITIT;
        return CASC_LOCALE_ALL_WOW;
    }

    struct ManifestEntry
    {
        uint32 mapId    = 0;
        std::string mapName;
        uint8  adtX     = 0;
        uint8  adtY     = 0;
        std::string zoneLabel;
        std::string caseKind;
    };

    std::vector<ManifestEntry> ParseManifest(std::string const& path)
    {
        std::vector<ManifestEntry> out;
        boost::property_tree::ptree pt;
        try
        {
            boost::property_tree::read_json(path, pt);
        }
        catch (std::exception const& e)
        {
            std::fprintf(stderr, "Failed to read manifest: %s\n", e.what());
            return out;
        }
        for (auto const& [_, entry] : pt.get_child("entries"))
        {
            ManifestEntry m;
            m.mapId     = entry.get<uint32>("map_id", 0);
            m.mapName   = entry.get<std::string>("map_name", "");
            m.adtX      = static_cast<uint8>(entry.get<uint32>("adt_x", 0));
            m.adtY      = static_cast<uint8>(entry.get<uint32>("adt_y", 0));
            m.zoneLabel = entry.get<std::string>("zone_label", "");
            m.caseKind  = entry.get<std::string>("case_kind", "");
            out.push_back(std::move(m));
        }
        return out;
    }

    struct MapEntry
    {
        uint32 Id;
        int32  WdtFileDataId;
        std::string Name;
        std::string Directory;
    };

    // Minimal Map.db2 loader. Mirrors System.cpp::ReadMapDBC but populates
    // a local table without touching globals.
    std::unordered_map<uint32, MapEntry> LoadMapTable(
        std::shared_ptr<CASC::Storage const> storage)
    {
        std::unordered_map<uint32, MapEntry> out;
        DB2CascFileSource source(storage,
                                  MapLoadInfo::Instance.Meta->FileDataId,
                                  false);
        DB2FileLoader db2;
        try
        {
            db2.Load(&source, &MapLoadInfo::Instance);
        }
        catch (std::exception const& e)
        {
            std::fprintf(stderr, "Failed to load Map.db2: %s\n", e.what());
            return out;
        }
        for (uint32 i = 0; i < db2.GetRecordCount(); ++i)
        {
            DB2Record rec = db2.GetRecord(i);
            if (!rec) continue;
            MapEntry m;
            m.Id            = rec.GetId();
            m.WdtFileDataId = rec.GetInt32("WdtFileDataID");
            m.Name          = rec.GetString("MapName");
            m.Directory     = rec.GetString("Directory");
            out[m.Id] = std::move(m);
        }
        // Handle record copies (alias IDs).
        for (uint32 i = 0; i < db2.GetRecordCopyCount(); ++i)
        {
            DB2RecordCopy c = db2.GetRecordCopy(i);
            auto it = out.find(c.SourceRowId);
            if (it == out.end()) continue;
            MapEntry m       = it->second;
            m.Id             = c.NewRowId;
            out[m.Id] = std::move(m);
        }
        return out;
    }

    // CSV escape per RFC 4180 subset (must match RoadValidation.cpp).
    std::string CsvEscape(std::string_view s)
    {
        bool needsQuote = false;
        for (char c : s)
            if (c == ',' || c == '"' || c == '\n' || c == '\r')
                { needsQuote = true; break; }
        if (!needsQuote) return std::string(s);
        std::string out;
        out.reserve(s.size() + 2);
        out.push_back('"');
        for (char c : s)
        {
            if (c == '"') out.push_back('"');
            out.push_back(c);
        }
        out.push_back('"');
        return out;
    }

    // Open CASC. Tries the client path directly; falls back to OpenRemote
    // with a temp cache if the client install isn't directly readable.
    std::shared_ptr<CASC::Storage> OpenCasc(Options const& opts)
    {
        boost::filesystem::path clientRoot(opts.clientPath);
        uint32 localeMask = LocaleToCascMask(opts.locale);
        CASC::Storage* raw =
            CASC::Storage::Open(clientRoot, localeMask, opts.product.c_str());
        if (!raw)
        {
            std::fprintf(stderr,
                "Failed to open CASC storage at %s (locale=%s product=%s):\n"
                "  %s\n",
                opts.clientPath.c_str(), opts.locale.c_str(),
                opts.product.c_str(),
                CASC::HumanReadableCASCError(GetCascError()));
            return nullptr;
        }
        return std::shared_ptr<CASC::Storage>(raw);
    }
}

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------

int main(int argc, char** argv)
{
    try
    {
        Options opts = ParseArgs(argc, argv);

        // 1. Open CASC.
        std::shared_ptr<CASC::Storage> storage = OpenCasc(opts);
        if (!storage)
            return 2;
        std::printf("Opened CASC storage (build %u).\n",
                    storage->GetBuildNumber());

        // 2. Load Map.db2 to resolve map_id → WdtFileDataId.
        auto mapTable = LoadMapTable(storage);
        if (mapTable.empty())
        {
            std::fprintf(stderr, "Map.db2 yielded zero entries.\n");
            return 2;
        }
        std::printf("Loaded %zu maps from Map.db2.\n", mapTable.size());

        // 3. Parse manifest.
        std::vector<ManifestEntry> manifest = ParseManifest(opts.manifestPath);
        if (manifest.empty())
        {
            std::fprintf(stderr, "Manifest is empty or malformed.\n");
            return 2;
        }
        std::printf("Manifest has %zu entries.\n", manifest.size());

        // 4. Open output CSV with the road_validator-compatible header.
        std::ofstream out(opts.outputPath, std::ios::binary);
        if (!out)
        {
            std::fprintf(stderr, "Cannot open output: %s\n", opts.outputPath.c_str());
            return 3;
        }
        out << "schema_version,map_id,adt_x,adt_y,mcnk_idx,texture_blp,"
               "effect_id,layer_count,labeler,label_date,label,confidence,notes\n";

        // Optional FileDataID → path listfile for resolving MDID texture
        // references in modern (Legion+) ADTs. Without it, modern textures
        // show as [FDID:N] placeholders.
        Road::ListfileMap listfile;
        if (!opts.listfilePath.empty())
        {
            std::vector<std::string> warns;
            if (listfile.LoadFromFile(opts.listfilePath, &warns))
                std::printf("Loaded %zu listfile entries from %s\n",
                            listfile.Size(), opts.listfilePath.c_str());
            else
                std::fprintf(stderr, "Failed to load listfile: %s\n",
                             opts.listfilePath.c_str());
        }

        Road::AdtTexture::AdtTextureReader reader(storage);
        if (!listfile.Empty())
            reader.SetListfileMap(&listfile);
        std::unordered_map<uint32, Road::AdtTexture::WdtInfo> wdtCache;

        std::size_t totalRowsWritten = 0;
        std::size_t adtsProcessed = 0;
        std::size_t adtsSkipped = 0;

        // 5. For each manifest entry:
        for (ManifestEntry const& m : manifest)
        {
            auto mapIt = mapTable.find(m.mapId);
            if (mapIt == mapTable.end())
            {
                std::fprintf(stderr,
                    "map_id %u not in Map.db2; skipping (zone: %s)\n",
                    m.mapId, m.zoneLabel.c_str());
                ++adtsSkipped;
                continue;
            }

            // Cache WDT per map.
            auto wdtIt = wdtCache.find(m.mapId);
            if (wdtIt == wdtCache.end())
            {
                auto wdt = Road::AdtTexture::ReadWdt(
                    *storage, m.mapId,
                    static_cast<uint32>(mapIt->second.WdtFileDataId));
                if (!wdt)
                {
                    std::fprintf(stderr,
                        "Failed to read WDT for map %u (%s); skipping\n",
                        m.mapId, mapIt->second.Name.c_str());
                    ++adtsSkipped;
                    continue;
                }
                wdtIt = wdtCache.emplace(m.mapId, std::move(*wdt)).first;
            }
            Road::AdtTexture::WdtInfo const& wdt = wdtIt->second;

            Road::AdtTexture::WdtAdtIds const& ids =
                wdt.adts[m.adtY][m.adtX];
            if (!ids.present || ids.rootADT == 0)
            {
                std::fprintf(stderr,
                    "ADT (%u, %u) not present in map %u (zone: %s)\n",
                    m.adtX, m.adtY, m.mapId, m.zoneLabel.c_str());
                ++adtsSkipped;
                continue;
            }

            // Read ADT texture data.
            auto summary = reader.ReadAdt(m.mapId, m.adtX, m.adtY,
                                           wdt.flags,
                                           ids.rootADT, ids.tex0ADT);
            if (!summary)
            {
                std::fprintf(stderr,
                    "Failed to read ADT (%u, %u) map %u\n",
                    m.adtX, m.adtY, m.mapId);
                ++adtsSkipped;
                continue;
            }
            ++adtsProcessed;
            for (std::string const& w : summary->warnings)
                std::fprintf(stderr, "  [warn] %s\n", w.c_str());

            // 6. Emit one CSV row per MCNK with non-zero layer count.
            //    Empty `label` for the human labeler to fill in.
            //    `notes` initialized with the manifest's zone label so the
            //    labeler has context.
            std::size_t rowsThisAdt = 0;
            for (Road::AdtTexture::McnkTextureSummary const& mcnk : summary->mcnks)
            {
                if (mcnk.nLayers == 0)
                    continue;
                out << "1," << m.mapId << ','
                    << static_cast<uint32>(m.adtX) << ','
                    << static_cast<uint32>(m.adtY) << ','
                    << mcnk.mcnkIdx << ','
                    << CsvEscape(mcnk.dominantTextureBlp) << ','
                    << mcnk.dominantEffectId << ','
                    << mcnk.nLayers << ','
                    << ",,,,"   // labeler, label_date, label, confidence empty
                    << CsvEscape(m.zoneLabel + " [" + m.caseKind + "]")
                    << '\n';
                ++rowsThisAdt;
            }
            totalRowsWritten += rowsThisAdt;
            std::printf("  map %u (%u,%u) [%s] → %zu MCNK rows\n",
                        m.mapId, m.adtX, m.adtY, m.zoneLabel.c_str(),
                        rowsThisAdt);
        }

        out.close();
        std::printf("\nDone. ADTs processed: %zu, skipped: %zu, "
                    "candidate rows written: %zu\n",
                    adtsProcessed, adtsSkipped, totalRowsWritten);
        std::printf("Output: %s\n", opts.outputPath.c_str());
        std::printf("\nNext step: run the labeling tool against this CSV,\n"
                    "then road_validator to score against the classifier.\n");
        return 0;
    }
    catch (std::exception const& e)
    {
        std::fprintf(stderr, "Unhandled exception: %s\n", e.what());
        return 4;
    }
}
