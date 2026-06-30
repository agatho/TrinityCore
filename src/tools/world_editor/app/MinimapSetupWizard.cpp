#include "MinimapSetupWizard.h"

#include "MainWindow.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace world_editor::app
{

namespace
{
// The two-path explainer body.  Kept as a QStringLiteral so AUTOMOC's tr()
// scanner picks the surrounding text up via the call sites below.
constexpr char const* kBodyHtml = R"(
<p><b>Minimap textures require client data.</b>  Choose one of two paths:</p>

<p><b>Path 1 (recommended): Read directly from your WoW client install</b></p>
<ul>
  <li><b>File &rarr; Set WoW client directory</b></li>
  <li>Point at your World of Warcraft folder (e.g. <code>C:\WoW</code>)</li>
  <li>The editor reads BLP minimap tiles directly via CASC, decoded at runtime.</li>
</ul>

<p><b>Path 2: Pre-extract minimap PNGs</b></p>
<ul>
  <li>Extract minimap BLPs via <i>wow.export</i> or a similar tool</li>
  <li>Convert to PNG, arrange as <code>&lt;minimap_dir&gt;/&lt;mapId&gt;/map&lt;gx&gt;_&lt;gy&gt;.png</code></li>
  <li><b>File &rarr; Set minimap directory</b></li>
</ul>

<p>Either path is enough; configuring both is fine and the editor prefers PNGs
on disk over a live CASC fetch when both are available.</p>
)";
} // namespace

MinimapSetupWizard::MinimapSetupWizard(MainWindow* owner, QWidget* parent)
    : QDialog(parent)
    , m_owner(owner)
{
    setWindowTitle(tr("Set up minimap textures"));
    setModal(true);
    // Wide enough that the bullet lines don't wrap awkwardly at default font.
    resize(560, 440);

    auto* body = new QLabel(tr(kBodyHtml), this);
    body->setTextFormat(Qt::RichText);
    body->setWordWrap(true);
    body->setTextInteractionFlags(Qt::TextSelectableByMouse);
    body->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto* cascBtn = new QPushButton(tr("Set WoW client directory now..."), this);
    cascBtn->setToolTip(tr(
        "Opens File -> Set WoW client directory... so the editor can read BLP "
        "minimap tiles directly from the CASC storage at runtime."));
    auto* pngBtn = new QPushButton(tr("Set minimap PNG directory now..."), this);
    pngBtn->setToolTip(tr(
        "Opens File -> Set minimap directory... for a folder of pre-extracted "
        "PNG tiles laid out as <dir>/<mapId>/map<gx>_<gy>.png."));
    auto* closeBtn = new QPushButton(tr("Close"), this);

    auto* btnRow = new QHBoxLayout();
    btnRow->addWidget(cascBtn);
    btnRow->addWidget(pngBtn);
    btnRow->addStretch(1);
    btnRow->addWidget(closeBtn);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(body, /*stretch*/ 1);
    layout->addLayout(btnRow);

    connect(cascBtn,  &QPushButton::clicked, this, &MinimapSetupWizard::onPickCascDir);
    connect(pngBtn,   &QPushButton::clicked, this, &MinimapSetupWizard::onPickMinimapDir);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
}

void MinimapSetupWizard::onPickCascDir()
{
    // Close the wizard first so the picker isn't parented under it (avoids
    // dialog stacking + lets the picker keep modality cleanly on the main
    // window).
    accept();
    if (m_owner)
        QMetaObject::invokeMethod(m_owner, "onSetCascClientDir", Qt::QueuedConnection);
}

void MinimapSetupWizard::onPickMinimapDir()
{
    accept();
    if (m_owner)
        QMetaObject::invokeMethod(m_owner, "onSetMinimapDir", Qt::QueuedConnection);
}

} // namespace world_editor::app
