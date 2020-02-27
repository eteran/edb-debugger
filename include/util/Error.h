
#ifndef UTIL_ERROR_H_2020227_
#define UTIL_ERROR_H_2020227_

#include <QString>
#include <iostream>

namespace util {

template <class Stream>
void print(Stream &str) {
	str << "\n";
}

template <class Stream, class Arg0, class... Args>
void print(Stream &str, Arg0 &&arg0, Args &&... args) {
	str << std::forward<Arg0>(arg0);
	print(str, std::forward<Args>(args)...);
}

}

#define EDB_PRINT_AND_DIE(...)                                                                              \
	do {                                                                                                    \
		util::print(std::cerr, __FILE__, ":", __LINE__, ": ", Q_FUNC_INFO, ": Fatal error: ", __VA_ARGS__); \
		std::abort();                                                                                       \
	} while (0)

#endif
