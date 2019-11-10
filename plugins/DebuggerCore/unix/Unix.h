
#ifndef UNIX_20181211_H_
#define UNIX_20181211_H_

#include "Status.h"
#include <QMap>
#include <QString>
#include <QtGlobal>

namespace DebuggerCorePlugin {
namespace Unix {

QMap<qlonglong, QString> exceptions();
QString exception_name(qlonglong value);
qlonglong exception_value(const QString &name);
Status execute_process(const QString &path, const QString &cwd, const QList<QByteArray> &args);

}
}

#endif
