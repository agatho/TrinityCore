/*
 * world_editor - native desktop world editor for TrinityCore.
 *
 * Entry point. Sets up QApplication, applies dark-mode palette
 * defaults to match the typical TC ops environment, instantiates the
 * MainWindow and runs the event loop.
 *
 * Standalone Qt 6 / OpenGL application. Reads .map / .mmap / .mmtile
 * via the same binary layout as the worldserver - no TC runtime
 * linkage; see src/tools/world_editor/io/MMapReader.* for the readers.
 *
 * Phase 0 milestone: window appears, "About" dialog mentions the
 * editor and Qt versions. Phase 1 plugs in the 2D viewer.
 */

#include "app/CrashHandler.h"
#include "app/DebugLog.h"
#include "app/MainWindow.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QStyleFactory>

int main(int argc, char* argv[])
{
    // Install the Windows top-level unhandled-exception filter BEFORE
    // QApplication so even early-startup crashes (Qt plugin load, GPU
    // driver init, etc.) produce a minidump + log under
    // %LOCALAPPDATA%\TrinityCore\world_editor\crashlogs\.  No-op on
    // non-Windows builds.
    world_editor::app::installCrashHandler();

    // Install the qDebug/qInfo/qWarning sink that routes Qt messages to
    // %LOCALAPPDATA%\TrinityCore\world_editor\debug.log.  Must come AFTER
    // crash handler (so Qt-driven crashes can mention the log path) but
    // BEFORE QApplication so the message handler picks up Qt's own
    // startup diagnostics too.
    world_editor::app::installDebugLog();

    QApplication app(argc, argv);
    QApplication::setOrganizationName("TrinityCore");
    QApplication::setApplicationName("world_editor");
    QApplication::setApplicationDisplayName("TrinityCore World Editor");
    QApplication::setApplicationVersion("0.1.0");

    // Fusion style works identically on every platform; the editor
    // depends on it for predictable widget metrics in the renderer.
    QApplication::setStyle(QStyleFactory::create("Fusion"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        "Native desktop editor for TrinityCore world data: spawns, paths, areas, phasing, pools.");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption mmapsOpt({"m", "mmaps"},
        "Path to extracted mmaps directory (the parent of 0000*.mmap).",
        "mmaps_dir");
    QCommandLineOption mapsOpt({"a", "maps"},
        "Path to extracted maps directory (the parent of 0000*.map).",
        "maps_dir");
    QCommandLineOption vmapsOpt({"v", "vmaps"},
        "Path to extracted vmaps directory (the parent of <mapId:04>/0000.vmtree).",
        "vmaps_dir");
    QCommandLineOption openMapOpt({"o", "open"},
        "Map id to open on startup.",
        "mapId");
    // ---- Headless visual-oracle: render a map+camera to PNG and exit. ----
    QCommandLineOption shotOpt("screenshot",
        "Headless: render the opened map's 3D view to this PNG file and exit.",
        "out.png");
    QCommandLineOption camOpt("cam",
        "Camera pose for --screenshot: x,y,z,yawDeg,pitchDeg (TC world coords).",
        "pose");
    QCommandLineOption sizeOpt("size",
        "Screenshot size WxH (default 1280x720).", "WxH");
    QCommandLineOption waitOpt("wait",
        "Screenshot async-stream wait in ms (default 6000).", "ms");
    QCommandLineOption noRealOpt("flat",
        "Screenshot in flat (non-realistic) mode; default is realistic textures.");
    parser.addOption(mmapsOpt);
    parser.addOption(mapsOpt);
    parser.addOption(vmapsOpt);
    parser.addOption(openMapOpt);
    parser.addOption(shotOpt);
    parser.addOption(camOpt);
    parser.addOption(sizeOpt);
    parser.addOption(waitOpt);
    parser.addOption(noRealOpt);
    parser.process(app);

    world_editor::MainWindow window;
    if (parser.isSet(mmapsOpt))
        window.setMMapsDir(parser.value(mmapsOpt));
    if (parser.isSet(mapsOpt))
        window.setMapsDir(parser.value(mapsOpt));
    if (parser.isSet(vmapsOpt))
        window.setVmapsDir(parser.value(vmapsOpt));

    if (parser.isSet(shotOpt))
    {
        // Parse size WxH.
        int w = 1280, h = 720;
        if (parser.isSet(sizeOpt))
        {
            QStringList const wh = parser.value(sizeOpt).split('x', Qt::SkipEmptyParts);
            if (wh.size() == 2) { w = wh[0].toInt(); h = wh[1].toInt(); }
            if (w < 16) w = 1280;
            if (h < 16) h = 720;
        }
        // Parse camera x,y,z,yawDeg,pitchDeg.
        float cx = 0, cy = 0, cz = 300, yawDeg = 0, pitchDeg = -45;
        if (parser.isSet(camOpt))
        {
            QStringList const c = parser.value(camOpt).split(',', Qt::SkipEmptyParts);
            if (c.size() >= 3) { cx = c[0].toFloat(); cy = c[1].toFloat(); cz = c[2].toFloat(); }
            if (c.size() >= 4) yawDeg   = c[3].toFloat();
            if (c.size() >= 5) pitchDeg = c[4].toFloat();
        }
        int const waitMs = parser.isSet(waitOpt) ? parser.value(waitOpt).toInt() : 6000;
        bool const realistic = !parser.isSet(noRealOpt);
        uint32_t const mapId = parser.isSet(openMapOpt) ? parser.value(openMapOpt).toUInt() : 0u;

        // Initialise the widget + GL context WITHOUT a visible window.
        constexpr double kDeg2Rad = 3.14159265358979323846 / 180.0;
        window.setAttribute(Qt::WA_DontShowOnScreen, true);
        window.resize(w, h);
        window.show();
        QApplication::processEvents();

        bool const ok = window.renderToPng(mapId, cx, cy, cz,
                                           float(yawDeg * kDeg2Rad), float(pitchDeg * kDeg2Rad),
                                           realistic, w, h, waitMs, parser.value(shotOpt));
        return ok ? 0 : 2;
    }

    if (parser.isSet(openMapOpt))
        window.requestOpenMap(parser.value(openMapOpt).toUInt());

    window.show();
    return app.exec();
}
