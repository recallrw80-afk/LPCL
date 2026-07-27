#include "bridge/player_bridge.h"

#include "lpcl.h"
#include "auth/offlineauth.h"
#include "core/settings.h"

PlayerBridge& PlayerBridge::instance() {
    static PlayerBridge b;
    return b;
}

QString PlayerBridge::currentUuid() const {
    return Settings::instance().selectedPlayer();
}

QString PlayerBridge::currentName() const {
    QString uuid = currentUuid();
    if (uuid.isEmpty()) return {};
    return Settings::instance().getProfile(uuid, "Name");
}

QString PlayerBridge::currentSkinType() const {
    QString uuid = currentUuid();
    if (uuid.isEmpty()) return {};
    return Settings::instance().getProfile(uuid, "SkinType", "slim");
}

QVariantList PlayerBridge::playerList() const {
    QVariantList out;
    QString selected = currentUuid();
    for (const auto &p : lpcl::listPlayers()) {
        QVariantMap m;
        m.insert("uuid", p.uuid);
        m.insert("name", p.name);
        m.insert("avatar", p.avatar);
        m.insert("skinType", p.skinType);
        m.insert("selected", p.uuid == selected);
        out.append(m);
    }
    return out;
}

bool PlayerBridge::selectPlayer(const QString &uuid) {
    bool ok = lpcl::selectPlayer(uuid);
    if (ok) emit playersChanged();
    return ok;
}

QVariantMap PlayerBridge::addPlayer(const QString &name, const QString &skinType) {
    auto p = lpcl::addPlayer(name, QString(), skinType);
    // lpcl::addPlayer 只在首个玩家时自动选中，这里保证"添加即选中"的契约
    if (!p.uuid.isEmpty()) lpcl::selectPlayer(p.uuid);
    emit playersChanged();
    return { {"uuid", p.uuid}, {"name", p.name}, {"skinType", p.skinType} };
}

bool PlayerBridge::removePlayer(const QString &uuid) {
    bool ok = lpcl::removePlayer(uuid);
    if (!ok) return false;
    // 删除选中玩家后 lpcl:: 会把 SelectedPlayer 置空——GUI 侧自动回退到剩余首个，避免无选中态
    if (currentUuid().isEmpty()) {
        auto rest = lpcl::listPlayers();
        if (!rest.isEmpty()) lpcl::selectPlayer(rest.first().uuid);
    }
    emit playersChanged();
    return true;
}

LoginResult PlayerBridge::createLogin() const {
    QString name = currentName();
    if (name.isEmpty()) name = "Player";
    return OfflineAuth::createOfflineLogin(name, currentSkinType());
}
