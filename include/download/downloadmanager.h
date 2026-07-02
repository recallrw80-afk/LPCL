#ifndef PCL_DOWNLOADMANAGER_H
#define PCL_DOWNLOADMANAGER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QMap>
#include <functional>
#include <nlohmann/json.hpp>
#include "core/types.h"

/**
 * HTTP download manager with retry logic, progress tracking,
 * and concurrent download support.
 * Mirrors the original ModNet + ModDownload functionality.
 */
class DownloadManager : public QObject
{
    Q_OBJECT

public:
    static DownloadManager& instance();

    /// Progress callback: (bytesReceived, bytesTotal)
    using ProgressCallback = std::function<void(qint64, qint64)>;
    /// Completion callback: (success, localFilePath or error)
    using CompletionCallback = std::function<void(bool, QString)>;

    // ---- Simple download ----

    /// Download a file to a local path
    QNetworkReply* download(const QString &url, const QString &savePath,
                            int maxRetries = 3);

    /// Download with callbacks
    QNetworkReply* download(const QString &url, const QString &savePath,
                            ProgressCallback onProgress,
                            CompletionCallback onComplete,
                            int maxRetries = 3);

    /// Download to memory (small files like version JSON)
    QNetworkReply* downloadToString(const QString &url,
                                     std::function<void(bool, QString)> onComplete,
                                     int maxRetries = 3);

    // ---- JSON download helpers ----

    /// Download and parse JSON
    QNetworkReply* downloadJson(const QString &url,
                                 std::function<void(bool, QString, nlohmann::json)> onComplete,
                                 int maxRetries = 3);

    /// Get the version manifest URL
    static QString versionManifestUrl();

    /// Get a version JSON URL
    static QString versionJsonUrl(const QString &versionId);

    /// Get assets index URL
    static QString assetsIndexUrl(const QString &assetsVersion);

signals:
    void downloadStarted(const QString &url);
    void downloadProgress(const QString &url, qint64 received, qint64 total);
    void downloadFinished(const QString &url, bool success, const QString &msg);

private:
    DownloadManager();
    QNetworkReply* downloadInternal(const QString &url, const QString &savePath,
                                     ProgressCallback onProgress,
                                     CompletionCallback onComplete,
                                     int retriesRemaining);

    QNetworkAccessManager *m_nam = nullptr;
};

#endif // PCL_DOWNLOADMANAGER_H
