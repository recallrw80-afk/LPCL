#ifndef LPCL_SETTINGS_H
#define LPCL_SETTINGS_H

#include "lpclcore/lpclcore_export.h"
#include <QObject>
#include <QSettings>
#include <QVariant>
#include <QString>
#include <QMap>

/**
 * Cross-platform settings manager.
 * Mirrors the original VB Settings.Get(Of T)(key) / Settings.Set(key, value) API.
 *
 * Uses QSettings (INI file on Linux/Mac, registry on Windows).
 * Stored in <app data>/PCL/PCL.ini on all platforms for consistency.
 */
class LPCLCORE_EXPORT Settings : public QObject
{
    Q_OBJECT

public:
    static Settings& instance();
    static void initialize(const QString &configPath = QString());

    // Typed getters
    template<typename T>
    T get(const QString &key, const T &defaultValue = T()) const {
        QVariant v = m_settings->value(key);
        if (!v.isValid() || v.isNull()) return defaultValue;
        return v.value<T>();
    }

    // Typed setter
    template<typename T>
    void set(const QString &key, const T &value) {
        m_settings->setValue(key, QVariant::fromValue(value));
        m_settings->sync();
    }

    // Bool convenience
    bool getBool(const QString &key, bool defaultValue = false) const;
    void setBool(const QString &key, bool value);

    // Int convenience
    int getInt(const QString &key, int defaultValue = 0) const;
    void setInt(const QString &key, int value);

    // String convenience
    QString getString(const QString &key, const QString &defaultValue = QString()) const;
    void setString(const QString &key, const QString &value);

    // Direct QVariant access
    QVariant value(const QString &key, const QVariant &defaultValue = QVariant()) const;
    void setValue(const QString &key, const QVariant &value);

    // Encrypted settings (DES via CryptoUtils::pclEncrypt/pclDecrypt)
    QString getEncrypted(const QString &key, const QString &defaultValue = QString()) const;
    void setEncrypted(const QString &key, const QString &value);

    // Instance isolation (per-version .minecraft)
    QString getInstance(const QString &instanceId, const QString &key,
                        const QString &defaultValue = QString()) const;
    void setInstance(const QString &instanceId, const QString &key, const QString &value);
    QString instancePath(const QString &instanceId) const;

    // Initialize defaults
    void initDefaults();

signals:
    void settingChanged(const QString &key);

private:
    Settings();
    ~Settings();
    Settings(const Settings&) = delete;
    Settings& operator=(const Settings&) = delete;

    QSettings *m_settings = nullptr;
};

// Template specializations for common types
template<>
inline QString Settings::get<QString>(const QString &key, const QString &defaultValue) const {
    return m_settings->value(key, defaultValue).toString();
}

template<>
inline int Settings::get<int>(const QString &key, const int &defaultValue) const {
    return m_settings->value(key, defaultValue).toInt();
}

template<>
inline bool Settings::get<bool>(const QString &key, const bool &defaultValue) const {
    return m_settings->value(key, defaultValue).toBool();
}

template<>
inline void Settings::set<QString>(const QString &key, const QString &value) {
    m_settings->setValue(key, value);
    m_settings->sync();
}

template<>
inline void Settings::set<int>(const QString &key, const int &value) {
    m_settings->setValue(key, value);
    m_settings->sync();
}

template<>
inline void Settings::set<bool>(const QString &key, const bool &value) {
    m_settings->setValue(key, value);
    m_settings->sync();
}

#endif // LPCL_SETTINGS_H
