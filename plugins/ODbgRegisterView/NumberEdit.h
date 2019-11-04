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

#ifndef NUMBER_EDIT_H_20190412_
#define NUMBER_EDIT_H_20190412_

#include <QLineEdit>

namespace ODbgRegisterView {

class NumberEdit final : public QLineEdit {
	Q_OBJECT
public:
	NumberEdit(int column, int colSpan, QWidget *parent = nullptr);
	~NumberEdit() override = default;

public:
	int column() const;
	int colSpan() const;
	void setNaturalWidthInChars(int nChars);

public:
	QSize minimumSizeHint() const override;
	QSize sizeHint() const override;

private:
	int naturalWidthInChars_ = 17; // default roughly as in QLineEdit
	int column_;
	int colSpan_;
};

}

#endif
