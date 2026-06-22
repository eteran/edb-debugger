/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "DialogArguments.h"
#include <QListWidgetItem>

/**
 * @brief
 */
DialogArguments::DialogArguments(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {
	ui.setupUi(this);
}

/**
 * @brief
 */
QList<QByteArray> DialogArguments::arguments() const {
	QList<QByteArray> ret;
	for (int i = 0; i < ui.listWidget->count(); ++i) {
		ret << ui.listWidget->item(i)->text().toUtf8();
	}
	return ret;
}

/**
 * @brief
 */
void DialogArguments::setArguments(const QList<QByteArray> &args) {
	ui.listWidget->clear();

	QStringList l;
	for (const QByteArray &ba : args) {
		l << QString::fromUtf8(ba);
	}

	ui.listWidget->addItems(l);
}
