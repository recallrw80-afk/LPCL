#include "core/lpclcore_export.h"
#ifndef LPCL_ARG_UTILS_H
#define LPCL_ARG_UTILS_H

#include <QStringList>

namespace ArgUtils {

/// Split Java argument string respecting quotes
LPCLCORE_EXPORT QStringList splitJavaArgs(const QString &str);

/// Deduplicate Java arguments (last occurrence wins for same prefix)
LPCLCORE_EXPORT QStringList deduplicateArgs(const QStringList &args);

} // namespace ArgUtils

#endif // LPCL_ARG_UTILS_H
