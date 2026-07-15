#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QIcon>
#include <QDir>
#include <QSurfaceFormat>
#include <QQuickWindow>
#include <QQuickItem>
#include <QTimer>
#include <QScreen>
#include <QLoggingCategory>

#include "core/settings.h"
#include "core/javamanager.h"
#include "core/versionmanager.h"
#include "core/launcher.h"
#include "download/downloadmanager.h"
#include "util/file_drop_handler.h"

int main(int argc, char *argv[]){
    // Enable multisample anti-aliasing for smooth rounded corners and shapes
    QSurfaceFormat fmt;
    fmt.setSamples(8);
    QSurfaceFormat::setDefaultFormat(fmt);

    QGuiApplication app(argc, argv);
    app.setApplicationName("LPCL");
    app.setApplicationVersion(QString::fromLatin1(APP_VERSION));
    app.setOrganizationName("LPCL");

    // Print version info at startup
    qInfo() << "LPCL version:" << GIT_DESCRIBE << "commit:" << GIT_COMMIT_HASH;

    // Initialize settings
    Settings::initialize();

    // Initialize managers
    auto &javaMgr = JavaManager::instance();
    auto &verMgr = VersionManager::instance();
    auto &launcher = Launcher::instance();
    auto &downloadMgr = DownloadManager::instance();

    // Set up logging
    QLoggingCategory::setFilterRules("lpcl.*.debug=true\nlpcl.java.debug=false");

    // Initial Minecraft folder from settings
    QString savedFolder = Settings::instance().getString("LaunchFolderSelect");
    if (savedFolder.isEmpty()) {
        // Default to ~/.minecraft
        savedFolder = QDir::homePath() + "/.minecraft/";
    }
    verMgr.setMcFolder(savedFolder);

    // Initial Java scan
    javaMgr.scanSystemJava();

    // Create QML engine
    QQmlApplicationEngine engine;

    // C++ types are now auto-registered via QML_ELEMENT / QML_SINGLETON macros.
    // They are available in QML via `import LPCL` (the main QML module).
    // Only FileDropHandler is still manually registered — it needs setupWindow().
    // Explicit call to ensure the static registration object is pulled in by the linker.
    extern void qml_register_types_LPCL();
    qml_register_types_LPCL();

    // Register FileDropHandler singleton — intercepts external file drag-and-drop
    FileDropHandler *dropHandler = new FileDropHandler(&app);
    qmlRegisterSingletonInstance("LPCL", 1, 0, "FileDropHandler", dropHandler);

    // Connect status text changes
    QObject::connect(&launcher, &Launcher::statusTextChanged, []() {
        qInfo() << "[Launcher]" << Launcher::instance().statusText();
    });

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    // Wire up the drop handler once the root ApplicationWindow is created.
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreated,
        dropHandler,
        [dropHandler](QObject *obj, const QUrl &) {
            if (auto *win = qobject_cast<QQuickWindow *>(obj))
                dropHandler->setupWindow(win);
        });

    
    // Splash screen — matches original FrmStart = SplashScreen("Images\icon.ico")
    // Shows immediately before the heavy main window loads
    
    auto *splashWin = new QQuickWindow();
    splashWin->setFlags(Qt::FramelessWindowHint);
    splashWin->setColor(Qt::transparent);
    splashWin->resize(128, 128);
    // Center on screen
    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect screenGeo = screen->availableGeometry();
        splashWin->setPosition((screenGeo.width() - 128) / 2,
                               (screenGeo.height() - 128) / 2);
    }
    // PCL icon from original (icon — same file used by WPF SplashScreen)
    QQmlComponent splashComp(&engine);
    splashComp.setData(R"(
        import QtQuick
        Rectangle { 
            width: 128; height: 128
            radius: 16
            color: "transparent"
            clip: true
            Image {
                anchors.fill: parent
                source: "qrc:/assets/logo.svg"
                sourceSize: Qt.size(256, 256)
                smooth: true
                mipmap: true
                fillMode: Image.PreserveAspectFit
            }
        }
    )",
                       QUrl());
    if (auto *content = qobject_cast<QQuickItem*>(splashComp.create())) {
        content->setParentItem(splashWin->contentItem());
    }
    splashWin->show();

    // Load main module (heavy — takes time while splash is visible)
    engine.setInitialProperties({
        {"appVersion", QString::fromLatin1(GIT_DESCRIBE)}
    });
    engine.loadFromModule("LPCL", "Main");

    // Close splash after main window renders + fade overlap
    //   Splash fades → 400ms, window fades in → 250ms (starts immediately)
    //   Close splash at 450ms so crossfade completes cleanly.
    //   deleteLater() frees the QQuickWindow — close() alone would leak it.
    QTimer::singleShot(450, splashWin, [splashWin]() {
        splashWin->close();
        splashWin->deleteLater();
    });

    return app.exec();
}
