#include "lpclcore/lpclcore_export.h"
#ifndef LPCL_ARG_UTILS_H
#define LPCL_ARG_UTILS_H

#include <QStringList>

namespace ArgUtils {

/// Split Java argument string respecting quotes
QStringList splitJavaArgs(const QString &str);

/// Deduplicate Java arguments (last occurrence wins for same prefix)
QStringList deduplicateArgs(const QStringList &args);

} // namespace ArgUtils

#endif // LPCL_ARG_UTILS_H
