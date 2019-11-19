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

#ifndef VOLATILE_NAME_FIELD_H_20151031_
#define VOLATILE_NAME_FIELD_H_20151031_

#include "FieldWidget.h"
#include <QString>
#include <functional>

namespace ODbgRegisterView {

class VolatileNameField : public FieldWidget {
	Q_OBJECT

private:
	std::function<QString()> valueFormatter;

public:
	VolatileNameField(int fieldWidth, const std::function<QString()> &valueFormatter, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	QString text() const override;
};

}

#endif
