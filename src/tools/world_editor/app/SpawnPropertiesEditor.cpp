#include "SpawnPropertiesEditor.h"

#include "FlagPickerDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSettings>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

#include <cmath>

namespace world_editor::app
{

namespace
{
// Local pi (MSVC's <cmath> hides M_PI unless _USE_MATH_DEFINES is set before
// the include, which the PCH order makes fragile).  Degrees <-> radians here.
constexpr double kPi = 3.14159265358979323846;
[[nodiscard]] double degToRad(double d) { return d * kPi / 180.0; }
[[nodiscard]] double radToDeg(double r) { return r * 180.0 / kPi; }

// Format a uint64 as 0x.... so flag fields are readable.
QString hexU64(uint64_t v)
{
    return QStringLiteral("0x%1").arg(v, 0, 16);
}
uint64_t parseHexU64(QString const& s, uint64_t fallback)
{
    bool ok = false;
    QString trimmed = s.trimmed();
    if (trimmed.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive))
        trimmed = trimmed.mid(2);
    uint64_t v = trimmed.toULongLong(&ok, 16);
    return ok ? v : fallback;
}
} // namespace

SpawnPropertiesEditor::SpawnPropertiesEditor(QWidget* parent)
    : QWidget(parent)
{
    auto* outer = new QVBoxLayout(this);

    m_kindLabel = new QLabel(tr("(no spawn selected)"), this);
    m_kindLabel->setStyleSheet(QStringLiteral("color: #aaa;"));
    outer->addWidget(m_kindLabel);

    m_tabs = new QTabWidget(this);
    outer->addWidget(m_tabs, 1);

    buildIdentityTab();
    buildPositionTab();
    buildBehaviorTab();
    buildPhaseTab();
    buildFlagsTab();
    buildScriptTab();

    auto* footer = new QHBoxLayout;
    m_deleteButton = new QPushButton(tr("Delete"), this);
    m_deleteButton->setEnabled(false);
    m_bulkButton   = new QPushButton(tr("Bulk Edit..."), this);
    m_bulkButton->setVisible(false);
    m_addonButton  = new QPushButton(tr("Edit addon..."), this);
    m_addonButton->setVisible(false);  // gated on creature kind in setRow()
    m_addonButton->setToolTip(tr("Open the creature_addon row editor for this spawn."));
    m_goAddonButton = new QPushButton(tr("Edit GO addon..."), this);
    m_goAddonButton->setVisible(false);  // gated on gameobject kind in setRow()
    m_goAddonButton->setToolTip(tr("Open the gameobject_addon row editor for this spawn."));
    m_smartAiButton = new QPushButton(tr("SmartAI..."), this);
    m_smartAiButton->setVisible(false);  // gated on a selected row in setRow()
    m_smartAiButton->setToolTip(tr("Author a smart_scripts rule for this spawn (SmartAI)."));
    m_poolButton = new QPushButton(tr("Spawn pool..."), this);
    m_poolButton->setVisible(false);     // gated on a selected row in setRow()
    m_poolButton->setToolTip(tr("Add or remove this spawn in a spawn pool (Groups & Pools)."));
    m_revertButton = new QPushButton(tr("Revert all"), this);
    m_commitButton = new QPushButton(tr("Commit..."), this);
    m_commitButton->setEnabled(false);
    footer->addWidget(m_deleteButton);
    footer->addWidget(m_bulkButton);
    footer->addWidget(m_addonButton);
    footer->addWidget(m_goAddonButton);
    footer->addWidget(m_smartAiButton);
    footer->addWidget(m_poolButton);
    footer->addStretch(1);
    footer->addWidget(m_revertButton);
    footer->addWidget(m_commitButton);
    outer->addLayout(footer);

    m_pendingLabel = new QLabel(tr("pending: 0"), this);
    outer->addWidget(m_pendingLabel);

    // spawn_group membership; populated by MainWindow on selection.
    m_groupMembershipLabel = new QLabel(tr("Spawn groups: none"), this);
    m_groupMembershipLabel->setStyleSheet(QStringLiteral("color: #888;"));
    m_groupMembershipLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_groupMembershipLabel->setWordWrap(true);
    outer->addWidget(m_groupMembershipLabel);

    m_snapCheckbox = new QCheckBox(tr("Snap dragged spawns to ground (.map heightmap)"), this);
    {
        QSettings settings;
        m_snapCheckbox->setChecked(settings.value(QStringLiteral("editor/snap_to_ground"), true).toBool());
    }
    connect(m_snapCheckbox, &QCheckBox::toggled, this, [](bool on) {
        QSettings settings;
        settings.setValue(QStringLiteral("editor/snap_to_ground"), on);
    });
    outer->addWidget(m_snapCheckbox);

    connect(m_deleteButton, &QPushButton::clicked, this, &SpawnPropertiesEditor::deleteRequested);
    connect(m_commitButton, &QPushButton::clicked, this, &SpawnPropertiesEditor::commitRequested);
    connect(m_revertButton, &QPushButton::clicked, this, &SpawnPropertiesEditor::revertRequested);
    connect(m_bulkButton,   &QPushButton::clicked, this, &SpawnPropertiesEditor::bulkEditRequested);
    connect(m_addonButton,  &QPushButton::clicked, this, &SpawnPropertiesEditor::editAddonRequested);
    connect(m_goAddonButton, &QPushButton::clicked, this, &SpawnPropertiesEditor::editGameObjectAddonRequested);
    connect(m_smartAiButton, &QPushButton::clicked, this, &SpawnPropertiesEditor::editSmartAiRequested);
    connect(m_poolButton,    &QPushButton::clicked, this, &SpawnPropertiesEditor::editPoolRequested);
}

void SpawnPropertiesEditor::setBulkMode(int count)
{
    if (count <= 0)
    {
        clear();
        m_bulkButton->setVisible(false);
        m_tabs->setEnabled(true);
        return;
    }
    m_index = -1;
    m_kindLabel->setText(tr("%1 spawns selected").arg(count));
    m_kindLabel->setStyleSheet(QStringLiteral("color: #60c8ff; font-weight: bold;"));
    m_tabs->setEnabled(false);
    m_deleteButton->setEnabled(true);  // delete-all-selected is allowed
    m_bulkButton->setVisible(true);
    if (m_addonButton)
        m_addonButton->setVisible(false);  // single-row tool; hidden in bulk mode
    if (m_goAddonButton)
        m_goAddonButton->setVisible(false);  // single-row tool; hidden in bulk mode
    if (m_smartAiButton)
        m_smartAiButton->setVisible(false);
    if (m_poolButton)
        m_poolButton->setVisible(false);
}

bool SpawnPropertiesEditor::snapToGroundEnabled() const
{
    return m_snapCheckbox && m_snapCheckbox->isChecked();
}

void SpawnPropertiesEditor::buildIdentityTab()
{
    auto* page = new QWidget(this);
    auto* form = new QFormLayout(page);

    m_guidEdit  = new QLineEdit(page); m_guidEdit->setReadOnly(true);
    m_entrySpin = new QSpinBox(page);  m_entrySpin->setRange(0, INT_MAX); m_entrySpin->setReadOnly(true); m_entrySpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_mapEdit   = new QLineEdit(page); m_mapEdit->setReadOnly(true);
    m_zoneEdit  = new QLineEdit(page); m_zoneEdit->setReadOnly(true);
    m_areaEdit  = new QLineEdit(page); m_areaEdit->setReadOnly(true);

    form->addRow(tr("guid"),    m_guidEdit);
    form->addRow(tr("entry"),   m_entrySpin);
    form->addRow(tr("map"),     m_mapEdit);
    form->addRow(tr("zoneId"),  m_zoneEdit);
    form->addRow(tr("areaId"),  m_areaEdit);

    m_tabs->addTab(page, tr("Identity"));
}

void SpawnPropertiesEditor::buildPositionTab()
{
    auto* page = new QWidget(this);
    auto* form = new QFormLayout(page);

    // World coordinates.  Range covers the full ±17066 yd map extent with
    // headroom; 3 decimals matches the DB column precision the core reads.
    auto makeCoord = [page]() {
        auto* sb = new QDoubleSpinBox(page);
        sb->setRange(-64000.0, 64000.0);
        sb->setDecimals(3);
        sb->setSingleStep(0.5);
        sb->setSuffix(QStringLiteral(" yd"));
        return sb;
    };
    m_xSpin = makeCoord();
    m_ySpin = makeCoord();
    m_zSpin = makeCoord();

    // Facing in DEGREES (user-friendly) -- internally stored as radians in
    // `orientation`.  Wraps so 359 -> 0 reads naturally.
    m_facingSpin = new QDoubleSpinBox(page);
    m_facingSpin->setRange(0.0, 360.0);
    m_facingSpin->setDecimals(1);
    m_facingSpin->setSingleStep(5.0);
    m_facingSpin->setWrapping(true);
    m_facingSpin->setSuffix(QStringLiteral(" deg"));

    // Advanced: raw rotation quaternion (gameobject only).  For the common
    // upright GO the facing control above writes these automatically; expose
    // them for tilted props (leaning signs, toppled pillars, etc).
    auto makeQuat = [page]() {
        auto* sb = new QDoubleSpinBox(page);
        sb->setRange(-1.0, 1.0);
        sb->setDecimals(6);
        sb->setSingleStep(0.01);
        return sb;
    };
    m_r0Spin = makeQuat();
    m_r1Spin = makeQuat();
    m_r2Spin = makeQuat();
    m_r3Spin = makeQuat();

    form->addRow(tr("position_x"),     m_xSpin);
    form->addRow(tr("position_y"),     m_ySpin);
    form->addRow(tr("position_z"),     m_zSpin);
    form->addRow(tr("Facing (yaw)"),   m_facingSpin);
    form->addRow(tr("rotation0 (qx)"), m_r0Spin);
    form->addRow(tr("rotation1 (qy)"), m_r1Spin);
    form->addRow(tr("rotation2 (qz)"), m_r2Spin);
    form->addRow(tr("rotation3 (qw)"), m_r3Spin);

    auto* note = new QLabel(tr("Tip: drag spawns in the 3D view to reposition, or "
                               "type exact coordinates here. Facing is yaw in degrees; "
                               "for gameobjects it sets the rotation quaternion "
                               "automatically (edit qx..qw directly only for tilted props)."), page);
    note->setStyleSheet(QStringLiteral("color: #888; font-style: italic;"));
    note->setWordWrap(true);
    form->addRow(note);

    m_tabs->addTab(page, tr("Position"));

    // Wire edits.  Coordinates + raw quaternion just re-emit; the facing
    // control additionally rebuilds the quaternion for gameobjects.
    auto onCoord = [this](QDoubleSpinBox* sb) {
        connect(sb, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &SpawnPropertiesEditor::onFieldChanged);
    };
    onCoord(m_xSpin);
    onCoord(m_ySpin);
    onCoord(m_zSpin);
    onCoord(m_r0Spin);
    onCoord(m_r1Spin);
    onCoord(m_r2Spin);
    onCoord(m_r3Spin);

    connect(m_facingSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double deg) {
        if (m_suppress)
            return;
        // Gameobjects render from the quaternion -- rebuild it from the yaw
        // about world +Z so the visual facing follows the degrees control.
        if (m_baseline.kind == render::SpawnKind::GameObject)
        {
            double const o = degToRad(deg);
            m_suppress = true;
            m_r0Spin->setValue(0.0);
            m_r1Spin->setValue(0.0);
            m_r2Spin->setValue(std::sin(o * 0.5));
            m_r3Spin->setValue(std::cos(o * 0.5));
            m_suppress = false;
        }
        onFieldChanged();
    });
}

void SpawnPropertiesEditor::buildBehaviorTab()
{
    auto* page = new QWidget(this);
    auto* form = new QFormLayout(page);

    m_spawntimeSpin = new QSpinBox(page);
    m_spawntimeSpin->setRange(0, 604800); // 7d ceiling.
    m_spawntimeSpin->setSuffix(QStringLiteral(" s"));

    m_wanderSpin = new QDoubleSpinBox(page);
    m_wanderSpin->setRange(0.0, 1000.0); m_wanderSpin->setDecimals(2);
    m_wanderSpin->setSuffix(QStringLiteral(" yd"));

    m_curHealthSpin = new QSpinBox(page);
    m_curHealthSpin->setRange(0, 100); m_curHealthSpin->setSuffix(QStringLiteral(" %"));

    m_movementCombo = new QComboBox(page);
    m_movementCombo->addItem(tr("0 - Idle"),     0);
    m_movementCombo->addItem(tr("1 - Random"),   1);
    m_movementCombo->addItem(tr("2 - Waypoint"), 2);

    m_currentwaypointSpin = new QSpinBox(page);
    m_currentwaypointSpin->setRange(0, INT_MAX);

    m_animprogressSpin = new QSpinBox(page);
    m_animprogressSpin->setRange(0, 255);

    m_stateSpin = new QSpinBox(page);
    m_stateSpin->setRange(0, 255);

    form->addRow(tr("spawntimesecs"),    m_spawntimeSpin);
    form->addRow(tr("wander_distance"),  m_wanderSpin);
    form->addRow(tr("curHealthPct"),     m_curHealthSpin);
    form->addRow(tr("MovementType"),     m_movementCombo);
    form->addRow(tr("currentwaypoint"),  m_currentwaypointSpin);
    form->addRow(tr("(GO) animprogress"), m_animprogressSpin);
    form->addRow(tr("(GO) state"),        m_stateSpin);

    m_tabs->addTab(page, tr("Behavior"));

    auto onSpin = [this](QSpinBox* sb) {
        connect(sb, QOverload<int>::of(&QSpinBox::valueChanged),
                this, &SpawnPropertiesEditor::onFieldChanged);
    };
    onSpin(m_spawntimeSpin);
    onSpin(m_curHealthSpin);
    onSpin(m_currentwaypointSpin);
    onSpin(m_animprogressSpin);
    onSpin(m_stateSpin);
    connect(m_wanderSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SpawnPropertiesEditor::onFieldChanged);
    connect(m_movementCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SpawnPropertiesEditor::onFieldChanged);
}

void SpawnPropertiesEditor::buildPhaseTab()
{
    auto* page = new QWidget(this);
    auto* form = new QFormLayout(page);

    m_phaseUseFlagsSpin = new QSpinBox(page); m_phaseUseFlagsSpin->setRange(0, 255);
    m_phaseIdSpin       = new QSpinBox(page); m_phaseIdSpin->setRange(0, INT_MAX);
    m_phaseGroupSpin    = new QSpinBox(page); m_phaseGroupSpin->setRange(0, INT_MAX);
    m_terrainSwapSpin   = new QSpinBox(page); m_terrainSwapSpin->setRange(-1, INT_MAX);
    m_difficultiesEdit  = new QLineEdit(page); m_difficultiesEdit->setMaxLength(100);

    form->addRow(tr("phaseUseFlags"),     m_phaseUseFlagsSpin);
    form->addRow(tr("PhaseId"),           m_phaseIdSpin);
    form->addRow(tr("PhaseGroup"),        m_phaseGroupSpin);
    form->addRow(tr("terrainSwapMap"),    m_terrainSwapSpin);
    form->addRow(tr("spawnDifficulties"), m_difficultiesEdit);

    auto onSpin = [this](QSpinBox* sb) {
        connect(sb, QOverload<int>::of(&QSpinBox::valueChanged),
                this, &SpawnPropertiesEditor::onFieldChanged);
    };
    onSpin(m_phaseUseFlagsSpin);
    onSpin(m_phaseIdSpin);
    onSpin(m_phaseGroupSpin);
    onSpin(m_terrainSwapSpin);
    connect(m_difficultiesEdit, &QLineEdit::editingFinished,
            this, &SpawnPropertiesEditor::onFieldChanged);

    m_tabs->addTab(page, tr("Phase"));
}

void SpawnPropertiesEditor::buildFlagsTab()
{
    auto* page = new QWidget(this);
    auto* form = new QFormLayout(page);

    auto* note = new QLabel(tr("Enter hex (0x...) or decimal. Creature-only."), page);
    note->setStyleSheet(QStringLiteral("color: #888; font-style: italic;"));

    m_npcflagEdit    = new QLineEdit(page);
    m_unitFlags1Edit = new QLineEdit(page);
    m_unitFlags2Edit = new QLineEdit(page);
    m_unitFlags3Edit = new QLineEdit(page);
    m_modelidSpin    = new QSpinBox(page); m_modelidSpin->setRange(0, INT_MAX);
    m_equipmentSpin  = new QSpinBox(page); m_equipmentSpin->setRange(0, 255);

    form->addRow(note);

    // Each bitmask field gets a "Pick..." button opening a named-checkbox
    // editor (FlagPickerDialog) so the operator never has to recall hex bits.
    // The pick function differs per field; a generic lambda dispatches.
    auto addFlagRow = [this, page, form](QString const& label, QLineEdit* edit,
                                         auto pickFn) {
        auto* rowWidget = new QWidget(page);
        auto* h = new QHBoxLayout(rowWidget);
        h->setContentsMargins(0, 0, 0, 0);
        h->addWidget(edit, 1);
        auto* btn = new QPushButton(tr("Pick..."), rowWidget);
        btn->setToolTip(tr("Choose flags by name instead of typing hex."));
        h->addWidget(btn);
        form->addRow(label, rowWidget);
        connect(btn, &QPushButton::clicked, this, [this, edit, pickFn]() {
            uint64_t const cur = parseHexU64(edit->text(), 0);
            auto const picked = pickFn(this, cur);
            if (picked)
            {
                edit->setText(hexU64(uint64_t(*picked)));
                onFieldChanged();
            }
        });
    };

    addFlagRow(tr("npcflag"),     m_npcflagEdit,    &pickNpcFlags);
    addFlagRow(tr("unit_flags"),  m_unitFlags1Edit, &pickUnitFlags);
    addFlagRow(tr("unit_flags2"), m_unitFlags2Edit, &pickUnitFlags2);
    addFlagRow(tr("unit_flags3"), m_unitFlags3Edit, &pickUnitFlags3);
    form->addRow(tr("modelid"),       m_modelidSpin);
    form->addRow(tr("equipment_id"),  m_equipmentSpin);

    connect(m_npcflagEdit,    &QLineEdit::editingFinished, this, &SpawnPropertiesEditor::onFieldChanged);
    connect(m_unitFlags1Edit, &QLineEdit::editingFinished, this, &SpawnPropertiesEditor::onFieldChanged);
    connect(m_unitFlags2Edit, &QLineEdit::editingFinished, this, &SpawnPropertiesEditor::onFieldChanged);
    connect(m_unitFlags3Edit, &QLineEdit::editingFinished, this, &SpawnPropertiesEditor::onFieldChanged);
    connect(m_modelidSpin,    QOverload<int>::of(&QSpinBox::valueChanged),
            this, &SpawnPropertiesEditor::onFieldChanged);
    connect(m_equipmentSpin,  QOverload<int>::of(&QSpinBox::valueChanged),
            this, &SpawnPropertiesEditor::onFieldChanged);

    m_tabs->addTab(page, tr("Flags"));
}

void SpawnPropertiesEditor::buildScriptTab()
{
    auto* page = new QWidget(this);
    auto* form = new QFormLayout(page);

    m_scriptNameEdit = new QLineEdit(page);
    m_scriptNameEdit->setMaxLength(64);
    m_stringIdEdit = new QLineEdit(page);
    m_stringIdEdit->setMaxLength(64);

    form->addRow(tr("ScriptName"), m_scriptNameEdit);
    form->addRow(tr("StringId"),   m_stringIdEdit);

    connect(m_scriptNameEdit, &QLineEdit::editingFinished, this, &SpawnPropertiesEditor::onFieldChanged);
    connect(m_stringIdEdit,   &QLineEdit::editingFinished, this, &SpawnPropertiesEditor::onFieldChanged);

    m_tabs->addTab(page, tr("Script"));
}

void SpawnPropertiesEditor::setRow(int index, render::Spawn const& s)
{
    m_index    = index;
    m_baseline = s;
    applyToForm(s);
    QString const kindStr = (s.kind == render::SpawnKind::Creature)
                          ? QStringLiteral("creature") : QStringLiteral("gameobject");
    m_kindLabel->setText(tr("selected: %1 guid=%2 entry=%3")
        .arg(kindStr).arg(s.guid).arg(s.entry));
    m_kindLabel->setStyleSheet(QStringLiteral("color: #f9b34a;"));
    m_deleteButton->setEnabled(index >= 0);
    // creature_addon is creature-only (PK references creature.guid).
    if (m_addonButton)
        m_addonButton->setVisible(index >= 0 && s.kind == render::SpawnKind::Creature);
    // gameobject_addon is gameobject-only (PK references gameobject.guid).
    if (m_goAddonButton)
        m_goAddonButton->setVisible(index >= 0 && s.kind == render::SpawnKind::GameObject);
    // SmartAI + spawn-pool apply to either kind once a single row is selected.
    if (m_smartAiButton)
        m_smartAiButton->setVisible(index >= 0);
    if (m_poolButton)
        m_poolButton->setVisible(index >= 0);
}

void SpawnPropertiesEditor::clear()
{
    m_index = -1;
    m_baseline = render::Spawn{};
    applyToForm(m_baseline);
    m_kindLabel->setText(tr("(no spawn selected)"));
    m_kindLabel->setStyleSheet(QStringLiteral("color: #aaa;"));
    m_deleteButton->setEnabled(false);
    if (m_addonButton)
        m_addonButton->setVisible(false);
    if (m_goAddonButton)
        m_goAddonButton->setVisible(false);
    if (m_smartAiButton)
        m_smartAiButton->setVisible(false);
    if (m_poolButton)
        m_poolButton->setVisible(false);
    if (m_groupMembershipLabel)
        m_groupMembershipLabel->setText(tr("Spawn groups: none"));
}

void SpawnPropertiesEditor::setPendingCount(size_t count)
{
    m_pendingLabel->setText(tr("pending: %1").arg(count));
    m_commitButton->setEnabled(count > 0);
    m_revertButton->setEnabled(count > 0);
}

void SpawnPropertiesEditor::setGroupMembershipText(QString const& text)
{
    if (m_groupMembershipLabel)
        m_groupMembershipLabel->setText(text);
}

void SpawnPropertiesEditor::applyToForm(render::Spawn const& s)
{
    m_suppress = true;
    m_guidEdit->setText(QString::number(s.guid));
    m_entrySpin->setValue(int(s.entry));
    m_mapEdit->setText(QString::number(s.mapId));
    m_zoneEdit->setText(QString::number(s.zoneId));
    m_areaEdit->setText(QString::number(s.areaId));

    m_xSpin->setValue(double(s.worldX));
    m_ySpin->setValue(double(s.worldY));
    m_zSpin->setValue(double(s.worldZ));
    // orientation (radians) -> facing degrees, normalised to [0, 360).
    double deg = radToDeg(double(s.orientation));
    deg = std::fmod(deg, 360.0);
    if (deg < 0.0) deg += 360.0;
    m_facingSpin->setValue(deg);
    m_r0Spin->setValue(double(s.rotation0));
    m_r1Spin->setValue(double(s.rotation1));
    m_r2Spin->setValue(double(s.rotation2));
    m_r3Spin->setValue(double(s.rotation3));

    m_spawntimeSpin->setValue(int(s.spawntimesecs));
    m_wanderSpin->setValue(s.wanderDistance);
    m_curHealthSpin->setValue(int(s.curHealthPct));
    int const mvIdx = m_movementCombo->findData(int(s.movementType));
    m_movementCombo->setCurrentIndex(mvIdx >= 0 ? mvIdx : 0);
    m_currentwaypointSpin->setValue(int(s.currentwaypoint));
    m_animprogressSpin->setValue(int(s.animprogress));
    m_stateSpin->setValue(int(s.goState));

    m_phaseUseFlagsSpin->setValue(int(s.phaseUseFlags));
    m_phaseIdSpin->setValue(int(s.phaseId));
    m_phaseGroupSpin->setValue(int(s.phaseGroup));
    m_terrainSwapSpin->setValue(s.terrainSwapMap);
    m_difficultiesEdit->setText(s.spawnDifficulties);

    m_npcflagEdit->setText(hexU64(s.npcflag));
    m_unitFlags1Edit->setText(hexU64(s.unitFlags1));
    m_unitFlags2Edit->setText(hexU64(s.unitFlags2));
    m_unitFlags3Edit->setText(hexU64(s.unitFlags3));
    m_modelidSpin->setValue(int(s.modelid));
    m_equipmentSpin->setValue(int(s.equipmentId));

    m_scriptNameEdit->setText(s.scriptName);
    m_stringIdEdit->setText(s.stringId);
    m_suppress = false;
}

render::Spawn SpawnPropertiesEditor::snapshotFromForm() const
{
    render::Spawn s = m_baseline;
    // Position + orientation (now editable; the 3D drag writes the same fields).
    s.worldX      = float(m_xSpin->value());
    s.worldY      = float(m_ySpin->value());
    s.worldZ      = float(m_zSpin->value());
    s.orientation = float(degToRad(m_facingSpin->value()));
    // Raw quaternion (gameobject); the facing control keeps these in sync for
    // upright props, but a power user may have dialled in a tilt directly.
    s.rotation0   = float(m_r0Spin->value());
    s.rotation1   = float(m_r1Spin->value());
    s.rotation2   = float(m_r2Spin->value());
    s.rotation3   = float(m_r3Spin->value());

    s.spawntimesecs   = uint32_t(m_spawntimeSpin->value());
    s.wanderDistance  = float(m_wanderSpin->value());
    s.curHealthPct    = uint32_t(m_curHealthSpin->value());
    s.movementType    = uint8_t(m_movementCombo->currentData().toInt());
    s.currentwaypoint = uint32_t(m_currentwaypointSpin->value());
    s.animprogress    = uint8_t(m_animprogressSpin->value());
    s.goState         = uint8_t(m_stateSpin->value());

    s.phaseUseFlags   = uint8_t(m_phaseUseFlagsSpin->value());
    s.phaseId         = uint32_t(m_phaseIdSpin->value());
    s.phaseGroup      = uint32_t(m_phaseGroupSpin->value());
    s.terrainSwapMap  = m_terrainSwapSpin->value();
    s.spawnDifficulties = m_difficultiesEdit->text();

    s.npcflag    = parseHexU64(m_npcflagEdit->text(),    m_baseline.npcflag);
    s.unitFlags1 = uint32_t(parseHexU64(m_unitFlags1Edit->text(), m_baseline.unitFlags1));
    s.unitFlags2 = uint32_t(parseHexU64(m_unitFlags2Edit->text(), m_baseline.unitFlags2));
    s.unitFlags3 = uint32_t(parseHexU64(m_unitFlags3Edit->text(), m_baseline.unitFlags3));
    s.modelid    = uint32_t(m_modelidSpin->value());
    s.equipmentId = uint8_t(m_equipmentSpin->value());

    s.scriptName = m_scriptNameEdit->text();
    s.stringId   = m_stringIdEdit->text();
    return s;
}

void SpawnPropertiesEditor::onFieldChanged()
{
    if (m_suppress || m_index < 0)
        return;
    emit rowEdited(snapshotFromForm());
}

} // namespace world_editor::app
