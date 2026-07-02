#include "core/installer.h"
#include "download/downloadmanager.h"
#include "core/settings.h"
#include "util/platform_utils.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

static Q_LOGGING_CATEGORY(logInstall, "lpcl.installer")

const QString Installer::FORGE_API = "https://files.minecraftforge.net/net/minecraftforge/forge";
const QString Installer::FABRIC_API = "https://meta.fabricmc.net/v2";
const QString Installer::NEOFORGE_API = "https://maven.neoforged.net/releases/net/neoforged/neoforge";

Installer& Installer::instance()
{
    static Installer i;
    return i;
}

====
// URL helpers
====

QString Installer::getInstallerUrl(const QString &loaderType, const QString &mcVersion)
{
    if (loaderType == "forge") {
        // Forge installer pattern:
        // https://maven.minecraftforge.net/net/minecraftforge/forge/{mcVersion}-{forgeVersion}/forge-{mcVersion}-{forgeVersion}-installer.jar
        QString forgeVer = detectBestForgeVersion(mcVersion);
        if (forgeVer.isEmpty()) return {};
        return QString("%1/index_%2.html").arg(FORGE_API, mcVersion);
    }
    if (loaderType == "fabric") {
        return FABRIC_API + "/versions/loader/" + mcVersion;
    }
    if (loaderType == "neoforge") {
        QString neoVer = detectBestNeoForgeVersion(mcVersion);
        if (neoVer.isEmpty()) return {};
        return NEOFORGE_API + "/" + neoVer + "/neoforge-" + neoVer + "-installer.jar";
    }
    return {};
}

void Installer::downloadInstaller(const QString &loaderType, const QString &mcVersion,
                                    const QString &loaderVersion,
                                    std::function<void(bool, QString)> onComplete)
{
    QString jarName;
    QString url;

    if (loaderType == "forge") {
        jarName = QString("forge-%1-%2-installer.jar").arg(mcVersion, loaderVersion);
        url = QString("https://maven.minecraftforge.net/net/minecraftforge/forge/%1-%2/%3")
                  .arg(mcVersion, loaderVersion, jarName);
    } else if (loaderType == "fabric") {
        jarName = "fabric-installer.jar";
        url = FABRIC_API + "/versions/loader/" + mcVersion + "/" + loaderVersion + "/server/jar";
    } else if (loaderType == "neoforge") {
        jarName = QString("neoforge-%1-installer.jar").arg(loaderVersion);
        url = NEOFORGE_API + "/" + loaderVersion + "/" + jarName;
    } else {
        if (onComplete) onComplete(false, "Unknown loader: " + loaderType);
        return;
    }

    QString savePath = Settings::instance().getString("PathTemp", "/tmp/") + jarName;
    emit installLog("Downloading " + jarName + "...");
    DownloadManager::instance().download(url, savePath,
        nullptr, [onComplete, savePath](bool ok, QString err) {
            if (onComplete) onComplete(ok, ok ? savePath : err);
        });
}

====
// Installer runner
====

void Installer::runInstallerJar(const QString &jarPath, const QString &javaPath,
                                  const QStringList &args,
                                  std::function<void(bool, QString)> onComplete)
{
    m_isRunning = true;
    emit runningChanged();

    QProcess *proc = new QProcess(this);
    proc->setProgram(javaPath);
    proc->setArguments(args);

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc, onComplete](int exitCode, QProcess::ExitStatus) {
        proc->deleteLater();
        m_isRunning = false;
        emit runningChanged();

        if (exitCode == 0) {
            m_statusText = "Installation complete";
            emit statusTextChanged();
            if (onComplete) onComplete(true, QString());
        } else {
            QString err = QString::fromUtf8(proc->readAllStandardError());
            qCWarning(logInstall) << "Installer failed:" << err;
            m_statusText = "Installation failed";
            emit statusTextChanged();
            if (onComplete) onComplete(false, err);
        }
    });

    connect(proc, &QProcess::readyReadStandardOutput, this, [this, proc]() {
        QString line = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        if (!line.isEmpty()) emit installLog(line);
    });

    // Log the command
    QString cmd = javaPath;
    for (const auto &a : args) cmd += " " + a;
    emit installLog("Running: " + cmd);
    qCInfo(logInstall) << "Running installer:" << cmd;

    proc->start();
    m_statusText = "Installing...";
    emit statusTextChanged();
    emit installProgress("Installing...", 50);
}

====
// Forge
====

void Installer::installForge(const QString &mcVersionDir, const QString &mcVersion,
                               const QString &forgeVersion, const QString &javaPath,
                               std::function<void(bool, QString)> onComplete)
{
    downloadInstaller("forge", mcVersion, forgeVersion,
        [this, mcVersionDir, javaPath, onComplete](bool ok, QString jarPath) {
            if (!ok) { if (onComplete) onComplete(false, jarPath); return; }

            QStringList args;
            args << "-jar" << jarPath;
            args << "--installClient";
            args << mcVersionDir;
            runInstallerJar(jarPath, javaPath, {"-jar", jarPath, "--installClient", mcVersionDir},
                            onComplete);
        });
}

void Installer::installFabric(const QString &mcVersionDir, const QString &mcVersion,
                                const QString &loaderVersion, const QString &javaPath,
                                std::function<void(bool, QString)> onComplete)
{
    downloadInstaller("fabric", mcVersion, loaderVersion,
        [this, mcVersionDir, mcVersion, javaPath, onComplete](bool ok, QString jarPath) {
            if (!ok) { if (onComplete) onComplete(false, jarPath); return; }
            QStringList args;
            args << "-jar" << jarPath << "client" << "-dir" << mcVersionDir
                 << "-mcversion" << mcVersion;
            runInstallerJar(jarPath, javaPath, args, onComplete);
        });
}

void Installer::installNeoForge(const QString &mcVersionDir, const QString &mcVersion,
                                  const QString &neoVersion, const QString &javaPath,
                                  std::function<void(bool, QString)> onComplete)
{
    downloadInstaller("neoforge", mcVersion, neoVersion,
        [this, mcVersionDir, javaPath, onComplete](bool ok, QString jarPath) {
            if (!ok) { if (onComplete) onComplete(false, jarPath); return; }
            runInstallerJar(jarPath, javaPath, {"-jar", jarPath, "--installClient", mcVersionDir},
                            onComplete);
        });
}

void Installer::installLoader(const QString &loaderType, const QString &mcVersionDir,
                                const QString &loaderVersion, const QString &javaPath,
                                std::function<void(bool, QString)> onComplete)
{
    // Extract MC version from directory name
    QString mcVersion;
    QRegularExpression re(R"(\d+\.\d+(?:\.\d+)?)");
    auto match = re.match(QFileInfo(mcVersionDir).fileName());
    if (match.hasMatch()) mcVersion = match.captured(1);

    if (loaderType == "forge") installForge(mcVersionDir, mcVersion, loaderVersion, javaPath, onComplete);
    else if (loaderType == "fabric") installFabric(mcVersionDir, mcVersion, loaderVersion, javaPath, onComplete);
    else if (loaderType == "neoforge") installNeoForge(mcVersionDir, mcVersion, loaderVersion, javaPath, onComplete);
    else if (onComplete) onComplete(false, "Unsupported loader: " + loaderType);
}

====
// Version detection — fetch available versions from APIs
====

void Installer::fetchForgeVersions(std::function<void(bool, QStringList)> onComplete)
{
    QString url = FORGE_API + "/index.html";
    DownloadManager::instance().downloadToString(url, [onComplete](bool ok, QString html) {
        QStringList versions;
        if (!ok) { onComplete(false, versions); return; }
        // Parse HTML to extract version links
        QRegularExpression re(R"(forge-(\d+\.\d+(?:\.\d+)?-\d+\.\d+\.\d+)-installer)");
        auto it = re.globalMatch(html);
        while (it.hasNext()) {
            auto m = it.next();
            versions.append(m.captured(1));
        }
        onComplete(true, versions);
    });
}

void Installer::fetchFabricVersions(std::function<void(bool, QStringList)> onComplete)
{
    QString url = FABRIC_API + "/versions/loader";
    DownloadManager::instance().downloadJson(url, [onComplete](bool ok, QString, json arr) {
        QStringList versions;
        if (!ok || !arr.is_array()) { onComplete(false, versions); return; }
        for (const auto &v : arr) {
            versions.append(QString::fromStdString(
                v.value("version", v.value("loader", json::object()).value("version", ""))));
        }
        // Remove empty and dedup
        versions.removeAll({});
        versions.removeDuplicates();
        onComplete(true, versions);
    });
}

void Installer::fetchNeoForgeVersions(std::function<void(bool, QStringList)> onComplete)
{
    // NeoForge uses Maven metadata
    QString url = NEOFORGE_API + "/maven-metadata.xml";
    DownloadManager::instance().downloadToString(url, [onComplete](bool ok, QString xml) {
        QStringList versions;
        if (!ok) { onComplete(false, versions); return; }
        QRegularExpression re(R"(<version>([\d.]+)</version>)");
        auto it = re.globalMatch(xml);
        while (it.hasNext()) {
            auto m = it.next();
            versions.append(m.captured(1));
        }
        onComplete(true, versions);
    });
}

QString Installer::detectBestForgeVersion(const QString &mcVersion)
{
    // Best-effort: return the recommended Forge version for this MC version
    // For production, this should call the Forge API
    // Known stable versions as fallback:
    QMap<QString, QString> knownStable = {
        {"1.20.1", "47.3.0"},
        {"1.20.4", "49.0.47"},
        {"1.19.4", "45.2.9"},
        {"1.19.2", "43.4.2"},
        {"1.18.2", "40.2.14"},
        {"1.16.5", "36.2.39"},
        {"1.12.2", "14.23.5.2859"},
        {"1.8.9",  "11.15.1.2318"},
    };
    return knownStable.value(mcVersion);
}

QString Installer::detectBestFabricVersion(const QString &)
{
    return "0.16.10"; // Latest stable Fabric loader
}

QString Installer::detectBestNeoForgeVersion(const QString &mcVersion)
{
    QMap<QString, QString> knownStable = {
        {"1.20.1", "47.1.109"},
        {"1.20.4", "68.1.66"},
        {"1.21",   "21.1.66"},
    };
    return knownStable.value(mcVersion);
}
