#ifndef PCL_CRYPTO_UTILS_H
#define PCL_CRYPTO_UTILS_H

#include <QString>
#include <QByteArray>

namespace CryptoUtils {

/// DES encrypt plaintext with key. Returns base64-encoded ciphertext.
/// Mirrors CryptographyUtils.DesEncrypt from Windows PCL.
QString desEncrypt(const QString &plainText, const QString &key);

/// DES decrypt base64-encoded ciphertext with key. Returns plaintext.
/// Mirrors CryptographyUtils.DesDecrypt from Windows PCL.
QString desDecrypt(const QString &cipherB64, const QString &key);

/// Generate the PCL encryption key: "PCL" + identify string
/// For LPCL, identify = "Liunx" (mirrors Windows' Identify constant)
inline QString pclEncryptKey() { return QString("PCL") + "Liunx"; }

/// Encrypt with PCL standard key
inline QString pclEncrypt(const QString &plainText) {
    return desEncrypt(plainText, pclEncryptKey());
}

/// Decrypt with PCL standard key
inline QString pclDecrypt(const QString &cipherB64) {
    return desDecrypt(cipherB64, pclEncryptKey());
}

} // namespace CryptoUtils

#endif // PCL_CRYPTO_UTILS_H
