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

#ifndef DIALOG_EDIT_FPU_H_20151031_
#define DIALOG_EDIT_FPU_H_20151031_

#include "Register.h"
#include <QDialog>

class QLineEdit;

namespace ODbgRegisterView {

class Float80Edit;

class DialogEditFPU : public QDialog {
	Q_OBJECT

public:
	explicit DialogEditFPU(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	Register value() const;
	void setValue(const Register &reg);

private Q_SLOTS:
	void onHexEdited(const QString &);
	void onFloatEdited(const QString &);
	void updateFloatEntry();
	void updateHexEntry();

protected:
	bool eventFilter(QObject *, QEvent *) override;

private:
	Register reg_;

	edb::value80 value_;
	Float80Edit *floatEntry_ = nullptr;
	QLineEdit *hexEntry_     = nullptr;
};

}

#endif
