#include "auth/authlibauth.h"
#include "auth/msauth.h"
#include "auth/offlineauth.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkReply>
#include <QUuid>
#include <QLoggingCategory>

static Q_LOGGING_CATEGORY(logAl, "lpcl.auth.authlib")

AuthlibAuth::AuthlibAuth(ServerType type)
    : m_type(type) {
    m_nam = new QNetworkAccessManager(this);
}

LoginType AuthlibAuth::loginType() const
{
    return m_type == ServerType::AuthlibInjector ? LoginType::Auth : LoginType::Nide;
}

void AuthlibAuth::login(Callback onComplete) {
    m_cancelled = false;
    m_callback = onComplete;

    if (m_username.trimmed().isEmpty() || m_password.isEmpty()) {
        qCWarning(logAl) << "Username or password empty";
        finishLogin(false, LoginResult(), onComplete);
        return;
    }

    if (m_type == ServerType::Nide8) {
        doNideLogin(onComplete);
    } else {
        doAuthlibLogin(onComplete);
    }
}

void AuthlibAuth::cancel() {
    m_cancelled = true;
    // 取消也必须触发完成回调——回调驱动的调用方还在等（与 MsAuth::cancel 行为一致）
    finishLogin(false, LoginResult(), m_callback);
    m_callback = nullptr;
}

void AuthlibAuth::finishLogin(bool success, const LoginResult &result, const Callback &cb) {
    emit loginFinished(success, result);
    if (cb) cb(success, result);
}

void AuthlibAuth::doAuthlibLogin(Callback onComplete) {
    // Authlib-Injector uses Yggdrasil API
    // POST {server}/authserver/authenticate
    // Body: { agent: { name: "Minecraft", version: 1 }, username, password, clientToken, requestUser: true }

    QString clientToken = QUuid::createUuid().toString(QUuid::WithoutBraces);

    QUrl url(m_serverUrl);
    QString authUrl = m_serverUrl;
    if (!authUrl.endsWith('/')) authUrl += '/';
    authUrl += "authserver/authenticate";

    QNetworkRequest request{QUrl(authUrl)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject body;
    QJsonObject agent;
    agent["name"] = "Minecraft";
    agent["version"] = 1;
    body["agent"] = agent;
    body["username"] = m_username;
    body["password"] = m_password;
    body["clientToken"] = clientToken;
    body["requestUser"] = true;

    emit loginProgress("Authenticating with third-party server...");

    QNetworkReply *reply = m_nam->post(request, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply, clientToken, onComplete]() {
        reply->deleteLater();
        if (m_cancelled) return;

        if (reply->error() != QNetworkReply::NoError) {
            QString errMsg = QString::fromUtf8(reply->readAll());
            qCWarning(logAl) << "Authlib auth failed:" << reply->errorString() << errMsg;
            finishLogin(false, LoginResult(), onComplete);
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonObject root = doc.object();

        LoginResult result;
        result.name = root.value("selectedProfile").toObject().value("name").toString();
        result.uuid = root.value("selectedProfile").toObject().value("id").toString();
        result.accessToken = root.value("accessToken").toString();
        result.type = "Auth";
        result.clientToken = clientToken;

        if (result.name.isEmpty() || result.accessToken.isEmpty()) {
            QString err = root.value("errorMessage").toString("Unknown error");
            qCWarning(logAl) << "Authlib login failed:" << err;
            finishLogin(false, LoginResult(), onComplete);
            return;
        }

        qCInfo(logAl) << "Authlib login success:" << result.name;
        finishLogin(true, result, onComplete);
    });
}

void AuthlibAuth::doNideLogin(Callback onComplete) {
    // Nide8 uses a simpler API
    // POST {server}/api/yggdrasil/authserver/authenticate
    // Similar to Authlib but at a different endpoint

    QString clientToken = QUuid::createUuid().toString(QUuid::WithoutBraces);

    QString authUrl = m_serverUrl;
    if (!authUrl.endsWith('/')) authUrl += '/';
    authUrl += "api/yggdrasil/authserver/authenticate";

    QNetworkRequest request{QUrl(authUrl)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject body;
    QJsonObject agent;
    agent["name"] = "Minecraft";
    agent["version"] = 1;
    body["agent"] = agent;
    body["username"] = m_username;
    body["password"] = m_password;
    body["clientToken"] = clientToken;
    body["requestUser"] = true;

    emit loginProgress("Authenticating with Nide8 server...");

    QNetworkReply *reply = m_nam->post(request, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply, clientToken, onComplete]() {
        reply->deleteLater();
        if (m_cancelled) return;

        if (reply->error() != QNetworkReply::NoError) {
            QString errMsg = QString::fromUtf8(reply->readAll());
            qCWarning(logAl) << "Nide8 auth failed:" << reply->errorString() << errMsg;
            finishLogin(false, LoginResult(), onComplete);
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QJsonObject root = doc.object();

        LoginResult result;
        result.name = root.value("selectedProfile").toObject().value("name").toString();
        result.uuid = root.value("selectedProfile").toObject().value("id").toString();
        result.accessToken = root.value("accessToken").toString();
        result.type = "Nide";
        result.clientToken = clientToken;

        if (result.name.isEmpty() || result.accessToken.isEmpty()) {
            QString err = root.value("errorMessage").toString("Unknown error");
            qCWarning(logAl) << "Nide8 login failed:" << err;
            finishLogin(false, LoginResult(), onComplete);
            return;
        }

        qCInfo(logAl) << "Nide8 login success:" << result.name;
        finishLogin(true, result, onComplete);
    });
}

// AuthBase factory

AuthBase* AuthBase::create(LoginType type) {
    switch (type) {
    case LoginType::Legacy:
        return new OfflineAuth();
    case LoginType::Ms:
        return new MsAuth();
    case LoginType::Auth:
        return new AuthlibAuth(AuthlibAuth::ServerType::AuthlibInjector);
    case LoginType::Nide:
        return new AuthlibAuth(AuthlibAuth::ServerType::Nide8);
    default:
        return nullptr;
    }
}
