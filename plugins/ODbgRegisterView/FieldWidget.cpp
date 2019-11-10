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

#include "ODbgRV_Util.h"
#include "RegisterView.h"

namespace ODbgRegisterView {

QString FieldWidget::text() const {
	if (!index_.isValid() && !this->isEnabled())
		return QLabel::text();
	const auto text = index_.data();
	if (!text.isValid())
		return QString(width() / letter_size(font()).width() - 1, QChar('?'));
	return text.toString();
}

int FieldWidget::lineNumber() const {
	const auto charSize = letter_size(font());
	return field_position(this).y() / charSize.height();
}

int FieldWidget::columnNumber() const {
	const auto charSize = letter_size(font());
	return field_position(this).x() / charSize.width();
}

void FieldWidget::init(int fieldWidth) {
	setObjectName("FieldWidget");
	const auto charSize = letter_size(font());
	setFixedHeight(charSize.height());
	if (fieldWidth > 0)
		setFixedWidth(fieldWidth * charSize.width());
	setDisabled(true);
}

FieldWidget::FieldWidget(int fieldWidth, const QModelIndex &index, QWidget *parent, Qt::WindowFlags f)
	: QLabel("Fw???", parent, f), index_(index), fieldWidth_(fieldWidth) {

	init(fieldWidth);
}

FieldWidget::FieldWidget(int fieldWidth, const QString &fixedText, QWidget *parent, Qt::WindowFlags f)
	: QLabel(fixedText, parent, f), fieldWidth_(fieldWidth) {

	init(fieldWidth); // NOTE: fieldWidth!=fixedText.length() in general
}

FieldWidget::FieldWidget(const QString &fixedText, QWidget *parent, Qt::WindowFlags f)
	: QLabel(fixedText, parent, f), fieldWidth_(fixedText.length()) {

	init(fixedText.length());
}

int FieldWidget::fieldWidth() const {
	return fieldWidth_;
}

void FieldWidget::adjustToData() {
	QLabel::setText(text());
	adjustSize();
}

ODBRegView *FieldWidget::regView() const {
	const auto parent = parentWidget()        // group
							->parentWidget()  // canvas
							->parentWidget()  // viewport
							->parentWidget(); // regview

	return checked_cast<ODBRegView>(parent);
}

RegisterGroup *FieldWidget::group() const {
	return checked_cast<RegisterGroup>(parentWidget());
}

}
