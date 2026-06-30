#include "AboutDialog.h"

#include "../db/MySqlClient.h"
#include "../render/SceneView3D.h"

#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVBoxLayout>
#include <QtGlobal>

namespace world_editor::app
{

namespace
{
// Hardcoded TC build target string, per spec.
constexpr char const* kTcBuildTarget = "playerbot-dev branch (12.0+)";

// __DATE__/__TIME__ are baked at compile time of this TU; that's the
// closest stand-in for "build date" without a CMake-injected git SHA.
constexpr char const* kBuildDate = __DATE__ " " __TIME__;
} // namespace

QString AboutDialog::crashlogPath()
{
#if defined(Q_OS_WIN)
    QString const localAppData = QString::fromLocal8Bit(qgetenv("LOCALAPPDATA"));
    QString base = localAppData.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        : localAppData;
    return QDir::toNativeSeparators(base + QStringLiteral("/TrinityCore/world_editor/crashlogs/"));
#else
    QString const base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    return QDir::toNativeSeparators(base + QStringLiteral("/TrinityCore/world_editor/crashlogs/"));
#endif
}

QString AboutDialog::docFileUrl(QString const& fileName)
{
    // Walk up from the executable looking for src/tools/world_editor/docs/<fileName>.
    // The deployed editor runs out of the install prefix where docs are not
    // copied, so we also probe a couple of common dev-tree relatives.
    QStringList const candidates = {
        QCoreApplication::applicationDirPath() + QStringLiteral("/../src/tools/world_editor/docs/") + fileName,
        QCoreApplication::applicationDirPath() + QStringLiteral("/../../src/tools/world_editor/docs/") + fileName,
        QCoreApplication::applicationDirPath() + QStringLiteral("/../../../src/tools/world_editor/docs/") + fileName,
        QCoreApplication::applicationDirPath() + QStringLiteral("/../../../../src/tools/world_editor/docs/") + fileName,
        QCoreApplication::applicationDirPath() + QStringLiteral("/docs/") + fileName,
    };
    for (QString const& c : candidates)
    {
        QFileInfo fi(c);
        if (fi.exists() && fi.isFile())
            return QUrl::fromLocalFile(fi.absoluteFilePath()).toString();
    }
    return QString();
}

QString AboutDialog::buildHeaderHtml() const
{
    QString const qtVer = QString::fromLatin1(qVersion());
    QString glLine = QStringLiteral("GL info unavailable");
    if (m_viewer3d)
    {
        QString const v = m_viewer3d->glVersionString();
        QString const r = m_viewer3d->glRendererString();
        if (!v.isEmpty())
            glLine = r.isEmpty() ? v : QStringLiteral("%1 - %2").arg(v, r);
    }

    QString dbLine = QStringLiteral("not connected");
    if (m_worldDb && m_worldDb->isConnected())
    {
        QString const sv = QString::fromStdString(m_worldDb->serverVersion());
        dbLine = sv.isEmpty() ? QStringLiteral("connected") : QStringLiteral("connected (server %1)").arg(sv);
    }

    QString const crashDir = crashlogPath();

    // Two-column key/value rendered via HTML table so labels align cleanly.
    QString html;
    html += QStringLiteral("<table cellspacing='4' cellpadding='0'>");
    html += QStringLiteral("<tr><td><b>Build:</b></td><td>%1</td></tr>").arg(QString::fromLatin1(kBuildDate).toHtmlEscaped());
    html += QStringLiteral("<tr><td><b>Qt runtime:</b></td><td>%1</td></tr>").arg(qtVer.toHtmlEscaped());
    html += QStringLiteral("<tr><td><b>TC target:</b></td><td>%1</td></tr>").arg(QString::fromLatin1(kTcBuildTarget).toHtmlEscaped());
    html += QStringLiteral("<tr><td><b>OpenGL:</b></td><td>%1</td></tr>").arg(glLine.toHtmlEscaped());
    html += QStringLiteral("<tr><td><b>Database:</b></td><td>%1</td></tr>").arg(dbLine.toHtmlEscaped());
    html += QStringLiteral("<tr><td><b>Crashlogs:</b></td><td><code>%1</code></td></tr>").arg(crashDir.toHtmlEscaped());
    html += QStringLiteral("</table>");
    return html;
}

QString AboutDialog::buildDiagnosticBlock() const
{
    QString const qtVer = QString::fromLatin1(qVersion());
    QString glLine = QStringLiteral("GL info unavailable");
    if (m_viewer3d)
    {
        QString const v = m_viewer3d->glVersionString();
        QString const r = m_viewer3d->glRendererString();
        if (!v.isEmpty())
            glLine = r.isEmpty() ? v : QStringLiteral("%1 - %2").arg(v, r);
    }
    QString dbLine = QStringLiteral("not connected");
    if (m_worldDb && m_worldDb->isConnected())
    {
        QString const sv = QString::fromStdString(m_worldDb->serverVersion());
        dbLine = sv.isEmpty() ? QStringLiteral("connected") : QStringLiteral("connected (server %1)").arg(sv);
    }

    QStringList out;
    out << QStringLiteral("TrinityCore world_editor");
    out << QStringLiteral("Build:    %1").arg(QString::fromLatin1(kBuildDate));
    out << QStringLiteral("Qt:       %1 (compiled %2)").arg(qtVer, QStringLiteral(QT_VERSION_STR));
    out << QStringLiteral("TC:       %1").arg(QString::fromLatin1(kTcBuildTarget));
    out << QStringLiteral("OpenGL:   %1").arg(glLine);
    out << QStringLiteral("Database: %1").arg(dbLine);
    out << QStringLiteral("Crashes:  %1").arg(crashlogPath());
    return out.join(QChar::LineFeed);
}

AboutDialog::AboutDialog(db::MySqlClient const* worldDb,
                         render::SceneView3D const* viewer3d,
                         QWidget* parent)
    : QDialog(parent)
    , m_worldDb(worldDb)
    , m_viewer3d(viewer3d)
{
    setWindowTitle(tr("About TrinityCore world_editor"));
    setModal(true);
    resize(560, 520);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(16, 16, 16, 12);
    outer->setSpacing(10);

    // Title row.
    auto* title = new QLabel(QStringLiteral("<h2 style='margin:0'>TrinityCore world_editor</h2>"), this);
    title->setTextFormat(Qt::RichText);
    outer->addWidget(title);

    // Build / runtime info block (rendered from HTML table).
    auto* infoBox = new QGroupBox(tr("Build && runtime"), this);
    auto* infoLay = new QVBoxLayout(infoBox);
    auto* infoLbl = new QLabel(buildHeaderHtml(), infoBox);
    infoLbl->setTextFormat(Qt::RichText);
    infoLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
    infoLbl->setWordWrap(true);
    infoLay->addWidget(infoLbl);
    outer->addWidget(infoBox);

    // Dependencies block - flat list, no live introspection.
    auto* depsBox = new QGroupBox(tr("Dependencies"), this);
    auto* depsLay = new QVBoxLayout(depsBox);
    auto* depsLbl = new QLabel(
        tr("<ul style='margin:0'>"
           "<li>Qt 6 (Widgets, OpenGLWidgets, Sql)</li>"
           "<li>CascLib - WoW client storage reader</li>"
           "<li>Detour - navmesh runtime (Recast/Detour)</li>"
           "<li>zlib - compression for .mmap/.map data</li>"
           "<li>libmysql - direct MySQL client (no Qt SQL plugin)</li>"
           "</ul>"), depsBox);
    depsLbl->setTextFormat(Qt::RichText);
    depsLbl->setWordWrap(true);
    depsLay->addWidget(depsLbl);
    outer->addWidget(depsBox);

    // Documentation block - file:// links resolved against the source tree.
    auto* docsBox = new QGroupBox(tr("Documentation"), this);
    auto* docsLay = new QVBoxLayout(docsBox);
    QStringList linkRows;
    for (QString const& fileName : { QStringLiteral("OVERNIGHT_TRACKER.md"),
                                     QStringLiteral("HANDOFF_v2.md"),
                                     QStringLiteral("CHANGELOG.md") })
    {
        QString const url = docFileUrl(fileName);
        if (url.isEmpty())
            linkRows << QStringLiteral("<li><code>%1</code> <i>(not found in source tree)</i></li>").arg(fileName.toHtmlEscaped());
        else
            linkRows << QStringLiteral("<li><a href=\"%1\">%2</a></li>").arg(url.toHtmlEscaped(), fileName.toHtmlEscaped());
    }
    auto* docsLbl = new QLabel(QStringLiteral("<ul style='margin:0'>%1</ul>").arg(linkRows.join(QString())), docsBox);
    docsLbl->setTextFormat(Qt::RichText);
    docsLbl->setOpenExternalLinks(false);
    docsLbl->setTextInteractionFlags(Qt::TextBrowserInteraction);
    // Use QDesktopServices explicitly so file:// links go through the OS
    // handler rather than Qt trying to render them in-place.
    QObject::connect(docsLbl, &QLabel::linkActivated, this, [](QString const& href) {
        QDesktopServices::openUrl(QUrl(href));
    });
    docsLay->addWidget(docsLbl);
    outer->addWidget(docsBox);

    // Credits block.
    auto* credBox = new QGroupBox(tr("Credits"), this);
    auto* credLay = new QVBoxLayout(credBox);
    auto* credLbl = new QLabel(
        tr("<ul style='margin:0'>"
           "<li>TrinityCore project - core server, data primitives, navmesh tools</li>"
           "<li>Playerbot module - AI extension this branch ships with</li>"
           "<li>world_editor - native desktop editor for spawn / path / area / "
           "annotation data, built on the same standalone IO primitives as "
           "<code>mmap_world_dump</code></li>"
           "</ul>"), credBox);
    credLbl->setTextFormat(Qt::RichText);
    credLbl->setWordWrap(true);
    credLay->addWidget(credLbl);
    outer->addWidget(credBox);

    outer->addStretch(1);

    // Footer: Copy diagnostics + Close.
    auto* footer = new QDialogButtonBox(this);
    auto* copyBtn = footer->addButton(tr("Copy diagnostic info"), QDialogButtonBox::ActionRole);
    footer->addButton(QDialogButtonBox::Close);
    connect(copyBtn, &QPushButton::clicked, this, &AboutDialog::onCopyDiagnostics);
    connect(footer, &QDialogButtonBox::rejected, this, &QDialog::accept);
    outer->addWidget(footer);
}

void AboutDialog::onCopyDiagnostics()
{
    if (QClipboard* cb = QApplication::clipboard())
        cb->setText(buildDiagnosticBlock());
}

} // namespace world_editor::app
