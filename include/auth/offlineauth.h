#ifndef LPCL_OFFLINEAUTH_H
#define LPCL_OFFLINEAUTH_H

#include "auth/authbase.h"
#include <QCryptographicHash>
#include <QtQml/qqmlregistration.h>
#include <QQmlEngine>

/**
 * Offline / legacy login.
 * Generates a UUID (v3 offline) from the username.
 */
class OfflineAuth : public AuthBase
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    OfflineAuth(const QString &username = QString());
    static OfflineAuth *create(QQmlEngine *, QJSEngine *) { return new OfflineAuth(); }

    void login(Callback onComplete) override;
    void cancel() override {}
    LoginType loginType() const override { return LoginType::Legacy; }

    void setUsername(const QString &username) { m_username = username; }
    QString username() const { return m_username; }

    /// Generate offline UUID from username (matching the Mojang algorithm)
    Q_INVOKABLE static QString generateOfflineUuid(const QString &username);

    /// Generate a random client token
    static QString generateClientToken();

    /// Build a complete offline LoginResult for a username (QML-callable factory).
    /// Used by the launch page to supply credentials to Launcher.launchVersion().
    Q_INVOKABLE static LoginResult createOfflineLogin(const QString &username);

private:
    QString m_username;
};

#endif // LPCL_OFFLINEAUTH_H
