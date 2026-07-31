#include "bridge/mod_platform_bridge.h"

#include <QTimer>

#include "lpcl.h"
#include "download/modplatform.h"

ModPlatformBridge& ModPlatformBridge::instance() {
    static ModPlatformBridge b;
    return b;
}

static QVariantMap modResourceToMap(const ModResource &r) {
    return {
        {"id", r.id}, {"name", r.name}, {"summary", r.summary},
        {"author", r.author}, {"iconUrl", r.iconUrl}, {"websiteUrl", r.websiteUrl},
        {"downloadCount", r.downloadCount}, {"versions", r.versions},
    };
}

void ModPlatformBridge::search(int platform, const QString &query, int page) {
    if (m_searching) return;
    m_searching = true;
    emit searchingChanged();

    ModPlatform::instance().searchMods(
        platform == 1 ? ModPlatform::Modrinth : ModPlatform::CurseForge,
        query, page, 25,
        [this](bool ok, QList<ModResource> mods) {
            QTimer::singleShot(0, this, [this, ok, mods]() {
                QVariantList out;
                for (const auto &r : mods) out.append(modResourceToMap(r));
                m_searching = false;
                emit searchingChanged();
                emit searchFinished(ok, out);
            });
        });
}

void ModPlatformBridge::getModFiles(int platform, const QString &modId) {
    ModPlatform::instance().getModFiles(
        platform == 1 ? ModPlatform::Modrinth : ModPlatform::CurseForge,
        modId,
        [this, modId](bool ok, QList<ModFileInfo> files) {
            QTimer::singleShot(0, this, [this, ok, modId, files]() {
                QVariantList out;
                for (const auto &f : files) {
                    out.append(QVariantMap{
                        {"id", f.id}, {"displayName", f.displayName},
                        {"fileName", f.fileName}, {"gameVersions", f.gameVersions},
                        {"loaders", f.loaders}, {"fileSize", f.fileSize},
                        {"sha1", f.sha1}, {"releaseDate", f.releaseDate},
                    });
                }
                emit modFilesFinished(ok, modId, out);
            });
        });
}

void ModPlatformBridge::downloadModToInstance(int platform, const QString &modId,
                                              const QString &fileId, const QString &fileName,
                                              const QString &instanceName) {
    if (m_downloading) return;
    auto info = lpcl::instanceInfo(instanceName);
    if (info.dirName.isEmpty()) {
        emit downloadFinished(false, QStringLiteral("实例不存在: ") + instanceName);
        return;
    }
    m_downloading = true;
    m_downloadPercent = 0;
    emit downloadingChanged();
    emit downloadProgressChanged();

    QString savePath = info.path + "mods/" + fileName;
    ModPlatform::instance().downloadMod(
        platform == 1 ? ModPlatform::Modrinth : ModPlatform::CurseForge,
        modId, fileId, savePath,
        [this](bool ok, QString msg) {
            QTimer::singleShot(0, this, [this, ok, msg]() {
                m_downloading = false;
                emit downloadingChanged();
                emit downloadFinished(ok, msg);
            });
        },
        [this](qint64 received, qint64 total) {
            if (total <= 0) return;
            QTimer::singleShot(0, this, [this, received, total]() {
                m_downloadPercent = int(received * 100 / total);
                emit downloadProgressChanged();
            });
        });
}
