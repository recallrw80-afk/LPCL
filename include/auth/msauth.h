#ifndef LPCL_MSAUTH_H
#define LPCL_MSAUTH_H

#include "auth/authbase.h"
#include <QNetworkAccessManager>
#include <QTimer>

/**
 * Microsoft OAuth login (Device Code Flow).
 *
 * Flow:
 * 1. Request device code from Microsoft
 * 2. User opens URL and enters code in browser
 * 3. Poll for token
 * 4. Exchange Microsoft token for Xbox Live token
 * 5. Exchange XBL token for XSTS token
 * 6. Exchange XSTS token for Minecraft token
 * 7. Get Minecraft profile
 */
class MsAuth : public AuthBase
{
    Q_OBJECT

public:
    MsAuth();

    void login(Callback onComplete) override;
    void cancel() override;
    LoginType loginType() const override { return LoginType::Ms; }

    /// QML-friendly: start login, results via deviceCodeReady + loginFinished signals
    Q_INVOKABLE void startLogin() { login(nullptr); }

signals:
    /// Emitted when user needs to open a URL and enter a code
    void deviceCodeReady(const QString &userCode, const QString &verificationUrl);

private:
    void requestDeviceCode();
    void pollForToken(const QString &deviceCode, int intervalSeconds);
    void authenticateWithXbl(const QString &accessToken);
    void authenticateWithXsts(const QString &xblToken);
    void authenticateWithMinecraft(const QString &xstsToken, const QString &userHash);
    void getMinecraftProfile(const QString &mcAccessToken);

    QNetworkAccessManager *m_nam = nullptr;
    Callback m_callback;
    QTimer *m_pollTimer = nullptr;
    int m_pollRetries = 0;
    bool m_cancelled = false;
};

#endif // LPCL_MSAUTH_H
