/*
 * Copyright (C) 2015 Ruslan Kabatsayev <b7.10110111@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
			  if (str.length() != 20) {
				  return str;
			  }
			  if (groupDigits_) {
				  return str.left(4) + " " + str.mid(4, 8) + " " + str.right(8);
			  }
			  return str;
		  },
		  group),
	  commentWidget_(commentWidget),
	  row_(row),
	  column_(column),
	  tagValueIndex_(tagValueIndex) {

	Q_ASSERT(group);
	Q_ASSERT(commentWidget);
	showAsRawActionIndex_ = menuItems_.size();
	menuItems_.push_back(new_action(tr("View FPU as raw values"), this, [this](bool) {
		showFPUAsRaw();
	}));

	showAsFloatActionIndex_ = menuItems_.size();
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
		menuItems_[showAsRawActionIndex_]->setVisible(false);
		menuItems_[showAsFloatActionIndex_]->setVisible(true);
		break;
	case NumberDisplayMode::Float:
		menuItems_[showAsRawActionIndex_]->setVisible(true);
		menuItems_[showAsFloatActionIndex_]->setVisible(false);
		break;
	default:
		menuItems_[showAsRawActionIndex_]->setVisible(true);
		menuItems_[showAsFloatActionIndex_]->setVisible(true);
		break;
	}

	const auto margins = group()->getFieldMargins();
	fieldWidth_        = valid_variant(index_.data(Model::FixedLengthRole)).toInt();

	Q_ASSERT(fieldWidth_ > 0);

	if (format == NumberDisplayMode::Hex) {
		groupDigits_ = true;
		fieldWidth_ += 2; // add some room for spaces between groups
	} else {
		groupDigits_ = false;
	}

	const auto charWidth = letter_size(font()).width();
	setFixedWidth(charWidth * fieldWidth_ + margins.left() + margins.right());
	commentWidget_->move(x() + maximumWidth(), commentWidget_->y());
}

void FpuValueField::updatePalette() {
	if (!changed() && tagValueIndex_.data().toUInt() == FpuTagEmpty) {
		auto palette = group()->palette();
		palette.setColor(foregroundRole(), palette.color(QPalette::Disabled, QPalette::Text));
		setPalette(palette);
		QLabel::update();
	} else {
		ValueField::updatePalette();
	}
}

}
