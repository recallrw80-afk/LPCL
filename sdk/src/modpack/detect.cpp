// 整合包类型检测（detectPackType / packTypeName）
#include "modpack_common.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

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

    // Type 9b: Launcher pack — 有 jar + 单个内层 zip，内层含 .minecraft 或 versions/ 结构
    {
        QString innerZipPath;  // 内层 zip 在压缩包内的完整路径
        bool hasJars = false;
        int  zipCount = 0;
        for (const auto &e : entries) {
            int slashes = e.count('/');
            if (slashes > 1) continue;
            QString fn = slashes == 0 ? e : e.mid(e.indexOf('/') + 1);
            if (fn.endsWith(".jar", Qt::CaseInsensitive)) hasJars = true;
            if (fn.endsWith(".zip", Qt::CaseInsensitive) || fn.endsWith(".mrpack", Qt::CaseInsensitive)) {
                innerZipPath = e;  // 保留完整路径用于 unzip -p
                zipCount++;
            }
        }

        if (hasJars && zipCount == 1) {
            // 把内层 zip 解出到唯一临时文件再检测（避免固定路径并发冲突）
            QString tmpInner = QDir::temp().filePath(
                QString("_lpcl_detect_%1.zip").arg(QCoreApplication::applicationPid()));
            QFile::remove(tmpInner);
            QString innerList;
            QProcess peek;
            peek.setStandardOutputFile(tmpInner);
            peek.start("unzip", {"-p", filePath, innerZipPath});
            if (peek.waitForFinished(15000) && peek.exitCode() <= 1) {
                QProcess lister;
                lister.start("unzip", {"-l", tmpInner});
                if (lister.waitForFinished(15000))
                    innerList = QString::fromUtf8(lister.readAllStandardOutput());
            }
            QFile::remove(tmpInner);
            if (innerList.contains("/.minecraft/") || innerList.contains("versions/"))
                return PackType::LauncherPack;
        }
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
    case PackType::LauncherPack: return "Launcher Pack";
    case PackType::Compressed:   return "Compressed .minecraft";
    default:                     return "Unknown";
    }
}
