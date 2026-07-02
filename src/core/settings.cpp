#include "core/settings.h"
#include "util/crypto_utils.h"

#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include <QLoggingCategory>

static Q_LOGGING_CATEGORY(logSettings, "pcl.settings")

Settings& Settings::instance()
{
    static Settings s;
    return s;
}

Settings::Settings()
{
}

Settings::~Settings()
{
    if (m_settings) {
        m_settings->sync();
        delete m_settings;
    }
}

void Settings::initialize(const QString &configPath)
{
    auto &s = instance();
    if (s.m_settings) return; // Already initialized

    QString path = configPath;
    if (path.isEmpty()) {
        // Use app data directory
        QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        path = dataDir + "/PCL.ini";
    }

    // Ensure directory exists
    QDir dir = QFileInfo(path).absoluteDir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    s.m_settings = new QSettings(path, QSettings::IniFormat);
    // Original VB uses "PCL" for all keys
    s.m_settings->beginGroup("PCL");
    s.initDefaults();

    qCDebug(logSettings) << "Settings initialized at" << path;
}

void Settings::initDefaults()
{
    // Set default values if not present
    if (!m_settings->contains("LoginType")) {
        m_settings->setValue("LoginType", static_cast<int>(2)); // Default: Nide/Auth? No — Original default is Legacy = 0
    }
    if (!m_settings->contains("LaunchArgumentWindowType")) {
        m_settings->setValue("LaunchArgumentWindowType", 1); // Windowed
    }
    if (!m_settings->contains("LaunchArgumentPriority")) {
        m_settings->setValue("LaunchArgumentPriority", 1); // Normal
    }
    if (!m_settings->contains("LaunchArgumentRam")) {
        m_settings->setValue("LaunchArgumentRam", true);
    }
    if (!m_settings->contains("VersionRamOptimize")) {
        m_settings->setValue("VersionRamOptimize", 0); // Global setting
    }
    if (!m_settings->contains("LaunchAdvanceGC")) {
        m_settings->setValue("LaunchAdvanceGC", 0); // Auto
    }
    if (!m_settings->contains("SystemLaunchCount")) {
        m_settings->setValue("SystemLaunchCount", 0);
    }
}

// ---- Convenience methods ----

bool Settings::getBool(const QString &key, bool defaultValue) const
{
    return get<bool>(key, defaultValue);
}

void Settings::setBool(const QString &key, bool value)
{
    set<bool>(key, value);
}

int Settings::getInt(const QString &key, int defaultValue) const
{
    return get<int>(key, defaultValue);
}

void Settings::setInt(const QString &key, int value)
{
    set<int>(key, value);
}

QString Settings::getString(const QString &key, const QString &defaultValue) const
{
    return get<QString>(key, defaultValue);
}

void Settings::setString(const QString &key, const QString &value)
{
    set<QString>(key, value);
}

QVariant Settings::value(const QString &key, const QVariant &defaultValue) const
{
    return m_settings->value(key, defaultValue);
}

void Settings::setValue(const QString &key, const QVariant &value)
{
    m_settings->setValue(key, value);
    m_settings->sync();
    emit settingChanged(key);
}

// ---- Encrypted settings ----

QString Settings::getEncrypted(const QString &key, const QString &defaultValue) const
{
    QString raw = get<QString>(key);
    if (raw.isEmpty()) return defaultValue;
    return CryptoUtils::pclDecrypt(raw);
}

void Settings::setEncrypted(const QString &key, const QString &value)
{
    set<QString>(key, CryptoUtils::pclEncrypt(value));
}

// ---- Instance isolation ----

QString Settings::getInstance(const QString &instanceId, const QString &key,
                                const QString &defaultValue) const
{
    if (instanceId.isEmpty()) return getString(key, defaultValue);
    QString instanceKey = QString("Instance_%1/%2").arg(instanceId, key);
    QString val = getString(instanceKey);
    return val.isEmpty() ? defaultValue : val;
}

void Settings::setInstance(const QString &instanceId, const QString &key, const QString &value)
{
    if (instanceId.isEmpty()) {
        setString(key, value);
    } else {
        QString instanceKey = QString("Instance_%1/%2").arg(instanceId, key);
        setString(instanceKey, value);
    }
}

QString Settings::instancePath(const QString &instanceId) const
{
    if (instanceId.isEmpty()) return getString("LaunchFolderSelect");
    // Per-instance path: base .minecraft / versions / instanceId
    QString base = getString("LaunchFolderSelect");
    if (base.isEmpty()) base = QDir::homePath() + "/.minecraft/";
    return base + "versions/" + instanceId + "/";
}
