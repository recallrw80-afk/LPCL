#include "core/launchbuilder.h"
#include "core/settings.h"
#include "core/versionmanager.h"
#include "util/arg_utils.h"

#include <QDir>
#include <QLoggingCategory>
#include <QRegularExpression>

static Q_LOGGING_CATEGORY(logBuild, "pcl.launchbuilder")

LaunchBuilder& LaunchBuilder::instance()
{
    static LaunchBuilder b;
    return b;
}

void LaunchBuilder::setVersionJson(const json &versionJson)
{
    m_versionJson = versionJson;
}

void LaunchBuilder::setLaunchOptions(const McLaunchOptions &options)
{
    m_options = options;
}

// ============================================================================
// Main build method
// ============================================================================

bool LaunchBuilder::build(const McVersion &version,
                           const JavaEntry &java,
                           const LoginResult &login)
{
    m_jvmArgs.clear();
    m_gameArgs.clear();
    m_mainClass.clear();

    if (m_versionJson.is_discarded() || m_versionJson.is_null()) {
        qCWarning(logBuild) << "No version JSON set";
        return false;
    }

    // Get main class
    if (m_versionJson.contains("mainClass")) {
        m_mainClass = QString::fromStdString(m_versionJson["mainClass"].get<std::string>());
    }

    // Build args
    m_jvmArgs = buildJvmArgs(version, java);
    m_gameArgs = buildGameArgs(version, login);

    // Build replacements and apply
    QMap<QString, QString> repl = buildReplacements(version, java, login);

    qCInfo(logBuild) << "=== Launch Arguments Begin ===";
    for (const auto &arg : applyReplacements(m_jvmArgs, repl)) {
        qCInfo(logBuild).noquote() << "[JVM]" << arg;
    }
    qCInfo(logBuild).noquote() << "[MAIN]" << m_mainClass;
    for (const auto &arg : applyReplacements(m_gameArgs, repl)) {
        qCInfo(logBuild).noquote() << "[GAME]" << arg;
    }
    qCInfo(logBuild) << "=== Launch Arguments End ===";

    return true;
}

QString LaunchBuilder::commandLine() const
{
    auto repl = QMap<QString, QString>(); // No replacements for display
    QStringList all;
    all.append(applyReplacements(m_jvmArgs, repl));
    all.append(m_mainClass);
    all.append(applyReplacements(m_gameArgs, repl));
    return all.join(' ');
}

// ============================================================================
// JVM arguments
// ============================================================================

QStringList LaunchBuilder::buildJvmArgs(const McVersion &version, const JavaEntry &java)
{
    QStringList args;

    // Read JVM args from version JSON (new format)
    if (m_versionJson.contains("arguments") && m_versionJson["arguments"].contains("jvm")) {
        for (const auto &arg : m_versionJson["arguments"]["jvm"]) {
            if (arg.is_string()) {
                args.append(QString::fromStdString(arg.get<std::string>()));
            } else if (arg.is_object()) {
                // Check rules
                if (arg.contains("rules") && !checkRules(arg["rules"])) continue;
                // Get value(s)
                if (arg.contains("value")) {
                    if (arg["value"].is_string()) {
                        args.append(QString::fromStdString(arg["value"].get<std::string>()));
                    } else if (arg["value"].is_array()) {
                        for (const auto &v : arg["value"]) {
                            args.append(QString::fromStdString(v.get<std::string>()));
                        }
                    }
                }
            }
        }
    } else {
        // Old format: fixed JVM args
        qCInfo(logBuild) << "Using legacy JVM args";
        args.append("-XX:HeapDumpPath=MojangTricksIntelDriversForPerformance_javaw.exe_minecraft.exe.heapdump");
        args.append("-Djava.library.path=${natives_directory}");
        args.append("-cp");
        args.append("${classpath}");
    }

    // Custom JVM args from settings
    QString customArg = Settings::instance().getInstance(version.id, "VersionAdvanceJvm");
    if (customArg.isEmpty()) {
        customArg = Settings::instance().getString("LaunchAdvanceJvm");
    }
    if (!customArg.isEmpty()) {
        args.append(ArgUtils::splitJavaArgs(customArg));
    }

    // Memory allocation
    int maxMemMB = m_options.maxMemoryMB;
    if (maxMemMB <= 0) maxMemMB = 4096;
    args.append(QString("-Xmx%1m").arg(maxMemMB));

    int minMemMB = m_options.minMemoryMB;
    if (minMemMB > 0) {
        args.append(QString("-Xms%1m").arg(minMemMB));
    }

    // GC selection
    int gcType = Settings::instance().getInstance(version.id, "VersionAdvanceGC", "0").toInt();
    if (gcType <= 0) gcType = Settings::instance().getInt("LaunchAdvanceGC");
    if (gcType != 3) { // 3 = custom/user-defined
        bool useG1GC = false;
        if ((gcType == 0 && java.majorVersion < 15) ||
            (gcType == 1 && java.majorVersion < 21) ||
            (gcType == 2 || gcType == 4)) {
            useG1GC = true;
        }

        // Remove existing GC args
        QRegularExpression gcArgRe(R"(^-XX:[+-]?(Use\w+GC|ZGenerational|UseCompactObjectHeaders|G1\w+Percent|G1\w+Size|MaxGCPauseMillis|MinHeapFreeRatio))");
        args.removeIf([&](const QString &a) { return gcArgRe.match(a).hasMatch(); });

        if (useG1GC) {
            args.append("-XX:+UseG1GC");
            if (gcType == 4) {
                // Optimized G1GC
                args.append("-XX:G1NewSizePercent=20");
                args.append("-XX:G1ReservePercent=20");
                args.append("-XX:G1HeapRegionSize=32M");
                args.append("-XX:MaxGCPauseMillis=50");
            }
        } else {
            args.append("-XX:+UseZGC");
            if (java.majorVersion >= 21) {
                args.append("-XX:+ZGenerational");
            }
        }
    }

    // Compact object headers (Java 24+, 64-bit)
    if (java.majorVersion >= 24 && java.is64Bit) {
        args.append("-XX:+UseCompactObjectHeaders");
    }

    return ArgUtils::deduplicateArgs(args);
}

// ============================================================================
// Game arguments
// ============================================================================

QStringList LaunchBuilder::buildGameArgs(const McVersion &version, const LoginResult &login)
{
    Q_UNUSED(login)
    QStringList args;

    // New format: arguments.game
    if (m_versionJson.contains("arguments") && m_versionJson["arguments"].contains("game")) {
        for (const auto &arg : m_versionJson["arguments"]["game"]) {
            if (arg.is_string()) {
                args.append(QString::fromStdString(arg.get<std::string>()));
            } else if (arg.is_object()) {
                if (arg.contains("rules") && !checkRules(arg["rules"])) continue;
                if (arg.contains("value")) {
                    if (arg["value"].is_string()) {
                        args.append(QString::fromStdString(arg["value"].get<std::string>()));
                    } else if (arg["value"].is_array()) {
                        for (const auto &v : arg["value"]) {
                            args.append(QString::fromStdString(v.get<std::string>()));
                        }
                    }
                }
            }
        }
    } else if (m_versionJson.contains("minecraftArguments")) {
        // Old format: minecraftArguments string
        QString oldArgs = QString::fromStdString(m_versionJson["minecraftArguments"].get<std::string>());
        args.append(ArgUtils::splitJavaArgs(oldArgs));
        // Always add resolution args
        args.append("--height");
        args.append("${resolution_height}");
        args.append("--width");
        args.append("${resolution_width}");
    }

    // Custom game args from settings
    QString customArg = Settings::instance().getInstance(version.id, "VersionAdvanceGame");
    if (customArg.isEmpty()) {
        customArg = Settings::instance().getString("LaunchAdvanceGame");
    }
    if (!customArg.isEmpty()) {
        args.append(ArgUtils::splitJavaArgs(customArg));
    }

    // Extra args from launch options
    args.append(m_options.extraGameArgs);

    // Fullscreen
    if (m_options.fullscreen) {
        args.append("--fullscreen");
    }

    // Quick join server
    if (!m_options.serverIp.isEmpty()) {
        if (version.releaseTime > QDateTime(QDate(2023, 4, 4), QTime(0, 0))) {
            args.append("--quickPlayMultiplayer");
            args.append(m_options.serverIp);
        } else {
            args.append("--server");
            if (m_options.serverIp.contains(':')) {
                args.append(m_options.serverIp.section(':', 0, 0));
                args.append("--port");
                args.append(m_options.serverIp.section(':', 1, 1));
            } else {
                args.append(m_options.serverIp);
                args.append("--port");
                args.append("25565");
            }
        }
    }

    return ArgUtils::deduplicateArgs(args);
}

// ============================================================================
// Replacements
// ============================================================================

QMap<QString, QString> LaunchBuilder::buildReplacements(const McVersion &version,
                                                         const JavaEntry &java,
                                                         const LoginResult &login)
{
    QMap<QString, QString> r;

    // Auth
    r["auth_player_name"] = login.name;
    r["auth_uuid"] = login.uuid;
    r["auth_access_token"] = login.accessToken;
    r["auth_session"] = login.accessToken; // Legacy alias
    r["auth_xuid"] = ""; // Xbox User ID (for Minecraft Bedrock)
    r["user_type"] = login.type == "Ms" ? "msa" : "legacy";
    r["user_properties"] = "{}";
    if (!login.profileJson.isEmpty()) {
        r["user_properties"] = login.profileJson;
    }

    // Version
    r["version_name"] = version.id;
    r["version_type"] = version.type;

    // Launcher
    r["launcher_name"] = "LPCL";
    r["launcher_version"] = "0.1";
    r["clientid"] = login.clientToken.isEmpty() ? "0" : login.clientToken;

    // Paths
    r["game_directory"] = version.pathIndie;
    r["game_assets"] = version.pathIndie + "assets/";

    // Assets
    QString assetsIndex = "legacy";
    if (m_versionJson.contains("assets")) {
        assetsIndex = QString::fromStdString(m_versionJson["assets"].get<std::string>());
    }
    if (m_versionJson.contains("assetIndex") && m_versionJson["assetIndex"].contains("id")) {
        assetsIndex = QString::fromStdString(m_versionJson["assetIndex"]["id"].get<std::string>());
    }
    r["assets_index_name"] = assetsIndex;
    r["assets_root"] = version.pathIndie + "assets/";

    // Classpath
    QStringList classpathParts;
    classpathParts.append(version.pathVersion + version.id + ".jar");
    // Libraries would be added here from the version JSON
    if (m_versionJson.contains("libraries")) {
        for (const auto &lib : m_versionJson["libraries"]) {
            if (lib.contains("rules") && !checkRules(lib["rules"])) continue;
            // Try to build the library path
            if (lib.contains("downloads") && lib["downloads"].contains("artifact")) {
                std::string libPath = lib["downloads"]["artifact"]["path"].get<std::string>();
                classpathParts.append(version.pathIndie + "libraries/" +
                                       QString::fromStdString(libPath));
            }
        }
    }
    QString classpathSep = (currentPlatform() == Platform::Windows) ? ";" : ":";
    r["classpath"] = classpathParts.join(classpathSep);
    r["classpath_separator"] = classpathSep;

    // Natives
    r["natives_directory"] = version.pathVersion + "natives/";

    // Resolution
    r["resolution_width"] = QString::number(m_options.windowWidth);
    r["resolution_height"] = QString::number(m_options.windowHeight);

    // Java
    r["library_directory"] = version.pathIndie + "libraries/";

    return r;
}

QStringList LaunchBuilder::applyReplacements(const QStringList &args,
                                              const QMap<QString, QString> &repl) const
{
    QStringList result;
    for (const auto &arg : args) {
        QString a = arg;
        for (auto it = repl.begin(); it != repl.end(); ++it) {
            a.replace("${" + it.key() + "}", it.value());
        }
        // Quote args with special characters
        if (a.contains(QRegularExpression("[&|<>^ ]")) &&
            !a.startsWith('"') && !a.endsWith('"')) {
            a = "\"" + a.replace('"', "\\\"") + "\"";
        }
        result.append(a);
    }
    return result;
}

// ============================================================================
// Rule checking
// ============================================================================

bool LaunchBuilder::checkRules(const json &rules)
{
    if (!rules.is_array() || rules.empty()) return true; // No rules = allow

    bool allowed = false;

    for (const auto &rule : rules) {
        bool ruleAllows = (rule.value("action", "allow") == "allow");

        // If no conditions, the rule matches
        bool matches = true;

        if (rule.contains("os")) {
            matches = false;
            auto &os = rule["os"];
            std::string osName = os.value("name", "");
            std::string osArch = os.value("arch", "");

            bool osMatch = true;
            if (!osName.empty()) {
                osMatch = (osName == platformName().toStdString());
            }
            if (!osArch.empty()) {
                osMatch = osMatch && (osArch == (is64BitSystem() ? "x86_64" : "x86"));
            }
            matches = osMatch;
        }

        if (rule.contains("features")) {
            matches = matches && checkFeatures(rule["features"]);
        }

        if (matches) {
            allowed = ruleAllows;
        }
    }

    return allowed;
}

bool LaunchBuilder::checkFeatures(const json &features)
{
    // Check feature flags
    // For now, common features:
    // - "is_demo_user": false (we're not a demo user)
    // - "has_custom_resolution": true (if windowed mode)
    // - "has_quick_plays_support": true for 1.20+
    if (!features.is_object()) return true;

    if (features.contains("is_demo_user")) {
        if (features["is_demo_user"].get<bool>()) return false;
    }

    if (features.contains("has_custom_resolution")) {
        // We always support custom resolution
    }

    return true;
}
