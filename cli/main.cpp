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
    parser.addOption(optHelp);
    parser.addOption(optVersion);

    parser.addPositionalArgument("command",
        _("命令: list | install <版本> | launch <版本> | list-javas",
          "Commands: list | install <version> | launch <version> | list-javas"));

    parser.process(app);

    if (parser.isSet(optHelp))  { parser.showHelp(0); }
    if (parser.isSet(optVersion)) {
        std::cout << app.applicationName().toStdString() << " "
                  << app.applicationVersion().toStdString() << std::endl;
        return 0;
    }

    // 初始化
    Settings::initialize();
    QString mcFolder = Settings::instance().getString("LaunchFolderSelect");
    if (mcFolder.isEmpty()) mcFolder = QDir::homePath() + "/.minecraft/";
    VersionManager::instance().setMcFolder(mcFolder);

    const QStringList args = parser.positionalArguments();
    if (args.isEmpty()) { parser.showHelp(1); }

    const QString cmd = args.at(0);

    if (cmd == "list") {
        auto ids = lpcl::listVersions();
        if (ids.isEmpty()) {
            std::cout << _("（没有找到已安装的版本）\n"
                           "提示：用 lpcl-cli install <版本号> 安装版本\n",
                           "(No installed versions found)\n"
                           "Hint: use lpcl-cli install <version>\n");
        } else {
            std::cout << _("已安装的版本:\n", "Installed versions:\n");
            for (const auto &id : ids)
                std::cout << "  " << id.toStdString() << "\n";
        }

    } else if (cmd == "install") {
        if (args.size() < 2) {
            std::cerr << _("用法: lpcl-cli install <版本号>\n",
                           "Usage: lpcl-cli install <version>\n");
            return 1;
        }
        std::cout << _(QString("正在安装 %1 ...\n").arg(args[1]).toStdString(),
                       QString("Installing %1 ...\n").arg(args[1]).toStdString());
        lpcl::installVersion(args[1]);
        std::cout << _("（安装功能尚未实现——后端仍为桩实现）\n",
                       "(Install not yet implemented — backend is stubbed)\n");

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

    } else {
        std::cerr << _(QString("未知命令: %1\n").arg(cmd).toStdString(),
                       QString("Unknown command: %1\n").arg(cmd).toStdString());
        parser.showHelp(1);
    }

    return 0;
}
