#include "util/file_utils.h"

#include <QFile>
#include <QCryptographicHash>

namespace FileUtils {

QString assetPathFromHash(const QString &hash)
{
    return hash.left(2) + "/" + hash;
}

bool verifySha1(const QString &filePath, const QString &expectedHash)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return false;

    QCryptographicHash hasher(QCryptographicHash::Sha1);
    hasher.addData(&file);
    file.close();

    QString actual = hasher.result().toHex().toLower();
    return actual == expectedHash.toLower();
}

} // namespace FileUtils
