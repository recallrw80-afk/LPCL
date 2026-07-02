#ifndef PCL_AUTHLIBAUTH_H
#define PCL_AUTHLIBAUTH_H

#include "auth/authbase.h"
#include <QNetworkAccessManager>

/**
 * Authlib-Injector / Nide8 (Unified Pass) login.
 * Both use a third-party authentication server with a custom Yggdrasil API.
 */
class AuthlibAuth : public AuthBase
{
    Q_OBJECT

public:
    enum class ServerType {
        AuthlibInjector,  // authlib-injector based
        Nide8             // Nide8 / Unified Pass
    };

    AuthlibAuth(ServerType type = ServerType::AuthlibInjector);

    void login(Callback onComplete) override;
    void cancel() override;
    LoginType loginType() const override;

    void setServerUrl(const QString &url) { m_serverUrl = url; }
    void setUsername(const QString &username) { m_username = username; }
    void setPassword(const QString &password) { m_password = password; }

    QString serverUrl() const { return m_serverUrl; }

private:
    void doAuthlibLogin(Callback onComplete);
    void doNideLogin(Callback onComplete);

    ServerType m_type;
    QString m_serverUrl;
    QString m_username;
    QString m_password;

    QNetworkAccessManager *m_nam = nullptr;
    bool m_cancelled = false;
};

#endif // PCL_AUTHLIBAUTH_H
