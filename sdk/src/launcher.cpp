#include "core/launcher.h"
#include "core/launchbuilder.h"
#include "core/versionmanager.h"
#include "core/settings.h"
#include "core/javamanager.h"

#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QStandardPaths>

static Q_LOGGING_CATEGORY(logLaunch, "lpcl.launcher")

Launcher& Launcher::instance() {
    static Launcher l;
    return l;
}

Launcher::Launcher() {
    m_gameProcess = new QProcess(this);
    m_gameProcess->setProcessChannelMode(QProcess::SeparateChannels);

    connect(m_gameProcess, &QProcess::readyReadStandardOutput,
            this, &Launcher::onGameStdout);
    connect(m_gameProcess, &QProcess::readyReadStandardError,
            this, &Launcher::onGameStderr);
    connect(m_gameProcess, &QProcess::started,
            this, &Launcher::onGameStarted);
    connect(m_gameProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &Launcher::onGameFinished);
    connect(m_gameProcess, &QProcess::errorOccurred,
            this, &Launcher::onGameError);
}

// Launch

bool Launcher::launch(const McVersion &version,
                       const JavaEntry &java,
                       const LoginResult &login,
                       const McLaunchOptions &options) {
    if (m_state == LaunchState::Running || m_state == LaunchState::Launching) {
        qCWarning(logLaunch) << "Game already running or launching";
        return false;
    }

    // Validate
    if (!QFileInfo::exists(java.pathJava)) {
        qCWarning(logLaunch) << "Java not found:" << java.pathJava;
        setState(LaunchState::Failed);
        emit launchFailed("Java not found: " + java.pathJava);
        return false;
    }

    m_version = version;
    m_java = java;
    m_login = login;
    m_options = options;

    // Load version JSON
    setState(LaunchState::Prechecking);
    setStatus("Loading version JSON...");

    // Resolve inheritance chain
    json versionJson = VersionManager::resolveInheritanceChain(
        m_version.pathVersion + m_version.id + ".json");

    LaunchBuilder::instance().setVersionJson(versionJson);
    LaunchBuilder::instance().setLaunchOptions(m_options);

    setState(LaunchState::Launching);
    setStatus("Building launch arguments...");

    if (!LaunchBuilder::instance().build(m_version, m_java, m_login)) {
        setState(LaunchState::Failed);
        emit launchFailed("Failed to build launch arguments");
        return false;
    }

    doLaunch();
    return true;
}

bool Launcher::launchVersion(const QString &versionId, const LoginResult &login) {
    if (m_state == LaunchState::Running || m_state == LaunchState::Launching) {
        qCWarning(logLaunch) << "Game already running or launching";
        return false;
    }

    // 1. Load version
    setState(LaunchState::Prechecking);
    setStatus("正在检查版本...");
    setProgress(5);

    auto &verMgr = VersionManager::instance();
    McVersion version = verMgr.loadVersion(versionId);
    if (!version.isValid) {
        setState(LaunchState::Failed);
        emit launchFailed("Cannot load version: " + versionId);
        return false;
    }

    // 2. Get Java
    setState(LaunchState::GettingJava);
    setStatus("正在查找 Java...");
    setProgress(10);

    auto &javaMgr = JavaManager::instance();
    auto javaList = javaMgr.javaList();
    if (javaList.isEmpty()) {
        javaMgr.scanSystemJava();
        javaMgr.waitForScanFinished();  // 扫描是异步的，读结果前必须等完成
        javaList = javaMgr.javaList();
    }
    if (javaList.isEmpty()) {
        setState(LaunchState::Failed);
        emit launchFailed("No Java runtime found. Please install Java.");
        return false;
    }

    // Pick best Java for this version
    JavaEntry bestJava = javaList.first();
    // MC 1.x 的特性版本号在第二段（"1.20.1" → 20），majorVersion() 恒为 1
    int feature = version.vanillaVersion.majorVersion() == 1
        ? version.vanillaVersion.minorVersion()
        : version.vanillaVersion.majorVersion();
    int targetMajor = feature > 0 ? feature : 8;
    for (const auto &j : javaList) {
        if (j.majorVersion >= targetMajor && j.is64Bit == is64BitSystem()) {
            bestJava = j;
            break;
        }
    }

    // 3. Login state is supplied by the caller — no hardcoded offline assumption.
    setState(LaunchState::LoggingIn);
    setStatus("正在设置登录...");
    setProgress(15);

    // 4. Build launch options from settings
    McLaunchOptions options;
    options.maxMemoryMB = Settings::instance().getString("LaunchMaxMemory", "4096").toInt();
    options.minMemoryMB = Settings::instance().getString("LaunchMinMemory", "512").toInt();
    QString fsValue = Settings::instance().getString("LaunchFullscreen", "false").toLower();
    options.fullscreen = (fsValue == "true" || fsValue == "1");
    options.windowWidth = Settings::instance().getString("LaunchWidth", "854").toInt();
    options.windowHeight = Settings::instance().getString("LaunchHeight", "480").toInt();

    // 5. Delegate to the full launch method
    return launch(version, bestJava, login, options);
}

void Launcher::doLaunch() {
    setState(LaunchState::Launching);
    setStatus("Starting game process...");

    QString javaExe = m_java.pathJava;

    // Build the full command
    QStringList allArgs;
    auto repl = LaunchBuilder::instance().buildReplacements(m_version, m_java, m_login);
    allArgs.append(LaunchBuilder::instance().applyReplacements(
        LaunchBuilder::instance().jvmArgs(), repl));
    allArgs.append(LaunchBuilder::instance().mainClass());
    allArgs.append(LaunchBuilder::instance().applyReplacements(
        LaunchBuilder::instance().gameArgs(), repl));

    // Log the command
    QString cmdLog = javaExe;
    for (const auto &arg : allArgs) {
        // 仅显示用：含空格的参数加引号（exec 参数本身保持原样）
        cmdLog += " " + (arg.contains(' ') ? "\"" + arg + "\"" : arg);
    }
    qCInfo(logLaunch) << "Launch command:" << cmdLog.toUtf8().constData();
    appendLog("> " + cmdLog);
    appendLog("");

    // Set up environment
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

    // PATH: add Java bin
    QString pathVar = env.value("PATH");
    QString javaBinDir = QDir(m_java.pathFolder).absolutePath();
    if (currentPlatform() == Platform::Windows) {
        pathVar = javaBinDir + ";" + pathVar;
    } else {
        pathVar = javaBinDir + ":" + pathVar;
    }
    env.insert("PATH", pathVar);

    // APPDATA / game directory
    QString gameDir = m_version.pathIndie;
    // Remove trailing slash
    if (gameDir.endsWith('/') || gameDir.endsWith('\\')) {
        gameDir.chop(1);
    }
    // APPDATA 仅 Windows 下 MC 用于定位 .minecraft，其他平台无意义
    if (currentPlatform() == Platform::Windows)
        env.insert("APPDATA", gameDir);

    // Minecraft-specific env
    env.insert("MINECRAFT_LAUNCHER_NAME", "LPCL");
    env.insert("MINECRAFT_LAUNCHER_VERSION", "0.1");

    // 无显示环境（无 GUI）：用 xvfb-run 虚拟显示包装启动
    QString program = javaExe;
    QStringList finalArgs = allArgs;
    bool headless = qEnvironmentVariableIsEmpty("DISPLAY") &&
                    qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY");
    if (headless) {
        QString xvfb = QStandardPaths::findExecutable("xvfb-run");
        if (!xvfb.isEmpty()) {
            appendLog("[LPCL] 无显示环境，使用 xvfb 虚拟显示启动");
            program = xvfb;
            finalArgs.prepend(javaExe);
            finalArgs.prepend("-a");  // 自动分配 display 号
        } else {
            qCWarning(logLaunch) << "无显示环境且未找到 xvfb-run——"
                    "游戏需要 X 显示才能创建窗口，请安装 xvfb 或配置 DISPLAY";
        }
    }

    m_gameProcess->setProcessEnvironment(env);
    m_gameProcess->setWorkingDirectory(gameDir);
    m_gameProcess->setProgram(program);
    m_gameProcess->setArguments(finalArgs);

    // Start asynchronously. The Running state and gameStarted() signal are
    // emitted from onGameStarted() (connected to QProcess::started) so we
    // never block the Qt event loop. Start failures arrive via errorOccurred.
    m_gameProcess->start();
}

void Launcher::onGameStarted() {
    // 中断竞态：start 已异步发出，started 信号可能在 interrupt() 之后到达——不得覆盖中断状态
    if (m_state == LaunchState::Interrupted) return;
    setState(LaunchState::Running);
    setStatus("Game running...");
    emit gameStarted();

    qCInfo(logLaunch) << "Game process started, PID:" << m_gameProcess->processId();
}

void Launcher::interrupt() {
    if (m_state == LaunchState::Running || m_state == LaunchState::Launching) {
        // Launching 状态下进程可能已异步 start——必须真的 kill，否则取消无效
        if (m_gameProcess->state() != QProcess::NotRunning)
            m_gameProcess->kill();
        setState(LaunchState::Interrupted);
        setStatus("Interrupted");
        appendLog("[LPCL] Game process killed by user.");
    } else if (m_state == LaunchState::Downloading) {
        setState(LaunchState::Interrupted);
        setStatus("Interrupted");
    }
}

// Game output

void Launcher::processGameOutput(QByteArray &buffer, const QByteArray &data) {
    buffer += data;
    // 字节级缓冲：多字节 UTF-8 字符跨 readyRead 边界时不会切碎（'\n' 不会出现在多字节序列中）
    int idx;
    while ((idx = buffer.indexOf('\n')) != -1) {
        QString line = QString::fromUtf8(buffer.left(idx)).trimmed();
        buffer.remove(0, idx + 1);
        if (!line.isEmpty()) {
            emit gameLog(line);
            appendLog(line);
        }
    }
}

void Launcher::flushLogBuffer(QByteArray &buffer) {
    QString line = QString::fromUtf8(buffer).trimmed();
    buffer.clear();
    if (!line.isEmpty()) {
        emit gameLog(line);
        appendLog(line);
    }
}

void Launcher::onGameStdout() {
    processGameOutput(m_logBuffer, m_gameProcess->readAllStandardOutput());
}

void Launcher::onGameStderr() {
    processGameOutput(m_logBufferErr, m_gameProcess->readAllStandardError());
}

void Launcher::onGameFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    // 冲刷两通道中无换行结尾的残留日志
    flushLogBuffer(m_logBuffer);
    flushLogBuffer(m_logBufferErr);

    // 用户主动中断时不再覆盖状态
    if (m_state == LaunchState::Interrupted) {
        emit gameExited(exitCode, "Interrupted by user");
        return;
    }
    if (exitStatus == QProcess::CrashExit) {
        appendLog(QString("[LPCL] Game crashed with exit code %1").arg(exitCode));
        setState(LaunchState::Failed);
        emit gameExited(exitCode, "Game crashed");
    } else if (exitCode != 0) {
        // JVM 正常启动但以非零码退出（参数错误、mainClass 缺失等）——不是"正常退出"
        appendLog(QString("[LPCL] Game exited abnormally with code %1").arg(exitCode));
        setState(LaunchState::Failed);
        emit gameExited(exitCode, "Game exited abnormally");
    } else {
        appendLog(QString("[LPCL] Game exited with code %1").arg(exitCode));
        setState(LaunchState::Finished);
        setStatus("Game exited");
        emit gameExited(exitCode, "Game exited normally");
    }
}

void Launcher::onGameError(QProcess::ProcessError error) {
    if (error == QProcess::Crashed) {
        // 崩溃时 QProcess 还会发 finished()——终态统一由 onGameFinished 上报，
        // 避免 launchFailed + gameExited 双触发（且运行中崩溃不是"启动失败"）
        return;
    }
    QString errMsg;
    switch (error) {
    case QProcess::FailedToStart:
        errMsg = "Failed to start game process. Check Java installation.";
        break;
    case QProcess::Timedout:
        errMsg = "Game process timed out.";
        break;
    default:
        errMsg = "Game process error: " + m_gameProcess->errorString();
        break;
    }

    appendLog("[LPCL] ERROR: " + errMsg);
    qCWarning(logLaunch) << errMsg;
    setState(LaunchState::Failed);
    emit launchFailed(errMsg);
}

// State management

void Launcher::setState(LaunchState newState) {
    if (m_state == newState) return;
    m_state = newState;
    emit stateChanged();

    if (newState == LaunchState::Failed) {
        setStatus("Failed");
        m_progress = 0;
        emit progressChanged();
    } else if (newState == LaunchState::Finished) {
        setStatus("Finished");
        m_progress = 100;
        emit progressChanged();
    }
}

void Launcher::setStatus(const QString &text) {
    m_statusText = text;
    emit statusTextChanged();
}

void Launcher::setProgress(int value) {
    if (m_progress == value) return;
    m_progress = value;
    emit progressChanged();
}

void Launcher::appendLog(const QString &line) {
    // Log to Qt logging system
    static QLoggingCategory logCat("lpcl.game");
    qCInfo(logCat).noquote() << line;

    // The UI can connect to gameLog() signal for display
}
