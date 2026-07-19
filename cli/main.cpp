// lpcl-cli — Minecraft launcher CLI frontend
// Links only liblpclcore (QtCore + QtNetwork, no GUI/QML)
//
// 维护指南：
//   新增命令只需三步：
//     1. 在 printHelp() 的 items[] 数组加一行
//     2. 写一个 static int handleXxx(const QStringList &args) 函数
//     3. 在 dispatchCommand() 中按是否需要 mcFolder 归类加入

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QPointer>
#include <QProcess>
#include <QTimer>
#include <QUuid>
#include <QLoggingCategory>
#include <iostream>

#include "lpcl.h"
#include "modpack.h"
#include "core/settings.h"
#include "core/versionmanager.h"
#include "core/javamanager.h"
#include "util/arg_utils.h"
#include "util/file_utils.h"
#include <QDir>

// ---- 中英文切换 ----

enum Lang { CN, EN };
static Lang g_lang = EN;

#define _(cn, en) (g_lang == CN ? cn : en)
static void setLang(bool en) { g_lang = en ? EN : CN; }
static auto T(const char *cn, const char *en) { return QString::fromUtf8(g_lang == CN ? cn : en); }

// ---- helpers ----

static QString extractFlag(QStringList &args, const QString &flag) {
    int idx = args.indexOf(flag);
    if (idx >= 0 && idx + 1 < args.size()) {
        QString value = args.at(idx + 1);
        args.removeAt(idx + 1);
        args.removeAt(idx);
        return value;
    }
    return {};
}
static QString extractFolder(QStringList &args) { return extractFlag(args, "--folder"); }
static QString extractRename(QStringList &args) { return extractFlag(args, "--r"); }

// ---- 命令处理函数 ----
// 每个函数签名: static int handleXxx(const QStringList &args)
// args[0] = 命令名, args[1..] = 参数
// 返回值 = 进程退出码

// ---- 无需 mcFolder 的命令 ----

static int handleSetFolder(const QStringList &args) {
    if (args.size() < 2) {
        std::cerr << _("error:  lpcl-cli set-folder <路径>\n",
                       "error:  lpcl-cli set-folder <path>\n");
        return 1;
    }
    Settings::instance().setString("LaunchFolderSelect", args[1]);
    std::cout << "success" << std::endl;
    return 0;
}

static int handleSetPlayer(const QStringList &args) {
    if (args.size() < 2) {
        std::cerr << T("error: 缺少参数\n", "error: missing argument\n").toStdString();
        return 1;
    }
    Settings::instance().setString("PlayerName", args[1]);
    std::cout << "success" << std::endl;
    return 0;
}

static int handleListJavas() {
    auto names = lpcl::listJavas();
    if (names.isEmpty()) {
        std::cout << _("(No Java detected)\n", "(No Java detected)\n");
    } else {
        std::cout << _("success\n", "success\n");
        for (const auto &name : names)
            std::cout << "  " << name.toStdString() << "\n";
    }
    return 0;
}

// ---- test 自检（CLI 层，逐条验证命令） ----

// ---- 冒烟测试辅助（合成包，几 KB，不依赖本机大文件） ----

struct ImportResult { bool success = false; bool done = false; QString message; };

// 同步等待一次导入完成（带超时；回调捕获 QSharedPointer/QPointer，超时后回调不再访问栈对象）
static ImportResult importAndWait(const QString &filePath, const QString &name, int timeoutMs = 600000) {
    auto state = QSharedPointer<ImportResult>::create();
    QEventLoop loop;
    QPointer<QEventLoop> loopGuard = &loop;
    lpcl::importModpack(filePath, name,
        [](const lpcl::ImportProgress &) {},
        [state, loopGuard](bool ok, const QString &msg) {
            state->success = ok; state->message = msg; state->done = true;
            if (loopGuard) loopGuard->quit();
        });
    QTimer::singleShot(timeoutMs, &loop, &QEventLoop::quit);
    if (!state->done) loop.exec();  // 同步失败路径下 quit 先于 exec，不能裸等
    if (!state->done) { state->message = "import timeout"; }
    return *state;
}

// 生成最小 CurseForge 整合包（1 个真实小 mod JEI；badMod=true 时附加一个必失败的 mod；
// forgeVer 可用于构造必失败的 modloader 版本）
static QString makeSyntheticCfPack(const QString &workDir, bool badMod,
                                   const QString &forgeVer = "47.4.10") {
    QDir().mkpath(workDir + "/overrides");
    QFile mf(workDir + "/manifest.json");
    if (!mf.open(QIODevice::WriteOnly)) return {};
    QByteArray files = R"({"projectID": 238222, "fileID": 8419086, "required": true})";
    if (badMod)
        files += R"(, {"projectID": 999999999, "fileID": 1, "required": true})";
    mf.write(QByteArray(R"({
  "minecraft": {"version": "1.20.1", "modLoaders": [{"id": "forge-)" + forgeVer.toUtf8() + R"(", "primary": true}]},
  "manifestType": "minecraftModpack",
  "manifestVersion": 1,
  "name": "LPCLSmokeCF",
  "version": "1.0",
  "author": "lpcl-test",
  "overrides": "overrides",
  "files": [)" + files + "]\n}"));
    mf.close();
    QFile f(workDir + "/overrides/keepme.txt");
    if (f.open(QIODevice::WriteOnly)) { f.write("test"); f.close(); }

    QString zipPath = workDir + "/pack.zip";
    QProcess zip;
    zip.setWorkingDirectory(workDir);
    zip.start("zip", {"-qr", zipPath, "."});
    zip.waitForFinished(15000);
    return zip.exitCode() == 0 ? zipPath : QString();
}

// 生成最小 HMCL 整合包（1.12.2，验证旧格式下载链）
static QString makeSyntheticHmclPack(const QString &workDir) {
    QDir().mkpath(workDir);
    QFile mf(workDir + "/modpack.json");
    if (!mf.open(QIODevice::WriteOnly)) return {};
    mf.write(R"({"name": "LPCLSmoke112", "gameVersion": "1.12.2", "version": "1.0"})");
    mf.close();

    QString zipPath = workDir + "/pack.zip";
    QProcess zip;
    zip.setWorkingDirectory(workDir);
    zip.start("zip", {"-qr", zipPath, "modpack.json"});
    zip.waitForFinished(15000);
    return zip.exitCode() == 0 ? zipPath : QString();
}

struct TestItem {
    QString cmd;
    QString status;  // OK / WARN / FAIL / SKIP
    QString detail;
};

static QList<TestItem> runCommandTests() {
    QList<TestItem> results;
    auto ok  = [&](const QString &cmd, const QString &detail)
        { results.append({cmd, "OK", detail}); };
    auto warn= [&](const QString &cmd, const QString &detail)
        { results.append({cmd, "WARN", detail}); };
    auto fail= [&](const QString &cmd, const QString &detail)
        { results.append({cmd, "FAIL", detail}); };
    auto skip= [&](const QString &cmd, const QString &detail)
        { results.append({cmd, "SKIP", detail}); };

    // --config
    {
        auto cfg = lpcl::getConfig();
        if (!cfg.version.isEmpty() && !cfg.commit.isEmpty())
            ok("--config", QString("v%1 (%2)").arg(cfg.version, cfg.commit));
        else
            fail("--config", "版本信息缺失");
    }

    // set-folder（写入 → 读回 → 还原）
    {
        QString orig = Settings::instance().getString("LaunchFolderSelect");
        const QString testPath = "/tmp/_lpcl_test_mc_/";
        Settings::instance().setString("LaunchFolderSelect", testPath);
        QString readBack = Settings::instance().getString("LaunchFolderSelect");
        Settings::instance().setString("LaunchFolderSelect", orig);
        if (readBack == testPath)
            ok("set-folder", "写入/读取/还原 正常");
        else
            fail("set-folder", QString("写入 '%1' 读回 '%2'").arg(testPath, readBack));
    }

    // set-player
    {
        const QString testName = "_lpcl_test_player_";
        Settings::instance().setString("PlayerName", testName);
        QString readBack = Settings::instance().getString("PlayerName");
        Settings::instance().setString("PlayerName", QString());
        if (readBack == testName)
            ok("set-player", "写入/读取/清除 正常");
        else
            fail("set-player", QString("写入 '%1' 读回 '%2'").arg(testName, readBack));
    }

    // player-add → list → select → rm 全链路
    QString testUuid;
    {
        auto entry = lpcl::addPlayer("_lpcl_test_");
        if (!entry.uuid.isEmpty() && entry.name == "_lpcl_test_") {
            ok("player-add", QString("创建 (uuid: %1)").arg(entry.uuid));
            testUuid = entry.uuid;
        } else {
            fail("player-add", "创建失败");
        }
    }

    if (!testUuid.isEmpty()) {
        auto players = lpcl::listPlayers();
        bool found = false;
        for (const auto &p : players)
            if (p.uuid == testUuid) { found = true; break; }
        if (found)
            ok("player-list", QString("共 %1 个玩家，测试玩家已找到").arg(players.size()));
        else
            fail("player-list", "测试玩家未出现在列表中");

        QString before = Settings::instance().selectedPlayer();
        lpcl::selectPlayer(testUuid);
        QString after = Settings::instance().selectedPlayer();
        lpcl::selectPlayer(before);
        if (after == testUuid)
            ok("player-select", "选中/还原 正常");
        else
            fail("player-select", "选中失败");

        lpcl::removePlayer(testUuid);
        players = lpcl::listPlayers();
        bool gone = true;
        for (const auto &p : players)
            if (p.uuid == testUuid) { gone = false; break; }
        if (gone)
            ok("player-rm", "删除成功");
        else
            fail("player-rm", "删除后仍在列表中");
    } else {
        skip("player-list",   "player-add 失败，跳过");
        skip("player-select", "player-add 失败，跳过");
        skip("player-rm",     "player-add 失败，跳过");
    }

    // list-javas
    {
        auto names = lpcl::listJavas();
        if (!names.isEmpty())
            ok("list-javas", QString("检测到 %1 个 Java: %2").arg(names.size()).arg(names.join(", ")));
        else
            warn("list-javas", "未检测到 Java 运行时");
    }

    // list（实例 — INI 映射）
    {
        QString folder = Settings::instance().getString("LaunchFolderSelect");
        if (!folder.isEmpty()) {
            VersionManager::instance().setMcFolder(folder);
            auto ids = lpcl::listVersions();
            if (ids.isEmpty())
                ok("list", "目录已设置，无导入实例");
            else
                ok("list", QString("检测到 %1 个实例: %2").arg(ids.size()).arg(ids.join(", ")));
        } else {
            warn("list", "游戏目录未设置，跳过（请先 set-folder）");
        }
    }

    // mc-list（原版 MC — versions/ 目录）
    {
        QString folder = Settings::instance().getString("LaunchFolderSelect");
        if (!folder.isEmpty()) {
            VersionManager::instance().setMcFolder(folder);
            auto ids = lpcl::listMcVersions();
            if (ids.isEmpty())
                ok("mc-list", "无原版 MC 版本");
            else
                ok("mc-list", QString("检测到 %1 个版本: %2").arg(ids.size()).arg(ids.join(", ")));
        } else {
            warn("mc-list", "游戏目录未设置，跳过");
        }
    }

    // launch
    {
        QString folder = Settings::instance().getString("LaunchFolderSelect");
        if (!folder.isEmpty()) {
            VersionManager::instance().setMcFolder(folder);
            VersionManager::instance().loadLocalVersions();
            auto ids = VersionManager::instance().versionIds();
            if (!ids.isEmpty())
                skip("launch", QString("实例 '%1' 存在但跳过（需 GUI 环境）").arg(ids.first()));
            else
                skip("launch", "无可用实例，跳过");
        } else {
            skip("launch", "游戏目录未设置，跳过");
        }
    }

    // ---- 纯函数单测（无网络） ----

    // mavenNameToPath：maven 坐标 → 仓库路径
    {
        struct Case { const char *in; const char *expect; };
        const Case cases[] = {
            {"org.lwjgl:lwjgl:3.2.2", "org/lwjgl/lwjgl/3.2.2/lwjgl-3.2.2.jar"},
            {"org.lwjgl:lwjgl-glfw:3.3.1:natives-linux",
             "org/lwjgl/lwjgl-glfw/3.3.1/lwjgl-glfw-3.3.1-natives-linux.jar"},
            {"net.minecraftforge:forge:1.12.2-14.23.5.2860:universal@zip",
             "net/minecraftforge/forge/1.12.2-14.23.5.2860/forge-1.12.2-14.23.5.2860-universal.zip"},
        };
        int bad = 0;
        for (const auto &c : cases)
            if (FileUtils::mavenNameToPath(c.in) != c.expect) bad++;
        if (bad == 0) ok("mavenNameToPath", "3 例通过");
        else fail("mavenNameToPath", QString("%1/3 例失败").arg(bad));
    }

    // deduplicateArgs：--add-opens 可重复保留、-Xmx 后者覆盖
    {
        QStringList in = {"--add-opens", "java.base/java.util=ALL-UNNAMED",
                          "--add-opens", "java.base/java.lang=ALL-UNNAMED",
                          "-Xmx1G", "-Xmx2G"};
        auto out = ArgUtils::deduplicateArgs(in);
        int opens = out.count("--add-opens");
        int xmxCount = 0;
        for (const auto &a : out) if (a.startsWith("-Xmx")) xmxCount++;
        if (opens == 2 && xmxCount == 1 && out.contains("-Xmx2G"))
            ok("deduplicateArgs", "可重复 flag 保留 + -Xmx 去重 正常");
        else
            fail("deduplicateArgs", "输出异常: " + out.join(" "));
    }

    // ---- 合成包冒烟（inpack 端到端，需要网络与游戏目录） ----
    {
        QString folder = Settings::instance().getString("LaunchFolderSelect");
        if (folder.isEmpty()) {
            warn("inpack-smoke", "游戏目录未设置，跳过合成包冒烟");
        } else {
            VersionManager::instance().setMcFolder(folder);
            QString base = QDir::temp().filePath("_lpcl_smoke_"
                + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8));

            QString cfOkZip  = makeSyntheticCfPack(base + "/cf_ok", false);
            QString cfBadZip = makeSyntheticCfPack(base + "/cf_bad", true);
            QString hmclZip  = makeSyntheticHmclPack(base + "/hmcl");

            // detectPackType：合成包类型识别
            if (!cfOkZip.isEmpty() && !hmclZip.isEmpty()) {
                bool typeOk = detectPackType(cfOkZip) == PackType::CurseForge
                           && detectPackType(hmclZip) == PackType::HMCL;
                if (typeOk) ok("detectPackType", "CF/HMCL 合成包识别正确");
                else fail("detectPackType", "类型识别错误");
            } else {
                fail("detectPackType", "合成测试包失败（zip 命令不可用？）");
            }

            // CF 成功路：1 个真实 mod（JEI）→ 导入成功 + 实例可见
            const QString cfName = "__lpcl_smoke_cf__";
            lpcl::removeInstance(cfName);
            if (!cfOkZip.isEmpty()) {
                auto r = importAndWait(cfOkZip, cfName);
                if (r.success && lpcl::listVersions().contains(cfName))
                    ok("inpack-cf", "CF 合成包导入成功（mod 下载链路正常）");
                else
                    fail("inpack-cf", "导入失败: " + r.message);
                lpcl::removeInstance(cfName);
            } else {
                skip("inpack-cf", "合成包失败");
            }

            // CF 回滚路：假 mod → 必须导入失败且实例无残留
            const QString badName = "__lpcl_smoke_bad__";
            lpcl::removeInstance(badName);
            if (!cfBadZip.isEmpty()) {
                auto r = importAndWait(cfBadZip, badName);
                bool listed = lpcl::listVersions().contains(badName);
                if (!r.success && !listed)
                    ok("inpack-rollback", "mod 失败 → 导入失败且无残留");
                else
                    fail("inpack-rollback",
                         QString("预期失败且无残留，实际 success=%1 listed=%2")
                             .arg(r.success).arg(listed));
                lpcl::removeInstance(badName);
            } else {
                skip("inpack-rollback", "合成包失败");
            }

            // modloader 回滚路：假 Forge 版本 → 安装必须失败且无残留
            const QString loaderName = "__lpcl_smoke_loader__";
            lpcl::removeInstance(loaderName);
            QString cfLoaderZip = makeSyntheticCfPack(base + "/cf_loader", false, "99.99.99");
            if (!cfLoaderZip.isEmpty()) {
                auto r = importAndWait(cfLoaderZip, loaderName);
                bool listed = lpcl::listVersions().contains(loaderName);
                if (!r.success && !listed)
                    ok("inpack-loader-rollback", "modloader 失败 → 导入失败且无残留");
                else
                    fail("inpack-loader-rollback",
                         QString("预期失败且无残留，实际 success=%1 listed=%2")
                             .arg(r.success).arg(listed));
                lpcl::removeInstance(loaderName);
            } else {
                skip("inpack-loader-rollback", "合成包失败");
            }

            // HMCL 1.12.2：旧格式库下载 + 老格式 natives 解压
            const QString hmclName = "__lpcl_smoke_112__";
            lpcl::removeInstance(hmclName);
            if (!hmclZip.isEmpty()) {
                auto r = importAndWait(hmclZip, hmclName);
                if (!folder.endsWith('/')) folder += '/';
                bool verOk = QFile::exists(folder + "versions/1.12.2/1.12.2.json")
                          && QFile::exists(folder + "versions/1.12.2/1.12.2.jar");
                bool nativesOk = !QDir(folder + "versions/1.12.2/natives")
                                      .entryList(QDir::Files | QDir::NoDotAndDotDot).isEmpty();
                if (r.success && verOk && nativesOk)
                    ok("inpack-112", "1.12.2 旧格式下载 + natives 解压正常");
                else
                    fail("inpack-112", QString("success=%1 verOk=%2 nativesOk=%3 (%4)")
                                         .arg(r.success).arg(verOk).arg(nativesOk).arg(r.message));
                lpcl::removeInstance(hmclName);
            } else {
                skip("inpack-112", "合成包失败");
            }

            QDir(base).removeRecursively();
        }
    }

    // rm（创建随机目录 + INI 映射 → removeInstance → 验证删除）
    {
        QString folder = Settings::instance().getString("LaunchFolderSelect");
        if (!folder.isEmpty()) {
            if (!folder.endsWith('/')) folder += '/';
            const QString testName = "_lpcl_test_rm_";
            // 用随机目录名模拟新存储模型（instances/ 子目录）
            QString randomDir = "test_" + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
            QString testDir = folder + "instances/" + randomDir + "/";
            QDir().mkpath(testDir);
            QDir(testDir + "PCL/").mkpath(".");
            // 写入 INI 映射
            Settings::instance().setInstanceDir(randomDir, testName);
            if (QDir(testDir).exists()) {
                bool removed = lpcl::removeInstance(testName);
                bool dirGone = !QDir(testDir).exists();
                bool mappingGone = Settings::instance().dirForDisplayName(testName).isEmpty();
                if (removed && dirGone && mappingGone)
                    ok("rm", "映射删除/目录清理 正常");
                else if (!mappingGone)
                    fail("rm", "映射残留");
                else
                    fail("rm", "删除失败或目录残留");
                // 兜底清理
                if (!dirGone) QDir(testDir).removeRecursively();
            } else {
                fail("rm", "无法创建测试目录");
            }
        } else {
            skip("rm", "游戏目录未设置，跳过");
        }
    }

    return results;
}

static int handleTest() {
    std::cout << _("=== LPCL 全系统自检 ===\n", "=== LPCL System Self-Test ===\n") << std::endl;

    auto results = runCommandTests();
    for (const auto &r : results) {
        QString tag;
        if (r.status == "OK")      tag = "\033[32m[ OK ]\033[0m";
        else if (r.status == "WARN")   tag = "\033[33m[WARN]\033[0m";
        else if (r.status == "FAIL")   tag = "\033[31m[FAIL]\033[0m";
        else if (r.status == "SKIP")   tag = "\033[90m[SKIP]\033[0m";
        else                      tag = "[INFO]";

        std::cout << "  " << tag.toStdString() << " "
                  << QString("%1").arg(r.cmd, -22).toStdString()
                  << r.detail.toStdString() << std::endl;
    }
    return 0;
}

// ---- 玩家 Profile 命令 ----

static int handlePlayerAdd(const QStringList &args) {
    if (args.size() < 2) {
        std::cerr << _("error:  lpcl-cli player-add <名称> [--avatar <路径>]\n",
                       "error:  lpcl-cli player-add <name> [--avatar <path>]\n");
        return 1;
    }
    // 检查 --avatar 标志
    QString avatar;
    for (int i = 1; i < args.size(); ++i) {
        if (args[i] == "--avatar" && i + 1 < args.size()) {
            avatar = args[i + 1];
            break;
        }
    }
    auto entry = lpcl::addPlayer(args[1], avatar);
    std::cout << _("已添加玩家:\n", "Player added:\n")
              << "  UUID: " << entry.uuid.toStdString() << "\n"
              << "  " << _("名称: ", "Name: ") << entry.name.toStdString() << "\n";
    if (!entry.avatar.isEmpty())
        std::cout << "  " << _("头像: ", "Avatar: ") << entry.avatar.toStdString() << "\n";
    return 0;
}

static int handlePlayerRm(const QStringList &args) {
    if (args.size() < 2) {
        std::cerr << _("error:  lpcl-cli player-rm <uuid>\n",
                       "error:  lpcl-cli player-rm <uuid>\n");
        return 1;
    }
    if (lpcl::removePlayer(args[1])) {
        std::cout << "success" << std::endl;
        return 0;
    }
    std::cerr << _("error: UUID 不存在\n", "error: UUID not found\n");
    return 1;
}

static int handlePlayerList() {
    auto players = lpcl::listPlayers();
    if (players.isEmpty()) {
        std::cout << _("（无玩家配置）\n", "(No player profiles)\n");
        return 0;
    }
    QString selected = Settings::instance().selectedPlayer();
    for (const auto &p : players) {
        bool isSel = (p.uuid == selected);
        std::cout << (isSel ? " * " : "   ")
                  << p.uuid.toStdString() << "\n"
                  << "     " << _("名称: ", "Name: ") << p.name.toStdString() << "\n";
        if (!p.avatar.isEmpty())
            std::cout << "     " << _("头像: ", "Avatar: ") << p.avatar.toStdString() << "\n";
        std::cout << "     Skin: " << p.skinType.toStdString() << "\n";
    }
    return 0;
}

static int handlePlayerSelect(const QStringList &args) {
    if (args.size() < 2) {
        std::cerr << _("error:  lpcl-cli player-select <uuid>\n",
                       "error:  lpcl-cli player-select <uuid>\n");
        return 1;
    }
    if (!lpcl::selectPlayer(args[1])) {
        std::cerr << _("error:  玩家 UUID 不存在\n", "error:  player UUID not found\n");
        return 1;
    }
    std::cout << "success" << std::endl;
    return 0;
}

// ---- 需要 mcFolder 的命令 ----

static int handleList() {
    auto ids = lpcl::listVersions();
    if (ids.isEmpty()) {
        std::cout << T("(No instances)\n", "(No instances)\n").toStdString();
    } else {
        std::cout << T("Instances:\n", "Instances:\n").toStdString();
        for (const auto &id : ids)
            std::cout << "  " << id.toStdString() << "\n";
    }
    return 0;
}

static int handleMcList() {
    auto ids = lpcl::listMcVersions();
    if (ids.isEmpty()) {
        std::cout << T("(No vanilla MC versions)\n",
                       "(No vanilla MC versions)\n").toStdString();
    } else {
        std::cout << T("Vanilla MC versions:\n",
                       "Vanilla MC versions:\n").toStdString();
        for (const auto &id : ids)
            std::cout << "  " << id.toStdString() << "\n";
    }
    return 0;
}

static int handleLaunch(const QStringList &args) {
    if (args.size() < 2) {
        std::cerr << T("error: lpcl-cli launch <名称>\n",
                       "error: lpcl-cli launch <name>\n").toStdString();
        return 1;
    }
    std::cout << _(QString("正在启动 %1 ...\n").arg(args[1]).toStdString(),
                   QString("Launching %1 ...\n").arg(args[1]).toStdString());
    if (!lpcl::launchVersion(args[1],
            [](const QString &line) { std::cout << "[MC] " << line.toStdString() << std::endl; },
            [](int code) {
                std::cout << _("exit: ", "exit: ") << code << "\n";
                QCoreApplication::quit();
            })) {
        std::cerr << T("error: launch failed\n", "error: launch failed\n").toStdString();
        return 1;
    }
    std::cout << "success" << std::endl;
    QCoreApplication::instance()->exec(); // 等待游戏进程结束
    return 0;
}

static int handleInpack(QStringList &args) {
    if (args.size() < 2) {
        std::cerr << _("error:  lpcl-cli inpack <文件> [--r <名称>] [--folder <路径>]\n",
                       "error:  lpcl-cli inpack <file> [--r <name>] [--folder <path>]\n");
        return 1;
    }
    QString rename = extractRename(args);
    if (args.size() < 2) {  // --r/--folder 移除后可能没有文件参数
        std::cerr << _("error:  lpcl-cli inpack <文件> [--r <名称>] [--folder <路径>]\n",
                       "error:  lpcl-cli inpack <file> [--r <name>] [--folder <path>]\n");
        return 1;
    }
    std::cout << _("正在导入整合包...\n", "Importing modpack...\n");

    bool done = false;
    int  result = 1;
    lpcl::importModpack(args[1], rename,
        [](const lpcl::ImportProgress &p) {
            int bars = p.percent / 5;
            std::cout << "\r  [";
            for (int i = 0; i < 20; ++i)
                std::cout << (i < bars ? "=" : i == bars ? ">" : " ");
            std::cout << "] " << p.percent << "% " << p.step.toStdString();
            if (p.percent >= 100) std::cout << std::endl;
            std::cout.flush();
        },
        [&](bool ok, const QString &msg) {
            if (ok) {
                std::cout << std::endl << _("success: ", "success: ")
                          << msg.toStdString() << std::endl;
                result = 0;
            } else {
                std::cerr << std::endl << _("error: ", "error: ")
                          << msg.toStdString() << std::endl;
            }
            done = true;
            QCoreApplication::quit();
        });

    if (!done) QCoreApplication::instance()->exec(); // 等待异步下载完成
    return result;
}

static int handleRm(const QStringList &args) {
    if (args.size() < 2) {
        std::cerr << _("error:  lpcl-cli rm <名称|*>\n",
                       "error:  lpcl-cli rm <name|*>\n");
        return 1;
    }
    if (args[1] == "*") {
        auto ids = lpcl::listVersions();
        if (ids.isEmpty()) {
            std::cout << _("没有可删除的实例\n", "No instances to remove\n");
            return 0;
        }
        int removed = 0;
        for (const auto &id : ids) {
            if (lpcl::removeInstance(id)) removed++;
        }
        std::cout << _(QString("已删除 %1 个实例\n").arg(removed).toStdString(),
                       QString("Removed %1 instance(s)\n").arg(removed).toStdString());
        return 0;
    }
    if (lpcl::removeInstance(args[1])) {
        std::cout << "success" << std::endl;
        return 0;
    }
    std::cerr << _("error: 实例不存在或删除失败\n",
                   "error: instance not found or removal failed\n");
    return 1;
}

// ---- 命令派发 ----

/// 解析 mcFolder 并派发到对应处理函数。
/// 返回值 >= 0 表示已处理（退出码），-1 表示未知命令。
static int dispatchCommand(const QString &cmd, QStringList &args) {
    // ---- 组 A: 无需 mcFolder ----
    if (cmd == "set-folder")     return handleSetFolder(args);
    if (cmd == "set-player")     return handleSetPlayer(args);
    if (cmd == "list-javas")     return handleListJavas();
    if (cmd == "player-add")     return handlePlayerAdd(args);
    if (cmd == "player-rm")      return handlePlayerRm(args);
    if (cmd == "player-list")    return handlePlayerList();
    if (cmd == "player-select")  return handlePlayerSelect(args);
    if (cmd == "test")           return handleTest();

    // ---- 组 B: 需要 mcFolder ----
    // 解析游戏目录：inpack 的 --folder 优先，否则从 Settings 读
    QString mcFolder;
    QString folderArg = extractFolder(args);
    if (!folderArg.isEmpty())
        mcFolder = folderArg;
    else
        mcFolder = Settings::instance().getString("LaunchFolderSelect");

    if (mcFolder.isEmpty()) {
        std::cerr << T("error: 未设置游戏目录，请先执行 set-folder\n",
                       "error: no game folder set, run set-folder first\n").toStdString();
        return 1;
    }
    // --folder 是一次性覆盖，不写回配置（持久化只走 set-folder 命令）
    VersionManager::instance().setMcFolder(mcFolder, folderArg.isEmpty());

    if (cmd == "list")     return handleList();
    if (cmd == "mc-list")  return handleMcList();
    if (cmd == "launch")   return handleLaunch(args);
    if (cmd == "inpack")  return handleInpack(args);
    if (cmd == "rm")      return handleRm(args);

    return -1; // unknown
}

// ---- main ----

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    app.setApplicationName("lpcl-cli");
    app.setApplicationVersion("0.1");

    // 默认静默，关闭 SDK 调试日志
    QLoggingCategory::setFilterRules("lpcl.*.info=false\nlpcl.*.debug=false");
    for (int i = 1; i < argc; ++i) {
        if (QString::fromLatin1(argv[i]) == "--cn") setLang(false);
        else if (QString::fromLatin1(argv[i]) == "--en") setLang(true);
    }

    // ---- 选项解析 ----
    QCommandLineParser parser;
    parser.setApplicationDescription(
        _("LPCL 命令行启动器", "LPCL Command-Line Launcher"));

    QCommandLineOption optEn("en",
        _("使用英文输出（默认）", "Use English output (default)"));
    QCommandLineOption optCn("cn",
        _("使用中文输出", "Use Chinese output"));
    QCommandLineOption optConfig("config",
        _("查看当前配置", "Show current configuration"));
    QCommandLineOption optHelp("help", _("显示帮助信息", "Show help information"));
    QCommandLineOption optVersion("version", _("显示版本号", "Show version number"));
    parser.addOption(optCn);
    parser.addOption(optEn);
    parser.addOption(optConfig);
    parser.addOption(optHelp);
    parser.addOption(optVersion);
    parser.addPositionalArgument("command", "placeholder");
    parser.setOptionsAfterPositionalArgumentsMode(
        QCommandLineParser::ParseAsPositionalArguments);
    parser.process(app);

    // ---- printHelp ----
    auto printHelp = [&]() {
        auto out = [](const QString &s) { std::cout << s.toStdString(); };
        struct Item { QString cmdCn, cmdEn; const char *descCn, *descEn; };
        const Item items[] = {
            {"list",              "list",              "列出已导入的整合包实例",  "List imported instances"},
            {"mc-list",           "mc-list",           "列出原版 MC 版本",       "List vanilla MC versions"},
            {"launch <名称>",     "launch <name>",     "启动整合包游戏",       "Launch a modpack"},
            {"list-javas",        "list-javas",        "列出可用 Java",        "List available Java runtimes"},
            {"set-folder <路径>", "set-folder <path>", "设置默认游戏目录",     "Set default Minecraft folder"},
            {"set-player <名称>", "set-player <name>", "设置玩家名称",         "Set player name"},
            {"inpack <文件> [--r <名称>]", "inpack <file> [--r <name>]", "导入整合包", "Import modpack"},
            {"rm <名称|*>",       "rm <name|*>",       "删除实例（* 清空全部）", "Remove instance (* for all)"},
            {"player-add <名称>","player-add <name>",  "添加玩家配置",          "Add player profile"},
            {"player-rm <uuid>", "player-rm <uuid>",  "删除玩家配置",          "Remove player profile"},
            {"player-list",      "player-list",       "列出玩家配置",          "List player profiles"},
            {"player-select <uuid>","player-select <uuid>","选择当前玩家",     "Select current player"},
            {"test",              "test",              "全系统自检",            "Run system self-test"},
        };
        const Item opts[] = {
            {"--cn",      "--cn",      "使用中文输出",          "Use Chinese output"},
            {"--en",      "--en",      "使用英文输出（默认）",   "Use English output (default)"},
            {"--config",  "--config",  "查看当前配置",          "Show current configuration"},
            {"--help",    "--help",    "显示帮助信息",          "Show help information"},
            {"--version", "--version", "显示版本号",            "Show version number"},
        };
        auto printItem = [&](const Item &it) {
            out(QString("  %1 %2\n").arg(g_lang == CN ? it.cmdCn : it.cmdEn, -20)
                    .arg(T(it.descCn, it.descEn)));
        };
        out(T("Usage: lpcl-cli [options] <command> [args]\n",
              "Usage: lpcl-cli [options] <command> [args]\n"));
        out("\n"); out(T("命令 / Commands:\n", "Commands:\n"));
        for (const auto &it : items) printItem(it);
        out("\n"); out(T("选项 / Options:\n", "Options:\n"));
        for (const auto &it : opts) printItem(it);
        std::cout.flush();
    };

    // ---- 纯选项（无命令） ----
    if (parser.isSet(optHelp))  { printHelp(); return 0; }
    if (parser.isSet(optVersion)) {
        std::cout << app.applicationName().toStdString() << " "
                  << app.applicationVersion().toStdString() << std::endl;
        return 0;
    }
    if (parser.isSet(optConfig)) {
        Settings::initialize();
        auto cfg = lpcl::getConfig();

        std::cout << _("LPCL 版本: ", "LPCL version: ") << cfg.version.toStdString() << std::endl
                  << _("提交: ",       "Commit: ")       << cfg.commit.toStdString() << std::endl;
        QString folder = cfg.gameFolderSet ? cfg.gameFolder : _("（未设置）", "(not set)");
        std::cout << _("默认游戏目录: ", "Default game folder: ") << folder.toStdString() << std::endl;

        if (cfg.players.isEmpty()) {
            std::cout << _("玩家配置: （无）\n", "Player profiles: (none)\n");
        } else {
            std::cout << _("玩家配置:", "Player profiles:") << std::endl;
            for (const auto &p : cfg.players) {
                bool isSel = (p.uuid == cfg.selectedPlayer);
                std::cout << (isSel ? "  * " : "    ")
                          << p.uuid.toStdString()
                          << "  " << p.name.toStdString();
                if (!p.avatar.isEmpty())
                    std::cout << "  avatar=" << p.avatar.toStdString();
                std::cout << "  skin=" << p.skinType.toStdString();
                if (isSel) std::cout << "  [" << _("当前", "active") << "]";
                std::cout << std::endl;
            }
        }

        // 实例映射表
        auto instMap = Settings::instance().instanceDirs();
        if (instMap.isEmpty()) {
            std::cout << _("实例映射: （无）\n", "Instance mappings: (none)\n");
        } else {
            std::cout << _("实例映射:", "Instance mappings:") << std::endl;
            for (auto it = instMap.begin(); it != instMap.end(); ++it) {
                std::cout << "  " << it.key().toStdString()
                          << " → " << it.value().toStdString() << std::endl;
            }
        }
        return 0;
    }

    // ---- 命令派发 ----
    QStringList args = parser.positionalArguments();
    if (args.isEmpty()) { printHelp(); return 1; }

    Settings::initialize();
    int ret = dispatchCommand(args.at(0), args);
    if (ret < 0) {
        std::cerr << _(QString("未知命令: %1\n").arg(args.at(0)).toStdString(),
                       QString("error: unknown command: %1\n").arg(args.at(0)).toStdString());
        printHelp();
        return 1;
    }
    return ret;
}
