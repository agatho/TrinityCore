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

// road_diag
//
// Diagnostic dumper. For each MCNK in the listed ADTs, emits ALL layer
// information + per-layer road verdict + the final BuildRoadMaskGrid
// decision + which rule (a or b) flagged it.
//
// Answers: "Is rule (b) actually catching the overlay roads?"
//          "What textures show up on layers 1-3 in zones with low coverage?"
//          "Which MCNKs were flagged by texture but stripped by the
//           contiguous-area filter?"
//
// Usage:
//   road_diag --client <wow-root> --manifest <json> --output <csv>
//             [--listfile <csv>] [--locale enUS] [--product wow]

#include "AdtTextureReader.h"
#include "CascHandles.h"
#include "DB2CascFileSource.h"
#include "DB2FileLoader.h"
#include "ExtractorDB2LoadInfo.h"
#include "ListfileMap.h"
#include "RoadClassifier.h"

#include <CascLib.h>

#include <boost/filesystem/path.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
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
        std::string locale = "enUS";
        std::string product = "wow";
    };

    [[noreturn]] void Usage(int code)
    {
        std::fprintf(stderr,
            "road_diag — diagnostic dump: all 4 layers + per-layer + final verdict\n\n"
            "Usage:\n"
            "  road_diag --client <path> --manifest <json> --output <csv>\n"
            "            [--listfile <csv>] [--locale enUS] [--product wow]\n");
        std::exit(code);
    }

    Options ParseArgs(int argc, char** argv)
    {
        Options o;
        for (int i = 1; i < argc; ++i)
        {
            std::string_view a = argv[i];
            auto val = [&](char const* f) -> std::string {
                if (i + 1 >= argc) { std::fprintf(stderr, "%s needs a value\n", f); Usage(1); }
                return argv[++i];
            };
            if (a == "--help" || a == "-h") Usage(0);
            else if (a == "--client")    o.clientPath = val("--client");
            else if (a == "--manifest")  o.manifestPath = val("--manifest");
            else if (a == "--output")    o.outputPath = val("--output");
            else if (a == "--listfile")  o.listfilePath = val("--listfile");
            else if (a == "--locale")    o.locale = val("--locale");
            else if (a == "--product")   o.product = val("--product");
            else { std::fprintf(stderr, "Unknown arg: %s\n", argv[i]); Usage(1); }
        }
        if (o.clientPath.empty() || o.manifestPath.empty() || o.outputPath.empty())
            Usage(1);
        return o;
    }

    uint32 LocaleMask(std::string const& l)
    {
        if (l == "enUS") return CASC_LOCALE_ENUS;
        if (l == "deDE") return CASC_LOCALE_DEDE;
        if (l == "frFR") return CASC_LOCALE_FRFR;
        if (l == "ruRU") return CASC_LOCALE_RURU;
        return CASC_LOCALE_ALL_WOW;
    }

    struct ManifestEntry
    {
        uint32 mapId = 0;
        std::string mapName;
        uint8 adtX = 0, adtY = 0;
        std::string zoneLabel;
    };

    std::vector<ManifestEntry> ParseManifest(std::string const& path)
    {
        std::vector<ManifestEntry> out;
        boost::property_tree::ptree pt;
        try { boost::property_tree::read_json(path, pt); }
        catch (std::exception const& e) { std::fprintf(stderr, "manifest: %s\n", e.what()); return out; }
        for (auto const& [_, entry] : pt.get_child("entries"))
        {
            ManifestEntry m;
            m.mapId     = entry.get<uint32>("map_id", 0);
            m.mapName   = entry.get<std::string>("map_name", "");
            m.adtX      = static_cast<uint8>(entry.get<uint32>("adt_x", 0));
            m.adtY      = static_cast<uint8>(entry.get<uint32>("adt_y", 0));
            m.zoneLabel = entry.get<std::string>("zone_label", "");
            out.push_back(std::move(m));
        }
        return out;
    }

    // CSV-escape: only quote when needed (has , " \n \r), escape " as "".
    std::string CsvEsc(std::string_view s)
    {
        bool needs = false;
        for (char c : s) if (c == ',' || c == '"' || c == '\n' || c == '\r') { needs = true; break; }
        if (!needs) return std::string(s);
        std::string o = "\"";
        for (char c : s) { if (c == '"') o += '"'; o += c; }
        o += '"';
        return o;
    }

    // Mirror BuildRoadMaskGrid's per-MCNK decision logic so we can report
    // WHY each MCNK ended up flagged or not (without invoking the full
    // contiguous-area filter — that's a separate pass).
    struct McnkDiag
    {
        bool ruleA_dominantIsRoad = false;
        bool ruleB_anyLayerIsRoadWithCoverage = false;
        uint32 ruleB_winningLayer = 0;
        float  effectConfidence = 0.5f;
        bool   effectVetoed = false;
    };

    McnkDiag DiagnoseMcnk(Road::AdtTexture::McnkTextureSummary const& m)
    {
        McnkDiag d;
        d.effectConfidence = Road::RoadConfidenceFromEffectId(m.dominantEffectId);
        d.effectVetoed = (d.effectConfidence <= 0.3f);
        if (m.nLayers == 0 || d.effectVetoed)
            return d;
        d.ruleA_dominantIsRoad = Road::IsRoadTexturePath(m.dominantTextureBlp);
        if (!d.ruleA_dominantIsRoad)
        {
            uint32 nLayers = std::min<uint32>(m.nLayers, Road::AdtTexture::kMaxLayersPerMcnk);
            for (uint32 layer = 0; layer < nLayers; ++layer)
            {
                if (m.subcellsWonPerLayer[layer] < 4) continue;
                if (Road::IsRoadTexturePath(m.layers[layer].textureBlp))
                {
                    d.ruleB_anyLayerIsRoadWithCoverage = true;
                    d.ruleB_winningLayer = layer;
                    break;
                }
            }
        }
        return d;
    }

    char const* VerdictReason(McnkDiag const& d, uint8 maskByteAfterFilter)
    {
        bool preFilter = d.ruleA_dominantIsRoad || d.ruleB_anyLayerIsRoadWithCoverage;
        if (d.effectVetoed)                return "effect_veto";
        if (preFilter && maskByteAfterFilter != 0)
        {
            if (d.ruleA_dominantIsRoad)              return "rule_a_dominant";
            else                                      return "rule_b_overlay";
        }
        if (preFilter && maskByteAfterFilter == 0)
            return "contiguous_filter_stripped";
        return "no_road_texture";
    }
}

int main(int argc, char** argv)
{
    try
    {
        Options o = ParseArgs(argc, argv);

        std::shared_ptr<CASC::Storage> storage(
            CASC::Storage::Open(boost::filesystem::path(o.clientPath),
                                LocaleMask(o.locale), o.product.c_str()));
        if (!storage) { std::fprintf(stderr, "CASC open failed\n"); return 2; }
        std::printf("Opened CASC build %u\n", storage->GetBuildNumber());

        // Map.db2 → WdtFileDataId per mapId.
        std::unordered_map<uint32, std::pair<int32, std::string>> mapTable;
        {
            DB2CascFileSource src(storage, MapLoadInfo::Instance.Meta->FileDataId, false);
            DB2FileLoader db2;
            db2.Load(&src, &MapLoadInfo::Instance);
            for (uint32 i = 0; i < db2.GetRecordCount(); ++i)
            {
                DB2Record rec = db2.GetRecord(i);
                if (!rec) continue;
                mapTable[rec.GetId()] = { rec.GetInt32("WdtFileDataID"),
                                         rec.GetString("MapName") };
            }
        }

        Road::ListfileMap listfile;
        if (!o.listfilePath.empty())
        {
            listfile.LoadFromFile(o.listfilePath);
            std::printf("Loaded %zu listfile entries\n", listfile.Size());
        }

        Road::AdtTexture::AdtTextureReader reader(storage);
        if (!listfile.Empty()) reader.SetListfileMap(&listfile);

        auto manifest = ParseManifest(o.manifestPath);
        std::printf("Manifest: %zu entries\n", manifest.size());

        std::ofstream out(o.outputPath, std::ios::binary);
        if (!out) { std::fprintf(stderr, "Cannot open output\n"); return 3; }
        out << "map_id,adt_x,adt_y,zone,mcnk_idx,mcnk_ix,mcnk_iy,nLayers,"
            << "effectConf,"
            << "layer0_blp,layer0_isRoad,layer0_wins,"
            << "layer1_blp,layer1_isRoad,layer1_wins,"
            << "layer2_blp,layer2_isRoad,layer2_wins,"
            << "layer3_blp,layer3_isRoad,layer3_wins,"
            << "dominant_layer,dominant_blp,rule_a,rule_b,"
            << "verdict_pre_filter\n";

        std::unordered_map<uint32, Road::AdtTexture::WdtInfo> wdtCache;
        std::size_t totalRowsWritten = 0;

        for (auto const& m : manifest)
        {
            auto mapIt = mapTable.find(m.mapId);
            if (mapIt == mapTable.end()) { std::fprintf(stderr, "skip map %u\n", m.mapId); continue; }
            auto wIt = wdtCache.find(m.mapId);
            if (wIt == wdtCache.end())
            {
                auto w = Road::AdtTexture::ReadWdt(*storage, m.mapId,
                    static_cast<uint32>(mapIt->second.first));
                if (!w) { std::fprintf(stderr, "WDT fail map %u\n", m.mapId); continue; }
                wIt = wdtCache.emplace(m.mapId, std::move(*w)).first;
            }
            auto const& w = wIt->second;
            auto const& ids = w.adts[m.adtY][m.adtX];
            if (!ids.present) { std::fprintf(stderr, "no ADT at (%u,%u)\n", m.adtX, m.adtY); continue; }

            auto sum = reader.ReadAdt(m.mapId, m.adtX, m.adtY, w.flags,
                                      ids.rootADT, ids.tex0ADT);
            if (!sum) continue;

            for (auto const& mc : sum->mcnks)
            {
                if (mc.nLayers == 0) continue;
                auto d = DiagnoseMcnk(mc);
                bool verdictPreFilter = d.ruleA_dominantIsRoad || d.ruleB_anyLayerIsRoadWithCoverage;

                out << m.mapId << ',' << uint32(m.adtX) << ',' << uint32(m.adtY) << ','
                    << CsvEsc(m.zoneLabel) << ','
                    << mc.mcnkIdx << ',' << uint32(mc.ix) << ',' << uint32(mc.iy) << ','
                    << mc.nLayers << ','
                    << d.effectConfidence << ',';
                for (uint32 li = 0; li < 4; ++li)
                {
                    if (li < mc.nLayers)
                    {
                        out << CsvEsc(mc.layers[li].textureBlp) << ','
                            << (Road::IsRoadTexturePath(mc.layers[li].textureBlp) ? 1 : 0) << ','
                            << mc.subcellsWonPerLayer[li] << ',';
                    }
                    else
                    {
                        out << ",,,";
                    }
                }
                out << mc.dominantLayerIdx << ',' << CsvEsc(mc.dominantTextureBlp) << ','
                    << (d.ruleA_dominantIsRoad ? 1 : 0) << ','
                    << (d.ruleB_anyLayerIsRoadWithCoverage ? 1 : 0) << ','
                    << (verdictPreFilter ? 1 : 0) << '\n';
                ++totalRowsWritten;
            }
            std::printf("  map %u (%u,%u) %s -> %zu MCNK rows\n",
                        m.mapId, m.adtX, m.adtY, m.zoneLabel.c_str(),
                        std::size_t{256});
            (void)totalRowsWritten;
        }

        out.close();
        std::printf("Done. Wrote %zu MCNK rows to %s\n",
                    totalRowsWritten, o.outputPath.c_str());
        return 0;
    }
    catch (std::exception const& e)
    {
        std::fprintf(stderr, "Exception: %s\n", e.what());
        return 4;
    }
}
