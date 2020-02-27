
#ifndef UTIL_STRING_H_2020227_
#define UTIL_STRING_H_2020227_

#include <QString>

namespace util {

/**
 * @brief is_numeric
 * @param s
 * @return true if the string only contains decimal digits
 */
inline bool is_numeric(const QString &s) {
	for (QChar ch : s) {
		if (!ch.isDigit()) {
			return false;
		}
	}

	return true;
}

}

#endif
