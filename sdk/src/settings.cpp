#include "core/settings.h"
#include "util/crypto_utils.h"

#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include <QLoggingCategory>
#include <QUuid>

static Q_LOGGING_CATEGORY(logSettings, "lpcl.settings")

Settings& Settings::instance() {
    static Settings s;
    return s;
}

Settings::Settings() {
}

Settings::~Settings() {
    if (m_settings) {
        m_settings->sync();
        delete m_settings;
    }
}

void Settings::initialize(const QString &configPath) {
    auto &s = instance();
    if (s.m_settings) return; // Already initialized

    QString path = configPath;
    if (path.isEmpty()) {
        // Use app data directory
        path = QCoreApplication::applicationDirPath() + "/LPCL.ini";
    }

    // Ensure directory exists
    QDir dir = QFileInfo(path).absoluteDir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    s.m_settings = new QSettings(path, QSettings::IniFormat);
    // 设置组名
    s.m_settings->beginGroup("LPCL");
    s.initDefaults();

    qCDebug(logSettings) << "Settings initialized at" << path;
}

void Settings::initDefaults() {
    // 默认游戏目录：可执行文件旁边的 mc/ 目录
    if (!m_settings->contains("LaunchFolderSelect")) {
        QString defaultMc = QCoreApplication::applicationDirPath() + "/mc/";
        m_settings->setValue("LaunchFolderSelect", defaultMc);
    }
    if (!m_settings->contains("LoginType")) {
        m_settings->setValue("LoginType", static_cast<int>(0)); // Default: Legacy (offline)
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

    // 默认玩家配置（首次运行时无任何 Profile 则自动创建）
    m_settings->beginGroup("Profile");
    bool hasProfiles = !m_settings->childGroups().isEmpty();
    m_settings->endGroup();
    if (!hasProfiles) {
        QString uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
        m_settings->setValue("Profile/" + uuid + "/Name", "Player");
        m_settings->setValue("Profile/" + uuid + "/SkinType", "slim");
        m_settings->setValue("SelectedPlayer", uuid);
    }
}

// ---- Convenience methods ----

bool Settings::getBool(const QString &key, bool defaultValue) const
{
    return get<bool>(key, defaultValue);
}

void Settings::setBool(const QString &key, bool value) {
    set<bool>(key, value);
}

int Settings::getInt(const QString &key, int defaultValue) const
{
    return get<int>(key, defaultValue);
}

void Settings::setInt(const QString &key, int value) {
    set<int>(key, value);
}

QString Settings::getString(const QString &key, const QString &defaultValue) const
{
    return get<QString>(key, defaultValue);
}

void Settings::setString(const QString &key, const QString &value) {
    set<QString>(key, value);
}

QVariant Settings::value(const QString &key, const QVariant &defaultValue) const
{
    return m_settings->value(key, defaultValue);
}

void Settings::setValue(const QString &key, const QVariant &value) {
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

void Settings::setEncrypted(const QString &key, const QString &value) {
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

void Settings::setInstance(const QString &instanceId, const QString &key, const QString &value) {
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

// ---- Player profiles ----

QStringList Settings::playerProfiles() const
{
    if (!m_settings) return {};
    m_settings->beginGroup("Profile");
    QStringList uuids = m_settings->childGroups();
    m_settings->endGroup();
    return uuids;
}

QString Settings::getProfile(const QString &uuid, const QString &key,
                             const QString &defaultValue) const
{
    if (!m_settings || uuid.isEmpty()) return defaultValue;
    QString fullKey = QString("Profile/%1/%2").arg(uuid, key);
    return m_settings->value(fullKey, defaultValue).toString();
}

void Settings::setProfile(const QString &uuid, const QString &key, const QString &value)
{
    if (!m_settings || uuid.isEmpty()) return;
    QString fullKey = QString("Profile/%1/%2").arg(uuid, key);
    m_settings->setValue(fullKey, value);
    m_settings->sync();
}

void Settings::removeProfile(const QString &uuid)
{
    if (!m_settings || uuid.isEmpty()) return;
    m_settings->beginGroup("Profile");
    m_settings->remove(uuid);
    m_settings->endGroup();
    m_settings->sync();
}

QString Settings::selectedPlayer() const
{
    return getString("SelectedPlayer");
}

void Settings::selectPlayer(const QString &uuid)
{
    setString("SelectedPlayer", uuid);
}
