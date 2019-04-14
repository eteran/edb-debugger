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

#include "RegisterView.h"
#include "ODbgRV_Util.h"
#include "ODbgRV_x86Common.h"

namespace ODbgRegisterView {

FPUValueField::FPUValueField(int fieldWidth, const QModelIndex &regValueIndex, const QModelIndex &tagValueIndex, RegisterGroup *group, FieldWidget *commentWidget, int row, int column)
	: ValueField(fieldWidth, regValueIndex,
                 [this](const QString &str) {
	                 if (str.length() != 20)
		                 return str;
	                 if (groupDigits)
		                 return str.left(4) + " " + str.mid(4, 8) + " " + str.right(8);
	                 return str;
				 }, group),
      commentWidget(commentWidget), row(row), column(column), tagValueIndex(tagValueIndex)
{
	Q_ASSERT(group);
	Q_ASSERT(commentWidget);
	showAsRawActionIndex = menuItems.size();
	menuItems.push_back(newAction(tr("View FPU as raw values"), this, this, SLOT(showFPUAsRaw())));

	showAsFloatActionIndex = menuItems.size();
	menuItems.push_back(newAction(tr("View FPU as floats"), this, this, SLOT(showFPUAsFloat())));

	group->insert(row, column, this);
	group->insert(commentWidget);
	// will be moved to its column in the next line
	group->setupPositionAndSize(row, 0, commentWidget);
	displayFormatChanged();
	connect(index.model(), SIGNAL(FPUDisplayFormatChanged()), this, SLOT(displayFormatChanged()));
}

void FPUValueField::showFPUAsRaw() {
	model()->setChosenFPUFormat(index.parent(), NumberDisplayMode::Hex);
}

void FPUValueField::showFPUAsFloat() {
	model()->setChosenFPUFormat(index.parent(), NumberDisplayMode::Float);
}

void FPUValueField::displayFormatChanged() {

	using RegisterViewModelBase::Model;
	const auto format = static_cast<NumberDisplayMode>(VALID_VARIANT(index.parent().data(Model::ChosenFPUFormatRole)).toInt());

	switch (format) {
	case NumberDisplayMode::Hex:
		menuItems[showAsRawActionIndex]->setVisible(false);
		menuItems[showAsFloatActionIndex]->setVisible(true);
		break;
	case NumberDisplayMode::Float:
		menuItems[showAsRawActionIndex]->setVisible(true);
		menuItems[showAsFloatActionIndex]->setVisible(false);
		break;
	default:
		menuItems[showAsRawActionIndex]->setVisible(true);
		menuItems[showAsFloatActionIndex]->setVisible(true);
		break;
	}

	const auto margins = group()->getFieldMargins();
	fieldWidth_        = VALID_VARIANT(index.data(Model::FixedLengthRole)).toInt();

	Q_ASSERT(fieldWidth_ > 0);

	if (format == NumberDisplayMode::Hex) {
		groupDigits = true;
		fieldWidth_ += 2; // add some room for spaces between groups
	} else
		groupDigits      = false;
	const auto charWidth = letterSize(font()).width();
	setFixedWidth(charWidth * fieldWidth_ + margins.left() + margins.right());
	commentWidget->move(x() + maximumWidth(), commentWidget->y());
}

void FPUValueField::updatePalette() {
	if (!changed() && tagValueIndex.data().toUInt() == FPU_TAG_EMPTY) {
		auto palette = group()->palette();
		palette.setColor(foregroundRole(), palette.color(QPalette::Disabled, QPalette::Text));
		setPalette(palette);
		QLabel::update();
	} else
		ValueField::updatePalette();
}

}
