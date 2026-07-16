#include "lpclcore/offlineauth.h"

#include <QUuid>
#include <QLoggingCategory>

static Q_LOGGING_CATEGORY(logAuth, "lpcl.auth.offline")

OfflineAuth::OfflineAuth(const QString &username)
    : m_username(username) {
}

void OfflineAuth::login(Callback onComplete) {
    if (m_username.trimmed().isEmpty()) {
        emit loginFinished(false, LoginResult());
        if (onComplete) onComplete(false, LoginResult());
        return;
    }

    LoginResult result = createOfflineLogin(m_username);

    qCInfo(logAuth) << "Offline login for:" << result.name << "uuid:" << result.uuid;

    emit loginFinished(true, result);
    if (onComplete) onComplete(true, result);
}

QString OfflineAuth::generateOfflineUuid(const QString &username) {
    // Mojang offline UUID = MD5("OfflinePlayer:" + username)
    // Formatted as UUID with version 3 variant
    QString input = "OfflinePlayer:" + username;
    QByteArray hash = QCryptographicHash::hash(input.toUtf8(), QCryptographicHash::Md5);

    // Set UUID version to 3 and variant to 8/9/a/b
    hash[6] = (hash[6] & 0x0f) | 0x30; // version 3
    hash[8] = (hash[8] & 0x3f) | 0x80; // variant

    // Format as UUID: 8-4-4-4-12
    QString uuid;
    for (int i = 0; i < 16; ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) uuid += '-';
        uuid += QString("%1").arg((unsigned char)hash[i], 2, 16, QChar('0'));
    }

    return uuid;
}

QString OfflineAuth::generateClientToken() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

LoginResult OfflineAuth::createOfflineLogin(const QString &username) {
    LoginResult result;
    result.name = username.trimmed();
    result.uuid = generateOfflineUuid(result.name);
    result.accessToken = "0"; // Offline mode doesn't need a real token
    result.type = "Legacy";
    result.clientToken = generateClientToken();
    return result;
}
