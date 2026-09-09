#include "MinimapDiagnosticsDock.h"

#include "../render/NavMeshView.h"

#include <QButtonGroup>
#include <QFont>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QShowEvent>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>

#include <array>

namespace world_editor::app
{

namespace
{

// Convenience: red text for "<not configured>" / "not open" placeholders
// so the operator can scan the dock at a glance.
constexpr char const* kRedTextStyle  = "color: #d96b6b;";
constexpr char const* kGoodTextStyle = "color: #cdd9a3;";
constexpr char const* kDimTextStyle  = "color: #aaa;";

QLabel* makeValueLabel(QWidget* parent)
{
    auto* label = new QLabel(parent);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    label->setWordWrap(true);
    return label;
}

} // namespace

MinimapDiagnosticsDock::MinimapDiagnosticsDock(QWidget* parent)
    : QWidget(parent)
{
    auto* outer = new QVBoxLayout(this);

    auto* header = new QLabel(tr("Minimap diagnostics"), this);
    QFont headerFont = header->font();
    headerFont.setBold(true);
    header->setFont(headerFont);
    outer->addWidget(header);

    // Two-column form: short field label on the left, value on the right.
    // Right column carries dynamic colour to flag missing/failed state.
    auto* form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
    form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_cascDirLabel    = makeValueLabel(this);
    m_cascStatusLabel = makeValueLabel(this);
    m_mapDb2Label     = makeValueLabel(this);
    m_minimapDirLabel = makeValueLabel(this);
    m_currentMapLabel = makeValueLabel(this);
    m_heightmapLabel  = makeValueLabel(this);
    m_cachedLabel        = makeValueLabel(this);
    m_successfulLabel    = makeValueLabel(this);
    m_failedLabel        = makeValueLabel(this);
    m_lastTriedLabel     = makeValueLabel(this);
    // The viewer publishes the SINGLE canonical CASC vpath it probed for
    // the most recent tile (formula:
    // world/minimaps/<dir>/map<pad2(gx)>_<pad2(gy)>.blp), so the operator
    // can paste it into a listfile lookup or file browser to verify an
    // absence is genuine rather than a path-construction bug.  The loader
    // no longer probes the (gx, gy) / (gy, gx) swapped pair — that was
    // the source of swapped-tile placement on continents where both
    // orderings happen to resolve to different real tiles.
    m_lastTriedLabel->setToolTip(tr(
        "Single canonical client path the loader probed for the last tile "
        "(world/minimaps/<dir>/map<pad2(gx)>_<pad2(gy)>.blp).  Loader no "
        "longer tries alternative orderings — that caused half the "
        "landmass to be placed at swapped grid cells when both orderings "
        "resolved to different FDIDs."));
    m_listfileLabel      = makeValueLabel(this);
    m_listfileEntriesLabel = makeValueLabel(this);
    m_pngDirCountLabel   = makeValueLabel(this);
    m_autoRoadLabel        = makeValueLabel(this);
    m_handcraftedRoadLabel = makeValueLabel(this);
    m_autoRoadLabel->setText(QStringLiteral("0"));
    m_handcraftedRoadLabel->setText(QStringLiteral("0"));

    form->addRow(tr("CASC client dir:"),         m_cascDirLabel);
    form->addRow(tr("CASC storage:"),            m_cascStatusLabel);
    form->addRow(tr("Map.db2:"),                 m_mapDb2Label);
    form->addRow(tr("Listfile CSV:"),            m_listfileLabel);
    form->addRow(tr("Listfile entries:"),        m_listfileEntriesLabel);
    form->addRow(tr("Minimap PNG dir:"),         m_minimapDirLabel);
    form->addRow(tr("PNG dir file count:"),      m_pngDirCountLabel);
    form->addRow(tr("Current map:"),             m_currentMapLabel);
    form->addRow(tr("Heightmap tiles:"),         m_heightmapLabel);
    form->addRow(tr("Minimap textures cached:"), m_cachedLabel);
    form->addRow(tr("Successful loads:"),        m_successfulLabel);
    form->addRow(tr("Failed loads:"),            m_failedLabel);
    form->addRow(tr("Last-tried tile:"),         m_lastTriedLabel);
    form->addRow(tr("Auto road polylines:"),         m_autoRoadLabel);
    form->addRow(tr("Handcrafted road polylines:"),  m_handcraftedRoadLabel);

    outer->addLayout(form);

    // Hint banner: only visible when neither minimap source is wired.  We
    // toggle setVisible() in setMinimapInfo so it disappears once the
    // operator fixes either path.
    m_hintLabel = new QLabel(this);
    m_hintLabel->setWordWrap(true);
    m_hintLabel->setStyleSheet(QStringLiteral("color: #d4b75a; font-style: italic;"));
    m_hintLabel->setText(tr(
        "Configure either File -> Set listfile CSV (for live CASC reads on "
        "modern client data) or File -> Set minimap PNG dir (for pre-"
        "extracted PNGs).  Without either, the minimap layer will render "
        "empty tiles."));
    m_hintLabel->setVisible(false);
    outer->addWidget(m_hintLabel);

    auto* buttons = new QHBoxLayout;
    m_refreshButton = new QPushButton(tr("Refresh"), this);
    m_refreshButton->setToolTip(tr("Poll NavMeshView + MainWindow state and redraw this panel."));
    m_reloadButton  = new QPushButton(tr("Force reload minimaps"), this);
    m_reloadButton->setToolTip(tr(
        "Re-pump the configured minimap dir into the viewer (drops cached GL textures "
        "and re-attempts every tile on the next paint)."));
    m_probeButton   = new QPushButton(tr("Probe CASC paths"), this);
    m_probeButton->setToolTip(tr(
        "Enumerate the CASC catalog for the current map's minimap directory and show "
        "the first 50 filenames that match.  Use this when no tiles are loading to "
        "discover the actual naming convention the client is using."));
    buttons->addWidget(m_refreshButton);
    buttons->addWidget(m_reloadButton);
    buttons->addWidget(m_probeButton);
    buttons->addStretch(1);
    outer->addLayout(buttons);

    // ---- Minimap-transform A/B panel --------------------------------
    //
    // Replaces the F1..F7 keyboard shortcuts that used to drive
    // NavMeshView::m_minimapTransform.  The panel ships 10 radio buttons
    // mapped to the canonical candidate transforms; clicking one saves
    // the choice to QSettings("viewer2d/minimap_transform") and pumps
    // NavMeshView::setMinimapTransform(), which flushes the texture
    // cache AND force-restarts the heightmap streaming queue.
    auto* transformBox = new QGroupBox(tr("Minimap transform (A/B test)"), this);
    auto* transformLayout = new QVBoxLayout(transformBox);
    transformLayout->setSpacing(2);

    struct TransformChoice
    {
        char const*                       label;
        render::NavMeshView::MinimapTransform value;
    };
    using MT = render::NavMeshView::MinimapTransform;
    std::array<TransformChoice, 10> const choices{{
        { "Identity (none)",            MT::Identity },
        { "Transpose (swap rows/cols)", MT::Transpose },
        { "Rotate 90 CW",               MT::Rotate90CW },
        { "Rotate 90 CCW",              MT::Rotate90CCW },
        { "Rotate 180",                 MT::Rotate180 },
        { "Mirror horizontal",          MT::MirrorH },
        { "Mirror vertical",            MT::MirrorV },
        { "Transpose + mirror H",       MT::Transpose_MirrorH },
        { "Transpose + mirror V",       MT::Transpose_MirrorV },
        { "Transpose + 180",            MT::Transpose_Rot180 },
    }};

    m_transformGroup = new QButtonGroup(this);
    m_transformGroup->setExclusive(true);

    // Read the previous selection from QSettings; default is Rotate90CW.
    // Operator A/B 2026-05-26 pass 2: pass 1 picked Rotate180 (correct per
    // tile) plus a view-level -90 deg rotation, but the view-level rotation
    // also rotated the heightmap / spawn / path / annotation layers (which
    // were already correct).  Pass 2 reverts view rotation to 0 and folds
    // the missing 90 deg into the per-tile transform: Rotate180 +
    // Rotate90CCW == Rotate90CW.  If the operator reports tiles oriented
    // correctly but mirrored relative to their neighbours, Rotate90CCW is
    // the one-click fallback.  We coerce to a valid enum range so a stale
    // setting from an earlier build falls back cleanly.
    QSettings settings;
    int const savedRaw = settings.value(QStringLiteral("viewer2d/minimap_transform"),
                                        int(MT::Rotate90CW)).toInt();
    auto const savedTransform = (savedRaw >= 0 && savedRaw < int(MT::Count_))
        ? MT(savedRaw)
        : MT::Rotate90CW;

    for (auto const& choice : choices)
    {
        auto* rb = new QRadioButton(tr(choice.label), transformBox);
        if (choice.value == savedTransform)
            rb->setChecked(true);
        m_transformGroup->addButton(rb, int(choice.value));
        transformLayout->addWidget(rb);
    }

    m_transformActive = new QLabel(transformBox);
    m_transformActive->setWordWrap(true);
    m_transformActive->setStyleSheet(kDimTextStyle);
    transformLayout->addWidget(m_transformActive);

    m_inspectButton = new QPushButton(tr("Inspect at canonical coords"), transformBox);
    m_inspectButton->setToolTip(tr(
        "Probe three known-good Eastern Kingdoms tiles (map34_61, map32_48, "
        "map49_36) and report the resolved FDID, the BLP encoding "
        "(DXT1/DXT3/DXT5/Pal8/ARGB) and the decoded QImage format for each.  "
        "Use this for side-by-side diagnostic comparison without enabling "
        "verbose logging."));
    transformLayout->addWidget(m_inspectButton);

    outer->addWidget(transformBox);
    outer->addStretch(1);

    auto const applyChoice = [this](MT t) {
        QSettings persist;
        persist.setValue(QStringLiteral("viewer2d/minimap_transform"), int(t));
        if (m_viewer)
            m_viewer->setMinimapTransform(t);
        int const cached = m_viewer ? m_viewer->minimapCachedCount() : 0;
        m_transformActive->setText(tr("Active: %1   |   tile textures cached: %2")
            .arg(QString::fromUtf8(render::NavMeshView::minimapTransformName(t)))
            .arg(cached));
    };
    // Seed the active-state label with the persisted selection so the
    // dock opens informative even before the operator clicks anything.
    applyChoice(savedTransform);

    connect(m_transformGroup, &QButtonGroup::idClicked,
            this, [applyChoice](int id) {
                if (id < 0 || id >= int(MT::Count_))
                    return;
                applyChoice(MT(id));
            });

    connect(m_inspectButton, &QPushButton::clicked, this, [this]() {
        if (!m_viewer)
        {
            QMessageBox::information(this, tr("Inspect at canonical coords"),
                tr("Viewer not wired - cannot probe."));
            return;
        }
        auto const reports = m_viewer->inspectCanonicalTiles();
        QStringList lines;
        for (auto const& r : reports)
        {
            lines << QStringLiteral("map%1_%2:")
                .arg(r.gx, 2, 10, QLatin1Char('0'))
                .arg(r.gy, 2, 10, QLatin1Char('0'));
            lines << QStringLiteral("    FDID:       %1").arg(r.resolvedFdid);
            lines << QStringLiteral("    encoding:   %1").arg(r.blpEncoding);
            lines << QStringLiteral("    QImage fmt: %1").arg(r.qimageFormat);
            if (!r.notes.isEmpty())
                lines << QStringLiteral("    notes:      %1").arg(r.notes);
            lines << QString();
        }
        QMessageBox box(this);
        box.setWindowTitle(tr("Inspect at canonical coords"));
        box.setIcon(QMessageBox::Information);
        box.setText(tr("Diagnostic probe of three known-good Eastern Kingdoms tiles:"));
        box.setDetailedText(lines.join(QStringLiteral("\n")));
        box.exec();
    });

    connect(m_refreshButton, &QPushButton::clicked, this, &MinimapDiagnosticsDock::refreshRequested);
    connect(m_reloadButton,  &QPushButton::clicked, this, [this]() {
        // Pump NavMeshView::setMinimapDir(...) with its own value; this
        // path internally calls destroyMinimapTextures() + re-attempts
        // every probed tile.  MainWindow may also choose to handle the
        // signal itself (e.g. to mirror to a 3D viewer) - we both emit
        // AND poke the viewer directly so a no-op handler still works.
        if (m_viewer)
            m_viewer->setMinimapDir(m_viewer->minimapDir());
        emit forceReloadRequested();
    });
    connect(m_probeButton,   &QPushButton::clicked, this, [this]() {
        // Run a one-shot CASC enumeration for the current map's minimap
        // directory and surface the result in a modal so the operator
        // can copy/paste filenames out for debugging.  The probe also
        // primes the viewer's discovered-names cache so subsequent tile
        // loads will try the freshly-seen naming convention.
        if (!m_viewer)
        {
            QMessageBox::information(this, tr("Probe CASC paths"),
                tr("Viewer not wired - cannot probe."));
            return;
        }
        QStringList const names = m_viewer->probeMinimapCascNames(50);
        if (names.isEmpty())
        {
            QMessageBox::warning(this, tr("Probe CASC paths"),
                tr("No matching entries found in CASC for the current map's "
                   "minimap directory.  Check that CASC is open, Map.db2 is "
                   "loaded, and the current map has a directory entry."));
            return;
        }
        QMessageBox box(this);
        box.setWindowTitle(tr("Probe CASC paths (%1 entries)").arg(names.size()));
        box.setIcon(QMessageBox::Information);
        box.setText(tr("Discovered CASC entries for the current map:"));
        box.setDetailedText(names.join(QStringLiteral("\n")));
        box.exec();
    });

    // Auto-poll while the dock is visible.  2s cadence matches the
    // log-tail dock; cheap enough that the operator can leave the dock
    // open while opening maps and the panel just trickles fresh values.
    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(2000);
    connect(m_pollTimer, &QTimer::timeout, this, &MinimapDiagnosticsDock::refreshRequested);
}

void MinimapDiagnosticsDock::setMinimapInfo(QString cascDir, bool cascOpen, int mapDb2Entries,
                                            QString minimapDir, uint32_t mapId, QString mapDir,
                                            int heightmapTiles, int cachedTextures,
                                            int successfulLoads, int failedLoads,
                                            QString lastTried,
                                            QString listfileCsv, int listfileEntries,
                                            int pngDirCount)
{
    if (cascDir.isEmpty())
    {
        m_cascDirLabel->setText(tr("<not configured>"));
        m_cascDirLabel->setStyleSheet(kRedTextStyle);
    }
    else
    {
        m_cascDirLabel->setText(cascDir);
        m_cascDirLabel->setStyleSheet(QString{});
    }

    if (cascOpen)
    {
        m_cascStatusLabel->setText(tr("open"));
        m_cascStatusLabel->setStyleSheet(kGoodTextStyle);
    }
    else
    {
        m_cascStatusLabel->setText(tr("not open"));
        m_cascStatusLabel->setStyleSheet(kRedTextStyle);
    }

    if (mapDb2Entries > 0)
    {
        m_mapDb2Label->setText(tr("loaded %1 entries").arg(mapDb2Entries));
        m_mapDb2Label->setStyleSheet(QString{});
    }
    else
    {
        m_mapDb2Label->setText(tr("not loaded"));
        m_mapDb2Label->setStyleSheet(kRedTextStyle);
    }

    if (minimapDir.isEmpty())
    {
        m_minimapDirLabel->setText(tr("<not configured>"));
        m_minimapDirLabel->setStyleSheet(kRedTextStyle);
    }
    else
    {
        m_minimapDirLabel->setText(minimapDir);
        m_minimapDirLabel->setStyleSheet(QString{});
    }

    if (mapId == 0 && mapDir.isEmpty())
    {
        m_currentMapLabel->setText(tr("none loaded"));
        m_currentMapLabel->setStyleSheet(kDimTextStyle);
    }
    else
    {
        m_currentMapLabel->setText(tr("%1: %2")
            .arg(mapId)
            .arg(mapDir.isEmpty() ? tr("<unknown directory>") : mapDir));
        m_currentMapLabel->setStyleSheet(QString{});
    }

    m_heightmapLabel->setText(QString::number(heightmapTiles));
    m_cachedLabel->setText(QString::number(cachedTextures));
    m_successfulLabel->setText(QString::number(successfulLoads));
    m_successfulLabel->setStyleSheet(successfulLoads > 0 ? kGoodTextStyle : kDimTextStyle);
    m_failedLabel->setText(QString::number(failedLoads));
    m_failedLabel->setStyleSheet(failedLoads > 0 ? kRedTextStyle : kDimTextStyle);

    if (lastTried.isEmpty())
    {
        m_lastTriedLabel->setText(tr("(none yet)"));
        m_lastTriedLabel->setStyleSheet(kDimTextStyle);
    }
    else
    {
        m_lastTriedLabel->setText(lastTried);
        m_lastTriedLabel->setStyleSheet(QString{});
    }

    // Listfile + PNG-dir-count rows.  Listfile is the only reliable
    // way to resolve modern (TWW build 67186+) minimap BLPs because the
    // client stores many of them as FileDataIDs only — no virtual path
    // in the CASC root.
    bool const haveListfile = !listfileCsv.isEmpty() && listfileEntries > 0;
    if (listfileCsv.isEmpty())
    {
        m_listfileLabel->setText(tr("<not configured>"));
        m_listfileLabel->setStyleSheet(kRedTextStyle);
    }
    else
    {
        m_listfileLabel->setText(listfileCsv);
        m_listfileLabel->setStyleSheet(haveListfile ? kGoodTextStyle : kRedTextStyle);
    }
    if (listfileEntries > 0)
    {
        m_listfileEntriesLabel->setText(QString::number(listfileEntries));
        m_listfileEntriesLabel->setStyleSheet(kGoodTextStyle);
    }
    else if (!listfileCsv.isEmpty())
    {
        m_listfileEntriesLabel->setText(tr("0 (parse failed?)"));
        m_listfileEntriesLabel->setStyleSheet(kRedTextStyle);
    }
    else
    {
        m_listfileEntriesLabel->setText(tr("(no listfile)"));
        m_listfileEntriesLabel->setStyleSheet(kDimTextStyle);
    }

    bool const havePngs = (pngDirCount > 0);
    if (pngDirCount < 0)
    {
        m_pngDirCountLabel->setText(tr("(dir not set)"));
        m_pngDirCountLabel->setStyleSheet(kDimTextStyle);
    }
    else if (pngDirCount == 0)
    {
        m_pngDirCountLabel->setText(tr("0 (empty)"));
        m_pngDirCountLabel->setStyleSheet(kRedTextStyle);
    }
    else
    {
        m_pngDirCountLabel->setText(QString::number(pngDirCount));
        m_pngDirCountLabel->setStyleSheet(kGoodTextStyle);
    }

    // Surface the hint banner when BOTH minimap sources are unusable.
    // PNG dir counts as usable only when it has files; an empty configured
    // dir is functionally identical to "not set" for the loader.
    if (m_hintLabel)
        m_hintLabel->setVisible(!haveListfile && !havePngs);
}

void MinimapDiagnosticsDock::setRoadOverlayInfo(int autoPolylines, int handcraftedPolylines)
{
    if (m_autoRoadLabel)
        m_autoRoadLabel->setText(QString::number(autoPolylines));
    if (m_handcraftedRoadLabel)
        m_handcraftedRoadLabel->setText(QString::number(handcraftedPolylines));
}

void MinimapDiagnosticsDock::startAutoRefresh()
{
    if (m_pollTimer && !m_pollTimer->isActive())
        m_pollTimer->start();
}

void MinimapDiagnosticsDock::stopAutoRefresh()
{
    if (m_pollTimer && m_pollTimer->isActive())
        m_pollTimer->stop();
}

void MinimapDiagnosticsDock::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    // Refresh immediately + start the 2s timer so values appear before
    // the first interval elapses.
    emit refreshRequested();
    startAutoRefresh();
}

void MinimapDiagnosticsDock::hideEvent(QHideEvent* event)
{
    stopAutoRefresh();
    QWidget::hideEvent(event);
}

} // namespace world_editor::app
