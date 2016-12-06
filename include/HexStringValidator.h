/*
Copyright (C) 2013 - 2015 Evan Teran
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

#ifndef HEX_STRING_VALIDATOR_20130625_H_
#define HEX_STRING_VALIDATOR_20130625_H_

#include <QValidator>

class HexStringValidator : public QValidator {
public:
	HexStringValidator(QObject * parent);

public:
	virtual void fixup(QString &input) const;
	virtual State validate(QString &input, int &pos) const;
};

#endif
