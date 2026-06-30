#include "FindSimilarDialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <sstream>
#include <string>

namespace world_editor::app
{

FindSimilarDialog::FindSimilarDialog(db::MySqlClient* db, render::Spawn const& reference, QWidget* parent)
    : QDialog(parent), m_db(db), m_ref(reference)
{
    setWindowTitle(tr("Find similar spawns"));
    setModal(true);
    resize(820, 560);

    // --- Fetch reference template fields (npcflag/faction) ------------
    // Done once at construction; the radio checkboxes for those fields
    // show the resolved value as part of their label so the operator
    // knows what they're filtering on.
    if (m_db && m_db->isConnected() && m_ref.entry != 0)
    {
        std::ostringstream q;
        q << "SELECT npcflag, faction FROM creature_template WHERE entry = " << m_ref.entry;
        db::QueryResult res;
        if (m_db->query(q.str(), res).ok() && res.rowCount() > 0)
        {
            m_refNpcFlag        = res.asUInt64(0, 0).value_or(0);
            m_refFaction        = uint32_t(res.asUInt64(0, 1).value_or(0));
            m_refTemplateLoaded = true;
        }
    }

    // --- Header -------------------------------------------------------
    auto* header = new QLabel(
        tr("Find spawns similar to guid %1 entry %2")
            .arg(qlonglong(m_ref.guid))
            .arg(m_ref.entry),
        this);
    QFont hf = header->font();
    hf.setBold(true);
    header->setFont(hf);

    // --- Similarity checkboxes ---------------------------------------
    m_cbEntry      = new QCheckBox(tr("Same entry (%1)").arg(m_ref.entry), this);
    m_cbEntry->setChecked(true);
    m_cbMap        = new QCheckBox(tr("Same map (%1)").arg(m_ref.mapId), this);
    m_cbZone       = new QCheckBox(tr("Same zone (%1)").arg(m_ref.zoneId), this);
    m_cbArea       = new QCheckBox(tr("Same area (%1)").arg(m_ref.areaId), this);
    m_cbPhaseId    = new QCheckBox(tr("Same PhaseId (%1)").arg(m_ref.phaseId), this);
    m_cbPhaseGroup = new QCheckBox(tr("Same PhaseGroup (%1)").arg(m_ref.phaseGroup), this);

    m_cbNpcFlag    = new QCheckBox(
        m_refTemplateLoaded
            ? tr("Same npcflag template (0x%1)").arg(QString::number(qulonglong(m_refNpcFlag), 16))
            : tr("Same npcflag template (template not loaded)"),
        this);
    m_cbNpcFlag->setEnabled(m_refTemplateLoaded);

    m_cbFaction    = new QCheckBox(
        m_refTemplateLoaded
            ? tr("Same faction template (%1)").arg(m_refFaction)
            : tr("Same faction template (template not loaded)"),
        this);
    m_cbFaction->setEnabled(m_refTemplateLoaded);

    m_cbRadius     = new QCheckBox(tr("Within radius (yards) - XY plane on same map"), this);
    m_radiusYards  = new QDoubleSpinBox(this);
    m_radiusYards->setRange(0.1, 100000.0);
    m_radiusYards->setDecimals(2);
    m_radiusYards->setSingleStep(5.0);
    m_radiusYards->setValue(50.0);
    m_radiusYards->setSuffix(tr(" yd"));

    auto* radiusRow = new QHBoxLayout;
    radiusRow->addWidget(m_cbRadius);
    radiusRow->addWidget(m_radiusYards);
    radiusRow->addStretch(1);

    auto* grid = new QGridLayout;
    int row = 0;
    grid->addWidget(m_cbEntry,      row,   0);
    grid->addWidget(m_cbMap,        row,   1);
    ++row;
    grid->addWidget(m_cbZone,       row,   0);
    grid->addWidget(m_cbArea,       row,   1);
    ++row;
    grid->addWidget(m_cbPhaseId,    row,   0);
    grid->addWidget(m_cbPhaseGroup, row,   1);
    ++row;
    grid->addWidget(m_cbNpcFlag,    row,   0);
    grid->addWidget(m_cbFaction,    row,   1);

    // --- Buttons -----------------------------------------------------
    m_runBtn  = new QPushButton(tr("Run"), this);
    m_jumpBtn = new QPushButton(tr("Jump to selected"), this);
    m_jumpBtn->setEnabled(false);

    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch(1);
    btnRow->addWidget(m_runBtn);
    btnRow->addWidget(m_jumpBtn);

    // --- Results table -----------------------------------------------
    m_results = new QTableWidget(this);
    m_results->setColumnCount(6);
    m_results->setHorizontalHeaderLabels(
        { tr("guid"), tr("entry"), tr("map"), tr("x"), tr("y"), tr("z") });
    m_results->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_results->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_results->setSelectionMode(QAbstractItemView::SingleSelection);
    m_results->setSortingEnabled(true);
    m_results->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_results->horizontalHeader()->setStretchLastSection(true);

    m_statusLbl = new QLabel(QString{}, this);

    auto* closeBox = new QDialogButtonBox(QDialogButtonBox::Close, this);

    auto* outer = new QVBoxLayout(this);
    outer->addWidget(header);
    outer->addLayout(grid);
    outer->addLayout(radiusRow);
    outer->addLayout(btnRow);
    outer->addWidget(m_results, 1);
    outer->addWidget(m_statusLbl);
    outer->addWidget(closeBox);

    // --- Wiring -------------------------------------------------------
    connect(m_runBtn,  &QPushButton::clicked,       this, &FindSimilarDialog::onRun);
    connect(m_jumpBtn, &QPushButton::clicked,       this, &FindSimilarDialog::onJumpSelected);
    connect(closeBox,  &QDialogButtonBox::rejected, this, &QDialog::accept);
    connect(m_results, &QTableWidget::itemSelectionChanged, this, [this]() {
        m_jumpBtn->setEnabled(!m_results->selectedItems().isEmpty());
    });
    connect(m_results, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        emitJumpFromRow(row);
    });
}

void FindSimilarDialog::onRun()
{
    if (!m_db || !m_db->isConnected())
    {
        m_statusLbl->setText(tr("not connected to DB"));
        return;
    }

    m_results->setSortingEnabled(false);
    m_results->setRowCount(0);

    bool const wantEntry      = m_cbEntry->isChecked();
    bool const wantMap        = m_cbMap->isChecked();
    bool const wantZone       = m_cbZone->isChecked();
    bool const wantArea       = m_cbArea->isChecked();
    bool const wantPhaseId    = m_cbPhaseId->isChecked();
    bool const wantPhaseGroup = m_cbPhaseGroup->isChecked();
    bool const wantNpcFlag    = m_cbNpcFlag->isChecked()  && m_refTemplateLoaded;
    bool const wantFaction    = m_cbFaction->isChecked()  && m_refTemplateLoaded;
    bool const wantRadius     = m_cbRadius->isChecked();

    // Need at least one constraint - otherwise we'd return every
    // creature in the world.  Surface that to the operator instead of
    // running a 30s LIMIT 500 scan.
    if (!wantEntry && !wantMap && !wantZone && !wantArea && !wantPhaseId && !wantPhaseGroup
        && !wantNpcFlag && !wantFaction && !wantRadius)
    {
        m_statusLbl->setText(tr("enable at least one similarity criterion"));
        return;
    }

    // Whether we need the creature_template join.  Adds no rows
    // (templates are 1:1 with creature.id) but costs a hash probe per
    // creature row, so omit unless a template field is actually filtered.
    bool const needTplJoin = wantNpcFlag || wantFaction;

    std::ostringstream sql;
    sql << "SELECT c.guid, c.id, c.map, c.position_x, c.position_y, c.position_z "
        << "FROM creature c";
    if (needTplJoin)
        sql << " LEFT JOIN creature_template ct ON ct.entry = c.id";
    sql << " WHERE 1=1";

    if (wantEntry)
        sql << " AND c.id = "          << m_ref.entry;
    if (wantMap)
        sql << " AND c.map = "         << m_ref.mapId;
    if (wantZone)
        sql << " AND c.zoneId = "      << m_ref.zoneId;
    if (wantArea)
        sql << " AND c.areaId = "      << m_ref.areaId;
    if (wantPhaseId)
        sql << " AND c.PhaseId = "     << m_ref.phaseId;
    if (wantPhaseGroup)
        sql << " AND c.PhaseGroup = "  << m_ref.phaseGroup;
    if (wantNpcFlag)
        sql << " AND ct.npcflag = "    << m_refNpcFlag;
    if (wantFaction)
        sql << " AND ct.faction = "    << m_refFaction;

    if (wantRadius)
    {
        double const r = m_radiusYards->value();
        double const r2 = r * r;
        // Implicit "same map" - distance across maps is meaningless.
        // We don't auto-tick the map checkbox; we just AND it in.
        sql << " AND c.map = " << m_ref.mapId;
        sql << " AND (POW(c.position_x - " << double(m_ref.worldX) << ", 2)"
            <<       " + POW(c.position_y - " << double(m_ref.worldY) << ", 2)) <= " << r2;
    }

    sql << " ORDER BY c.map, c.guid LIMIT 500";

    QApplication::setOverrideCursor(Qt::WaitCursor);
    db::QueryResult res;
    auto const err = m_db->query(sql.str(), res);
    QApplication::restoreOverrideCursor();

    if (!err.ok())
    {
        m_statusLbl->setText(tr("query failed: %1").arg(QString::fromStdString(err.message)));
        return;
    }

    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        int const rowIdx = m_results->rowCount();
        m_results->insertRow(rowIdx);

        auto setInt = [&](int col, qlonglong v) {
            auto* it = new QTableWidgetItem();
            it->setData(Qt::DisplayRole, v);
            m_results->setItem(rowIdx, col, it);
        };
        auto setFloat = [&](int col, double v) {
            auto* it = new QTableWidgetItem();
            it->setData(Qt::DisplayRole, QString::number(v, 'f', 2));
            m_results->setItem(rowIdx, col, it);
        };

        setInt  (0, qlonglong(res.asInt64 (r, 0).value_or(0)));
        setInt  (1, qlonglong(res.asUInt64(r, 1).value_or(0)));
        setInt  (2, qlonglong(res.asUInt64(r, 2).value_or(0)));
        setFloat(3, res.asDouble(r, 3).value_or(0.0));
        setFloat(4, res.asDouble(r, 4).value_or(0.0));
        setFloat(5, res.asDouble(r, 5).value_or(0.0));
    }

    m_results->setSortingEnabled(true);
    m_statusLbl->setText(tr("matches=%1%2")
        .arg(res.rowCount())
        .arg(res.rowCount() >= 500 ? tr(" (LIMIT 500 hit)") : QString{}));
}

void FindSimilarDialog::onJumpSelected()
{
    auto const sel = m_results->selectionModel()->selectedRows();
    if (sel.isEmpty()) return;
    emitJumpFromRow(sel.front().row());
}

void FindSimilarDialog::emitJumpFromRow(int row)
{
    if (row < 0 || row >= m_results->rowCount()) return;
    auto* guidItem = m_results->item(row, 0);
    auto* mapItem  = m_results->item(row, 2);
    auto* xItem    = m_results->item(row, 3);
    auto* yItem    = m_results->item(row, 4);
    if (!guidItem || !mapItem || !xItem || !yItem) return;

    bool okMap = false, okX = false, okY = false, okGuid = false;
    uint32_t const mapId = uint32_t(mapItem->text().toUInt(&okMap));
    float const x = xItem->text().toFloat(&okX);
    float const y = yItem->text().toFloat(&okY);
    qlonglong const guid = guidItem->text().toLongLong(&okGuid);
    if (!okMap || !okX || !okY) return;

    std::optional<int64_t> guidOpt;
    if (okGuid && guid != 0)
        guidOpt = int64_t(guid);
    emit jumpRequested(mapId, x, y, guidOpt);
    accept();
}

} // namespace world_editor::app
