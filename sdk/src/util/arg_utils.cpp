#include "util/arg_utils.h"

#include <QRegularExpression>
#include <QSet>

namespace ArgUtils {

QStringList splitJavaArgs(const QString &str) {
    QStringList args;
    bool inQuote = false;
    QString current;

    for (int i = 0; i < str.length(); ++i) {
        QChar c = str[i];
        if (c == '"') {
            inQuote = !inQuote;
        } else if (c == ' ' && !inQuote) {
            if (!current.isEmpty()) {
                args.append(current.trimmed());
                current.clear();
            }
        } else {
            current += c;
        }
    }
    if (!current.trimmed().isEmpty())
        args.append(current.trimmed());

    return args;
}

QStringList deduplicateArgs(const QStringList &args) {
    // 可重复参数（成对出现的模块 flag）不参与去重：
    // Forge/NeoForge 的 JVM 参数含多个 --add-opens/--add-exports，
    // 按前缀去重会把它们删到只剩一个，值变成孤儿参数
    static const QSet<QString> repeatableFlags = {
        QStringLiteral("--add-opens"), QStringLiteral("--add-exports"),
        QStringLiteral("--add-reads"), QStringLiteral("--patch-module")
    };
    // -Xmx2G / -Xms512m 这类无分隔符参数按 flag 名去重（后写的覆盖先写的）
    // 排除 -XX: 前缀（-XX:+UseG1GC、-XX:G1NewSizePercent=20 等各自独立）
    static const QRegularExpression xFlagRe(QStringLiteral("^(-X(?!X)[a-zA-Z]+)"));

    auto keyOf = [&](const QString &a) -> QString {
        if (repeatableFlags.contains(a)) return {};  // 空 key = 不去重
        auto m = xFlagRe.match(a);
        if (m.hasMatch()) return m.captured(1);      // -Xmx2G → -Xmx
        return a.section(QRegularExpression("[= ]"), 0, 0);
    };

    // 保留每个 key 的最后一次出现
    QMap<QString, int> lastIndex;
    for (int i = 0; i < args.size(); ++i) {
        QString key = keyOf(args[i]);
        if (!key.isEmpty()) lastIndex[key] = i;
    }
    QStringList result;
    for (int i = 0; i < args.size(); ++i) {
        QString key = keyOf(args[i]);
        if (key.isEmpty() || lastIndex.value(key) == i)
            result.append(args[i]);
    }
    return result;
}

} // namespace ArgUtils
