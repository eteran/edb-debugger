/*
Copyright (C) 2006 - 2023 Evan Teran
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

#include "DialogInputBinaryString.h"

//------------------------------------------------------------------------------
// Name: DialogInputBinaryString
// Desc: constructor
//------------------------------------------------------------------------------
DialogInputBinaryString::DialogInputBinaryString(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {

	ui.setupUi(this);
}

//------------------------------------------------------------------------------
// Name: binary_string
// Desc: returns the binary string we wrap around
//------------------------------------------------------------------------------
BinaryString *DialogInputBinaryString::binaryString() const {
	return ui.binaryString;
}
