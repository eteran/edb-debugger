#ifndef EDB_DEMANGLE_H_20151113
#define EDB_DEMANGLE_H_20151113

#include <QString>
#include <QStringList>

#ifdef __GNUG__
#include <memory>
#include <cxxabi.h>

#define DEMANGLING_SUPPORTED true

inline QString demangle(const QString& mangled) {
	int failed=0;
	QStringList split=mangled.split("@"); // for cases like funcName@plt
	std::unique_ptr<char,decltype(std::free)*> demangled(abi::__cxa_demangle(split.front().toStdString().c_str(),0,0,&failed), std::free);
	if(failed) return mangled;
	split.front()=QString(demangled.get());
	return split.join("@");
}

#else

#define DEMANGLING_SUPPORTED false

inline QString demangle(const QString& mangled) {
	return mangled;
}

#endif

#endif
