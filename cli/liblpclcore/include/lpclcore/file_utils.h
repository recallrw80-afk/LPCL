#include "lpclcore/lpclcore_export.h"
#ifndef LPCL_FILE_UTILS_H
#define LPCL_FILE_UTILS_H

#include <QString>
#include <QStringList>

namespace FileUtils {

/// Build asset path from SHA1 hash: first 2 chars / full hash
QString assetPathFromHash(const QString &hash);

/// Verify file SHA1 matches expected hash
bool verifySha1(const QString &filePath, const QString &expectedHash);

/// Extract native libraries from a JAR/ZIP file into destDir.
/// Only extracts .so, .dll, .dylib, .jnilib files (skipping META-INF/).
/// Returns list of extracted file paths, or empty on failure.
QStringList extractNativesJar(const QString &jarPath, const QString &destDir);

} // namespace FileUtils

#endif // LPCL_FILE_UTILS_H
