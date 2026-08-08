// lpcl — Minecraft launcher CLI frontend
// Links only liblpclcore (QtCore + QtNetwork, no GUI/QML)
//
// 维护指南：
//   新增命令只需四步：
//     1. 在 printHelp() 的 items[] 数组加一行
//     2. 写一个 static int handleXxx(const QStringList &args) 函数
//     3. 在 dispatchCommand() 中按是否需要 mcFolder 归类加入
//     4. 在 printCommandHelp() 的 helps[] 加该命令的参数详解（中英双语）——硬性约定，禁止遗漏

#include <QCoreApplication>
#include <cstdlib>
#include <QCommandLineParser>
#include <unistd.h>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QPointer>
#include <QProcess>
#include <QRegularExpression>
#include <QSysInfo>
#include <QUrl>
#include <iostream>

#include "i18n.h"
#include "test.h"
#include "tui_select.h"
#include "tui_prompt.h"
#include "lpcl.h"
#include "core/settings.h"
#include "core/versionmanager.h"
#include "download/downloadmanager.h"
#include "util/file_utils.h"

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
        std::cerr << _("error:  lpcl set-folder <路径>\n",
                       "error:  lpcl set-folder <path>\n");
        return 1;
    }
    Settings::instance().setString("LaunchFolderSelect", args[1]);
    std::cout << "success" << std::endl;
    return 0;
}

static int handleSetLang(const QStringList &args) {
    if (args.size() < 2 || (args[1] != "en" && args[1] != "zh")) {
        std::cerr << _("error:  lpcl set-lang <en|zh>\n",
                       "error:  lpcl set-lang <en|zh>\n");
        return 1;
    }
    // 持久保存 + 立即生效
    Settings::instance().setString("UiLanguage", args[1]);
    setLang(args[1] == "en");
    std::cout << "success" << std::endl;
    return 0;
}

// set-mem <MB|auto>：对应 PCL 的 LaunchRamType/LaunchRamCustom（0=自动，>0=固定 MB）
static int handleSetMem(const QStringList &args) {
    if (args.size() < 2) {
        std::cerr << _("error:  lpcl set-mem <MB|auto>\n",
                       "error:  lpcl set-mem <MB|auto>\n");
        return 1;
    }
    QString v = args[1].toLower();
    if (v == "auto" || v == "自动") {
        Settings::instance().setString("LaunchMaxMemory", "0");
    } else {
        bool ok = false;
        int mb = v.toInt(&ok);
        if (!ok || mb < 512 || mb > 65536) {
            std::cerr << _("error:  内存需为 512~65536 之间的 MB 数，或 auto\n",
                           "error:  memory must be 512-65536 MB, or auto\n");
            return 1;
        }
        Settings::instance().setString("LaunchMaxMemory", QString::number(mb));
    }
    std::cout << "success" << std::endl;
    return 0;
}

// ---- 问题上报（方案 B：生成 GitHub Issue 预填链接） ----

// 最近一份启动日志的末尾，脱敏：accessToken 打码、家目录缩略为 ~
static QString latestLaunchLogTail(int maxLines, int maxChars) {
    QString mcFolder = VersionManager::instance().mcFolder();  // 尊重 --folder 一次性覆盖
    if (mcFolder.isEmpty()) return QString();
    QDir logDir(mcFolder + "/logs");
    const auto files = logDir.entryInfoList({"lpcl-launch-*.log"}, QDir::Files, QDir::Time);
    if (files.isEmpty()) return QString();
    QFile f(files.first().absoluteFilePath());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return QString();

    QStringList lines = QString::fromUtf8(f.readAll()).split('\n');
    static const QRegularExpression tokenRe("(--accessToken\\s+)\\S+");
    const QString home = QDir::homePath();
    for (auto &l : lines) {
        l.replace(tokenRe, "\\1***");
        if (!home.isEmpty()) l.replace(home, "~");
    }
    if (lines.size() > maxLines) lines = lines.mid(lines.size() - maxLines);
    QString tail = lines.join('\n');
    if (tail.size() > maxChars) tail = _("（截断）\n", "(truncated)\n") + tail.right(maxChars);
    return tail;
}

static QString buildIssueUrl(const QString &repo, const QString &desc, const QString &tail) {
    QString body = QString("%1\n\n**环境 / Environment**\n- LPCL: %2 (%3)\n- OS: %4 %5\n- Qt: %6\n")
        .arg(desc, GIT_DESCRIBE, GIT_COMMIT_HASH,
             QSysInfo::prettyProductName(), QSysInfo::currentCpuArchitecture(), qVersion());
    if (!tail.isEmpty())
        body += "\n**最近启动日志 / Launch log (tail)**\n```\n" + tail + "\n```\n";
    QString title = desc.size() > 60 ? desc.left(60) + "..." : desc;
    return QString("https://github.com/%1/issues/new?title=%2&body=%3")
        .arg(repo, QString(QUrl::toPercentEncoding(title)),
             QString(QUrl::toPercentEncoding(body)));
}

static int handleReport(const QStringList &args) {
    QString desc;
    if (args.size() >= 2) {
        desc = args.mid(1).join(' ');
    } else if (isatty(fileno(stdin))) {
        auto d = tuiInput(_("用一句话描述问题", "Describe the issue in one sentence"));
        if (!d) {
            std::cerr << _("已取消\n", "Cancelled\n");
            return 1;
        }
        desc = *d;
    } else {
        std::cerr << _("error:  lpcl report <问题描述>\n",
                       "error:  lpcl report <description>\n");
        return 1;
    }
    if (desc.isEmpty()) desc = "LPCL 问题反馈";

    QString repo = qEnvironmentVariable("LPCL_REPO", "recallrw80-afk/LPCL");
    // GitHub URL 长度有限，先给 4000 字符日志，超长再砍到 1200
    QString url = buildIssueUrl(repo, desc, latestLaunchLogTail(40, 4000));
    if (url.size() > 7500)
        url = buildIssueUrl(repo, desc, latestLaunchLogTail(15, 1200));

    std::cout << _("Issue 预填链接（内容已生成，提交前可再编辑）:\n",
                   "Prefilled issue URL (editable before submitting):\n")
              << url.toStdString() << "\n";
    if (isatty(fileno(stdin))) {
        auto open = tuiConfirm(_("在浏览器中打开吗", "Open in browser"), true);
        if (open && *open)
            QProcess::startDetached("xdg-open", {url});
    }
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

// 玩家配置向导（create-vite 风格问答，仅 TTY；existing 非空 = 编辑模式，各问取现值为默认）
// 交互流程全在 CLI 层，SDK 只提供 add/update/list 数据接口
struct WizardResult { QString name, avatar, skin, customUuid; };
static bool playerWizard(const lpcl::PlayerEntry *existing, WizardResult &out) {
    auto name = tuiInput(_("玩家名字", "Player name"),
                         existing ? existing->name : QString(),
                         _("必填", "required"));
    if (!name) return false;
    if (name->isEmpty()) {
        std::cerr << _("error: 玩家名字不能为空\n", "error: player name required\n");
        return false;
    }

    // 皮肤类型：上下键单选
    static const QStringList skins = {"slim", "wide", "default"};
    int skinIdx = tuiSelect(_("皮肤类型", "Skin type"), skins,
                            existing ? skins.indexOf(existing->skinType) : 0);
    if (skinIdx < 0) return false;

    auto avatar = tuiInput(_("头像路径", "Avatar path"),
                           existing ? existing->avatar : QString(),
                           _("可留空", "optional"));
    if (!avatar) return false;

    QString customUuid;
    auto adv = tuiConfirm(_("需要高级配置吗", "Advanced options"), false);
    if (!adv) return false;
    if (*adv) {
        auto u = tuiInput(_("自定义 UUID", "Custom UUID"),
                          existing ? existing->uuid : QString(),
                          _("留空自动生成", "empty = auto"));
        if (!u) return false;
        customUuid = *u;
    }

    out = {*name, *avatar, skins[skinIdx], customUuid};
    return true;
}

static int handlePlayerAdd(QStringList &args) {
    if (args.size() < 2) {
        // 无参：TTY 进交互向导，非 TTY（脚本/管道）报用法
        if (!isatty(fileno(stdin))) {
            std::cerr << _("error:  lpcl player-add <名称> [--avatar <路径>] [--skin <slim|wide|default>]\n",
                           "error:  lpcl player-add <name> [--avatar <path>] [--skin <slim|wide|default>]\n");
            return 1;
        }
        WizardResult w;
        if (!playerWizard(nullptr, w)) {
            std::cerr << _("已取消\n", "Cancelled\n");
            return 1;
        }
        auto entry = lpcl::addPlayer(w.name, w.avatar, w.skin, w.customUuid);
        std::cout << _("已添加玩家:\n", "Player added:\n")
                  << "  UUID: " << entry.uuid.toStdString() << "\n"
                  << "  " << _("名称: ", "Name: ") << entry.name.toStdString() << "\n"
                  << "  Skin: " << entry.skinType.toStdString() << "\n";
        if (!entry.avatar.isEmpty())
            std::cout << "  " << _("头像: ", "Avatar: ") << entry.avatar.toStdString() << "\n";
        return 0;
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

// 把"序号或 uuid"解析成玩家 uuid（序号 = player-list 中的 1 起始编号）
static QString resolvePlayerUuid(const QString &idOrIndex) {
    bool isNum = false;
    int n = idOrIndex.toInt(&isNum);
    if (isNum && n >= 1) {
        auto players = lpcl::listPlayers();
        if (n <= players.size()) return players[n - 1].uuid;
    }
    return idOrIndex;  // 不是有效序号则按 uuid 处理
}

static int handlePlayerRm(const QStringList &args) {
    if (args.size() < 2) {
        // 无参：TTY 上下键选择要删的玩家（二次确认）；非 TTY 报用法
        if (!isatty(fileno(stdin))) {
            std::cerr << _("error:  lpcl player-rm <uuid|序号>\n",
                           "error:  lpcl player-rm <uuid|index>\n");
            return 1;
        }
        auto players = lpcl::listPlayers();
        if (players.isEmpty()) {
            std::cerr << _("error:  没有玩家配置\n", "error:  no player profiles\n");
            return 1;
        }
        QStringList names;
        for (const auto &p : players) names << p.name;
        int pick = tuiSelect(_("选择要删除的玩家", "Select a player to remove"), names);
        if (pick < 0) {
            std::cerr << _("已取消\n", "Cancelled\n");
            return 1;
        }
        auto yes = tuiConfirm(QString(_("确定删除 %1 吗", "Remove %1")).arg(players[pick].name), false);
        if (!yes || !*yes) {
            std::cerr << _("已取消\n", "Cancelled\n");
            return 1;
        }
        if (lpcl::removePlayer(players[pick].uuid)) {
            std::cout << "success" << std::endl;
            return 0;
        }
        std::cerr << _("error: UUID 或序号不存在\n", "error: UUID or index not found\n");
        return 1;
    }
    QString uuid = resolvePlayerUuid(args[1]);
    if (lpcl::removePlayer(uuid)) {
        std::cout << "success" << std::endl;
        return 0;
    }
    std::cerr << _("error: UUID 或序号不存在\n", "error: UUID or index not found\n");
    return 1;
}

static int handlePlayerEdit(const QStringList &args) {
    auto players = lpcl::listPlayers();
    if (players.isEmpty()) {
        std::cerr << _("error:  没有玩家配置，请先 player-add\n",
                       "error:  no player profiles, run player-add first\n");
        return 1;
    }
    // 定位目标：参数为 uuid|序号；无参时 TTY 上下键选择
    QString uuid;
    if (args.size() >= 2) {
        uuid = resolvePlayerUuid(args[1]);
    } else {
        if (!isatty(fileno(stdin))) {
            std::cerr << _("error:  lpcl player-edit <uuid|序号>\n",
                           "error:  lpcl player-edit <uuid|index>\n");
            return 1;
        }
        QStringList names;
        for (const auto &p : players) names << p.name;
        int pick = tuiSelect(_("选择要修改的玩家", "Select a player to edit"), names);
        if (pick < 0) {
            std::cerr << _("已取消\n", "Cancelled\n");
            return 1;
        }
        uuid = players[pick].uuid;
    }
    const lpcl::PlayerEntry *existing = nullptr;
    for (const auto &p : players)
        if (p.uuid == uuid) { existing = &p; break; }
    if (!existing) {
        std::cerr << _("error: UUID 或序号不存在\n", "error: UUID or index not found\n");
        return 1;
    }

    WizardResult w;
    if (!playerWizard(existing, w)) {
        std::cerr << _("已取消\n", "Cancelled\n");
        return 1;
    }
    if (!lpcl::updatePlayer(uuid, w.name, w.avatar, w.skin, w.customUuid)) {
        std::cerr << _("error: 修改失败（UUID 冲突？）\n", "error: update failed (UUID conflict?)\n");
        return 1;
    }
    std::cout << "success" << std::endl;
    return 0;
}

static int handlePlayerList() {
    auto players = lpcl::listPlayers();
    if (players.isEmpty()) {
        std::cout << _("（无玩家配置）\n", "(No player profiles)\n");
        return 0;
    }
    QString selected = Settings::instance().selectedPlayer();
    int idx = 1;
    for (const auto &p : players) {
        bool isSel = (p.uuid == selected);
        std::cout << (isSel ? " * " : "   ") << idx++ << ") "
                  << p.uuid.toStdString() << "\n"
                  << "     " << _("名称: ", "Name: ") << p.name.toStdString() << "\n";
        if (!p.avatar.isEmpty())
            std::cout << "     " << _("头像: ", "Avatar: ") << p.avatar.toStdString() << "\n";
        std::cout << "     Skin: " << p.skinType.toStdString() << "\n";
    }
    return 0;
}

static int handlePlayerSelect(const QStringList &args) {
    QString uuid;
    if (args.size() < 2) {
        // 无参：TTY 弹上下键选择（非 TTY 提示用法）
        auto players = lpcl::listPlayers();
        if (players.isEmpty()) {
            std::cerr << _("error:  没有玩家配置，请先 player-add\n",
                           "error:  no player profiles, run player-add first\n");
            return 1;
        }
        if (!isatty(fileno(stdin))) {
            std::cerr << _("error:  lpcl player-select <uuid|序号>\n",
                           "error:  lpcl player-select <uuid|index>\n");
            return 1;
        }
        QStringList names;
        for (const auto &p : players) names << p.name;
        int pick = tuiSelect("选择当前玩家", names);
        if (pick < 0) {
            std::cerr << _("已取消\n", "Cancelled\n");
            return 1;
        }
        uuid = players[pick].uuid;
    } else {
        uuid = resolvePlayerUuid(args[1]);
    }
    if (!lpcl::selectPlayer(uuid)) {
        std::cerr << _("error:  玩家 UUID 或序号不存在\n", "error:  player UUID or index not found\n");
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

static int handleMods(const QStringList &args) {
    QString name = args.size() >= 2 ? args.at(1) : QString();
    if (name.isEmpty()) {
        auto ids = lpcl::listVersions();
        std::cerr << T("用法: lpcl mods <实例名>\n", "usage: lpcl mods <instance>\n").toStdString();
        for (const auto &id : ids)
            std::cerr << "  " << id.toStdString() << "\n";
        return ids.isEmpty() ? 0 : 1;
    }
    auto info = lpcl::instanceInfo(name);
    if (info.dirName.isEmpty()) {
        std::cerr << T("error: 实例不存在: ", "error: instance not found: ").toStdString()
                  << name.toStdString() << "\n";
        return 1;
    }
    auto mods = lpcl::listMods(name);
    std::cout << name.toStdString() << T(" 的 Mod（共 ", " mods (").toStdString()
              << mods.size() << T(" 个）:\n", "):\n").toStdString();
    if (mods.isEmpty())
        std::cout << T("  (无 Mod)\n", "  (no mods)\n").toStdString();
    for (const auto &m : mods) {
        std::cout << "  " << (m.enabled ? "[on]  " : "[off] ")
                  << m.fileName.toStdString() << "  ("
                  << QString::number(m.size / 1048576.0, 'f', 2).toStdString() << " MB)\n";
    }
    return 0;
}

static int handleLaunch(const QStringList &args) {
    QString target;
    if (args.size() < 2) {
        // 未指定实例：TTY 用上下键 TUI 选择，非 TTY（管道）退回输序号。
        // 列表 = 实例 + 原版/加载器版本（launchVersion 解析时实例名优先，原版走 loadVersion）
        auto ids = lpcl::listVersions();
        const auto mcIds = lpcl::listMcVersions();
        for (const auto &v : mcIds)
            if (!ids.contains(v)) ids << v;
        if (ids.isEmpty()) {
            std::cerr << _("error:  没有实例，请先导入整合包\n",
                           "error:  no instances, import a modpack first\n");
            return 1;
        }
        int choice = -1;
        if (isatty(fileno(stdin))) {
            choice = tuiSelect("选择要启动的实例", ids);
            if (choice < 0) {
                std::cerr << _("已取消\n", "Cancelled\n");
                return 1;
            }
        } else {
            std::cout << _("选择要启动的实例:\n", "Select an instance to launch:\n");
            for (int i = 0; i < ids.size(); ++i)
                std::cout << "  " << (i + 1) << ") " << ids[i].toStdString() << "\n";
            std::cout << _("请输入序号: ", "Enter number: ");
            std::cout.flush();
            std::string line;
            std::getline(std::cin, line);
            bool okNum = false;
            int n = QString::fromStdString(line).toInt(&okNum);
            if (!okNum || n < 1 || n > ids.size()) {
                std::cerr << _("error:  无效的选择\n", "error:  invalid choice\n");
                return 1;
            }
            choice = n - 1;
        }
        target = ids[choice];
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
        std::cerr << _("error:  lpcl inpack <文件> [--r <名称>] [--to <实例>] [--folder <路径>]\n",
                       "error:  lpcl inpack <file> [--r <name>] [--to <instance>] [--folder <path>]\n");
        return 1;
    }
    QString rename = extractRename(args);
    QString to = extractFlag(args, "--to");
    if (args.size() < 2) {  // --r/--to/--folder 移除后可能没有文件参数
        std::cerr << _("error:  lpcl inpack <文件> [--r <名称>] [--to <实例>] [--folder <路径>]\n",
                       "error:  lpcl inpack <file> [--r <name>] [--to <instance>] [--folder <path>]\n");
        return 1;
    }
    std::cout << _("正在导入整合包...\n", "Importing modpack...\n");

    for (;;) {
        bool done = false;
        int  result = 1;
        QString retryTo;  // mod 包场景：用户在 TUI 里选中的目标实例
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
                    // mod 包缺 --to：TTY 弹上下键选择实例后重试；否则按原样报错
                    if (msg.contains("--to") && !data.isEmpty() && isatty(fileno(stdin))) {
                        int pick = tuiSelect(msg, data);
                        if (pick >= 0) retryTo = data[pick];
                        done = true;
                        QCoreApplication::quit();
                        return;
                    }
                    std::cerr << std::endl << _("error: ", "error: ")
                              << msg.toStdString() << std::endl;
                    // mod 包缺 --to 且无 TTY：输出当前实例列表（先判断有没有实例）
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

        if (retryTo.isEmpty()) return result;
        // 用户已选择目标实例，重试导入
        to = retryTo;
        std::cout << _("以实例 ", "Retrying with target instance ")
                  << to.toStdString() << _(" 为目标重新导入...\n", " ...\n");
    }
}

static int handleRm(const QStringList &args) {
    if (args.size() < 2) {
        // 无参：TTY 上下键选择要删的实例（二次确认）；非 TTY 报用法
        if (!isatty(fileno(stdin))) {
            std::cerr << _("error:  lpcl list-rm <名称|*>\n",
                           "error:  lpcl list-rm <name|*>\n");
            return 1;
        }
        auto ids = lpcl::listVersions();
        if (ids.isEmpty()) {
            std::cout << _("没有可删除的实例\n", "No instances to remove\n");
            return 0;
        }
        int pick = tuiSelect(_("选择要删除的实例", "Select an instance to remove"), ids);
        if (pick < 0) {
            std::cerr << _("已取消\n", "Cancelled\n");
            return 1;
        }
        auto yes = tuiConfirm(QString(_("确定删除 %1 吗", "Remove %1")).arg(ids[pick]), false);
        if (!yes || !*yes) {
            std::cerr << _("已取消\n", "Cancelled\n");
            return 1;
        }
        if (lpcl::removeInstance(ids[pick])) {
            std::cout << "success" << std::endl;
            return 0;
        }
        std::cerr << _("error: 实例不存在或删除失败\n",
                       "error: instance not found or removal failed\n");
        return 1;
    }
    // shell 会把不带引号的 * 展开成当前目录文件列表（多个参数）——检测并提示加引号
    if (args.size() > 2) {
        std::cerr << _("error:  参数过多（shell 会展开 *）。删除全部实例请加引号：lpcl list-rm \"*\"\n",
                       "error:  too many arguments (shell expands *). To remove all instances, quote it: lpcl list-rm \"*\"\n");
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

static void printHelp();

// 命令级详细帮助（lpcl <命令> -h）：用法 + 参数逐项说明
static void printCommandHelp(const QString &cmd) {
    struct H { const char *cmd; const char *cn; const char *en; };
    static const H helps[] = {
        {"list",
         "用法: lpcl list\n列出所有已导入的整合包实例（INI 映射中的）。",
         "Usage: lpcl list\nList all imported modpack instances (from the INI mapping)."},
        {"mods",
         "用法: lpcl mods <名称>\n  <名称>  实例显示名（lpcl list 里看到的）。",
         "Usage: lpcl mods <name>\n  <name>  instance display name (as shown by lpcl list)."},
        {"mc-list",
         "用法: lpcl mc-list\n列出已下载的原版 MC 版本与加载器版本。",
         "Usage: lpcl mc-list\nList downloaded vanilla MC and loader versions."},
        {"launch",
         "用法: lpcl launch [名称]\n"
         "  [名称]  实例显示名或原版版本号；省略则上下键选择（非 TTY 退回输序号）。\n"
         "有外置登录态时自动用该账号启动（在线刷新），失败回退离线玩家。",
         "Usage: lpcl launch [name]\n"
         "  [name]  instance display name or vanilla version; omit for the picker.\n"
         "With a persisted external login, launches with that account (auto-refresh); falls back to offline on failure."},
        {"inpack",
         "用法: lpcl inpack <文件> [--r <名称>] [--to <实例>] [--folder <路径>]\n"
         "  <文件>       整合包路径（CF/Modrinth/MultiMC/HMCL/外壳包/压缩 .minecraft/纯 Mod 包）\n"
         "  --r <名称>   重命名实例（默认用包内名称）\n"
         "  --to <实例>  纯 Mod 包必填：目标实例（Mod 装进它的 mods/）\n"
         "  --folder <路径>  临时改用别的游戏目录（一次性，不写回配置）",
         "Usage: lpcl inpack <file> [--r <name>] [--to <instance>] [--folder <path>]\n"
         "  <file>       modpack file (CF/Modrinth/MultiMC/HMCL/launcher-shell/compressed .minecraft/plain mods)\n"
         "  --r <name>   rename the instance\n"
         "  --to <inst>  required for plain-mod zips: target instance to receive the mods\n"
         "  --folder <path>  one-off game folder override (not persisted)"},
        {"mc-install",
         "用法: lpcl mc-install [版本]\n  [版本]  如 1.20.1；省略 = 最新正式版。重复执行 = 校验补齐缺失文件。",
         "Usage: lpcl mc-install [version]\n  [version]  e.g. 1.20.1; omit for latest release. Re-running verifies/repairs files."},
        {"java-install",
         "用法: lpcl java-install <大版本>\n  <大版本>  如 8 / 17 / 21（从 Adoptium 下载 JRE 并注册）。",
         "Usage: lpcl java-install <major>\n  <major>  e.g. 8 / 17 / 21 (downloads a JRE from Adoptium and registers it)."},
        {"server-install",
         "用法: lpcl server-install [版本] [--forge|--fabric|--neoforge [加载器版本]] [--from <实例>]\n"
         "  [版本]                MC 版本，如 1.20.1；省略 = 最新正式版\n"
         "  --forge / --fabric / --neoforge   装加载器服务端；裸写 = 自动最新加载器版本，带值 = 指定版本\n"
         "  --from <实例>         把该实例的 mods/config/defaultconfigs 复制进服务端目录\n"
         "产物在 {游戏目录}/servers/<标识>/，标识如 1.20.1 或 1.20.1-forge-47.3.0。\n"
         "注意：客户端专属 mod（Sodium 等渲染/界面类）会让服务端启动崩溃。",
         "Usage: lpcl server-install [version] [--forge|--fabric|--neoforge [loader-ver]] [--from <instance>]\n"
         "  [version]             MC version, e.g. 1.20.1; omit for latest release\n"
         "  --forge/--fabric/--neoforge   loader server; bare flag = latest loader version, value = pinned\n"
         "  --from <instance>     copy the instance's mods/config/defaultconfigs into the server dir\n"
         "Output goes to {game folder}/servers/<id>/, e.g. 1.20.1 or 1.20.1-forge-47.3.0.\n"
         "Note: client-only mods (rendering/UI like Sodium) crash dedicated servers."},
        {"server-start",
         "用法: lpcl server-start <标识> [--eula]\n"
         "  <标识>   servers/ 下的目录名（原版=版本号，加载器=版本-加载器-版本）\n"
         "  --eula   书面同意 Minecraft EULA（首次启动必须；TTY 下也可交互确认）\n"
         "前台运行，控制台直通（/stop 关服）。首启自动写 online-mode=false。",
         "Usage: lpcl server-start <id> [--eula]\n"
         "  <id>     directory name under servers/ (vanilla = version, loader = version-loader-ver)\n"
         "  --eula   accept the Minecraft EULA in writing (required on first start; TTY can confirm interactively)\n"
         "Runs in the foreground with an attached console (/stop to halt). First start writes online-mode=false."},
        {"list-rm",
         "用法: lpcl list-rm [名称|*]\n"
         "  [名称]  实例显示名；* = 删除全部（注意加引号防 shell 展开）；省略则上下键选择 + 二次确认。",
         "Usage: lpcl list-rm [name|*]\n"
         "  [name]  instance display name; * = remove all (quote it); omit for picker + confirmation."},
        {"player-add",
         "用法: lpcl player-add [名称] [--avatar <路径>] [--skin <slim|wide|default>]\n"
         "  无参进入交互向导（名字→皮肤→头像→高级自定义 UUID）。首个玩家自动选中。",
         "Usage: lpcl player-add [name] [--avatar <path>] [--skin <slim|wide|default>]\n"
         "  No args = interactive wizard (name → skin → avatar → advanced custom UUID). First player auto-selected."},
        {"player-edit",
         "用法: lpcl player-edit [uuid|序号]\n交互向导修改，回车保留原值；无参上下键选择玩家。",
         "Usage: lpcl player-edit [uuid|index]\nInteractive wizard; Enter keeps current value. No args = picker."},
        {"player-rm",
         "用法: lpcl player-rm [uuid|序号]\n无参上下键选择 + 二次确认。删除选中玩家后自动选中剩余首个。",
         "Usage: lpcl player-rm [uuid|index]\nNo args = picker + confirmation. Removing the selected player selects the first remaining."},
        {"player-list",
         "用法: lpcl player-list\n列出玩家（带序号，* 为当前选中）。",
         "Usage: lpcl player-list\nList players (numbered, * marks the selected one)."},
        {"player-select",
         "用法: lpcl player-select [uuid|序号]\n选择当前玩家（离线模式启动用）；无参上下键选择。",
         "Usage: lpcl player-select [uuid|index]\nSelect the active player (used for offline launches); no args = picker."},
        {"login",
         "用法: lpcl login [服务器] [邮箱]\n"
         "外置登录（authlib-injector，如 LittleSkin）。无参向导：服务器→邮箱→密码（掩码输入）。\n"
         "登录态加密持久化，launch 自动在线刷新。需交互终端。",
         "Usage: lpcl login [server] [email]\n"
         "External authlib-injector login (e.g. LittleSkin). No args = wizard: server → email → password (masked).\n"
         "Session encrypted & persisted; launch auto-refreshes. Requires a TTY."},
        {"logout",
         "用法: lpcl logout\n清除外置登录态，launch 回退离线玩家。",
         "Usage: lpcl logout\nClear the external login; launches fall back to the offline player."},
        {"set-folder",
         "用法: lpcl set-folder <路径>\n设置默认游戏目录（持久保存）。",
         "Usage: lpcl set-folder <path>\nSet the default game folder (persisted)."},
        {"set-lang",
         "用法: lpcl set-lang <en|zh>\n设置界面语言（持久保存）。",
         "Usage: lpcl set-lang <en|zh>\nSet the UI language (persisted)."},
        {"set-mem",
         "用法: lpcl set-mem <MB|auto>\n设置游戏最大内存；auto（默认）按可用内存 50% 分配（上限 16G）。",
         "Usage: lpcl set-mem <MB|auto>\nSet max game memory; auto (default) = 50% of available RAM (cap 16G)."},
        {"set-cf-key",
         "用法: lpcl set-cf-key <key|--clear>\n"
         "  <key>     设置自定义 CurseForge API key（加密存储，优先于编译期内嵌）\n"
         "  --clear   清除自定义 key，回退编译期内嵌/镜像\n"
         "  无参      显示当前 key 来源（不回显 key 本体）",
         "Usage: lpcl set-cf-key <key|--clear>\n"
         "  <key>     set a custom CurseForge API key (encrypted, overrides the embedded one)\n"
         "  --clear   remove the custom key, fall back to embedded/mirror\n"
         "  no args   show the current key source (never prints the key itself)"},
        {"config",
         "用法: lpcl config\n查看当前配置（版本/目录/内存/玩家/外置登录态等）。",
         "Usage: lpcl config\nShow current config (version/folder/memory/players/login state)."},
        {"report",
         "用法: lpcl report [描述]\n生成 GitHub Issue 预填链接（自动附环境信息+最近启动日志，已脱敏）。",
         "Usage: lpcl report [description]\nGenerate a prefilled GitHub issue link (env + recent launch log attached, sanitized)."},
        {"update",
         "用法: lpcl update\n检查 GitHub Releases 正式版并原地更新（仅 install.sh 安装的副本；预发布不参与）。",
         "Usage: lpcl update\nCheck GitHub Releases (stable only) and update in place (install.sh-installed copies only)."},
        {"uninstall",
         "用法: lpcl uninstall [-r]\n  -r  保留游戏目录内容（默认连游戏目录一起清空）。仅 install.sh 安装的副本可用。",
         "Usage: lpcl uninstall [-r]\n  -r  keep game folder contents (default clears them). install.sh-installed copies only."},
        {"test",
         "用法: lpcl test\n全系统自检（含真实下载冒烟）；存在 FAIL 时退出码为 1。",
         "Usage: lpcl test\nFull system self-check (incl. real downloads); exit code 1 on any FAIL."},
        {"help",
         "用法: lpcl help\n显示命令总表。lpcl <命令> -h 显示该命令的参数详解。",
         "Usage: lpcl help\nShow the command list. lpcl <command> -h shows detailed parameter help."},
        {"version",
         "用法: lpcl version\n显示版本号（git describe 注入，如 v0.1.3 / v0.1.4-beta）。",
         "Usage: lpcl version\nShow the version (injected via git describe, e.g. v0.1.3 / v0.1.4-beta)."},
    };
    for (const auto &h : helps) {
        if (cmd == QLatin1String(h.cmd)) {
            std::cout << (g_lang == CN ? h.cn : h.en) << std::endl;
            return;
        }
    }
    std::cerr << T("error:  未知命令: %1\n", "error:  unknown command: %1\n").arg(cmd).toStdString();
    printHelp();
}

static void printHelp() {
    auto out = [](const QString &s) { std::cout << s.toStdString(); };
    struct Item { QString cmdCn, cmdEn; const char *descCn, *descEn; };
    const Item items[] = {
        {"list", "list", "列出已导入的整合包实例", "List imported instances"},
        {"mods <名称>", "mods <name>", "列出实例的 Mod 及启用状态", "List mods of an instance with enabled state"},
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
        {"set-lang <en|zh>",
         "set-lang <en|zh>",
         "设置界面语言（持久保存）",
         "Set UI language (persistent)"},
        {"set-mem <MB|auto>",
         "set-mem <MB|auto>",
         "设置游戏最大内存（auto=自动分配）",
         "Set max game memory (auto = automatic)"},
        {"set-cf-key <key|--clear>",
         "set-cf-key <key|--clear>",
         "设置/清除自定义 CurseForge key（无参查看来源）",
         "Set/clear custom CurseForge key (no arg = show source)"},
        {"inpack <文件> [--r <名称>]", "inpack <file> [--r <name>]", "导入整合包", "Import modpack"},
        {"mc-install [版本]",
         "mc-install [version]",
         "下载原版 MC 版本（不带参数为最新正式版）",
         "Download a vanilla MC version (latest release without args)"},
        {"java-install <大版本>",
         "java-install <major>",
         "下载安装 Java（Adoptium JRE）",
         "Download & install Java (Adoptium JRE)"},
        {"server-install [版本] [--forge|--fabric|--neoforge [版本]] [--from 实例]",
         "server-install [version] [--forge|--fabric|--neoforge [ver]] [--from inst]",
         "下载/安装服务端（原版或加载器；--from 复制实例 mod 与配置）",
         "Install a server (vanilla or loader; --from copies instance mods/config)"},
        {"server-start <版本> [--eula]",
         "server-start <version> [--eula]",
         "前台启动服务端（控制台直通；--eula 书面同意协议）",
         "Run server in foreground (console attached; --eula accepts the EULA)"},
        {"list-rm [名称|*]",
         "list-rm [name|*]",
         "删除实例（* 清空全部；无参上下键选择）",
         "Remove instance (* for all, select with arrows without args)"},
        {"player-add <名称>",
         "player-add <name>",
         "添加玩家配置（无参进入交互向导）",
         "Add player profile (interactive wizard without args)"},
        {"player-edit [uuid|序号]",
         "player-edit [uuid|index]",
         "修改玩家配置（交互向导）",
         "Edit player profile (interactive wizard)"},
        {"player-rm [uuid|序号]",
         "player-rm [uuid|index]",
         "删除玩家配置（无参上下键选择）",
         "Remove player profile (select with arrows without args)"},
        {"player-list", "player-list", "列出玩家配置", "List player profiles"},
        {"player-select <uuid|序号>",
         "player-select <uuid|index>",
         "选择当前玩家（按列表序号或 uuid）",
         "Select current player (by index or uuid)"},
        {"login [服务器] [邮箱]",
         "login [server] [email]",
         "外置登录（authlib-injector，如 LittleSkin；登录态持久保存）",
         "External authlib-injector login (e.g. LittleSkin; session persisted)"},
        {"logout", "logout", "退出外置登录", "Log out the external account"},
        {"config", "config", "查看当前配置", "Show current configuration"},
        {"report [描述]", "report [description]", "生成 GitHub Issue 预填链接（附环境+日志）", "Create a prefilled GitHub issue link (env + logs attached)"},
        {"update", "update", "检查并更新到最新版本", "Check for and apply updates"},
        {"uninstall [-r]",
         "uninstall [-r]",
         "卸载 lpcl（-r 保留游戏目录）",
         "Uninstall lpcl (-r keeps game folder)"},
        {"test", "test", "全系统自检", "Run system self-test"},
        {"help", "help", "显示帮助信息", "Show help information"},
        {"version", "version", "显示版本号", "Show version number"},
    };
    auto printItem = [&](const Item &it) {
        out(QString("  %1 %2\n").arg(g_lang == CN ? it.cmdCn : it.cmdEn, -20)
                .arg(T(it.descCn, it.descEn)));
    };
    out(T("Usage: lpcl <command> [args]\n",
          "Usage: lpcl <command> [args]\n"));
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

    // 最大内存：0 = 自动（可用内存 50%，上限 16G）
    int maxMem = Settings::instance().getString("LaunchMaxMemory", "0").toInt();
    QString memStr = maxMem <= 0 ? _("自动", "auto") : QString("%1 MB").arg(maxMem);
    std::cout << _("游戏最大内存: ", "Max game memory: ") << memStr.toStdString() << std::endl;

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
    // 外置登录态（authlib，持久化）
    auto al = lpcl::currentAuthlibLogin();
    std::cout << _("外置登录: ", "External login: ");
    if (al.loggedIn)
        std::cout << al.name.toStdString() << " @ " << al.server.toStdString() << std::endl;
    else
        std::cout << _("（未登录）\n", "(not logged in)\n");
    return 0;
}

// ---- CF API key（指令设置 > 编译内嵌 > 镜像） ----

static int handleSetCfKey(const QStringList &args) {
    // 无参 = 状态查询（不回显 key 本体——内嵌 key 禁止透露）
    if (args.size() < 2) {
        QString src = lpcl::cfApiKeySource();
        if (src == "user")
            std::cout << _("CurseForge key: 指令设置的自定义 key（加密保存中）\n",
                           "CurseForge key: custom key set via command (stored encrypted)\n");
        else if (src == "embedded")
            std::cout << _("CurseForge key: 编译期内嵌（发布版完整体验）\n",
                           "CurseForge key: embedded at build time (full experience)\n");
        else
            std::cout << _("CurseForge key: 未设置，走 MCIM 镜像\n",
                           "CurseForge key: not set, using MCIM mirror\n");
        return 0;
    }
    if (args[1] == "--clear" || args[1] == "-") {
        lpcl::setCfApiKey("");
        std::cout << _("已清除自定义 key，回退编译期内嵌/镜像\n",
                       "Custom key cleared, falling back to embedded/mirror\n");
        return 0;
    }
    lpcl::setCfApiKey(args[1]);
    std::cout << _("已保存自定义 CurseForge key（加密存储，立即生效）\n",
                   "Custom CurseForge key saved (encrypted, effective immediately)\n");
    return 0;
}

// ---- 外置登录（authlib-injector，如 LittleSkin） ----

static int handleLogin(const QStringList &args) {
    auto cur = lpcl::currentAuthlibLogin();
    if (cur.loggedIn) {
        std::cout << T("当前已登录: %1 @ %2\n", "Currently logged in: %1 @ %2\n")
                         .arg(cur.name, cur.server).toStdString();
        if (isatty(fileno(stdin))) {
            auto again = tuiConfirm(_("重新登录？", "Log in again?"), false);
            if (!again || !*again) return 0;
        } else {
            std::cout << _("切换账号请先 lpcl logout\n", "Run lpcl logout first to switch accounts\n");
            return 0;
        }
    }

    // 位置参数可预填服务器/邮箱；密码只走交互输入（不进命令行与 shell 历史）
    QString server = args.size() >= 2 ? args[1] : QString();
    QString email  = args.size() >= 3 ? args[2] : QString();
    if (!isatty(fileno(stdin))) {
        std::cerr << _("error:  login 需要交互终端（用法: lpcl login [服务器] [邮箱]）\n",
                       "error:  login requires an interactive terminal (usage: lpcl login [server] [email])\n");
        return 1;
    }
    if (server.isEmpty()) {
        auto s = tuiInput(_("外置登录服务器", "Auth server"),
                          "https://littleskin.cn/api/yggdrasil",
                          _("如 LittleSkin", "e.g. LittleSkin"));
        if (!s) { std::cerr << _("已取消\n", "Cancelled\n"); return 1; }
        server = *s;
    }
    while (server.endsWith('/')) server.chop(1);
    if (email.isEmpty()) {
        auto e = tuiInput(_("邮箱 / 用户名", "Email / username"), {}, _("必填", "required"));
        if (!e) { std::cerr << _("已取消\n", "Cancelled\n"); return 1; }
        email = *e;
    }
    auto p = tuiPassword(_("密码", "Password"));
    if (!p) { std::cerr << _("已取消\n", "Cancelled\n"); return 1; }
    if (email.isEmpty() || p->isEmpty()) {
        std::cerr << _("error:  邮箱和密码不能为空\n", "error:  email and password must not be empty\n");
        return 1;
    }

    std::cout << _("正在登录...\n", "Logging in...\n");
    bool done = false, ok = false;
    QString msg;
    QEventLoop loop;
    QPointer<QEventLoop> guard = &loop;
    lpcl::loginAuthlib(server, email, *p,
        [&](bool o, const QString &m, const lpcl::AuthlibLoginInfo &) {
            ok = o; msg = m; done = true;
            if (guard) guard->quit();
        });
    if (!done) loop.exec();  // done 标志防同步完成导致裸等

    if (ok) {
        std::cout << T("登录成功: %1 @ %2\n启动游戏将使用此外置账号（每次启动自动在线刷新）。\n",
                       "Logged in: %1 @ %2\nLaunches will use this account (auto-refreshed online).\n")
                         .arg(msg, server).toStdString();
        return 0;
    }
    std::cerr << T("error:  登录失败: %1\n", "error:  login failed: %1\n").arg(msg).toStdString();
    return 1;
}

static int handleLogout(const QStringList &args) {
    Q_UNUSED(args);
    auto cur = lpcl::currentAuthlibLogin();
    if (!cur.loggedIn) {
        std::cout << _("当前没有外置登录态\n", "No external login to clear\n");
        return 0;
    }
    lpcl::logoutAuthlib();
    std::cout << T("已退出外置账号 %1，启动将回退为离线玩家\n",
                   "Logged out %1; launches fall back to the offline player\n")
                     .arg(cur.name).toStdString();
    return 0;
}

// ---- 服务端（本地开服） ----

// 解析 --forge/--fabric/--neoforge：可带值（指定版本）也可裸写（自动最新）
static std::optional<QString> extractLoaderFlag(QStringList &args, const QString &flag,
                                                QString &loaderType) {
    int i = args.indexOf(flag);
    if (i < 0) return std::nullopt;
    args.removeAt(i);
    loaderType = flag.mid(2);
    if (i < args.size() && !args[i].startsWith('-'))
        return args.takeAt(i);  // 显式版本
    return QString();           // 裸写：空串 = 自动最新
}

static int handleServerInstall(QStringList &args) {
    QString loaderType, loaderVer, fromInstance;
    bool hasLoader = false;
    for (const auto &f : {"--forge", "--fabric", "--neoforge"}) {
        if (auto v = extractLoaderFlag(args, f, loaderType)) {
            hasLoader = true;
            loaderVer = *v;
            break;
        }
    }
    fromInstance = extractFlag(args, "--from");

    QString ver = args.size() >= 2 ? args[1] : QString();
    std::cout << (ver.isEmpty()
        ? _("正在下载最新版 MC 服务端 ...\n", "Downloading latest MC server ...\n")
        : _(QString("正在下载 MC %1 服务端 ...\n").arg(ver).toStdString(),
            QString("Downloading MC %1 server ...\n").arg(ver).toStdString()));
    bool ok = lpcl::installServer(ver, hasLoader ? loaderType : QString(), loaderVer,
        [](const lpcl::ImportProgress &p) {
            std::cout << "  " << p.step.toStdString() << "\n";
        });
    if (!ok) {
        std::cerr << _("error:  服务端安装失败\n", "error:  server install failed\n");
        return 1;
    }

    // --from <实例>：把该实例的 mods/config/defaultconfigs 复制进服务端目录
    if (!fromInstance.isEmpty()) {
        QString dirName = Settings::instance().dirForDisplayName(fromInstance);
        if (dirName.isEmpty()) {
            std::cerr << T("error:  实例 %1 不存在（服务端已装好，未复制 mod）\n",
                           "error:  instance %1 not found (server installed, mods not copied)\n")
                             .arg(fromInstance).toStdString();
            return 1;
        }
        QString instDir = VersionManager::instance().mcFolder() + "instances/" + dirName + "/";
        // 服务端标识 = 版本[-加载器-版本]：与 installServer 内部一致；
        // --from 需要显式版本号（ver 为空时刚装的是最新正式版，无法可靠拼目录名）
        if (ver.isEmpty()) {
            std::cerr << _("error:  --from 需要显式版本号（如 lpcl server-install 1.20.1 --forge --from xxx）\n",
                           "error:  --from requires an explicit version\n");
            return 1;
        }
        QString serverId = ver;
        if (hasLoader) {
            // 重新解析一次加载器版本以拼目录名（installServer 内部解析的）
            // 简化：读取 servers/ 下以 ver-loaderType- 开头的目录
            QString prefix = ver + "-" + loaderType + "-";
            QDir sd(VersionManager::instance().mcFolder() + "servers");
            const auto entries = sd.entryList({prefix + "*"}, QDir::Dirs | QDir::NoDotAndDotDot);
            if (entries.isEmpty()) {
                std::cerr << _("error:  找不到服务端目录\n", "error:  server dir not found\n");
                return 1;
            }
            serverId = entries.first();
        }
        QString dst = VersionManager::instance().mcFolder() + "servers/" + serverId + "/";
        for (const auto &sub : {"mods", "config", "defaultconfigs"}) {
            if (QDir(instDir + sub).exists()) {
                std::cout << _("复制 ", "Copying ") << sub << " ..." << std::endl;
                FileUtils::copyDir(instDir + sub, dst + sub);
            }
        }
        std::cout << _("注意：客户端专属 mod（如 Sodium 等渲染类）会让服务端崩溃，启动失败请先删 mods/ 里的渲染/界面类 mod\n",
                       "Note: client-only mods (rendering/UI like Sodium) crash dedicated servers — remove them from mods/ if startup fails\n");
    }
    std::cout << "success" << std::endl;
    return 0;
}

static int handleServerStart(QStringList &args) {
    if (args.size() < 2) {
        std::cerr << _("error:  lpcl server-start <版本>\n", "error:  lpcl server-start <version>\n");
        return 1;
    }
    QString ver = args[1];
    QString dir = VersionManager::instance().mcFolder() + "servers/" + ver + "/";
    if (!QDir(dir).exists()) {
        std::cerr << T("error:  服务端未安装，请先 lpcl server-install %1\n",
                       "error:  server not installed, run lpcl server-install %1 first\n")
                         .arg(ver).toStdString();
        return 1;
    }

    // EULA 闸门：--eula 直接书面同意；TTY 交互确认；否则拒绝
    bool eulaFlag = args.contains("--eula");
    QFile eulaFile(dir + "eula.txt");
    bool accepted = eulaFile.open(QIODevice::ReadOnly)
                    && eulaFile.readAll().contains("eula=true");
    eulaFile.close();
    if (!accepted && eulaFlag) {
        QFile f(dir + "eula.txt");
        if (f.open(QIODevice::WriteOnly)) f.write("eula=true\n");
        accepted = true;
    }
    if (!accepted) {
        std::cout << _("Minecraft 最终用户许可协议: https://aka.ms/MinecraftEULA\n",
                       "Minecraft EULA: https://aka.ms/MinecraftEULA\n");
        if (isatty(fileno(stdin))) {
            auto yes = tuiConfirm(_("我已阅读并同意 EULA", "I have read and agree to the EULA"), false);
            if (yes && *yes) {
                QFile f(dir + "eula.txt");
                if (f.open(QIODevice::WriteOnly)) f.write("eula=true\n");
                accepted = true;
            }
        } else {
            std::cerr << _("error:  请先阅读 EULA，并以 --eula 参数表示同意\n",
                           "error:  read the EULA first, then pass --eula to accept\n");
        }
        if (!accepted) return 1;
    }

    std::cout << T("正在启动服务端 %1（控制台直通，/stop 关服）...\n",
                   "Starting server %1 (console attached, /stop to halt)...\n").arg(ver).toStdString();
    if (!lpcl::startServer(ver,
            [](const QString &line) { std::cout << line.toStdString() << "\n"; },
            [](int code) {
                std::cout << _("服务端已退出: ", "Server exited: ") << code << "\n";
                QCoreApplication::quit();
            })) {
        std::cerr << _("error:  服务端启动失败\n", "error:  failed to start server\n");
        return 1;
    }
    QCoreApplication::instance()->exec();  // 等服务端进程结束
    return 0;
}

// ---- 自身安装管理（uninstall / update） ----

// 安装根目录（install.sh 的落位）；不是该布局时拒绝卸载/更新（防止误动开发/分发副本）
static QString installedRoot() {
    QString root = QDir::homePath() + "/.local/lib/lpcl";
    QString appDir = QCoreApplication::applicationDirPath();
    return appDir.startsWith(root) ? root : QString();
}

// 删除单个文件或目录（替换/回滚共用；FileUtils::removeTree 不顺符号链接）
static void removeAny(const QString &path) {
    QFileInfo fi(path);
    if (fi.isDir() && !fi.isSymLink()) FileUtils::removeTree(path);
    else QFile::remove(path);
}

static int handleUninstall(const QStringList &args) {
    bool keepGame = args.contains("-r");  // -r：保留游戏目录内容
    QString root = installedRoot();
    if (root.isEmpty()) {
        std::cerr << _("error:  当前不是 install.sh 安装副本（开发/分发路径），拒绝卸载\n",
                       "error:  not an install.sh-installed copy (dev/dist path), refusing to uninstall\n");
        return 1;
    }

    // 1. 清空游戏目录内容（除非 -r；只清内容不删目录本身；不顺符号链接）
    if (!keepGame) {
        QString gameDir = VersionManager::instance().mcFolder();
        if (QDir(gameDir).exists()) {
            std::cout << _("正在清空游戏目录: ", "Clearing game folder: ")
                      << gameDir.toStdString() << std::endl;
            FileUtils::removeDirContents(gameDir);
        }
    }

    // 2. 删除 PATH 符号链接（指向本安装目录的才删；lpcl-cli 为改名前遗留，lpcl-gui 为 GUI 入口）
    for (const char *name : {"lpcl", "lpcl-cli", "lpcl-gui"}) {
        QString link = QDir::homePath() + "/.local/bin/" + name;
        if (QFileInfo(link).isSymLink() && QFileInfo(link).symLinkTarget().startsWith(root)) {
            std::cout << _("删除命令链接: ", "Removing command link: ") << link.toStdString() << std::endl;
            QFile::remove(link);
        }
    }

    // 3. 删除安装目录（Linux 下删除运行中的二进制是安全的，进程退出后 inode 回收）
    if (keepGame) {
        // -r：只删程序本体和配置，保留 mc/ 等其余内容
        std::cout << _("删除程序本体和配置（保留游戏内容）\n",
                       "Removing binaries and config (keeping game contents)\n");
        QFile::remove(root + "/lpcl");
        QFile::remove(root + "/lpcl-cli");            // 改名前遗留二进制
        QFile::remove(root + "/lpcl-gui");
        QFile::remove(root + "/THIRD-PARTY-NOTICES.md");
        FileUtils::removeTree(root + "/lib");       // 零依赖包收编的 Qt/第三方库
        FileUtils::removeTree(root + "/plugins");   // TLS 插件
        QFile::remove(root + "/liblpclcore.so");  // 旧动态布局遗留
        QFile::remove(root + "/LPCL.ini");
    } else {
        std::cout << _("删除安装目录: ", "Removing install dir: ") << root.toStdString() << std::endl;
        FileUtils::removeTree(root);
    }

    std::cout << _("卸载完成。\n", "Uninstall complete.\n");
    if (keepGame)
        std::cout << _("（已按 -r 保留游戏目录内容）\n", "(game folder contents kept as requested by -r)\n");
    std::cout.flush();
    // 自毁退出：跳过析构（QSettings 析构会把缓存配置重新写回 LPCL.ini，导致"删不干净"）
    _Exit(0);
}

static int handleUpdate(const QStringList &args) {
    Q_UNUSED(args);
    // 只允许更新 install.sh 安装副本（开发/分发路径下跑 update 会误替换副本二进制）
    QString root = installedRoot();
    if (root.isEmpty()) {
        std::cerr << _("error:  当前不是 install.sh 安装副本（开发/分发路径），拒绝更新\n",
                       "error:  not an install.sh-installed copy (dev/dist path), refusing to update\n");
        return 1;
    }
    // 发布仓库（与 install.sh 同一来源，可用环境变量覆盖）
    QString repo = qEnvironmentVariable("LPCL_REPO", "recallrw80-afk/LPCL");
    QString apiUrl = QString("https://api.github.com/repos/%1/releases/latest").arg(repo);

    std::cout << _("正在检查更新...\n", "Checking for updates...\n");
    bool done = false;
    int result = 1;
    QEventLoop loop;
    QPointer<QEventLoop> guard = &loop;

    DownloadManager::instance().downloadJson(apiUrl,
        [&](bool ok, QString err, nlohmann::json rel) {
        auto finish = [&](int code) { result = code; done = true; if (guard) guard->quit(); };

        if (!ok || !rel.contains("tag_name") || !rel["tag_name"].is_string()) {
            std::cerr << T("error:  检查更新失败（%1）。如仓库未公开，请先用 LPCL_REPO 配置\n",
                           "error:  update check failed (%1). If the repo is private, set LPCL_REPO first\n")
                         .arg(err.isEmpty() ? "no tag_name" : err).toStdString();
            finish(1); return;
        }

        QString remoteTag = QString::fromStdString(rel.value("tag_name", ""));
        // 版本比较：提取 vX.Y.Z 数字段
        QRegularExpression re(R"(v?(\d+\.\d+(?:\.\d+)?))");
        auto rm = re.match(remoteTag), lm = re.match(QString(GIT_DESCRIBE));
        QVersionNumber remoteVer = rm.hasMatch() ? QVersionNumber::fromString(rm.captured(1)) : QVersionNumber();
        QVersionNumber localVer = lm.hasMatch() ? QVersionNumber::fromString(lm.captured(1)) : QVersionNumber(0, 0, 0);
        if (remoteVer.isNull() || remoteVer <= localVer) {
            std::cout << _("已是最新版本: ", "Already up to date: ")
                      << QString(GIT_DESCRIBE).toStdString() << std::endl;
            finish(0); return;
        }

        // 找对应架构的包
        QString arch = QSysInfo::currentCpuArchitecture() == "aarch64" ? "aarch64" : "x86_64";
        QString pkg = "lpcl-linux-" + arch + ".tar.gz";
        QString dlUrl;
        auto assetsIt = rel.find("assets");
        if (assetsIt != rel.end() && assetsIt->is_array()) {
            for (const auto &a : *assetsIt) {
                if (!a.is_object()) continue;
                QString name = QString::fromStdString(a.value("name", ""));
                if (name == pkg) { dlUrl = QString::fromStdString(a.value("browser_download_url", "")); break; }
            }
        }
        if (dlUrl.isEmpty()) {
            std::cerr << T("error:  新版本 %1 没有 %2 架构的包\n",
                           "error:  new release %1 has no package for %2\n").arg(remoteTag, arch).toStdString();
            finish(1); return;
        }

        std::cout << T("发现新版本 %1（当前 %2），正在下载...\n",
                       "New version %1 found (current %2), downloading...\n")
                       .arg(remoteTag, QString(GIT_DESCRIBE)).toStdString();
        QString appDir = QCoreApplication::applicationDirPath();
        QString tmpDir = appDir + "/.update-tmp";
        FileUtils::removeTree(tmpDir);
        QDir().mkpath(tmpDir);
        QString pkgPath = tmpDir + "/" + pkg;
        DownloadManager::instance().download(dlUrl, pkgPath, nullptr,
            [=, &result](bool dlOk, QString dlErr) {
            if (!dlOk) {
                std::cerr << T("error:  下载失败: ", "error:  download failed: ").toStdString()
                          << dlErr.toStdString() << std::endl;
                FileUtils::removeTree(tmpDir);
                finish(1); return;
            }
            if (!FileUtils::extractTarGz(pkgPath, tmpDir)) {
                std::cerr << _("error:  解压失败\n", "error:  extract failed\n");
                FileUtils::removeTree(tmpDir);
                finish(1); return;
            }
            // 交换式替换，与发布包布局一致（lpcl + lib/ + plugins/）：
            // 旧项先移入 .update-tmp/old/ 备份，再移入新项；任一步失败整体回滚，
            // 避免出现"二进制新、库文件旧"的版本错配。同目录 rename 无跨盘问题。
            QString backupDir = tmpDir + "/old";
            QDir().mkpath(backupDir);
            const QStringList items = {"lpcl", "lib", "plugins", "THIRD-PARTY-NOTICES.md"};
            QStringList swapped;
            bool swapOk = true;
            for (const auto &item : items) {
                QString src = tmpDir + "/" + item;
                if (!QFileInfo::exists(src)) continue;  // 包内无此项则保留旧的
                QString dst = appDir + "/" + item;
                QString bak = backupDir + "/" + item;
                if (QFileInfo::exists(dst) && !QFile::rename(dst, bak)) { swapOk = false; break; }
                if (!QFile::rename(src, dst)) {
                    if (QFileInfo::exists(bak)) QFile::rename(bak, dst);
                    swapOk = false; break;
                }
                swapped << item;
            }
            if (!swapOk) {
                for (const auto &item : swapped) {  // 回滚已换入的项
                    removeAny(appDir + "/" + item);
                    QFile::rename(backupDir + "/" + item, appDir + "/" + item);
                }
                FileUtils::removeTree(tmpDir);
                std::cerr << _("error:  替换文件失败，已回滚（目录不可写？）\n",
                               "error:  failed to replace files, rolled back (dir not writable?)\n");
                finish(1); return;
            }
            FileUtils::removeTree(tmpDir);  // 含 old/ 备份与下载的压缩包
            QFile::remove(appDir + "/liblpclcore.so");  // 旧动态布局遗留（现已静态链接进主程序）
            QFile::remove(appDir + "/lpcl-cli");        // 改名前遗留二进制
            QFile(appDir + "/lpcl").setPermissions(
                QFile::permissions(appDir + "/lpcl") | QFile::ExeOwner | QFile::ExeGroup | QFile::ExeOther);
            std::cout << _("更新完成: ", "Updated to: ") << remoteTag.toStdString()
                      << _("（重启 lpcl 生效）\n", " (restart lpcl to take effect)\n");
            finish(0);
        });
    });

    if (!done) loop.exec();
    return result;
}

static int handleInstall(const QStringList &args) {
    // 不带参数 = 最新正式版（SDK 解析 latest.release）
    QString ver = args.size() >= 2 ? args[1] : QString();
    std::cout << (ver.isEmpty()
        ? _("正在下载最新版 MC ...\n", "Downloading latest MC ...\n")
        : _(QString("正在下载 MC %1 ...\n").arg(ver).toStdString(),
            QString("Downloading MC %1 ...\n").arg(ver).toStdString()));
    bool ok = lpcl::installVersion(ver,
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
        std::cerr << _("error:  lpcl java-install <大版本>\n",
                       "error:  lpcl java-install <major>\n");
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
    if (cmd == "report")       return handleReport(args);
    if (cmd == "login")        return handleLogin(args);
    if (cmd == "logout")       return handleLogout(args);
    if (cmd == "uninstall")    return handleUninstall(args);
    if (cmd == "update")       return handleUpdate(args);
    if (cmd == "set-folder")     return handleSetFolder(args);
    if (cmd == "set-lang")       return handleSetLang(args);
    if (cmd == "set-mem")        return handleSetMem(args);
    if (cmd == "set-cf-key")     return handleSetCfKey(args);
    if (cmd == "list-javas")     return handleListJavas();
    if (cmd == "player-add")     return handlePlayerAdd(args);
    if (cmd == "player-edit")    return handlePlayerEdit(args);
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
    if (cmd == "mods")     return handleMods(args);
    if (cmd == "mc-list")  return handleMcList();
    if (cmd == "mc-install") return handleInstall(args);
    if (cmd == "java-install") return handleInstallJava(args);
    if (cmd == "launch")   return handleLaunch(args);
    if (cmd == "inpack")  return handleInpack(args);
    if (cmd == "list-rm") return handleRm(args);
    if (cmd == "server-install") return handleServerInstall(args);
    if (cmd == "server-start")   return handleServerStart(args);

    return -1; // unknown
}

// ---- main ----

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    app.setApplicationName("lpcl");
    app.setApplicationVersion(GIT_DESCRIBE);

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

    // 任何命令带 -h/-help/--help 都只打印帮助（防"想看帮助却执行了真实操作"，如 update -help）
    for (int i = 1; i < args.size(); ++i) {
        if (args[i] == "-h" || args[i] == "-help" || args[i] == "--help") {
            printCommandHelp(args.at(0));
            return 0;
        }
    }

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
