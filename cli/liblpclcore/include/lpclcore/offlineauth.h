#ifndef LPCL_OFFLINEAUTH_H
#define LPCL_OFFLINEAUTH_H

#include "lpclcore/authbase.h"
#include <QCryptographicHash>

/**
 * Offline / legacy login.
 * Generates a UUID (v3 offline) from the username.
 */
class LPCLCORE_EXPORT OfflineAuth : public AuthBase
{
    Q_OBJECT

public:
    OfflineAuth(const QString &username = QString());

    void login(Callback onComplete) override;
    void cancel() override {}
    LoginType loginType() const override { return LoginType::Legacy; }

    void setUsername(const QString &username) { m_username = username; }
    QString username() const { return m_username; }

    /// Generate offline UUID from username (matching the Mojang algorithm)
    Q_INVOKABLE static QString generateOfflineUuid(const QString &username);

    /// Generate a random client token
    static QString generateClientToken();

    /// Build a complete offline LoginResult for a username.
    /// Used by the launch page to supply credentials to Launcher.launchVersion().
    Q_INVOKABLE static LoginResult createOfflineLogin(const QString &username);

private:
    QString m_username;
};

#endif // LPCL_OFFLINEAUTH_H
