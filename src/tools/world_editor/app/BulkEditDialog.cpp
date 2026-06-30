#include "BulkEditDialog.h"

#include "UndoManager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>

namespace world_editor::app
{

namespace
{
QString hex64(uint64_t v) { return QStringLiteral("0x%1").arg(v, 0, 16); }
} // namespace

uint64_t BulkEditDialog::parseHexOrDec(QString const& s, uint64_t fallback) const
{
    bool ok = false;
    QString t = s.trimmed();
    if (t.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive))
    {
        uint64_t v = t.mid(2).toULongLong(&ok, 16);
        return ok ? v : fallback;
    }
    uint64_t v = t.toULongLong(&ok, 10);
    return ok ? v : fallback;
}

BulkEditDialog::BulkEditDialog(db::SpawnModel& model,
                               QVector<int> const& selectionIndices,
                               QWidget* parent,
                               UndoManager* undo)
    : QDialog(parent), m_model(model), m_selection(selectionIndices), m_undo(undo)
{
    setWindowTitle(tr("Bulk edit"));
    setModal(true);
    resize(560, 820);

    // Seed each field with the first selected row's value so the operator
    // sees what they're starting from.
    render::Spawn seed;
    if (!m_selection.isEmpty() && m_selection[0] < int(m_model.current().size()))
        seed = m_model.current()[m_selection[0]];

    auto* form = new QFormLayout;

    auto rowSpin = [&](char const* name, int min, int max, int value, QSpinBox*& outWidget, QCheckBox*& outEn) {
        outEn     = new QCheckBox(this);
        outWidget = new QSpinBox(this);
        outWidget->setRange(min, max);
        outWidget->setValue(value);
        auto* h = new QHBoxLayout;
        h->addWidget(outEn);
        h->addWidget(outWidget, 1);
        auto* wrap = new QWidget(this);
        wrap->setLayout(h);
        form->addRow(QString::fromLatin1(name), wrap);
    };
    auto rowLineEdit = [&](char const* name, QString const& value, int maxLen, QLineEdit*& outWidget, QCheckBox*& outEn) {
        outEn     = new QCheckBox(this);
        outWidget = new QLineEdit(this);
        outWidget->setText(value);
        outWidget->setMaxLength(maxLen);
        auto* h = new QHBoxLayout;
        h->addWidget(outEn);
        h->addWidget(outWidget, 1);
        auto* wrap = new QWidget(this);
        wrap->setLayout(h);
        form->addRow(QString::fromLatin1(name), wrap);
    };

    rowSpin("spawntimesecs",   0, 604800, int(seed.spawntimesecs),  m_spawntime,    m_enSpawntime);

    m_enMovement = new QCheckBox(this);
    m_movement = new QComboBox(this);
    m_movement->addItem(QStringLiteral("0 - Idle"),     0);
    m_movement->addItem(QStringLiteral("1 - Random"),   1);
    m_movement->addItem(QStringLiteral("2 - Waypoint"), 2);
    m_movement->setCurrentIndex(seed.movementType < 3 ? int(seed.movementType) : 0);
    {
        auto* h = new QHBoxLayout;
        h->addWidget(m_enMovement);
        h->addWidget(m_movement, 1);
        auto* wrap = new QWidget(this);
        wrap->setLayout(h);
        form->addRow(QStringLiteral("MovementType"), wrap);
    }

    rowSpin("phaseUseFlags",  0,     255,    int(seed.phaseUseFlags), m_phaseUse,    m_enPhaseUse);
    rowSpin("PhaseId",        0, INT_MAX,    int(seed.phaseId),       m_phaseId,     m_enPhaseId);
    rowSpin("PhaseGroup",     0, INT_MAX,    int(seed.phaseGroup),    m_phaseGroup,  m_enPhaseGroup);
    rowSpin("terrainSwapMap", -1, INT_MAX,   seed.terrainSwapMap,     m_terrainSwap, m_enTerrainSwap);
    rowLineEdit("spawnDifficulties", seed.spawnDifficulties, 100, m_difficulties, m_enDifficulties);
    rowLineEdit("ScriptName",        seed.scriptName,         64, m_scriptName,   m_enScriptName);
    rowLineEdit("StringId",          seed.stringId,           64, m_stringId,     m_enStringId);
    rowLineEdit("npcflag (hex or dec)",     hex64(seed.npcflag),    32, m_npcflag,    m_enNpcflag);
    rowLineEdit("unit_flags (hex or dec)",  hex64(seed.unitFlags1), 32, m_unitFlags1, m_enUnitFlags1);
    rowLineEdit("unit_flags2 (hex or dec)", hex64(seed.unitFlags2), 32, m_unitFlags2, m_enUnitFlags2);
    rowLineEdit("unit_flags3 (hex or dec)", hex64(seed.unitFlags3), 32, m_unitFlags3, m_enUnitFlags3);
    rowSpin("curHealthPct",     0,   100,  int(seed.curHealthPct), m_curHealth,  m_enCurHealth);
    rowSpin("(GO) state",       0,   255,  int(seed.goState),      m_goState,    m_enGoState);
    rowSpin("(GO) animprogress",0,   255,  int(seed.animprogress), m_animprog,   m_enAnimprog);

    // Respawn-time group: set/multiply/add on spawntimesecs, mutually exclusive.
    auto* respawnGroup = new QGroupBox(tr("Respawn time"), this);
    {
        auto* gl = new QFormLayout(respawnGroup);

        m_enRespawnSet = new QCheckBox(tr("Set spawntimesecs to"), respawnGroup);
        m_respawnSet   = new QSpinBox(respawnGroup);
        m_respawnSet->setRange(5, 3600000);
        m_respawnSet->setValue(120);
        gl->addRow(m_enRespawnSet, m_respawnSet);

        m_enRespawnMul = new QCheckBox(tr("Multiply by factor"), respawnGroup);
        m_respawnMul   = new QDoubleSpinBox(respawnGroup);
        m_respawnMul->setRange(0.1, 10.0);
        m_respawnMul->setSingleStep(0.1);
        m_respawnMul->setDecimals(2);
        m_respawnMul->setValue(1.0);
        gl->addRow(m_enRespawnMul, m_respawnMul);

        m_enRespawnAdd = new QCheckBox(tr("Add seconds"), respawnGroup);
        m_respawnAdd   = new QSpinBox(respawnGroup);
        m_respawnAdd->setRange(-3600000, 3600000);
        m_respawnAdd->setValue(0);
        gl->addRow(m_enRespawnAdd, m_respawnAdd);
    }
    // Radio-like behavior across the three checkboxes: enabling one clears
    // the others.  Checkboxes (not radios) so the value spinboxes stay
    // legible side-by-side.
    auto mutexRespawn = [this](QCheckBox* on) {
        QCheckBox* boxes[3] = { m_enRespawnSet, m_enRespawnMul, m_enRespawnAdd };
        for (QCheckBox* b : boxes)
            if (b != on && b->isChecked())
                b->setChecked(false);
    };
    connect(m_enRespawnSet, &QCheckBox::toggled, this, [this, mutexRespawn](bool on) { if (on) mutexRespawn(m_enRespawnSet); });
    connect(m_enRespawnMul, &QCheckBox::toggled, this, [this, mutexRespawn](bool on) { if (on) mutexRespawn(m_enRespawnMul); });
    connect(m_enRespawnAdd, &QCheckBox::toggled, this, [this, mutexRespawn](bool on) { if (on) mutexRespawn(m_enRespawnAdd); });

    // Phase group: three independent setters.  Distinct from the existing
    // top-of-form Phase* rows so the operator can do a one-shot "set
    // phaseId=N" batch without scrolling.
    auto* phaseGroup = new QGroupBox(tr("Phase"), this);
    {
        auto* gl = new QFormLayout(phaseGroup);

        m_enSetPhaseId = new QCheckBox(tr("Set phaseId to"), phaseGroup);
        m_setPhaseId   = new QSpinBox(phaseGroup);
        m_setPhaseId->setRange(0, INT_MAX);
        m_setPhaseId->setValue(0);
        gl->addRow(m_enSetPhaseId, m_setPhaseId);

        m_enSetPhaseGroup = new QCheckBox(tr("Set phaseGroup to"), phaseGroup);
        m_setPhaseGroup   = new QSpinBox(phaseGroup);
        m_setPhaseGroup->setRange(0, INT_MAX);
        m_setPhaseGroup->setValue(0);
        gl->addRow(m_enSetPhaseGroup, m_setPhaseGroup);

        m_enSetPhaseUse = new QCheckBox(tr("Set phaseUseFlags to"), phaseGroup);
        m_setPhaseUse   = new QSpinBox(phaseGroup);
        m_setPhaseUse->setRange(0, 255);
        m_setPhaseUse->setValue(0);
        gl->addRow(m_enSetPhaseUse, m_setPhaseUse);
    }

    m_summary = new QLabel(tr("Will apply to %1 selected spawn(s).  Check a box to enable that field.")
        .arg(m_selection.size()), this);
    m_summary->setWordWrap(true);

    auto* buttons = new QDialogButtonBox(this);
    m_applyButton = buttons->addButton(tr("Apply"), QDialogButtonBox::AcceptRole);
    buttons->addButton(QDialogButtonBox::Cancel);
    connect(m_applyButton, &QPushButton::clicked, this, &BulkEditDialog::onApply);
    connect(buttons,       &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* outer = new QVBoxLayout(this);
    outer->addWidget(m_summary);
    outer->addLayout(form, 1);
    outer->addWidget(respawnGroup);
    outer->addWidget(phaseGroup);
    outer->addWidget(buttons);
}

void BulkEditDialog::onApply()
{
    if (m_selection.isEmpty())
    {
        reject();
        return;
    }

    // Per-row mutate closure shared between the undo-wrapped and bare paths.
    auto applyAll = [this]() {
        m_rowsTouched = 0;
        for (int srcIdx : m_selection)
        {
            if (srcIdx < 0 || srcIdx >= int(m_model.current().size()))
                continue;
            render::Spawn row = m_model.current()[srcIdx];

            if (m_enSpawntime->isChecked())    row.spawntimesecs    = uint32_t(m_spawntime->value());
            if (m_enMovement->isChecked())     row.movementType     = uint8_t(m_movement->currentData().toInt());
            if (m_enPhaseUse->isChecked())     row.phaseUseFlags    = uint8_t(m_phaseUse->value());
            if (m_enPhaseId->isChecked())      row.phaseId          = uint32_t(m_phaseId->value());
            if (m_enPhaseGroup->isChecked())   row.phaseGroup       = uint32_t(m_phaseGroup->value());
            if (m_enTerrainSwap->isChecked())  row.terrainSwapMap   = m_terrainSwap->value();
            if (m_enDifficulties->isChecked()) row.spawnDifficulties = m_difficulties->text();
            if (m_enScriptName->isChecked())   row.scriptName       = m_scriptName->text();
            if (m_enStringId->isChecked())     row.stringId         = m_stringId->text();
            if (m_enNpcflag->isChecked())      row.npcflag          = parseHexOrDec(m_npcflag->text(),   row.npcflag);
            if (m_enUnitFlags1->isChecked())   row.unitFlags1       = uint32_t(parseHexOrDec(m_unitFlags1->text(), row.unitFlags1));
            if (m_enUnitFlags2->isChecked())   row.unitFlags2       = uint32_t(parseHexOrDec(m_unitFlags2->text(), row.unitFlags2));
            if (m_enUnitFlags3->isChecked())   row.unitFlags3       = uint32_t(parseHexOrDec(m_unitFlags3->text(), row.unitFlags3));
            if (m_enCurHealth->isChecked())    row.curHealthPct     = uint32_t(m_curHealth->value());
            if (m_enGoState->isChecked())      row.goState          = uint8_t(m_goState->value());
            if (m_enAnimprog->isChecked())     row.animprogress     = uint8_t(m_animprog->value());

            // Respawn-time group: only the first checked op fires (radio-like).
            if (m_enRespawnSet->isChecked())
            {
                row.spawntimesecs = uint32_t(m_respawnSet->value());
            }
            else if (m_enRespawnMul->isChecked())
            {
                double const scaled = double(row.spawntimesecs) * m_respawnMul->value();
                // Clamp to the same 5..3600000 envelope as the Set spinbox.
                long long const clamped = std::clamp<long long>(static_cast<long long>(scaled + 0.5), 5LL, 3600000LL);
                row.spawntimesecs = uint32_t(clamped);
            }
            else if (m_enRespawnAdd->isChecked())
            {
                long long const added = static_cast<long long>(row.spawntimesecs) + m_respawnAdd->value();
                long long const clamped = std::clamp<long long>(added, 5LL, 3600000LL);
                row.spawntimesecs = uint32_t(clamped);
            }

            // Phase group: independent setters.
            if (m_enSetPhaseId->isChecked())    row.phaseId       = uint32_t(m_setPhaseId->value());
            if (m_enSetPhaseGroup->isChecked()) row.phaseGroup    = uint32_t(m_setPhaseGroup->value());
            if (m_enSetPhaseUse->isChecked())   row.phaseUseFlags = uint8_t(m_setPhaseUse->value());

            if (m_model.replaceRow(srcIdx, row))
                ++m_rowsTouched;
        }
    };

    // Single recordOn frame so Ctrl+Z reverses the whole batch atomically.
    if (m_undo)
        m_undo->recordOn(&m_model, tr("Bulk edit %1 spawns").arg(m_selection.size()), applyAll);
    else
        applyAll();

    accept();
}

} // namespace world_editor::app
