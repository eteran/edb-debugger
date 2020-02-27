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

#include "ValueField.h"
#include "DialogEditGPR.h"
#include "DialogEditSimdRegister.h"
#include "ODbgRV_Common.h"
#include "ODbgRV_Util.h"
#include "RegisterGroup.h"
#include "RegisterView.h"
#include "util/Font.h"
#if defined(EDB_X86) || defined(EDB_X86_64)
#include "DialogEditFPU.h"
#include "ODbgRV_x86Common.h"
#endif

#include <QApplication>
#include <QClipboard>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleOptionViewItem>

namespace ODbgRegisterView {
namespace {
QStyle *flatStyle = nullptr;
}

ValueField::ValueField(int fieldWidth, const QModelIndex &index, QWidget *parent, Qt::WindowFlags f)
	: ValueField(
		  fieldWidth, index, [](const QString &s) { return s; }, parent, f) {
}

ValueField::ValueField(int fieldWidth, const QModelIndex &index, const std::function<QString(const QString &)> &valueFormatter, QWidget *parent, Qt::WindowFlags f)
	: FieldWidget(fieldWidth, index, parent, f), valueFormatter_(valueFormatter) {

	setObjectName("ValueField");
	setDisabled(false);
	setMouseTracking(true);

// Set some known style to avoid e.g. Oxygen's label transition animations, which
// break updating of colors such as "register changed" when single-stepping frequently
#define FLAT_STYLE_NAME "fusion"

	if (!flatStyle)
		flatStyle = QStyleFactory::create(FLAT_STYLE_NAME);

	assert(flatStyle);
	setStyle(flatStyle);

	using namespace RegisterViewModelBase;

	if (index.data(Model::IsNormalRegisterRole).toBool() || index.data(Model::IsSIMDElementRole).toBool()) {
		menuItems_.push_back(new_action(tr("&Modify") + QChar(0x2026), this, [this](bool) {
			defaultAction();
		}));

		menuItems_.back()->setShortcut(QKeySequence(Qt::Key_Enter));
	} else if (index.data(Model::IsBitFieldRole).toBool() && index.data(Model::BitFieldLengthRole).toInt() == 1) {
		menuItems_.push_back(new_action(tr("&Toggle"), this, [this](bool) {
			defaultAction();
		}));

		menuItems_.back()->setShortcut(QKeySequence(Qt::Key_Enter));
	}

	menuItems_.push_back(new_action(tr("&Copy to clipboard"), this, [this](bool) {
		copyToClipboard();
	}));

	menuItems_.back()->setShortcut(QKeySequence::Copy);

#if defined(EDB_X86) || defined(EDB_X86_64)
	if (index.sibling(index.row(), ModelNameColumn).data().toString() == FsrName) {
		menuItems_.push_back(new_action(tr("P&ush FPU stack"), this, [this](bool) {
			pushFPUStack();
		}));

		menuItems_.push_back(new_action(tr("P&op FPU stack"), this, [this](bool) {
			popFPUStack();
		}));
	}
#endif

	if (index.parent().data().toString() == GprCategoryName) {
		// These should be above others, so prepending instead of appending
		menuItems_.push_front(new_action(tr("In&vert"), this, [this](bool) {
			invert();
		}));

		setToOneAction_ = new_action(tr("Set to &1"), this, [this](bool) {
			setToOne();
		});

		menuItems_.push_front(setToOneAction_);

		setToZeroAction_ = new_action(tr("&Zero"), this, [this](bool) {
			setZero();
		});

		menuItems_.push_front(setToZeroAction_);

		menuItems_.front()->setShortcut(QKeySequence(SetToZeroKey));

		menuItems_.push_front(new_action(tr("&Decrement"), this, [this](bool) {
			decrement();
		}));

		menuItems_.front()->setShortcut(QKeySequence(DecrementKey));

		menuItems_.push_front(new_action(tr("&Increment"), this, [this](bool) {
			increment();
		}));

		menuItems_.front()->setShortcut(QKeySequence(IncrementKey));
	}
}

RegisterViewModelBase::Model *ValueField::model() const {
	using namespace RegisterViewModelBase;
	const auto model = static_cast<const Model *>(index_.model());
	// The model is not supposed to have been created as const object,
	// and our manipulations won't invalidate the index.
	// Thus cast the const away.
	return const_cast<Model *>(model);
}

ValueField *ValueField::bestNeighbor(const std::function<bool(const QPoint &, const ValueField *, const QPoint &)> &firstIsBetter) const {
	ValueField *result = nullptr;
	Q_FOREACH (const auto neighbor, regView()->valueFields()) {
		if (neighbor->isVisible() && firstIsBetter(field_position(neighbor), result, field_position(this))) {
			result = neighbor;
		}
	}
	return result;
}

ValueField *ValueField::up() const {
	return bestNeighbor([](const QPoint &nPos, const ValueField *up, const QPoint &fPos) {
		return nPos.y() < fPos.y() && (!up || distance_squared(nPos, fPos) < distance_squared(field_position(up), fPos));
	});
}

ValueField *ValueField::down() const {
	return bestNeighbor([](const QPoint &nPos, const ValueField *down, const QPoint &fPos) {
		return nPos.y() > fPos.y() && (!down || distance_squared(nPos, fPos) < distance_squared(field_position(down), fPos));
	});
}

ValueField *ValueField::left() const {
	return bestNeighbor([](const QPoint &nPos, const ValueField *left, const QPoint &fPos) {
		return nPos.y() == fPos.y() && nPos.x() < fPos.x() && (!left || left->x() < nPos.x());
	});
}

ValueField *ValueField::right() const {
	return bestNeighbor([](const QPoint &nPos, const ValueField *right, const QPoint &fPos) {
		return nPos.y() == fPos.y() && nPos.x() > fPos.x() && (!right || right->x() > nPos.x());
	});
}

QString ValueField::text() const {
	return valueFormatter_(FieldWidget::text());
}

bool ValueField::changed() const {
	if (!index_.isValid()) {
		return true;
	}

	return valid_variant(index_.data(RegisterViewModelBase::Model::RegisterChangedRole)).toBool();
}

QColor ValueField::fgColorForChangedField() const {
	return Qt::red; // TODO: read from user palette
}

bool ValueField::isSelected() const {
	return selected_;
}

void ValueField::editNormalReg(const QModelIndex &indexToEdit, const QModelIndex &clickedIndex) const {
	using namespace RegisterViewModelBase;

	const auto rV = model()->data(indexToEdit, Model::ValueAsRegisterRole);
	if (!rV.isValid()) {
		return;
	}

	auto r = rV.value<Register>();
	if (!r) {
		return;
	}

	if ((r.type() != Register::TYPE_SIMD) && r.bitSize() <= 64) {

		const auto gprEdit = regView()->gprEditDialog();
		gprEdit->setValue(r);
		if (gprEdit->exec() == QDialog::Accepted) {
			r = gprEdit->value();
			model()->setData(indexToEdit, QVariant::fromValue(r), Model::ValueAsRegisterRole);
		}
	} else if (r.type() == Register::TYPE_SIMD) {
		const auto simdEdit = regView()->simdEditDialog();
		simdEdit->setValue(r);
		const int size         = valid_variant(indexToEdit.parent().data(Model::ChosenSIMDSizeRole)).toInt();
		const int format       = valid_variant(indexToEdit.parent().data(Model::ChosenSIMDFormatRole)).toInt();
		const int elementIndex = clickedIndex.row();
		simdEdit->set_current_element(static_cast<Model::ElementSize>(size), static_cast<NumberDisplayMode>(format), elementIndex);
		if (simdEdit->exec() == QDialog::Accepted) {
			r = simdEdit->value();
			model()->setData(indexToEdit, QVariant::fromValue(r), Model::ValueAsRegisterRole);
		}
	}
#if defined(EDB_X86) || defined(EDB_X86_64)
	else if (r.type() == Register::TYPE_FPU) {
		const auto fpuEdit = regView()->fpuEditDialog();
		fpuEdit->setValue(r);
		if (fpuEdit->exec() == QDialog::Accepted) {
			r = fpuEdit->value();
			model()->setData(indexToEdit, QVariant::fromValue(r), Model::ValueAsRegisterRole);
		}
	}
#endif
}

QModelIndex ValueField::regIndex() const {
	using namespace RegisterViewModelBase;

	if (index_.data(Model::IsBitFieldRole).toBool()) {
		return index_;
	}

	if (index_.data(Model::IsNormalRegisterRole).toBool()) {
		return index_.sibling(index_.row(), ModelNameColumn);
	}

	return {};
}

void ValueField::defaultAction() {
	using namespace RegisterViewModelBase;
	if (index_.data(Model::IsBitFieldRole).toBool() && index_.data(Model::BitFieldLengthRole).toInt() == 1) {
		// toggle
		// TODO: Model: make it possible to set bit field itself, without manipulating parent directly
		//              I.e. set value without knowing field offset, then setData(fieldIndex,word)
		const auto regIndex = index_.parent().sibling(index_.parent().row(), ModelValueColumn);
		auto byteArr        = regIndex.data(Model::RawValueRole).toByteArray();
		if (byteArr.isEmpty())
			return;
		std::uint64_t word(0);
		std::memcpy(&word, byteArr.constData(), byteArr.size());
		const auto offset = valid_variant(index_.data(Model::BitFieldOffsetRole)).toInt();
		word ^= 1ull << offset;
		std::memcpy(byteArr.data(), &word, byteArr.size());
		model()->setData(regIndex, byteArr, Model::RawValueRole);
	} else if (index_.data(Model::IsNormalRegisterRole).toBool()) {
		editNormalReg(index_, index_);
	} else if (index_.data(Model::IsSIMDElementRole).toBool()) {
		editNormalReg(index_.parent().parent(), index_);
	} else if (index_.parent().data(Model::IsFPURegisterRole).toBool()) {
		editNormalReg(index_.parent(), index_);
	}
}

void ValueField::adjustToData() {
	if (index_.parent().data().toString() == GprCategoryName) {
		using RegisterViewModelBase::Model;
		auto byteArr = index_.data(Model::RawValueRole).toByteArray();
		if (byteArr.isEmpty()) {
			return;
		}

		std::uint64_t value(0);
		assert(byteArr.size() <= int(sizeof(value)));
		std::memcpy(&value, byteArr.constData(), byteArr.size());

		setToOneAction_->setVisible(value != 1u);
		setToZeroAction_->setVisible(value != 0u);
	}
	FieldWidget::adjustToData();
	updatePalette();
}

void ValueField::updatePalette() {
	if (changed()) {
		auto palette                = this->palette();
		const QColor changedFGColor = fgColorForChangedField();
		palette.setColor(foregroundRole(), changedFGColor);
		palette.setColor(QPalette::HighlightedText, changedFGColor);
		setPalette(palette);
	} else
		setPalette(QApplication::palette());

	QLabel::update();
}

void ValueField::enterEvent(QEvent *) {
	hovered_ = true;
	updatePalette();
}

void ValueField::leaveEvent(QEvent *) {
	hovered_ = false;
	updatePalette();
}

void ValueField::select() {
	if (selected_) {
		return;
	}

	selected_ = true;
	model()->setActiveIndex(regIndex());
	Q_EMIT selected();
	updatePalette();
}

void ValueField::showMenu(const QPoint &position) {
	group()->showMenu(position, menuItems_);
}

void ValueField::mousePressEvent(QMouseEvent *event) {
	if (event->button() & (Qt::LeftButton | Qt::RightButton)) {
		select();
	}

	if (event->button() == Qt::RightButton && event->type() != QEvent::MouseButtonDblClick) {
		showMenu(event->globalPos());
	}
}

void ValueField::unselect() {
	if (!selected_) {
		return;
	}

	selected_ = false;
	updatePalette();
}

void ValueField::mouseDoubleClickEvent(QMouseEvent *event) {
	mousePressEvent(event);
	defaultAction();
}

void ValueField::paintEvent(QPaintEvent *) {
	const auto regView = this->regView();
	QPainter painter(this);

	QStyleOptionViewItem option;

	option.rect                   = rect();
	option.showDecorationSelected = true;
	option.text                   = text();
	option.font                   = font();
	option.palette                = palette();
	option.textElideMode          = Qt::ElideNone;
	option.state |= QStyle::State_Enabled;
	option.displayAlignment = alignment();

	if (selected_) {
		option.state |= QStyle::State_Selected;
	}

	if (hovered_) {
		option.state |= QStyle::State_MouseOver;
	}

	if (regView->hasFocus()) {
		option.state |= QStyle::State_Active;
	}

	QApplication::style()->drawControl(QStyle::CE_ItemViewItem, &option, &painter);
}

namespace {

void add_to_top(RegisterViewModelBase::Model *model, const QModelIndex &fsrIndex, std::int16_t delta) {

	using namespace RegisterViewModelBase;

	// TODO: Model: make it possible to set bit field itself, without manipulating parent directly
	//              I.e. set value without knowing field offset, then setData(fieldIndex,word)
	auto byteArr = fsrIndex.data(Model::RawValueRole).toByteArray();
	if (byteArr.isEmpty())
		return;

	std::uint16_t word(0);
	assert(byteArr.size() == sizeof(word));
	std::memcpy(&word, byteArr.constData(), byteArr.size());
	const auto value = (word >> 11) & 7;
	word             = (word & ~0x3800) | (((value + delta) & 7) << 11);
	std::memcpy(byteArr.data(), &word, byteArr.size());
	model->setData(fsrIndex, byteArr, Model::RawValueRole);
}

}

#if defined(EDB_X86) || defined(EDB_X86_64)
void ValueField::pushFPUStack() {
	assert(index_.sibling(index_.row(), ModelNameColumn).data().toString() == FsrName);
	add_to_top(model(), index_, -1);
}

void ValueField::popFPUStack() {
	assert(index_.sibling(index_.row(), ModelNameColumn).data().toString() == FsrName);
	add_to_top(model(), index_, +1);
}
#endif

void ValueField::copyToClipboard() const {
	QApplication::clipboard()->setText(text());
}

namespace {

template <typename Op>
void change_gpr(const QModelIndex &index, RegisterViewModelBase::Model *const model, const Op &change) {

	if (index.parent().data().toString() != GprCategoryName) {
		return;
	}

	using RegisterViewModelBase::Model;
	auto byteArr = index.data(Model::RawValueRole).toByteArray();

	if (byteArr.isEmpty()) {
		return;
	}

	std::uint64_t value(0);
	assert(byteArr.size() <= int(sizeof(value)));
	std::memcpy(&value, byteArr.constData(), byteArr.size());
	value = change(value);
	std::memcpy(byteArr.data(), &value, byteArr.size());
	model->setData(index, byteArr, Model::RawValueRole);
}

}

void ValueField::decrement() {
	change_gpr(index_, model(), [](std::uint64_t v) {
		return v - 1;
	});
}

void ValueField::increment() {
	change_gpr(index_, model(), [](std::uint64_t v) {
		return v + 1;
	});
}

void ValueField::invert() {
	change_gpr(index_, model(), [](std::uint64_t v) {
		return ~v;
	});
}

void ValueField::setZero() {
	change_gpr(index_, model(), [](int) {
		return 0;
	});
}

void ValueField::setToOne() {
	change_gpr(index_, model(), [](int) {
		return 1;
	});
}

}
