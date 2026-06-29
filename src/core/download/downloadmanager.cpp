#include "downloadmanager.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTimer>
#include <QCryptographicHash>
#include <QLoggingCategory>
#include <nlohmann/json.hpp>

static Q_LOGGING_CATEGORY(logDl, "pcl.download")

DownloadManager& DownloadManager::instance()
{
    static DownloadManager m;
    return m;
}

DownloadManager::DownloadManager()
{
    m_nam = new QNetworkAccessManager(this);
}

// ============================================================================
// Simple download
// ============================================================================

QNetworkReply* DownloadManager::download(const QString &url, const QString &savePath,
                                          int maxRetries)
{
    return download(url, savePath, nullptr, nullptr, maxRetries);
}

QNetworkReply* DownloadManager::download(const QString &url, const QString &savePath,
                                          ProgressCallback onProgress,
                                          CompletionCallback onComplete,
                                          int maxRetries)
{
    return downloadInternal(url, savePath, onProgress, onComplete, maxRetries);
}

QNetworkReply* DownloadManager::downloadInternal(const QString &url,
                                                  const QString &savePath,
                                                  ProgressCallback onProgress,
                                                  CompletionCallback onComplete,
                                                  int retriesRemaining)
{
    emit downloadStarted(url);

    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "LPCL/0.1");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = m_nam->get(request);

    // Progress
    connect(reply, &QNetworkReply::downloadProgress, this,
            [this, url, onProgress](qint64 received, qint64 total) {
                emit downloadProgress(url, received, total);
                if (onProgress) onProgress(received, total);
            });

    // Completion
    connect(reply, &QNetworkReply::finished, this, [=]() {
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
            emit downloadFinished(url, false, "Retrying...");
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

// ============================================================================
// Memory download
// ============================================================================

QNetworkReply* DownloadManager::downloadToString(const QString &url,
                                                   std::function<void(bool, QString)> onComplete,
                                                   int maxRetries)
{
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "LPCL/0.1");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = m_nam->get(request);

    connect(reply, &QNetworkReply::finished, this, [=]() {
        reply->deleteLater();
        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        bool success = (reply->error() == QNetworkReply::NoError && statusCode < 400);

        if (!success && maxRetries > 0) {
            QTimer::singleShot(500, this, [=]() {
                downloadToString(url, onComplete, maxRetries - 1);
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

QNetworkReply* DownloadManager::downloadJson(const QString &url,
                                               std::function<void(bool, QString, nlohmann::json)> onComplete,
                                               int maxRetries)
{
    return downloadToString(url, [onComplete](bool success, QString data) {
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

// ============================================================================
// URL helpers
// ============================================================================

QString DownloadManager::versionManifestUrl()
{
    return "https://launchermeta.mojang.com/mc/game/version_manifest.json";
}

QString DownloadManager::versionJsonUrl(const QString &versionId)
{
    // The version manifest gives us the per-version URL
    // This is a fallback — normally fetch manifest first, then use the returned URL
    // For known version IDs we construct the URL directly
    return QString("https://launchermeta.mojang.com/v1/packages/"
                   "%1/%2.json")
        .arg(QString(QCryptographicHash::hash(versionId.toUtf8(), QCryptographicHash::Sha1).toHex().left(2)),
             versionId);
}

QString DownloadManager::assetsIndexUrl(const QString &assetsVersion)
{
    return QString("https://launchermeta.mojang.com/v1/packages/%1.json")
        .arg(assetsVersion);
}
