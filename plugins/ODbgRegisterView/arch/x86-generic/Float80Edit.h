/*
 * Copyright (C) 2015 Ruslan Kabatsayev <b7.10110111@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef FLOAT80_EDIT_H_20151031_
#define FLOAT80_EDIT_H_20151031_

#include "Types.h"
#include <QLineEdit>

namespace ODbgRegisterView {

class Float80Edit : public QLineEdit {
	Q_OBJECT

public:
	explicit Float80Edit(QWidget *parent = nullptr);
	void setValue(edb::value80 input);

public:
	[[nodiscard]] QSize sizeHint() const override;

protected:
	void focusOutEvent(QFocusEvent *e) override;

Q_SIGNALS:
	void defocussed();
};

}

#endif
