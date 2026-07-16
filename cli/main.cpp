// lpcl-cli — Minecraft launcher CLI frontend
// Links only liblpclcore (QtCore + QtNetwork, no GUI/QML)

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDir>
#include <iostream>

#include "lpcl.h"
#include "core/settings.h"
#include "core/versionmanager.h"

// ---- 中英文切换 ----

enum Lang { CN, EN };
static Lang g_lang = EN;

#define _(cn, en) (g_lang == CN ? cn : en)

static void setLang(bool en) { g_lang = en ? EN : CN; }

// ---- main ----

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    app.setApplicationName("lpcl-cli");
    app.setApplicationVersion("0.1");

    // 先扫描 --cn/--en，确保 help 文本在 parser 设置时就是正确语言
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
    parser.addOption(optCn);
    parser.addOption(optEn);
    QCommandLineOption optHelp("help", _("显示帮助信息", "Show help information"));
    QCommandLineOption optVersion("version", _("显示版本号", "Show version number"));
    QCommandLineOption optMcFolder("mc-folder",
        _("指定 Minecraft 文件夹路径（默认 ~/.minecraft/）",
          "Set Minecraft folder path (default ~/.minecraft/)"),
        _("路径", "path"));
    QCommandLineOption optConfig("config",
        _("查看当前配置", "Show current configuration"));
    parser.addOption(optHelp);
    parser.addOption(optVersion);
    parser.addOption(optMcFolder);
    parser.addOption(optConfig);

    parser.addPositionalArgument("command", "placeholder");
    parser.process(app);

    // 自定义 help，对齐 Options 风格
    auto printHelp = [&]() {
        auto T = [](const char *cn, const char *en) { return QString::fromUtf8(g_lang == CN ? cn : en); };

        auto out = [](const QString &s) { std::cout << s.toStdString(); };

        struct Item {
            QString cmdCn, cmdEn;
            const char *descCn, *descEn;
        };
        const Item items[] = {
            {"list",              "list",              "列出已导入的整合包",              "List imported modpacks"},
            {"launch <名称>",     "launch <name>",     "启动整合包游戏",                 "Launch a modpack"},
            {"list-javas",        "list-javas",        "列出可用 Java",                  "List available Java runtimes"},
            {"set-folder <路径>", "set-folder <path>", "设置默认游戏目录",               "Set default Minecraft folder"},
            {"set-player <名称>", "set-player <name>", "设置玩家名称",                   "Set player name"},
            {"inpack <文件>",     "inpack <file>",     "导入整合包（需 --mc-folder）",   "Import modpack (--mc-folder required)"},
        };
        const Item opts[] = {
            {"--cn",              "--cn",              "使用中文输出",                   "Use Chinese output"},
            {"--en",              "--en",              "使用英文输出（默认）",            "Use English output (default)"},
            {"--config",          "--config",          "查看当前配置",                   "Show current configuration"},
            {"--help",            "--help",            "显示帮助信息",                   "Show help information"},
            {"--version",         "--version",         "显示版本号",                     "Show version number"},
            {"--mc-folder <路径>","--mc-folder <path>","指定 Minecraft 文件夹",          "Set Minecraft folder path"},
        };
        auto printItem = [&](const Item &it) {
            QString cmd  = g_lang == CN ? it.cmdCn : it.cmdEn;
            QString desc = T(it.descCn, it.descEn);
            out(QString("  %1 %2\n").arg(cmd, -20).arg(desc));
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

    // 初始化（--config 需要 Settings）
    Settings::initialize();

    if (parser.isSet(optConfig)) {
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
    QString mcFolder;
    if (parser.isSet(optMcFolder))
        mcFolder = parser.value(optMcFolder);
    else
        mcFolder = Settings::instance().getString("LaunchFolderSelect");
    if (mcFolder.isEmpty()) mcFolder = QDir::homePath() + "/.minecraft/";
    VersionManager::instance().setMcFolder(mcFolder);

    if (parser.isSet(optMcFolder)) {
        std::cout << _("Minecraft 文件夹: ", "Minecraft folder: ") << mcFolder.toStdString() << std::endl;
    }

    const QStringList args = parser.positionalArguments();
    if (args.isEmpty()) { printHelp(); return 1; }

    const QString cmd = args.at(0);

    if (cmd == "list") {
        auto ids = lpcl::listVersions();
        if (ids.isEmpty()) {
            std::cout << _("（没有找到已安装的版本）\n"
                           "提示：用 lpcl-cli inpack <文件> --mc-folder <路径> 导入整合包\n",
                           "(No installed versions found)\n"
                           "Hint: use lpcl-cli inpack <file> --mc-folder <path>\n");
        } else {
            std::cout << _("已安装的版本:\n", "Installed versions:\n");
            for (const auto &id : ids)
                std::cout << "  " << id.toStdString() << "\n";
        }

    } else if (cmd == "launch") {
        if (args.size() < 2) {
            std::cerr << _("用法: lpcl-cli launch <版本号>\n",
                           "Usage: lpcl-cli launch <version>\n");
            return 1;
        }
        std::cout << _(QString("正在启动 %1 ...\n").arg(args[1]).toStdString(),
                       QString("Launching %1 ...\n").arg(args[1]).toStdString());
        if (!lpcl::launchVersion(args[1],
                [](const QString &line) { std::cout << "[MC] " << line.toStdString() << std::endl; },
                [&](int code) {
                    std::cout << _("游戏退出，exit code: ", "Game exited, exit code: ") << code << "\n";
                    QCoreApplication::quit();
                })) {
            std::cerr << _("启动失败\n", "Launch failed\n");
            return 1;
        }
        std::cout << _("游戏已启动，等待退出...\n", "Game started, waiting for exit...\n");
        return app.exec();

    } else if (cmd == "list-javas") {
        auto names = lpcl::listJavas();
        if (names.isEmpty()) {
            std::cout << _("（未检测到 Java，正在扫描中...）\n",
                           "(No Java detected, scanning...)\n");
        } else {
            std::cout << _("检测到的 Java:\n", "Detected Java:\n");
            for (const auto &name : names)
                std::cout << "  " << name.toStdString() << "\n";
        }

    } else if (cmd == "set-folder") {
        if (args.size() < 2) {
            std::cerr << _("用法: lpcl-cli set-folder <路径>\n",
                           "Usage: lpcl-cli set-folder <path>\n");
            return 1;
        }
        Settings::instance().setString("LaunchFolderSelect", args[1]);
        std::cout << _("默认游戏目录已设置为: ", "Default game folder set to: ")
                  << args[1].toStdString() << std::endl;

    } else if (cmd == "set-player") {
        if (args.size() < 2) {
            std::cerr << _("用法: lpcl-cli set-player <名称>\n",
                           "Usage: lpcl-cli set-player <name>\n");
            return 1;
        }
        Settings::instance().setString("PlayerName", args[1]);
        std::cout << _("玩家名称已设置为: ", "Player name set to: ")
                  << args[1].toStdString() << std::endl;

    } else if (cmd == "inpack") {
        if (args.size() < 2) {
            std::cerr << _("用法: lpcl-cli inpack <文件路径> [--mc-folder <路径>]\n",
                           "Usage: lpcl-cli inpack <file> [--mc-folder <path>]\n");
            return 1;
        }
        if (!parser.isSet(optMcFolder) && Settings::instance().getString("LaunchFolderSelect").isEmpty()) {
            std::cerr << _("错误：未设置安装目录，请使用 --mc-folder 指定\n",
                           "Error: no install folder set, use --mc-folder\n")
                      << _("用法: lpcl-cli inpack <文件> --mc-folder <路径>\n",
                           "Usage: lpcl-cli inpack <file> --mc-folder <path>\n");
            return 1;
        }
        std::cout << _("正在导入整合包...\n", "Importing modpack...\n");
        lpcl::importModpack(args[1], "",
            [](const QString &status, int progress) {
                // 进度条: [====>     ] 50%
                int bars = progress / 5;
                std::cout << "\r  [";
                for (int i = 0; i < 20; ++i)
                    std::cout << (i < bars ? "=" : i == bars ? ">" : " ");
                std::cout << "] " << progress << "% " << status.toStdString();
                if (progress >= 100) std::cout << std::endl;
                std::cout.flush();
            },
            [](bool ok, const QString &msg) {
                if (ok)
                    std::cout << std::endl << _("✓ 导入成功！实例名: ", "✓ Import success! Instance: ")
                              << msg.toStdString() << std::endl;
                else
                    std::cerr << std::endl << _("✗ 导入失败: ", "✗ Import failed: ")
                              << msg.toStdString() << std::endl;
                QCoreApplication::quit();
            });
        return app.exec();

    } else {
        std::cerr << _(QString("未知命令: %1\n").arg(cmd).toStdString(),
                       QString("Unknown command: %1\n").arg(cmd).toStdString());
        printHelp(); return 1;
    }

    return 0;
}
