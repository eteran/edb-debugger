/*
Copyright (C) 2014 - 2015 Evan Teran
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

#include "FixedFontSelector.h"
#include <QtDebug>

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
FixedFontSelector::FixedFontSelector(QWidget *parent) : QWidget(parent) {
	ui.setupUi(this);

	for(int size: QFontDatabase::standardSizes()) {
		ui.fontSize->addItem(QString("%1").arg(size), size);
	}
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
FixedFontSelector::~FixedFontSelector() {
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
QFont FixedFontSelector::currentFont() {

	return ui.fontCombo->currentFont();
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void FixedFontSelector::setCurrentFont(const QString &font) {

	QFont f;
	f.fromString(font);
	setCurrentFont(f);
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void FixedFontSelector::setCurrentFont(const QFont &font) {
	ui.fontCombo->setCurrentFont(font);
	int n = ui.fontSize->findData(font.pointSize());
	if(n != -1) {
		ui.fontSize->setCurrentIndex(n);
	}
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void FixedFontSelector::on_fontCombo_currentFontChanged(const QFont &font) {
	Q_EMIT currentFontChanged(font);
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void FixedFontSelector::on_fontSize_currentIndexChanged(int index) {

	QFont font = ui.fontCombo->currentFont();
	font.setPointSize(ui.fontSize->itemData(index).toInt());
	ui.fontCombo->setCurrentFont(font);

	Q_EMIT currentFontChanged(font);
}
