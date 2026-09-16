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

#ifndef Scenario_h__
#define Scenario_h__

#include "CriteriaHandler.h"
#include <map>
#include <unordered_map>
#include <unordered_set>

class Map;
struct ScenarioData;
struct ScenarioEntry;
struct ScenarioStepEntry;

namespace WorldPackets
{
    namespace Achievement
    {
        struct CriteriaProgress;
    }

    namespace Scenario
    {
        struct BonusObjectiveData;
        class ScenarioState;
    }
}

enum ScenarioStepState
{
    SCENARIO_STEP_INVALID       = 0,
    SCENARIO_STEP_NOT_STARTED   = 1,
    SCENARIO_STEP_IN_PROGRESS   = 2,
    SCENARIO_STEP_DONE          = 3
};

// SMSG_SCENARIO_VACATE Reason (2 bits). Retail 12.1 sends Completed together with SMSG_SCENARIO_COMPLETED when an
// open-world scenario ends, and Left when a player leaves the scenario's map or area or the scenario is stopped.
enum class ScenarioVacateReason : uint8
{
    Completed   = 0,
    Left        = 1
};

// A full SMSG_SCENARIO_STATE (join, final refresh) always carries this many spell entries; step changes carry
// only the new step's spells.
constexpr std::size_t SCENARIO_STATE_SPELL_SLOTS = 4;

class TC_GAME_API Scenario : public CriteriaHandler
{
    public:
        Scenario(Map* map, ScenarioData const* scenarioData);
        virtual ~Scenario();

        void Reset() override;
        void SetStep(ScenarioStepEntry const* step);

        virtual void CompleteStep(ScenarioStepEntry const* step);
        virtual void CompleteScenario();

        virtual void OnPlayerEnter(Player* player);
        virtual void OnPlayerExit(Player* player);
        virtual void Update(uint32 /*diff*/) { }

        bool IsComplete() const;
        bool IsCompletedStep(ScenarioStepEntry const* step);
        void SetStepState(ScenarioStepEntry const* step, ScenarioStepState state) { _stepStates[step] = state; }
        ScenarioEntry const* GetEntry() const;
        ScenarioStepState GetStepState(ScenarioStepEntry const* step) const;
        ScenarioStepEntry const* GetStep() const { return _currentstep; }
        ScenarioStepEntry const* GetFirstStep() const;
        ScenarioStepEntry const* GetLastStep() const;

        void SendScenarioState(Player const* player) const;
        void SendBootPlayer(Player const* player, ScenarioVacateReason reason = ScenarioVacateReason::Left) const;

        ObjectGuid const& GetGUID() const { return _guid; }
        bool HasPlayer(ObjectGuid const& guid) const { return _players.contains(guid); }

    protected:
        Map const* _map;
        GuidUnorderedSet _players;

        void SendCriteriaUpdate(Criteria const* criteria, CriteriaProgress const* progress, Seconds timeElapsed, bool timedCompleted) const override;
        void SendCriteriaProgressRemoved(uint32 /*criteriaId*/) override { }

        bool CanUpdateCriteriaTree(Criteria const* criteria, CriteriaTree const* tree, Player* referencePlayer) const override;
        bool CanCompleteCriteriaTree(CriteriaTree const* tree) override;
        void CompletedCriteriaTree(CriteriaTree const* tree, Player* referencePlayer) override;
        void AfterCriteriaTreeUpdate(CriteriaTree const* /*tree*/, Player* /*referencePlayer*/) override { }

        void DoForAllPlayers(std::function<void(Player*)> const& worker) const;
        void SendPacket(WorldPacket const* data) const override;

        void SendAllData(Player const* /*receiver*/) const override { }

        enum class StateSpells
        {
            None,       // no spell entries
            StepSpells, // the current step's spells (sent when the step changes)
            Full        // SCENARIO_STATE_SPELL_SLOTS entries (join, final refresh)
        };

        void BuildScenarioStateFor(Player const* player, WorldPackets::Scenario::ScenarioState* scenarioState, StateSpells spells) const;
        void SendFullStateToAllPlayers(bool forceIncomplete) const;

        std::vector<WorldPackets::Scenario::BonusObjectiveData> GetBonusObjectivesData() const;
        std::vector<WorldPackets::Achievement::CriteriaProgress> GetCriteriasProgressFor(Player const* player) const;

        CriteriaList const& GetCriteriaByType(CriteriaType type, uint32 asset) const override;
        ScenarioData const* _data;

    private:
        Seconds GetTimeFromCreate(uint32 criteriaId, time_t fallback) const;

        ObjectGuid const _guid;
        ScenarioStepEntry const* _currentstep;
        std::map<ScenarioStepEntry const*, ScenarioStepState> _stepStates;

        // when each criteria of this scenario first gained progress: retail sends the time since then as both
        // TimeFromStart and TimeFromCreate of every scenario criteria progress
        mutable std::unordered_map<uint32, time_t> _criteriaCreateTime;
};

#endif // Scenario_h__
