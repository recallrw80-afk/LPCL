#include "lpcl.h"
#include "modpack.h"
#include "core/settings.h"
#include "core/versionmanager.h"
#include "core/javamanager.h"
#include "core/launcher.h"
#include "core/launchbuilder.h"
#include "auth/offlineauth.h"
#include "download/assetdownloader.h"
#include "download/downloadmanager.h"
#include "util/file_utils.h"
#include "util/platform_utils.h"
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QPointer>
#include <QSysInfo>
#include <QUuid>
#include <QSharedPointer>
#include <nlohmann/json.hpp>

namespace {

using json = nlohmann::json;

// 等待一个异步操作完成（CLI 同步等待）
static bool waitForAsync(const std::function<void(std::function<void(bool, QString)>)> &starter) {
    QEventLoop loop;
    QPointer<QEventLoop> guard = &loop;
    auto state = QSharedPointer<bool>::create(false);
    auto done = QSharedPointer<bool>::create(false);
    starter([state, done, guard](bool ok, QString) {
        *state = ok;
        *done = true;
        if (guard) guard->quit();
    });
    if (!*done) loop.exec();  // 同步完成路径下 quit 先于 exec，不能裸等
    return *state;
}

// 启动预检：补齐缺失的游戏文件（对照原版 PCL 的 DlClientFix 补全下载）
// vanilla 文件缺失 → 自动重下（downloadVersion 按 sha1 跳过已有文件）；
// loader/pack 的库缺失 → 报错（无来源可自动补齐）
static bool ensureLaunchReadyImpl(const McVersion &version) {
    auto &vm = VersionManager::instance();
    QString mcFolder = vm.mcFolder();
    QString vanillaId = version.vanillaVersion.toString();

    json resolved = VersionManager::resolveInheritanceChain(version.pathJson);
    if (resolved.is_null()) {
        qWarning() << "启动预检: 无法解析 version json:" << version.pathJson;
        return false;
    }

    bool needVanilla = !QFile::exists(version.pathJar);
    QSet<QString> vanillaLibs;
    QSet<QString> loaderMissing;

    // 收集 vanilla json 的库名集合（判断缺失库能否自动补齐）
    if (!vanillaId.isEmpty()) {
        json vanillaJson = VersionManager::resolveInheritanceChain(
            mcFolder + "versions/" + vanillaId + "/" + vanillaId + ".json");
        if (!vanillaJson.is_null() && vanillaJson.contains("libraries")) {
            for (const auto &lib : vanillaJson["libraries"])
                vanillaLibs.insert(QString::fromStdString(lib.value("name", "")));
        }
    }

    // libraries 存在性检查
    if (resolved.contains("libraries")) {
        for (const auto &lib : resolved["libraries"]) {
            // 按平台 rules 过滤（osx-only 等库在本平台本就不该存在，跳过而非判缺失）
            if (lib.contains("rules") && !LaunchBuilder::checkRules(lib["rules"])) continue;
            QString rel;
            QString name = QString::fromStdString(lib.value("name", ""));
            if (lib.contains("downloads") && lib["downloads"].contains("artifact")) {
                rel = QString::fromStdString(lib["downloads"]["artifact"].value("path", ""));
            } else {
                if (lib.contains("natives")) continue;
                rel = FileUtils::mavenNameToPath(name);
            }
            if (rel.isEmpty()) continue;
            if (QFile::exists(mcFolder + "libraries/" + rel)) continue;
            if (vanillaLibs.contains(name))
                needVanilla = true;
            else
                loaderMissing.insert(name);
        }
    }

    // assets 索引 + 对象存在性检查
    QString assetsIndexId = "legacy";
    if (resolved.contains("assetIndex"))
        assetsIndexId = QString::fromStdString(resolved["assetIndex"].value("id", ""));
    else if (resolved.contains("assets"))
        assetsIndexId = QString::fromStdString(resolved["assets"].get<std::string>());
    QString idxPath = mcFolder + "assets/indexes/" + assetsIndexId + ".json";
    if (!QFile::exists(idxPath)) {
        needVanilla = true;
    } else {
        QFile f(idxPath);
        if (f.open(QIODevice::ReadOnly)) {
            json idx = json::parse(f.readAll().toStdString(), nullptr, false);
            if (!idx.is_discarded() && idx.contains("objects")) {
                for (auto it = idx["objects"].begin(); it != idx["objects"].end(); ++it) {
                    QString hash = QString::fromStdString(it.value().value("hash", ""));
                    if (hash.isEmpty()) continue;
                    if (!QFile::exists(mcFolder + "assets/objects/" +
                                       FileUtils::assetPathFromHash(hash))) {
                        needVanilla = true;
                        break;
                    }
                }
            }
            f.close();
        }
    }

    if (needVanilla && vanillaId.isEmpty()) {
        qWarning() << "启动预检: 主 jar/资产缺失但无法确定 MC 版本";
        return false;
    }

    // 补齐 vanilla 缺失（downloadVersion 内部按 sha1 跳过已有文件）
    if (needVanilla) {
        qWarning() << "启动预检: 补齐缺失的游戏文件（MC" << vanillaId << "）...";
        if (!waitForAsync([&](std::function<void(bool, QString)> cb) {
                AssetDownloader::instance().downloadVersion(vanillaId, cb);
            })) {
            qWarning() << "启动预检: 补齐游戏文件失败";
            return false;
        }
    }

    if (!loaderMissing.isEmpty()) {
        qWarning() << "启动预检: modloader 库缺失（无法自动补齐，请重新导入整合包）:"
                   << loaderMissing.values().join(", ");
        return false;
    }

    // natives：目录为空则补
    QString nativesDir = mcFolder + "versions/" + vanillaId + "/natives/";
    if (!vanillaId.isEmpty() &&
        QDir(nativesDir).entryList(QDir::Files | QDir::NoDotAndDotDot).isEmpty()) {
        McVersion ver;
        ver.id = vanillaId;
        ver.pathVersion = mcFolder + "versions/" + vanillaId + "/";
        ver.pathJar = ver.pathVersion + vanillaId + ".jar";
        ver.pathIndie = mcFolder;
        qWarning() << "启动预检: 补齐 natives...";
        if (!waitForAsync([&ver](std::function<void(bool, QString)> cb) {
                AssetDownloader::instance().downloadNatives(ver, cb);
            })) {
            qWarning() << "启动预检: 补齐 natives 失败";
            return false;
        }
    }
    return true;
}

// 清单解析异常兜底：畸形 version json/清单的 nlohmann 异常统一转为干净失败
static bool ensureLaunchReady(const McVersion &version) {
    try {
        return ensureLaunchReadyImpl(version);
    } catch (const std::exception &e) {
        qWarning() << "启动预检: 清单解析失败:" << e.what();
        return false;
    }
}

// 中文输入修复：fcitx/ibus 的 XIM 会让 GLFW 3.3 在 glfwWaitEventsTimeout 里 SIGSEGV。
// LWJGL ≥ 3.3.3 自带的 GLFW 3.4 已重构 XIM 路径不会崩。
// 注意 LWJGL 的 natives 加载机制：它从 classpath 里的 *-natives-linux.jar 提取
// libglfw.so 到 SharedLibraryExtractPath，文件不匹配就重新提取覆盖——所以直接
// 替换 natives 目录或前置 java.library.path 都无效（均已实测）。
// 唯一可靠做法：把 libraries/ 里的 lwjgl-glfw-<ver>-natives-linux.jar 内容换成
// 3.3.6 版（文件名不变），LWJGL 提取出来的就是 GLFW 3.4。
// 任何失败都静默返回：doLaunch 会回退到 XMODIFIERS=@im=none（禁输入法式修复）。
static void maybeInstallGlfw34(const McVersion &version) {
    QString xim = qEnvironmentVariable("XMODIFIERS");
    if (!xim.contains("@im=") || xim == "@im=none") return;  // 无 IME 钩子，无需处理

    auto &vm = VersionManager::instance();
    QString mcFolder = vm.mcFolder();
    QString vanillaId = version.vanillaVersion.toString();
    if (vanillaId.isEmpty()) return;
    QString nativesDir = mcFolder + "versions/" + vanillaId + "/natives/";
    QString marker = nativesDir + "libglfw.so.glfw34-fixed";
    if (QFile::exists(marker)) return;

    // 找 LWJGL 版本；≥ 3.3.3 自带 GLFW 3.4，无需替换
    QString lwjglVer;
    json resolved = VersionManager::resolveInheritanceChain(version.pathJson);
    if (!resolved.is_null() && resolved.contains("libraries")) {
        for (const auto &lib : resolved["libraries"]) {
            QString name = QString::fromStdString(lib.value("name", ""));
            if (name.startsWith("org.lwjgl:lwjgl:") || name.startsWith("org.lwjgl:lwjgl-glfw:")) {
                QString ver = name.section(':', -1);
                QStringList p = ver.split('.');
                if (p.size() >= 3) {
                    int minor = p[1].toInt(), patch = p[2].toInt();
                    if (minor > 3 || (minor == 3 && patch >= 3)) return;
                }
                lwjglVer = ver;
                break;
            }
        }
    }
    if (lwjglVer.isEmpty()) return;

    QString jarPath = mcFolder + "libraries/org/lwjgl/lwjgl-glfw/" + lwjglVer +
                      "/lwjgl-glfw-" + lwjglVer + "-natives-linux.jar";
    if (!QFile::exists(jarPath)) return;

    QString tmpJar = mcFolder + "tmp/lwjgl-glfw-3.3.6-natives-linux.jar";
    bool ok = waitForAsync([&](std::function<void(bool, QString)> cb) {
        DownloadManager::instance().download(
            "https://repo1.maven.org/maven2/org/lwjgl/lwjgl-glfw/3.3.6/lwjgl-glfw-3.3.6-natives-linux.jar",
            tmpJar, nullptr, cb);
    });
    if (ok) {
        QString bak = jarPath + ".bak-vanilla";
        if (!QFile::exists(bak))
            QFile::copy(jarPath, bak);                         // 原 jar 备份只做一次
        QFile::remove(jarPath);
        ok = QFile::copy(tmpJar, jarPath);
        if (ok) {
            QFile m(marker);
            if (m.open(QIODevice::WriteOnly)) m.write("lwjgl-glfw 3.3.6 (GLFW 3.4)\n");
            qWarning() << "IME 修复: MC" << vanillaId << "将使用 GLFW 3.4（保留中文输入）";
        }
    }
    QFile::remove(tmpJar);
    if (!ok) qWarning() << "IME 修复: GLFW 3.4 准备失败，将回退到禁用 XIM 方案";
}

} // namespace

namespace lpcl {

QStringList listVersions() {
    auto &vm = VersionManager::instance();
    vm.loadLocalVersions();
    return vm.versionIds();
}

QStringList listMcVersions() {
    auto &vm = VersionManager::instance();
    vm.loadMcVersions();
    return vm.versionIds();
}

bool installJavaRuntime(int majorVersion, QString *errOut) {
    if (majorVersion <= 0) majorVersion = 8;
    QString osName;
    switch (currentPlatform()) {
    case Platform::Windows: osName = "windows"; break;
    case Platform::MacOS:   osName = "mac"; break;
    default:                osName = "linux"; break;
    }
    QString cpu = QSysInfo::currentCpuArchitecture();
    QString arch = (cpu == "arm64" || cpu == "aarch64") ? "aarch64"
                 : is64BitSystem() ? "x64" : "x86";
    QString url = QString("https://api.adoptium.net/v3/binary/latest/%1/ga/%2/%3/jre/hotspot/normal/eclipse")
                      .arg(majorVersion).arg(osName, arch);

    QString mcFolder = VersionManager::instance().mcFolder();
    QString tmpFile = mcFolder + "tmp/jre-" + QString::number(majorVersion)
                      + (osName == "windows" ? ".zip" : ".tar.gz");

    // 下载 JRE 包（Adoptium API 会 302 到实际地址，DownloadManager 跟随重定向）
    if (!waitForAsync([&](std::function<void(bool, QString)> cb) {
            DownloadManager::instance().download(url, tmpFile, nullptr, cb);
        })) {
        if (errOut) *errOut = "JRE 下载失败: " + url;
        return false;
    }

    // 解压到 {mcFolder}/javas/
    QString javasDir = mcFolder + "javas/";
    QString err;
    bool ok = (osName == "windows")
        ? FileUtils::extractZip(tmpFile, javasDir, &err)
        : FileUtils::extractTarGz(tmpFile, javasDir, &err);
    QFile::remove(tmpFile);
    if (!ok) {
        if (errOut) *errOut = "JRE 解压失败: " + err;
        return false;
    }

    // 扫描注册（scanFolder 递归查找 java 可执行文件并 checkJava 验证）
    JavaManager::instance().scanFolder(javasDir, false);
    return true;
}

bool installVersion(const QString &versionId,
                    std::function<void(const ImportProgress &)> onProgress) {
    // 空版本号 = 最新正式版（读官方版本清单 latest.release）
    QString id = versionId;
    if (id.isEmpty()) {
        json manifest;
        bool ok = waitForAsync([&](std::function<void(bool, QString)> cb) {
            DownloadManager::instance().downloadJson(
                "https://launchermeta.mojang.com/mc/game/version_manifest.json",
                [&manifest, cb](bool ok2, QString err, json m) {
                    manifest = std::move(m);
                    cb(ok2, err);
                });
        });
        if (!ok || !manifest.contains("latest")) return false;
        id = QString::fromStdString(manifest["latest"].value("release", ""));
        if (id.isEmpty()) return false;
        if (onProgress) onProgress({"Latest release: " + id, 0});
    }

    // 连接 AssetDownloader 的信号转发进度
    auto &ad = AssetDownloader::instance();
    auto conn1 = QObject::connect(&ad, &AssetDownloader::downloadLog,
        [onProgress](const QString &msg) {
            if (onProgress) onProgress({msg, -1});
        });
    auto conn2 = QObject::connect(&ad, &AssetDownloader::downloadProgress,
        [onProgress](const QString &msg, int cur, int total) {
            if (onProgress && total > 0) onProgress({msg, 5 + 70 * cur / total});
        });

    // Step 1: 下载 MC 版本（json + jar + libraries + assets）
    bool ok = waitForAsync([&](std::function<void(bool, QString)> cb) {
        ad.downloadVersion(id, cb);
    });
    if (!ok) {
        QObject::disconnect(conn1);
        QObject::disconnect(conn2);
        return false;
    }

    // Step 2: natives
    QString mcFolder = VersionManager::instance().mcFolder();
    McVersion ver;
    ver.id = id;
    ver.pathVersion = mcFolder + "versions/" + id + "/";
    ver.pathJar = ver.pathVersion + id + ".jar";
    ver.pathIndie = mcFolder;
    if (onProgress) onProgress({"Downloading native libraries...", 85});
    ok = waitForAsync([&](std::function<void(bool, QString)> cb) {
        ad.downloadNatives(ver, cb);
    });

    QObject::disconnect(conn1);
    QObject::disconnect(conn2);
    if (onProgress) onProgress({ok ? "Complete" : "Natives failed", 100});
    return ok;
}

bool launchVersion(const QString &versionId,
                   LogCallback onLog,
                   ExitCallback onExit) {
    auto &launcher = Launcher::instance();
    auto &jm = JavaManager::instance();

    // 解析：INI 映射命中 → 实例版本（loadInstanceVersion）；否则全局版本
    QString dirName = Settings::instance().dirForDisplayName(versionId);
    McVersion version;
    if (!dirName.isEmpty())
        version = VersionManager::instance().loadInstanceVersion(dirName);
    else
        version = VersionManager::instance().loadVersion(versionId);
    if (!version.isValid) return false;

    // 登录：使用选中的玩家 Profile（名字 + 皮肤类型，不再硬编码 "Player"）
    QString playerUuid = Settings::instance().selectedPlayer();
    QString playerName = Settings::instance().getProfile(playerUuid, "Name", "Player");
    if (playerName.isEmpty()) playerName = "Player";
    QString skinType = Settings::instance().getProfile(playerUuid, "SkinType", "slim");
    auto login = OfflineAuth::createOfflineLogin(playerName, skinType);

    if (jm.javaList().isEmpty()) {
        jm.scanSystemJava();
        jm.waitForScanFinished();
    }
    // 优先按版本兼容矩阵选 Java；无严格匹配时回退到最优可用
    // （如 1.12.2 机器上只有 Java 21/25，严格匹配 Java 8 会落空，但新版本也能跑）
    JavaEntry java = jm.selectJavaForVersion(version);
    if (java.pathJava.isEmpty()) java = jm.selectJava();
    if (java.pathJava.isEmpty()) {
        // 一个 Java 都没有：自动下载矩阵推荐的最低大版本 JRE（同原版 PCL 的自动 Java 下载）
        QVersionNumber minVer, maxVer;
        JavaManager::getJavaCompatibilityRange(version, minVer, maxVer);
        int major = minVer.isNull() ? 8
            : (minVer.majorVersion() == 1 ? minVer.minorVersion() : minVer.majorVersion());
        qWarning() << "未检测到 Java 运行时，自动下载 JRE" << major << "...";
        QString err;
        if (!installJavaRuntime(major, &err)) {
            qWarning() << "Java 自动下载失败:" << err;
            return false;
        }
        java = jm.selectJavaForVersion(version);
        if (java.pathJava.isEmpty()) java = jm.selectJava();
        if (java.pathJava.isEmpty()) return false;
    }

    // 启动预检：补齐缺失的游戏文件（对照 PCL DlClientFix）
    if (!ensureLaunchReady(version)) return false;
    // 中文输入修复：XIM 激活且 LWJGL 老（GLFW 3.3）时替换 GLFW 3.4
    maybeInstallGlfw34(version);

    // 注：如在同一进程中多次调用 launchVersion，信号会累积连接。
    // CLI 每次只启动一次游戏即退出，不受影响。
    if (onLog) {
        QObject::connect(&launcher, &Launcher::gameLog,
                         [onLog](const QString &line) { onLog(line); });
    }
    if (onExit) {
        QObject::connect(&launcher, &Launcher::gameExited,
                         [onExit](int code, const QString &) { onExit(code); });
        // FailedToStart 只发 launchFailed 不发 gameExited——必须转发，否则 CLI 挂死
        QObject::connect(&launcher, &Launcher::launchFailed,
                         [onExit](const QString &) { onExit(-1); });
    }

    return launcher.launch(version, java, login);
}

QStringList listJavas() {
    auto &jm = JavaManager::instance();
    jm.scanSystemJava();
    jm.waitForScanFinished();  // 扫描是异步的，读结果前必须等完成
    return jm.javaNames();
}

void importModpack(const QString &filePath,
                    const QString &instanceName,
                    const QString &targetInstance,
                    std::function<void(const ImportProgress &)> onProgress,
                    ImportCompleteCallback onComplete) {
    // mod 包（jar-only zip）缺目标实例：直接返回错误 + 当前实例列表
    if (targetInstance.isEmpty() && detectPackType(filePath) == PackType::Mod) {
        if (onComplete) onComplete(false, "此 zip 为 mod 包，需要加 --to <实例名>", listVersions());
        return;
    }
    // 适配：将 ImportProgress 结构体拆成 (status, percent) 传给内部实现
    auto adaptedProgress = [onProgress](const QString &status, int percent) {
        if (onProgress) onProgress({status, percent});
    };
    installModpack(filePath, instanceName, targetInstance, adaptedProgress,
        [onComplete](bool ok, const QString &msg) {
            if (onComplete) onComplete(ok, msg, {});
        });
}

bool removeInstance(const QString &name) {
    if (name.isEmpty() || name.contains('/') || name.contains('\\') || name.contains(".."))
        return false;
    // 用 VersionManager 的运行时目录（尊重 --folder 一次性覆盖），不直接读 Settings
    QString folder = VersionManager::instance().mcFolder();

    // 优先从 INI 映射查找随机目录名
    QString dirName = Settings::instance().dirForDisplayName(name);
    if (!dirName.isEmpty()) {
        QString instanceDir = folder + "instances/" + dirName;
        // 先删 INI 映射，再删目录（即使目录删除失败，映射也已清理）
        Settings::instance().removeInstanceDir(dirName);
        return QDir(instanceDir).removeRecursively();
    }

    // 回退：直接用显示名作为目录名（兼容旧格式或测试）
    QString instanceDir = folder + "instances/" + name;
    if (!QDir(instanceDir).exists()) return false;
    bool ok = QDir(instanceDir).removeRecursively();
    // 也尝试清理可能残留的 INI 映射
    Settings::instance().removeInstanceDir(name);
    return ok;
}

ConfigInfo getConfig() {
    ConfigInfo info;
    info.version       = GIT_DESCRIBE;
    info.commit        = GIT_COMMIT_HASH;
    info.gameFolder    = Settings::instance().getString("LaunchFolderSelect");
    info.gameFolderSet = !info.gameFolder.isEmpty();
    info.players       = listPlayers();
    info.selectedPlayer = Settings::instance().selectedPlayer();
    return info;
}

QList<PlayerEntry> listPlayers() {
    QList<PlayerEntry> result;
    QStringList uuids = Settings::instance().playerProfiles();
    for (const auto &uuid : uuids) {
        PlayerEntry e;
        e.uuid     = uuid;
        e.name     = Settings::instance().getProfile(uuid, "Name");
        e.avatar   = Settings::instance().getProfile(uuid, "Avatar");
        e.skinType = Settings::instance().getProfile(uuid, "SkinType", "slim");
        result.append(e);
    }
    return result;
}

PlayerEntry addPlayer(const QString &name, const QString &avatar, const QString &skinType,
                      const QString &customUuid) {
    QString uuid = customUuid;
    uuid.remove('{'); uuid.remove('}');
    if (uuid.isEmpty()) uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    Settings::instance().setProfile(uuid, "Name", name);
    Settings::instance().setProfile(uuid, "Avatar", avatar);
    Settings::instance().setProfile(uuid, "SkinType", skinType);

    // 首个玩家自动选中
    if (Settings::instance().playerProfiles().size() == 1)
        Settings::instance().selectPlayer(uuid);

    return {uuid, name, avatar, skinType};
}

bool updatePlayer(const QString &uuid, const QString &name, const QString &avatar,
                  const QString &skinType, const QString &newUuid) {
    auto &s = Settings::instance();
    if (!s.playerProfiles().contains(uuid)) return false;

    QString target = newUuid;
    target.remove('{'); target.remove('}');
    if (!target.isEmpty() && target != uuid && s.playerProfiles().contains(target))
        return false;  // 目标键已被占用，拒绝覆盖

    s.setProfile(uuid, "Name", name);
    s.setProfile(uuid, "Avatar", avatar);
    s.setProfile(uuid, "SkinType", skinType);

    // 改了配置键：整体迁移，并跟随选中态
    if (!target.isEmpty() && target != uuid) {
        s.setProfile(target, "Name", name);
        s.setProfile(target, "Avatar", avatar);
        s.setProfile(target, "SkinType", skinType);
        bool wasSelected = (s.selectedPlayer() == uuid);
        s.removeProfile(uuid);
        if (wasSelected) s.selectPlayer(target);
    }
    return true;
}

bool removePlayer(const QString &uuid) {
    if (uuid.isEmpty()) return false;
    if (!Settings::instance().playerProfiles().contains(uuid)) return false;
    Settings::instance().removeProfile(uuid);
    if (Settings::instance().selectedPlayer() == uuid)
        Settings::instance().selectPlayer(QString());
    return true;
}

bool selectPlayer(const QString &uuid) {
    if (!Settings::instance().playerProfiles().contains(uuid)) return false;
    Settings::instance().selectPlayer(uuid);
    return true;
}

} // namespace lpcl
