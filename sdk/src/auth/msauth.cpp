#include "auth/msauth.h"
#include "ms_client_id.h"  // CMake 生成：编译期嵌入的 MS OAuth Client ID（未配置为空串）

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkReply>
#include <QLoggingCategory>
#include <QUrl>

static Q_LOGGING_CATEGORY(logMs, "lpcl.auth.ms")

// Microsoft OAuth constants
// 官方启动器的 client ID 不支持设备码流程（AADSTS700016），必须用自注册的 Azure 应用
// （公开客户端，无需 secret；注册步骤见 CONTRIBUTING「Azure 应用注册」）
static const QString CLIENT_ID = QStringLiteral(LPCL_MS_CLIENT_ID);
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
    m_refreshToken.clear();
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
    if (CLIENT_ID.isEmpty()) {
        qCWarning(logMs) << "MS client ID not embedded (build without LPCL_MS_CLIENT_ID)";
        finishLogin(false, LoginResult());
        return;
    }
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

        // 用成员定时器调度下一轮（cancel() 可停止；裸 QTimer::singleShot 无法取消）
        auto scheduleNext = [this, deviceCode](int nextInterval) {
            m_pollTimer->stop();
            m_pollTimer->disconnect();
            connect(m_pollTimer, &QTimer::timeout, this, [this, deviceCode, nextInterval]() {
                pollForToken(deviceCode, nextInterval);
            });
            m_pollTimer->start(nextInterval * 1000);
        };

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
            scheduleNext(intervalSeconds);
            return;
        }

        if (error == "slow_down") {
            // RFC 8628：限流时轮询间隔 +5s 继续，不是致命错误
            scheduleNext(intervalSeconds + 5);
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

        // offline_access scope 会带回 refresh token——持久化它才能跨会话免登录
        m_refreshToken = root.value("refresh_token").toString();
        qCInfo(logMs) << "Got Microsoft access token";

        emit loginProgress("Authenticating with Xbox Live...");
        authenticateWithXbl(accessToken);
    });
}

void MsAuth::loginWithRefreshToken(const QString &refreshToken, Callback onComplete) {
    m_callback = onComplete;
    m_cancelled = false;
    if (refreshToken.isEmpty()) {
        finishLogin(false, LoginResult());
        return;
    }
    m_refreshToken = refreshToken;  // 未下发新 token 时沿用它
    emit loginProgress("Refreshing Microsoft token...");

    QNetworkRequest request(TOKEN_URL);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    QByteArray data = QString("grant_type=refresh_token&client_id=%1&refresh_token=%2"
                              "&scope=XboxLive.signin%20offline_access")
                          .arg(CLIENT_ID, QString(QUrl::toPercentEncoding(refreshToken))).toUtf8();

    QNetworkReply *reply = m_nam->post(request, data);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (m_cancelled) return;

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonObject root = doc.object();

        QString error = root.value("error").toString();
        if (!error.isEmpty()) {  // invalid_grant = token 过期/被撤销，需重新设备码登录
            qCWarning(logMs) << "Refresh token error:" << error;
            finishLogin(false, LoginResult());
            return;
        }

        QString accessToken = root.value("access_token").toString();
        if (accessToken.isEmpty()) {
            qCWarning(logMs) << "No access token in refresh response";
            finishLogin(false, LoginResult());
            return;
        }

        // MS 轮换 refresh token：优先保存新token，未下发则沿用旧的
        QString newRefresh = root.value("refresh_token").toString();
        m_refreshToken = newRefresh.isEmpty() ? m_refreshToken : newRefresh;

        qCInfo(logMs) << "Refreshed Microsoft access token";
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
        result.refreshToken = m_refreshToken;  // 由调用方持久化（launch 时换新）

        if (result.name.isEmpty() || result.uuid.isEmpty()) {
            qCWarning(logMs) << "Empty profile - need to buy Minecraft?";
            finishLogin(false, LoginResult());
            return;
        }

        qCInfo(logMs) << "Logged in as:" << result.name << "(UUID:" << result.uuid << ")";
        finishLogin(true, result);
    });
}
