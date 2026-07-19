#include "download/downloadmanager.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTimer>
#include <QCryptographicHash>
#include <QLoggingCategory>
#include <nlohmann/json.hpp>

static Q_LOGGING_CATEGORY(logDl, "lpcl.download")

DownloadManager& DownloadManager::instance() {
    static DownloadManager m;
    return m;
}

DownloadManager::DownloadManager() {
    m_nam = new QNetworkAccessManager(this);
}

// ---- Concurrency control ----

void DownloadManager::setMaxConcurrent(int max)
{
    m_maxConcurrent = qMax(1, max);
    processQueue();
}

void DownloadManager::enqueue(const QueueEntry &entry)
{
    m_queue.enqueue(entry);
    processQueue();
}

void DownloadManager::processQueue()
{
    while (m_activeCount < m_maxConcurrent && !m_queue.isEmpty()) {
        QueueEntry entry = m_queue.dequeue();
        m_activeCount++;
        emit activeCountChanged(m_activeCount);

        // Apply mirror URL rewriting
        QString url = applyMirror(entry.url);

        downloadInternal(url, entry.savePath, entry.onProgress,
            [this, entry](bool success, QString msg) {
                onDownloadFinished();
                if (entry.onComplete) entry.onComplete(success, msg);
            }, entry.retries);
    }
}

void DownloadManager::onDownloadFinished()
{
    m_activeCount--;
    emit activeCountChanged(m_activeCount);
    // Process next item in queue
    QTimer::singleShot(0, this, &DownloadManager::processQueue);
}

// ---- Mirror source ----

void DownloadManager::setMirrorSource(MirrorSource source)
{
    m_mirror = source;
}

QString DownloadManager::applyMirror(const QString &url) const
{
    if (m_mirror == None) return url;

    // BMCLAPI mirror: https://bmclapi2.bangbang93.com
    // Rewrites Mojang URLs:
    //   launchermeta.mojang.com → bmclapi2.bangbang93.com
    //   launcher.mojang.com → bmclapi2.bangbang93.com
    //   resources.download.minecraft.net → bmclapi2.bangbang93.com
    //   libraries.minecraft.net → bmclapi2.bangbang93.com
    //   piston-data.mojang.com → bmclapi2.bangbang93.com
    //   piston-meta.mojang.com → bmclapi2.bangbang93.com

    QString mirrored = url;
    const QString bmclHost = "bmclapi2.bangbang93.com";

    if (m_mirror == BMCLAPI) {
        mirrored.replace("launchermeta.mojang.com", bmclHost);
        mirrored.replace("launcher.mojang.com", bmclHost);
        mirrored.replace("resources.download.minecraft.net", bmclHost);
        mirrored.replace("libraries.minecraft.net", bmclHost);
        mirrored.replace("piston-data.mojang.com", bmclHost);
        mirrored.replace("piston-meta.mojang.com", bmclHost);
    } else if (m_mirror == MCBBS) {
        // MCBBS mirror (download.mcbbs.net) — similar URL rewriting
        const QString mcbbsHost = "download.mcbbs.net";
        mirrored.replace("launchermeta.mojang.com", mcbbsHost);
        mirrored.replace("launcher.mojang.com", mcbbsHost);
        mirrored.replace("resources.download.minecraft.net", mcbbsHost);
        mirrored.replace("libraries.minecraft.net", mcbbsHost);
        mirrored.replace("piston-data.mojang.com", mcbbsHost);
        mirrored.replace("piston-meta.mojang.com", mcbbsHost);
    }

    if (mirrored != url) {
        qCDebug(logDl) << "Mirror rewrite:" << url << "->" << mirrored;
    }
    return mirrored;
}

// ---- Simple download ----

QNetworkReply* DownloadManager::download(const QString &url, const QString &savePath,
                                          int maxRetries) {
    return download(url, savePath, nullptr, nullptr, maxRetries);
}

QNetworkReply* DownloadManager::download(const QString &url, const QString &savePath,
                                          ProgressCallback onProgress,
                                          CompletionCallback onComplete,
                                          int maxRetries) {
    return downloadInternal(url, savePath, onProgress, onComplete, maxRetries);
}

QNetworkReply* DownloadManager::downloadInternal(const QString &url,
                                                  const QString &savePath,
                                                  ProgressCallback onProgress,
                                                  CompletionCallback onComplete,
                                                  int retriesRemaining) {
    emit downloadStarted(url);

    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "LPCL/0.1");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);

    QNetworkReply *reply = m_nam->get(request);

    // Progress
    connect(reply, &QNetworkReply::downloadProgress, this,
            [this, url, onProgress](qint64 received, qint64 total) {
                emit downloadProgress(url, received, total);
                if (onProgress) onProgress(received, total);
            });

    // Completion
    connect(reply, &QNetworkReply::finished, this, [=, this]() {
        reply->deleteLater();

        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        bool success = (reply->error() == QNetworkReply::NoError);

        // Handle HTTP errors
        if (statusCode >= 400) success = false;

        // Retry on failure
        if (!success && retriesRemaining > 0) {
            qCWarning(logDl) << "Download failed, retrying:" << url
                             << "Error:" << reply->errorString()
                             << "Retries left:" << retriesRemaining - 1;
            // Delay retry slightly
            QTimer::singleShot(500, this, [this, url, savePath, onProgress, onComplete, retriesRemaining]() {
                downloadInternal(url, savePath, onProgress, onComplete, retriesRemaining - 1);
            });
            // 重试期间不发 downloadFinished——任务尚未终结，误报失败会误导上层
            return;
        }

        if (!success) {
            QString err = reply->errorString();
            qCWarning(logDl) << "Download failed:" << url << err;
            emit downloadFinished(url, false, err);
            if (onComplete) onComplete(false, err);
            return;
        }

        // Save to file
        QByteArray data = reply->readAll();
        QDir().mkpath(QFileInfo(savePath).absolutePath());

        QFile file(savePath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(data);
            file.close();
            qCInfo(logDl) << "Downloaded:" << url << "->" << savePath
                          << "(" << data.size() << "bytes)";
            emit downloadFinished(url, true, savePath);
            if (onComplete) onComplete(true, savePath);
        } else {
            QString err = "Cannot write to: " + savePath;
            qCWarning(logDl) << err;
            emit downloadFinished(url, false, err);
            if (onComplete) onComplete(false, err);
        }
    });

    return reply;
}

// ---- Memory download ----

QNetworkReply* DownloadManager::downloadToString(const QString &url,
                                                   std::function<void(bool, QString)> onComplete,
                                                   int maxRetries) {
    return downloadToStringWithHeaders(url, {}, onComplete, maxRetries);
}

QNetworkReply* DownloadManager::downloadJson(const QString &url,
                                               std::function<void(bool, QString, nlohmann::json)> onComplete,
                                               int maxRetries) {
    return downloadJsonWithHeaders(url, {}, onComplete, maxRetries);
}

// ---- Memory download with custom headers ----

QNetworkReply* DownloadManager::downloadToStringWithHeaders(const QString &url,
                                                              const QMap<QByteArray, QByteArray> &headers,
                                                              std::function<void(bool, QString)> onComplete,
                                                              int maxRetries) {
    emit downloadStarted(url);
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "LPCL/0.1");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    for (auto it = headers.constBegin(); it != headers.constEnd(); ++it) {
        request.setRawHeader(it.key(), it.value());
    }

    QNetworkReply *reply = m_nam->get(request);

    // Capture headers by value for retry recursion.
    connect(reply, &QNetworkReply::finished, this, [=, this]() {
        reply->deleteLater();
        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        bool success = (reply->error() == QNetworkReply::NoError && statusCode < 400);

        if (!success && maxRetries > 0) {
            QTimer::singleShot(500, this, [=, this]() {
                downloadToStringWithHeaders(url, headers, onComplete, maxRetries - 1);
            });
            return;
        }

        if (!success) {
            if (onComplete) onComplete(false, reply->errorString());
            return;
        }

        QByteArray data = reply->readAll();
        if (onComplete) onComplete(true, QString::fromUtf8(data));
    });

    return reply;
}

QNetworkReply* DownloadManager::downloadJsonWithHeaders(const QString &url,
                                                          const QMap<QByteArray, QByteArray> &headers,
                                                          std::function<void(bool, QString, nlohmann::json)> onComplete,
                                                          int maxRetries) {
    return downloadToStringWithHeaders(url, headers,
        [onComplete](bool success, QString data) {
            if (!success) {
                if (onComplete) onComplete(false, data, nlohmann::json());
                return;
            }
            try {
                nlohmann::json j = nlohmann::json::parse(data.toStdString());
                if (onComplete) onComplete(true, data, j);
            } catch (const std::exception &e) {
                if (onComplete) onComplete(false, QString("JSON parse error: ") + e.what(), nlohmann::json());
            }
        }, maxRetries);
}

// ---- URL helpers ----

QString DownloadManager::versionManifestUrl() {
    return "https://launchermeta.mojang.com/mc/game/version_manifest.json";
}

QString DownloadManager::versionJsonUrl(const QString &versionId) {
    // The version manifest gives us the per-version URL
    // This is a fallback — normally fetch manifest first, then use the returned URL
    // For known version IDs we construct the URL directly
    return QString("https://launchermeta.mojang.com/v1/packages/"
                   "%1/%2.json")
        .arg(QString(QCryptographicHash::hash(versionId.toUtf8(), QCryptographicHash::Sha1).toHex().left(2)),
             versionId);
}

QString DownloadManager::assetsIndexUrl(const QString &assetsVersion) {
    return QString("https://launchermeta.mojang.com/v1/packages/%1.json")
        .arg(assetsVersion);
}
