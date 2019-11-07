/*
Copyright (C) 2015 Ruslan Kabatsayev <b7.10110111@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "BitFieldFormatter.h"
#include "BitFieldDescription.h"
#include "RegisterView.h"

namespace ODbgRegisterView {

BitFieldFormatter::BitFieldFormatter(const BitFieldDescription &bfd)
	: valueNames(bfd.valueNames) {
}

QString BitFieldFormatter::operator()(const QString &str) const {
	assert(str.length());
	if (str.isEmpty()) {
		return str; // for release builds have defined behavior
	}

	if (str[0] == '?') {
		return "????";
	}

	bool parseOK    = false;
	const int value = str.toInt(&parseOK);
	if (!parseOK) {
		return "????";
	}

	assert(0 <= value);
	assert(std::size_t(value) < valueNames.size());
	return valueNames[value];
}

}
