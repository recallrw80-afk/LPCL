#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QIcon>
#include <QDir>
#include <QSurfaceFormat>
#include <QLoggingCategory>

#include "src/core/settings.h"
#include "src/core/javamanager.h"
#include "src/core/versionmanager.h"
#include "src/core/launchbuilder.h"
#include "src/core/launcher.h"
#include "src/core/auth/offlineauth.h"
#include "src/core/download/downloadmanager.h"
#include "src/core/download/assetdownloader.h"

int main(int argc, char *argv[])
{
    // Enable multisample anti-aliasing for smooth rounded corners and shapes
    QSurfaceFormat fmt;
    fmt.setSamples(8);
    QSurfaceFormat::setDefaultFormat(fmt);

    QGuiApplication app(argc, argv);
    app.setApplicationName("LPCL");
    app.setApplicationVersion("0.1");
    app.setOrganizationName("PCL");

    // Initialize settings
    Settings::initialize();

    // Initialize managers
    auto &javaMgr = JavaManager::instance();
    auto &verMgr = VersionManager::instance();
    auto &launcher = Launcher::instance();
    auto &downloadMgr = DownloadManager::instance();

    // Set up logging
    QLoggingCategory::setFilterRules("pcl.*.debug=true");

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

    // Register C++ singletons for QML access
    // These become available in QML as: import PCL.Core 1.0
    qmlRegisterSingletonInstance("PCL.Core", 1, 0, "JavaManager", &javaMgr);
    qmlRegisterSingletonInstance("PCL.Core", 1, 0, "VersionManager", &verMgr);
    qmlRegisterSingletonInstance("PCL.Core", 1, 0, "Launcher", &launcher);
    qmlRegisterSingletonInstance("PCL.Core", 1, 0, "DownloadManager", &downloadMgr);
    qmlRegisterSingletonInstance("PCL.Core", 1, 0, "Settings", &Settings::instance());

    // Register types for enums
    qmlRegisterUncreatableType<Launcher>("PCL.Core", 1, 0, "LaunchState",
                                         "LaunchState enum only");
    qmlRegisterUncreatableType<VersionManager>("PCL.Core", 1, 0, "VersionMgr",
                                               "VersionManager singleton only");

    // Register OfflineAuth static methods
    qmlRegisterSingletonType<OfflineAuth>("PCL.Core", 1, 0, "OfflineAuth",
        [](QQmlEngine *, QJSEngine *) -> QObject* {
            return new OfflineAuth();
        });

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

    engine.loadFromModule("LPCL", "Main");

    return app.exec();
}
