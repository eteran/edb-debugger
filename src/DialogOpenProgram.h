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
