/*
 * Copyright (C) 2015 Ruslan Kabatsayev <b7.10110111@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DIALOG_OPEN_PROGRAM_H_20151117_
#define DIALOG_OPEN_PROGRAM_H_20151117_

#include <QFileDialog>
#include <QList>

class QLineEdit;
class QByteArray;

class DialogOpenProgram : public QFileDialog {
	Q_OBJECT

public:
	explicit DialogOpenProgram(QWidget *parent = nullptr, const QString &caption = QString(), const QString &directory = QString(), const QString &filter = QString());

public:
	[[nodiscard]] QList<QByteArray> arguments() const;
	[[nodiscard]] QString workingDirectory() const;

private Q_SLOTS:
	void browsePressed();

private:
	QLineEdit *argsEdit_;
	QLineEdit *workDir_;
};

#endif
