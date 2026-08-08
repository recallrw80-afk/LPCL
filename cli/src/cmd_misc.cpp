// 命令处理函数组（从 main.cpp 拆分；逻辑未动）
#include "commands.h"
#include "i18n.h"
#include "tui_select.h"
#include "tui_prompt.h"
#include "lpcl.h"
#include "core/settings.h"
#include "core/versionmanager.h"
#include "download/downloadmanager.h"
#include "util/file_utils.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QPointer>
#include <QProcess>
#include <QRegularExpression>
#include <QSysInfo>
#include <QUrl>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <unistd.h>

int handleSetFolder(QStringList &args) {
    if (args.size() < 2) {
        std::cerr << _("error:  lpcl set-folder <路径>\n",
                       "error:  lpcl set-folder <path>\n");
        return 1;
    }
    Settings::instance().setString("LaunchFolderSelect", args[1]);
    std::cout << "success" << std::endl;
    return 0;
}

int handleSetLang(QStringList &args) {
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
int handleSetMem(QStringList &args) {
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

int handleReport(QStringList &args) {
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

int handleListJavas(QStringList &args) { Q_UNUSED(args);
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
int handleConfig(QStringList &args) { Q_UNUSED(args);
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

int handleSetCfKey(QStringList &args) {
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

int handleUninstall(QStringList &args) {
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

int handleUpdate(QStringList &args) {
    bool beta = args.contains("-beta") || args.contains("--beta");
    bool cn = args.contains("-cn") || args.contains("--cn");
    // 只允许更新 install.sh 安装副本（开发/分发路径下跑 update 会误替换副本二进制）
    QString root = installedRoot();
    if (root.isEmpty()) {
        std::cerr << _("error:  当前不是 install.sh 安装副本（开发/分发路径），拒绝更新\n",
                       "error:  not an install.sh-installed copy (dev/dist path), refusing to update\n");
        return 1;
    }
    // 发布源：GitHub（默认）或 Gitee（-cn，国内网络）
    QString repo = qEnvironmentVariable("LPCL_REPO", "recallrw80-afk/LPCL");
    QString giteeRepo = qEnvironmentVariable("LPCL_GITEE_REPO", "Recall_m_wxd/lpcl");
    // 默认查正式版（releases/latest 不含预发布）；-beta 走列表接口取最新一条（含预发布）
    QString apiUrl;
    if (cn) {
        apiUrl = beta
            ? QString("https://gitee.com/api/v5/repos/%1/releases?per_page=1").arg(giteeRepo)
            : QString("https://gitee.com/api/v5/repos/%1/releases/latest").arg(giteeRepo);
    } else {
        apiUrl = beta
            ? QString("https://api.github.com/repos/%1/releases?per_page=1").arg(repo)
            : QString("https://api.github.com/repos/%1/releases/latest").arg(repo);
    }

    std::cout << _("正在检查更新...\n", "Checking for updates...\n");
    bool done = false;
    int result = 1;
    QEventLoop loop;
    QPointer<QEventLoop> guard = &loop;

    DownloadManager::instance().downloadJson(apiUrl,
        [&](bool ok, QString err, nlohmann::json rel) {
        auto finish = [&](int code) { result = code; done = true; if (guard) guard->quit(); };

        // -beta 的列表接口返回数组：取最新一条
        if (ok && rel.is_array()) {
            if (!rel.empty() && rel[0].is_object()) rel = rel[0];
            else { rel = nlohmann::json::object(); }
        }

        if (!ok || !rel.contains("tag_name") || !rel["tag_name"].is_string()) {
            std::cerr << T("error:  检查更新失败（%1）。如仓库未公开，请先用 LPCL_REPO 配置\n",
                           "error:  update check failed (%1). If the repo is private, set LPCL_REPO first\n")
                         .arg(err.isEmpty() ? "no tag_name" : err).toStdString();
            finish(1); return;
        }

        QString remoteTag = QString::fromStdString(rel.value("tag_name", ""));
        // 版本比较：先比数字段；数字段相同按 SemVer 规则——正式版 > 预发布
        // （v0.1.4-beta → v0.1.4 应提示更新），双预发布按后缀字母序（beta < rc）
        QRegularExpression re(R"(v?(\d+\.\d+(?:\.\d+)?))");
        auto rm = re.match(remoteTag), lm = re.match(QString(GIT_DESCRIBE));
        QVersionNumber remoteVer = rm.hasMatch() ? QVersionNumber::fromString(rm.captured(1)) : QVersionNumber();
        QVersionNumber localVer = lm.hasMatch() ? QVersionNumber::fromString(lm.captured(1)) : QVersionNumber(0, 0, 0);
        bool hasUpdate = false;
        if (remoteVer > localVer) {
            hasUpdate = true;
        } else if (!remoteVer.isNull() && remoteVer == localVer) {
            auto suffixOf = [](const QString &tag) {
                int i = tag.indexOf('-');
                return i < 0 ? QString() : tag.mid(i + 1);
            };
            QString rs = suffixOf(remoteTag), ls = suffixOf(QString(GIT_DESCRIBE));
            hasUpdate = (rs.isEmpty() && !ls.isEmpty())          // 本地预发布 → 远端正式版
                        || (!rs.isEmpty() && !ls.isEmpty() && rs > ls);  // beta → rc 等
        }
        if (!hasUpdate) {
            std::cout << _("已是最新版本: ", "Already up to date: ")
                      << QString(GIT_DESCRIBE).toStdString() << std::endl;
            finish(0); return;
        }

        // 找对应架构的包
        QString arch = QSysInfo::currentCpuArchitecture() == "aarch64" ? "aarch64" : "x86_64";
        QString pkg = "lpcl-linux-" + arch + ".tar.xz";
        QString dlUrl;
        if (cn) {
            // Gitee 的 release JSON 不带资产直链，按固定路径规则拼接
            dlUrl = QString("https://gitee.com/%1/releases/download/%2/%3")
                        .arg(giteeRepo, remoteTag, pkg);
        } else {
            auto assetsIt = rel.find("assets");
            if (assetsIt != rel.end() && assetsIt->is_array()) {
                for (const auto &a : *assetsIt) {
                    if (!a.is_object()) continue;
                    QString name = QString::fromStdString(a.value("name", ""));
                    if (name == pkg) { dlUrl = QString::fromStdString(a.value("browser_download_url", "")); break; }
                }
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

