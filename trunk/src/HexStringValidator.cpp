
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

	Q_FOREACH(QChar ch, input) {
		const int c = ch.toAscii();
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
