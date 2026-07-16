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
        javaList = javaMgr.javaList();
    }
    if (javaList.isEmpty()) {
        setState(LaunchState::Failed);
        emit launchFailed("No Java runtime found. Please install Java.");
        return false;
    }

    // Pick best Java for this version
    JavaEntry bestJava = javaList.first();
    int targetMajor = version.vanillaVersion.majorVersion() > 0
        ? version.vanillaVersion.majorVersion() : 8;
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
    options.fullscreen = Settings::instance().getString("LaunchFullscreen", "False") == "True";
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
        cmdLog += " " + arg;
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
    env.insert("APPDATA", gameDir);

    // Minecraft-specific env
    env.insert("MINECRAFT_LAUNCHER_NAME", "LPCL");
    env.insert("MINECRAFT_LAUNCHER_VERSION", "0.1");

    m_gameProcess->setProcessEnvironment(env);
    m_gameProcess->setWorkingDirectory(gameDir);
    m_gameProcess->setProgram(javaExe);
    m_gameProcess->setArguments(allArgs);

    // Start asynchronously. The Running state and gameStarted() signal are
    // emitted from onGameStarted() (connected to QProcess::started) so we
    // never block the Qt event loop. Start failures arrive via errorOccurred.
    m_gameProcess->start();
}

void Launcher::onGameStarted() {
    setState(LaunchState::Running);
    setStatus("Game running...");
    emit gameStarted();

    qCInfo(logLaunch) << "Game process started, PID:" << m_gameProcess->processId();
}

void Launcher::interrupt() {
    if (m_state == LaunchState::Running) {
        m_gameProcess->kill();
        setState(LaunchState::Interrupted);
        setStatus("Interrupted");
        appendLog("[LPCL] Game process killed by user.");
    } else if (m_state == LaunchState::Launching || m_state == LaunchState::Downloading) {
        setState(LaunchState::Interrupted);
        setStatus("Interrupted");
    }
}

// Game output

void Launcher::onGameStdout() {
    QByteArray data = m_gameProcess->readAllStandardOutput();
    m_logBuffer += QString::fromUtf8(data);
    QStringList lines = m_logBuffer.split('\n');
    // 最后一段是不完整的行，保留到下次
    m_logBuffer = lines.takeLast();
    for (const auto &line : lines) {
        QString trimmed = line.trimmed();
        if (!trimmed.isEmpty()) {
            emit gameLog(trimmed);
            appendLog(trimmed);
        }
    }
}

void Launcher::onGameStderr() {
    QByteArray data = m_gameProcess->readAllStandardError();
    m_logBuffer += QString::fromUtf8(data);
    QStringList lines = m_logBuffer.split('\n');
    m_logBuffer = lines.takeLast();
    for (const auto &line : lines) {
        QString trimmed = line.trimmed();
        if (!trimmed.isEmpty()) {
            emit gameLog(trimmed);
            appendLog(trimmed);
        }
    }
}

void Launcher::onGameFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    // 用户主动中断时不再覆盖状态
    if (m_state == LaunchState::Interrupted) {
        emit gameExited(exitCode, "Interrupted by user");
        return;
    }
    if (exitStatus == QProcess::CrashExit) {
        appendLog(QString("[LPCL] Game crashed with exit code %1").arg(exitCode));
        setState(LaunchState::Failed);
        emit gameExited(exitCode, "Game crashed");
    } else {
        appendLog(QString("[LPCL] Game exited with code %1").arg(exitCode));
        setState(LaunchState::Finished);
        setStatus("Game exited");
        emit gameExited(exitCode, "Game exited normally");
    }
}

void Launcher::onGameError(QProcess::ProcessError error) {
    QString errMsg;
    switch (error) {
    case QProcess::FailedToStart:
        errMsg = "Failed to start game process. Check Java installation.";
        break;
    case QProcess::Crashed:
        errMsg = "Game process crashed.";
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
