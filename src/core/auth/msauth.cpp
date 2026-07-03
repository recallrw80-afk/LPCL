#include "auth/msauth.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkReply>
#include <QLoggingCategory>

static Q_LOGGING_CATEGORY(logMs, "lpcl.auth.ms")

// Microsoft OAuth constants
static const QString CLIENT_ID = "00000000402b5328"; // Official Minecraft launcher client ID
static const QString DEVICE_CODE_URL = "https://login.microsoftonline.com/consumers/oauth2/v2.0/devicecode";
static const QString TOKEN_URL = "https://login.microsoftonline.com/consumers/oauth2/v2.0/token";
static const QString XBL_AUTH_URL = "https://user.auth.xboxlive.com/user/authenticate";
static const QString XSTS_AUTH_URL = "https://xsts.auth.xboxlive.com/xsts/authorize";
static const QString MC_AUTH_URL = "https://api.minecraftservices.com/authentication/login_with_xbox";
static const QString MC_PROFILE_URL = "https://api.minecraftservices.com/minecraft/profile";

MsAuth::MsAuth() {
    m_nam = new QNetworkAccessManager(this);
    m_pollTimer = new QTimer(this);
    m_pollTimer->setSingleShot(true);
}

void MsAuth::login(Callback onComplete) {
    m_callback = onComplete;
    m_cancelled = false;
    m_pollRetries = 0;
    emit loginProgress("Requesting device code...");
    requestDeviceCode();
}

void MsAuth::cancel() {
    m_cancelled = true;
    if (m_pollTimer) m_pollTimer->stop();
    finishLogin(false, LoginResult());
}

void MsAuth::finishLogin(bool success, const LoginResult &result) {
    emit loginFinished(success, result);
    if (m_callback) m_callback(success, result);
}

void MsAuth::requestDeviceCode() {
    QNetworkRequest request(DEVICE_CODE_URL);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QByteArray data = QString("client_id=%1&scope=XboxLive.signin%20offline_access").arg(CLIENT_ID).toUtf8();

    QNetworkReply *reply = m_nam->post(request, data);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (m_cancelled) return;

        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(logMs) << "Device code request failed:" << reply->errorString();
            finishLogin(false, LoginResult());
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonObject root = doc.object();

        QString userCode = root.value("user_code").toString();
        QString deviceCode = root.value("device_code").toString();
        QString verificationUrl = root.value("verification_uri").toString();
        int interval = root.value("interval").toInt(5);

        if (userCode.isEmpty() || deviceCode.isEmpty()) {
            qCWarning(logMs) << "Invalid device code response";
            finishLogin(false, LoginResult());
            return;
        }

        qCInfo(logMs) << "Device code:" << userCode << "URL:" << verificationUrl;
        emit deviceCodeReady(userCode, verificationUrl);
        emit loginProgress(QString("Open %1 and enter code: %2").arg(verificationUrl, userCode));

        // Start polling
        pollForToken(deviceCode, interval);
    });
}

void MsAuth::pollForToken(const QString &deviceCode, int intervalSeconds) {
    if (m_cancelled) return;

    QNetworkRequest request(TOKEN_URL);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QByteArray data = QString("grant_type=urn:ietf:params:oauth:grant-type:device_code"
                              "&client_id=%1&device_code=%2")
                          .arg(CLIENT_ID, deviceCode).toUtf8();

    QNetworkReply *reply = m_nam->post(request, data);
    connect(reply, &QNetworkReply::finished, this, [this, reply, deviceCode, intervalSeconds]() {
        reply->deleteLater();
        if (m_cancelled) return;

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonObject root = doc.object();

        QString error = root.value("error").toString();
        if (error == "authorization_pending") {
            // User hasn't approved yet, keep polling
            m_pollRetries++;
            if (m_pollRetries > 120) { // 10 minutes max
                qCWarning(logMs) << "Polling timed out";
                finishLogin(false, LoginResult());
                return;
            }
            emit loginProgress("Waiting for approval...");
            QTimer::singleShot(intervalSeconds * 1000, this,
                               [this, deviceCode, intervalSeconds]() {
                pollForToken(deviceCode, intervalSeconds);
            });
            return;
        }

        if (!error.isEmpty()) {
            qCWarning(logMs) << "Token error:" << error;
            finishLogin(false, LoginResult());
            return;
        }

        QString accessToken = root.value("access_token").toString();
        if (accessToken.isEmpty()) {
            qCWarning(logMs) << "No access token in response";
            finishLogin(false, LoginResult());
            return;
        }

        qCInfo(logMs) << "Got Microsoft access token";

        emit loginProgress("Authenticating with Xbox Live...");
        authenticateWithXbl(accessToken);
    });
}

void MsAuth::authenticateWithXbl(const QString &accessToken) {
    QNetworkRequest request(XBL_AUTH_URL);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject body;
    QJsonObject props;
    props["AuthMethod"] = "RPS";
    props["SiteName"] = "user.auth.xboxlive.com";
    props["RpsTicket"] = QString("d=%1").arg(accessToken);
    body["Properties"] = props;
    body["RelyingParty"] = "http://auth.xboxlive.com";
    body["TokenType"] = "JWT";

    QNetworkReply *reply = m_nam->post(request, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (m_cancelled) return;

        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(logMs) << "XBL auth failed:" << reply->errorString();
            finishLogin(false, LoginResult());
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QString xblToken = doc.object().value("Token").toString();

        if (xblToken.isEmpty()) {
            qCWarning(logMs) << "No XBL token";
            finishLogin(false, LoginResult());
            return;
        }

        qCInfo(logMs) << "Got XBL token";
        emit loginProgress("Authenticating with XSTS...");
        authenticateWithXsts(xblToken);
    });
}

void MsAuth::authenticateWithXsts(const QString &xblToken) {
    QNetworkRequest request(XSTS_AUTH_URL);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject body;
    QJsonObject props;
    props["SandboxId"] = "RETAIL";
    QJsonArray userTokens;
    userTokens.append(xblToken);
    props["UserTokens"] = userTokens;
    body["Properties"] = props;
    body["RelyingParty"] = "rp://api.minecraftservices.com/";
    body["TokenType"] = "JWT";

    QNetworkReply *reply = m_nam->post(request, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (m_cancelled) return;

        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(logMs) << "XSTS auth failed:" << reply->errorString();
            finishLogin(false, LoginResult());
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonObject root = doc.object();

        QString xstsToken = root.value("Token").toString();
        QJsonObject displayClaims = root.value("DisplayClaims").toObject();
        QJsonArray xui = displayClaims.value("xui").toArray();
        QString userHash;
        if (!xui.isEmpty()) {
            userHash = xui[0].toObject().value("uhs").toString();
        }

        if (xstsToken.isEmpty() || userHash.isEmpty()) {
            qCWarning(logMs) << "No XSTS token or user hash";
            finishLogin(false, LoginResult());
            return;
        }

        qCInfo(logMs) << "Got XSTS token";
        emit loginProgress("Authenticating with Minecraft...");
        authenticateWithMinecraft(xstsToken, userHash);
    });
}

void MsAuth::authenticateWithMinecraft(const QString &xstsToken, const QString &userHash) {
    QNetworkRequest request(MC_AUTH_URL);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject body;
    body["identityToken"] = QString("XBL3.0 x=%1;%2").arg(userHash, xstsToken);

    QNetworkReply *reply = m_nam->post(request, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (m_cancelled) return;

        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(logMs) << "MC auth failed:" << reply->errorString();
            finishLogin(false, LoginResult());
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QString mcToken = doc.object().value("access_token").toString();

        if (mcToken.isEmpty()) {
            qCWarning(logMs) << "No Minecraft token";
            finishLogin(false, LoginResult());
            return;
        }

        qCInfo(logMs) << "Got Minecraft access token";
        emit loginProgress("Getting Minecraft profile...");
        getMinecraftProfile(mcToken);
    });
}

void MsAuth::getMinecraftProfile(const QString &mcAccessToken) {
    QNetworkRequest request(MC_PROFILE_URL);
    request.setRawHeader("Authorization", ("Bearer " + mcAccessToken).toUtf8());

    QNetworkReply *reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, mcAccessToken]() {
        reply->deleteLater();
        if (m_cancelled) return;

        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(logMs) << "Profile request failed:" << reply->errorString();
            finishLogin(false, LoginResult());
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonObject root = doc.object();

        LoginResult result;
        result.name = root.value("name").toString();
        result.uuid = root.value("id").toString();
        result.accessToken = mcAccessToken;
        result.type = "Ms";
        result.clientToken = ""; // Generated separately
        result.profileJson = QString::fromUtf8(doc.toJson());

        if (result.name.isEmpty() || result.uuid.isEmpty()) {
            qCWarning(logMs) << "Empty profile - need to buy Minecraft?";
            finishLogin(false, LoginResult());
            return;
        }

        qCInfo(logMs) << "Logged in as:" << result.name << "(UUID:" << result.uuid << ")";
        finishLogin(true, result);
    });
}
