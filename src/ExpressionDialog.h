/*
 * Copyright (C) 2014 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef EXPRESSION_DIALOG_H_20191119_
#define EXPRESSION_DIALOG_H_20191119_

#include "Types.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPalette>
#include <QVBoxLayout>

class QString;

class ExpressionDialog final : public QDialog {
	Q_OBJECT

public:
	explicit ExpressionDialog(const QString &title, const QString &prompt, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());

public:
	edb::address_t getAddress();

private Q_SLOTS:
	void on_text_changed(const QString &text);

private:
	QVBoxLayout *layout_         = nullptr;
	QLabel *labelText_           = nullptr;
	QLabel *labelError_          = nullptr;
	QLineEdit *expression_       = nullptr;
	QDialogButtonBox *buttonBox_ = nullptr;
	QPalette paletteError_;
	edb::address_t lastAddress_;
};

#endif
