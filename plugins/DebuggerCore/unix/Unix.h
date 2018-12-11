
#ifndef UNIX_20181211_H_
#define UNIX_20181211_H_

#include <QMap>
#include <QString>
#include <QtGlobal>
#include "Status.h"

namespace DebuggerCorePlugin {
namespace Unix {

QMap<qlonglong, QString> exceptions();
QString exceptionName(qlonglong value);
qlonglong exceptionValue(const QString &name);
Status execute_process(const QString &path, const QString &cwd, const QList<QByteArray> &args);

}
}

#endif
