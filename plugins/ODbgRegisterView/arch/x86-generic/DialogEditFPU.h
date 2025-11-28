/*
 * Copyright (C) 2015 Ruslan Kabatsayev <b7.10110111@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
	[[nodiscard]] Register value() const;
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
