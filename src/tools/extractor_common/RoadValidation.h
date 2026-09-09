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

#ifndef TRINITYCORE_ROAD_VALIDATION_H
#define TRINITYCORE_ROAD_VALIDATION_H

#include "Define.h"

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Road::Validation
{
    // -------------------------------------------------------------------
    // Ground-truth corpus data model.
    //
    // One entry per labeled MCNK. The CSV serialization mirrors this
    // struct one-to-one; see CORPUS_BUILDING_GUIDE.md.
    // -------------------------------------------------------------------

    enum class Label : uint8
    {
        Road       = 1,    // human says: this MCNK IS a road
        NotRoad    = 2,    // human says: this MCNK is NOT a road
        Ambiguous  = 3,    // human says: judgment call, skip from scoring
    };

    constexpr std::string_view LabelToString(Label l)
    {
        switch (l)
        {
            case Label::Road:      return "ROAD";
            case Label::NotRoad:   return "NOT_ROAD";
            case Label::Ambiguous: return "AMBIGUOUS";
        }
        return "INVALID";
    }

    // Parse "ROAD" | "NOT_ROAD" | "AMBIGUOUS" (case-insensitive).
    // Returns Label::Ambiguous if unrecognized — caller should treat this
    // as a parse warning and record it.
    Label LabelFromString(std::string_view s);

    struct CorpusEntry
    {
        uint32 schemaVersion = 1;
        uint32 mapId         = 0;       // TC continent id
        uint8  adtX          = 0;       // 0..63
        uint8  adtY          = 0;       // 0..63
        uint8  mcnkIdx       = 0;       // 0..255 (16x16 grid within ADT)
        std::string textureBlp;         // dominant-layer BLP path
        uint32 effectId      = 0;       // MCLY effectId (0 / -1 = none)
        uint8  layerCount    = 0;       // 1..4
        std::string labeler;
        std::string labelDate;          // ISO-8601 yyyy-mm-dd
        Label  label         = Label::Ambiguous;
        std::string confidence;         // free-form: HIGH/MEDIUM/LOW
        std::string notes;              // free-form, commas escaped
    };

    // -------------------------------------------------------------------
    // Classifier under test
    //
    // The harness invokes a function-object per CorpusEntry. This indirection
    // exists so tests can supply mock classifiers and scoring stays unit-
    // testable.
    // -------------------------------------------------------------------

    struct ClassifierVerdict
    {
        bool  isRoad           = false;
        float effectConfidence = 0.5f;          // [0..1] from secondary signal
        std::string matchedToken;               // for telemetry/diagnostics
    };

    using ClassifierFn = std::function<ClassifierVerdict(CorpusEntry const&)>;

    // The default classifier — wires IsRoadTexturePath + RoadConfidenceFromEffectId
    // exactly as the design doc §3.3 prescribes:
    //   verdict.isRoad = IsRoadTexturePath(textureBlp) &&
    //                    RoadConfidenceFromEffectId(effectId) > 0.3
    //
    // Lives in RoadValidation.cpp so callers don't need to know the wiring.
    ClassifierVerdict DefaultClassify(CorpusEntry const& entry);

    // -------------------------------------------------------------------
    // Confusion matrix + derived metrics
    // -------------------------------------------------------------------

    struct ConfusionMatrix
    {
        uint64 truePositive      = 0;
        uint64 falsePositive     = 0;
        uint64 trueNegative      = 0;
        uint64 falseNegative     = 0;
        uint64 ambiguousSkipped  = 0;

        uint64 ScoredCount() const noexcept
        {
            return truePositive + falsePositive + trueNegative + falseNegative;
        }

        // Precision = TP / (TP + FP). Returns NaN when no positive predictions.
        double Precision() const noexcept;

        // Recall = TP / (TP + FN). Returns NaN when no positive ground truths.
        double Recall() const noexcept;

        // F1 = 2 * P * R / (P + R). Returns NaN when P or R is NaN or both 0.
        double F1() const noexcept;

        // Accuracy = (TP + TN) / scored. NaN if no scored.
        double Accuracy() const noexcept;
    };

    struct ZoneMetrics
    {
        uint32 mapId = 0;
        ConfusionMatrix matrix;
    };

    struct ValidationResult
    {
        ConfusionMatrix overall;
        std::vector<ZoneMetrics> perZone;             // sorted by mapId

        // Indices into the input corpus for each disagreement type.
        // Used by WriteDisagreementsCsv to dump full rows for human review.
        std::vector<std::size_t> falsePositives;
        std::vector<std::size_t> falseNegatives;

        // Parse warnings encountered during corpus ingest (empty if loaded
        // pre-parsed). Populated by ParseCorpusCsv.
        std::vector<std::string> warnings;
    };

    // -------------------------------------------------------------------
    // Scoring entry point. Pure function; no I/O.
    //
    // For each entry:
    //   - If label == Ambiguous: increment ambiguousSkipped, continue.
    //   - Else invoke classifier; record TP / FP / TN / FN.
    // Per-zone matrices break down by mapId.
    // -------------------------------------------------------------------

    ValidationResult ScoreCorpus(std::span<CorpusEntry const> entries,
                                 ClassifierFn const& classifier);

    // Convenience overload using DefaultClassify.
    ValidationResult ScoreCorpus(std::span<CorpusEntry const> entries);

    // -------------------------------------------------------------------
    // CSV ingest / output
    //
    // Schema (header row, in order, comma-separated):
    //   schema_version, map_id, adt_x, adt_y, mcnk_idx, texture_blp,
    //   effect_id, layer_count, labeler, label_date, label,
    //   confidence, notes
    //
    // notes is the only field permitted to contain commas; it MUST be
    // quoted with " " if it contains , or ". Double-quotes inside the
    // notes field are escaped as "".
    //
    // ParseCorpusCsv pushes parse errors into the optional `warnings`
    // out-parameter (line number + reason) rather than throwing.
    // -------------------------------------------------------------------

    std::vector<CorpusEntry> ParseCorpusCsv(std::string_view csvContent,
                                            std::vector<std::string>* warnings = nullptr);

    // Serialize entries to CSV string (with the header row).
    std::string WriteCorpusCsv(std::span<CorpusEntry const> entries);

    // Top-line metrics summary (overall + per-zone) as CSV.
    std::string WriteMetricsCsv(ValidationResult const& result);

    // One row per FP and FN, with full corpus entry data + classifier verdict.
    // Useful for human review during P1.2 iteration.
    std::string WriteDisagreementsCsv(std::span<CorpusEntry const> entries,
                                       ValidationResult const& result,
                                       ClassifierFn const& classifier);
    std::string WriteDisagreementsCsv(std::span<CorpusEntry const> entries,
                                       ValidationResult const& result);

    // -------------------------------------------------------------------
    // Deterministic held-out split.
    //
    // We hash (mapId, adtX, adtY, mcnkIdx) into a 64-bit value; the first
    // `heldOutFraction` portion (by hash modulus) goes to the held-out
    // slice. Same corpus + same fraction always yields the same split,
    // regardless of input ordering — critical for reproducibility of the
    // P1.0c "dev vs held-out" generalization check.
    //
    // heldOutFraction is clamped to [0, 1].
    // -------------------------------------------------------------------

    struct HeldOutSplit
    {
        std::vector<CorpusEntry> dev;
        std::vector<CorpusEntry> heldOut;
    };

    HeldOutSplit DeterministicHeldOutSplit(std::span<CorpusEntry const> entries,
                                            double heldOutFraction = 0.10);

    // Exposed for testability — the hash function used by the split.
    uint64 HashLocation(uint32 mapId, uint8 adtX, uint8 adtY, uint8 mcnkIdx) noexcept;
}

#endif // TRINITYCORE_ROAD_VALIDATION_H
