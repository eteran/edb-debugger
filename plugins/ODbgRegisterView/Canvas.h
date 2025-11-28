/*
 * Copyright (C) 2015 Ruslan Kabatsayev <b7.10110111@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef CANVAS_H_20151031_
#define CANVAS_H_20151031_

#include <QWidget>

namespace ODbgRegisterView {

class Canvas : public QWidget {
	Q_OBJECT
public:
	explicit Canvas(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());

protected:
	void mousePressEvent(QMouseEvent *event) override;
};

}

#endif
