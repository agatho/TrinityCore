#include "GameEventEditDialog.h"

#include "../db/MySqlClient.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>

#include <cstdio>
#include <limits>

namespace world_editor::app
{

GameEventEditDialog::GameEventEditDialog(db::MySqlClient* dbClient,
                                         QString const& worldDbName,
                                         QWidget* parent)
    : QDialog(parent), m_db(dbClient), m_worldDb(worldDbName)
{
    setWindowTitle(tr("Game events"));
    setModal(true);
    resize(1100, 640);

    // --- Left pane: game_event listing + new/delete toolbar. ----------
    auto* leftPanel = new QVBoxLayout;
    leftPanel->addWidget(new QLabel(tr("game_event")));
    m_eventTable = new QTableWidget(this);
    m_eventTable->setColumnCount(10);
    m_eventTable->setHorizontalHeaderLabels({
        tr("eventEntry"), tr("start_time"), tr("end_time"),
        tr("occurence"), tr("length"), tr("holiday"), tr("holidayStage"),
        tr("description"), tr("world_event"), tr("announce") });
    m_eventTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_eventTable->horizontalHeader()->setStretchLastSection(true);
    m_eventTable->verticalHeader()->setVisible(false);
    m_eventTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_eventTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_eventTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_eventTable->setSortingEnabled(true);
    leftPanel->addWidget(m_eventTable, 1);

    auto* leftToolbar = new QHBoxLayout;
    m_newBtn    = new QPushButton(tr("New event"), this);
    m_deleteBtn = new QPushButton(tr("Delete event"), this);
    m_deleteBtn->setEnabled(false);
    leftToolbar->addWidget(m_newBtn);
    leftToolbar->addWidget(m_deleteBtn);
    leftToolbar->addStretch(1);
    leftPanel->addLayout(leftToolbar);

    // --- Right pane: tabs for Properties / Creatures / GameObjects. ---
    m_tabs = new QTabWidget(this);

    // Properties tab.
    {
        auto* page = new QWidget(this);
        auto* lay  = new QVBoxLayout(page);

        auto addLine = [&](char const* lbl, QWidget* w) {
            auto* row = new QHBoxLayout;
            auto* l   = new QLabel(tr(lbl), this);
            l->setMinimumWidth(110);
            row->addWidget(l);
            row->addWidget(w, 1);
            lay->addLayout(row);
        };

        m_startTimeEdit = new QLineEdit(this);
        m_startTimeEdit->setPlaceholderText(tr("YYYY-MM-DD HH:MM:SS or empty for NULL"));
        addLine("start_time:", m_startTimeEdit);

        m_endTimeEdit = new QLineEdit(this);
        m_endTimeEdit->setPlaceholderText(tr("YYYY-MM-DD HH:MM:SS or empty for NULL"));
        addLine("end_time:", m_endTimeEdit);

        m_occurenceSpin = new QSpinBox(this);
        m_occurenceSpin->setRange(0, std::numeric_limits<int>::max());
        addLine("occurence (min):", m_occurenceSpin);

        m_lengthSpin = new QSpinBox(this);
        m_lengthSpin->setRange(0, std::numeric_limits<int>::max());
        addLine("length (min):", m_lengthSpin);

        m_holidaySpin = new QSpinBox(this);
        m_holidaySpin->setRange(0, std::numeric_limits<int>::max());
        addLine("holiday:", m_holidaySpin);

        m_holidayStageSpin = new QSpinBox(this);
        m_holidayStageSpin->setRange(0, 255);
        addLine("holidayStage:", m_holidayStageSpin);

        m_descriptionEdit = new QPlainTextEdit(this);
        m_descriptionEdit->setMaximumHeight(80);
        addLine("description:", m_descriptionEdit);

        m_worldEventSpin = new QSpinBox(this);
        m_worldEventSpin->setRange(0, 255);
        addLine("world_event:", m_worldEventSpin);

        m_announceSpin = new QSpinBox(this);
        m_announceSpin->setRange(0, 255);
        addLine("announce:", m_announceSpin);

        m_saveBtn = new QPushButton(tr("Save"), this);
        m_saveBtn->setEnabled(false);
        auto* saveRow = new QHBoxLayout;
        saveRow->addStretch(1);
        saveRow->addWidget(m_saveBtn);
        lay->addLayout(saveRow);
        lay->addStretch(1);

        m_tabs->addTab(page, tr("Properties"));
    }

    // Creatures-linked tab.
    {
        auto* page = new QWidget(this);
        auto* lay  = new QVBoxLayout(page);

        auto* toolbar = new QHBoxLayout;
        m_addCreatureBtn = new QPushButton(tr("Add creature link..."), this);
        m_rmCreatureBtn  = new QPushButton(tr("Remove link"), this);
        m_addCreatureBtn->setEnabled(false);
        m_rmCreatureBtn ->setEnabled(false);
        toolbar->addWidget(m_addCreatureBtn);
        toolbar->addWidget(m_rmCreatureBtn);
        toolbar->addStretch(1);
        lay->addLayout(toolbar);

        m_creatureTable = new QTableWidget(this);
        m_creatureTable->setColumnCount(4);
        m_creatureTable->setHorizontalHeaderLabels(
            { tr("guid"), tr("eventEntry"), tr("creature.id1"), tr("creature.map") });
        m_creatureTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        m_creatureTable->verticalHeader()->setVisible(false);
        m_creatureTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_creatureTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_creatureTable->setSelectionMode(QAbstractItemView::SingleSelection);
        lay->addWidget(m_creatureTable, 1);

        m_tabs->addTab(page, tr("Creatures linked"));
    }

    // GameObjects-linked tab.
    {
        auto* page = new QWidget(this);
        auto* lay  = new QVBoxLayout(page);

        auto* toolbar = new QHBoxLayout;
        m_addGoBtn = new QPushButton(tr("Add GO link..."), this);
        m_rmGoBtn  = new QPushButton(tr("Remove link"), this);
        m_addGoBtn->setEnabled(false);
        m_rmGoBtn ->setEnabled(false);
        toolbar->addWidget(m_addGoBtn);
        toolbar->addWidget(m_rmGoBtn);
        toolbar->addStretch(1);
        lay->addLayout(toolbar);

        m_gameObjectTable = new QTableWidget(this);
        m_gameObjectTable->setColumnCount(4);
        m_gameObjectTable->setHorizontalHeaderLabels(
            { tr("guid"), tr("eventEntry"), tr("gameobject.id"), tr("gameobject.map") });
        m_gameObjectTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        m_gameObjectTable->verticalHeader()->setVisible(false);
        m_gameObjectTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_gameObjectTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_gameObjectTable->setSelectionMode(QAbstractItemView::SingleSelection);
        lay->addWidget(m_gameObjectTable, 1);

        m_tabs->addTab(page, tr("GameObjects linked"));
    }

    auto* split = new QHBoxLayout;
    auto* leftWrapper = new QWidget(this);
    leftWrapper->setLayout(leftPanel);
    split->addWidget(leftWrapper, 2);
    split->addWidget(m_tabs, 3);

    m_statusLabel = new QLabel(tr("loading..."), this);
    auto* outer = new QVBoxLayout(this);
    outer->addLayout(split, 1);
    outer->addWidget(m_statusLabel);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    outer->addWidget(buttons);

    // Wire signals.
    connect(m_eventTable, &QTableWidget::itemSelectionChanged,
            this, &GameEventEditDialog::onEventRowChanged);
    connect(m_newBtn,    &QPushButton::clicked, this, &GameEventEditDialog::onNewEvent);
    connect(m_deleteBtn, &QPushButton::clicked, this, &GameEventEditDialog::onDeleteEvent);
    connect(m_saveBtn,   &QPushButton::clicked, this, &GameEventEditDialog::onSaveProperties);
    connect(m_addCreatureBtn, &QPushButton::clicked, this, &GameEventEditDialog::onAddCreatureLink);
    connect(m_rmCreatureBtn,  &QPushButton::clicked, this, &GameEventEditDialog::onRemoveCreatureLink);
    connect(m_addGoBtn,       &QPushButton::clicked, this, &GameEventEditDialog::onAddGameObjectLink);
    connect(m_rmGoBtn,        &QPushButton::clicked, this, &GameEventEditDialog::onRemoveGameObjectLink);

    QTimer::singleShot(0, this, [this] { loadEvents(); });
}

void GameEventEditDialog::loadEvents(int preserveEntry)
{
    if (!m_db || !m_db->isConnected())
    {
        m_statusLabel->setText(tr("not connected"));
        return;
    }
    QApplication::setOverrideCursor(Qt::WaitCursor);

    char sql[768];
    std::snprintf(sql, sizeof(sql),
        "SELECT eventEntry, "
        "       COALESCE(CAST(start_time AS CHAR), ''), "
        "       COALESCE(CAST(end_time   AS CHAR), ''), "
        "       occurence, length, holiday, holidayStage, "
        "       COALESCE(description, ''), world_event, COALESCE(announce, 2) "
        "FROM %s.game_event ORDER BY eventEntry",
        m_worldDb.toStdString().c_str());
    db::QueryResult res;
    auto const err = m_db->query(sql, res);
    QApplication::restoreOverrideCursor();
    if (!err.ok())
    {
        m_statusLabel->setText(tr("query failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }

    m_eventTable->setSortingEnabled(false);
    m_eventTable->setRowCount(int(res.rowCount()));
    int rowToReselect = -1;
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        int      const entry   = int(res.asInt64(r, 0).value_or(0));
        QString  const startTs = QString::fromStdString(res.cell(r, 1));
        QString  const endTs   = QString::fromStdString(res.cell(r, 2));
        uint64_t const occ     = res.asUInt64(r, 3).value_or(0);
        uint64_t const len     = res.asUInt64(r, 4).value_or(0);
        uint64_t const hol     = res.asUInt64(r, 5).value_or(0);
        uint64_t const hstage  = res.asUInt64(r, 6).value_or(0);
        QString  const desc    = QString::fromStdString(res.cell(r, 7));
        uint64_t const we      = res.asUInt64(r, 8).value_or(0);
        uint64_t const ann     = res.asUInt64(r, 9).value_or(2);

        auto* entryItem = new QTableWidgetItem;
        entryItem->setData(Qt::DisplayRole, entry);
        entryItem->setData(Qt::UserRole,    entry);
        m_eventTable->setItem(int(r), 0, entryItem);
        m_eventTable->setItem(int(r), 1, new QTableWidgetItem(startTs));
        m_eventTable->setItem(int(r), 2, new QTableWidgetItem(endTs));
        m_eventTable->setItem(int(r), 3, new QTableWidgetItem(QString::number(occ)));
        m_eventTable->setItem(int(r), 4, new QTableWidgetItem(QString::number(len)));
        m_eventTable->setItem(int(r), 5, new QTableWidgetItem(QString::number(hol)));
        m_eventTable->setItem(int(r), 6, new QTableWidgetItem(QString::number(hstage)));
        m_eventTable->setItem(int(r), 7, new QTableWidgetItem(desc));
        m_eventTable->setItem(int(r), 8, new QTableWidgetItem(QString::number(we)));
        m_eventTable->setItem(int(r), 9, new QTableWidgetItem(QString::number(ann)));

        if (preserveEntry >= 0 && entry == preserveEntry)
            rowToReselect = int(r);
    }
    m_eventTable->setSortingEnabled(true);

    if (rowToReselect >= 0)
        m_eventTable->selectRow(rowToReselect);
    m_statusLabel->setText(tr("events=%1").arg(res.rowCount()));
}

int GameEventEditDialog::selectedEventEntry() const
{
    if (!m_eventTable) return -1;
    int const row = m_eventTable->currentRow();
    if (row < 0) return -1;
    QTableWidgetItem* it = m_eventTable->item(row, 0);
    if (!it) return -1;
    return it->data(Qt::UserRole).toInt();
}

void GameEventEditDialog::onEventRowChanged()
{
    int const entry = selectedEventEntry();
    bool const haveSelection = (entry >= 0);
    m_deleteBtn      ->setEnabled(haveSelection);
    m_saveBtn        ->setEnabled(haveSelection);
    m_addCreatureBtn ->setEnabled(haveSelection);
    m_addGoBtn       ->setEnabled(haveSelection);
    m_rmCreatureBtn  ->setEnabled(false);
    m_rmGoBtn        ->setEnabled(false);

    if (!haveSelection)
        return;

    loadPropertiesForSelected();
    loadCreatureLinks  (entry);
    loadGameObjectLinks(entry);
}

void GameEventEditDialog::loadPropertiesForSelected()
{
    int const row = m_eventTable->currentRow();
    if (row < 0) return;

    m_startTimeEdit   ->setText(m_eventTable->item(row, 1)->text());
    m_endTimeEdit     ->setText(m_eventTable->item(row, 2)->text());
    m_occurenceSpin   ->setValue(m_eventTable->item(row, 3)->text().toInt());
    m_lengthSpin      ->setValue(m_eventTable->item(row, 4)->text().toInt());
    m_holidaySpin     ->setValue(m_eventTable->item(row, 5)->text().toInt());
    m_holidayStageSpin->setValue(m_eventTable->item(row, 6)->text().toInt());
    m_descriptionEdit ->setPlainText(m_eventTable->item(row, 7)->text());
    m_worldEventSpin  ->setValue(m_eventTable->item(row, 8)->text().toInt());
    m_announceSpin    ->setValue(m_eventTable->item(row, 9)->text().toInt());
}

void GameEventEditDialog::loadCreatureLinks(int eventEntry)
{
    if (!m_db || !m_db->isConnected()) return;
    char sql[768];
    std::snprintf(sql, sizeof(sql),
        "SELECT g.guid, g.eventEntry, COALESCE(c.id1, 0), COALESCE(c.map, 0) "
        "FROM %s.game_event_creature g "
        "LEFT JOIN %s.creature c ON c.guid = g.guid "
        "WHERE g.eventEntry = %d "
        "ORDER BY g.guid",
        m_worldDb.toStdString().c_str(), m_worldDb.toStdString().c_str(), eventEntry);
    db::QueryResult res;
    if (!m_db->query(sql, res).ok())
        return;
    m_creatureTable->setRowCount(int(res.rowCount()));
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        uint64_t const guid   = res.asUInt64(r, 0).value_or(0);
        int64_t  const evt    = res.asInt64 (r, 1).value_or(0);
        uint64_t const id1    = res.asUInt64(r, 2).value_or(0);
        uint64_t const map    = res.asUInt64(r, 3).value_or(0);
        m_creatureTable->setItem(int(r), 0, new QTableWidgetItem(QString::number(guid)));
        m_creatureTable->setItem(int(r), 1, new QTableWidgetItem(QString::number(evt)));
        m_creatureTable->setItem(int(r), 2, new QTableWidgetItem(QString::number(id1)));
        m_creatureTable->setItem(int(r), 3, new QTableWidgetItem(QString::number(map)));
    }
    // Enable remove iff there are rows + an event is selected.
    connect(m_creatureTable, &QTableWidget::itemSelectionChanged, this, [this] {
        m_rmCreatureBtn->setEnabled(m_creatureTable->currentRow() >= 0 && selectedEventEntry() >= 0);
    }, Qt::UniqueConnection);
}

void GameEventEditDialog::loadGameObjectLinks(int eventEntry)
{
    if (!m_db || !m_db->isConnected()) return;
    char sql[768];
    std::snprintf(sql, sizeof(sql),
        "SELECT g.guid, g.eventEntry, COALESCE(o.id, 0), COALESCE(o.map, 0) "
        "FROM %s.game_event_gameobject g "
        "LEFT JOIN %s.gameobject o ON o.guid = g.guid "
        "WHERE g.eventEntry = %d "
        "ORDER BY g.guid",
        m_worldDb.toStdString().c_str(), m_worldDb.toStdString().c_str(), eventEntry);
    db::QueryResult res;
    if (!m_db->query(sql, res).ok())
        return;
    m_gameObjectTable->setRowCount(int(res.rowCount()));
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        uint64_t const guid = res.asUInt64(r, 0).value_or(0);
        int64_t  const evt  = res.asInt64 (r, 1).value_or(0);
        uint64_t const id   = res.asUInt64(r, 2).value_or(0);
        uint64_t const map  = res.asUInt64(r, 3).value_or(0);
        m_gameObjectTable->setItem(int(r), 0, new QTableWidgetItem(QString::number(guid)));
        m_gameObjectTable->setItem(int(r), 1, new QTableWidgetItem(QString::number(evt)));
        m_gameObjectTable->setItem(int(r), 2, new QTableWidgetItem(QString::number(id)));
        m_gameObjectTable->setItem(int(r), 3, new QTableWidgetItem(QString::number(map)));
    }
    connect(m_gameObjectTable, &QTableWidget::itemSelectionChanged, this, [this] {
        m_rmGoBtn->setEnabled(m_gameObjectTable->currentRow() >= 0 && selectedEventEntry() >= 0);
    }, Qt::UniqueConnection);
}

bool GameEventEditDialog::runInTransaction(QString const& sql, QString const& description, uint64_t* affectedOut)
{
    if (!m_db || !m_db->isConnected())
    {
        QMessageBox::warning(this, tr("Not connected"), tr("World DB not open."));
        return false;
    }
    auto err = m_db->exec("START TRANSACTION");
    if (!err.ok())
    {
        QMessageBox::critical(this, tr("Transaction failed"),
            tr("BEGIN failed: %1").arg(QString::fromStdString(err.message)));
        return false;
    }
    uint64_t affected = 0;
    err = m_db->exec(sql.toStdString(), &affected);
    if (!err.ok())
    {
        (void)m_db->exec("ROLLBACK");
        QMessageBox::critical(this, tr("DML failed"),
            tr("%1\n\n%2").arg(description).arg(QString::fromStdString(err.message)));
        return false;
    }
    err = m_db->exec("COMMIT");
    if (!err.ok())
    {
        (void)m_db->exec("ROLLBACK");
        QMessageBox::critical(this, tr("Commit failed"),
            tr("%1\n\n%2").arg(description).arg(QString::fromStdString(err.message)));
        return false;
    }
    if (affectedOut)
        *affectedOut = affected;
    m_statusLabel->setText(tr("%1 (affected=%2)").arg(description).arg(qulonglong(affected)));
    return true;
}

void GameEventEditDialog::onNewEvent()
{
    if (!m_db || !m_db->isConnected()) return;

    // game_event.eventEntry is tinyint UNSIGNED (max 255).  Pick the lowest
    // unused slot starting from 1.
    char sql[256];
    std::snprintf(sql, sizeof(sql),
        "SELECT eventEntry FROM %s.game_event ORDER BY eventEntry",
        m_worldDb.toStdString().c_str());
    db::QueryResult res;
    if (!m_db->query(sql, res).ok())
        return;
    int candidate = 1;
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        int const e = int(res.asInt64(r, 0).value_or(0));
        if (e == candidate) ++candidate;
        else if (e > candidate) break;
    }
    if (candidate > 255)
    {
        QMessageBox::warning(this, tr("Out of slots"),
            tr("All 255 eventEntry slots are occupied. Delete one first."));
        return;
    }
    bool okPick = false;
    int const entry = QInputDialog::getInt(this, tr("New game_event"),
        tr("eventEntry (1..255):"), candidate, 1, 255, 1, &okPick);
    if (!okPick) return;

    // Sane defaults per spec: occurence=525600 (one year), length=10080 (one
    // week), holiday=0, world_event=0, announce=0.
    char ins[512];
    std::snprintf(ins, sizeof(ins),
        "INSERT INTO %s.game_event "
        "(eventEntry, start_time, end_time, occurence, length, holiday, "
        " holidayStage, description, world_event, announce) "
        "VALUES (%d, NULL, NULL, 525600, 10080, 0, 0, 'new event %d', 0, 0)",
        m_worldDb.toStdString().c_str(), entry, entry);
    if (runInTransaction(QString::fromUtf8(ins), tr("INSERT game_event eventEntry=%1").arg(entry)))
        loadEvents(entry);
}

void GameEventEditDialog::onDeleteEvent()
{
    int const entry = selectedEventEntry();
    if (entry < 0) return;

    // Warn if any link rows exist - operator may want to clean those up
    // first, but the DELETE itself is allowed (link rows become orphans).
    char cntSql[384];
    std::snprintf(cntSql, sizeof(cntSql),
        "SELECT "
        "  (SELECT COUNT(*) FROM %s.game_event_creature   WHERE eventEntry=%d), "
        "  (SELECT COUNT(*) FROM %s.game_event_gameobject WHERE eventEntry=%d)",
        m_worldDb.toStdString().c_str(), entry,
        m_worldDb.toStdString().c_str(), entry);
    db::QueryResult cntRes;
    uint64_t cCnt = 0, gCnt = 0;
    if (m_db->query(cntSql, cntRes).ok() && cntRes.rowCount() > 0)
    {
        cCnt = cntRes.asUInt64(0, 0).value_or(0);
        gCnt = cntRes.asUInt64(0, 1).value_or(0);
    }
    QString warning = tr("Delete game_event eventEntry=%1?").arg(entry);
    if (cCnt > 0 || gCnt > 0)
    {
        warning += tr("\n\nWARNING: %1 game_event_creature row(s) and %2 game_event_gameobject row(s) "
                     "are linked to this event.  These link rows will be orphaned (the spawns "
                     "themselves are untouched).")
            .arg(cCnt).arg(gCnt);
    }
    if (QMessageBox::question(this, tr("Delete event"), warning,
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    char del[256];
    std::snprintf(del, sizeof(del),
        "DELETE FROM %s.game_event WHERE eventEntry=%d",
        m_worldDb.toStdString().c_str(), entry);
    if (runInTransaction(QString::fromUtf8(del), tr("DELETE game_event eventEntry=%1").arg(entry)))
        loadEvents();
}

void GameEventEditDialog::onSaveProperties()
{
    int const entry = selectedEventEntry();
    if (entry < 0) return;

    // Build SET clause.  Empty start_time / end_time strings collapse to NULL
    // so the operator can clear the timestamp by emptying the field.
    QString const escDesc = QString::fromStdString(
        m_db->escapeString(m_descriptionEdit->toPlainText().toStdString()));
    auto tsClause = [&](char const* col, QLineEdit* edit) -> QString {
        QString const t = edit->text().trimmed();
        if (t.isEmpty())
            return QStringLiteral("%1=NULL").arg(QLatin1String(col));
        QString const esc = QString::fromStdString(m_db->escapeString(t.toStdString()));
        return QStringLiteral("%1='%2'").arg(QLatin1String(col), esc);
    };

    QString sql = QStringLiteral(
        "UPDATE %1.game_event SET %2, %3, occurence=%4, length=%5, holiday=%6, "
        "holidayStage=%7, description='%8', world_event=%9, announce=%10 WHERE eventEntry=%11")
        .arg(m_worldDb)
        .arg(tsClause("start_time", m_startTimeEdit))
        .arg(tsClause("end_time",   m_endTimeEdit))
        .arg(m_occurenceSpin->value())
        .arg(m_lengthSpin->value())
        .arg(m_holidaySpin->value())
        .arg(m_holidayStageSpin->value())
        .arg(escDesc)
        .arg(m_worldEventSpin->value())
        .arg(m_announceSpin->value())
        .arg(entry);

    if (runInTransaction(sql, tr("UPDATE game_event eventEntry=%1").arg(entry)))
        loadEvents(entry);
}

void GameEventEditDialog::onAddCreatureLink()
{
    int const entry = selectedEventEntry();
    if (entry < 0) return;
    bool okPick = false;
    qlonglong const guid = QInputDialog::getInt(this, tr("Add creature link"),
        tr("creature.guid:"), 0, 0, std::numeric_limits<int>::max(), 1, &okPick);
    if (!okPick) return;

    char ins[256];
    std::snprintf(ins, sizeof(ins),
        "INSERT INTO %s.game_event_creature (eventEntry, guid) VALUES (%d, %lld)",
        m_worldDb.toStdString().c_str(), entry, (long long)guid);
    if (runInTransaction(QString::fromUtf8(ins),
            tr("INSERT game_event_creature (event=%1, guid=%2)").arg(entry).arg(guid)))
        loadCreatureLinks(entry);
}

void GameEventEditDialog::onAddGameObjectLink()
{
    int const entry = selectedEventEntry();
    if (entry < 0) return;
    bool okPick = false;
    qlonglong const guid = QInputDialog::getInt(this, tr("Add GO link"),
        tr("gameobject.guid:"), 0, 0, std::numeric_limits<int>::max(), 1, &okPick);
    if (!okPick) return;

    char ins[256];
    std::snprintf(ins, sizeof(ins),
        "INSERT INTO %s.game_event_gameobject (eventEntry, guid) VALUES (%d, %lld)",
        m_worldDb.toStdString().c_str(), entry, (long long)guid);
    if (runInTransaction(QString::fromUtf8(ins),
            tr("INSERT game_event_gameobject (event=%1, guid=%2)").arg(entry).arg(guid)))
        loadGameObjectLinks(entry);
}

void GameEventEditDialog::onRemoveCreatureLink()
{
    int const entry = selectedEventEntry();
    int const row   = m_creatureTable->currentRow();
    if (entry < 0 || row < 0) return;
    qlonglong const guid = m_creatureTable->item(row, 0)->text().toLongLong();
    int       const ev   = m_creatureTable->item(row, 1)->text().toInt();

    char del[256];
    std::snprintf(del, sizeof(del),
        "DELETE FROM %s.game_event_creature WHERE guid=%lld AND eventEntry=%d",
        m_worldDb.toStdString().c_str(), (long long)guid, ev);
    if (runInTransaction(QString::fromUtf8(del),
            tr("DELETE game_event_creature (event=%1, guid=%2)").arg(ev).arg(guid)))
        loadCreatureLinks(entry);
}

void GameEventEditDialog::onRemoveGameObjectLink()
{
    int const entry = selectedEventEntry();
    int const row   = m_gameObjectTable->currentRow();
    if (entry < 0 || row < 0) return;
    qlonglong const guid = m_gameObjectTable->item(row, 0)->text().toLongLong();
    int       const ev   = m_gameObjectTable->item(row, 1)->text().toInt();

    char del[256];
    std::snprintf(del, sizeof(del),
        "DELETE FROM %s.game_event_gameobject WHERE guid=%lld AND eventEntry=%d",
        m_worldDb.toStdString().c_str(), (long long)guid, ev);
    if (runInTransaction(QString::fromUtf8(del),
            tr("DELETE game_event_gameobject (event=%1, guid=%2)").arg(ev).arg(guid)))
        loadGameObjectLinks(entry);
}

} // namespace world_editor::app
