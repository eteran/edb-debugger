/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DIALOG_ARGUMENTS_H_20090609_
#define DIALOG_ARGUMENTS_H_20090609_

#include "ui_DialogArguments.h"
#include <QDialog>

class DialogArguments : public QDialog {
	Q_OBJECT

public:
	explicit DialogArguments(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogArguments() override = default;

public:
	[[nodiscard]] QList<QByteArray> arguments() const;
	void setArguments(const QList<QByteArray> &args);

private:
	Ui::DialogArguments ui;
};

#endif
