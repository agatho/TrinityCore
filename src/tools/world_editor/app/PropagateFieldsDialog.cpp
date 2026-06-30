#include "PropagateFieldsDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace world_editor::app
{

// Field-name tokens.  Plain ASCII; intended to be cheap to copy across the
// signal payload and stable enough to write down in tests.
QString const PropagateFieldsDialog::kSpawntimesecs     = QStringLiteral("spawntimesecs");
QString const PropagateFieldsDialog::kPhaseUseFlags     = QStringLiteral("phaseUseFlags");
QString const PropagateFieldsDialog::kPhaseId           = QStringLiteral("phaseId");
QString const PropagateFieldsDialog::kPhaseGroup        = QStringLiteral("phaseGroup");
QString const PropagateFieldsDialog::kSpawnDifficulties = QStringLiteral("spawnDifficulties");
QString const PropagateFieldsDialog::kNpcflag           = QStringLiteral("npcflag");
QString const PropagateFieldsDialog::kUnitFlags1        = QStringLiteral("unitFlags1");
QString const PropagateFieldsDialog::kUnitFlags2        = QStringLiteral("unitFlags2");
QString const PropagateFieldsDialog::kUnitFlags3        = QStringLiteral("unitFlags3");
QString const PropagateFieldsDialog::kMovementType      = QStringLiteral("movementType");
QString const PropagateFieldsDialog::kModelid           = QStringLiteral("modelid");
QString const PropagateFieldsDialog::kEquipmentId       = QStringLiteral("equipmentId");
QString const PropagateFieldsDialog::kRotation0         = QStringLiteral("rotation0");
QString const PropagateFieldsDialog::kRotation1         = QStringLiteral("rotation1");
QString const PropagateFieldsDialog::kRotation2         = QStringLiteral("rotation2");
QString const PropagateFieldsDialog::kRotation3         = QStringLiteral("rotation3");
QString const PropagateFieldsDialog::kGoState           = QStringLiteral("goState");
QString const PropagateFieldsDialog::kAnimprogress      = QStringLiteral("animprogress");
QString const PropagateFieldsDialog::kScriptName        = QStringLiteral("scriptName");
QString const PropagateFieldsDialog::kStringId          = QStringLiteral("stringId");
QString const PropagateFieldsDialog::kCurHealthPct      = QStringLiteral("curHealthPct");
QString const PropagateFieldsDialog::kWanderDistance    = QStringLiteral("wanderDistance");

QCheckBox* PropagateFieldsDialog::addCheck(QString const& label, QString const& token, bool defaultOn)
{
    auto* cb = new QCheckBox(label, this);
    cb->setChecked(defaultOn);
    m_checks.insert(token, cb);
    return cb;
}

PropagateFieldsDialog::PropagateFieldsDialog(render::Spawn const& canonical,
                                             QVector<render::Spawn> const& receivers,
                                             QWidget* parent)
    : QDialog(parent), m_canonical(canonical), m_receivers(receivers)
{
    setWindowTitle(tr("Propagate fields"));
    setModal(true);
    resize(540, 700);

    bool const isCreature = (m_canonical.kind == render::SpawnKind::Creature);
    bool const isGO       = (m_canonical.kind == render::SpawnKind::GameObject);

    // Header line - exactly the wording requested in the spec.
    m_header = new QLabel(this);
    m_header->setWordWrap(true);
    m_header->setText(tr("Propagate fields from canonical spawn guid %1 entry %2 to %3 other spawn(s) of entry %2 on map %4.")
        .arg(m_canonical.guid)
        .arg(m_canonical.entry)
        .arg(m_receivers.size())
        .arg(m_canonical.mapId));

    // Shared (creature + GO) field group.
    auto* sharedGroup = new QGroupBox(tr("Shared fields"), this);
    {
        auto* gl = new QVBoxLayout(sharedGroup);
        gl->addWidget(addCheck(tr("spawntimesecs (%1)").arg(m_canonical.spawntimesecs),
                               kSpawntimesecs, /*defaultOn=*/true));
        gl->addWidget(addCheck(tr("phaseUseFlags (%1)").arg(m_canonical.phaseUseFlags),
                               kPhaseUseFlags, true));
        gl->addWidget(addCheck(tr("phaseId (%1)").arg(m_canonical.phaseId),
                               kPhaseId, true));
        gl->addWidget(addCheck(tr("phaseGroup (%1)").arg(m_canonical.phaseGroup),
                               kPhaseGroup, true));
        gl->addWidget(addCheck(tr("spawnDifficulties (\"%1\")").arg(m_canonical.spawnDifficulties),
                               kSpawnDifficulties, true));
        gl->addWidget(addCheck(tr("scriptName (\"%1\")").arg(m_canonical.scriptName),
                               kScriptName, false));
        gl->addWidget(addCheck(tr("stringId (\"%1\")").arg(m_canonical.stringId),
                               kStringId, false));
    }

    // Creature-only group.
    auto* creatureGroup = new QGroupBox(tr("Creature-only fields"), this);
    {
        auto* gl = new QVBoxLayout(creatureGroup);
        gl->addWidget(addCheck(tr("npcflag (0x%1)").arg(m_canonical.npcflag, 0, 16),
                               kNpcflag, true));
        gl->addWidget(addCheck(tr("unitFlags1 (0x%1)").arg(m_canonical.unitFlags1, 0, 16),
                               kUnitFlags1, true));
        gl->addWidget(addCheck(tr("unitFlags2 (0x%1)").arg(m_canonical.unitFlags2, 0, 16),
                               kUnitFlags2, true));
        gl->addWidget(addCheck(tr("unitFlags3 (0x%1)").arg(m_canonical.unitFlags3, 0, 16),
                               kUnitFlags3, true));
        gl->addWidget(addCheck(tr("movementType (%1)").arg(m_canonical.movementType),
                               kMovementType, true));
        gl->addWidget(addCheck(tr("modelid (%1)").arg(m_canonical.modelid),
                               kModelid, true));
        gl->addWidget(addCheck(tr("equipmentId (%1)").arg(m_canonical.equipmentId),
                               kEquipmentId, true));
        gl->addWidget(addCheck(tr("curHealthPct (%1)").arg(m_canonical.curHealthPct),
                               kCurHealthPct, false));
        gl->addWidget(addCheck(tr("wanderDistance (%1)").arg(double(m_canonical.wanderDistance), 0, 'f', 2),
                               kWanderDistance, false));
    }
    creatureGroup->setVisible(isCreature);

    // GO-only group.
    auto* goGroup = new QGroupBox(tr("GameObject-only fields"), this);
    {
        auto* gl = new QVBoxLayout(goGroup);
        gl->addWidget(addCheck(tr("rotation0 (%1)").arg(double(m_canonical.rotation0), 0, 'f', 4),
                               kRotation0, true));
        gl->addWidget(addCheck(tr("rotation1 (%1)").arg(double(m_canonical.rotation1), 0, 'f', 4),
                               kRotation1, true));
        gl->addWidget(addCheck(tr("rotation2 (%1)").arg(double(m_canonical.rotation2), 0, 'f', 4),
                               kRotation2, true));
        gl->addWidget(addCheck(tr("rotation3 (%1)").arg(double(m_canonical.rotation3), 0, 'f', 4),
                               kRotation3, true));
        gl->addWidget(addCheck(tr("goState (%1)").arg(m_canonical.goState),
                               kGoState, true));
        gl->addWidget(addCheck(tr("animprogress (%1)").arg(m_canonical.animprogress),
                               kAnimprogress, true));
    }
    goGroup->setVisible(isGO);

    // Stuff the checkbox columns inside a scroll area in case the field
    // list grows past the dialog height (it already nearly fills 700px).
    auto* innerWidget = new QWidget(this);
    auto* innerLayout = new QVBoxLayout(innerWidget);
    innerLayout->addWidget(sharedGroup);
    innerLayout->addWidget(creatureGroup);
    innerLayout->addWidget(goGroup);
    innerLayout->addStretch(1);

    auto* scroll = new QScrollArea(this);
    scroll->setWidget(innerWidget);
    scroll->setWidgetResizable(true);

    // Footer: Preview / Apply / Cancel.  Preview is a "Reset" role so the
    // button box doesn't auto-close the dialog on click.
    auto* buttons = new QDialogButtonBox(this);
    auto* previewButton = buttons->addButton(tr("Preview affected rows"), QDialogButtonBox::ResetRole);
    m_applyButton       = buttons->addButton(tr("Apply"), QDialogButtonBox::AcceptRole);
    buttons->addButton(QDialogButtonBox::Cancel);
    m_applyButton->setEnabled(!m_receivers.isEmpty());

    connect(previewButton, &QPushButton::clicked, this, &PropagateFieldsDialog::onPreviewAffected);
    connect(m_applyButton, &QPushButton::clicked, this, &PropagateFieldsDialog::onApply);
    connect(buttons,       &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* outer = new QVBoxLayout(this);
    outer->addWidget(m_header);
    outer->addWidget(scroll, 1);
    outer->addWidget(buttons);
}

void PropagateFieldsDialog::onPreviewAffected()
{
    // Small modal listing the receiver GUIDs + positions.  Read-only.
    QDialog preview(this);
    preview.setWindowTitle(tr("Affected rows (%1)").arg(m_receivers.size()));
    preview.resize(420, 360);
    auto* layout = new QVBoxLayout(&preview);

    auto* hdr = new QLabel(tr("Receivers for entry %1 on map %2:")
                           .arg(m_canonical.entry).arg(m_canonical.mapId), &preview);
    layout->addWidget(hdr);

    auto* list = new QListWidget(&preview);
    for (render::Spawn const& s : m_receivers)
    {
        list->addItem(tr("guid %1   (%2, %3, %4)")
            .arg(s.guid)
            .arg(double(s.worldX), 0, 'f', 1)
            .arg(double(s.worldY), 0, 'f', 1)
            .arg(double(s.worldZ), 0, 'f', 1));
    }
    layout->addWidget(list, 1);

    auto* closeButtons = new QDialogButtonBox(QDialogButtonBox::Close, &preview);
    connect(closeButtons, &QDialogButtonBox::rejected, &preview, &QDialog::accept);
    connect(closeButtons, &QDialogButtonBox::accepted, &preview, &QDialog::accept);
    layout->addWidget(closeButtons);

    preview.exec();
}

void PropagateFieldsDialog::onApply()
{
    // Walk the checkbox map and collect the tokens that are still on.
    // Hidden creature-only / GO-only checkboxes never end up selected
    // because they're filtered out by the visibility check.
    QSet<QString> selected;
    for (auto it = m_checks.constBegin(); it != m_checks.constEnd(); ++it)
    {
        QCheckBox* cb = it.value();
        if (cb && cb->isVisible() && cb->isChecked())
            selected.insert(it.key());
    }

    emit propagateRequested(m_canonical, selected);
    accept();
}

} // namespace world_editor::app
