#ifndef LPCL_FILE_UTILS_H
#define LPCL_FILE_UTILS_H

#include <QString>

namespace FileUtils {

/// Build asset path from SHA1 hash: first 2 chars / full hash
QString assetPathFromHash(const QString &hash);

/// Verify file SHA1 matches expected hash
bool verifySha1(const QString &filePath, const QString &expectedHash);

} // namespace FileUtils

#endif // LPCL_FILE_UTILS_H
