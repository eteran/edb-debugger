
#ifndef QT_HELPER_H_20191119_
#define QT_HELPER_H_20191119_

#include <QCoreApplication>

#define Q_DECLARE_NAMESPACE_TR(context)                                                             \
	inline QString tr(const char *sourceText, const char *disambiguation = Q_NULLPTR, int n = -1) { \
		return QCoreApplication::translate(#context, sourceText, disambiguation, n);                \
	}

#endif
