
#ifndef EDB_DEMANGLE_H_20151113_
#define EDB_DEMANGLE_H_20151113_

#include <QString>
#include <QStringList>

#ifdef __GNUG__
#include <cxxabi.h>
#include <memory>

#define DEMANGLING_SUPPORTED

inline QString demangle(const QString &mangled) {
	if (!mangled.startsWith("_Z")) {
		return mangled; // otherwise we'll try to demangle C functions coinciding with types like "f" as "float", which is bad
	}

	int failed        = 0;
	QStringList split = mangled.split("@"); // for cases like funcName@plt

	std::unique_ptr<char, decltype(std::free) *> demangled(abi::__cxa_demangle(split.front().toStdString().c_str(), nullptr, nullptr, &failed), std::free);

	if (failed) {
		return mangled;
	}

	split.front() = QString(demangled.get());
	return split.join("@");
}

#else

inline QString demangle(const QString &mangled) {
	return mangled;
}

#endif

#endif
