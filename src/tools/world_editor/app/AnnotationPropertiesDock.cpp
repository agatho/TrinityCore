#include "AnnotationPropertiesDock.h"

#include <QDoubleSpinBox>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace world_editor::app
{

AnnotationPropertiesDock::AnnotationPropertiesDock(QWidget* parent)
    : QWidget(parent)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(6, 6, 6, 6);

    m_promptLabel = new QLabel(
        tr("Click an annotation disc to inspect it.\n\n"
           "world-metadata table: characters.playerbot_v2_world_metadata.\n"
           "kind / position / created_by are immutable per upstream design — "
           "delete + readd to change them."),
        this);
    m_promptLabel->setWordWrap(true);
    root->addWidget(m_promptLabel);

    // ---- Identity (read-only) ---------------------------------------
    auto* idBox    = new QGroupBox(tr("Identity"), this);
    auto* idForm   = new QFormLayout(idBox);
    QFont fixedFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    auto makeReadOnly = [&](QLineEdit*& slot, QString const& placeholder) {
        slot = new QLineEdit(idBox);
        slot->setReadOnly(true);
        slot->setFont(fixedFont);
        slot->setPlaceholderText(placeholder);
        return slot;
    };
    idForm->addRow(tr("id:"),         makeReadOnly(m_idEdit,        tr("(auto)")));
    idForm->addRow(tr("map_id:"),     makeReadOnly(m_mapIdEdit,     tr("0")));
    idForm->addRow(tr("zone_id:"),    makeReadOnly(m_zoneIdEdit,    tr("0")));
    idForm->addRow(tr("kind:"),       makeReadOnly(m_kindEdit,      tr("-")));
    idForm->addRow(tr("pos_x:"),      makeReadOnly(m_xEdit,         tr("0.000")));
    idForm->addRow(tr("pos_y:"),      makeReadOnly(m_yEdit,         tr("0.000")));
    idForm->addRow(tr("pos_z:"),      makeReadOnly(m_zEdit,         tr("0.000")));
    idForm->addRow(tr("created_by:"), makeReadOnly(m_createdByEdit, tr("-")));
    root->addWidget(idBox);

    // ---- Editable ----------------------------------------------------
    auto* editBox  = new QGroupBox(tr("Edit"), this);
    auto* editForm = new QFormLayout(editBox);

    m_radiusSpin = new QDoubleSpinBox(editBox);
    m_radiusSpin->setRange(0.5, 500.0);
    m_radiusSpin->setSingleStep(0.5);
    m_radiusSpin->setDecimals(2);
    m_radiusSpin->setSuffix(tr(" yd"));
    editForm->addRow(tr("radius:"), m_radiusSpin);

    m_labelEdit = new QLineEdit(editBox);
    m_labelEdit->setMaxLength(96);
    m_labelEdit->setPlaceholderText(tr("short tag (e.g. 'Stormwind east bridge')"));
    editForm->addRow(tr("label:"), m_labelEdit);

    m_notesEdit = new QPlainTextEdit(editBox);
    m_notesEdit->setPlaceholderText(tr("free-form notes"));
    m_notesEdit->setMaximumHeight(96);
    editForm->addRow(tr("notes:"), m_notesEdit);

    root->addWidget(editBox);

    // ---- Footer ------------------------------------------------------
    m_pendingLabel = new QLabel(tr("0 pending"), this);
    m_pendingLabel->setStyleSheet(QStringLiteral("color: #888;"));
    root->addWidget(m_pendingLabel);

    auto* btnRow = new QHBoxLayout();
    m_deleteButton = new QPushButton(tr("Delete"),       this);
    m_commitButton = new QPushButton(tr("Commit..."),    this);
    m_revertButton = new QPushButton(tr("Revert all"),   this);
    m_deleteButton->setToolTip(tr("Mark this annotation for deletion (pending until commit)."));
    m_commitButton->setToolTip(tr("Review pending annotation changes and write them to DB."));
    m_revertButton->setToolTip(tr("Discard ALL pending annotation edits and reload from DB state."));
    btnRow->addWidget(m_deleteButton);
    btnRow->addStretch(1);
    btnRow->addWidget(m_revertButton);
    btnRow->addWidget(m_commitButton);
    root->addLayout(btnRow);

    root->addStretch(1);

    connect(m_radiusSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, &AnnotationPropertiesDock::onFieldChanged);
    connect(m_labelEdit,  &QLineEdit::textEdited,
            this, &AnnotationPropertiesDock::onFieldChanged);
    connect(m_notesEdit,  &QPlainTextEdit::textChanged,
            this, &AnnotationPropertiesDock::onFieldChanged);

    connect(m_deleteButton, &QPushButton::clicked, this,
            [this]() { emit deleteRequested(); });
    connect(m_commitButton, &QPushButton::clicked, this,
            [this]() { emit commitRequested(); });
    connect(m_revertButton, &QPushButton::clicked, this,
            [this]() { emit revertRequested(); });

    clear();
}

void AnnotationPropertiesDock::clear()
{
    m_suppress = true;
    m_index    = -1;
    m_baseline = render::Annotation{};
    m_idEdit->clear();
    m_mapIdEdit->clear();
    m_zoneIdEdit->clear();
    m_kindEdit->clear();
    m_xEdit->clear();
    m_yEdit->clear();
    m_zEdit->clear();
    m_createdByEdit->clear();
    m_radiusSpin->setValue(5.0);
    m_labelEdit->clear();
    m_notesEdit->clear();
    m_radiusSpin->setEnabled(false);
    m_labelEdit->setEnabled(false);
    m_notesEdit->setEnabled(false);
    m_deleteButton->setEnabled(false);
    m_promptLabel->setVisible(true);
    m_suppress = false;
}

void AnnotationPropertiesDock::setRow(int index, render::Annotation const& a)
{
    m_suppress = true;
    m_index    = index;
    m_baseline = a;
    applyToForm(a);
    m_radiusSpin->setEnabled(true);
    m_labelEdit->setEnabled(true);
    m_notesEdit->setEnabled(true);
    m_deleteButton->setEnabled(true);
    m_promptLabel->setVisible(false);
    m_suppress = false;
}

void AnnotationPropertiesDock::applyToForm(render::Annotation const& a)
{
    m_idEdit->setText(QString::number(a.id));
    m_mapIdEdit->setText(QString::number(a.mapId));
    m_zoneIdEdit->setText(QString::number(a.zoneId));
    m_kindEdit->setText(QStringLiteral("%1 (%2)")
        .arg(uint8_t(a.kind))
        .arg(QString::fromLatin1(render::annotationKindName(a.kind))));
    m_xEdit->setText(QString::number(a.x, 'f', 3));
    m_yEdit->setText(QString::number(a.y, 'f', 3));
    m_zEdit->setText(QString::number(a.z, 'f', 3));
    m_createdByEdit->setText(a.createdBy.isEmpty() ? QStringLiteral("-") : a.createdBy);
    m_radiusSpin->setValue(double(a.radius));
    m_labelEdit->setText(a.label);
    // Replace plain text without firing textChanged feedback loops.
    if (m_notesEdit->toPlainText() != a.notes)
        m_notesEdit->setPlainText(a.notes);
}

render::Annotation AnnotationPropertiesDock::snapshotFromForm() const
{
    render::Annotation a = m_baseline;
    a.radius = float(m_radiusSpin->value());
    a.label  = m_labelEdit->text();
    a.notes  = m_notesEdit->toPlainText();
    return a;
}

void AnnotationPropertiesDock::onFieldChanged()
{
    if (m_suppress || m_index < 0)
        return;
    emit rowEdited(m_index, snapshotFromForm());
}

void AnnotationPropertiesDock::setPendingCount(size_t count)
{
    m_pendingLabel->setText(count == 0
        ? tr("0 pending")
        : tr("%1 pending").arg(count));
    m_pendingLabel->setStyleSheet(count == 0
        ? QStringLiteral("color: #888;")
        : QStringLiteral("color: #d59f00; font-weight: bold;"));
}

} // namespace world_editor::app
