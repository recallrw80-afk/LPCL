// lpcl-cli — Minecraft launcher CLI frontend
// Links only liblpclcore (QtCore + QtNetwork, no GUI/QML)
//
// 维护指南：
//   新增命令只需三步：
//     1. 在 printHelp() 的 items[] 数组加一行
//     2. 写一个 static int handleXxx(const QStringList &args) 函数
//     3. 在 dispatchCommand() 中按是否需要 mcFolder 归类加入

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
        std::cerr << _("error:  lpcl-cli set-folder <路径>\n",
                       "error:  lpcl-cli set-folder <path>\n");
        return 1;
    }
    Settings::instance().setString("LaunchFolderSelect", args[1]);
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

// set-mem <MB|auto>：对应 PCL 的 LaunchRamType/LaunchRamCustom（0=自动，>0=固定 MB）
static int handleSetMem(const QStringList &args) {
    if (args.size() < 2) {
        std::cerr << _("error:  lpcl-cli set-mem <MB|auto>\n",
                       "error:  lpcl-cli set-mem <MB|auto>\n");
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
    QString mcFolder = Settings::instance().getString("LaunchFolderSelect");
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
        std::cerr << _("error:  lpcl-cli report <问题描述>\n",
                       "error:  lpcl-cli report <description>\n");
        return 1;
    }
    if (desc.isEmpty()) desc = "LPCL 问题反馈";

    QString repo = qEnvironmentVariable("LPCL_REPO", "OWNER/LPCL");
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
            std::cerr << _("error:  lpcl-cli player-add <名称> [--avatar <路径>] [--skin <slim|wide|default>]\n",
                           "error:  lpcl-cli player-add <name> [--avatar <path>] [--skin <slim|wide|default>]\n");
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
            std::cerr << _("error:  lpcl-cli player-rm <uuid|序号>\n",
                           "error:  lpcl-cli player-rm <uuid|index>\n");
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
            std::cerr << _("error:  lpcl-cli player-edit <uuid|序号>\n",
                           "error:  lpcl-cli player-edit <uuid|index>\n");
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
            std::cerr << _("error:  lpcl-cli player-select <uuid|序号>\n",
                           "error:  lpcl-cli player-select <uuid|index>\n");
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
        std::cerr << T("用法: lpcl-cli mods <实例名>\n", "usage: lpcl-cli mods <instance>\n").toStdString();
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
            std::cerr << _("error:  lpcl-cli list-rm <名称|*>\n",
                           "error:  lpcl-cli list-rm <name|*>\n");
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
        {"inpack <文件> [--r <名称>]", "inpack <file> [--r <name>]", "导入整合包", "Import modpack"},
        {"mc-install [版本]",
         "mc-install [version]",
         "下载原版 MC 版本（不带参数为最新正式版）",
         "Download a vanilla MC version (latest release without args)"},
        {"java-install <大版本>",
         "java-install <major>",
         "下载安装 Java（Adoptium JRE）",
         "Download & install Java (Adoptium JRE)"},
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
        {"config", "config", "查看当前配置", "Show current configuration"},
        {"report [描述]", "report [description]", "生成 GitHub Issue 预填链接（附环境+日志）", "Create a prefilled GitHub issue link (env + logs attached)"},
        {"update", "update", "检查并更新到最新版本", "Check for and apply updates"},
        {"uninstall [-r]",
         "uninstall [-r]",
         "卸载 lpcl-cli（-r 保留游戏目录）",
         "Uninstall lpcl-cli (-r keeps game folder)"},
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
    return 0;
}

// ---- 自身安装管理（uninstall / update） ----

// 安装根目录（install.sh 的落位）；不是该布局时拒绝卸载（防止误删开发/分发副本）
static QString installedRoot() {
    QString root = QDir::homePath() + "/.local/lib/lpcl";
    QString appDir = QCoreApplication::applicationDirPath();
    return appDir.startsWith(root) ? root : QString();
}

static int handleUninstall(const QStringList &args) {
    bool keepGame = args.contains("-r");  // -r：保留游戏目录内容
    QString root = installedRoot();
    if (root.isEmpty()) {
        std::cerr << _("error:  当前不是 install.sh 安装副本（开发/分发路径），拒绝卸载\n",
                       "error:  not an install.sh-installed copy (dev/dist path), refusing to uninstall\n");
        return 1;
    }

    // 1. 清空游戏目录内容（除非 -r；只清内容不删目录本身）
    if (!keepGame) {
        QString gameDir = VersionManager::instance().mcFolder();
        if (QDir(gameDir).exists()) {
            std::cout << _("正在清空游戏目录: ", "Clearing game folder: ")
                      << gameDir.toStdString() << std::endl;
            QDir gd(gameDir);
            for (const auto &entry : gd.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden))
                QDir(entry.absoluteFilePath()).removeRecursively();
        }
    }

    // 2. 删除 PATH 符号链接（指向本二进制的才删）
    QString link = QDir::homePath() + "/.local/bin/lpcl-cli";
    if (QFileInfo(link).isSymLink() &&
        QFileInfo(link).symLinkTarget() == QCoreApplication::applicationFilePath()) {
        std::cout << _("删除命令链接: ", "Removing command link: ") << link.toStdString() << std::endl;
        QFile::remove(link);
    }

    // 3. 删除安装目录（Linux 下删除运行中的二进制是安全的，进程退出后 inode 回收）
    if (keepGame) {
        // -r：只删程序本体和配置，保留 mc/ 等其余内容
        std::cout << _("删除程序本体和配置（保留游戏内容）\n",
                       "Removing binaries and config (keeping game contents)\n");
        QFile::remove(root + "/lpcl-cli");
        QFile::remove(root + "/liblpclcore.so");
        QFile::remove(root + "/LPCL.ini");
    } else {
        std::cout << _("删除安装目录: ", "Removing install dir: ") << root.toStdString() << std::endl;
        QDir(root).removeRecursively();
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
    // 仓库与包地址（与 install.sh 同一套占位，可用环境变量覆盖）
    QString repo = qEnvironmentVariable("LPCL_REPO", "OWNER/LPCL");
    QString apiUrl = QString("https://api.github.com/repos/%1/releases/latest").arg(repo);

    std::cout << _("正在检查更新...\n", "Checking for updates...\n");
    bool done = false;
    int result = 1;
    QEventLoop loop;
    QPointer<QEventLoop> guard = &loop;

    DownloadManager::instance().downloadJson(apiUrl,
        [&](bool ok, QString err, nlohmann::json rel) {
        auto finish = [&](int code) { result = code; done = true; if (guard) guard->quit(); };

        if (!ok || !rel.contains("tag_name")) {
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
        QString pkg = "lpcl-cli-linux-" + arch + ".tar.gz";
        QString dlUrl;
        for (const auto &a : rel["assets"]) {
            QString name = QString::fromStdString(a.value("name", ""));
            if (name == pkg) { dlUrl = QString::fromStdString(a.value("browser_download_url", "")); break; }
        }
        if (dlUrl.isEmpty()) {
            std::cerr << T("error:  新版本 %1 没有 %2 架构的包\n",
                           "error:  new release %1 has no package for %2\n").arg(remoteTag, arch).toStdString();
            finish(1); return;
        }

        std::cout << T("发现新版本 %1（当前 %2），正在下载...\n",
                       "New version %1 found (current %2), downloading...\n")
                       .arg(remoteTag, QString(GIT_DESCRIBE)).toStdString();
        QString tmpDir = QCoreApplication::applicationDirPath() + "/.update-tmp";
        QDir(tmpDir).removeRecursively();
        QDir().mkpath(tmpDir);
        QString pkgPath = tmpDir + "/" + pkg;
        DownloadManager::instance().download(dlUrl, pkgPath, nullptr,
            [=, &result](bool dlOk, QString dlErr) {
            if (!dlOk) {
                std::cerr << T("error:  下载失败: ", "error:  download failed: ").toStdString()
                          << dlErr.toStdString() << std::endl;
                QDir(tmpDir).removeRecursively();
                finish(1); return;
            }
            // 解压并原子替换（rename 覆盖运行中的二进制在 Linux 下安全）
            if (!FileUtils::extractTarGz(pkgPath, tmpDir)) {
                std::cerr << _("error:  解压失败\n", "error:  extract failed\n");
                QDir(tmpDir).removeRecursively();
                finish(1); return;
            }
            QString appDir = QCoreApplication::applicationDirPath();
            bool okBin = QFile::rename(tmpDir + "/lpcl-cli", appDir + "/lpcl-cli");
            bool okSo  = QFile::rename(tmpDir + "/liblpclcore.so", appDir + "/liblpclcore.so");
            QDir(tmpDir).removeRecursively();
            if (!okBin || !okSo) {
                std::cerr << _("error:  替换二进制失败（目录不可写？）\n",
                               "error:  failed to replace binary (dir not writable?)\n");
                finish(1); return;
            }
            QFile(appDir + "/lpcl-cli").setPermissions(
                QFile::permissions(appDir + "/lpcl-cli") | QFile::ExeOwner | QFile::ExeGroup | QFile::ExeOther);
            std::cout << _("更新完成: ", "Updated to: ") << remoteTag.toStdString()
                      << _("（重启 lpcl-cli 生效）\n", " (restart lpcl-cli to take effect)\n");
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
        std::cerr << _("error:  lpcl-cli java-install <大版本>\n",
                       "error:  lpcl-cli java-install <major>\n");
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
    if (cmd == "uninstall")    return handleUninstall(args);
    if (cmd == "update")       return handleUpdate(args);
    if (cmd == "set-folder")     return handleSetFolder(args);
    if (cmd == "set-lang")       return handleSetLang(args);
    if (cmd == "set-mem")        return handleSetMem(args);
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
