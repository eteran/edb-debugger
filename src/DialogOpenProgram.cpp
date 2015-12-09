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
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QGridLayout>
#include <QDebug>

DialogOpenProgram::DialogOpenProgram(QWidget* parent,const QString& caption, const QString& directory,const QString& filter)
	: QFileDialog(parent,caption,directory,filter),
			  argsEdit(new QLineEdit(this)),
			  workDir(new QLineEdit(QDir::currentPath(),this))
{
	QGridLayout* const layout=dynamic_cast<QGridLayout*>(this->layout());
	// We want to be sure that the layout is as we expect it
	if(layout && layout->rowCount()==4 && layout->columnCount()==3)
	{
		setFileMode(QFileDialog::ExistingFile);
		const int rowCount=layout->rowCount();
		QPushButton* const browseDirButton(new QPushButton(tr("Browse..."),this));
		layout->addWidget(new QLabel(tr("Program arguments:"),this),rowCount,0);
		layout->addWidget(argsEdit,rowCount,1);
		layout->addWidget(new QLabel(tr("Working directory:"),this),rowCount+1,0);
		layout->addWidget(workDir,rowCount+1,1);
		layout->addWidget(browseDirButton,rowCount+1,2);

		connect(browseDirButton,SIGNAL(clicked()),this,SLOT(browsePressed()));
	}
	else qDebug() << tr("Failed to setup program arguments and working directory entries for file open dialog, please report and be sure to tell your Qt version");
}

void DialogOpenProgram::browsePressed()
{
	const QString dir=QFileDialog::getExistingDirectory(this,tr("Choose program working directory"),workDir->text());
	if(dir.size()) workDir->setText(dir);
}

QList<QByteArray> DialogOpenProgram::arguments() const
{
	const QStringList args=argsEdit->text().split(' ');
	qDebug() << "stringlist:" << args;
	QList<QByteArray> ret;
	for(auto arg : args)
		ret << arg.toLocal8Bit();
	return ret;
}

QString DialogOpenProgram::workingDirectory() const
{
	return workDir->text();
}
