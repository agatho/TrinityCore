#include "SpawnSearchDialog.h"

#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <cstdint>
#include <sstream>
#include <string>

namespace world_editor::app
{

namespace
{
// Common NPC flag bits sourced from src/server/game/Entities/Unit/UnitDefines.h
// (UNIT_NPC_FLAG_*).  Keep in sync if the core enum changes.
struct NpcFlagOption
{
    char const* label;
    uint32_t    bit;
};

NpcFlagOption const kNpcFlags[] = {
    { "(any)",         0u           },
    { "Vendor",        0x00000080u  },
    { "Innkeeper",     0x00010000u  },
    { "Mailbox",       0x04000000u  },
    { "Auctioneer",    0x00200000u  },
    { "Trainer",       0x00000010u  },
    { "Flightmaster",  0x00002000u  },
    { "Battlemaster",  0x00100000u  },
    { "Banker",        0x00020000u  },
};

QString esc(db::MySqlClient* c, QString const& v)
{
    if (!c) return v;
    return QString::fromStdString(c->escapeString(v.toStdString()));
}
} // namespace

SpawnSearchDialog::SpawnSearchDialog(db::MySqlClient* dbClient,
                                     uint32_t currentMapId,
                                     QWidget* parent)
    : QDialog(parent), m_dbClient(dbClient), m_currentMapId(currentMapId)
{
    setWindowTitle(tr("Spawn search"));
    setModal(true);
    resize(900, 620);

    // --- Form fields ---------------------------------------------------
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText(tr("name fragment (LIKE %x%) - blank = skip"));

    m_entrySpin = new QSpinBox(this);
    m_entrySpin->setRange(0, 100000000);
    m_entrySpin->setSpecialValueText(tr("(any)"));

    m_npcFlagCombo = new QComboBox(this);
    for (auto const& opt : kNpcFlags)
        m_npcFlagCombo->addItem(QString::fromLatin1(opt.label), QVariant::fromValue(uint(opt.bit)));

    m_factionSpin = new QSpinBox(this);
    m_factionSpin->setRange(0, 100000);
    m_factionSpin->setSpecialValueText(tr("(any)"));

    m_mapSpin = new QSpinBox(this);
    m_mapSpin->setRange(0, 100000);
    m_mapSpin->setSpecialValueText(tr("(any map)"));
    m_mapSpin->setValue(int(m_currentMapId));

    m_itemSpin = new QSpinBox(this);
    m_itemSpin->setRange(0, 100000000);
    m_itemSpin->setSpecialValueText(tr("(any)"));

    m_kindCombo = new QComboBox(this);
    m_kindCombo->addItem(tr("Both creatures and gameobjects"), QStringLiteral("both"));
    m_kindCombo->addItem(tr("Creatures only"),                  QStringLiteral("creature"));
    m_kindCombo->addItem(tr("Gameobjects only"),                QStringLiteral("gameobject"));

    auto* form = new QFormLayout;
    form->addRow(tr("Name LIKE"),     m_nameEdit);
    form->addRow(tr("Entry (id)"),    m_entrySpin);
    form->addRow(tr("NPC flag"),      m_npcFlagCombo);
    form->addRow(tr("Faction"),       m_factionSpin);
    form->addRow(tr("Map id"),        m_mapSpin);
    form->addRow(tr("Drops item id"), m_itemSpin);
    form->addRow(tr("Kind"),          m_kindCombo);

    // --- Buttons ------------------------------------------------------
    m_searchBtn = new QPushButton(tr("Search"), this);
    m_jumpBtn   = new QPushButton(tr("Jump to selected"), this);
    m_jumpBtn->setEnabled(false);

    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch(1);
    btnRow->addWidget(m_searchBtn);
    btnRow->addWidget(m_jumpBtn);

    // --- Results table -----------------------------------------------
    m_results = new QTableWidget(this);
    m_results->setColumnCount(8);
    m_results->setHorizontalHeaderLabels(
        { tr("kind"), tr("guid"), tr("entry"), tr("name"),
          tr("map"), tr("x"), tr("y"), tr("z") });
    m_results->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_results->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_results->setSelectionMode(QAbstractItemView::SingleSelection);
    m_results->setSortingEnabled(true);
    m_results->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_results->horizontalHeader()->setStretchLastSection(true);

    m_statusLbl = new QLabel(QString{}, this);

    auto* close = new QDialogButtonBox(QDialogButtonBox::Close, this);

    auto* outer = new QVBoxLayout(this);
    outer->addLayout(form);
    outer->addLayout(btnRow);
    outer->addWidget(m_results, 1);
    outer->addWidget(m_statusLbl);
    outer->addWidget(close);

    // --- Wiring -------------------------------------------------------
    connect(m_searchBtn,  &QPushButton::clicked,     this, &SpawnSearchDialog::onSearch);
    connect(m_nameEdit,   &QLineEdit::returnPressed, this, &SpawnSearchDialog::onSearch);
    connect(m_jumpBtn,    &QPushButton::clicked,     this, &SpawnSearchDialog::onJumpSelected);
    connect(close,        &QDialogButtonBox::rejected, this, &QDialog::accept);
    connect(m_results,    &QTableWidget::itemSelectionChanged, this, [this]() {
        m_jumpBtn->setEnabled(!m_results->selectedItems().isEmpty());
    });
    connect(m_results,    &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        emitJumpFromRow(row);
    });
}

void SpawnSearchDialog::onSearch()
{
    if (!m_dbClient || !m_dbClient->isConnected())
    {
        m_statusLbl->setText(tr("not connected to DB"));
        return;
    }

    m_results->setSortingEnabled(false);
    m_results->setRowCount(0);

    QString const   nameFrag   = m_nameEdit->text().trimmed();
    uint32_t const  entryFilt  = uint32_t(m_entrySpin->value());
    uint32_t const  npcFlagBit = m_npcFlagCombo->currentData().toUInt();
    uint32_t const  factionFlt = uint32_t(m_factionSpin->value());
    uint32_t const  mapFilt    = uint32_t(m_mapSpin->value());
    uint32_t const  itemFilt   = uint32_t(m_itemSpin->value());
    QString const   kind       = m_kindCombo->currentData().toString();

    bool const wantCreature   = (kind != QStringLiteral("gameobject"));
    bool const wantGameobject = (kind != QStringLiteral("creature"));

    // NPC flag / faction only make sense for creatures; if they're set
    // and the operator excluded creatures, we silently drop GO results
    // anyway since those columns don't exist on gameobject_template.
    bool const creatureOnlyFilters =
        (npcFlagBit != 0u) || (factionFlt != 0u);

    size_t totalRows = 0;
    std::string safeName = nameFrag.isEmpty()
        ? std::string{}
        : esc(m_dbClient, nameFrag).toStdString();

    auto runOne = [&](bool isCreature) -> size_t
    {
        // TC schema notes:
        //   creature.faction does NOT exist; faction lives on
        //     creature_template.faction, so the spawn join carries it.
        //   creature lootid lives on creature_template_difficulty.LootID
        //     (keyed by Entry, with DifficultyID=0 the base row).
        //   gameobject lootid is type-dependent (Data1 for chests +
        //     fishing nodes covers the common cases); we match Data1.
        char const* spawnTable = isCreature ? "creature"          : "gameobject";
        char const* tplTable   = isCreature ? "creature_template" : "gameobject_template";
        char const* kindLabel  = isCreature ? "creature"          : "gameobject";

        std::ostringstream sql;
        sql << "SELECT s.guid, s.id, tpl.name, s.map, s.position_x, s.position_y, s.position_z "
            << "FROM " << spawnTable << " s "
            << "JOIN "  << tplTable   << " tpl ON tpl.entry = s.id";

        std::ostringstream where;
        bool first = true;
        auto addClause = [&](std::string const& clause) {
            where << (first ? " WHERE " : " AND ") << clause;
            first = false;
        };

        if (!safeName.empty())
            addClause("tpl.name LIKE '%" + safeName + "%'");
        if (entryFilt != 0u)
            addClause("s.id = " + std::to_string(entryFilt));
        if (mapFilt != 0u)
            addClause("s.map = " + std::to_string(mapFilt));
        if (isCreature && npcFlagBit != 0u)
            addClause("(tpl.npcflag & " + std::to_string(npcFlagBit) + ") <> 0");
        if (isCreature && factionFlt != 0u)
            addClause("tpl.faction = " + std::to_string(factionFlt));
        if (itemFilt != 0u)
        {
            std::string ex;
            if (isCreature)
            {
                ex = "EXISTS (SELECT 1 FROM creature_template_difficulty d "
                     "JOIN creature_loot_template lt ON lt.Entry = d.LootID "
                     "WHERE d.Entry = tpl.entry AND lt.Item = "
                   + std::to_string(itemFilt) + ")";
            }
            else
            {
                ex = "EXISTS (SELECT 1 FROM gameobject_loot_template lt "
                     "WHERE lt.Entry = tpl.Data1 AND lt.Item = "
                   + std::to_string(itemFilt) + ")";
            }
            addClause(ex);
        }

        sql << where.str();
        sql << " ORDER BY s.map, s.guid LIMIT 500";

        db::QueryResult res;
        auto const err = m_dbClient->query(sql.str(), res);
        if (!err.ok())
            return 0;

        size_t added = 0;
        for (size_t r = 0; r < res.rowCount(); ++r)
        {
            int const rowIdx = m_results->rowCount();
            m_results->insertRow(rowIdx);

            auto setText = [&](int col, QString const& text) {
                auto* it = new QTableWidgetItem(text);
                m_results->setItem(rowIdx, col, it);
            };
            auto setNumber = [&](int col, double v, int prec = 0) {
                auto* it = new QTableWidgetItem();
                it->setData(Qt::DisplayRole, prec > 0
                    ? QVariant(QString::number(v, 'f', prec))
                    : QVariant(qlonglong(v)));
                m_results->setItem(rowIdx, col, it);
            };

            setText  (0, QString::fromLatin1(kindLabel));
            setNumber(1, double(res.asInt64 (r, 0).value_or(0)));
            setNumber(2, double(res.asUInt64(r, 1).value_or(0)));
            setText  (3, QString::fromStdString(res.cell(r, 2)));
            setNumber(4, double(res.asUInt64(r, 3).value_or(0)));
            setNumber(5, res.asDouble(r, 4).value_or(0.0), 2);
            setNumber(6, res.asDouble(r, 5).value_or(0.0), 2);
            setNumber(7, res.asDouble(r, 6).value_or(0.0), 2);
            ++added;
        }
        return added;
    };

    QApplication::setOverrideCursor(Qt::WaitCursor);
    size_t nC = 0, nG = 0;
    if (wantCreature)
        nC = runOne(true);
    if (wantGameobject && !creatureOnlyFilters)
        nG = runOne(false);
    QApplication::restoreOverrideCursor();

    totalRows = nC + nG;
    m_results->setSortingEnabled(true);
    if (creatureOnlyFilters && wantGameobject)
        m_statusLbl->setText(tr("creatures=%1  (gameobjects skipped: npcflag/faction filters set)").arg(nC));
    else
        m_statusLbl->setText(tr("creatures=%1  gameobjects=%2  total=%3").arg(nC).arg(nG).arg(totalRows));
}

void SpawnSearchDialog::onJumpSelected()
{
    auto const sel = m_results->selectionModel()->selectedRows();
    if (sel.isEmpty()) return;
    emitJumpFromRow(sel.front().row());
}

void SpawnSearchDialog::emitJumpFromRow(int row)
{
    if (row < 0 || row >= m_results->rowCount()) return;
    auto* guidItem = m_results->item(row, 1);
    auto* mapItem  = m_results->item(row, 4);
    auto* xItem    = m_results->item(row, 5);
    auto* yItem    = m_results->item(row, 6);
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
