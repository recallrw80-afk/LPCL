#include "core/lpclcore_export.h"
#ifndef LPCL_FILE_UTILS_H
#define LPCL_FILE_UTILS_H

#include <QString>
#include <QStringList>

namespace FileUtils {

/// Build asset path from SHA1 hash: first 2 chars / full hash
LPCLCORE_EXPORT QString assetPathFromHash(const QString &hash);

/// Verify file SHA1 matches expected hash
LPCLCORE_EXPORT bool verifySha1(const QString &filePath, const QString &expectedHash);

/// Extract native libraries from a JAR/ZIP file into destDir.
/// Only extracts .so, .dll, .dylib, .jnilib files (skipping META-INF/).
/// Returns list of extracted file paths, or empty on failure.
LPCLCORE_EXPORT QStringList extractNativesJar(const QString &jarPath, const QString &destDir);

/// maven 坐标 → 仓库相对路径（旧格式 libraries 用，≤1.13 的 JSON 无 downloads.artifact）
/// "group:artifact:version[:classifier][@ext]" → "group/artifact/version/artifact-version[-classifier].ext"
/// 解析失败返回空串
LPCLCORE_EXPORT QString mavenNameToPath(const QString &name);

} // namespace FileUtils

#endif // LPCL_FILE_UTILS_H
