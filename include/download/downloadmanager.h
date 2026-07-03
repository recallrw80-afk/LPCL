#ifndef LPCL_DOWNLOADMANAGER_H
#define LPCL_DOWNLOADMANAGER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QMap>
#include <QQueue>
#include <functional>
#include <nlohmann/json.hpp>
#include "core/types.h"

/**
 * HTTP download manager with retry logic, progress tracking,
 * concurrency control, and mirror source support.
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

    // ---- Concurrency control ----

    /// Set max concurrent downloads (default: 8)
    void setMaxConcurrent(int max);
    int maxConcurrent() const { return m_maxConcurrent; }
    int activeDownloads() const { return m_activeCount; }
    int queuedDownloads() const { return m_queue.size(); }

    // ---- Mirror source ----

    enum MirrorSource { None, BMCLAPI, MCBBS };
    void setMirrorSource(MirrorSource source);
    MirrorSource mirrorSource() const { return m_mirror; }

    /// Rewrite a Mojang URL to use the selected mirror
    QString applyMirror(const QString &url) const;

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

    /// Download JSON to memory with custom headers
    QNetworkReply* downloadJsonWithHeaders(const QString &url,
                                            const QMap<QByteArray, QByteArray> &headers,
                                            std::function<void(bool, QString, nlohmann::json)> onComplete,
                                            int maxRetries = 3);

    /// Download to memory with custom headers
    QNetworkReply* downloadToStringWithHeaders(const QString &url,
                                                const QMap<QByteArray, QByteArray> &headers,
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
    void activeCountChanged(int count);

private:
    DownloadManager();
    QNetworkReply* downloadInternal(const QString &url, const QString &savePath,
                                     ProgressCallback onProgress,
                                     CompletionCallback onComplete,
                                     int retriesRemaining);

    // Queue management
    struct QueueEntry {
        QString url;
        QString savePath;
        ProgressCallback onProgress;
        CompletionCallback onComplete;
        int retries;
    };
    void enqueue(const QueueEntry &entry);
    void processQueue();
    void onDownloadFinished();

    QNetworkAccessManager *m_nam = nullptr;
    int m_maxConcurrent = 8;
    int m_activeCount = 0;
    QQueue<QueueEntry> m_queue;
    MirrorSource m_mirror = None;
};

#endif // LPCL_DOWNLOADMANAGER_H
