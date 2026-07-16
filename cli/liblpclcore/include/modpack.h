#ifndef LPCLCORE_MODPACK_H
#define LPCLCORE_MODPACK_H

#include <QString>
#include <QStringList>
#include <functional>
#include "core/lpclcore_export.h"

/// 整合包类型（对应原版 ModModpack.vb PackType）
enum class PackType {
    Unknown      = -1,
    CurseForge   = 0,   // manifest.json（无 addons 键）
    HMCL         = 1,   // modpack.json
    MultiMC      = 2,   // mmc-pack.json
    MCBBS        = 3,   // mcbbs.packmeta 或 manifest.json（有 addons 键）
    Modrinth     = 4,   // modrinth.index.json
    LauncherPack = 9,   // 内含 modpack.zip / modpack.mrpack（递归）
    Compressed   = 99,  // 回退：压缩版 .minecraft
};

/// 进度回调：(statusText, progress 0~100)
using PackProgressCallback = std::function<void(const QString &status, int progress)>;
/// 完成回调：(success, instanceName or error)
using PackCompleteCallback = std::function<void(bool ok, const QString &msg)>;

/// 检测整合包类型（打开 zip 扫描标记文件）
LPCLCORE_EXPORT PackType detectPackType(const QString &filePath);

/// 返回 pack type 的中文名
LPCLCORE_EXPORT QString packTypeName(PackType type);

/// 安装整合包
/// @param filePath      zip 文件路径
/// @param instanceName  实例名（为空则自动从清单读取或使用文件名）
/// @param onProgress    进度回调
/// @param onComplete    完成回调
LPCLCORE_EXPORT void installModpack(const QString &filePath,
                                     const QString &instanceName,
                                     PackProgressCallback onProgress,
                                     PackCompleteCallback onComplete);

#endif // LPCLCORE_MODPACK_H
