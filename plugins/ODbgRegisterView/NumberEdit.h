/*
 * Copyright (C) 2015 Ruslan Kabatsayev <b7.10110111@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
	[[nodiscard]] int column() const;
	[[nodiscard]] int colSpan() const;
	void setNaturalWidthInChars(int nChars);

public:
	[[nodiscard]] QSize minimumSizeHint() const override;
	[[nodiscard]] QSize sizeHint() const override;

private:
	int naturalWidthInChars_ = 17; // default roughly as in QLineEdit
	int column_;
	int colSpan_;
};

}

#endif
