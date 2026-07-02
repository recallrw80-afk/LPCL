#ifndef PCL_OFFLINEAUTH_H
#define PCL_OFFLINEAUTH_H

#include "auth/authbase.h"
#include <QCryptographicHash>

/**
 * Offline / legacy login.
 * Generates a UUID (v3 offline) from the username.
 */
class OfflineAuth : public AuthBase
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
    static QString generateOfflineUuid(const QString &username);

    /// Generate a random client token
    static QString generateClientToken();

private:
    QString m_username;
};

#endif // PCL_OFFLINEAUTH_H
