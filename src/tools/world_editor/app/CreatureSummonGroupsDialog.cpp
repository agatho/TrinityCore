#include "CreatureSummonGroupsDialog.h"

#include "../db/MySqlClient.h"

#include <QApplication>
#include <QDebug>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <cstdio>
#include <limits>

namespace world_editor::app
{

namespace
{

// Column indices for the main table.  Kept centralized so Jump/Edit/Remove
// can read PK columns straight off the rendered cells without re-parsing.
enum Col : int
{
    COL_SUMMONER_ID   = 0,
    COL_SUMMONER_TYPE = 1,
    COL_GROUP_ID      = 2,
    COL_ENTRY         = 3,
    COL_X             = 4,
    COL_Y             = 5,
    COL_Z             = 6,
    COL_ORIENT        = 7,
    COL_SUMMON_TYPE   = 8,
    COL_SUMMON_TIME   = 9,
    COL_COUNT         = 10,
};

// SummonerType enum mirrors src/server/game/Entities/Object/Object.h
// SummonerType.  Keep the labelling here so the editor surfaces the
// meaningful name in the column without pulling in the worldserver header.
char const* summonerTypeLabel(int t)
{
    switch (t)
    {
        case 0: return "CREATURE_SUMMON_GROUP";
        case 1: return "CREATURE_SUMMON_GROUP_BY_GUID";
        case 2: return "GAMEOBJECT_SUMMON_GROUP";
        default: return "Unknown";
    }
}

} // namespace

CreatureSummonGroupsDialog::CreatureSummonGroupsDialog(db::MySqlClient* dbClient,
                                                      QString const& worldDbName,
                                                      QWidget* parent)
    : QDialog(parent), m_db(dbClient), m_worldDb(worldDbName)
{
    setWindowTitle(tr("Creature summon groups"));
    setModal(true);
    resize(1200, 600);

    auto* outer = new QVBoxLayout(this);

    // -- Top filter row -------------------------------------------------
    // Three optional filters: when all are zero, the dialog shows the first
    // 5000 rows.  Any non-zero value narrows the WHERE.  Load is explicit
    // so spinbox typing does not hammer the DB.
    auto* filterRow = new QHBoxLayout;
    filterRow->addWidget(new QLabel(tr("summonerId:"), this));
    m_filterSummonerId = new QSpinBox(this);
    m_filterSummonerId->setRange(0, std::numeric_limits<int>::max());
    m_filterSummonerId->setValue(0);
    filterRow->addWidget(m_filterSummonerId);

    filterRow->addWidget(new QLabel(tr("summonerType:"), this));
    m_filterSummonerType = new QSpinBox(this);
    m_filterSummonerType->setRange(0, 255);
    m_filterSummonerType->setValue(0);
    m_filterSummonerType->setToolTip(tr(
        "0 = CREATURE_SUMMON_GROUP\n"
        "1 = CREATURE_SUMMON_GROUP_BY_GUID\n"
        "2 = GAMEOBJECT_SUMMON_GROUP"));
    filterRow->addWidget(m_filterSummonerType);

    filterRow->addWidget(new QLabel(tr("groupId:"), this));
    m_filterGroupId = new QSpinBox(this);
    m_filterGroupId->setRange(0, 255);
    m_filterGroupId->setValue(0);
    filterRow->addWidget(m_filterGroupId);

    m_loadBtn = new QPushButton(tr("Load"), this);
    filterRow->addWidget(m_loadBtn);

    filterRow->addWidget(new QLabel(tr("Jump MapID:"), this));
    m_jumpMapSpin = new QSpinBox(this);
    m_jumpMapSpin->setRange(0, std::numeric_limits<int>::max());
    m_jumpMapSpin->setValue(0);
    m_jumpMapSpin->setToolTip(tr(
        "Map ID for 'Jump to position'.  The summon-groups schema carries "
        "no map context; the operator picks the destination map manually."));
    filterRow->addWidget(m_jumpMapSpin);

    filterRow->addStretch(1);
    outer->addLayout(filterRow);

    // -- Main table -----------------------------------------------------
    m_table = new QTableWidget(this);
    m_table->setColumnCount(COL_COUNT);
    m_table->setHorizontalHeaderLabels({
        tr("summonerId"), tr("summonerType"), tr("groupId"), tr("entry"),
        tr("position_x"), tr("position_y"), tr("position_z"), tr("orientation"),
        tr("summonType"), tr("summonTime") });
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setSortingEnabled(false);  // canonical ORDER BY is set in SQL.
    outer->addWidget(m_table, 1);

    // -- Action buttons -------------------------------------------------
    auto* btnRow = new QHBoxLayout;
    m_addBtn    = new QPushButton(tr("Add summon..."),       this);
    m_editBtn   = new QPushButton(tr("Edit summon"),         this);
    m_removeBtn = new QPushButton(tr("Remove summon"),       this);
    m_lookupBtn = new QPushButton(tr("Lookup summon entry"), this);
    m_jumpBtn   = new QPushButton(tr("Jump to position"),    this);
    m_editBtn  ->setEnabled(false);
    m_removeBtn->setEnabled(false);
    m_lookupBtn->setEnabled(false);
    m_jumpBtn  ->setEnabled(false);
    btnRow->addWidget(m_addBtn);
    btnRow->addWidget(m_editBtn);
    btnRow->addWidget(m_removeBtn);
    btnRow->addWidget(m_lookupBtn);
    btnRow->addWidget(m_jumpBtn);
    btnRow->addStretch(1);
    outer->addLayout(btnRow);

    m_statusLabel = new QLabel(tr("Press Load to populate.  Filters all zero = first 5000 rows."), this);
    outer->addWidget(m_statusLabel);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    outer->addWidget(buttons);

    connect(m_loadBtn,    &QPushButton::clicked, this, &CreatureSummonGroupsDialog::onLoad);
    connect(m_addBtn,     &QPushButton::clicked, this, &CreatureSummonGroupsDialog::onAdd);
    connect(m_editBtn,    &QPushButton::clicked, this, &CreatureSummonGroupsDialog::onEdit);
    connect(m_removeBtn,  &QPushButton::clicked, this, &CreatureSummonGroupsDialog::onRemove);
    connect(m_lookupBtn,  &QPushButton::clicked, this, &CreatureSummonGroupsDialog::onLookupEntry);
    connect(m_jumpBtn,    &QPushButton::clicked, this, &CreatureSummonGroupsDialog::onJump);
    connect(m_table,      &QTableWidget::itemSelectionChanged,
            this, &CreatureSummonGroupsDialog::onSelectionChanged);

    loadRows();
}

void CreatureSummonGroupsDialog::onLoad()
{
    loadRows();
}

void CreatureSummonGroupsDialog::onSelectionChanged()
{
    if (m_loading) return;
    bool const has = m_table->currentRow() >= 0;
    m_editBtn  ->setEnabled(has);
    m_removeBtn->setEnabled(has);
    m_lookupBtn->setEnabled(has);
    m_jumpBtn  ->setEnabled(has);
}

bool CreatureSummonGroupsDialog::currentRowPk(uint32_t& summonerIdOut, int& summonerTypeOut,
                                              int& groupIdOut, uint32_t& entryOut,
                                              double& posXOut, int& rowOut) const
{
    int const row = m_table->currentRow();
    if (row < 0) return false;
    auto* sIdC   = m_table->item(row, COL_SUMMONER_ID);
    auto* sTypeC = m_table->item(row, COL_SUMMONER_TYPE);
    auto* gIdC   = m_table->item(row, COL_GROUP_ID);
    auto* entryC = m_table->item(row, COL_ENTRY);
    auto* xC     = m_table->item(row, COL_X);
    if (!sIdC || !entryC) return false;
    summonerIdOut   = static_cast<uint32_t>(sIdC->data(Qt::DisplayRole).toULongLong());
    // summonerType cell stores the raw int in UserRole + a friendly label in DisplayRole.
    summonerTypeOut = sTypeC ? sTypeC->data(Qt::UserRole).toInt() : 0;
    groupIdOut      = gIdC   ? static_cast<int>(gIdC->data(Qt::DisplayRole).toLongLong()) : 0;
    entryOut        = static_cast<uint32_t>(entryC->data(Qt::DisplayRole).toULongLong());
    posXOut         = xC     ? xC->data(Qt::DisplayRole).toDouble() : 0.0;
    rowOut          = row;
    return true;
}

void CreatureSummonGroupsDialog::loadRows()
{
    m_loading = true;
    m_table->setRowCount(0);

    if (!m_db || !m_db->isConnected())
    {
        m_statusLabel->setText(tr("DB not connected."));
        m_loading = false;
        return;
    }

    // Build the WHERE incrementally so zero-valued filters are treated as
    // "match anything" rather than as exact "= 0" matches.  Operator can
    // still filter explicitly on type=0 by setting summonerId or groupId
    // to a non-zero value alongside it.
    QString whereClause;
    uint32_t const fSummonerId   = static_cast<uint32_t>(m_filterSummonerId->value());
    int      const fSummonerType = m_filterSummonerType->value();
    int      const fGroupId      = m_filterGroupId->value();
    bool const anyFilter = (fSummonerId != 0) || (fSummonerType != 0) || (fGroupId != 0);
    if (anyFilter)
    {
        QStringList clauses;
        if (fSummonerId   != 0) clauses << QStringLiteral("summonerId=%1").arg(fSummonerId);
        if (fSummonerType != 0) clauses << QStringLiteral("summonerType=%1").arg(fSummonerType);
        if (fGroupId      != 0) clauses << QStringLiteral("groupId=%1").arg(fGroupId);
        whereClause = QStringLiteral("WHERE ") + clauses.join(QStringLiteral(" AND ")) + QLatin1Char(' ');
    }

    QString const sql = QStringLiteral(
        "SELECT summonerId, summonerType, groupId, entry, "
        "       position_x, position_y, position_z, orientation, "
        "       summonType, summonTime "
        "FROM %1.creature_summon_groups %2"
        "ORDER BY summonerId, summonerType, groupId, entry "
        "LIMIT 5000")
        .arg(m_worldDb).arg(whereClause);

    db::QueryResult res;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    auto const err = m_db->query(sql.toStdString(), res);
    QApplication::restoreOverrideCursor();
    if (!err.ok())
    {
        m_statusLabel->setText(tr("creature_summon_groups query failed: %1")
            .arg(QString::fromStdString(err.message)));
        m_loading = false;
        return;
    }

    m_table->setRowCount(static_cast<int>(res.rowCount()));
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        uint32_t const summonerId   = static_cast<uint32_t>(res.asUInt64(r, 0).value_or(0));
        int      const summonerType = static_cast<int>(res.asUInt64(r, 1).value_or(0));
        int      const groupId      = static_cast<int>(res.asUInt64(r, 2).value_or(0));
        uint32_t const entry        = static_cast<uint32_t>(res.asUInt64(r, 3).value_or(0));
        double   const px           = res.asDouble(r, 4).value_or(0.0);
        double   const py           = res.asDouble(r, 5).value_or(0.0);
        double   const pz           = res.asDouble(r, 6).value_or(0.0);
        double   const orient       = res.asDouble(r, 7).value_or(0.0);
        int      const summonType   = static_cast<int>(res.asUInt64(r, 8).value_or(0));
        uint32_t const summonTime   = static_cast<uint32_t>(res.asUInt64(r, 9).value_or(0));

        auto setU = [&](int col, uint64_t v) {
            auto* c = new QTableWidgetItem;
            c->setData(Qt::DisplayRole, qulonglong(v));
            m_table->setItem(int(r), col, c);
        };
        auto setI = [&](int col, qlonglong v) {
            auto* c = new QTableWidgetItem;
            c->setData(Qt::DisplayRole, v);
            m_table->setItem(int(r), col, c);
        };
        auto setD = [&](int col, double v) {
            auto* c = new QTableWidgetItem;
            c->setData(Qt::DisplayRole, v);
            m_table->setItem(int(r), col, c);
        };

        setU(COL_SUMMONER_ID, summonerId);

        // summonerType uses a labelled text cell with the raw int parked
        // in UserRole so Edit/Remove still recover the numeric value.
        {
            QString label = (summonerType >= 0 && summonerType <= 2)
                ? tr("%1 (%2)").arg(summonerTypeLabel(summonerType)).arg(summonerType)
                : tr("Unknown (%1)").arg(summonerType);
            auto* c = new QTableWidgetItem(label);
            c->setData(Qt::UserRole, summonerType);
            m_table->setItem(int(r), COL_SUMMONER_TYPE, c);
        }
        setI(COL_GROUP_ID, qlonglong(groupId));
        setU(COL_ENTRY,    entry);
        setD(COL_X,        px);
        setD(COL_Y,        py);
        setD(COL_Z,        pz);
        setD(COL_ORIENT,   orient);
        setI(COL_SUMMON_TYPE, qlonglong(summonType));
        setU(COL_SUMMON_TIME, summonTime);
    }

    m_statusLabel->setText(tr("rows=%1 (capped at 5000) filter=%2")
        .arg(res.rowCount())
        .arg(anyFilter ? whereClause.trimmed() : tr("(none)")));
    m_loading = false;
    onSelectionChanged();
}

bool CreatureSummonGroupsDialog::runInTransaction(QStringList const& sqls, QString const& description)
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
    uint64_t totalAffected = 0;
    for (QString const& sql : sqls)
    {
        uint64_t affected = 0;
        err = m_db->exec(sql.toStdString(), &affected);
        if (!err.ok())
        {
            (void)m_db->exec("ROLLBACK");
            QMessageBox::critical(this, tr("DML failed"),
                tr("%1\n\n%2").arg(description).arg(QString::fromStdString(err.message)));
            return false;
        }
        totalAffected += affected;
    }
    err = m_db->exec("COMMIT");
    if (!err.ok())
    {
        (void)m_db->exec("ROLLBACK");
        QMessageBox::critical(this, tr("Commit failed"),
            tr("%1\n\n%2").arg(description).arg(QString::fromStdString(err.message)));
        return false;
    }
    m_statusLabel->setText(tr("%1 (affected=%2)").arg(description).arg(qulonglong(totalAffected)));
    return true;
}

void CreatureSummonGroupsDialog::onAdd()
{
    openModal(false);
}

void CreatureSummonGroupsDialog::onEdit()
{
    uint32_t sId = 0, entry = 0;
    int sType = 0, gId = 0, row = -1;
    double px = 0.0;
    if (!currentRowPk(sId, sType, gId, entry, px, row)) return;
    openModal(true);
}

void CreatureSummonGroupsDialog::onRemove()
{
    uint32_t sId = 0, entry = 0;
    int sType = 0, gId = 0, row = -1;
    double px = 0.0;
    if (!currentRowPk(sId, sType, gId, entry, px, row)) return;

    QString const pxStr = QString::number(px, 'f', 4);
    auto const choice = QMessageBox::question(this, tr("Remove summon"),
        tr("Delete creature_summon_groups row\n"
           "summonerId=%1 summonerType=%2 groupId=%3 entry=%4 position_x=%5?")
            .arg(sId).arg(sType).arg(gId).arg(entry).arg(pxStr),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (choice != QMessageBox::Yes) return;

    // PK is composite (summonerId, summonerType, groupId, entry, position_x);
    // include all five so duplicate entry rows at different position_x stay
    // disambiguated.
    QString const sql = QStringLiteral(
        "DELETE FROM %1.creature_summon_groups "
        "WHERE summonerId=%2 AND summonerType=%3 AND groupId=%4 "
        "AND entry=%5 AND ABS(position_x - %6) < 0.0001")
        .arg(m_worldDb).arg(sId).arg(sType).arg(gId).arg(entry).arg(pxStr);
    if (runInTransaction(QStringList{ sql },
            tr("DELETE creature_summon_groups (summonerId=%1, summonerType=%2, groupId=%3, entry=%4)")
                .arg(sId).arg(sType).arg(gId).arg(entry)))
        loadRows();
}

void CreatureSummonGroupsDialog::onLookupEntry()
{
    uint32_t sId = 0, entry = 0;
    int sType = 0, gId = 0, row = -1;
    double px = 0.0;
    if (!currentRowPk(sId, sType, gId, entry, px, row)) return;

    // MainWindow does NOT today expose an openItemInfoDock()-style helper
    // for creature_template; the spec forbids introducing one.  Surface
    // the request via qDebug + status label so the operator can see it
    // landed and can route via the standard spawn-search workflow.
    qDebug() << "CreatureSummonGroupsDialog: lookup creature_template Entry=" << entry;
    m_statusLabel->setText(tr("Lookup creature_template requested: Entry=%1 "
        "(open via Spawn search dialog or NPC editor)").arg(entry));
}

void CreatureSummonGroupsDialog::onJump()
{
    uint32_t sId = 0, entry = 0;
    int sType = 0, gId = 0, row = -1;
    double px = 0.0;
    if (!currentRowPk(sId, sType, gId, entry, px, row)) return;

    auto* yC = m_table->item(row, COL_Y);
    double const py = yC ? yC->data(Qt::DisplayRole).toDouble() : 0.0;
    uint32_t const mapId = static_cast<uint32_t>(m_jumpMapSpin->value());
    emit jumpRequested(mapId, static_cast<float>(px), static_cast<float>(py));
}

void CreatureSummonGroupsDialog::openModal(bool isEdit)
{
    QDialog dlg(this);
    dlg.setWindowTitle(isEdit ? tr("Edit summon") : tr("Add summon"));
    dlg.setModal(true);

    auto* form = new QFormLayout(&dlg);

    auto* summonerIdSpin = new QSpinBox(&dlg);
    summonerIdSpin->setRange(0, std::numeric_limits<int>::max());
    summonerIdSpin->setEnabled(!isEdit);  // part of composite PK.
    form->addRow(tr("summonerId:"), summonerIdSpin);

    auto* summonerTypeSpin = new QSpinBox(&dlg);
    summonerTypeSpin->setRange(0, 255);
    summonerTypeSpin->setEnabled(!isEdit);
    summonerTypeSpin->setToolTip(tr(
        "0 = CREATURE_SUMMON_GROUP\n"
        "1 = CREATURE_SUMMON_GROUP_BY_GUID\n"
        "2 = GAMEOBJECT_SUMMON_GROUP"));
    form->addRow(tr("summonerType:"), summonerTypeSpin);

    auto* groupIdSpin = new QSpinBox(&dlg);
    groupIdSpin->setRange(0, 255);
    groupIdSpin->setEnabled(!isEdit);
    form->addRow(tr("groupId:"), groupIdSpin);

    auto* entrySpin = new QSpinBox(&dlg);
    entrySpin->setRange(0, std::numeric_limits<int>::max());
    entrySpin->setEnabled(!isEdit);
    form->addRow(tr("entry:"), entrySpin);

    auto mkDouble = [&](double lo, double hi, int decimals, double step) {
        auto* sp = new QDoubleSpinBox(&dlg);
        sp->setRange(lo, hi);
        sp->setDecimals(decimals);
        sp->setSingleStep(step);
        return sp;
    };
    auto* xSpin = mkDouble(-1e7, 1e7, 4, 0.1);
    auto* ySpin = mkDouble(-1e7, 1e7, 4, 0.1);
    auto* zSpin = mkDouble(-1e7, 1e7, 4, 0.1);
    auto* oSpin = mkDouble(-12.6, 12.6, 4, 0.1);
    xSpin->setEnabled(!isEdit);  // position_x is part of composite PK.
    form->addRow(tr("position_x:"),  xSpin);
    form->addRow(tr("position_y:"),  ySpin);
    form->addRow(tr("position_z:"),  zSpin);
    form->addRow(tr("orientation:"), oSpin);

    auto* summonTypeSpin = new QSpinBox(&dlg);
    summonTypeSpin->setRange(0, 255);
    summonTypeSpin->setToolTip(tr(
        "TempSummonType enum value (see Object.h).  Common values: 1 (TIMED_OR_DEAD_DESPAWN), "
        "3 (TIMED_DESPAWN), 8 (MANUAL_DESPAWN)."));
    form->addRow(tr("summonType:"), summonTypeSpin);

    auto* summonTimeSpin = new QSpinBox(&dlg);
    summonTimeSpin->setRange(0, std::numeric_limits<int>::max());
    summonTimeSpin->setSuffix(tr(" ms"));
    form->addRow(tr("summonTime:"), summonTimeSpin);

    // Pre-populate from selected row when editing.  Composite PK fields
    // come from the row directly so the UPDATE WHERE clause matches even
    // if the operator never touched the new fields.
    uint32_t origSummonerId = 0, origEntry = 0;
    int origSummonerType = 0, origGroupId = 0;
    double origPosX = 0.0;
    if (isEdit)
    {
        int row = -1;
        if (!currentRowPk(origSummonerId, origSummonerType, origGroupId, origEntry, origPosX, row))
        {
            QMessageBox::warning(this, tr("Edit summon"), tr("No row selected."));
            return;
        }
        auto getD = [&](int col) -> double {
            auto* c = m_table->item(row, col);
            return c ? c->data(Qt::DisplayRole).toDouble() : 0.0;
        };
        auto getI = [&](int col) -> qlonglong {
            auto* c = m_table->item(row, col);
            return c ? c->data(Qt::DisplayRole).toLongLong() : 0;
        };
        auto getU = [&](int col) -> uint64_t {
            auto* c = m_table->item(row, col);
            return c ? c->data(Qt::DisplayRole).toULongLong() : 0;
        };
        summonerIdSpin  ->setValue(static_cast<int>(origSummonerId));
        summonerTypeSpin->setValue(origSummonerType);
        groupIdSpin     ->setValue(origGroupId);
        entrySpin       ->setValue(static_cast<int>(origEntry));
        xSpin           ->setValue(origPosX);
        ySpin           ->setValue(getD(COL_Y));
        zSpin           ->setValue(getD(COL_Z));
        oSpin           ->setValue(getD(COL_ORIENT));
        summonTypeSpin  ->setValue(static_cast<int>(getI(COL_SUMMON_TYPE)));
        summonTimeSpin  ->setValue(static_cast<int>(getU(COL_SUMMON_TIME)));
    }

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    form->addRow(btns);

    if (dlg.exec() != QDialog::Accepted)
        return;

    uint32_t const summonerId   = static_cast<uint32_t>(summonerIdSpin->value());
    int      const summonerType = summonerTypeSpin->value();
    int      const groupId      = groupIdSpin->value();
    uint32_t const entry        = static_cast<uint32_t>(entrySpin->value());
    QString  const xs = QString::number(xSpin->value(), 'f', 4);
    QString  const ys = QString::number(ySpin->value(), 'f', 4);
    QString  const zs = QString::number(zSpin->value(), 'f', 4);
    QString  const os = QString::number(oSpin->value(), 'f', 4);
    int      const summonType   = summonTypeSpin->value();
    uint32_t const summonTime   = static_cast<uint32_t>(summonTimeSpin->value());

    if (isEdit)
    {
        // PK columns pinned to the original row's values; only the mutable
        // columns (position_y/z, orientation, summonType, summonTime) get
        // rewritten.  Switching the type / re-keying requires Remove + Add.
        QString const origPxStr = QString::number(origPosX, 'f', 4);
        QString const upd = QStringLiteral(
            "UPDATE %1.creature_summon_groups SET "
            "position_y=%2, position_z=%3, orientation=%4, "
            "summonType=%5, summonTime=%6 "
            "WHERE summonerId=%7 AND summonerType=%8 AND groupId=%9 "
            "AND entry=%10 AND ABS(position_x - %11) < 0.0001")
            .arg(m_worldDb).arg(ys).arg(zs).arg(os)
            .arg(summonType).arg(summonTime)
            .arg(origSummonerId).arg(origSummonerType).arg(origGroupId)
            .arg(origEntry).arg(origPxStr);
        if (runInTransaction(QStringList{ upd },
                tr("UPDATE creature_summon_groups (summonerId=%1, summonerType=%2, "
                   "groupId=%3, entry=%4)")
                    .arg(origSummonerId).arg(origSummonerType)
                    .arg(origGroupId).arg(origEntry)))
            loadRows();
    }
    else
    {
        // PK collision check.  INSERT would already fail server-side but a
        // friendly QMessageBox beats a raw MySQL duplicate-key error.
        char checkSql[512];
        std::snprintf(checkSql, sizeof(checkSql),
            "SELECT COUNT(*) FROM %s.creature_summon_groups "
            "WHERE summonerId=%u AND summonerType=%d AND groupId=%d "
            "AND entry=%u AND ABS(position_x - %s) < 0.0001",
            m_worldDb.toStdString().c_str(),
            summonerId, summonerType, groupId, entry,
            xs.toStdString().c_str());
        db::QueryResult cRes;
        auto err = m_db->query(checkSql, cRes);
        if (err.ok() && cRes.rowCount() > 0 && cRes.asUInt64(0, 0).value_or(0) != 0)
        {
            QMessageBox::warning(this, tr("Add summon"),
                tr("creature_summon_groups row already exists for "
                   "summonerId=%1, summonerType=%2, groupId=%3, entry=%4, position_x=%5.")
                    .arg(summonerId).arg(summonerType).arg(groupId).arg(entry).arg(xs));
            return;
        }

        QString const ins = QStringLiteral(
            "INSERT INTO %1.creature_summon_groups "
            "(summonerId, summonerType, groupId, entry, "
            " position_x, position_y, position_z, orientation, "
            " summonType, summonTime) "
            "VALUES (%2, %3, %4, %5, %6, %7, %8, %9, %10, %11)")
            .arg(m_worldDb).arg(summonerId).arg(summonerType).arg(groupId)
            .arg(entry).arg(xs).arg(ys).arg(zs).arg(os)
            .arg(summonType).arg(summonTime);
        if (runInTransaction(QStringList{ ins },
                tr("INSERT creature_summon_groups (summonerId=%1, summonerType=%2, "
                   "groupId=%3, entry=%4)")
                    .arg(summonerId).arg(summonerType).arg(groupId).arg(entry)))
            loadRows();
    }
}

} // namespace world_editor::app
