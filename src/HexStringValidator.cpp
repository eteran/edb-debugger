/*
Copyright (C) 2006 - 2015 Evan Teran
                          evan.teran@gmail.com

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

#include "HexStringValidator.h"
#include <QString>
#include <cctype> // for std::isxdigit

//------------------------------------------------------------------------------
// Name: HexStringValidator
// Desc: constructor
//------------------------------------------------------------------------------
HexStringValidator::HexStringValidator(QObject * parent) : QValidator(parent) {
}

//------------------------------------------------------------------------------
// Name: fixup
// Desc:
//------------------------------------------------------------------------------
void HexStringValidator::fixup(QString &input) const {
	QString temp;
	int index = 0;

	for(QChar ch: input) {
		const int c = ch.toLatin1();
		if(c < 0x80 && std::isxdigit(c)) {

			if(index != 0 && (index & 1) == 0) {
				temp += ' ';
			}

			temp += ch.toUpper();
			++index;
		}
	}

	input = temp;
}

//------------------------------------------------------------------------------
// Name: validate
// Desc:
//------------------------------------------------------------------------------
QValidator::State HexStringValidator::validate(QString &input, int &pos) const {
	if(!input.isEmpty()) {
		// TODO: can we detect if the char which was JUST deleted
		// (if any was deleted) was a space? and special case this?
		// as to not have the minor bug in this case?

		const int char_pos = pos - input.left(pos).count(' ');
		int chars          = 0;
		fixup(input);

		pos = 0;

		while(chars != char_pos) {
			if(input[pos] != ' ') {
				++chars;
			}
			++pos;
		}

		// favor the right side of a space
		if(input[pos] == ' ') {
			++pos;
		}
	}
	return QValidator::Acceptable;
}
