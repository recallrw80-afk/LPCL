#include "core/versionmanager.h"
#include "core/settings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QSet>

static Q_LOGGING_CATEGORY(logVer, "lpcl.version")

VersionManager& VersionManager::instance()
{
    static VersionManager m;
    return m;
}

// ============================================================================
// Minecraft folder
// ============================================================================

void VersionManager::setMcFolder(const QString &path)
{
    QString normalized = QDir(path).absolutePath() + "/";
    if (m_mcFolder == normalized) return;

    m_mcFolder = normalized;
    Settings::instance().setString("LaunchFolderSelect", path);
    qCInfo(logVer) << "Minecraft folder set to:" << m_mcFolder;
    emit mcFolderChanged(m_mcFolder);

    // Reload versions
    loadLocalVersions();
}

QList<McFolder> VersionManager::loadFolderList()
{
    QList<McFolder> folders;

    // 1. Check exe folder (current launcher directory)
    QString exeFolder = QCoreApplication::applicationDirPath() + "/";
    if (QDir(exeFolder + "versions").exists()) {
        folders.append({.name = "Current Folder", .location = exeFolder, .type = McFolder::Type::Vanilla});
    }

    // Scan subdirectories of exe folder for MC installations
    QDir exeDir(exeFolder);
    for (const auto &entry : exeDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QString subPath = entry.absoluteFilePath() + "/";
        if (QDir(subPath + "versions").exists() || entry.fileName() == ".minecraft") {
            bool alreadyExists = false;
            for (const auto &f : folders) {
                if (f.location == subPath) { alreadyExists = true; break; }
            }
            if (!alreadyExists) {
                folders.append({.name = "Current Folder", .location = subPath, .type = McFolder::Type::Vanilla});
            }
        }
    }

    // 2. Official launcher folder
    QString mojangPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    // On Windows: %APPDATA%/.minecraft
    // On Linux: ~/.minecraft
    // On Mac: ~/Library/Application Support/minecraft
    QString officialPath;
    switch (currentPlatform()) {
    case Platform::Windows:
        officialPath = qEnvironmentVariable("APPDATA") + "/.minecraft/";
        break;
    case Platform::Linux:
        officialPath = QDir::homePath() + "/.minecraft/";
        break;
    case Platform::MacOS:
        officialPath = QDir::homePath() + "/Library/Application Support/minecraft/";
        break;
    default:
        officialPath = QDir::homePath() + "/.minecraft/";
        break;
    }

    if (QDir(officialPath + "versions").exists()) {
        bool alreadyExists = false;
        for (const auto &f : folders) {
            if (f.location == officialPath) { alreadyExists = true; break; }
        }
        if (!alreadyExists) {
            folders.append({.name = "Official Launcher", .location = officialPath, .type = McFolder::Type::Vanilla});
        }
    }

    // 3. Custom folders from settings
    QString customStr = Settings::instance().getString("LaunchFolders");
    if (!customStr.isEmpty()) {
        const auto entries = customStr.split('|', Qt::SkipEmptyParts);
        for (const auto &entry : entries) {
            if (!entry.contains('>')) continue;
            QString name = entry.section('>', 0, 0);
            QString loc = entry.section('>', 1);
            if (!loc.endsWith('/')) loc += '/';

            bool exists = false;
            for (auto &f : folders) {
                if (f.location == loc) {
                    f.name = name;
                    f.type = McFolder::Type::RenamedVanilla;
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                folders.append({.name = name, .location = loc, .type = McFolder::Type::Custom});
            }
        }
    }

    return folders;
}

// ============================================================================
// Version listing
// ============================================================================

void VersionManager::loadLocalVersions()
{
    if (m_mcFolder.isEmpty()) return;

    m_isLoading = true;
    emit loadingChanged();
    emit versionLoadProgress("Loading local versions...");

    m_versionList.clear();

    QString versionsDir = m_mcFolder + "versions/";
    QDir dir(versionsDir);
    if (!dir.exists()) {
        m_isLoading = false;
        emit loadingChanged();
        return;
    }

    for (const auto &entry : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QString jsonPath = entry.absoluteFilePath() + "/" + entry.fileName() + ".json";
        if (!QFileInfo::exists(jsonPath)) continue;

        McVersionInfo info;
        info.id = entry.fileName();
        info.isLocal = true;

        // Try to get more info from the JSON
        try {
            QFile file(jsonPath);
            if (file.open(QIODevice::ReadOnly)) {
                QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
                QJsonObject root = doc.object();
                info.type = root.value("type").toString("unknown");
                QString timeStr = root.value("releaseTime").toString();
                if (!timeStr.isEmpty()) {
                    info.releaseTime = QDateTime::fromString(timeStr, Qt::ISODate);
                }
            }
        } catch (...) {
            // Use defaults
        }

        m_versionList.append(info);
    }

    // Sort: releases first, then snapshots, then others; newest first
    std::sort(m_versionList.begin(), m_versionList.end(), [](const McVersionInfo &a, const McVersionInfo &b) {
        if (a.type != b.type) {
            if (a.type == "release") return true;
            if (b.type == "release") return false;
        }
        return a.releaseTime > b.releaseTime;
    });

    emit versionListChanged();
    m_isLoading = false;
    emit loadingChanged();

    qCInfo(logVer) << "Loaded" << m_versionList.size() << "local versions from" << versionsDir;
}

void VersionManager::fetchVersionManifest()
{
    m_isLoading = true;
    emit loadingChanged();
    emit versionLoadProgress("Fetching remote version list...");

    QNetworkRequest request(QUrl("https://launchermeta.mojang.com/mc/game/version_manifest.json"));
    request.setRawHeader("User-Agent", "LPCL/0.1");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    if (!m_nam) {
        m_nam = new QNetworkAccessManager(this);
    }

    QNetworkReply *reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(logVer) << "Failed to fetch version manifest:" << reply->errorString();
            m_isLoading = false;
            emit loadingChanged();
            emit versionLoadError("", "Network error: " + reply->errorString());
            return;
        }

        QByteArray data = reply->readAll();
        json manifest = json::parse(data.toStdString(), nullptr, false);
        if (manifest.is_discarded() || !manifest.contains("versions")) {
            qCWarning(logVer) << "Invalid version manifest JSON";
            m_isLoading = false;
            emit loadingChanged();
            return;
        }

        // Cache manifest to disk
        QString cacheDir = mcFolder() + "versions/";
        QDir().mkpath(cacheDir);
        QFile cacheFile(cacheDir + "version_manifest.json");
        if (cacheFile.open(QIODevice::WriteOnly)) {
            cacheFile.write(data);
            cacheFile.close();
        }

        // Parse remote versions
        QList<McVersionInfo> remoteVersions;
        for (const auto &v : manifest["versions"]) {
            McVersionInfo info;
            info.id = QString::fromStdString(v.value("id", ""));
            info.type = QString::fromStdString(v.value("type", ""));
            info.url = QString::fromStdString(v.value("url", ""));
            QString timeStr = QString::fromStdString(v.value("releaseTime", ""));
            if (!timeStr.isEmpty())
                info.releaseTime = QDateTime::fromString(timeStr, Qt::ISODate);
            info.isLocal = false;
            remoteVersions.append(info);
        }

        // Merge: add remote versions not already in local list
        QSet<QString> localIds;
        for (const auto &v : m_versionList) localIds.insert(v.id);
        for (const auto &rv : remoteVersions) {
            if (!localIds.contains(rv.id))
                m_versionList.append(rv);
        }

        // Sort: releases first, then by releaseTime desc
        std::sort(m_versionList.begin(), m_versionList.end(),
                  [](const McVersionInfo &a, const McVersionInfo &b) {
            if (a.type != b.type) {
                if (a.type == "release") return true;
                if (b.type == "release") return false;
            }
            return a.releaseTime > b.releaseTime;
        });

        emit versionListChanged();
        m_isLoading = false;
        emit loadingChanged();
        qCInfo(logVer) << "Fetched" << remoteVersions.size()
                        << "remote versions, total:" << m_versionList.size();
    });
}

QStringList VersionManager::versionIds() const
{
    QStringList ids;
    for (const auto &v : m_versionList) {
        ids.append(v.id);
    }
    return ids;
}

// ============================================================================
// Version parsing
// ============================================================================

McVersion VersionManager::loadVersion(const QString &versionId)
{
    QString jsonPath = m_mcFolder + "versions/" + versionId + "/" + versionId + ".json";
    return parseVersionJson(jsonPath);
}

McVersion VersionManager::parseVersionJson(const QString &jsonPath)
{
    McVersion ver;

    QFile file(jsonPath);
    if (!file.open(QIODevice::ReadOnly)) {
        ver.isValid = false;
        ver.info = "Cannot open version JSON: " + jsonPath;
        return ver;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isNull()) {
        ver.isValid = false;
        ver.info = "Invalid JSON in: " + jsonPath;
        return ver;
    }

    QJsonObject root = doc.object();

    // Convert QJsonObject to nlohmann::json for easier handling
    QString jsonStr = QString::fromUtf8(doc.toJson());
    json j = json::parse(jsonStr.toStdString(), nullptr, false);
    if (j.is_discarded()) {
        ver.isValid = false;
        ver.info = "Failed to parse JSON";
        return ver;
    }

    ver = parseVersionJson(j, QFileInfo(jsonPath).dir().dirName());
    ver.pathJson = jsonPath;
    ver.pathVersion = QFileInfo(jsonPath).absolutePath() + "/";
    ver.pathJar = ver.pathVersion + ver.id + ".jar";
    // pathIndie may differ for version isolation — same as pathVersion for now
    ver.pathIndie = m_mcFolder;

    return ver;
}

McVersion VersionManager::parseVersionJson(const json &j, const QString &versionId)
{
    McVersion ver;
    ver.id = versionId;
    ver.isValid = true;

    // Basic fields
    ver.type = QString::fromStdString(j.value("type", "unknown"));
    QString timeStr = QString::fromStdString(j.value("releaseTime", ""));
    if (!timeStr.isEmpty()) {
        ver.releaseTime = QDateTime::fromString(timeStr, Qt::ISODate);
    }

    // Inheritance
    ver.inheritName = QString::fromStdString(j.value("inheritsFrom", ""));

    // Detect mod loaders
    ver.modLoader = detectModLoaders(j);

    // Detect vanilla version
    QString vanillaStr = detectVanillaVersion(j, versionId);
    ver.vanillaVersion = QVersionNumber::fromString(vanillaStr);

    return ver;
}

// ============================================================================
// Mod loader detection
// ============================================================================

McModLoaderInfo VersionManager::detectModLoaders(const json &versionJson)
{
    McModLoaderInfo info;

    // Forge: has "inheritsFrom" and has "forge" in the version ID
    // Or has "arguments.jvm" referencing forge client
    if (versionJson.contains("inheritsFrom")) {
        std::string inherit = versionJson["inheritsFrom"].get<std::string>();
        std::string versionId;

        if (versionJson.contains("id")) {
            versionId = versionJson["id"].get<std::string>();
        }

        QString qVerId = QString::fromStdString(versionId).toLower();
        QString qInherit = QString::fromStdString(inherit).toLower();

        // Forge detection
        if (qVerId.contains("forge") || qVerId.contains("forge_")) {
            info.hasForge = true;
            // Extract Forge version from the version ID
            // Format: 1.20.1-forge-47.2.0
            QRegularExpression forgeRe("forge[-_]([\\d.]+)");
            auto match = forgeRe.match(QString::fromStdString(versionId));
            if (match.hasMatch()) {
                info.forgeVersion = match.captured(1);
            }
        }

        // Fabric detection
        if (qVerId.contains("fabric")) {
            info.hasFabric = true;
        }

        // NeoForge detection
        if (qVerId.contains("neoforge")) {
            info.hasNeoForge = true;
        }

        // OptiFine detection
        if (qVerId.contains("optifine")) {
            info.hasOptiFine = true;
        }

        // LiteLoader detection
        if (qVerId.contains("liteloader")) {
            info.hasLiteLoader = true;
        }
    }

    // Check for mod loader metadata in the JSON
    // Fabric puts its version in a custom field
    if (versionJson.contains("fabricLoader")) {
        info.hasFabric = true;
        info.fabricVersion = QString::fromStdString(
            versionJson["fabricLoader"].value("version", ""));
    }

    return info;
}

QString VersionManager::detectVanillaVersion(const json &versionJson, const QString &versionId)
{
    // For modded versions, the vanilla version is in the inheritsFrom or the ID
    if (versionJson.contains("inheritsFrom")) {
        QString inherit = QString::fromStdString(versionJson["inheritsFrom"].get<std::string>());
        // If inheritsFrom is something like "1.20.1", that's the vanilla version
        if (!inherit.contains("forge") && !inherit.contains("fabric") &&
            !inherit.contains("neoforge") && !inherit.contains("optifine")) {
            return inherit;
        }
    }

    // Try to extract from version ID
    // "1.20.1-forge-47.2.0" -> "1.20.1"
    // "fabric-loader-0.15.7-1.20.1" -> "1.20.1"
    QRegularExpression vanillaRe(R"(^(\d+\.\d+(?:\.\d+)?))");
    auto match = vanillaRe.match(versionId);
    if (match.hasMatch()) {
        return match.captured(1);
    }

    return versionId;
}

// ============================================================================
// Inheritance resolution
// ============================================================================

json VersionManager::resolveInheritanceChain(const QString &jsonPath)
{
    QFile file(jsonPath);
    if (!file.open(QIODevice::ReadOnly)) return json();

    QString jsonStr = QString::fromUtf8(file.readAll());
    json result = json::parse(jsonStr.toStdString(), nullptr, false);
    if (result.is_discarded()) return json();

    // If no inheritance, return as-is
    if (!result.contains("inheritsFrom")) return result;

    std::string inheritId = result["inheritsFrom"].get<std::string>();
    QDir versionDir = QFileInfo(jsonPath).absoluteDir();
    versionDir.cdUp(); // Go to versions dir
    QString parentPath = versionDir.absolutePath() + "/" + QString::fromStdString(inheritId) + "/" +
                         QString::fromStdString(inheritId) + ".json";

    json parent = resolveInheritanceChain(parentPath);
    if (parent.is_discarded()) return result;

    // Merge: child overrides parent
    // Merge arguments.jvm
    if (parent.contains("arguments") && parent["arguments"].contains("jvm")) {
        if (!result.contains("arguments")) result["arguments"] = json::object();
        if (!result["arguments"].contains("jvm")) {
            result["arguments"]["jvm"] = parent["arguments"]["jvm"];
        }
    }

    // Merge arguments.game
    if (parent.contains("arguments") && parent["arguments"].contains("game")) {
        if (!result.contains("arguments")) result["arguments"] = json::object();
        if (!result["arguments"].contains("game")) {
            result["arguments"]["game"] = parent["arguments"]["game"];
        }
    }

    // Merge libraries
    if (parent.contains("libraries")) {
        if (!result.contains("libraries")) {
            result["libraries"] = parent["libraries"];
        } else {
            // Append parent libraries not already present
            for (const auto &lib : parent["libraries"]) {
                result["libraries"].push_back(lib);
            }
        }
    }

    // Merge mainClass
    if (parent.contains("mainClass") && !result.contains("mainClass")) {
        result["mainClass"] = parent["mainClass"];
    }

    // Merge minecraftArguments (old format)
    if (parent.contains("minecraftArguments") && !result.contains("minecraftArguments")) {
        result["minecraftArguments"] = parent["minecraftArguments"];
    }

    // Merge assetIndex
    if (parent.contains("assetIndex") && !result.contains("assetIndex")) {
        result["assetIndex"] = parent["assetIndex"];
    }

    // Merge assets (old format)
    if (parent.contains("assets") && !result.contains("assets")) {
        result["assets"] = parent["assets"];
    }

    return result;
}
