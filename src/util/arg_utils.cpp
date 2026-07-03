#include "util/arg_utils.h"

#include <QRegularExpression>

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
    QMap<QString, int> seen;
    for (int i = 0; i < args.size(); ++i) {
        QString prefix = args[i].section(QRegularExpression("[= ]"), 0, 0);
        seen[prefix] = i;
    }
    QStringList result;
    for (int i = 0; i < args.size(); ++i) {
        QString prefix = args[i].section(QRegularExpression("[= ]"), 0, 0);
        if (seen[prefix] == i)
            result.append(args[i]);
    }
    return result;
}

} // namespace ArgUtils
