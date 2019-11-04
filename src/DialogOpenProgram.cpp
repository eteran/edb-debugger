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

#include "DialogOpenProgram.h"
#include "edb.h"

#include <QDebug>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

/**
 * @brief DialogOpenProgram::DialogOpenProgram
 * @param parent
 * @param caption
 * @param directory
 * @param filter
 */
DialogOpenProgram::DialogOpenProgram(QWidget *parent, const QString &caption, const QString &directory, const QString &filter)
	: QFileDialog(parent, caption, directory, filter),
	  argsEdit_(new QLineEdit(this)),
	  workDir_(new QLineEdit(QDir::currentPath(), this)) {

	setOptions(QFileDialog::DontUseNativeDialog);

	auto layout = qobject_cast<QGridLayout *>(this->layout());

	// We want to be sure that the layout is as we expect it
	if (layout && layout->rowCount() == 4 && layout->columnCount() == 3) {
		setFileMode(QFileDialog::ExistingFile);

		const int rowCount = layout->rowCount();
		QPushButton *const browseDirButton(new QPushButton(tr("&Browse..."), this));

		const auto argsLabel = new QLabel(tr("Program &arguments:"), this);
		argsLabel->setBuddy(argsEdit_);
		layout->addWidget(argsLabel, rowCount, 0);
		layout->addWidget(argsEdit_, rowCount, 1);

		const auto workDirLabel = new QLabel(tr("Working &directory:"), this);
		workDirLabel->setBuddy(workDir_);
		layout->addWidget(workDirLabel, rowCount + 1, 0);
		layout->addWidget(workDir_, rowCount + 1, 1);
		layout->addWidget(browseDirButton, rowCount + 1, 2);

		connect(browseDirButton, &QPushButton::clicked, this, &DialogOpenProgram::browsePressed);
	} else {
		qWarning() << tr("Failed to setup program arguments and working directory entries for file open dialog, please report and be sure to tell your Qt version");
	}
}

/**
 * @brief DialogOpenProgram::browsePressed
 */
void DialogOpenProgram::browsePressed() {
	const QString dir = QFileDialog::getExistingDirectory(this, tr("Choose program working directory"), workDir_->text());
	if (!dir.isEmpty()) {
		workDir_->setText(dir);
	}
}

/**
 * @brief DialogOpenProgram::arguments
 * @return
 */
QList<QByteArray> DialogOpenProgram::arguments() const {
	const QStringList args = edb::v1::parse_command_line(argsEdit_->text());
	QList<QByteArray> ret;
	for (const QString &arg : args) {
		ret << arg.toLocal8Bit();
	}
	return ret;
}

/**
 * @brief DialogOpenProgram::workingDirectory
 * @return
 */
QString DialogOpenProgram::workingDirectory() const {
	return workDir_->text();
}
