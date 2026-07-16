#include "modpack.h"
#include "core/settings.h"
#include "core/versionmanager.h"
#include "core/installer.h"
#include "download/downloadmanager.h"
#include "util/file_utils.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
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
    return unzip.exitCode() == 0;
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

    // 复制 overrides
    QString overridesDir = QString::fromStdString(manifest.value("overrides", "overrides"));
    QString targetDir = VersionManager::instance().mcFolder() + "/versions/" + name + "/";
    QDir().mkpath(targetDir);

    QString srcOverride = packDir + "/" + overridesDir;
    if (QDir(srcOverride).exists()) {
        if (onProgress) onProgress("复制整合包文件...", 20);
        // cp -r
        copyDir(srcOverride, targetDir);
    }

    // TODO Phase 4: 从 CurseForge API 下载模组文件
    if (onProgress) onProgress("（模组下载功能将在 Phase 4 实现）", 30);

    // 写 Setup.ini
    QDir(targetDir + "LPCL/").mkpath(".");
    QFile setupIni(targetDir + "PCL/Setup.ini");
    if (setupIni.open(QIODevice::WriteOnly)) {
        setupIni.write("[Setup]\nVersionArgumentIndie=1\nVersionArgumentIndieV2=True\n");
        setupIni.close();
    }

    if (onComplete) onComplete(true, name);
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

    QString targetDir = VersionManager::instance().mcFolder() + "/versions/" + name + "/";
    QDir().mkpath(targetDir);

    // 复制 minecraft/ 文件夹
    QString mcSrc = packDir + "/minecraft/";
    if (QDir(mcSrc).exists()) {
        if (onProgress) onProgress("复制整合包文件...", 20);
        copyDir(mcSrc, targetDir);
    }

    QDir(targetDir + "LPCL/").mkpath(".");
    QFile setupIni(targetDir + "PCL/Setup.ini");
    if (setupIni.open(QIODevice::WriteOnly)) {
        setupIni.write("[Setup]\nVersionArgumentIndie=1\nVersionArgumentIndieV2=True\n");
        setupIni.close();
    }

    if (onComplete) onComplete(true, name);
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

    QString targetDir = VersionManager::instance().mcFolder() + "/versions/" + name + "/";
    QDir().mkpath(targetDir);

    // 复制 .minecraft/ 文件夹
    QString mcSrc = packDir + "/.minecraft/";
    if (QDir(mcSrc).exists()) {
        if (onProgress) onProgress("复制整合包文件...", 20);
        copyDir(mcSrc, targetDir);
    }

    QDir(targetDir + "LPCL/").mkpath(".");
    QFile setupIni(targetDir + "PCL/Setup.ini");
    if (setupIni.open(QIODevice::WriteOnly)) {
        setupIni.write("[Setup]\nVersionArgumentIndie=1\nVersionArgumentIndieV2=True\n");
        setupIni.close();
    }

    if (onComplete) onComplete(true, name);
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

    QString targetDir = VersionManager::instance().mcFolder() + "/versions/" + name + "/";
    QDir().mkpath(targetDir);

    // 复制 overrides
    QString srcOverride = packDir + "/" + overridesDir;
    if (QDir(srcOverride).exists()) {
        if (onProgress) onProgress("复制整合包文件...", 20);
        copyDir(srcOverride, targetDir);
    }

    QDir(targetDir + "LPCL/").mkpath(".");
    QFile setupIni(targetDir + "PCL/Setup.ini");
    if (setupIni.open(QIODevice::WriteOnly)) {
        setupIni.write("[Setup]\nVersionArgumentIndie=1\nVersionArgumentIndieV2=True\n");
        setupIni.close();
    }

    if (onComplete) onComplete(true, name);
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

    QString targetDir = VersionManager::instance().mcFolder() + "/versions/" + name + "/";
    QDir().mkpath(targetDir);

    // 复制 overrides 和 client-overrides
    for (const auto &sub : {"overrides", "client-overrides"}) {
        QString src = packDir + "/" + sub;
        if (QDir(src).exists()) {
            copyDir(src, targetDir);
        }
    }

    // TODO Phase 4: 从 downloads 下载模组文件
    if (onProgress) onProgress("（模组下载功能将在 Phase 4 实现）", 30);

    QDir(targetDir + "LPCL/").mkpath(".");
    QFile setupIni(targetDir + "PCL/Setup.ini");
    if (setupIni.open(QIODevice::WriteOnly)) {
        setupIni.write("[Setup]\nVersionArgumentIndie=1\nVersionArgumentIndieV2=True\n");
        setupIni.close();
    }

    if (onComplete) onComplete(true, name);
}

// ---- Type 9: Launcher pack (recursive) ----

static void installLauncherPack(const QString &filePath, const QString &packDir,
                                 const QString &instanceName,
                                 PackProgressCallback onProgress,
                                 PackCompleteCallback onComplete) {
    // 找到 modpack.zip 或 modpack.mrpack
    QString innerPack;
    if (QFile::exists(packDir + "/modpack.zip"))
        innerPack = packDir + "/modpack.zip";
    else if (QFile::exists(packDir + "/modpack.mrpack"))
        innerPack = packDir + "/modpack.mrpack";

    if (!innerPack.isEmpty()) {
        if (onProgress) onProgress("检测到嵌套整合包，递归解包...", 10);
        // 递归调用 installModpack
        installModpack(innerPack, instanceName, onProgress, onComplete);
        return;
    }

    // 否则尝试找一级目录下的 modpack.*
    QDir dir(packDir);
    for (const auto &entry : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QString inner = entry.absoluteFilePath() + "/modpack.zip";
        if (QFile::exists(inner)) {
            installModpack(inner, instanceName, onProgress, onComplete);
            return;
        }
        inner = entry.absoluteFilePath() + "/modpack.mrpack";
        if (QFile::exists(inner)) {
            installModpack(inner, instanceName, onProgress, onComplete);
            return;
        }
    }

    if (onComplete) onComplete(false, "未在压缩包中找到 modpack.zip / modpack.mrpack");
}

// ---- Fallback: Compressed .minecraft ----

static void installCompressed(const QString &filePath, const QString &packDir,
                               const QString &instanceName,
                               PackProgressCallback onProgress,
                               PackCompleteCallback onComplete) {
    // 找到 .minecraft 根目录
    QProcess unzip;
    unzip.start("unzip", {"-l", filePath});
    unzip.waitForFinished(5000);
    QStringList entries = QString::fromUtf8(unzip.readAllStandardOutput()).split('\n');

    // 提取 entries 中的文件名
    QStringList names;
    for (const auto &line : entries) {
        auto parts = line.trimmed().split(QRegularExpression("\\s+"));
        if (parts.size() >= 4) {
            QString name = parts.mid(3).join(' ');
            if (!name.isEmpty() && !name.startsWith("-")) names.append(name);
        }
    }

    QString mcRoot = findMcRoot(names);
    QString name = instanceName.isEmpty() ? QFileInfo(filePath).completeBaseName() : instanceName;
    QString targetDir;

    if (onProgress) onProgress("检测到压缩版 .minecraft: " + name, 15);

    if (mcRoot.isEmpty()) {
        // 没有 versions/X/X.json 结构，整个解压到 versions/{name}/
        targetDir = VersionManager::instance().mcFolder() + "/versions/" + name + "/";
        QDir().mkpath(targetDir);
        if (onProgress) onProgress("解压中（无标准结构，直接展开）...", 20);
        extractZip(filePath, targetDir, onProgress, 25);
    } else {
        // 有标准结构，解压到 .minecraft/
        targetDir = VersionManager::instance().mcFolder();
        if (onProgress) onProgress("解压到 " + targetDir + " ...", 20);
        extractZip(filePath, targetDir, onProgress, 25);
    }

    QDir(targetDir + "/PCL/").mkpath(".");
    QFile setupIni(targetDir + "PCL/Setup.ini");
    if (setupIni.open(QIODevice::WriteOnly)) {
        setupIni.write("[Setup]\nVersionArgumentIndie=1\nVersionArgumentIndieV2=True\n");
        setupIni.close();
    }

    if (onComplete) onComplete(true, name);
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

    // 创建临时解压目录
    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        if (onComplete) onComplete(false, "无法创建临时目录");
        return;
    }
    QString packDir = tempDir.path();

    if (!extractZip(filePath, packDir, onProgress, 10)) {
        if (onComplete) onComplete(false, "解压失败");
        return;
    }

    // 处理一级目录包装（如 zip/蛊真人/ → 进入子目录）
    QDir rootDir(packDir);
    auto entries = rootDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    if (entries.size() == 1) {
        // 只有一个子目录，可能是包装层
        QString subDir = entries.first().absoluteFilePath() + "/";
        // 检查子目录内是否有标记文件
        QDir sub(subDir);
        if (sub.exists("manifest.json") || sub.exists("modrinth.index.json") ||
            sub.exists("mmc-pack.json") || sub.exists("mcbbs.packmeta") ||
            sub.exists("modpack.json") || sub.exists("modpack.zip") ||
            sub.exists("modpack.mrpack")) {
            packDir = subDir;
        }
    }

    // 分派到对应的安装器
    switch (type) {
    case PackType::CurseForge:
        installCurseForge(filePath, packDir, instanceName, onProgress, onComplete);
        break;
    case PackType::HMCL:
        installHMCL(filePath, packDir, instanceName, onProgress, onComplete);
        break;
    case PackType::MultiMC:
        installMultiMC(filePath, packDir, instanceName, onProgress, onComplete);
        break;
    case PackType::MCBBS:
        installMCBBS(filePath, packDir, instanceName, onProgress, onComplete);
        break;
    case PackType::Modrinth:
        installModrinth(filePath, packDir, instanceName, onProgress, onComplete);
        break;
    case PackType::LauncherPack:
        installLauncherPack(filePath, packDir, instanceName, onProgress, onComplete);
        break;
    case PackType::Compressed:
        installCompressed(filePath, packDir, instanceName, onProgress, onComplete);
        break;
    default:
        // 回退：当作压缩版 .minecraft 处理
        installCompressed(filePath, packDir, instanceName, onProgress, onComplete);
        break;
    }
}
