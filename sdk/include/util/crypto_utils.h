#include "core/lpclcore_export.h"
#ifndef LPCL_CRYPTO_UTILS_H
#define LPCL_CRYPTO_UTILS_H

#include <QString>
#include <QByteArray>

namespace CryptoUtils {

/// DES encrypt plaintext with key. Returns base64-encoded ciphertext.
/// Mirrors CryptographyUtils.DesEncrypt from Windows PCL.
LPCLCORE_EXPORT QString desEncrypt(const QString &plainText, const QString &key);

/// DES decrypt base64-encoded ciphertext with key. Returns plaintext.
/// Mirrors CryptographyUtils.DesDecrypt from Windows PCL.
LPCLCORE_EXPORT QString desDecrypt(const QString &cipherB64, const QString &key);

/// Generate the LPCL encryption key: "LPCL" + identify string
inline QString pclEncryptKey() { return QString("LPCL") + "Liunx"; }

/// Encrypt with PCL standard key
inline QString pclEncrypt(const QString &plainText) {
    return desEncrypt(plainText, pclEncryptKey());
}

/// Decrypt with PCL standard key
inline QString pclDecrypt(const QString &cipherB64) {
    return desDecrypt(cipherB64, pclEncryptKey());
}

} // namespace CryptoUtils

#endif // LPCL_CRYPTO_UTILS_H
