// lpcl-cli — Minecraft launcher CLI frontend
// Links only liblpclcore (QtCore + QtNetwork, no GUI/QML)
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QDir>
#include <iostream>

#include "lpclcore/settings.h"
#include "lpclcore/javamanager.h"
#include "lpclcore/versionmanager.h"
#include "lpclcore/launcher.h"
#include "lpclcore/downloadmanager.h"
#include "lpclcore/offlineauth.h"

static void cmdListVersions() {
    auto &vm = VersionManager::instance();
    vm.loadLocalVersions();
    auto ids = vm.versionIds();
    if (ids.isEmpty()) {
        std::cout << "（没有找到已安装的版本）\n";
        std::cout << "提示：用 lpcl-cli install <版本号> 安装版本\n";
        return;
    }
    std::cout << "已安装的版本:\n";
    for (const auto &id : ids) {
        std::cout << "  " << id.toStdString() << "\n";
    }
}

static void cmdInstallVersion(const QString &versionId) {
    std::cout << "正在安装 " << versionId.toStdString() << " ...\n";
    auto &vm = VersionManager::instance();
    Q_UNUSED(vm);
    std::cout << "（安装功能尚未实现——后端仍为桩实现）\n";
}

static void cmdLaunch(const QString &versionId) {
    std::cout << "正在启动 " << versionId.toStdString() << " ...\n";
    auto &launcher = Launcher::instance();
    auto &jm = JavaManager::instance();

    // 离线登录
    auto login = OfflineAuth::createOfflineLogin("Player");

    // 选 Java
    auto version = VersionManager::instance().loadVersion(versionId);
    auto *java = jm.selectJava();

    if (!java) {
        std::cerr << "错误：未找到可用的 Java\n";
        return;
    }

    // 连接日志输出
    QObject::connect(&launcher, &Launcher::gameLog, [](const QString &line) {
        std::cout << "[MC] " << line.toStdString() << std::endl;
    });
    QObject::connect(&launcher, &Launcher::gameExited, [](int code, const QString &) {
        std::cout << "游戏退出，exit code: " << code << "\n";
        QCoreApplication::quit();
    });

    if (!launcher.launch(version, *java, login)) {
        std::cerr << "启动失败\n";
        return;
    }

    std::cout << "游戏已启动，等待退出...\n";
}

static void cmdListJavas() {
    auto &jm = JavaManager::instance();
    jm.scanSystemJava();
    auto names = jm.javaNames();
    if (names.isEmpty()) {
        std::cout << "（未检测到 Java，正在扫描中...）\n";
        return;
    }
    std::cout << "检测到的 Java:\n";
    for (const auto &name : names) {
        std::cout << "  " << name.toStdString() << "\n";
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    app.setApplicationName("lpcl-cli");
    app.setApplicationVersion("0.1");

    // 初始化核心库
    Settings::initialize();
    auto &jm = JavaManager::instance();
    auto &vm = VersionManager::instance();

    // 设置默认 Minecraft 文件夹
    QString mcFolder = Settings::instance().getString("LaunchFolderSelect");
    if (mcFolder.isEmpty()) {
        mcFolder = QDir::homePath() + "/.minecraft/";
    }
    vm.setMcFolder(mcFolder);

    // 命令行解析
    QCommandLineParser parser;
    parser.setApplicationDescription("LPCL 命令行启动器");
    parser.addHelpOption();
    parser.addVersionOption();

    parser.addPositionalArgument("command", "命令: list | install <版本> | launch <版本> | list-javas");

    parser.parse(app.arguments());

    const QStringList args = parser.positionalArguments();
    if (args.isEmpty()) {
        parser.showHelp(1);
    }

    const QString cmd = args.at(0);

    if (cmd == "list") {
        cmdListVersions();
    } else if (cmd == "install") {
        if (args.size() < 2) {
            std::cerr << "用法: lpcl-cli install <版本号>\n";
            return 1;
        }
        cmdInstallVersion(args.at(1));
    } else if (cmd == "launch") {
        if (args.size() < 2) {
            std::cerr << "用法: lpcl-cli launch <版本号>\n";
            return 1;
        }
        cmdLaunch(args.at(1));
        return app.exec();  // 等待游戏退出
    } else if (cmd == "list-javas") {
        cmdListJavas();
    } else {
        std::cerr << "未知命令: " << cmd.toStdString() << "\n";
        parser.showHelp(1);
    }

    return 0;
}
