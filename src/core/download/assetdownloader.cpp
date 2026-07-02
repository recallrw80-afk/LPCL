#include "download/assetdownloader.h"
#include "download/downloadmanager.h"
#include "util/file_utils.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QLoggingCategory>
#include <QNetworkReply>

static Q_LOGGING_CATEGORY(logAsset, "lpcl.asset")

AssetDownloader& AssetDownloader::instance()
{
    static AssetDownloader m;
    return m;
}

====
// Full pipeline
====

void AssetDownloader::downloadVersion(const QString &versionId,
                                       std::function<void(bool, QString)> onComplete)
{
    emit downloadLog("Starting download for version: " + versionId);
    emit downloadProgress("Fetching version manifest...", 0, 5);

    // Step 1: Get version manifest
    DownloadManager::instance().downloadJson(
        "https://launchermeta.mojang.com/mc/game/version_manifest.json",
        [this, versionId, onComplete](bool ok, QString err, json manifest) {
            if (!ok) {
                emit downloadLog("Failed: " + err);
                if (onComplete) onComplete(false, err);
                return;
            }

            QString versionUrl;
            auto &versions = manifest["versions"];
            for (const auto &v : versions) {
                if (v.value("id", "") == versionId.toStdString()) {
                    versionUrl = QString::fromStdString(v.value("url", ""));
                    break;
                }
            }
            if (versionUrl.isEmpty()) {
                QString msg = "Version not found: " + versionId;
                emit downloadLog(msg);
                if (onComplete) onComplete(false, msg);
                return;
            }

            // Step 2: Download version JSON
            emit downloadProgress("Downloading version JSON...", 1, 5);
            DownloadManager::instance().downloadJson(
                versionUrl, [this, versionId, onComplete](bool ok2, QString err2, json verJson) {
                    if (!ok2) {
                        emit downloadLog("Failed to download version JSON: " + err2);
                        if (onComplete) onComplete(false, err2);
                        return;
                    }

                    // Save version JSON locally
                    QString versionsDir = DownloadManager::instance().property("mcFolder").toString();
                    QString jsonPath = versionsDir + versionId + "/" + versionId + ".json";
                    QDir().mkpath(QFileInfo(jsonPath).absolutePath());
                    QFile f(jsonPath);
                    if (f.open(QIODevice::WriteOnly)) {
                        f.write(QJsonDocument(QJsonDocument::fromJson(
                            QString::fromStdString(verJson.dump()).toUtf8())).toJson());
                        f.close();
                    }

                    // Build a McVersion for path references
                    McVersion ver;
                    ver.id = versionId;
                    ver.pathVersion = QFileInfo(jsonPath).absolutePath() + "/";
                    ver.pathJar = ver.pathVersion + versionId + ".jar";
                    ver.pathIndie = QFileInfo(versionsDir).absolutePath() + "/";

                    // Parse asset index
                    QString assetIndex;
                    if (verJson.contains("assetIndex")) {
                        assetIndex = QString::fromStdString(verJson["assetIndex"].value("id", ""));
                    } else if (verJson.contains("assets")) {
                        assetIndex = QString::fromStdString(verJson["assets"].get<std::string>());
                    }

                    // Step 3: Download client JAR
                    emit downloadProgress("Downloading client JAR...", 2, 5);
                    downloadClientJar(ver, [this, ver, verJson, assetIndex, onComplete]
                        (bool ok3, QString err3) {
                        if (!ok3) {
                            emit downloadLog("JAR download failed: " + err3);
                            if (onComplete) onComplete(false, err3);
                            return;
                        }

                        // Step 4: Download libraries
                        emit downloadProgress("Downloading libraries...", 3, 5);
                        downloadLibraries(ver, verJson, [this, ver, assetIndex, onComplete]
                            (bool ok4, QString err4) {
                            if (!ok4) {
                                emit downloadLog("Library download failed: " + err4);
                                if (onComplete) onComplete(false, err4);
                                return;
                            }

                            // Step 5: Download assets
                            emit downloadProgress("Downloading assets...", 4, 5);
                            downloadAssets(ver, [this, onComplete](bool ok5, QString err5) {
                                emit downloadProgress("Complete!", 5, 5);
                                if (onComplete) onComplete(ok5, ok5 ? QString() : err5);
                            });
                        });
                    });
                });
        });
}

====
// Version JSON
====

void AssetDownloader::downloadVersionJson(const QString &versionId,
                                            std::function<void(bool, QString)> onComplete)
{
    downloadVersion(versionId, onComplete);
}

====
// Client JAR
====

void AssetDownloader::downloadClientJar(const McVersion &version,
                                          std::function<void(bool, QString)> onComplete)
{
    // The client JAR URL is derived from the version JSON's "downloads" -> "client" -> "url"
    // We need the version JSON. If it's already local, use it to get the download URL.
    QString jsonPath = version.pathVersion + version.id + ".json";

    QFile f(jsonPath);
    if (!f.open(QIODevice::ReadOnly)) {
        // Try direct Mojang download (without knowing the hash)
        QString fallbackUrl = QString(
            "https://launcher.mojang.com/v1/objects/%1/client.jar")
            .arg(version.id); // This isn't correct but serves as placeholder
        emit downloadLog("No version JSON found, cannot determine JAR download URL");
        if (onComplete) onComplete(false, "Version JSON not found");
        return;
    }

    QByteArray raw = f.readAll();
    f.close();
    json verJson = json::parse(raw.toStdString(), nullptr, false);

    if (verJson.is_discarded()) {
        if (onComplete) onComplete(false, "Invalid version JSON");
        return;
    }

    // Get download URL
    QString downloadUrl;
    QString expectedHash;

    if (verJson.contains("downloads") && verJson["downloads"].contains("client")) {
        auto &client = verJson["downloads"]["client"];
        downloadUrl = QString::fromStdString(client.value("url", ""));
        expectedHash = QString::fromStdString(client.value("sha1", ""));
    }

    if (downloadUrl.isEmpty()) {
        emit downloadLog("No client download URL in version JSON");
        if (onComplete) onComplete(false, "No download URL");
        return;
    }

    QString savePath = version.pathJar;
    QDir().mkpath(QFileInfo(savePath).absolutePath());

    // If file exists and hash matches, skip
    if (QFileInfo::exists(savePath) && !expectedHash.isEmpty()) {
        if (FileUtils::verifySha1(savePath, expectedHash)) {
            emit downloadLog("Client JAR already up-to-date: " + version.id);
            if (onComplete) onComplete(true, savePath);
            return;
        }
    }

    emit downloadLog("Downloading client JAR: " + downloadUrl);

    DownloadManager::instance().download(
        downloadUrl, savePath,
        [this](qint64 recv, qint64 total) {
            emit downloadProgress("Client JAR", (int)recv, (int)total);
        },
        [this, savePath, expectedHash, onComplete](bool success, QString msg) {
            if (!success) {
                if (onComplete) onComplete(false, msg);
                return;
            }
            // Verify SHA1
            if (!expectedHash.isEmpty() && !FileUtils::verifySha1(savePath, expectedHash)) {
                QFile::remove(savePath);
                emit downloadLog("Client JAR SHA1 mismatch");
                if (onComplete) onComplete(false, "SHA1 verification failed");
                return;
            }
            emit downloadLog("Client JAR downloaded: " + savePath);
            if (onComplete) onComplete(true, savePath);
        });
}

====
// Libraries
====

void AssetDownloader::downloadLibraries(const McVersion &version,
                                          const json &versionJson,
                                          std::function<void(bool, QString)> onComplete)
{
    if (!versionJson.contains("libraries") || !versionJson["libraries"].is_array()) {
        emit downloadLog("No libraries to download");
        if (onComplete) onComplete(true, QString());
        return;
    }

    // Count downloadable libraries (those with artifact downloads, passing rules)
    QList<std::pair<QString, QString>> toDownload; // <url, savePath>

    for (const auto &lib : versionJson["libraries"]) {
        // Check rules
        if (lib.contains("rules")) {
            bool allowed = false;
            for (const auto &rule : lib["rules"]) {
                bool ruleAllows = (rule.value("action", "allow") == "allow");
                bool matches = true;
                if (rule.contains("os")) {
                    std::string osName = rule["os"].value("name", "");
                    std::string osArch = rule["os"].value("arch", "");
                    if (!osName.empty()) {
                        switch (currentPlatform()) {
                        case Platform::Windows: matches = (osName == "windows"); break;
                        case Platform::Linux:   matches = (osName == "linux"); break;
                        case Platform::MacOS:   matches = (osName == "osx"); break;
                        default: break;
                        }
                    }
                    if (!osArch.empty()) {
                        matches = matches && (osArch == (is64BitSystem() ? "x86_64" : "x86"));
                    }
                }
                if (matches) allowed = ruleAllows;
            }
            if (!allowed) continue;
        }

        if (!lib.contains("downloads") || !lib["downloads"].contains("artifact")) continue;

        auto &artifact = lib["downloads"]["artifact"];
        std::string url = artifact.value("url", "");
        std::string path = artifact.value("path", "");
        std::string sha1 = artifact.value("sha1", "");

        if (url.empty() || path.empty()) continue;

        QString savePath = version.pathIndie + "libraries/" + QString::fromStdString(path);
        // Skip if already exists and hash matches
        if (QFileInfo::exists(savePath) && !sha1.empty()) {
            if (FileUtils::verifySha1(savePath, QString::fromStdString(sha1))) continue;
        }

        QDir().mkpath(QFileInfo(savePath).absolutePath());
        toDownload.append({QString::fromStdString(url), savePath});
    }

    if (toDownload.isEmpty()) {
        emit downloadLog("All libraries up-to-date");
        if (onComplete) onComplete(true, QString());
        return;
    }

    int total = toDownload.size();
    int *completed = new int(0);
    int *failed = new int(0);
    int *totalPtr = new int(total);

    emit downloadLog(QString("Downloading %1 libraries...").arg(total));
    emit downloadProgress("Libraries", 0, total);

    for (const auto &item : toDownload) {
        DownloadManager::instance().download(
            item.first, item.second,
            nullptr,
            [this, completed, failed, totalPtr, onComplete](bool success, QString) {
                (*completed)++;
                emit downloadProgress("Libraries", *completed, *totalPtr);
                if (!success) (*failed)++;

                if (*completed >= *totalPtr) {
                    int f = *failed;
                    delete completed; delete failed; delete totalPtr;
                    if (f > 0) {
                        QString msg = QString("%1/%2 libraries failed").arg(f).arg(*totalPtr);
                        emit downloadLog(msg);
                        if (onComplete) onComplete(*totalPtr - f > 0, msg);
                    } else {
                        emit downloadLog("All libraries downloaded");
                        if (onComplete) onComplete(true, QString());
                    }
                }
            });
    }
}

====
// Assets
====

void AssetDownloader::downloadAssets(const McVersion &version,
                                       std::function<void(bool, QString)> onComplete)
{
    // First download the asset index JSON
    QString jsonPath = version.pathVersion + version.id + ".json";
    QFile f(jsonPath);
    if (!f.open(QIODevice::ReadOnly)) {
        if (onComplete) onComplete(false, "Cannot read version JSON");
        return;
    }

    json verJson = json::parse(f.readAll().toStdString(), nullptr, false);
    f.close();

    QString assetIndexId;
    if (verJson.contains("assetIndex")) {
        assetIndexId = QString::fromStdString(verJson["assetIndex"].value("id", ""));
        if (assetIndexId.isEmpty() && verJson["assetIndex"].contains("id")) {
            // Already extracted above, but check the inner id
            assetIndexId = QString::fromStdString(verJson["assetIndex"]["id"].get<std::string>());
        }
    }
    if (assetIndexId.isEmpty() && verJson.contains("assets")) {
        assetIndexId = QString::fromStdString(verJson["assets"].get<std::string>());
    }

    QString assetIndexUrl;
    if (verJson.contains("assetIndex") && verJson["assetIndex"].contains("url")) {
        assetIndexUrl = QString::fromStdString(verJson["assetIndex"]["url"].get<std::string>());
    }

    if (assetIndexId.isEmpty()) {
        emit downloadLog("No asset index found, using legacy assets");
        if (onComplete) onComplete(true, QString());
        return;
    }

    emit downloadLog("Downloading asset index: " + assetIndexId);

    auto doAssetDownload = [this, version, onComplete](json assetIndexJson) {
        if (!assetIndexJson.contains("objects")) {
            if (onComplete) onComplete(true, QString());
            return;
        }

        auto &objects = assetIndexJson["objects"];
        QList<std::pair<QString, QString>> toDownload; // <url, savePath>

        for (auto it = objects.begin(); it != objects.end(); ++it) {
            std::string hash = it.value().value("hash", "");
            if (hash.empty()) continue;

            QString hashStr = QString::fromStdString(hash);
            QString subPath = FileUtils::assetPathFromHash(hashStr);
            QString savePath = version.pathIndie + "assets/objects/" + subPath;

            if (QFileInfo::exists(savePath) && FileUtils::verifySha1(savePath, hashStr))
                continue;

            QString url = "https://resources.download.minecraft.net/" + subPath;
            QDir().mkpath(QFileInfo(savePath).absolutePath());
            toDownload.append({url, savePath});
        }

        if (toDownload.isEmpty()) {
            emit downloadLog("All assets up-to-date");
            if (onComplete) onComplete(true, QString());
            return;
        }

        int total = toDownload.size();
        int *completed = new int(0);
        int *failed = new int(0);

        emit downloadLog(QString("Downloading %1 assets...").arg(total));
        emit downloadProgress("Assets", 0, total);

        for (const auto &item : toDownload) {
            DownloadManager::instance().download(
                item.first, item.second,
                nullptr,
                [this, completed, failed, total, onComplete](bool success, QString) {
                    (*completed)++;
                    if (!success) (*failed)++;
                    if (*completed % 50 == 0 || *completed >= total) {
                        emit downloadProgress("Assets", *completed, total);
                    }
                    if (*completed >= total) {
                        int f = *failed;
                        delete completed; delete failed;
                        if (f > 0) {
                            emit downloadLog(QString("%1/%2 assets failed").arg(f).arg(total));
                        } else {
                            emit downloadLog("All assets downloaded");
                        }
                        if (onComplete) onComplete(true, QString());
                    }
                });
        }
    };

    // Download asset index JSON
    if (!assetIndexUrl.isEmpty()) {
        DownloadManager::instance().downloadJson(
            assetIndexUrl,
            [doAssetDownload](bool ok, QString, json idxJson) {
                if (ok) doAssetDownload(idxJson);
            });
    } else {
        // Fallback: construct URL from asset index ID
        QString fallbackUrl = QString("https://launchermeta.mojang.com/v1/packages/%1.json")
                                  .arg(assetIndexId);
        DownloadManager::instance().downloadJson(
            fallbackUrl,
            [this, doAssetDownload, onComplete](bool ok, QString err, json idxJson) {
                if (!ok) {
                    emit downloadLog("Failed to download asset index: " + err);
                    if (onComplete) onComplete(false, err);
                    return;
                }
                doAssetDownload(idxJson);
            });
    }
}

====
// Natives
====

void AssetDownloader::downloadNatives(const McVersion &version,
                                        std::function<void(bool, QString)> onComplete)
{
    QString jsonPath = version.pathVersion + version.id + ".json";
    QFile f(jsonPath);
    if (!f.open(QIODevice::ReadOnly)) {
        if (onComplete) onComplete(false, "Cannot read version JSON");
        return;
    }

    json verJson = json::parse(f.readAll().toStdString(), nullptr, false);
    f.close();

    if (!verJson.contains("libraries")) {
        if (onComplete) onComplete(true, QString());
        return;
    }

    // Find native libraries matching current platform
    QList<std::pair<QString, QString>> nativesToDownload; // <url, savePath>

    QString nativeClassifier;
    switch (currentPlatform()) {
    case Platform::Windows: nativeClassifier = "natives-windows"; break;
    case Platform::Linux:   nativeClassifier = "natives-linux"; break;
    case Platform::MacOS:   nativeClassifier = "natives-macos"; break;
    default: break;
    }

    for (const auto &lib : verJson["libraries"]) {
        // Check rules
        if (lib.contains("rules")) {
            bool allowed = false;
            for (const auto &rule : lib["rules"]) {
                if (rule.value("action", "allow") == "allow") allowed = true;
                else allowed = false;
            }
            if (!allowed) continue;
        }

        if (!lib.contains("natives") && !lib.contains("downloads")) continue;

        auto &downloads = lib["downloads"];
        if (!downloads.contains("classifiers")) continue;

        auto &classifiers = downloads["classifiers"];
        std::string classifierKey = nativeClassifier.toStdString();
        if (currentPlatform() == Platform::Windows && is64BitSystem())
            classifierKey = "natives-windows-64";
        if (currentPlatform() == Platform::Linux && is64BitSystem())
            classifierKey = "natives-linux-64";

        if (!classifiers.contains(classifierKey)) {
            // Try without arch suffix
            if (!classifiers.contains(nativeClassifier.toStdString())) continue;
            classifierKey = nativeClassifier.toStdString();
        }

        auto &native = classifiers[classifierKey];
        std::string url = native.value("url", "");
        std::string path = lib.contains("name")
            ? std::string("libraries/") + lib["name"].get<std::string>()
            : native.value("path", "");

        if (url.empty()) continue;

        QString savePath = version.pathIndie +
                           QString::fromStdString(path).replace('/', QDir::separator());
        QDir().mkpath(QFileInfo(savePath).absolutePath());

        // Check if the native JAR already exists
        if (QFileInfo::exists(savePath)) {
            // Check SHA1 if available
            std::string sha1 = native.value("sha1", "");
            if (!sha1.empty() && FileUtils::verifySha1(savePath, QString::fromStdString(sha1)))
                continue;
        }

        nativesToDownload.append({QString::fromStdString(url), savePath});
    }

    if (nativesToDownload.isEmpty()) {
        emit downloadLog("No native libraries to download");
        if (onComplete) onComplete(true, QString());
        return;
    }

    int total = nativesToDownload.size();
    int *completed = new int(0);

    QString nativesDir = version.pathVersion + "natives/";
    QDir().mkpath(nativesDir);

    emit downloadLog(QString("Downloading %1 native libraries...").arg(total));
    emit downloadProgress("Natives", 0, total);

    for (const auto &item : nativesToDownload) {
        DownloadManager::instance().download(
            item.first, item.second,
            nullptr,
            [this, completed, total, nativesDir, onComplete](bool success, QString) {
                (*completed)++;
                emit downloadProgress("Natives", *completed, total);
                if (*completed >= total) {
                    delete completed;
                    emit downloadLog("Native libraries downloaded to: " + nativesDir);
                    if (onComplete) onComplete(true, QString());
                }
            });
    }
}

