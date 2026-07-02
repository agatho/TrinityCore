#include "AnnotationToolbox.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace world_editor::app
{

namespace
{
struct KindEntry { render::AnnotationKind kind; char const* label; float defaultRadius; };
constexpr KindEntry KIND_TABLE[] =
{
    { render::AnnotationKind::Road,      "road",       12.0f },
    { render::AnnotationKind::Crossroad, "crossroad",  20.0f },
    { render::AnnotationKind::City,      "city",      150.0f },
    { render::AnnotationKind::Village,   "village",    75.0f },
    { render::AnnotationKind::Hub,       "hub",        50.0f },
    { render::AnnotationKind::Danger,    "danger",     60.0f },
    { render::AnnotationKind::Vendor,    "vendor",      6.0f },
    { render::AnnotationKind::Mailbox,   "mailbox",     4.0f },
    { render::AnnotationKind::Innkeeper, "innkeeper",   6.0f },
    { render::AnnotationKind::Other,     "other",      10.0f },
    // Vertical / multi-modal transit hints — sized for the typical
    // elevator footprint (~8y radius covers the shaft) and dock pier
    // anchor (~12y covers the boarding ring).
    { render::AnnotationKind::Elevator,  "elevator",    8.0f },
    { render::AnnotationKind::Dock,      "dock",       12.0f },
};
} // namespace

AnnotationToolbox::AnnotationToolbox(QWidget* parent)
    : QWidget(parent)
{
    auto* outer = new QVBoxLayout(this);

    // Persistent committed-count badge.  Sits at the top of the dock so
    // the operator can confirm at a glance how many annotations are
    // currently in DB for the loaded map.  Mirrors the HandcraftedRoadDock
    // committed-count badge pattern.
    m_committedBadge = new QLabel(tr("0 annotations on this map"), this);
    m_committedBadge->setStyleSheet(QStringLiteral("color: #888;"));
    QFont badgeFont = m_committedBadge->font();
    badgeFont.setBold(true);
    m_committedBadge->setFont(badgeFont);
    outer->addWidget(m_committedBadge);

    m_kindCombo = new QComboBox(this);
    for (KindEntry const& e : KIND_TABLE)
        m_kindCombo->addItem(QString::fromLatin1(e.label), int(e.kind));

    m_radiusSpin = new QDoubleSpinBox(this);
    m_radiusSpin->setRange(0.5, 5000.0);
    m_radiusSpin->setDecimals(2);
    m_radiusSpin->setSuffix(QStringLiteral(" yd"));
    m_radiusSpin->setValue(KIND_TABLE[0].defaultRadius);

    m_labelEdit     = new QLineEdit(this);
    m_labelEdit->setMaxLength(96);
    m_labelEdit->setPlaceholderText(tr("(optional) e.g. 'crossroad east of Goldshire'"));

    m_notesEdit     = new QLineEdit(this);
    m_notesEdit->setMaxLength(255);
    m_notesEdit->setPlaceholderText(tr("(optional) free-form"));

    m_createdByEdit = new QLineEdit(this);
    m_createdByEdit->setMaxLength(64);
    m_createdByEdit->setPlaceholderText(tr("operator name"));

    auto* form = new QFormLayout;
    form->addRow(tr("Kind"),       m_kindCombo);
    form->addRow(tr("Radius"),     m_radiusSpin);
    form->addRow(tr("Label"),      m_labelEdit);
    form->addRow(tr("Notes"),      m_notesEdit);
    form->addRow(tr("Created by"), m_createdByEdit);
    outer->addLayout(form);

    m_placeToggle = new QCheckBox(tr("Place new annotations on click"), this);
    outer->addWidget(m_placeToggle);

    m_selectedHeader = new QLabel(tr("(no annotation selected)"), this);
    m_selectedHeader->setStyleSheet(QStringLiteral("color: #aaa;"));
    outer->addWidget(m_selectedHeader);

    auto* buttonRow = new QHBoxLayout;
    m_deleteButton = new QPushButton(tr("Delete selected"), this);
    m_deleteButton->setEnabled(false);
    m_revertButton = new QPushButton(tr("Revert all"), this);
    m_commitButton = new QPushButton(tr("Commit..."), this);
    m_commitButton->setEnabled(false);
    buttonRow->addWidget(m_deleteButton);
    buttonRow->addStretch(1);
    buttonRow->addWidget(m_revertButton);
    buttonRow->addWidget(m_commitButton);
    outer->addLayout(buttonRow);

    m_pendingLabel = new QLabel(tr("pending: 0"), this);
    outer->addWidget(m_pendingLabel);
    outer->addStretch(1);

    // Transient mutation-feedback toast.  Hidden until showToast() pops it
    // after an insert/update/delete; auto-hides 2.5s later (6s for errors).
    m_toastLabel = new QLabel(this);
    m_toastLabel->setVisible(false);
    m_toastLabel->setWordWrap(true);
    m_toastLabel->setMargin(6);
    m_toastLabel->setAlignment(Qt::AlignCenter);
    outer->addWidget(m_toastLabel);

    connect(m_kindCombo,     QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AnnotationToolbox::onKindChanged);
    connect(m_placeToggle,   &QCheckBox::toggled,
            this, &AnnotationToolbox::onPlaceToggled);
    connect(m_radiusSpin,    QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &AnnotationToolbox::onRadiusEdited);
    connect(m_labelEdit,     &QLineEdit::editingFinished,
            this, &AnnotationToolbox::onLabelEdited);
    connect(m_notesEdit,     &QLineEdit::editingFinished,
            this, &AnnotationToolbox::onNotesEdited);
    connect(m_deleteButton,  &QPushButton::clicked, this, &AnnotationToolbox::deleteSelectedRequested);
    connect(m_commitButton,  &QPushButton::clicked, this, &AnnotationToolbox::commitRequested);
    connect(m_revertButton,  &QPushButton::clicked, this, &AnnotationToolbox::revertRequested);
}

bool AnnotationToolbox::isPlacing() const
{
    return m_placeToggle->isChecked();
}

void AnnotationToolbox::setPlacing(bool on)
{
    // setChecked emits toggled -> placeModeChanged, so MainWindow's mode
    // state follows automatically (Escape-exit path relies on this).
    m_placeToggle->setChecked(on);
}

render::AnnotationKind AnnotationToolbox::currentKind() const
{
    int const idx = m_kindCombo->currentIndex();
    if (idx < 0 || idx >= int(sizeof(KIND_TABLE) / sizeof(KIND_TABLE[0])))
        return render::AnnotationKind::Other;
    return KIND_TABLE[idx].kind;
}

float AnnotationToolbox::currentRadius() const
{
    return static_cast<float>(m_radiusSpin->value());
}

QString AnnotationToolbox::currentLabel() const     { return m_labelEdit->text(); }
QString AnnotationToolbox::currentNotes() const     { return m_notesEdit->text(); }
QString AnnotationToolbox::currentCreatedBy() const { return m_createdByEdit->text(); }

void AnnotationToolbox::setPendingCount(size_t count)
{
    m_pendingLabel->setText(tr("pending: %1").arg(count));
    m_commitButton->setEnabled(count > 0);
    m_revertButton->setEnabled(count > 0);
}

void AnnotationToolbox::setSelectedRow(int index, render::Annotation const& a)
{
    m_selectedIndex = index;
    m_selectedHeader->setText(tr("selected: id=%1 (%2)")
        .arg(a.id).arg(render::annotationKindName(a.kind)));
    m_selectedHeader->setStyleSheet(QStringLiteral("color: #f9b34a;"));
    m_deleteButton->setEnabled(true);

    m_suppressEdits = true;
    // Reflect selected row into widgets.  Kind/position are read-only
    // per the WorldMetadataStore policy ("delete + readd"), so the kind
    // combo follows along but the operator's edits to it don't propagate
    // back when a row is selected.
    for (int i = 0; i < m_kindCombo->count(); ++i)
        if (m_kindCombo->itemData(i).toInt() == int(a.kind))
        {
            m_kindCombo->setCurrentIndex(i);
            break;
        }
    m_radiusSpin->setValue(a.radius);
    m_labelEdit->setText(a.label);
    m_notesEdit->setText(a.notes);
    m_createdByEdit->setText(a.createdBy);
    m_suppressEdits = false;
}

void AnnotationToolbox::clearSelectedRow()
{
    m_selectedIndex = -1;
    m_selectedHeader->setText(tr("(no annotation selected)"));
    m_selectedHeader->setStyleSheet(QStringLiteral("color: #aaa;"));
    m_deleteButton->setEnabled(false);
}

void AnnotationToolbox::onKindChanged(int index)
{
    if (m_suppressEdits)
        return;
    if (index < 0 || index >= int(sizeof(KIND_TABLE) / sizeof(KIND_TABLE[0])))
        return;
    // Only auto-bump the radius when there is no selected row (the
    // operator is configuring defaults for the next placement).
    if (m_selectedIndex < 0)
    {
        m_suppressEdits = true;
        m_radiusSpin->setValue(KIND_TABLE[index].defaultRadius);
        m_suppressEdits = false;
    }
}

void AnnotationToolbox::onPlaceToggled(bool checked)
{
    emit placeModeChanged(checked);
}

void AnnotationToolbox::onRadiusEdited(double value)
{
    if (m_suppressEdits || m_selectedIndex < 0)
        return;
    emit selectedRowRadiusChanged(static_cast<float>(value));
}

void AnnotationToolbox::onLabelEdited()
{
    if (m_suppressEdits || m_selectedIndex < 0)
        return;
    emit selectedRowLabelChanged(m_labelEdit->text());
}

void AnnotationToolbox::onNotesEdited()
{
    if (m_suppressEdits || m_selectedIndex < 0)
        return;
    emit selectedRowNotesChanged(m_notesEdit->text());
}

void AnnotationToolbox::setCommittedCount(size_t count, uint32_t mapId)
{
    if (!m_committedBadge)
        return;
    if (count == 0)
    {
        m_committedBadge->setText(tr("0 annotations on this map"));
        m_committedBadge->setStyleSheet(QStringLiteral("color: #888;"));
    }
    else
    {
        // U+2713 CHECK MARK (UTF-8 = E2 9C 93) — same glyph the
        // HandcraftedRoadDock badge uses so the two surfaces feel
        // consistent.
        m_committedBadge->setText(tr("\xE2\x9C\x93 %1 annotation(s) on this map (map %2)")
                                  .arg(count).arg(mapId));
        m_committedBadge->setStyleSheet(QStringLiteral("color: #3da85a;"));
    }
}

void AnnotationToolbox::showToast(QString const& text, QString const& kind)
{
    if (!m_toastLabel)
        return;
    QString bg;
    int holdMs = 2500;
    if (kind == QStringLiteral("ok"))
        bg = QStringLiteral("#3da85a");
    else if (kind == QStringLiteral("warn"))
        bg = QStringLiteral("#d98a3d");
    else
    {
        bg = QStringLiteral("#c0392b");
        holdMs = 6000;
    }
    m_toastLabel->setText(text);
    m_toastLabel->setStyleSheet(QString(
        QStringLiteral("background-color: %1; color: white; "
                       "font-weight: bold; border-radius: 4px;")).arg(bg));
    m_toastLabel->setVisible(true);

    quint64 const epoch = ++m_toastEpoch;
    QTimer::singleShot(holdMs, this, [this, epoch]() {
        if (!m_toastLabel)
            return;
        // Newer toast may have superseded; only clear if our epoch is
        // still the latest one.
        if (epoch != m_toastEpoch)
            return;
        m_toastLabel->setVisible(false);
        m_toastLabel->clear();
        m_toastLabel->setStyleSheet(QString());
    });
}

} // namespace world_editor::app
