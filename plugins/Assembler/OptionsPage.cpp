/*
Copyright (C) 2006 - 2015 Evan Teran
                          evan.teran@gmail.com

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

#include "OptionsPage.h"
#include <QSettings>
#include <QDebug>
#include <QFileDialog>
#include <QDomDocument>

#include "ui_OptionsPage.h"

namespace Assembler {

//------------------------------------------------------------------------------
// Name: OptionsPage
// Desc:
//------------------------------------------------------------------------------
OptionsPage::OptionsPage(QWidget *parent) : QWidget(parent), ui(new Ui::OptionsPage) {
	ui->setupUi(this);
	
	
	QSettings settings;
	const QString name = settings.value("Assembler/helper", "yasm").toString();
	
	
	ui->assemblerName->clear();
	
	QFile file(":/debugger/Assembler/xml/assemblers.xml");
	if(file.open(QIODevice::ReadOnly | QIODevice::Text)) {	
		QDomDocument xml;
		xml.setContent(&file);
		QDomElement root = xml.documentElement();

		for(QDomElement assembler = root.firstChildElement("assembler"); !assembler.isNull(); assembler = assembler.nextSiblingElement("assembler")) {
			const QString name = assembler.attribute("name");
			ui->assemblerName->addItem(name);
		}
	}
	




	const int index = ui->assemblerName->findText(name, Qt::MatchFixedString);
	if(index == -1 && ui->assemblerName->count() > 0) {
		ui->assemblerName->setCurrentIndex(0);
	} else {
		ui->assemblerName->setCurrentIndex(index);
	}
}

//------------------------------------------------------------------------------
// Name: ~OptionsPage
// Desc:
//------------------------------------------------------------------------------
OptionsPage::~OptionsPage() {
	delete ui;
}

//------------------------------------------------------------------------------
// Name: showEvent
// Desc:
//------------------------------------------------------------------------------
void OptionsPage::showEvent(QShowEvent *event) {
	Q_UNUSED(event);
}

//------------------------------------------------------------------------------
// Name: on_assemblerName_currentIndexChanged
// Desc:
//------------------------------------------------------------------------------
void OptionsPage::on_assemblerName_currentIndexChanged(const QString &text) {
	QSettings settings;
	settings.setValue("Assembler/helper", text);
}

}
