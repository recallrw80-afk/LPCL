#include "lpcl.h"
#include "core/settings.h"
#include "core/versionmanager.h"
#include "core/javamanager.h"
#include "core/launcher.h"
#include "auth/offlineauth.h"
#include <QCoreApplication>
#include <QDir>

namespace lpcl {

QStringList listVersions() {
    auto &vm = VersionManager::instance();
    vm.loadLocalVersions();
    return vm.versionIds();
}

bool installVersion(const QString &versionId) {
    Q_UNUSED(versionId);
    // TODO: Phase 4
    return true;
}

bool launchVersion(const QString &versionId,
                   LogCallback onLog,
                   ExitCallback onExit) {
    auto &launcher = Launcher::instance();
    auto &jm = JavaManager::instance();

    auto login = OfflineAuth::createOfflineLogin("Player");
    auto version = VersionManager::instance().loadVersion(versionId);
    auto *java = jm.selectJava();
    if (!java) return false;

    if (onLog) {
        QObject::connect(&launcher, &Launcher::gameLog,
                         [onLog](const QString &line) { onLog(line); });
    }
    if (onExit) {
        QObject::connect(&launcher, &Launcher::gameExited,
                         [onExit](int code, const QString &) { onExit(code); });
    }

    return launcher.launch(version, *java, login);
}

QStringList listJavas() {
    auto &jm = JavaManager::instance();
    jm.scanSystemJava();
    return jm.javaNames();
}

} // namespace lpcl
