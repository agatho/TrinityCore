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

// road_validator
//
// Reads a ground-truth corpus CSV, runs the road classifier against it,
// and emits metrics + disagreements CSVs. Driven by the P1.0b "build the
// validation rig BEFORE classifier tuning" workflow.
//
// Usage:
//   road_validator --corpus <path> [--out-dir <dir>]
//                  [--split-fraction <f>] [--mode dev|heldout|all]
//
// Examples:
//   road_validator --corpus corpus.csv
//      → writes ./metrics.csv + ./disagreements.csv, prints summary
//
//   road_validator --corpus corpus.csv --mode heldout --split-fraction 0.10
//      → scores ONLY the 10% deterministic held-out slice
//
// All flags have sensible defaults; no positional args.

#include "RoadClassifier.h"
#include "RoadValidation.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    struct Options
    {
        std::string corpusPath;
        std::string outDir       = ".";
        double      splitFraction = 0.10;
        enum class Mode { All, Dev, HeldOut } mode = Mode::All;
    };

    [[noreturn]] void PrintUsageAndExit(int exitCode)
    {
        std::fprintf(stderr,
            "road_validator — score a road-classifier ground-truth corpus\n"
            "\n"
            "Usage:\n"
            "  road_validator --corpus <path> [options]\n"
            "\n"
            "Options:\n"
            "  --corpus <path>             Path to corpus CSV (required)\n"
            "  --out-dir <dir>             Where to write metrics.csv +\n"
            "                              disagreements.csv (default: .)\n"
            "  --split-fraction <0..1>     Held-out fraction (default 0.10)\n"
            "  --mode all|dev|heldout      Which slice to score (default: all)\n"
            "  --help                      Print this message and exit\n"
            "\n"
            "Exit codes:\n"
            "  0  success (metrics emitted; may or may not meet quality bar)\n"
            "  1  bad CLI args\n"
            "  2  corpus parse error or no rows scored\n"
            "  3  I/O error writing output\n");
        std::exit(exitCode);
    }

    bool ParseDouble(std::string_view s, double& out)
    {
        try
        {
            std::size_t pos = 0;
            std::string str(s);
            out = std::stod(str, &pos);
            return pos == str.size();
        }
        catch (...)
        {
            return false;
        }
    }

    Options ParseArgs(int argc, char** argv)
    {
        Options opts;
        for (int i = 1; i < argc; ++i)
        {
            std::string_view arg = argv[i];
            auto wantValue = [&](char const* flag) -> std::string_view {
                if (i + 1 >= argc)
                {
                    std::fprintf(stderr, "%s requires a value\n", flag);
                    PrintUsageAndExit(1);
                }
                return argv[++i];
            };

            if (arg == "--help" || arg == "-h")
                PrintUsageAndExit(0);
            else if (arg == "--corpus")
                opts.corpusPath = wantValue("--corpus");
            else if (arg == "--out-dir")
                opts.outDir = wantValue("--out-dir");
            else if (arg == "--split-fraction")
            {
                if (!ParseDouble(wantValue("--split-fraction"), opts.splitFraction))
                {
                    std::fprintf(stderr,
                                 "--split-fraction expects a number in [0, 1]\n");
                    PrintUsageAndExit(1);
                }
            }
            else if (arg == "--mode")
            {
                std::string_view m = wantValue("--mode");
                if (m == "all")     opts.mode = Options::Mode::All;
                else if (m == "dev")     opts.mode = Options::Mode::Dev;
                else if (m == "heldout") opts.mode = Options::Mode::HeldOut;
                else
                {
                    std::fprintf(stderr,
                                 "--mode must be one of: all, dev, heldout\n");
                    PrintUsageAndExit(1);
                }
            }
            else
            {
                std::fprintf(stderr, "Unknown argument: %.*s\n",
                             static_cast<int>(arg.size()), arg.data());
                PrintUsageAndExit(1);
            }
        }

        if (opts.corpusPath.empty())
        {
            std::fprintf(stderr, "--corpus is required\n");
            PrintUsageAndExit(1);
        }
        if (opts.splitFraction < 0.0 || opts.splitFraction > 1.0)
        {
            std::fprintf(stderr, "--split-fraction must be in [0, 1]\n");
            PrintUsageAndExit(1);
        }
        return opts;
    }

    std::string ReadFile(std::string const& path)
    {
        std::ifstream f(path, std::ios::binary);
        if (!f)
            return {};
        std::stringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }

    bool WriteFile(std::filesystem::path const& path, std::string const& content)
    {
        std::ofstream f(path, std::ios::binary);
        if (!f)
            return false;
        f.write(content.data(), static_cast<std::streamsize>(content.size()));
        return f.good();
    }

    void PrintSummary(char const* sliceName,
                      Road::Validation::ValidationResult const& r)
    {
        Road::Validation::ConfusionMatrix const& m = r.overall;
        std::printf("\n== %s slice ==\n", sliceName);
        std::printf("  scored:           %llu\n",
                    static_cast<unsigned long long>(m.ScoredCount()));
        std::printf("  ambiguous_skip:   %llu\n",
                    static_cast<unsigned long long>(m.ambiguousSkipped));
        std::printf("  true_positive:    %llu\n",
                    static_cast<unsigned long long>(m.truePositive));
        std::printf("  false_positive:   %llu\n",
                    static_cast<unsigned long long>(m.falsePositive));
        std::printf("  true_negative:    %llu\n",
                    static_cast<unsigned long long>(m.trueNegative));
        std::printf("  false_negative:   %llu\n",
                    static_cast<unsigned long long>(m.falseNegative));
        std::printf("  precision:        %.6f\n", m.Precision());
        std::printf("  recall:           %.6f\n", m.Recall());
        std::printf("  f1:               %.6f\n", m.F1());
        std::printf("  accuracy:         %.6f\n", m.Accuracy());

        if (!r.perZone.empty())
        {
            std::printf("  per-zone (mapId / tp / fp / tn / fn / precision):\n");
            for (auto const& z : r.perZone)
            {
                std::printf("    %u : %llu / %llu / %llu / %llu / %.4f\n",
                            z.mapId,
                            static_cast<unsigned long long>(z.matrix.truePositive),
                            static_cast<unsigned long long>(z.matrix.falsePositive),
                            static_cast<unsigned long long>(z.matrix.trueNegative),
                            static_cast<unsigned long long>(z.matrix.falseNegative),
                            z.matrix.Precision());
            }
        }
    }

    // Quality-bar check per design doc §3.5: precision ≥ 0.99, recall ≥ 0.95.
    // Returns true iff the slice clears the bar (or has no positive ground
    // truths, in which case nothing to score).
    bool MeetsQualityBar(Road::Validation::ConfusionMatrix const& m)
    {
        double p = m.Precision();
        double r = m.Recall();
        // NaN comparisons are false. If precision is NaN it means no positive
        // predictions — also a failure case (recall would be 0 if any
        // positives exist).
        return (m.ScoredCount() > 0) && (p >= 0.99) && (r >= 0.95);
    }

    int Run(int argc, char** argv)
    {
        Options opts = ParseArgs(argc, argv);

        std::string csv = ReadFile(opts.corpusPath);
        if (csv.empty())
        {
            std::fprintf(stderr, "Could not read corpus file: %s\n",
                         opts.corpusPath.c_str());
            return 2;
        }

        std::vector<std::string> warnings;
        auto entries = Road::Validation::ParseCorpusCsv(csv, &warnings);
        for (std::string const& w : warnings)
            std::fprintf(stderr, "[corpus warning] %s\n", w.c_str());

        if (entries.empty())
        {
            std::fprintf(stderr, "Corpus parsed to 0 entries — aborting.\n");
            return 2;
        }

        std::printf("Loaded %zu corpus entries from %s\n",
                    entries.size(), opts.corpusPath.c_str());

        // Slice selection.
        std::span<Road::Validation::CorpusEntry const> activeSlice;
        std::string sliceLabel;
        Road::Validation::HeldOutSplit split;

        switch (opts.mode)
        {
            case Options::Mode::All:
                activeSlice = entries;
                sliceLabel  = "full-corpus";
                std::printf("Scoring full corpus (no held-out split).\n");
                break;
            case Options::Mode::Dev:
                split = Road::Validation::DeterministicHeldOutSplit(
                    entries, opts.splitFraction);
                activeSlice = split.dev;
                sliceLabel  = "dev";
                std::printf("Scoring DEV slice (%zu of %zu entries; "
                            "held-out fraction = %.3f)\n",
                            split.dev.size(), entries.size(),
                            opts.splitFraction);
                break;
            case Options::Mode::HeldOut:
                split = Road::Validation::DeterministicHeldOutSplit(
                    entries, opts.splitFraction);
                activeSlice = split.heldOut;
                sliceLabel  = "heldout";
                std::printf("Scoring HELD-OUT slice (%zu of %zu entries; "
                            "held-out fraction = %.3f)\n",
                            split.heldOut.size(), entries.size(),
                            opts.splitFraction);
                break;
        }

        auto result = Road::Validation::ScoreCorpus(activeSlice);
        PrintSummary(sliceLabel.c_str(), result);

        // Write CSVs.
        std::filesystem::create_directories(opts.outDir);
        std::filesystem::path metricsPath =
            std::filesystem::path(opts.outDir) / ("metrics_" + sliceLabel + ".csv");
        std::filesystem::path disagreementsPath =
            std::filesystem::path(opts.outDir) /
            ("disagreements_" + sliceLabel + ".csv");

        if (!WriteFile(metricsPath, Road::Validation::WriteMetricsCsv(result)))
        {
            std::fprintf(stderr, "Failed to write %s\n",
                         metricsPath.string().c_str());
            return 3;
        }
        if (!WriteFile(disagreementsPath,
                       Road::Validation::WriteDisagreementsCsv(activeSlice, result)))
        {
            std::fprintf(stderr, "Failed to write %s\n",
                         disagreementsPath.string().c_str());
            return 3;
        }

        std::printf("\nWrote: %s\nWrote: %s\n",
                    metricsPath.string().c_str(),
                    disagreementsPath.string().c_str());

        if (MeetsQualityBar(result.overall))
            std::printf("\n*** Quality bar PASSED (P >= 0.99, R >= 0.95) ***\n");
        else
            std::printf("\n--- Quality bar NOT met yet ---\n");

        return 0;
    }
}

int main(int argc, char** argv)
{
    try
    {
        return Run(argc, argv);
    }
    catch (std::exception const& e)
    {
        std::fprintf(stderr, "Unhandled exception: %s\n", e.what());
        return 4;
    }
    catch (...)
    {
        std::fprintf(stderr, "Unhandled non-std exception\n");
        return 4;
    }
}
