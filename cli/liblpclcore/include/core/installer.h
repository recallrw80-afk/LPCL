#ifndef LPCL_INSTALLER_H
#define LPCL_INSTALLER_H

#include <QObject>
#include <QProcess>
#include <QMap>
#include <functional>
#include "core/types.h"
#include "core/lpclcore_export.h"

/**
 * Mod loader installer — Forge, Fabric, NeoForge, OptiFine, LiteLoader.
 * Mirrors the installer logic from ModLaunch and ModDownload in Windows PCL.
 */
class LPCLCORE_EXPORT Installer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isRunning READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)

public:
    static Installer& instance();

    // ---- Installer download ----

    /// Get the installer download URL for a loader type + MC version
    QString getInstallerUrl(const QString &loaderType, const QString &mcVersion);

    /// Download the installer JAR
    void downloadInstaller(const QString &loaderType, const QString &mcVersion,
                           const QString &loaderVersion,
                           std::function<void(bool, QString)> onComplete);

    // ---- Installation ----

    /// Install a mod loader into the current Minecraft version
    /// loaderType: "forge", "fabric", "neoforge", "optifine", "liteloader"
    void installLoader(const QString &loaderType, const QString &mcVersionDir,
                       const QString &loaderVersion,
                       const QString &javaPath,
                       std::function<void(bool, QString)> onComplete);

    /// Install Fabric loader (uses installer JAR or direct JSON generation)
    void installFabric(const QString &mcVersionDir, const QString &mcVersion,
                       const QString &loaderVersion, const QString &javaPath,
                       std::function<void(bool, QString)> onComplete);

    /// Install Forge loader
    void installForge(const QString &mcVersionDir, const QString &mcVersion,
                      const QString &forgeVersion, const QString &javaPath,
                      std::function<void(bool, QString)> onComplete);

    /// Install NeoForge loader
    void installNeoForge(const QString &mcVersionDir, const QString &mcVersion,
                          const QString &neoVersion, const QString &javaPath,
                          std::function<void(bool, QString)> onComplete);

    // ---- Version detection ----

    /// Detect the best loader version for a Minecraft version (async, with cache)
    void detectBestForgeVersion(const QString &mcVersion,
                                 std::function<void(QString)> onComplete);
    QString detectBestFabricVersion(const QString &mcVersion);
    void detectBestNeoForgeVersion(const QString &mcVersion,
                                    std::function<void(QString)> onComplete);

    /// Get available loader versions
    void fetchForgeVersions(std::function<void(bool, QStringList)> onComplete);
    void fetchFabricVersions(std::function<void(bool, QStringList)> onComplete);
    void fetchNeoForgeVersions(std::function<void(bool, QStringList)> onComplete);

    // ---- Status ----

    bool isRunning() const { return m_isRunning; }
    QString statusText() const { return m_statusText; }

signals:
    void runningChanged();
    void statusTextChanged();
    void installProgress(const QString &status, int progress);
    void installLog(const QString &message);

private:
    Installer() = default;

    void runInstallerJar(const QString &jarPath, const QString &javaPath,
                         const QStringList &args,
                         std::function<void(bool, QString)> onComplete);

    QString m_statusText;
    bool m_isRunning = false;

    // Version cache (mcVersion -> loaderVersion)
    QMap<QString, QString> m_forgeCache;
    QMap<QString, QString> m_neoCache;
    QStringList m_fabricVersions;

    // API URLs
    static const QString FORGE_API;
    static const QString FABRIC_API;
    static const QString NEOFORGE_API;
};

#endif // LPCL_INSTALLER_H
