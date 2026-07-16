#include "modpack.h"
#include "core/settings.h"
#include "core/versionmanager.h"
#include "core/javamanager.h"
#include "core/installer.h"
#include "download/downloadmanager.h"
#include "download/assetdownloader.h"
#include "download/modplatform.h"
#include "util/file_utils.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <QSettings>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// ---- pack type detection ----

PackType detectPackType(const QString &filePath) {
    // 通过 unzip -l 列出内容，避免加载整个 zip 到内存
    QProcess unzip;
    unzip.start("unzip", {"-l", filePath});
    if (!unzip.waitForFinished(10000)) return PackType::Unknown;
    QStringList lines = QString::fromUtf8(unzip.readAllStandardOutput()).split('\n');
    QStringList entries;
    for (const auto &line : lines) {
        // unzip -l 格式: "    length  date  time  name"
        auto parts = line.trimmed().split(QRegularExpression("\\s+"));
        if (parts.size() >= 4) {
            // 最后一个字段是文件名
            QString name = parts.mid(3).join(' ');
            if (!name.isEmpty() && name != "Name" && !name.startsWith("-")) {
                entries.append(name);
            }
        }
    }

    // 收集根目录和一级子目录的文件名
    QStringList roots, firstLevel;
    for (const auto &e : entries) {
        if (!e.contains('/')) {
            roots.append(e);
        } else {
            int slash = e.indexOf('/');
            if (e.lastIndexOf('/') == slash) {  // exactly one slash
                firstLevel.append(e);
            }
        }
    }

    auto hasRoot = [&](const QString &name) { return roots.contains(name); };
    // firstLevel entries are "DirName/filename", match on the filename part
    auto hasFirst = [&](const QString &name) {
        for (const auto &e : firstLevel) {
            int slash = e.indexOf('/');
            if (slash != -1 && e.mid(slash + 1) == name) return true;
        }
        return false;
    };

    // 按优先级检测
    // Type 3: MCBBS — mcbbs.packmeta (root or first-level)
    if (hasRoot("mcbbs.packmeta") || hasFirst("mcbbs.packmeta"))
        return PackType::MCBBS;

    // Type 2: MultiMC — mmc-pack.json
    if (hasRoot("mmc-pack.json") || hasFirst("mmc-pack.json"))
        return PackType::MultiMC;

    // Type 4: Modrinth
    if (hasRoot("modrinth.index.json") || hasFirst("modrinth.index.json"))
        return PackType::Modrinth;

    // Type 0/3: manifest.json
    if (hasRoot("manifest.json") || hasFirst("manifest.json")) {
        // 需要读取内容判断是否有 addons 键
        QString manifestPath;
        if (hasRoot("manifest.json")) manifestPath = "manifest.json";
        else {
            for (const auto &e : firstLevel) {
                if (e.endsWith("/manifest.json")) { manifestPath = e; break; }
            }
        }
        // 提取并解析 manifest.json
        QProcess extract;
        extract.start("unzip", {"-p", filePath, manifestPath});
        if (extract.waitForFinished(5000)) {
            QByteArray data = extract.readAllStandardOutput();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (!doc.isNull()) {
                QJsonObject obj = doc.object();
                if (obj.contains("addons"))
                    return PackType::MCBBS;  // has addons → MCBBS
                else
                    return PackType::CurseForge;  // no addons → CurseForge
            }
        }
        return PackType::CurseForge;  // fallback
    }

    // Type 1: HMCL — modpack.json
    if (hasRoot("modpack.json") || hasFirst("modpack.json"))
        return PackType::HMCL;

    // Type 9: Launcher pack — contains modpack.zip or modpack.mrpack
    if (hasRoot("modpack.zip") || hasRoot("modpack.mrpack") ||
        hasFirst("modpack.zip") || hasFirst("modpack.mrpack"))
        return PackType::LauncherPack;

    // Type 99: PCL launcher pack — PCL/Setup.ini + PCL/LatestLaunch.bat
    {
        bool hasPclSetup = false, hasPclBat = false;
        for (const auto &e : entries) {
            int slashes = e.count('/');
            if (slashes > 2) continue;
            if (e == "PCL/Setup.ini" || (slashes == 2 && e.endsWith("/PCL/Setup.ini")))
                hasPclSetup = true;
            if (e == "PCL/LatestLaunch.bat" || (slashes == 2 && e.endsWith("/PCL/LatestLaunch.bat")))
                hasPclBat = true;
        }
        if (hasPclSetup && hasPclBat) return PackType::Compressed;
    }

    // Fallback: check if it looks like a .minecraft folder
    for (const auto &e : entries) {
        // pattern: */versions/*/X.json
        if (e.contains("versions/") && e.endsWith(".json"))
            return PackType::Compressed;
    }

    return PackType::Unknown;
}

QString packTypeName(PackType type) {
    switch (type) {
    case PackType::CurseForge:   return "CurseForge";
    case PackType::HMCL:         return "HMCL";
    case PackType::MultiMC:      return "MultiMC (MMC)";
    case PackType::MCBBS:        return "MCBBS";
    case PackType::Modrinth:     return "Modrinth";
    case PackType::LauncherPack: return "带启动器的压缩包";
    case PackType::Compressed:   return "压缩版 .minecraft";
    default:                     return "未知类型";
    }
}

// ---- helpers ----

static json parseJsonSafe(const QByteArray &data, bool *ok = nullptr) {
    try {
        if (ok) *ok = true;
        return json::parse(data.toStdString());
    } catch (const json::parse_error &e) {
        if (ok) *ok = false;
        return json::object();  // empty object on failure
    }
}

static bool copyDir(const QString &src, const QString &dst) {
    QProcess cp;
    cp.start("cp", {"-r", src + "/.", dst});
    cp.waitForFinished(60000);
    return cp.exitCode() == 0;
}

// ---- extract zip ----

static bool extractZip(const QString &zipPath, const QString &destDir,
                       PackProgressCallback onProgress, int baseProgress = 10) {
    QDir().mkpath(destDir);
    QProcess unzip;
    unzip.start("unzip", {"-o", zipPath, "-d", destDir});
    if (onProgress) onProgress("正在解压...", baseProgress);
    if (!unzip.waitForFinished(300000)) return false;  // 5 min timeout for large packs
    int code = unzip.exitCode();
    // exit 0 = success, exit 1 = success with warnings (common with Windows-encoded filenames)
    return code == 0 || code == 1;
}

// ---- find .minecraft root inside archive ----

static QString findMcRoot(const QStringList &entries) {
    // 搜索 */versions/X/X.json 模式
    QRegularExpression re(R"((.+/)versions/[^/]+/[^/]+\.json)");
    for (const auto &e : entries) {
        auto match = re.match(e);
        if (match.hasMatch())
            return match.captured(1);  // the prefix before versions/
    }
    return "";
}

// 校验实例名：拒绝空名、路径分隔符、路径穿越
static bool validateInstanceName(const QString &name) {
    if (name.isEmpty()) return false;
    if (name.contains('/') || name.contains('\\')) return false;
    if (name.contains("..")) return false;
    if (name == ".") return false;
    return true;
}

// 获取导入临时工作目录
static QString importWorkDir() {
    return VersionManager::instance().mcFolder() + "tmp/import/";
}

// Mod 下载条目
struct ModDownloadEntry {
    QString url;       // 直接下载 URL（Modrinth）
    QString savePath;  // 保存到 workDir + savePath
    QString cfModId;   // CurseForge project ID
    QString cfFileId;  // CurseForge file ID
};

static void downloadAndFinalize(const QString &mcVersion,
                                 const QString &forgeVer, const QString &neoVer, const QString &fabricVer,
                                 const QString &workDir, const QString &finalDir, const QString &name,
                                 const QList<ModDownloadEntry> &mods,
                                 PackProgressCallback onProgress,
                                 PackCompleteCallback onComplete);

static bool checkNameConflict(const QString &targetDir, const QString &name,
                               bool explicitName, PackCompleteCallback onComplete) {
    if (!validateInstanceName(name)) {
        if (onComplete)
            onComplete(false, "实例名无效: \"" + name + "\"");
        return false;
    }
    if (QDir(targetDir).exists()) {
        if (explicitName) {
            // --r 指定的名字也冲突，不允许覆盖
            if (onComplete)
                onComplete(false, "实例 \"" + name + "\" 已存在，请使用其他名称");
            return false;
        }
        if (onComplete)
            onComplete(false, "实例 \"" + name + "\" 已存在，请使用 --r <名称> 重命名");
        return false;
    }
    return true;
}

// ---- Type 0: CurseForge ----

static void installCurseForge(const QString &filePath, const QString &packDir,
                               const QString &instanceName,
                               PackProgressCallback onProgress,
                               PackCompleteCallback onComplete) {
    // 读取 manifest.json
    QString manifestPath = packDir + "/manifest.json";
    QFile f(manifestPath);
    if (!f.open(QIODevice::ReadOnly)) {
        if (onComplete) onComplete(false, "无法读取 manifest.json");
        return;
    }
    bool ok; json manifest = parseJsonSafe(f.readAll(), &ok); if (!ok) { if (onComplete) onComplete(false, "manifest.json 解析失败"); return; }
    f.close();

    QString mcVersion = QString::fromStdString(manifest.value("minecraft", json::object()).value("version", ""));
    QString name = instanceName.isEmpty()
        ? QString::fromStdString(manifest.value("name", "Modpack"))
        : instanceName;

    if (onProgress) onProgress("检测到 CurseForge 整合包: " + name, 15);

    // 读取 modloaders
    QString forgeVer, neoVer, fabricVer;
    if (manifest.contains("minecraft") && manifest["minecraft"].contains("modLoaders")) {
        for (const auto &loader : manifest["minecraft"]["modLoaders"]) {
            QString id = QString::fromStdString(loader.value("id", ""));
            if (id.startsWith("forge-")) forgeVer = id.mid(6);
            else if (id.startsWith("neoforge-")) neoVer = id.mid(9);
            else if (id.startsWith("fabric-")) fabricVer = id.mid(7);
        }
    }

    QString finalDir = VersionManager::instance().mcFolder() + name + "/";
    if (!checkNameConflict(finalDir, name, !instanceName.isEmpty(), onComplete)) return;

    // 在 tmp 下工作，下载完成前不放入游戏目录
    QString workDir = importWorkDir() + name + "/";
    QDir(workDir).removeRecursively();
    QDir().mkpath(workDir);

    // 复制 overrides
    QString overridesDir = QString::fromStdString(manifest.value("overrides", "overrides"));
    QString srcOverride = packDir + "/" + overridesDir;
    if (QDir(srcOverride).exists()) {
        if (onProgress) onProgress("复制整合包文件...", 20);
        copyDir(srcOverride, workDir);
    }

    // 提取 mod 列表
    QList<ModDownloadEntry> mods;
    if (manifest.contains("files")) {
        for (const auto &f : manifest["files"]) {
            ModDownloadEntry m;
            m.cfModId = QString::fromStdString(f.value("projectID", json()).dump());
            // projectID is a number, clean up the dump
            m.cfModId = QString::number(f.value("projectID", 0));
            m.cfFileId = QString::number(f.value("fileID", 0));
            m.savePath = "mods/" + m.cfModId + "_" + m.cfFileId + ".jar";
            mods.append(m);
        }
    }

    // 下载 MC + modloader → 移入最终位置
    if (onProgress) onProgress("准备下载...", 30);
    downloadAndFinalize(mcVersion, forgeVer, neoVer, fabricVer,
                        workDir, finalDir, name, mods, onProgress, onComplete);
}

// ---- Type 1: HMCL ----

static void installHMCL(const QString &filePath, const QString &packDir,
                         const QString &instanceName,
                         PackProgressCallback onProgress,
                         PackCompleteCallback onComplete) {
    QFile f(packDir + "/modpack.json");
    if (!f.open(QIODevice::ReadOnly)) {
        if (onComplete) onComplete(false, "无法读取 modpack.json");
        return;
    }
    bool ok; json modpack = parseJsonSafe(f.readAll(), &ok); if (!ok) { if (onComplete) onComplete(false, "modpack.json 解析失败"); return; }
    f.close();

    QString name = instanceName.isEmpty()
        ? QString::fromStdString(modpack.value("name", "HMCL Modpack"))
        : instanceName;
    QString mcVersion = QString::fromStdString(modpack.value("gameVersion", ""));

    if (onProgress) onProgress("检测到 HMCL 整合包: " + name, 15);

    QString finalDir = VersionManager::instance().mcFolder() + name + "/";
    if (!checkNameConflict(finalDir, name, !instanceName.isEmpty(), onComplete)) return;

    QString workDir = importWorkDir() + name + "/";
    QDir(workDir).removeRecursively();
    QDir().mkpath(workDir);

    // 复制 minecraft/ 文件夹
    QString mcSrc = packDir + "/minecraft/";
    if (QDir(mcSrc).exists()) {
        if (onProgress) onProgress("复制整合包文件...", 20);
        copyDir(mcSrc, workDir);
    }

    if (onProgress) onProgress("准备下载...", 30);
    downloadAndFinalize(mcVersion, {}, {}, {}, workDir, finalDir, name, {}, onProgress, onComplete);
}

// ---- Type 2: MultiMC ----

static void installMultiMC(const QString &filePath, const QString &packDir,
                            const QString &instanceName,
                            PackProgressCallback onProgress,
                            PackCompleteCallback onComplete) {
    QFile f(packDir + "/mmc-pack.json");
    if (!f.open(QIODevice::ReadOnly)) {
        if (onComplete) onComplete(false, "无法读取 mmc-pack.json");
        return;
    }
    bool ok; json mmc = parseJsonSafe(f.readAll(), &ok); if (!ok) { if (onComplete) onComplete(false, "mmc-pack.json 解析失败"); return; }
    f.close();

    // 从 components 读取 MC 版本和 modloader
    QString mcVersion, forgeVer, neoVer, fabricVer;
    if (mmc.contains("components")) {
        for (const auto &comp : mmc["components"]) {
            QString uid = QString::fromStdString(comp.value("uid", ""));
            if (uid == "net.minecraft") mcVersion = QString::fromStdString(comp.value("version", ""));
            else if (uid == "net.minecraftforge") forgeVer = QString::fromStdString(comp.value("version", ""));
            else if (uid == "net.neoforged") neoVer = QString::fromStdString(comp.value("version", ""));
            else if (uid == "net.fabricmc.fabric-loader") fabricVer = QString::fromStdString(comp.value("version", ""));
        }
    }
    QString name = instanceName.isEmpty() ? QString::fromStdString(mmc.value("name", "MMC Modpack")) : instanceName;

    if (onProgress) onProgress("检测到 MultiMC 整合包: " + name, 15);

    QString finalDir = VersionManager::instance().mcFolder() + name + "/";
    if (!checkNameConflict(finalDir, name, !instanceName.isEmpty(), onComplete)) return;

    QString workDir = importWorkDir() + name + "/";
    QDir(workDir).removeRecursively();
    QDir().mkpath(workDir);

    // 复制 .minecraft/ 文件夹
    QString mcSrc = packDir + "/.minecraft/";
    if (QDir(mcSrc).exists()) {
        if (onProgress) onProgress("复制整合包文件...", 20);
        copyDir(mcSrc, workDir);
    }

    if (onProgress) onProgress("准备下载...", 30);
    downloadAndFinalize(mcVersion, forgeVer, neoVer, fabricVer,
                        workDir, finalDir, name, {}, onProgress, onComplete);
}

// ---- Type 3: MCBBS ----

static void installMCBBS(const QString &filePath, const QString &packDir,
                          const QString &instanceName,
                          PackProgressCallback onProgress,
                          PackCompleteCallback onComplete) {
    // 尝试读取 mcbbs.packmeta，否则读 manifest.json
    json meta;
    QString mcVersion, forgeVer, neoVer, fabricVer;
    QString overridesDir = "overrides";

    if (QFile::exists(packDir + "/mcbbs.packmeta")) {
        QFile f(packDir + "/mcbbs.packmeta");
        if (f.open(QIODevice::ReadOnly)) {
            bool ok; meta = parseJsonSafe(f.readAll(), &ok); if (!ok) { if (onComplete) onComplete(false, "packmeta 解析失败"); return; }
            f.close();
        }
    } else if (QFile::exists(packDir + "/manifest.json")) {
        QFile f(packDir + "/manifest.json");
        if (f.open(QIODevice::ReadOnly)) {
            bool ok; meta = parseJsonSafe(f.readAll(), &ok); if (!ok) { if (onComplete) onComplete(false, "packmeta 解析失败"); return; }
            f.close();
        }
        overridesDir = QString::fromStdString(meta.value("overrides", "overrides"));
    }

    // 解析 addons
    if (meta.contains("addons")) {
        for (const auto &addon : meta["addons"]) {
            QString id = QString::fromStdString(addon.value("id", ""));
            QString ver = QString::fromStdString(addon.value("version", ""));
            if (id == "game") mcVersion = ver;
            else if (id == "forge") forgeVer = ver;
            else if (id == "neoforge") neoVer = ver;
            else if (id == "fabric") fabricVer = ver;
        }
    }
    QString name = instanceName.isEmpty() ? QString::fromStdString(meta.value("name", "MCBBS Modpack")) : instanceName;

    if (onProgress) onProgress("检测到 MCBBS 整合包: " + name, 15);

    QString finalDir = VersionManager::instance().mcFolder() + name + "/";
    if (!checkNameConflict(finalDir, name, !instanceName.isEmpty(), onComplete)) return;

    QString workDir = importWorkDir() + name + "/";
    QDir(workDir).removeRecursively();
    QDir().mkpath(workDir);

    // 复制 overrides
    QString srcOverride = packDir + "/" + overridesDir;
    if (QDir(srcOverride).exists()) {
        if (onProgress) onProgress("复制整合包文件...", 20);
        copyDir(srcOverride, workDir);
    }

    if (onProgress) onProgress("准备下载...", 30);
    downloadAndFinalize(mcVersion, forgeVer, neoVer, fabricVer,
                        workDir, finalDir, name, {}, onProgress, onComplete);
}

// ---- Type 4: Modrinth ----

static void installModrinth(const QString &filePath, const QString &packDir,
                             const QString &instanceName,
                             PackProgressCallback onProgress,
                             PackCompleteCallback onComplete) {
    QFile f(packDir + "/modrinth.index.json");
    if (!f.open(QIODevice::ReadOnly)) {
        if (onComplete) onComplete(false, "无法读取 modrinth.index.json");
        return;
    }
    bool ok; json index = parseJsonSafe(f.readAll(), &ok); if (!ok) { if (onComplete) onComplete(false, "modrinth.index.json 解析失败"); return; }
    f.close();

    // 从 dependencies 读取版本
    QString mcVersion, forgeVer, neoVer, fabricVer;
    if (index.contains("dependencies")) {
        for (const auto &[key, val] : index["dependencies"].items()) {
            QString ver = QString::fromStdString(val.get<std::string>());
            if (key == "minecraft") mcVersion = ver;
            else if (key == "forge") forgeVer = ver;
            else if (key == "neoforge" || key == "neo-forge") neoVer = ver;
            else if (key == "fabric-loader") fabricVer = ver;
        }
    }
    QString name = instanceName.isEmpty() ? QString::fromStdString(index.value("name", "Modrinth Modpack")) : instanceName;

    if (onProgress) onProgress("检测到 Modrinth 整合包: " + name, 15);

    QString finalDir = VersionManager::instance().mcFolder() + name + "/";
    if (!checkNameConflict(finalDir, name, !instanceName.isEmpty(), onComplete)) return;

    QString workDir = importWorkDir() + name + "/";
    QDir(workDir).removeRecursively();
    QDir().mkpath(workDir);

    // 复制 overrides 和 client-overrides
    for (const auto &sub : {"overrides", "client-overrides"}) {
        QString src = packDir + "/" + sub;
        if (QDir(src).exists()) {
            copyDir(src, workDir);
        }
    }

    // 提取 mod 列表（Modrinth 有直接下载 URL）
    QList<ModDownloadEntry> mods;
    if (index.contains("files")) {
        for (const auto &f : index["files"]) {
            ModDownloadEntry m;
            m.savePath = QString::fromStdString(f.value("path", "mods/unknown.jar"));
            if (f.contains("downloads") && f["downloads"].is_array() && !f["downloads"].empty()) {
                m.url = QString::fromStdString(f["downloads"][0].get<std::string>());
            }
            if (!m.url.isEmpty()) mods.append(m);
        }
    }

    if (onProgress) onProgress("准备下载...", 30);
    downloadAndFinalize(mcVersion, forgeVer, neoVer, fabricVer,
                        workDir, finalDir, name, mods, onProgress, onComplete);
}

// ---- forward declaration ----

static void installModpackFromDir(const QString &filePath, const QString &packDir,
                                   PackType type, const QString &instanceName,
                                   PackProgressCallback onProgress,
                                   PackCompleteCallback onComplete);

// ---- Type 9: Launcher pack (recursive into modpack.zip/mrpack) ----

static void installLauncherPack(const QString &filePath, const QString &packDir,
                                 const QString &instanceName,
                                 PackProgressCallback onProgress,
                                 PackCompleteCallback onComplete) {
    QString mcFolder = VersionManager::instance().mcFolder();
    QString tmpRoot = mcFolder + "tmp/";

    // 查找 modpack.zip 或 modpack.mrpack（root 或一级子目录）
    QString innerPack;
    if (QFile::exists(packDir + "/modpack.zip"))
        innerPack = packDir + "/modpack.zip";
    else if (QFile::exists(packDir + "/modpack.mrpack"))
        innerPack = packDir + "/modpack.mrpack";
    else {
        QDir dir(packDir);
        for (const auto &entry : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            QString inner = entry.absoluteFilePath() + "/modpack.zip";
            if (QFile::exists(inner)) { innerPack = inner; break; }
            inner = entry.absoluteFilePath() + "/modpack.mrpack";
            if (QFile::exists(inner)) { innerPack = inner; break; }
        }
    }

    if (innerPack.isEmpty()) {
        if (onComplete) onComplete(false, "未在压缩包中找到 modpack.zip / modpack.mrpack");
        return;
    }

    if (onProgress) onProgress("检测到嵌套整合包，递归解包...", 10);
    PackType innerType = detectPackType(innerPack);
    QString fallbackDir = tmpRoot + "modpack_fallback/";
    QDir().mkpath(fallbackDir);
    if (!extractZip(innerPack, fallbackDir, onProgress, 10)) {
        if (onComplete) onComplete(false, "解压内层整合包失败");
        return;
    }
    installModpackFromDir(innerPack, fallbackDir, innerType, instanceName, onProgress, onComplete);
}

// ---- Fallback: Compressed .minecraft ----

// 从 LatestLaunch.bat 中提取版本名（title 行: "title 启动 - 1.21.1-NeoForge_21.1.226"）
static QString parseVersionFromBat(const QString &batPath) {
    QFile f(batPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    QString content = QString::fromUtf8(f.readAll());
    f.close();

    // 尝试匹配 title 行: "title 启动 - XXX"
    QRegularExpression re("title\\s+[^-]+-\\s*(\\S+)");
    auto match = re.match(content);
    if (match.hasMatch())
        return match.captured(1).trimmed();

    // 回退: 找 --version 参数
    QRegularExpression reVer("--version\\s+(\\S+)");
    auto matchVer = reVer.match(content);
    if (matchVer.hasMatch())
        return matchVer.captured(1).trimmed();

    return {};
}

// 在目录及其一级子目录中查找 .minecraft 或 PCL 标记
// 返回 {rootDir, isPclPack}
static QPair<QString, bool> findMcOrPclRoot(const QString &dir) {
    // 直接匹配 dir/.minecraft/
    if (QDir(dir + "/.minecraft").exists())
        return {dir, QFile::exists(dir + "/PCL/Setup.ini") &&
                     QFile::exists(dir + "/PCL/LatestLaunch.bat")};

    // 直接匹配 dir/PCL/ 标记（无 .minecraft 但有 PCL 配置）
    if (QFile::exists(dir + "/PCL/Setup.ini") &&
        QFile::exists(dir + "/PCL/LatestLaunch.bat"))
        return {dir, true};

    // 一级子目录
    QDir d(dir);
    for (const auto &entry : d.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QString sub = entry.absoluteFilePath();
        if (QDir(sub + "/.minecraft").exists())
            return {sub, QFile::exists(sub + "/PCL/Setup.ini") &&
                         QFile::exists(sub + "/PCL/LatestLaunch.bat")};
        if (QFile::exists(sub + "/PCL/Setup.ini") &&
            QFile::exists(sub + "/PCL/LatestLaunch.bat"))
            return {sub, true};
    }
    return {{}, false};
}

// 从 .minecraft/versions/ 目录提取 MC 版本名
static QString detectMcVersion(const QString &rootDir) {
    QString base = QDir(rootDir + "/.minecraft").exists() ? rootDir + "/.minecraft" : rootDir;
    QDir vd(base + "/versions");
    if (!vd.exists()) return {};
    for (const auto &entry : vd.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (QFile::exists(entry.absoluteFilePath() + "/" + entry.fileName() + ".json"))
            return entry.fileName();
    }
    return {};
}

// 处理已找到的 .minecraft / PCL 根目录
static void processCompressedRoot(const QString &rootDir, bool isPclPack,
                                   const QString &filePath, const QString &instanceName,
                                   const QString &mcFolder,
                                   PackProgressCallback onProgress,
                                   PackCompleteCallback onComplete) {
    // 确定实例名：优先用户指定 → zip 文件名 → 兜底 LatestLaunch.bat
    QString name = instanceName;
    if (name.isEmpty())
        name = QFileInfo(filePath).completeBaseName();
    if (name.isEmpty() && isPclPack) {
        QString batPath = rootDir + "/PCL/LatestLaunch.bat";
        name = parseVersionFromBat(batPath);
    }

    if (onProgress) onProgress(
        isPclPack ? "检测到 PCL 启动器整合包: " + name
                  : "检测到压缩版 .minecraft: " + name, 15);

    // 检测 MC 版本（用于下载正确的平台文件）
    QString mcVersion = detectMcVersion(rootDir);
    if (mcVersion.isEmpty() && isPclPack)
        mcVersion = parseVersionFromBat(rootDir + "/PCL/LatestLaunch.bat");

    // 定位 .minecraft 内容源
    QString mcSource = rootDir;
    bool hasMcFolder = QDir(rootDir + "/.minecraft").exists();
    if (hasMcFolder)
        mcSource = rootDir + "/.minecraft/";

    // tmp 工作目录，下载完成前不放入游戏目录
    QString finalDir = mcFolder + name + "/";
    if (!checkNameConflict(finalDir, name, !instanceName.isEmpty(), onComplete)) return;

    QString workDir = importWorkDir() + name + "/";
    QDir(workDir).removeRecursively();
    QDir().mkpath(workDir);

    if (onProgress) onProgress("复制整合包文件...", 20);
    copyDir(mcSource, workDir);

    // 如果 PCL/ 在 .minecraft 同级，也复制到工作目录
    if (hasMcFolder && QDir(rootDir + "/PCL").exists())
        copyDir(rootDir + "/PCL/", workDir + "PCL/");

    // 如果根目录有散落的 mod jar，复制到工作目录 mods/
    QDir rd(rootDir);
    bool hasLooseJars = false;
    for (const auto &entry : rd.entryInfoList(QDir::Files | QDir::NoDotAndDotDot)) {
        if (entry.fileName().endsWith(".jar", Qt::CaseInsensitive)) {
            QDir(workDir + "mods/").mkpath(".");
            QFile::copy(entry.absoluteFilePath(), workDir + "mods/" + entry.fileName());
            hasLooseJars = true;
        }
    }
    if (hasLooseJars && onProgress)
        onProgress("已复制附加模组...", 22);

    // 下载 MC 版本文件（确保 libraries/natives/assets 匹配当前平台）
    if (onProgress) onProgress("准备下载...", 30);
    downloadAndFinalize(mcVersion, {}, {}, {},
                        workDir, finalDir, name, {}, onProgress, onComplete);
}

static void installCompressed(const QString &filePath, const QString &packDir,
                               const QString &instanceName,
                               PackProgressCallback onProgress,
                               PackCompleteCallback onComplete) {
    QString mcFolder = VersionManager::instance().mcFolder();

    // Step 1: 在 packDir 中直接查找 .minecraft 或 PCL 标记
    auto [rootDir, isPcl] = findMcOrPclRoot(packDir);
    if (!rootDir.isEmpty()) {
        processCompressedRoot(rootDir, isPcl, filePath, instanceName,
                              mcFolder, onProgress, onComplete);
        return;
    }

    // Step 2: 递归解压内层 zip（最多二级），查找 .minecraft
    QString tmpRoot = mcFolder + "tmp/";
    QList<QPair<QString, int>> zipQueue;
    int innerSeq = 0;

    // 收集 packDir 中的 zip
    QDir pd(packDir);
    for (const auto &entry : pd.entryInfoList(QDir::Files | QDir::NoDotAndDotDot)) {
        QString n = entry.fileName().toLower();
        if (n.endsWith(".zip") || n.endsWith(".mrpack"))
            zipQueue.append({entry.absoluteFilePath(), 0});
    }
    for (const auto &subdir : pd.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QDir sd(subdir.absoluteFilePath());
        for (const auto &entry : sd.entryInfoList(QDir::Files | QDir::NoDotAndDotDot)) {
            QString n = entry.fileName().toLower();
            if (n.endsWith(".zip") || n.endsWith(".mrpack"))
                zipQueue.append({entry.absoluteFilePath(), 0});
        }
    }

    while (!zipQueue.isEmpty()) {
        auto [zipPath, depth] = zipQueue.takeFirst();
        if (depth >= 2) continue;

        QString extractDir = tmpRoot + "inner_" + QString::number(innerSeq++) + "/";
        QDir().mkpath(extractDir);

        if (!extractZip(zipPath, extractDir, onProgress, 15 + depth * 5))
            continue;

        auto [innerRoot, innerPcl] = findMcOrPclRoot(extractDir);
        if (!innerRoot.isEmpty()) {
            if (onProgress) onProgress("在内层压缩包中找到游戏文件...", 15 + depth * 5);

            // 也复制外层散落的 mod jar
            QDir outerDir = QFileInfo(zipPath).absoluteDir();
            for (const auto &entry : outerDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot)) {
                if (entry.fileName().endsWith(".jar", Qt::CaseInsensitive)) {
                    // 把 jar 临时复制到 rootDir 让 processCompressedRoot 处理
                    QFile::copy(entry.absoluteFilePath(), innerRoot + "/" + entry.fileName());
                }
            }

            processCompressedRoot(innerRoot, innerPcl, filePath, instanceName,
                                  mcFolder, onProgress, onComplete);
            return;
        }

        // 继续深入一级
        if (depth < 1) {
            QDir ed(extractDir);
            for (const auto &entry : ed.entryInfoList(QDir::Files | QDir::NoDotAndDotDot)) {
                QString n = entry.fileName().toLower();
                if (n.endsWith(".zip") || n.endsWith(".mrpack"))
                    zipQueue.append({entry.absoluteFilePath(), depth + 1});
            }
        }
    }

    // Step 3: 回退 — 通过 unzip -l 定位 versions 模式，直接解压
    QProcess unzip;
    unzip.start("unzip", {"-l", filePath});
    unzip.waitForFinished(5000);
    QStringList entries = QString::fromUtf8(unzip.readAllStandardOutput()).split('\n');

    QStringList names;
    for (const auto &line : entries) {
        auto parts = line.trimmed().split(QRegularExpression("\\s+"));
        if (parts.size() >= 4) {
            QString n = parts.mid(3).join(' ');
            if (!n.isEmpty() && !n.startsWith("-")) names.append(n);
        }
    }

    QString mcRoot = findMcRoot(names);
    QString name = instanceName.isEmpty() ? QFileInfo(filePath).completeBaseName() : instanceName;

    if (onProgress) onProgress("检测到压缩版 .minecraft: " + name, 15);

    QString finalDir = mcFolder + name + "/";
    if (!checkNameConflict(finalDir, name, !instanceName.isEmpty(), onComplete)) return;

    // Compressed 已含全部文件，直接在 tmp 解压后移入
    QString workDir = importWorkDir() + name + "/";
    QDir(workDir).removeRecursively();
    QDir().mkpath(workDir);
    if (onProgress) onProgress("解压中...", 20);
    extractZip(filePath, workDir, onProgress, 25);

    // 如有 mcRoot 包装层，把内容提出来
    if (!mcRoot.isEmpty()) {
        QString innerDir = workDir + mcRoot;
        if (QDir(innerDir).exists() && innerDir != workDir) {
            copyDir(innerDir, workDir);
            QDir(innerDir).removeRecursively();
        }
    }

    QDir().mkpath(QFileInfo(finalDir).absolutePath());
    copyDir(workDir, finalDir);
    QDir(workDir).removeRecursively();

    QDir(finalDir + "PCL/").mkpath(".");
    QSettings ini(finalDir + "PCL/Setup.ini", QSettings::IniFormat);
    ini.beginGroup("Setup");
    ini.setValue("Name", name);
    ini.setValue("VersionArgumentIndie", 1);
    ini.setValue("VersionArgumentIndieV2", true);
    ini.endGroup();
    ini.sync();
    if (onComplete) onComplete(true, name);
}

// ---- download pipeline ----

static void downloadModsAsync(const QList<ModDownloadEntry> &mods, int index,
                               const QString &workDir, const QString &finalDir, const QString &name,
                               PackProgressCallback onProgress,
                               PackCompleteCallback onComplete) {
    // 所有 mod 下载完成后的 finalize
    auto finalizeNow = [=]() {
        QDir().mkpath(QFileInfo(finalDir).absolutePath());
        copyDir(workDir, finalDir);
        QDir(workDir).removeRecursively();
        QDir(finalDir + "PCL/").mkpath(".");
        QSettings ini(finalDir + "PCL/Setup.ini", QSettings::IniFormat);
        ini.beginGroup("Setup");
        ini.setValue("Name", name);
        ini.setValue("VersionArgumentIndie", 1);
        ini.setValue("VersionArgumentIndieV2", true);
        ini.endGroup();
        ini.sync();
        if (onComplete) onComplete(true, name);
    };

    if (index >= mods.size()) {
        finalizeNow();
        return;
    }

    const auto &mod = mods[index];
    if (onProgress) onProgress(QString("下载 Mod (%1/%2)...").arg(index + 1).arg(mods.size()),
                                70 + (30 * index / mods.size()));

    if (!mod.url.isEmpty()) {
        // 直接 URL（Modrinth）
        DownloadManager::instance().download(mod.url, workDir + mod.savePath,
            nullptr,
            [=](bool ok, QString) {
                if (!ok) qWarning() << "Mod download failed:" << mod.url;
                downloadModsAsync(mods, index + 1, workDir, finalDir, name, onProgress, onComplete);
            });
    } else if (!mod.cfModId.isEmpty()) {
        // CurseForge — 通过 ModPlatform 解析下载 URL
        ModPlatform::instance().downloadMod(ModPlatform::CurseForge,
            mod.cfModId, mod.cfFileId, workDir + mod.savePath,
            [=](bool ok, QString) {
                if (!ok) qWarning() << "CF mod download failed:" << mod.cfModId;
                downloadModsAsync(mods, index + 1, workDir, finalDir, name, onProgress, onComplete);
            });
    } else {
        downloadModsAsync(mods, index + 1, workDir, finalDir, name, onProgress, onComplete);
    }
}

static void downloadAndFinalize(const QString &mcVersion,
                                 const QString &forgeVer, const QString &neoVer, const QString &fabricVer,
                                 const QString &workDir, const QString &finalDir, const QString &name,
                                 const QList<ModDownloadEntry> &mods,
                                 PackProgressCallback onProgress,
                                 PackCompleteCallback onComplete) {
    // 提取纯净 MC 版本号（去掉 modloader 前缀，如 "1.21.1-NeoForge_21.1.226" → "1.21.1"）
    auto vanillaVersion = [](const QString &v) -> QString {
        QRegularExpression re(R"(^\d+\.\d+(?:\.\d+)?)");
        auto m = re.match(v);
        return m.hasMatch() ? m.captured(0) : v;
    };

    auto startModDownloads = [=]() {
        downloadModsAsync(mods, 0, workDir, finalDir, name, onProgress, onComplete);
    };

    if (mcVersion.isEmpty()) {
        startModDownloads();
        return;
    }

    // Step 1: 下载 MC 版本（JSON + JAR + libraries + assets）
    QString vanilla = vanillaVersion(mcVersion);
    if (onProgress) onProgress("下载 Minecraft " + vanilla + "...", 35);
    AssetDownloader::instance().downloadVersion(vanilla,
        [=](bool ok, QString err) {
            if (!ok) {
                QDir(workDir).removeRecursively();
                if (onComplete) onComplete(false, "下载 Minecraft 失败: " + err);
                return;
            }

            // Step 2: 下载 natives（平台相关原生库）
            if (onProgress) onProgress("下载原生库...", 55);
            McVersion ver;
            ver.id = vanilla;
            ver.pathVersion = VersionManager::instance().mcFolder() + vanilla + "/";
            ver.pathJar = ver.pathVersion + vanilla + ".jar";
            ver.pathIndie = VersionManager::instance().mcFolder();
            AssetDownloader::instance().downloadNatives(ver,
                [=](bool ok2, QString err2) {
                    if (!ok2)
                        qWarning() << "Natives download issue:" << err2;

                    // Step 3: 安装 modloader
                    auto installLoader = [=]() {
                        QString loaderType;
                        QString loaderVer;
                        if (!forgeVer.isEmpty())   { loaderType = "forge"; loaderVer = forgeVer; }
                        else if (!neoVer.isEmpty()) { loaderType = "neoforge"; loaderVer = neoVer; }
                        else if (!fabricVer.isEmpty()) { loaderType = "fabric"; loaderVer = fabricVer; }

                        if (loaderType.isEmpty()) {
                            startModDownloads();
                            return;
                        }

                        if (onProgress) onProgress("安装 " + loaderType + "...", 65);
                        auto javaList = JavaManager::instance().javaList();
                        if (javaList.isEmpty()) {
                            qWarning() << "No Java found, skipping modloader install";
                            startModDownloads();
                            return;
                        }
                        QString javaPath = javaList.first().pathJava;

                        Installer::instance().installLoader(loaderType,
                            ver.pathVersion, loaderVer, javaPath,
                            [=](bool ok3, QString err3) {
                                if (!ok3)
                                    qWarning() << "Modloader install issue:" << err3;
                                startModDownloads();
                            });
                    };

                    installLoader();
                });
        });
}

// ---- internal dispatch (from already-extracted directory) ----

static void installModpackFromDir(const QString &filePath, const QString &packDir,
                                   PackType type, const QString &instanceName,
                                   PackProgressCallback onProgress,
                                   PackCompleteCallback onComplete) {
    // 处理一级目录包装（如 zip/蛊真人/ → 进入子目录）
    QString effectiveDir = packDir;
    QDir rootDir(effectiveDir);
    auto entries = rootDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    if (entries.size() == 1) {
        QString subDir = entries.first().absoluteFilePath() + "/";
        QDir sub(subDir);
        if (sub.exists("manifest.json") || sub.exists("modrinth.index.json") ||
            sub.exists("mmc-pack.json") || sub.exists("mcbbs.packmeta") ||
            sub.exists("modpack.json") || sub.exists("modpack.zip") ||
            sub.exists("modpack.mrpack") ||
            sub.exists("PCL/Setup.ini")) {
            effectiveDir = subDir;
        }
    }

    // 分派到对应的安装器
    switch (type) {
    case PackType::CurseForge:
        installCurseForge(filePath, effectiveDir, instanceName, onProgress, onComplete);
        break;
    case PackType::HMCL:
        installHMCL(filePath, effectiveDir, instanceName, onProgress, onComplete);
        break;
    case PackType::MultiMC:
        installMultiMC(filePath, effectiveDir, instanceName, onProgress, onComplete);
        break;
    case PackType::MCBBS:
        installMCBBS(filePath, effectiveDir, instanceName, onProgress, onComplete);
        break;
    case PackType::Modrinth:
        installModrinth(filePath, effectiveDir, instanceName, onProgress, onComplete);
        break;
    case PackType::LauncherPack:
        installLauncherPack(filePath, effectiveDir, instanceName, onProgress, onComplete);
        break;
    case PackType::Compressed:
        installCompressed(filePath, effectiveDir, instanceName, onProgress, onComplete);
        break;
    default:
        // 回退：当作压缩版 .minecraft 处理
        installCompressed(filePath, effectiveDir, instanceName, onProgress, onComplete);
        break;
    }
}

// ---- main entry ----

void installModpack(const QString &filePath,
                     const QString &instanceName,
                     PackProgressCallback onProgress,
                     PackCompleteCallback onComplete) {
    if (!QFile::exists(filePath)) {
        if (onComplete) onComplete(false, "文件不存在: " + filePath);
        return;
    }

    if (onProgress) onProgress("正在检测整合包类型...", 5);

    PackType type = detectPackType(filePath);
    if (onProgress) onProgress("类型: " + packTypeName(type), 8);

    // 在游戏目录下创建 tmp 目录，解压到 tmp/pack/
    QString mcFolder = VersionManager::instance().mcFolder();
    // 设置 mcFolder 供 AssetDownloader 使用
    DownloadManager::instance().setProperty("mcFolder", mcFolder);
    QString tmpRoot = mcFolder + "tmp/";

    // 清理旧 tmp，确保干净环境
    QDir(tmpRoot).removeRecursively();
    QDir().mkpath(tmpRoot);

    QString packDir = tmpRoot + "pack/";
    QDir().mkpath(packDir);

    if (!extractZip(filePath, packDir, onProgress, 10)) {
        if (onComplete) onComplete(false, "解压失败");
        QDir(tmpRoot).removeRecursively();
        return;
    }

    installModpackFromDir(filePath, packDir, type, instanceName, onProgress, onComplete);

    // 导入完成后清理 tmp 目录
    QDir(tmpRoot).removeRecursively();
}
