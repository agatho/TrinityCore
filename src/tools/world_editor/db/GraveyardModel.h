/*
 * GraveyardModel - in-memory edit log for `world_safe_locs` rows.
 *
 * Mirrors AreatriggerModel: per-row Insert/Update/Delete changelog
 * with baseline + current vectors.  GraveyardCommitDialog turns the
 * changelog into SQL inside a single transaction.
 *
 * Identity: PK is `world_safe_locs.ID` alone (auto-increment).  We
 * pre-reserve IDs from MAX+1 the same way the rest of the editor does,
 * so post-commit refresh finds new rows at the same ID the operator saw.
 */

#pragma once

#include "../core/model/WorldEntities.h"

#include <cstdint>
#include <vector>

namespace world_editor::db
{

enum class GraveyardChangeKind : uint8_t
{
    None   = 0,
    Insert = 1,
    Update = 2,
    Delete = 3,
};

struct GraveyardChangeRecord
{
    GraveyardChangeKind kind = GraveyardChangeKind::None;
    render::Graveyard   before; // valid for Update + Delete
    render::Graveyard   after;  // valid for Insert + Update
};

class GraveyardModel
{
public:
    void setBaseline(std::vector<render::Graveyard> rows);

    [[nodiscard]] std::vector<render::Graveyard>     const& current() const noexcept { return m_current; }
    [[nodiscard]] std::vector<GraveyardChangeRecord> const& changes() const noexcept { return m_changes; }
    [[nodiscard]] size_t pendingCount() const;

    int  addNew(render::Graveyard row);
    bool replaceRow(int index, render::Graveyard const& newRow);
    bool removeRow(int index);

    void revertAll();
    void acceptCommit(std::vector<render::Graveyard> committed);

    struct StateSnapshot
    {
        std::vector<render::Graveyard>     baseline;
        std::vector<render::Graveyard>     current;
        std::vector<GraveyardChangeRecord> changes;
    };
    [[nodiscard]] StateSnapshot captureState() const;
    void restoreState(StateSnapshot const& s);

private:
    void recordUpdate(int index);

    std::vector<render::Graveyard>      m_baseline;
    std::vector<render::Graveyard>      m_current;
    std::vector<GraveyardChangeRecord>  m_changes;
};

} // namespace world_editor::db
