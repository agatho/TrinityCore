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

#include "RoadClassifier.h"
#include "RoadValidation.h"

#include <cmath>
#include <unordered_set>

using namespace Road;
using namespace Road::Validation;

namespace
{
    CorpusEntry MakeEntry(uint32 mapId, uint8 adtX, uint8 adtY, uint8 mcnkIdx,
                          std::string textureBlp, Label label,
                          uint32 effectId = 0)
    {
        CorpusEntry e;
        e.mapId       = mapId;
        e.adtX        = adtX;
        e.adtY        = adtY;
        e.mcnkIdx     = mcnkIdx;
        e.textureBlp  = std::move(textureBlp);
        e.effectId    = effectId;
        e.layerCount  = 2;
        e.labeler     = "test";
        e.labelDate   = "2026-05-20";
        e.label       = label;
        e.confidence  = "HIGH";
        return e;
    }

    // Always-road classifier — used to force a known TP/FN pattern in tests.
    ClassifierVerdict AlwaysRoad(CorpusEntry const&)
    {
        ClassifierVerdict v;
        v.isRoad = true;
        v.matchedToken = "__forced_true__";
        return v;
    }

    // Always-not-road classifier.
    ClassifierVerdict NeverRoad(CorpusEntry const&)
    {
        ClassifierVerdict v;
        v.isRoad = false;
        return v;
    }

    // Oracle classifier — knows the ground truth, returns it exactly.
    // Used to confirm metrics math is correct.
    ClassifierVerdict Oracle(CorpusEntry const& e)
    {
        ClassifierVerdict v;
        v.isRoad = (e.label == Label::Road);
        return v;
    }
}

// =============================================================================
// Label parsing
// =============================================================================

TEST_CASE("LabelFromString - canonical forms", "[RoadValidation]")
{
    REQUIRE(LabelFromString("ROAD") == Label::Road);
    REQUIRE(LabelFromString("NOT_ROAD") == Label::NotRoad);
    REQUIRE(LabelFromString("AMBIGUOUS") == Label::Ambiguous);
}

TEST_CASE("LabelFromString - case insensitive + whitespace tolerant",
          "[RoadValidation]")
{
    REQUIRE(LabelFromString("road") == Label::Road);
    REQUIRE(LabelFromString("Road") == Label::Road);
    REQUIRE(LabelFromString(" ROAD ") == Label::Road);
    REQUIRE(LabelFromString("\tnot_road\n") == Label::NotRoad);
    REQUIRE(LabelFromString("notroad") == Label::NotRoad);   // tolerant variant
}

TEST_CASE("LabelFromString - unknown values default to Ambiguous",
          "[RoadValidation]")
{
    REQUIRE(LabelFromString("") == Label::Ambiguous);
    REQUIRE(LabelFromString("MAYBE") == Label::Ambiguous);
    REQUIRE(LabelFromString("road_ish") == Label::Ambiguous);
}

TEST_CASE("LabelToString round-trip", "[RoadValidation]")
{
    REQUIRE(LabelToString(Label::Road) == "ROAD");
    REQUIRE(LabelToString(Label::NotRoad) == "NOT_ROAD");
    REQUIRE(LabelToString(Label::Ambiguous) == "AMBIGUOUS");

    REQUIRE(LabelFromString(LabelToString(Label::Road)) == Label::Road);
    REQUIRE(LabelFromString(LabelToString(Label::NotRoad)) == Label::NotRoad);
    REQUIRE(LabelFromString(LabelToString(Label::Ambiguous)) == Label::Ambiguous);
}

// =============================================================================
// ConfusionMatrix metrics — verify the math
// =============================================================================

TEST_CASE("ConfusionMatrix - empty matrix returns NaN for all rates",
          "[RoadValidation]")
{
    ConfusionMatrix m;
    REQUIRE(std::isnan(m.Precision()));
    REQUIRE(std::isnan(m.Recall()));
    REQUIRE(std::isnan(m.F1()));
    REQUIRE(std::isnan(m.Accuracy()));
    REQUIRE(m.ScoredCount() == 0);
}

TEST_CASE("ConfusionMatrix - perfect classification gives 1.0 everywhere",
          "[RoadValidation]")
{
    ConfusionMatrix m;
    m.truePositive = 50;
    m.trueNegative = 50;
    REQUIRE(m.Precision() == Approx(1.0));
    REQUIRE(m.Recall() == Approx(1.0));
    REQUIRE(m.F1() == Approx(1.0));
    REQUIRE(m.Accuracy() == Approx(1.0));
}

TEST_CASE("ConfusionMatrix - all-wrong gives 0", "[RoadValidation]")
{
    ConfusionMatrix m;
    m.falsePositive = 50;
    m.falseNegative = 50;
    REQUIRE(m.Precision() == Approx(0.0));
    REQUIRE(m.Recall() == Approx(0.0));
    REQUIRE(std::isnan(m.F1()));      // P + R == 0
    REQUIRE(m.Accuracy() == Approx(0.0));
}

TEST_CASE("ConfusionMatrix - canonical TP=8 FP=2 TN=8 FN=2 example",
          "[RoadValidation]")
{
    ConfusionMatrix m;
    m.truePositive  = 8;
    m.falsePositive = 2;
    m.trueNegative  = 8;
    m.falseNegative = 2;
    REQUIRE(m.Precision() == Approx(8.0 / 10.0));
    REQUIRE(m.Recall()    == Approx(8.0 / 10.0));
    REQUIRE(m.F1()        == Approx(0.8));
    REQUIRE(m.Accuracy()  == Approx(16.0 / 20.0));
    REQUIRE(m.ScoredCount() == 20);
}

TEST_CASE("ConfusionMatrix - precision/recall asymmetry",
          "[RoadValidation]")
{
    // High precision, low recall (very few positive predictions, but most
    // correct). Mirrors the "tune harder, classifier is conservative" goal.
    ConfusionMatrix m;
    m.truePositive  = 9;
    m.falsePositive = 1;     // precision = 0.9
    m.trueNegative  = 100;
    m.falseNegative = 9;     // recall = 0.5
    REQUIRE(m.Precision() == Approx(0.9));
    REQUIRE(m.Recall() == Approx(0.5));
    REQUIRE(m.F1() == Approx(2.0 * 0.9 * 0.5 / (0.9 + 0.5)));
}

TEST_CASE("ConfusionMatrix - precision NaN when no positive predictions",
          "[RoadValidation]")
{
    ConfusionMatrix m;
    m.trueNegative  = 100;
    m.falseNegative = 10;
    REQUIRE(std::isnan(m.Precision()));
    REQUIRE(m.Recall() == Approx(0.0));
}

// =============================================================================
// ScoreCorpus
// =============================================================================

TEST_CASE("ScoreCorpus - empty corpus", "[RoadValidation]")
{
    std::vector<CorpusEntry> entries;
    ValidationResult r = ScoreCorpus(entries, &Oracle);
    REQUIRE(r.overall.ScoredCount() == 0);
    REQUIRE(r.overall.ambiguousSkipped == 0);
    REQUIRE(r.perZone.empty());
    REQUIRE(r.falsePositives.empty());
    REQUIRE(r.falseNegatives.empty());
}

TEST_CASE("ScoreCorpus - oracle classifier yields perfect metrics",
          "[RoadValidation]")
{
    std::vector<CorpusEntry> entries = {
        MakeEntry(0, 32, 32, 0, "TILESET/ELWYNN/cobble.blp", Label::Road),
        MakeEntry(0, 32, 32, 1, "TILESET/ELWYNN/grass.blp",  Label::NotRoad),
        MakeEntry(0, 32, 32, 2, "TILESET/ELWYNN/dirt.blp",   Label::NotRoad),
        MakeEntry(0, 32, 32, 3, "TILESET/ELWYNN/cobble.blp", Label::Road),
    };
    ValidationResult r = ScoreCorpus(entries, &Oracle);
    REQUIRE(r.overall.truePositive == 2);
    REQUIRE(r.overall.trueNegative == 2);
    REQUIRE(r.overall.falsePositive == 0);
    REQUIRE(r.overall.falseNegative == 0);
    REQUIRE(r.overall.Precision() == Approx(1.0));
    REQUIRE(r.overall.Recall() == Approx(1.0));
    REQUIRE(r.falsePositives.empty());
    REQUIRE(r.falseNegatives.empty());
}

TEST_CASE("ScoreCorpus - always-road classifier on mixed corpus",
          "[RoadValidation]")
{
    std::vector<CorpusEntry> entries = {
        MakeEntry(0, 0, 0, 0, "x", Label::Road),     // TP
        MakeEntry(0, 0, 0, 1, "y", Label::Road),     // TP
        MakeEntry(0, 0, 0, 2, "z", Label::NotRoad),  // FP
        MakeEntry(0, 0, 0, 3, "w", Label::NotRoad),  // FP
    };
    ValidationResult r = ScoreCorpus(entries, &AlwaysRoad);
    REQUIRE(r.overall.truePositive == 2);
    REQUIRE(r.overall.falsePositive == 2);
    REQUIRE(r.overall.trueNegative == 0);
    REQUIRE(r.overall.falseNegative == 0);
    REQUIRE(r.overall.Precision() == Approx(0.5));
    REQUIRE(r.overall.Recall() == Approx(1.0));
    REQUIRE(r.falsePositives.size() == 2);
    REQUIRE(r.falseNegatives.empty());
}

TEST_CASE("ScoreCorpus - never-road classifier on mixed corpus",
          "[RoadValidation]")
{
    std::vector<CorpusEntry> entries = {
        MakeEntry(0, 0, 0, 0, "x", Label::Road),     // FN
        MakeEntry(0, 0, 0, 1, "y", Label::NotRoad),  // TN
        MakeEntry(0, 0, 0, 2, "z", Label::NotRoad),  // TN
    };
    ValidationResult r = ScoreCorpus(entries, &NeverRoad);
    REQUIRE(r.overall.truePositive == 0);
    REQUIRE(r.overall.falsePositive == 0);
    REQUIRE(r.overall.trueNegative == 2);
    REQUIRE(r.overall.falseNegative == 1);
    REQUIRE(std::isnan(r.overall.Precision()));   // 0/0
    REQUIRE(r.overall.Recall() == Approx(0.0));
}

TEST_CASE("ScoreCorpus - AMBIGUOUS entries excluded from scoring",
          "[RoadValidation]")
{
    std::vector<CorpusEntry> entries = {
        MakeEntry(0, 0, 0, 0, "x", Label::Road),
        MakeEntry(0, 0, 0, 1, "y", Label::Ambiguous),
        MakeEntry(0, 0, 0, 2, "z", Label::Ambiguous),
        MakeEntry(0, 0, 0, 3, "w", Label::NotRoad),
    };
    ValidationResult r = ScoreCorpus(entries, &Oracle);
    REQUIRE(r.overall.ScoredCount() == 2);
    REQUIRE(r.overall.ambiguousSkipped == 2);
    REQUIRE(r.overall.truePositive == 1);
    REQUIRE(r.overall.trueNegative == 1);
    // Per-zone matrix should also reflect the skip.
    REQUIRE(r.perZone.size() == 1);
    REQUIRE(r.perZone[0].matrix.ambiguousSkipped == 2);
}

TEST_CASE("ScoreCorpus - per-zone breakdown by mapId", "[RoadValidation]")
{
    std::vector<CorpusEntry> entries = {
        MakeEntry(0, 0, 0, 0, "x", Label::Road),
        MakeEntry(0, 0, 0, 1, "y", Label::Road),
        MakeEntry(1, 0, 0, 0, "x", Label::Road),
        MakeEntry(1, 0, 0, 1, "y", Label::NotRoad),
        MakeEntry(530, 0, 0, 0, "x", Label::NotRoad),
    };
    ValidationResult r = ScoreCorpus(entries, &AlwaysRoad);

    REQUIRE(r.perZone.size() == 3);
    // Sorted by mapId.
    REQUIRE(r.perZone[0].mapId == 0);
    REQUIRE(r.perZone[1].mapId == 1);
    REQUIRE(r.perZone[2].mapId == 530);

    REQUIRE(r.perZone[0].matrix.truePositive == 2);
    REQUIRE(r.perZone[0].matrix.falsePositive == 0);

    REQUIRE(r.perZone[1].matrix.truePositive == 1);
    REQUIRE(r.perZone[1].matrix.falsePositive == 1);

    REQUIRE(r.perZone[2].matrix.truePositive == 0);
    REQUIRE(r.perZone[2].matrix.falsePositive == 1);

    // Overall matches sum of per-zone.
    REQUIRE(r.overall.truePositive == 3);
    REQUIRE(r.overall.falsePositive == 2);
}

TEST_CASE("ScoreCorpus - false-positive/negative indices point at right entries",
          "[RoadValidation]")
{
    std::vector<CorpusEntry> entries = {
        MakeEntry(0, 0, 0, 0, "tp", Label::Road),       // 0: TP
        MakeEntry(0, 0, 0, 1, "fp", Label::NotRoad),    // 1: FP
        MakeEntry(0, 0, 0, 2, "tn", Label::NotRoad),    // 2: TN
        MakeEntry(0, 0, 0, 3, "fn", Label::Road),       // 3: FN
    };

    // Classifier that says "road" for entries 0 and 1.
    auto cls = [](CorpusEntry const& e) {
        ClassifierVerdict v;
        v.isRoad = (e.mcnkIdx == 0 || e.mcnkIdx == 1);
        return v;
    };

    ValidationResult r = ScoreCorpus(entries, cls);
    REQUIRE(r.falsePositives.size() == 1);
    REQUIRE(r.falsePositives[0] == 1);
    REQUIRE(r.falseNegatives.size() == 1);
    REQUIRE(r.falseNegatives[0] == 3);
}

TEST_CASE("ScoreCorpus default overload uses real classifier",
          "[RoadValidation]")
{
    // Smoke test that the default-classifier overload hooks into
    // IsRoadTexturePath. Use known-positive and known-negative paths.
    std::vector<CorpusEntry> entries = {
        MakeEntry(0, 0, 0, 0, "TILESET/ELWYNN/ElwynnCobbleStoneBase.blp",
                  Label::Road),
        MakeEntry(0, 0, 0, 1, "TILESET/ELWYNN/ElwynnGrass.blp",
                  Label::NotRoad),
    };
    ClearGroundEffectTable();   // 0.5 default => above 0.3 threshold
    ValidationResult r = ScoreCorpus(entries);
    REQUIRE(r.overall.truePositive == 1);
    REQUIRE(r.overall.trueNegative == 1);
    REQUIRE(r.overall.falsePositive == 0);
    REQUIRE(r.overall.falseNegative == 0);
}

TEST_CASE("DefaultClassify - effect-confidence below 0.3 vetoes road verdict",
          "[RoadValidation]")
{
    ClearGroundEffectTable();
    SetGroundEffectConfidenceForTesting(99, 0.1f);   // strong vegetation signal

    CorpusEntry e = MakeEntry(0, 0, 0, 0,
        "TILESET/ELWYNN/ElwynnCobbleStoneBase.blp", Label::Road, /*effect*/ 99);

    ClassifierVerdict v = DefaultClassify(e);
    // Texture says road, but secondary signal (0.1) is below 0.3 threshold
    // → combined verdict is NOT road.
    REQUIRE_FALSE(v.isRoad);
    REQUIRE(v.effectConfidence == Approx(0.1f));
    REQUIRE(v.matchedToken == "cobble");
    ClearGroundEffectTable();
}

// =============================================================================
// CSV parsing
// =============================================================================

namespace
{
    constexpr std::string_view kValidCsv =
        "schema_version,map_id,adt_x,adt_y,mcnk_idx,texture_blp,effect_id,"
        "layer_count,labeler,label_date,label,confidence,notes\n"
        "1,0,32,32,57,TILESET/ELWYNN/ElwynnCobbleStoneBase.blp,42,3,"
        "daimon,2026-05-21,ROAD,HIGH,\"Stormwind to Goldshire main road\"\n"
        "1,0,32,32,58,TILESET/ELWYNN/ElwynnGrass.blp,0,2,"
        "daimon,2026-05-21,NOT_ROAD,HIGH,\n"
        "1,1,32,32,57,TILESET/Durotar/DUROTARROAD.BLP,0,2,"
        "daimon,2026-05-21,ROAD,MEDIUM,\"\"\n";
}

TEST_CASE("ParseCorpusCsv - happy path", "[RoadValidation]")
{
    std::vector<std::string> warnings;
    auto entries = ParseCorpusCsv(kValidCsv, &warnings);

    REQUIRE(warnings.empty());
    REQUIRE(entries.size() == 3);

    REQUIRE(entries[0].mapId == 0);
    REQUIRE(entries[0].adtX == 32);
    REQUIRE(entries[0].mcnkIdx == 57);
    REQUIRE(entries[0].textureBlp == "TILESET/ELWYNN/ElwynnCobbleStoneBase.blp");
    REQUIRE(entries[0].effectId == 42);
    REQUIRE(entries[0].label == Label::Road);
    REQUIRE(entries[0].notes == "Stormwind to Goldshire main road");

    REQUIRE(entries[1].label == Label::NotRoad);
    REQUIRE(entries[1].notes.empty());

    REQUIRE(entries[2].mapId == 1);
    REQUIRE(entries[2].textureBlp == "TILESET/Durotar/DUROTARROAD.BLP");
}

TEST_CASE("ParseCorpusCsv - handles escaped quotes inside notes",
          "[RoadValidation]")
{
    std::string csv =
        "schema_version,map_id,adt_x,adt_y,mcnk_idx,texture_blp,effect_id,"
        "layer_count,labeler,label_date,label,confidence,notes\n"
        "1,0,0,0,0,x,0,1,me,2026-05-21,ROAD,HIGH,"
        "\"He said \"\"hello\"\" yes\"\n";

    std::vector<std::string> warnings;
    auto entries = ParseCorpusCsv(csv, &warnings);

    REQUIRE(warnings.empty());
    REQUIRE(entries.size() == 1);
    REQUIRE(entries[0].notes == "He said \"hello\" yes");
}

TEST_CASE("ParseCorpusCsv - empty input warns and returns nothing",
          "[RoadValidation]")
{
    std::vector<std::string> warnings;
    auto entries = ParseCorpusCsv("", &warnings);
    REQUIRE(entries.empty());
    REQUIRE_FALSE(warnings.empty());
}

TEST_CASE("ParseCorpusCsv - tolerates \\r\\n line endings", "[RoadValidation]")
{
    std::string csv =
        "schema_version,map_id,adt_x,adt_y,mcnk_idx,texture_blp,effect_id,"
        "layer_count,labeler,label_date,label,confidence,notes\r\n"
        "1,0,0,0,0,x,0,1,me,2026-05-21,ROAD,HIGH,\r\n";

    std::vector<std::string> warnings;
    auto entries = ParseCorpusCsv(csv, &warnings);
    REQUIRE(warnings.empty());
    REQUIRE(entries.size() == 1);
}

TEST_CASE("ParseCorpusCsv - bad numeric field is a warning, row skipped",
          "[RoadValidation]")
{
    std::string csv =
        "schema_version,map_id,adt_x,adt_y,mcnk_idx,texture_blp,effect_id,"
        "layer_count,labeler,label_date,label,confidence,notes\n"
        "1,abc,0,0,0,x,0,1,me,2026-05-21,ROAD,HIGH,\n"
        "1,0,0,0,0,y,0,1,me,2026-05-21,ROAD,HIGH,\n";

    std::vector<std::string> warnings;
    auto entries = ParseCorpusCsv(csv, &warnings);
    REQUIRE(entries.size() == 1);   // only the valid row
    REQUIRE_FALSE(warnings.empty());
    REQUIRE(entries[0].textureBlp == "y");
}

TEST_CASE("ParseCorpusCsv - skips empty lines", "[RoadValidation]")
{
    std::string csv =
        "schema_version,map_id,adt_x,adt_y,mcnk_idx,texture_blp,effect_id,"
        "layer_count,labeler,label_date,label,confidence,notes\n"
        "\n"
        "1,0,0,0,0,x,0,1,me,2026-05-21,ROAD,HIGH,\n"
        "\n"
        "1,0,0,0,1,y,0,1,me,2026-05-21,NOT_ROAD,HIGH,\n"
        "\n";

    std::vector<std::string> warnings;
    auto entries = ParseCorpusCsv(csv, &warnings);
    REQUIRE(warnings.empty());
    REQUIRE(entries.size() == 2);
}

// =============================================================================
// CSV writing round-trip
// =============================================================================

TEST_CASE("WriteCorpusCsv + ParseCorpusCsv round-trip", "[RoadValidation]")
{
    std::vector<CorpusEntry> entries = {
        MakeEntry(0, 32, 32, 57, "TILESET/ELWYNN/cobble.blp", Label::Road, 42),
        MakeEntry(1, 31, 30, 99, "TILESET/Durotar/road.blp", Label::NotRoad),
    };
    entries[0].notes = "with, commas \"and\" quotes";
    entries[1].confidence = "MEDIUM";

    std::string csv = WriteCorpusCsv(entries);

    std::vector<std::string> warnings;
    auto reparsed = ParseCorpusCsv(csv, &warnings);

    REQUIRE(warnings.empty());
    REQUIRE(reparsed.size() == 2);
    REQUIRE(reparsed[0].textureBlp == entries[0].textureBlp);
    REQUIRE(reparsed[0].notes == entries[0].notes);
    REQUIRE(reparsed[0].label == entries[0].label);
    REQUIRE(reparsed[0].effectId == entries[0].effectId);
    REQUIRE(reparsed[1].confidence == "MEDIUM");
}

TEST_CASE("WriteMetricsCsv - header + overall + per-zone", "[RoadValidation]")
{
    std::vector<CorpusEntry> entries = {
        MakeEntry(0, 0, 0, 0, "x", Label::Road),     // TP
        MakeEntry(0, 0, 0, 1, "y", Label::NotRoad),  // TN with oracle
        MakeEntry(1, 0, 0, 0, "z", Label::Road),     // TP
    };
    ValidationResult r = ScoreCorpus(entries, &Oracle);

    std::string csv = WriteMetricsCsv(r);
    // Header.
    REQUIRE(csv.find("scope,map_id,tp,fp,tn,fn,") != std::string::npos);
    // Overall row.
    REQUIRE(csv.find("overall,-,2,0,1,0,") != std::string::npos);
    // Per-zone rows.
    REQUIRE(csv.find("zone,0,") != std::string::npos);
    REQUIRE(csv.find("zone,1,") != std::string::npos);
}

TEST_CASE("WriteDisagreementsCsv - emits FP then FN with kind column",
          "[RoadValidation]")
{
    std::vector<CorpusEntry> entries = {
        MakeEntry(0, 0, 0, 0, "fp_path", Label::NotRoad),    // FP under always-road
        MakeEntry(0, 0, 0, 1, "tp_path", Label::Road),       // TP under always-road
        MakeEntry(0, 0, 0, 2, "tn_path", Label::NotRoad),    // FP under always-road
    };
    ValidationResult r = ScoreCorpus(entries, &AlwaysRoad);

    std::string csv = WriteDisagreementsCsv(entries, r, &AlwaysRoad);

    // Header includes pred columns.
    REQUIRE(csv.find("pred_isroad,pred_effectconfidence,pred_matchedtoken")
            != std::string::npos);

    // FPs are emitted (entries 0 and 2).
    REQUIRE(csv.find("FP,") != std::string::npos);
    REQUIRE(csv.find("fp_path") != std::string::npos);
    REQUIRE(csv.find("tn_path") != std::string::npos);
    // pred_isroad column shows 1 for always-road classifier.
    REQUIRE(csv.find(",1,") != std::string::npos);
}

// =============================================================================
// Held-out split
// =============================================================================

TEST_CASE("HashLocation - distinct inputs produce distinct hashes",
          "[RoadValidation]")
{
    std::unordered_set<uint64> seen;
    for (uint32 m = 0; m < 5; ++m)
        for (uint8 x = 0; x < 10; ++x)
            for (uint8 y = 0; y < 10; ++y)
                for (uint8 k = 0; k < 16; ++k)
                    seen.insert(HashLocation(m, x, y, k));

    // No collisions on this small sample.
    REQUIRE(seen.size() == 5u * 10u * 10u * 16u);
}

TEST_CASE("DeterministicHeldOutSplit - empty input", "[RoadValidation]")
{
    std::vector<CorpusEntry> empty;
    HeldOutSplit split = DeterministicHeldOutSplit(empty, 0.10);
    REQUIRE(split.dev.empty());
    REQUIRE(split.heldOut.empty());
}

TEST_CASE("DeterministicHeldOutSplit - fraction 0.0 sends nothing to held-out",
          "[RoadValidation]")
{
    std::vector<CorpusEntry> entries;
    for (uint8 i = 0; i < 100; ++i)
        entries.push_back(MakeEntry(0, 0, 0, i, "x", Label::Road));

    HeldOutSplit split = DeterministicHeldOutSplit(entries, 0.0);
    REQUIRE(split.heldOut.empty());
    REQUIRE(split.dev.size() == 100);
}

TEST_CASE("DeterministicHeldOutSplit - fraction 1.0 sends everything to held-out",
          "[RoadValidation]")
{
    std::vector<CorpusEntry> entries;
    for (uint8 i = 0; i < 100; ++i)
        entries.push_back(MakeEntry(0, 0, 0, i, "x", Label::Road));

    HeldOutSplit split = DeterministicHeldOutSplit(entries, 1.0);
    REQUIRE(split.dev.empty());
    REQUIRE(split.heldOut.size() == 100);
}

TEST_CASE("DeterministicHeldOutSplit - 10% split is approximately correct",
          "[RoadValidation]")
{
    std::vector<CorpusEntry> entries;
    // 4000 entries spanning a real-ish key space.
    for (uint8 m = 0; m < 10; ++m)
        for (uint8 x = 0; x < 5; ++x)
            for (uint8 y = 0; y < 4; ++y)
                for (uint8 k = 0; k < 20; ++k)
                    entries.push_back(MakeEntry(m, x, y, k, "x", Label::Road));

    REQUIRE(entries.size() == 4000);

    HeldOutSplit split = DeterministicHeldOutSplit(entries, 0.10);
    // Allow ±2% tolerance for the hash bucket distribution.
    REQUIRE(split.heldOut.size() >= 320);
    REQUIRE(split.heldOut.size() <= 480);
    REQUIRE(split.dev.size() + split.heldOut.size() == 4000);
}

TEST_CASE("DeterministicHeldOutSplit - same input + same fraction = same split",
          "[RoadValidation]")
{
    std::vector<CorpusEntry> entries;
    for (uint8 i = 0; i < 100; ++i)
        entries.push_back(MakeEntry(0, 0, 0, i, "x", Label::Road));

    HeldOutSplit a = DeterministicHeldOutSplit(entries, 0.10);
    HeldOutSplit b = DeterministicHeldOutSplit(entries, 0.10);

    REQUIRE(a.dev.size() == b.dev.size());
    REQUIRE(a.heldOut.size() == b.heldOut.size());

    // Same entries appear in the same partition.
    for (std::size_t i = 0; i < a.heldOut.size(); ++i)
        REQUIRE(a.heldOut[i].mcnkIdx == b.heldOut[i].mcnkIdx);
}

TEST_CASE("DeterministicHeldOutSplit - input ORDER does NOT affect split",
          "[RoadValidation]")
{
    std::vector<CorpusEntry> entries;
    for (uint8 i = 0; i < 100; ++i)
        entries.push_back(MakeEntry(0, 0, 0, i, "x", Label::Road));

    std::vector<CorpusEntry> reversed(entries.rbegin(), entries.rend());

    HeldOutSplit a = DeterministicHeldOutSplit(entries, 0.10);
    HeldOutSplit b = DeterministicHeldOutSplit(reversed, 0.10);

    // Same SET of entries in held-out (order may differ).
    std::unordered_set<uint32> setA, setB;
    for (auto const& e : a.heldOut) setA.insert(e.mcnkIdx);
    for (auto const& e : b.heldOut) setB.insert(e.mcnkIdx);
    REQUIRE(setA == setB);
}

TEST_CASE("DeterministicHeldOutSplit - scoring dev + held-out independently",
          "[RoadValidation]")
{
    // Composite test demonstrating the P1.0c workflow: split → score → assert.
    std::vector<CorpusEntry> entries;
    for (uint8 i = 0; i < 100; ++i)
        entries.push_back(MakeEntry(0, 0, 0, i,
            (i % 3 == 0) ? "TILESET/road.blp" : "TILESET/grass.blp",
            (i % 3 == 0) ? Label::Road : Label::NotRoad));

    ClearGroundEffectTable();
    HeldOutSplit split = DeterministicHeldOutSplit(entries, 0.20);

    ValidationResult devR     = ScoreCorpus(split.dev);
    ValidationResult heldOutR = ScoreCorpus(split.heldOut);

    // Both should classify perfectly (or near-perfectly) since our toy
    // texture names trivially match.
    REQUIRE(devR.overall.Precision() == Approx(1.0));
    REQUIRE(heldOutR.overall.Precision() == Approx(1.0));
}
