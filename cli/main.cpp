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
#include <QLoggingCategory>
#include <iostream>

#include "i18n.h"
#include "test.h"
#include "lpcl.h"
#include "core/settings.h"
#include "core/versionmanager.h"

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

static int handleSetLang(const QStringList &args) {
    if (args.size() < 2 || (args[1] != "en" && args[1] != "zh")) {
        std::cerr << _("error:  lpcl-cli set-lang <en|zh>\n",
                       "error:  lpcl-cli set-lang <en|zh>\n");
        return 1;
    }
    // 持久保存 + 立即生效
    Settings::instance().setString("UiLanguage", args[1]);
    setLang(args[1] == "en");
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

// ---- 玩家 Profile 命令 ----

static int handlePlayerAdd(QStringList &args) {
    if (args.size() < 2) {
        std::cerr << _("error:  lpcl-cli player-add <名称> [--avatar <路径>] [--skin <slim|wide|default>]\n",
                       "error:  lpcl-cli player-add <name> [--avatar <path>] [--skin <slim|wide|default>]\n");
        return 1;
    }
    QString avatar = extractFlag(args, "--avatar");
    QString skin = extractFlag(args, "--skin");
    if (args.size() < 2) {
        std::cerr << _("error: 缺少玩家名称\n", "error: missing player name\n");
        return 1;
    }
    if (!skin.isEmpty() && skin != "slim" && skin != "wide" && skin != "default") {
        std::cerr << _("error: 皮肤类型必须是 slim / wide / default\n",
                       "error: skin type must be slim / wide / default\n");
        return 1;
    }
    auto entry = lpcl::addPlayer(args[1], avatar, skin.isEmpty() ? "slim" : skin);
    std::cout << _("已添加玩家:\n", "Player added:\n")
              << "  UUID: " << entry.uuid.toStdString() << "\n"
              << "  " << _("名称: ", "Name: ") << entry.name.toStdString() << "\n"
              << "  Skin: " << entry.skinType.toStdString() << "\n";
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
    QString target;
    if (args.size() < 2) {
        // 未指定实例：列出实例列表，用户按序号选择
        auto ids = lpcl::listVersions();
        if (ids.isEmpty()) {
            std::cerr << _("error:  没有实例，请先导入整合包\n",
                           "error:  no instances, import a modpack first\n");
            return 1;
        }
        std::cout << _("选择要启动的实例:\n", "Select an instance to launch:\n");
        for (int i = 0; i < ids.size(); ++i)
            std::cout << "  " << (i + 1) << ") " << ids[i].toStdString() << "\n";
        std::cout << _("请输入序号: ", "Enter number: ");
        std::cout.flush();
        std::string line;
        std::getline(std::cin, line);
        bool okNum = false;
        int choice = QString::fromStdString(line).toInt(&okNum);
        if (!okNum || choice < 1 || choice > ids.size()) {
            std::cerr << _("error:  无效的选择\n", "error:  invalid choice\n");
            return 1;
        }
        target = ids[choice - 1];
    } else {
        target = args[1];
    }
    std::cout << _(QString("正在启动 %1 ...\n").arg(target).toStdString(),
                   QString("Launching %1 ...\n").arg(target).toStdString());
    if (!lpcl::launchVersion(target,
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
        std::cerr << _("error:  lpcl-cli inpack <文件> [--r <名称>] [--to <实例>] [--folder <路径>]\n",
                       "error:  lpcl-cli inpack <file> [--r <name>] [--to <instance>] [--folder <path>]\n");
        return 1;
    }
    QString rename = extractRename(args);
    QString to = extractFlag(args, "--to");
    if (args.size() < 2) {  // --r/--to/--folder 移除后可能没有文件参数
        std::cerr << _("error:  lpcl-cli inpack <文件> [--r <名称>] [--to <实例>] [--folder <路径>]\n",
                       "error:  lpcl-cli inpack <file> [--r <name>] [--to <instance>] [--folder <path>]\n");
        return 1;
    }
    std::cout << _("正在导入整合包...\n", "Importing modpack...\n");

    bool done = false;
    int  result = 1;
    lpcl::importModpack(args[1], rename, to,
        [](const lpcl::ImportProgress &p) {
            int bars = p.percent / 5;
            std::cout << "\r  [";
            for (int i = 0; i < 20; ++i)
                std::cout << (i < bars ? "=" : i == bars ? ">" : " ");
            std::cout << "] " << p.percent << "% " << p.step.toStdString();
            if (p.percent >= 100) std::cout << std::endl;
            std::cout.flush();
        },
        [&](bool ok, const QString &msg, const QStringList &data) {
            if (ok) {
                std::cout << std::endl << _("success: ", "success: ")
                          << msg.toStdString() << std::endl;
                result = 0;
            } else {
                std::cerr << std::endl << _("error: ", "error: ")
                          << msg.toStdString() << std::endl;
                // mod 包缺 --to：输出当前实例列表（先判断有没有实例）
                if (msg.contains("--to")) {
                    if (data.isEmpty()) {
                        std::cout << _("（当前没有实例，请先导入整合包）\n",
                                       "(no instances yet, import a modpack first)\n");
                    } else {
                        std::cout << _("当前实例:\n", "Current instances:\n");
                        for (const auto &d : data)
                            std::cout << "  " << d.toStdString() << "\n";
                    }
                }
            }
            done = true;
            QCoreApplication::quit();
        });

    if (!done) QCoreApplication::instance()->exec(); // 等待异步下载完成
    return result;
}

static int handleRm(const QStringList &args) {
    if (args.size() < 2) {
        std::cerr << _("error:  lpcl-cli list-rm <名称|*>\n",
                       "error:  lpcl-cli list-rm <name|*>\n");
        return 1;
    }
    // shell 会把不带引号的 * 展开成当前目录文件列表（多个参数）——检测并提示加引号
    if (args.size() > 2) {
        std::cerr << _("error:  参数过多（shell 会展开 *）。删除全部实例请加引号：lpcl-cli list-rm \"*\"\n",
                       "error:  too many arguments (shell expands *). To remove all instances, quote it: lpcl-cli list-rm \"*\"\n");
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

// ---- 帮助与配置 ----

static void printHelp() {
    auto out = [](const QString &s) { std::cout << s.toStdString(); };
    struct Item { QString cmdCn, cmdEn; const char *descCn, *descEn; };
    const Item items[] = {
        {"list", "list", "列出已导入的整合包实例", "List imported instances"},
        {"mc-list", "mc-list", "列出原版 MC 版本", "List vanilla MC versions"},
        {"launch [名称]",
         "launch [name]",
         "启动整合包游戏（不填写 name 则列出实例选择）",
         "Launch a modpack (If name is not filled in, the instance selection will be listed.)"},
        {"list-javas", "list-javas", "列出可用 Java", "List available Java runtimes"},
        {"set-folder <路径>",
         "set-folder <path>",
         "设置默认游戏目录",
         "Set default Minecraft folder"},
        {"set-player <名称>", "set-player <name>", "设置玩家名称", "Set player name"},
        {"set-lang <en|zh>",
         "set-lang <en|zh>",
         "设置界面语言（持久保存）",
         "Set UI language (persistent)"},
        {"inpack <文件> [--r <名称>]", "inpack <file> [--r <name>]", "导入整合包", "Import modpack"},
        {"install <版本>", "install <version>", "下载原版 MC 版本", "Download a vanilla MC version"},
        {"install-java <大版本>",
         "install-java <major>",
         "下载安装 Java（Adoptium JRE）",
         "Download & install Java (Adoptium JRE)"},
        {"list-rm <名称|*>",
         "list-rm <name|*>",
         "删除实例（* 清空全部）",
         "Remove instance (* for all)"},
        {"player-add <名称>", "player-add <name>", "添加玩家配置（--avatar/--skin）", "Add player profile (--avatar/--skin)"},
        {"player-rm <uuid>", "player-rm <uuid>", "删除玩家配置", "Remove player profile"},
        {"player-list", "player-list", "列出玩家配置", "List player profiles"},
        {"player-select <uuid>", "player-select <uuid>", "选择当前玩家", "Select current player"},
        {"config", "config", "查看当前配置", "Show current configuration"},
        {"test", "test", "全系统自检", "Run system self-test"},
        {"help", "help", "显示帮助信息", "Show help information"},
        {"version", "version", "显示版本号", "Show version number"},
    };
    auto printItem = [&](const Item &it) {
        out(QString("  %1 %2\n").arg(g_lang == CN ? it.cmdCn : it.cmdEn, -20)
                .arg(T(it.descCn, it.descEn)));
    };
    out(T("Usage: lpcl-cli <command> [args]\n",
          "Usage: lpcl-cli <command> [args]\n"));
    out("\n"); out(T("命令 / Commands:\n", "Commands:\n"));
    for (const auto &it : items) printItem(it);
    std::cout.flush();
}

static int handleConfig() {
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

static int handleInstall(const QStringList &args) {
    if (args.size() < 2) {
        std::cerr << _("error:  lpcl-cli install <MC版本>\n",
                       "error:  lpcl-cli install <mc-version>\n");
        return 1;
    }
    std::cout << _(QString("正在下载 MC %1 ...\n").arg(args[1]).toStdString(),
                   QString("Downloading MC %1 ...\n").arg(args[1]).toStdString());
    bool ok = lpcl::installVersion(args[1],
        [](const lpcl::ImportProgress &p) {
            if (p.percent >= 0) {
                int bars = p.percent / 5;
                std::cout << "\r  [";
                for (int i = 0; i < 20; ++i)
                    std::cout << (i < bars ? "=" : i == bars ? ">" : " ");
                std::cout << "] " << p.percent << "% " << p.step.toStdString();
                std::cout.flush();
            } else {
                std::cout << "\r  " << p.step.toStdString() << "                    ";
                std::cout.flush();
            }
        });
    std::cout << std::endl;
    if (ok) {
        std::cout << "success" << std::endl;
        return 0;
    }
    std::cerr << _("error:  下载失败（版本不存在或网络错误）\n",
                   "error:  download failed (version not found or network error)\n");
    return 1;
}

static int handleInstallJava(const QStringList &args) {
    if (args.size() < 2) {
        std::cerr << _("error:  lpcl-cli install-java <大版本>\n",
                       "error:  lpcl-cli install-java <major>\n");
        return 1;
    }
    bool okNum = false;
    int major = args[1].toInt(&okNum);
    if (!okNum || major <= 0) {
        std::cerr << _("error:  无效的 Java 大版本\n", "error:  invalid Java major version\n");
        return 1;
    }
    std::cout << _(QString("正在下载 JRE %1 ...\n").arg(major).toStdString(),
                   QString("Downloading JRE %1 ...\n").arg(major).toStdString());
    QString err;
    if (!lpcl::installJavaRuntime(major, &err)) {
        std::cerr << _("error:  ", "error: ") << err.toStdString() << std::endl;
        return 1;
    }
    std::cout << "success" << std::endl;
    return 0;
}

// ---- 命令派发 ----

/// 解析 mcFolder 并派发到对应处理函数。
/// 返回值 >= 0 表示已处理（退出码），-1 表示未知命令。
static int dispatchCommand(const QString &cmd, QStringList &args) {
    // ---- 组 A: 无需 mcFolder ----
    if (cmd == "help")    { printHelp(); return 0; }
    if (cmd == "version") {
        std::cout << QCoreApplication::applicationName().toStdString() << " "
                  << QCoreApplication::applicationVersion().toStdString() << std::endl;
        return 0;
    }
    if (cmd == "config")       return handleConfig();
    if (cmd == "set-folder")     return handleSetFolder(args);
    if (cmd == "set-player")     return handleSetPlayer(args);
    if (cmd == "set-lang")       return handleSetLang(args);
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
    if (cmd == "install")  return handleInstall(args);
    if (cmd == "install-java") return handleInstallJava(args);
    if (cmd == "launch")   return handleLaunch(args);
    if (cmd == "inpack")  return handleInpack(args);
    if (cmd == "list-rm") return handleRm(args);

    return -1; // unknown
}

// ---- main ----

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    app.setApplicationName("lpcl-cli");
    app.setApplicationVersion("0.1");

    // 默认静默，关闭 SDK 调试日志
    QLoggingCategory::setFilterRules("lpcl.*.info=false\nlpcl.*.debug=false");
    // 语言只由 set-lang 持久设置控制（无命令行 flag）
    Settings::initialize();
    if (Settings::instance().getString("UiLanguage") == "zh")
        setLang(false);

    // ---- 选项解析 ----
    QCommandLineParser parser;
    parser.setApplicationDescription(
        _("LPCL 命令行启动器", "LPCL Command-Line Launcher"));

    parser.addPositionalArgument("command", "placeholder");
    parser.setOptionsAfterPositionalArgumentsMode(
        QCommandLineParser::ParseAsPositionalArguments);
    parser.process(app);

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
