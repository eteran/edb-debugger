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

#include "FpuValueField.h"
#include "ODbgRV_Util.h"
#include "ODbgRV_x86Common.h"
#include "RegisterGroup.h"
#include "RegisterView.h"

namespace ODbgRegisterView {

FpuValueField::FpuValueField(int fieldWidth, const QModelIndex &regValueIndex, const QModelIndex &tagValueIndex, RegisterGroup *group, FieldWidget *commentWidget, int row, int column)
	: ValueField(
		  fieldWidth, regValueIndex,
		  [this](const QString &str) {
			  if (str.length() != 20)
				  return str;
			  if (groupDigits)
				  return str.left(4) + " " + str.mid(4, 8) + " " + str.right(8);
			  return str;
		  },
		  group),
	  commentWidget(commentWidget),
	  row(row),
	  column(column),
	  tagValueIndex(tagValueIndex) {

	Q_ASSERT(group);
	Q_ASSERT(commentWidget);
	showAsRawActionIndex = menuItems_.size();
	menuItems_.push_back(new_action(tr("View FPU as raw values"), this, [this](bool) {
		showFPUAsRaw();
	}));

	showAsFloatActionIndex = menuItems_.size();
	menuItems_.push_back(new_action(tr("View FPU as floats"), this, [this](bool) {
		showFPUAsFloat();
	}));

	group->insert(row, column, this);
	group->insert(commentWidget);
	// will be moved to its column in the next line
	group->setupPositionAndSize(row, 0, commentWidget);
	displayFormatChanged();
	connect(index_.model(), SIGNAL(FPUDisplayFormatChanged()), this, SLOT(displayFormatChanged()));
}

void FpuValueField::showFPUAsRaw() {
	model()->setChosenFPUFormat(index_.parent(), NumberDisplayMode::Hex);
}

void FpuValueField::showFPUAsFloat() {
	model()->setChosenFPUFormat(index_.parent(), NumberDisplayMode::Float);
}

void FpuValueField::displayFormatChanged() {

	using RegisterViewModelBase::Model;
	const auto format = static_cast<NumberDisplayMode>(valid_variant(index_.parent().data(Model::ChosenFPUFormatRole)).toInt());

	switch (format) {
	case NumberDisplayMode::Hex:
		menuItems_[showAsRawActionIndex]->setVisible(false);
		menuItems_[showAsFloatActionIndex]->setVisible(true);
		break;
	case NumberDisplayMode::Float:
		menuItems_[showAsRawActionIndex]->setVisible(true);
		menuItems_[showAsFloatActionIndex]->setVisible(false);
		break;
	default:
		menuItems_[showAsRawActionIndex]->setVisible(true);
		menuItems_[showAsFloatActionIndex]->setVisible(true);
		break;
	}

	const auto margins = group()->getFieldMargins();
	fieldWidth_        = valid_variant(index_.data(Model::FixedLengthRole)).toInt();

	Q_ASSERT(fieldWidth_ > 0);

	if (format == NumberDisplayMode::Hex) {
		groupDigits = true;
		fieldWidth_ += 2; // add some room for spaces between groups
	} else {
		groupDigits = false;
	}

	const auto charWidth = letter_size(font()).width();
	setFixedWidth(charWidth * fieldWidth_ + margins.left() + margins.right());
	commentWidget->move(x() + maximumWidth(), commentWidget->y());
}

void FpuValueField::updatePalette() {
	if (!changed() && tagValueIndex.data().toUInt() == FpuTagEmpty) {
		auto palette = group()->palette();
		palette.setColor(foregroundRole(), palette.color(QPalette::Disabled, QPalette::Text));
		setPalette(palette);
		QLabel::update();
	} else {
		ValueField::updatePalette();
	}
}

}
