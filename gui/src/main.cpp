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
#include "auth/offlineauth.h"
#include "auth/msauth.h"
#include "util/file_drop_handler.h"
#include "bridge/player_bridge.h"
#include "bridge/install_bridge.h"
#include "bridge/instance_bridge.h"

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

    // ---- QML type registrations (manual, since liblpclcore has no QML macros) ----
    // Singletons
    qmlRegisterSingletonInstance("LPCL", 1, 0, "Settings", &Settings::instance());
    qmlRegisterSingletonInstance("LPCL", 1, 0, "JavaManager", &JavaManager::instance());
    qmlRegisterSingletonInstance("LPCL", 1, 0, "VersionManager", &VersionManager::instance());
    qmlRegisterSingletonInstance("LPCL", 1, 0, "Launcher", &Launcher::instance());
    qmlRegisterSingletonInstance("LPCL", 1, 0, "DownloadManager", &DownloadManager::instance());
    // OfflineAuth is a singleton (created on demand)
    qmlRegisterSingletonType<OfflineAuth>("LPCL", 1, 0, "OfflineAuth",
        [](QQmlEngine *, QJSEngine *) -> QObject* { return new OfflineAuth(); });
    // MsAuth is a regular type (instantiated per login)
    qmlRegisterType<MsAuth>("LPCL", 1, 0, "MsAuth");

    // FileDropHandler — manual registration because it needs setupWindow()
    FileDropHandler *dropHandler = new FileDropHandler(&app);
    qmlRegisterSingletonInstance("LPCL", 1, 0, "FileDropHandler", dropHandler);

    // GUI 桥接层（包装 lpcl:: 自由函数为 QML 可调用的单例）
    qmlRegisterSingletonInstance("LPCL", 1, 0, "PlayerBridge", &PlayerBridge::instance());
    qmlRegisterSingletonInstance("LPCL", 1, 0, "InstallBridge", &InstallBridge::instance());
    qmlRegisterSingletonInstance("LPCL", 1, 0, "InstanceBridge", &InstanceBridge::instance());

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
                source: "qrc:/gui/assets/logo.svg"
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
