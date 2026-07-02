#ifndef PCL_ASSETDOWNLOADER_H
#define PCL_ASSETDOWNLOADER_H

#include <QObject>
#include "core/types.h"

/**
 * Minecraft asset and library downloader.
 * Handles version JSON, JAR, assets, and native library download/extraction.
 */
class AssetDownloader : public QObject
{
    Q_OBJECT

public:
    static AssetDownloader& instance();

    /// Download a full Minecraft version (JSON + JAR + assets + libraries)
    void downloadVersion(const QString &versionId,
                         std::function<void(bool, QString)> onComplete);

    /// Download version JSON from Mojang
    void downloadVersionJson(const QString &versionId,
                              std::function<void(bool, QString)> onComplete);

    /// Download the version JAR
    void downloadClientJar(const McVersion &version,
                            std::function<void(bool, QString)> onComplete);

    /// Download asset index and all assets
    void downloadAssets(const McVersion &version,
                         std::function<void(bool, QString)> onComplete);

    /// Download and extract native libraries
    void downloadNatives(const McVersion &version,
                          std::function<void(bool, QString)> onComplete);

signals:
    void downloadProgress(const QString &task, qint64 received, qint64 total);

private:
    AssetDownloader() = default;
};

#endif // PCL_ASSETDOWNLOADER_H
