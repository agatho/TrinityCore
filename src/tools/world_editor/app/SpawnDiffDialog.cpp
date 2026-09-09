/*
 * SpawnDiffDialog implementation.  All values for each field are stringified
 * the same way for both spawns (so a difference table never reports a false
 * positive driven by formatting alone) and identical rows are still listed
 * so the operator gets a stable mental map of where each field lives.
 */

#include "SpawnDiffDialog.h"

#include <QBrush>
#include <QColor>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <cstdint>

namespace world_editor::app
{
namespace
{

// Stable formatters keyed off field semantics so A vs B compares apples
// to apples (e.g. a float 0.1 + tiny epsilon shouldn't read as different
// because of QString::number default precision).
QString fmtF(float v, int prec)
{
    return QString::number(static_cast<double>(v), 'f', prec);
}

QString fmtU32(uint32_t v)
{
    return QString::number(v);
}

QString fmtI32(int32_t v)
{
    return QString::number(v);
}

QString fmtU64Hex(uint64_t v)
{
    return QStringLiteral("0x") + QString::number(v, 16).toUpper();
}

QString fmtU32Hex(uint32_t v)
{
    return QStringLiteral("0x") + QString::number(static_cast<qulonglong>(v), 16).toUpper();
}

QString fmtKind(render::SpawnKind k)
{
    return k == render::SpawnKind::Creature ? QStringLiteral("Creature") : QStringLiteral("GameObject");
}

QString fmtStr(QString const& s)
{
    return s;
}

struct Row
{
    QString field;
    QString a;
    QString b;
};

// Single source of truth for which fields are surfaced.  Order mirrors the
// declaration order in render::Spawn so the dialog reads top-to-bottom like
// the struct.
std::vector<Row> buildRows(render::Spawn const& a, render::Spawn const& b)
{
    std::vector<Row> rows;
    rows.reserve(40);

    rows.push_back({QStringLiteral("kind"),               fmtKind(a.kind),               fmtKind(b.kind)});
    rows.push_back({QStringLiteral("guid"),               QString::number(a.guid),       QString::number(b.guid)});
    rows.push_back({QStringLiteral("entry"),              fmtU32(a.entry),               fmtU32(b.entry)});
    rows.push_back({QStringLiteral("mapId"),              fmtU32(a.mapId),               fmtU32(b.mapId)});
    rows.push_back({QStringLiteral("zoneId"),             fmtU32(a.zoneId),              fmtU32(b.zoneId)});
    rows.push_back({QStringLiteral("areaId"),             fmtU32(a.areaId),              fmtU32(b.areaId)});
    rows.push_back({QStringLiteral("spawnDifficulties"),  fmtStr(a.spawnDifficulties),   fmtStr(b.spawnDifficulties)});
    rows.push_back({QStringLiteral("phaseUseFlags"),      fmtU32(a.phaseUseFlags),       fmtU32(b.phaseUseFlags)});
    rows.push_back({QStringLiteral("phaseId"),            fmtU32(a.phaseId),             fmtU32(b.phaseId)});
    rows.push_back({QStringLiteral("phaseGroup"),         fmtU32(a.phaseGroup),          fmtU32(b.phaseGroup)});
    rows.push_back({QStringLiteral("terrainSwapMap"),     fmtI32(a.terrainSwapMap),      fmtI32(b.terrainSwapMap)});
    rows.push_back({QStringLiteral("spawntimesecs"),      fmtU32(a.spawntimesecs),       fmtU32(b.spawntimesecs)});

    rows.push_back({QStringLiteral("worldX"),             fmtF(a.worldX, 3),             fmtF(b.worldX, 3)});
    rows.push_back({QStringLiteral("worldY"),             fmtF(a.worldY, 3),             fmtF(b.worldY, 3)});
    rows.push_back({QStringLiteral("worldZ"),             fmtF(a.worldZ, 3),             fmtF(b.worldZ, 3)});
    rows.push_back({QStringLiteral("orientation"),        fmtF(a.orientation, 4),        fmtF(b.orientation, 4)});

    rows.push_back({QStringLiteral("modelid"),            fmtU32(a.modelid),             fmtU32(b.modelid)});
    rows.push_back({QStringLiteral("equipmentId"),        fmtU32(a.equipmentId),         fmtU32(b.equipmentId)});
    rows.push_back({QStringLiteral("wanderDistance"),     fmtF(a.wanderDistance, 1),     fmtF(b.wanderDistance, 1)});
    rows.push_back({QStringLiteral("currentwaypoint"),    fmtU32(a.currentwaypoint),     fmtU32(b.currentwaypoint)});
    rows.push_back({QStringLiteral("curHealthPct"),       fmtU32(a.curHealthPct),        fmtU32(b.curHealthPct)});
    rows.push_back({QStringLiteral("movementType"),       fmtU32(a.movementType),        fmtU32(b.movementType)});

    rows.push_back({QStringLiteral("npcflag"),            fmtU64Hex(a.npcflag),          fmtU64Hex(b.npcflag)});
    rows.push_back({QStringLiteral("unitFlags1"),         fmtU32Hex(a.unitFlags1),       fmtU32Hex(b.unitFlags1)});
    rows.push_back({QStringLiteral("unitFlags2"),         fmtU32Hex(a.unitFlags2),       fmtU32Hex(b.unitFlags2)});
    rows.push_back({QStringLiteral("unitFlags3"),         fmtU32Hex(a.unitFlags3),       fmtU32Hex(b.unitFlags3)});

    rows.push_back({QStringLiteral("rotation0"),          fmtF(a.rotation0, 4),          fmtF(b.rotation0, 4)});
    rows.push_back({QStringLiteral("rotation1"),          fmtF(a.rotation1, 4),          fmtF(b.rotation1, 4)});
    rows.push_back({QStringLiteral("rotation2"),          fmtF(a.rotation2, 4),          fmtF(b.rotation2, 4)});
    rows.push_back({QStringLiteral("rotation3"),          fmtF(a.rotation3, 4),          fmtF(b.rotation3, 4)});
    rows.push_back({QStringLiteral("animprogress"),       fmtU32(a.animprogress),        fmtU32(b.animprogress)});
    rows.push_back({QStringLiteral("goState"),            fmtU32(a.goState),             fmtU32(b.goState)});

    rows.push_back({QStringLiteral("scriptName"),         fmtStr(a.scriptName),          fmtStr(b.scriptName)});
    rows.push_back({QStringLiteral("stringId"),           fmtStr(a.stringId),            fmtStr(b.stringId)});
    rows.push_back({QStringLiteral("verifiedBuild"),      fmtU32(a.verifiedBuild),       fmtU32(b.verifiedBuild)});

    return rows;
}

} // namespace

SpawnDiffDialog::SpawnDiffDialog(render::Spawn const& a, render::Spawn const& b, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Spawn diff"));
    resize(720, 640);

    auto* root = new QVBoxLayout(this);

    QString const headerText = tr("Spawn A: guid %1 entry %2   |   Spawn B: guid %3 entry %4")
        .arg(a.guid)
        .arg(a.entry)
        .arg(b.guid)
        .arg(b.entry);
    auto* header = new QLabel(headerText, this);
    header->setTextInteractionFlags(Qt::TextSelectableByMouse);
    root->addWidget(header);

    std::vector<Row> const rows = buildRows(a, b);

    auto* table = new QTableWidget(static_cast<int>(rows.size()), 3, this);
    table->setHorizontalHeaderLabels({tr("Field"), tr("Spawn A"), tr("Spawn B")});
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setAlternatingRowColors(true);

    QBrush const diffBrush(QColor(0xFF, 0xF7, 0xC0)); // light yellow

    for (int r = 0; r < static_cast<int>(rows.size()); ++r)
    {
        Row const& row = rows[static_cast<size_t>(r)];
        auto* cField = new QTableWidgetItem(row.field);
        auto* cA     = new QTableWidgetItem(row.a);
        auto* cB     = new QTableWidgetItem(row.b);

        if (row.a != row.b)
        {
            cField->setBackground(diffBrush);
            cA->setBackground(diffBrush);
            cB->setBackground(diffBrush);
        }

        table->setItem(r, 0, cField);
        table->setItem(r, 1, cA);
        table->setItem(r, 2, cB);
    }

    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    root->addWidget(table, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    root->addWidget(buttons);
}

} // namespace world_editor::app
