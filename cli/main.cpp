// lpcl-cli — Minecraft launcher CLI frontend
// Links only liblpclcore (QtCore + QtNetwork, no GUI/QML)

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QLoggingCategory>
#include <iostream>

#include "lpcl.h"
#include "core/settings.h"
#include "core/versionmanager.h"

// ---- 中英文切换 ----

enum Lang { CN, EN };
static Lang g_lang = EN;

#define _(cn, en) (g_lang == CN ? cn : en)
static void setLang(bool en) { g_lang = en ? EN : CN; }
static auto T(const char *cn, const char *en) { return QString::fromUtf8(g_lang == CN ? cn : en); }

// ---- helper ----

// 从 args 列表中提取 --folder <path>，返回 path，并从列表中移除这两个元素
static QString extractFolder(QStringList &args) {
    int idx = args.indexOf("--folder");
    if (idx >= 0 && idx + 1 < args.size()) {
        QString path = args.at(idx + 1);
        args.removeAt(idx + 1);
        args.removeAt(idx);
        return path;
    }
    return {};
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

    auto printHelp = [&]() {
        auto out = [](const QString &s) { std::cout << s.toStdString(); };
        struct Item { QString cmdCn, cmdEn; const char *descCn, *descEn; };
        const Item items[] = {
            {"list",              "list",              "列出已导入的整合包",    "List imported modpacks"},
            {"launch <名称>",     "launch <name>",     "启动整合包游戏",       "Launch a modpack"},
            {"list-javas",        "list-javas",        "列出可用 Java",        "List available Java runtimes"},
            {"set-folder <路径>", "set-folder <path>", "设置默认游戏目录",     "Set default Minecraft folder"},
            {"set-player <名称>", "set-player <name>", "设置玩家名称",         "Set player name"},
            {"inpack <文件>",     "inpack <file>",     "导入整合包",           "Import modpack"},
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

    if (parser.isSet(optHelp))  { printHelp(); return 0; }
    if (parser.isSet(optVersion)) {
        std::cout << app.applicationName().toStdString() << " "
                  << app.applicationVersion().toStdString() << std::endl;
        return 0;
    }

    QStringList args = parser.positionalArguments();
    if (args.isEmpty()) { printHelp(); return 1; }

    if (parser.isSet(optConfig)) {
        Settings::initialize();
        std::cout << _("LPCL 版本: ", "LPCL version: ") << GIT_DESCRIBE << std::endl
                  << _("提交: ",       "Commit: ")       << GIT_COMMIT_HASH << std::endl;
        QString folder = Settings::instance().getString("LaunchFolderSelect");
        if (folder.isEmpty()) folder = _("（未设置）", "(not set)");
        std::cout << _("默认游戏目录: ", "Default game folder: ") << folder.toStdString() << std::endl;
        QString player = Settings::instance().getString("PlayerName");
        if (player.isEmpty()) player = _("（未设置）", "(not set)");
        std::cout << _("玩家名称: ", "Player name: ") << player.toStdString() << std::endl;
        QString avatar = Settings::instance().getString("PlayerAvatar");
        if (avatar.isEmpty()) avatar = _("（未设置）", "(not set)");
        std::cout << _("头像路径: ", "Avatar path: ") << avatar.toStdString() << std::endl;
        return 0;
    }

    const QString cmd = args.at(0);
    Settings::initialize();

    // ---- set-folder ----
    if (cmd == "set-folder") {
        if (args.size() < 2) {
            std::cerr << _("error:  lpcl-cli set-folder <路径>\n",
                           "error:  lpcl-cli set-folder <path>\n");
            return 1;
        }
        Settings::instance().setString("LaunchFolderSelect", args[1]);
        std::cout << "success" << std::endl;
        return 0;
    }

    // ---- set-player ----
    if (cmd == "set-player") {
        if (args.size() < 2) {
            std::cerr << T("error: 缺少参数\n", "error: missing argument\n").toStdString();
            return 1;
        }
        Settings::instance().setString("PlayerName", args[1]);
        std::cout << "success" << std::endl;
        return 0;
    }

    // 确定游戏目录
    QString mcFolder;
    // inpack 支持 --folder 选项（仅 inpack 可用）
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
    VersionManager::instance().setMcFolder(mcFolder);

    // ---- list ----
    if (cmd == "list") {
        auto ids = lpcl::listVersions();
        if (ids.isEmpty()) {
            std::cout << T("(No installed versions found)\n",
                           "(No installed versions found)\n").toStdString();
        } else {
            std::cout << _("已安装的版本:\n", "Installed versions:\n");
            for (const auto &id : ids)
                std::cout << "  " << id.toStdString() << "\n";
        }
        return 0;
    }

    // ---- launch ----
    if (cmd == "launch") {
        if (args.size() < 2) {
            std::cerr << T("error: lpcl-cli launch <名称>\n",
                           "error: lpcl-cli launch <name>\n").toStdString();
            return 1;
        }
        std::cout << _(QString("正在启动 %1 ...\n").arg(args[1]).toStdString(),
                       QString("Launching %1 ...\n").arg(args[1]).toStdString());
        if (!lpcl::launchVersion(args[1],
                [](const QString &line) { std::cout << "[MC] " << line.toStdString() << std::endl; },
                [&](int code) {
                    std::cout << _("exit: ", "exit: ") << code << "\n";
                    QCoreApplication::quit();
                })) {
            std::cerr << T("error: launch failed\n", "error: launch failed\n").toStdString();
            return 1;
        }
        std::cout << "success" << std::endl;
        return app.exec();
    }

    // ---- list-javas ----
    if (cmd == "list-javas") {
        auto names = lpcl::listJavas();
        if (names.isEmpty()) {
            std::cout << _("(No Java detected)\n",
                           "(No Java detected)\n");
        } else {
            std::cout << _("success\n", "success\n");
            for (const auto &name : names)
                std::cout << "  " << name.toStdString() << "\n";
        }
        return 0;
    }

    // ---- inpack ----
    if (cmd == "inpack") {
        if (args.size() < 2) {
            std::cerr << _("error:  lpcl-cli inpack <文件> [--folder <路径>]\n",
                           "error:  lpcl-cli inpack <file> [--folder <path>]\n");
            return 1;
        }
        std::cout << _("正在导入整合包...\n", "Importing modpack...\n");
        bool inpackDone = false;
        int  inpackResult = 1;
        lpcl::importModpack(args[1], "",
            [&](const QString &status, int progress) {
                int bars = progress / 5;
                std::cout << "\r  [";
                for (int i = 0; i < 20; ++i)
                    std::cout << (i < bars ? "=" : i == bars ? ">" : " ");
                std::cout << "] " << progress << "% " << status.toStdString();
                if (progress >= 100) std::cout << std::endl;
                std::cout.flush();
            },
            [&](bool ok, const QString &msg) {
                if (ok) {
                    std::cout << std::endl << _("success: ", "success: ")
                              << msg.toStdString() << std::endl;
                    inpackResult = 0;
                } else {
                    std::cerr << std::endl << _("error: ", "error: ")
                              << msg.toStdString() << std::endl;
                }
                inpackDone = true;
                QCoreApplication::quit();
            });
        if (inpackDone) return inpackResult;
        app.exec();
        return inpackResult;
    }

    // ---- unknown ----
    std::cerr << _(QString("未知命令: %1\n").arg(cmd).toStdString(),
                   QString("error: unknown command: %1\n").arg(cmd).toStdString());
    printHelp(); return 1;
}
