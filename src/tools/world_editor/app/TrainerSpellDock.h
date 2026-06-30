/*
 * TrainerSpellDock - read-only panel showing the trainer offerings of
 * the currently-selected creature spawn.  Resolves
 *   creature.entry  -> creature_default_trainer.TrainerID  -> trainer.Id
 * then renders the trainer header (type, requirement) and the
 * trainer_spell rows (SpellID, ReqLevel, MoneyCost, ReqSkill/Rank,
 * prereq spell IDs).
 *
 * Falls back to creature.entry == trainer.Id when no row in
 * creature_default_trainer matches; some forks bind trainers directly
 * on the entry instead of going through the link table.
 *
 * Auto-populates from MainWindow::onSpawnClicked when the selected
 * creature template carries any of UNIT_NPC_FLAG_TRAINER (0x10) /
 * TRAINER_CLASS (0x20) / TRAINER_PROFESSION (0x40); cleared otherwise.
 */

#pragma once

#include <QWidget>

#include <cstdint>

class QLabel;
class QTableWidget;

namespace world_editor::db { class MySqlClient; }

namespace world_editor::app
{

class TrainerSpellDock final : public QWidget
{
    Q_OBJECT

public:
    explicit TrainerSpellDock(db::MySqlClient* dbClient,
                              QWidget* parent = nullptr);

    // Refresh the table to show the trainer offerings for this
    // creature template entry.  entry=0 clears.
    void setTrainerForCreature(uint32_t creatureEntry);
    void clear();
    // Late-bind the DB client (the dock is constructed before the
    // connection is established).  Pass nullptr to drop the binding.
    void setDbClient(db::MySqlClient* db) { m_db = db; }

private:
    db::MySqlClient* m_db;
    QLabel*          m_header = nullptr;
    QTableWidget*    m_table  = nullptr;
};

} // namespace world_editor::app
