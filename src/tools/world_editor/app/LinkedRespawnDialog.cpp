#include "LinkedRespawnDialog.h"

#include "../db/MySqlClient.h"

#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
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

// Column indices for the main linked_respawn table.
enum Col : int
{
    COL_GUID        = 0,
    COL_LINKED_GUID = 1,
    COL_LINK_TYPE   = 2,
    COL_COUNT       = 3,
};

// Side of the linkType arrow that names the *dependent* spawn (the `guid`
// column).  Used by "Jump to dependent spawn" to pick which spawn table
// to probe for (map, position_x, position_y).
//   0 = CREATURE -> CREATURE     -> dependent is creature
//   1 = CREATURE -> GAMEOBJECT   -> dependent is creature
//   2 = GAMEOBJECT -> GAMEOBJECT -> dependent is gameobject
//   3 = GAMEOBJECT -> CREATURE   -> dependent is gameobject
bool dependentIsCreature(uint32_t linkType)
{
    return linkType == 0u || linkType == 1u;
}

} // namespace

LinkedRespawnDialog::LinkedRespawnDialog(db::MySqlClient* dbClient,
                                         QString const& worldDbName,
                                         QWidget* parent)
    : QDialog(parent), m_db(dbClient), m_worldDb(worldDbName)
{
    setWindowTitle(tr("Linked respawn"));
    setModal(true);
    resize(820, 560);

    auto* outer = new QVBoxLayout(this);

    // -- Top filter row: substring filter on guid / linkedGuid + Refresh --
    auto* filterRow = new QHBoxLayout;
    filterRow->addWidget(new QLabel(tr("Filter:"), this));
    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText(tr("substring match on guid or linkedGuid"));
    filterRow->addWidget(m_filterEdit, 1);
    m_refreshBtn = new QPushButton(tr("Refresh"), this);
    filterRow->addWidget(m_refreshBtn);
    outer->addLayout(filterRow);

    // -- Main link table -------------------------------------------------
    m_table = new QTableWidget(this);
    m_table->setColumnCount(COL_COUNT);
    m_table->setHorizontalHeaderLabels({ tr("guid"), tr("linkedGuid"), tr("linkType") });
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setSortingEnabled(false);
    outer->addWidget(m_table, 1);

    // -- Action buttons --------------------------------------------------
    auto* btnRow = new QHBoxLayout;
    m_addBtn    = new QPushButton(tr("Add link..."), this);
    m_editBtn   = new QPushButton(tr("Edit link"), this);
    m_removeBtn = new QPushButton(tr("Remove link"), this);
    m_jumpBtn   = new QPushButton(tr("Jump to dependent spawn"), this);
    m_editBtn  ->setEnabled(false);
    m_removeBtn->setEnabled(false);
    m_jumpBtn  ->setEnabled(false);
    btnRow->addWidget(m_addBtn);
    btnRow->addWidget(m_editBtn);
    btnRow->addWidget(m_removeBtn);
    btnRow->addWidget(m_jumpBtn);
    btnRow->addStretch(1);
    outer->addLayout(btnRow);

    m_statusLabel = new QLabel(tr("(no rows loaded)"), this);
    outer->addWidget(m_statusLabel);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    outer->addWidget(buttons);

    // Wire signals.
    connect(m_refreshBtn, &QPushButton::clicked, this, &LinkedRespawnDialog::onRefresh);
    connect(m_filterEdit, &QLineEdit::textChanged, this, &LinkedRespawnDialog::onFilterChanged);
    connect(m_addBtn,     &QPushButton::clicked, this, &LinkedRespawnDialog::onAdd);
    connect(m_editBtn,    &QPushButton::clicked, this, &LinkedRespawnDialog::onEdit);
    connect(m_removeBtn,  &QPushButton::clicked, this, &LinkedRespawnDialog::onRemove);
    connect(m_jumpBtn,    &QPushButton::clicked, this, &LinkedRespawnDialog::onJump);
    connect(m_table,      &QTableWidget::itemSelectionChanged,
            this, &LinkedRespawnDialog::onSelectionChanged);

    loadRows();
}

QString LinkedRespawnDialog::linkTypeLabel(uint32_t linkType)
{
    switch (linkType)
    {
        case 0: return tr("Creature -> Creature");
        case 1: return tr("Creature -> GameObject");
        case 2: return tr("GameObject -> GameObject");
        case 3: return tr("GameObject -> Creature");
        default: return tr("Unknown (%1)").arg(linkType);
    }
}

void LinkedRespawnDialog::loadRows()
{
    m_table->setRowCount(0);
    if (!m_db || !m_db->isConnected())
    {
        m_statusLabel->setText(tr("DB not connected."));
        return;
    }

    QString const sql = QStringLiteral(
        "SELECT guid, linkedGuid, linkType "
        "FROM %1.linked_respawn "
        "ORDER BY guid "
        "LIMIT 5000").arg(m_worldDb);

    db::QueryResult res;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    auto const err = m_db->query(sql.toStdString(), res);
    QApplication::restoreOverrideCursor();
    if (!err.ok())
    {
        m_statusLabel->setText(tr("linked_respawn query failed: %1")
            .arg(QString::fromStdString(err.message)));
        return;
    }

    m_table->setRowCount(static_cast<int>(res.rowCount()));
    for (size_t r = 0; r < res.rowCount(); ++r)
    {
        uint64_t const guid       = res.asUInt64(r, 0).value_or(0);
        uint64_t const linkedGuid = res.asUInt64(r, 1).value_or(0);
        uint32_t const linkType   = static_cast<uint32_t>(res.asUInt64(r, 2).value_or(0));

        auto* guidCell = new QTableWidgetItem;
        guidCell->setData(Qt::DisplayRole, qulonglong(guid));
        m_table->setItem(int(r), COL_GUID, guidCell);

        auto* linkedCell = new QTableWidgetItem;
        linkedCell->setData(Qt::DisplayRole, qulonglong(linkedGuid));
        m_table->setItem(int(r), COL_LINKED_GUID, linkedCell);

        // linkType column carries the friendly label visually, but the raw
        // byte is stashed in UserRole so the row can be edited / removed
        // without re-parsing the label string.
        auto* linkTypeCell = new QTableWidgetItem(linkTypeLabel(linkType));
        linkTypeCell->setData(Qt::UserRole, linkType);
        m_table->setItem(int(r), COL_LINK_TYPE, linkTypeCell);
    }

    m_statusLabel->setText(tr("rows=%1%2")
        .arg(res.rowCount())
        .arg(res.rowCount() >= 5000 ? tr(" (truncated at 5000)") : QString()));

    applyFilter();
    onSelectionChanged();
}

void LinkedRespawnDialog::applyFilter()
{
    QString const needle = m_filterEdit ? m_filterEdit->text().trimmed() : QString();
    int visible = 0;
    int const rows = m_table->rowCount();
    for (int r = 0; r < rows; ++r)
    {
        bool show = needle.isEmpty();
        if (!show)
        {
            auto* guidCell   = m_table->item(r, COL_GUID);
            auto* linkedCell = m_table->item(r, COL_LINKED_GUID);
            QString const guidStr   = guidCell   ? guidCell  ->data(Qt::DisplayRole).toString() : QString();
            QString const linkedStr = linkedCell ? linkedCell->data(Qt::DisplayRole).toString() : QString();
            show = guidStr.contains(needle, Qt::CaseInsensitive)
                || linkedStr.contains(needle, Qt::CaseInsensitive);
        }
        m_table->setRowHidden(r, !show);
        if (show) ++visible;
    }
    if (!needle.isEmpty())
        m_statusLabel->setText(tr("rows=%1 visible=%2 (filter='%3')")
            .arg(rows).arg(visible).arg(needle));
}

void LinkedRespawnDialog::onRefresh()
{
    loadRows();
}

void LinkedRespawnDialog::onFilterChanged()
{
    applyFilter();
}

void LinkedRespawnDialog::onSelectionChanged()
{
    bool const has = m_table->currentRow() >= 0;
    m_editBtn  ->setEnabled(has);
    m_removeBtn->setEnabled(has);
    m_jumpBtn  ->setEnabled(has);
}

bool LinkedRespawnDialog::currentRowKey(uint64_t& guidOut, uint32_t& linkTypeOut,
                                         uint64_t& linkedGuidOut) const
{
    int const row = m_table->currentRow();
    if (row < 0) return false;
    auto* guidCell     = m_table->item(row, COL_GUID);
    auto* linkedCell   = m_table->item(row, COL_LINKED_GUID);
    auto* linkTypeCell = m_table->item(row, COL_LINK_TYPE);
    if (!guidCell || !linkedCell || !linkTypeCell) return false;
    guidOut       = guidCell  ->data(Qt::DisplayRole).toULongLong();
    linkedGuidOut = linkedCell->data(Qt::DisplayRole).toULongLong();
    linkTypeOut   = linkTypeCell->data(Qt::UserRole).toUInt();
    return true;
}

bool LinkedRespawnDialog::runInTransaction(QStringList const& sqls, QString const& description)
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
    m_statusLabel->setText(tr("%1 (affected=%2)")
        .arg(description).arg(qulonglong(totalAffected)));
    return true;
}

void LinkedRespawnDialog::onAdd()
{
    openModal(false, 0, 0, 0);
}

void LinkedRespawnDialog::onEdit()
{
    uint64_t guid       = 0;
    uint32_t linkType   = 0;
    uint64_t linkedGuid = 0;
    if (!currentRowKey(guid, linkType, linkedGuid)) return;
    openModal(true, guid, linkType, linkedGuid);
}

void LinkedRespawnDialog::onRemove()
{
    uint64_t guid       = 0;
    uint32_t linkType   = 0;
    uint64_t linkedGuid = 0;
    if (!currentRowKey(guid, linkType, linkedGuid)) return;

    auto const choice = QMessageBox::question(this, tr("Remove link"),
        tr("Delete linked_respawn row\n  guid=%1\n  linkedGuid=%2\n  linkType=%3 (%4)?")
            .arg(qulonglong(guid))
            .arg(qulonglong(linkedGuid))
            .arg(linkType)
            .arg(linkTypeLabel(linkType)),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (choice != QMessageBox::Yes) return;

    QString const sql = QStringLiteral(
        "DELETE FROM %1.linked_respawn WHERE guid=%2 AND linkType=%3")
        .arg(m_worldDb).arg(qulonglong(guid)).arg(linkType);
    if (runInTransaction(QStringList{ sql },
            tr("DELETE linked_respawn (guid=%1, linkType=%2)")
                .arg(qulonglong(guid)).arg(linkType)))
        loadRows();
}

void LinkedRespawnDialog::onJump()
{
    uint64_t guid       = 0;
    uint32_t linkType   = 0;
    uint64_t linkedGuid = 0;
    if (!currentRowKey(guid, linkType, linkedGuid)) return;
    if (!m_db || !m_db->isConnected())
    {
        m_statusLabel->setText(tr("DB not connected."));
        return;
    }

    // The `guid` column names the dependent spawn.  linkType's source side
    // decides whether that lives in `creature` or `gameobject`.  Probe the
    // matching table directly rather than UNION ALL to keep the result row
    // unambiguous.
    bool const fromCreature = dependentIsCreature(linkType);
    QString const sql = QStringLiteral(
        "SELECT map, position_x, position_y FROM %1.%2 WHERE guid=%3 LIMIT 1")
        .arg(m_worldDb)
        .arg(fromCreature ? QStringLiteral("creature") : QStringLiteral("gameobject"))
        .arg(qulonglong(guid));

    db::QueryResult res;
    auto const err = m_db->query(sql.toStdString(), res);
    if (!err.ok())
    {
        m_statusLabel->setText(tr("jump query failed: %1")
            .arg(QString::fromStdString(err.message)));
        return;
    }
    if (res.rowCount() == 0)
    {
        m_statusLabel->setText(tr("dependent spawn guid=%1 not found in %2 table")
            .arg(qulonglong(guid))
            .arg(fromCreature ? QStringLiteral("creature") : QStringLiteral("gameobject")));
        return;
    }
    uint32_t const mapId = static_cast<uint32_t>(res.asUInt64(0, 0).value_or(0));
    float    const x     = static_cast<float>(res.asDouble(0, 1).value_or(0.0));
    float    const y     = static_cast<float>(res.asDouble(0, 2).value_or(0.0));
    emit jumpRequested(mapId, x, y);
}

void LinkedRespawnDialog::openModal(bool editing,
                                     uint64_t editingGuid,
                                     uint32_t editingLinkType,
                                     uint64_t editingLinkedGuid)
{
    QDialog dlg(this);
    dlg.setWindowTitle(editing ? tr("Edit linked_respawn row") : tr("Add linked_respawn row"));
    dlg.setModal(true);
    dlg.resize(420, 200);

    auto* outerL = new QVBoxLayout(&dlg);
    auto* form   = new QFormLayout;

    // QSpinBox clamps at INT_MAX; bigint guids can exceed that on modern
    // shards.  Operators editing such guids should use SQL directly --
    // this dialog is for the common < 2^31 case (the live table on a
    // clean extract maxes around ~340K).
    auto* guidSpin = new QSpinBox(&dlg);
    guidSpin->setRange(0, std::numeric_limits<int>::max());
    guidSpin->setValue(static_cast<int>(editingGuid > uint64_t(INT_MAX) ? INT_MAX : editingGuid));
    guidSpin->setEnabled(!editing);    // PK component: immutable on edit.
    form->addRow(tr("guid (dependent spawn):"), guidSpin);

    auto* linkedSpin = new QSpinBox(&dlg);
    linkedSpin->setRange(0, std::numeric_limits<int>::max());
    linkedSpin->setValue(static_cast<int>(editingLinkedGuid > uint64_t(INT_MAX) ? INT_MAX : editingLinkedGuid));
    form->addRow(tr("linkedGuid (trigger spawn):"), linkedSpin);

    auto* linkTypeCombo = new QComboBox(&dlg);
    linkTypeCombo->addItem(linkTypeLabel(0), 0u);
    linkTypeCombo->addItem(linkTypeLabel(1), 1u);
    linkTypeCombo->addItem(linkTypeLabel(2), 2u);
    linkTypeCombo->addItem(linkTypeLabel(3), 3u);
    linkTypeCombo->setCurrentIndex(static_cast<int>(editingLinkType <= 3 ? editingLinkType : 0));
    linkTypeCombo->setEnabled(!editing); // PK component: immutable on edit.
    form->addRow(tr("linkType:"), linkTypeCombo);

    outerL->addLayout(form);

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    outerL->addWidget(btns);

    if (dlg.exec() != QDialog::Accepted)
        return;

    uint64_t const guid       = static_cast<uint64_t>(guidSpin->value());
    uint64_t const linkedGuid = static_cast<uint64_t>(linkedSpin->value());
    uint32_t const linkType   = linkTypeCombo->currentData().toUInt();

    if (editing)
    {
        // UPDATE only changes linkedGuid -- the PK pair (guid, linkType)
        // is immutable for the lifetime of this row.
        QString const upd = QStringLiteral(
            "UPDATE %1.linked_respawn SET linkedGuid=%2 "
            "WHERE guid=%3 AND linkType=%4")
            .arg(m_worldDb)
            .arg(qulonglong(linkedGuid))
            .arg(qulonglong(editingGuid))
            .arg(editingLinkType);
        if (runInTransaction(QStringList{ upd },
                tr("UPDATE linked_respawn (guid=%1, linkType=%2)")
                    .arg(qulonglong(editingGuid)).arg(editingLinkType)))
            loadRows();
    }
    else
    {
        // PK collision check: friendlier than a raw MySQL duplicate-key
        // error.
        QString const checkSql = QStringLiteral(
            "SELECT COUNT(*) FROM %1.linked_respawn WHERE guid=%2 AND linkType=%3")
            .arg(m_worldDb).arg(qulonglong(guid)).arg(linkType);
        db::QueryResult cRes;
        auto err = m_db->query(checkSql.toStdString(), cRes);
        if (err.ok() && cRes.rowCount() > 0 && cRes.asUInt64(0, 0).value_or(0) != 0)
        {
            QMessageBox::warning(this, tr("Add link"),
                tr("linked_respawn row already exists for guid=%1, linkType=%2 (%3).")
                    .arg(qulonglong(guid)).arg(linkType).arg(linkTypeLabel(linkType)));
            return;
        }

        QString const ins = QStringLiteral(
            "INSERT INTO %1.linked_respawn (guid, linkedGuid, linkType) "
            "VALUES (%2, %3, %4)")
            .arg(m_worldDb)
            .arg(qulonglong(guid)).arg(qulonglong(linkedGuid)).arg(linkType);
        if (runInTransaction(QStringList{ ins },
                tr("INSERT linked_respawn (guid=%1, linkType=%2)")
                    .arg(qulonglong(guid)).arg(linkType)))
            loadRows();
    }
}

} // namespace world_editor::app
