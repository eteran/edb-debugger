/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MULTI_BIT_FIELD_WIDGET_H_20191119_
#define MULTI_BIT_FIELD_WIDGET_H_20191119_

#include "ValueField.h"

namespace ODbgRegisterView {

struct BitFieldDescription;

class MultiBitFieldWidget final : public ValueField {
	Q_OBJECT

public:
	MultiBitFieldWidget(const QModelIndex &index, const BitFieldDescription &bfd, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());

public Q_SLOTS:
	void setValue(int value);
	void adjustToData() override;

private:
	QList<QAction *> valueActions_;
	std::function<bool(unsigned, unsigned)> equal_;
};

}

#endif
