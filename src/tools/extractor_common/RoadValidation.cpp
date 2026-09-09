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

#include "RoadValidation.h"
#include "RoadClassifier.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <map>
#include <sstream>
#include <string>

namespace Road::Validation
{
    // -------------------------------------------------------------------
    // Label parsing
    // -------------------------------------------------------------------

    namespace
    {
        std::string ToUpperAscii(std::string_view s)
        {
            std::string out;
            out.reserve(s.size());
            for (char c : s)
                out.push_back((c >= 'a' && c <= 'z') ? static_cast<char>(c - 32) : c);
            return out;
        }

        // Trim ASCII whitespace from both ends.
        std::string_view TrimAscii(std::string_view s)
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
    }

    Label LabelFromString(std::string_view s)
    {
        std::string u = ToUpperAscii(TrimAscii(s));
        if (u == "ROAD")      return Label::Road;
        if (u == "NOT_ROAD")  return Label::NotRoad;
        if (u == "NOTROAD")   return Label::NotRoad;   // tolerant
        if (u == "AMBIGUOUS") return Label::Ambiguous;
        return Label::Ambiguous;
    }

    // -------------------------------------------------------------------
    // Default classifier — wires RoadClassifier per design doc §3.3
    // -------------------------------------------------------------------

    ClassifierVerdict DefaultClassify(CorpusEntry const& entry)
    {
        ClassifierVerdict v;
        bool const texHit = IsRoadTexturePath(entry.textureBlp);
        v.matchedToken = std::string(MatchedRoadToken(entry.textureBlp));
        v.effectConfidence = RoadConfidenceFromEffectId(entry.effectId);
        // Combined rule: BOTH signals must concur. effectConfidence > 0.3
        // mirrors the threshold in the design doc.
        v.isRoad = texHit && (v.effectConfidence > 0.3f);
        return v;
    }

    // -------------------------------------------------------------------
    // Confusion-matrix metrics
    // -------------------------------------------------------------------

    namespace
    {
        constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
    }

    double ConfusionMatrix::Precision() const noexcept
    {
        uint64 denom = truePositive + falsePositive;
        if (denom == 0)
            return kNaN;
        return static_cast<double>(truePositive) / static_cast<double>(denom);
    }

    double ConfusionMatrix::Recall() const noexcept
    {
        uint64 denom = truePositive + falseNegative;
        if (denom == 0)
            return kNaN;
        return static_cast<double>(truePositive) / static_cast<double>(denom);
    }

    double ConfusionMatrix::F1() const noexcept
    {
        double p = Precision();
        double r = Recall();
        if (std::isnan(p) || std::isnan(r))
            return kNaN;
        if (p + r == 0.0)
            return kNaN;
        return 2.0 * p * r / (p + r);
    }

    double ConfusionMatrix::Accuracy() const noexcept
    {
        uint64 scored = ScoredCount();
        if (scored == 0)
            return kNaN;
        return static_cast<double>(truePositive + trueNegative) /
               static_cast<double>(scored);
    }

    // -------------------------------------------------------------------
    // ScoreCorpus
    // -------------------------------------------------------------------

    ValidationResult ScoreCorpus(std::span<CorpusEntry const> entries,
                                 ClassifierFn const& classifier)
    {
        ValidationResult result;
        std::map<uint32, ConfusionMatrix> perZone;

        for (std::size_t i = 0; i < entries.size(); ++i)
        {
            CorpusEntry const& e = entries[i];

            if (e.label == Label::Ambiguous)
            {
                ++result.overall.ambiguousSkipped;
                ++perZone[e.mapId].ambiguousSkipped;
                continue;
            }

            ClassifierVerdict v = classifier(e);
            bool truth = (e.label == Label::Road);
            bool pred  = v.isRoad;

            ConfusionMatrix& zm = perZone[e.mapId];
            if (truth && pred)
            {
                ++result.overall.truePositive;
                ++zm.truePositive;
            }
            else if (!truth && pred)
            {
                ++result.overall.falsePositive;
                ++zm.falsePositive;
                result.falsePositives.push_back(i);
            }
            else if (!truth && !pred)
            {
                ++result.overall.trueNegative;
                ++zm.trueNegative;
            }
            else // truth && !pred
            {
                ++result.overall.falseNegative;
                ++zm.falseNegative;
                result.falseNegatives.push_back(i);
            }
        }

        result.perZone.reserve(perZone.size());
        for (auto const& [mapId, matrix] : perZone)
            result.perZone.push_back({ mapId, matrix });

        return result;
    }

    ValidationResult ScoreCorpus(std::span<CorpusEntry const> entries)
    {
        return ScoreCorpus(entries, &DefaultClassify);
    }

    // -------------------------------------------------------------------
    // CSV parsing
    //
    // Hand-rolled to avoid a third-party dependency. Conforms to the
    // common-sense subset of RFC 4180:
    //   - Fields separated by ','.
    //   - A field starting with '"' is quoted; ends at the next '"' that
    //     is not followed by another '"'. Inside a quoted field, '""'
    //     escapes a literal '"'.
    //   - Lines separated by '\n' (or '\r\n'; '\r' alone NOT supported).
    //   - Empty lines are skipped.
    //   - Unterminated quoted fields trigger a warning and are best-
    //     effort recovered to end-of-line.
    // -------------------------------------------------------------------

    namespace
    {
        // Parse one CSV row into a vector of field string_views into a
        // backing string buffer. Returns the position after the row's
        // newline (or csv.size() if at EOF).
        std::size_t ParseCsvRow(std::string_view csv, std::size_t pos,
                                std::vector<std::string>& fields,
                                std::vector<std::string>& warnings,
                                std::size_t lineNumber)
        {
            fields.clear();
            std::string field;

            auto flushField = [&]() {
                fields.push_back(std::move(field));
                field.clear();
            };

            bool inQuotes = false;

            while (pos < csv.size())
            {
                char c = csv[pos];

                if (inQuotes)
                {
                    if (c == '"')
                    {
                        // Lookahead for escape "".
                        if (pos + 1 < csv.size() && csv[pos + 1] == '"')
                        {
                            field.push_back('"');
                            pos += 2;
                        }
                        else
                        {
                            inQuotes = false;
                            ++pos;
                        }
                    }
                    else
                    {
                        field.push_back(c);
                        ++pos;
                    }
                }
                else
                {
                    if (c == '"' && field.empty())
                    {
                        inQuotes = true;
                        ++pos;
                    }
                    else if (c == ',')
                    {
                        flushField();
                        ++pos;
                    }
                    else if (c == '\n')
                    {
                        flushField();
                        return pos + 1;
                    }
                    else if (c == '\r')
                    {
                        // Skip; \r\n line ending.
                        if (pos + 1 < csv.size() && csv[pos + 1] == '\n')
                        {
                            flushField();
                            return pos + 2;
                        }
                        // Bare \r — treat as end of line.
                        flushField();
                        return pos + 1;
                    }
                    else
                    {
                        field.push_back(c);
                        ++pos;
                    }
                }
            }

            if (inQuotes)
            {
                warnings.push_back("line " + std::to_string(lineNumber) +
                                   ": unterminated quoted field, recovered to EOF");
            }

            flushField();
            return pos;
        }

        bool ParseUInt(std::string const& s, uint32& out)
        {
            std::string_view sv = TrimAscii(s);
            if (sv.empty())
            {
                out = 0;
                return false;
            }
            uint32 v = 0;
            auto* first = sv.data();
            auto* last  = sv.data() + sv.size();
            auto r = std::from_chars(first, last, v);
            if (r.ec != std::errc{} || r.ptr != last)
                return false;
            out = v;
            return true;
        }

        bool ParseUInt8(std::string const& s, uint8& out)
        {
            uint32 v;
            if (!ParseUInt(s, v) || v > 255)
                return false;
            out = static_cast<uint8>(v);
            return true;
        }
    }

    std::vector<CorpusEntry> ParseCorpusCsv(std::string_view csvContent,
                                            std::vector<std::string>* warnings)
    {
        std::vector<std::string> localWarnings;
        std::vector<std::string>& wlist = warnings ? *warnings : localWarnings;

        std::vector<CorpusEntry> out;
        std::vector<std::string> fields;
        std::size_t pos = 0;
        std::size_t lineNumber = 0;
        bool headerRead = false;

        // Expected columns in expected order.
        static constexpr std::string_view kExpectedHeader[] = {
            "schema_version", "map_id", "adt_x", "adt_y", "mcnk_idx",
            "texture_blp", "effect_id", "layer_count", "labeler",
            "label_date", "label", "confidence", "notes"
        };
        constexpr std::size_t kExpectedColumnCount =
            sizeof(kExpectedHeader) / sizeof(kExpectedHeader[0]);

        while (pos < csvContent.size())
        {
            ++lineNumber;
            pos = ParseCsvRow(csvContent, pos, fields, wlist, lineNumber);

            // Skip empty rows.
            if (fields.empty() ||
                (fields.size() == 1 && TrimAscii(fields[0]).empty()))
                continue;

            if (!headerRead)
            {
                if (fields.size() < kExpectedColumnCount)
                {
                    wlist.push_back("line " + std::to_string(lineNumber) +
                                    ": header has " + std::to_string(fields.size()) +
                                    " columns, expected " +
                                    std::to_string(kExpectedColumnCount));
                }
                // We don't strictly require header names to match — order is
                // contractual. But warn if anything is off.
                for (std::size_t i = 0; i < kExpectedColumnCount && i < fields.size(); ++i)
                {
                    std::string lower;
                    lower.reserve(fields[i].size());
                    for (char c : fields[i])
                        lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
                    if (lower != kExpectedHeader[i])
                    {
                        wlist.push_back("line " + std::to_string(lineNumber) +
                                        ": column " + std::to_string(i) +
                                        " expected '" +
                                        std::string(kExpectedHeader[i]) +
                                        "', got '" + fields[i] + "'");
                    }
                }
                headerRead = true;
                continue;
            }

            if (fields.size() < kExpectedColumnCount)
            {
                wlist.push_back("line " + std::to_string(lineNumber) +
                                ": skipping row with " +
                                std::to_string(fields.size()) + " fields");
                continue;
            }

            CorpusEntry e;
            bool ok = true;
            ok &= ParseUInt(fields[0], e.schemaVersion);
            ok &= ParseUInt(fields[1], e.mapId);
            ok &= ParseUInt8(fields[2], e.adtX);
            ok &= ParseUInt8(fields[3], e.adtY);
            ok &= ParseUInt8(fields[4], e.mcnkIdx);
            e.textureBlp = fields[5];
            ok &= ParseUInt(fields[6], e.effectId);
            ok &= ParseUInt8(fields[7], e.layerCount);
            e.labeler   = fields[8];
            e.labelDate = fields[9];
            e.label     = LabelFromString(fields[10]);
            e.confidence = fields[11];
            e.notes      = fields[12];

            if (!ok)
            {
                wlist.push_back("line " + std::to_string(lineNumber) +
                                ": one or more numeric fields failed to parse");
                continue;
            }
            out.push_back(std::move(e));
        }

        if (!headerRead)
        {
            wlist.push_back("CSV is empty or has no header row");
        }

        return out;
    }

    // -------------------------------------------------------------------
    // CSV writing
    // -------------------------------------------------------------------

    namespace
    {
        // Quote a field if it contains ',', '"', '\n', or '\r'. Always
        // quote when it contains '"' (must be escaped).
        std::string CsvEscape(std::string_view s)
        {
            bool needsQuote = false;
            for (char c : s)
            {
                if (c == ',' || c == '"' || c == '\n' || c == '\r')
                {
                    needsQuote = true;
                    break;
                }
            }
            if (!needsQuote)
                return std::string(s);

            std::string out;
            out.reserve(s.size() + 2);
            out.push_back('"');
            for (char c : s)
            {
                if (c == '"')
                    out.push_back('"');  // escape quote with double-quote
                out.push_back(c);
            }
            out.push_back('"');
            return out;
        }

        void AppendUInt(std::string& out, uint64 v)
        {
            char buf[32];
            auto r = std::to_chars(buf, buf + sizeof(buf), v);
            out.append(buf, r.ptr);
        }

        void AppendDouble(std::string& out, double v)
        {
            if (std::isnan(v))
            {
                out.append("nan");
                return;
            }
            // 6 significant digits is plenty for human reading.
            char buf[32];
            auto r = std::to_chars(buf, buf + sizeof(buf), v,
                                   std::chars_format::fixed, 6);
            out.append(buf, r.ptr);
        }

        void AppendEntryFields(std::string& out, CorpusEntry const& e)
        {
            AppendUInt(out, e.schemaVersion);
            out.push_back(',');
            AppendUInt(out, e.mapId);
            out.push_back(',');
            AppendUInt(out, e.adtX);
            out.push_back(',');
            AppendUInt(out, e.adtY);
            out.push_back(',');
            AppendUInt(out, e.mcnkIdx);
            out.push_back(',');
            out.append(CsvEscape(e.textureBlp));
            out.push_back(',');
            AppendUInt(out, e.effectId);
            out.push_back(',');
            AppendUInt(out, e.layerCount);
            out.push_back(',');
            out.append(CsvEscape(e.labeler));
            out.push_back(',');
            out.append(CsvEscape(e.labelDate));
            out.push_back(',');
            out.append(LabelToString(e.label));
            out.push_back(',');
            out.append(CsvEscape(e.confidence));
            out.push_back(',');
            out.append(CsvEscape(e.notes));
        }
    }

    std::string WriteCorpusCsv(std::span<CorpusEntry const> entries)
    {
        std::string out;
        out.append("schema_version,map_id,adt_x,adt_y,mcnk_idx,"
                   "texture_blp,effect_id,layer_count,labeler,"
                   "label_date,label,confidence,notes\n");
        for (CorpusEntry const& e : entries)
        {
            AppendEntryFields(out, e);
            out.push_back('\n');
        }
        return out;
    }

    std::string WriteMetricsCsv(ValidationResult const& result)
    {
        std::string out;
        out.append("scope,map_id,tp,fp,tn,fn,ambiguous_skipped,"
                   "precision,recall,f1,accuracy,scored_total\n");

        auto appendRow = [&](std::string_view scope, uint32 mapId,
                             ConfusionMatrix const& m) {
            out.append(scope);
            out.push_back(',');
            if (scope == "overall")
                out.append("-");
            else
                AppendUInt(out, mapId);
            out.push_back(',');
            AppendUInt(out, m.truePositive);    out.push_back(',');
            AppendUInt(out, m.falsePositive);   out.push_back(',');
            AppendUInt(out, m.trueNegative);    out.push_back(',');
            AppendUInt(out, m.falseNegative);   out.push_back(',');
            AppendUInt(out, m.ambiguousSkipped); out.push_back(',');
            AppendDouble(out, m.Precision());   out.push_back(',');
            AppendDouble(out, m.Recall());      out.push_back(',');
            AppendDouble(out, m.F1());          out.push_back(',');
            AppendDouble(out, m.Accuracy());    out.push_back(',');
            AppendUInt(out, m.ScoredCount());
            out.push_back('\n');
        };

        appendRow("overall", 0, result.overall);
        for (ZoneMetrics const& z : result.perZone)
            appendRow("zone", z.mapId, z.matrix);

        return out;
    }

    std::string WriteDisagreementsCsv(std::span<CorpusEntry const> entries,
                                       ValidationResult const& result,
                                       ClassifierFn const& classifier)
    {
        std::string out;
        out.append("kind,schema_version,map_id,adt_x,adt_y,mcnk_idx,"
                   "texture_blp,effect_id,layer_count,labeler,"
                   "label_date,label,confidence,notes,"
                   "pred_isroad,pred_effectconfidence,pred_matchedtoken\n");

        auto appendDisagreement = [&](std::string_view kind, std::size_t idx) {
            if (idx >= entries.size())
                return;
            CorpusEntry const& e = entries[idx];
            ClassifierVerdict v = classifier(e);
            out.append(kind);
            out.push_back(',');
            AppendEntryFields(out, e);
            out.push_back(',');
            out.append(v.isRoad ? "1" : "0");
            out.push_back(',');
            AppendDouble(out, v.effectConfidence);
            out.push_back(',');
            out.append(CsvEscape(v.matchedToken));
            out.push_back('\n');
        };

        for (std::size_t idx : result.falsePositives)
            appendDisagreement("FP", idx);
        for (std::size_t idx : result.falseNegatives)
            appendDisagreement("FN", idx);

        return out;
    }

    std::string WriteDisagreementsCsv(std::span<CorpusEntry const> entries,
                                       ValidationResult const& result)
    {
        return WriteDisagreementsCsv(entries, result, &DefaultClassify);
    }

    // -------------------------------------------------------------------
    // Deterministic held-out split.
    //
    // We use SplitMix64 to hash the 4-tuple (mapId, adtX, adtY, mcnkIdx)
    // into a uniform 64-bit value, then partition by (hash mod 1000) <
    // (fraction * 1000).
    //
    // SplitMix64 is a tiny finalizer mix; trivially deterministic across
    // compilers; output is uniform enough for partitioning by modulus.
    // -------------------------------------------------------------------

    uint64 HashLocation(uint32 mapId, uint8 adtX, uint8 adtY, uint8 mcnkIdx) noexcept
    {
        // Pack into 64 bits with a distinct mixing constant per field so
        // two zones with similar (x,y) don't collide.
        uint64 z = static_cast<uint64>(mapId) * 0x9E3779B97F4A7C15ull;
        z ^= static_cast<uint64>(adtX) * 0xBF58476D1CE4E5B9ull;
        z ^= static_cast<uint64>(adtY) * 0x94D049BB133111EBull;
        z ^= static_cast<uint64>(mcnkIdx) * 0xD1342543DE82EF95ull;
        // SplitMix64 finalizer.
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        z = z ^ (z >> 31);
        return z;
    }

    HeldOutSplit DeterministicHeldOutSplit(std::span<CorpusEntry const> entries,
                                            double heldOutFraction)
    {
        heldOutFraction = std::clamp(heldOutFraction, 0.0, 1.0);
        // Convert to integer threshold over 10000 buckets for ~0.01% precision.
        uint64 threshold = static_cast<uint64>(heldOutFraction * 10000.0 + 0.5);

        HeldOutSplit split;
        split.dev.reserve(entries.size());
        split.heldOut.reserve(static_cast<std::size_t>(entries.size() * heldOutFraction));

        for (CorpusEntry const& e : entries)
        {
            uint64 h = HashLocation(e.mapId, e.adtX, e.adtY, e.mcnkIdx);
            uint64 bucket = h % 10000ull;
            if (bucket < threshold)
                split.heldOut.push_back(e);
            else
                split.dev.push_back(e);
        }
        return split;
    }
}
