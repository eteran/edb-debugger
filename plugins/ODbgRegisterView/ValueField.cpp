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
#include "edb.h"
#include "ODbgRV_Common.h"
#include "ODbgRV_Util.h"
#if defined EDB_X86 || defined EDB_X86_64
#	include "ODbgRV_x86Common.h"
#	include "DialogEditFPU.h"
#endif
#include "DialogEditGPR.h"
#include "DialogEditSIMDRegister.h"
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

ValueField::ValueField(int fieldWidth,
                       const QModelIndex &index,
                       QWidget *parent,
                       const std::function<QString(const QString &)> &valueFormatter)
	: FieldWidget(fieldWidth, index, parent),
	  valueFormatter(valueFormatter)
{
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
		menuItems.push_back(newAction(trUtf8("&Modify…"), this, this, SLOT(defaultAction())));
		menuItems.back()->setShortcut(QKeySequence(Qt::Key_Enter));
	} else if (index.data(Model::IsBitFieldRole).toBool() && index.data(Model::BitFieldLengthRole).toInt() == 1) {
		menuItems.push_back(newAction(tr("&Toggle"), this, this, SLOT(defaultAction())));
		menuItems.back()->setShortcut(QKeySequence(Qt::Key_Enter));
	}

	menuItems.push_back(newAction(tr("&Copy to clipboard"), this, this, SLOT(copyToClipboard())));
	menuItems.back()->setShortcut(copyFieldShortcut);

#if defined EDB_X86 || defined EDB_X86_64
	if (index.sibling(index.row(), MODEL_NAME_COLUMN).data().toString() == FSR_NAME) {
		menuItems.push_back(newAction(tr("P&ush FPU stack"), this, this, SLOT(pushFPUStack())));
		menuItems.push_back(newAction(tr("P&op FPU stack"), this, this, SLOT(popFPUStack())));
	}
#endif

	if (index.parent().data().toString() == GPRCategoryName) {
		// These should be above others, so prepending instead of appending
		menuItems.push_front(newAction(tr("In&vert"), this, this, SLOT(invert())));

		menuItems.push_front(newAction(tr("Follow Register in Dump"), this, this, SLOT(followRegisterInDump())));

		menuItems.push_front(newAction(tr("Follow Register in Stack"), this, this, SLOT(followRegisterInStack())));

		menuItems.push_front(setToOneAction = newAction(tr("Set to &1"), this, this, SLOT(setToOne())));

		menuItems.push_front(setToZeroAction = newAction(tr("&Zero"), this, this, SLOT(setZero())));
		menuItems.front()->setShortcut(QKeySequence(setToZeroKey));

		menuItems.push_front(newAction(tr("&Decrement"), this, this, SLOT(decrement())));
		menuItems.front()->setShortcut(QKeySequence(decrementKey));

		menuItems.push_front(newAction(tr("&Increment"), this, this, SLOT(increment())));
		menuItems.front()->setShortcut(QKeySequence(incrementKey));
	}
}

RegisterViewModelBase::Model *ValueField::model() const {
	using namespace RegisterViewModelBase;
	const auto model = static_cast<const Model *>(index.model());
	// The model is not supposed to have been created as const object,
	// and our manipulations won't invalidate the index.
	// Thus cast the const away.
	return const_cast<Model *>(model);
}

ValueField *ValueField::bestNeighbor(const std::function<bool(const QPoint &, const ValueField *, const QPoint &)> &firstIsBetter) const {
	ValueField *result = nullptr;
	Q_FOREACH(const auto neighbor, regView()->valueFields()) {
		if (neighbor->isVisible() && firstIsBetter(fieldPos(neighbor), result, fieldPos(this)))
			result = neighbor;
	}
	return result;
}

ValueField *ValueField::up() const {
	return bestNeighbor([](const QPoint &nPos, const ValueField *up, const QPoint &fPos)
						{ return nPos.y() < fPos.y() && (!up || distSqr(nPos, fPos) < distSqr(fieldPos(up), fPos)); }
						);
}

ValueField *ValueField::down() const {
	return bestNeighbor([](const QPoint &nPos, const ValueField *down, const QPoint &fPos)
			{ return nPos.y() > fPos.y() && (!down || distSqr(nPos, fPos) < distSqr(fieldPos(down), fPos)); }
						);
}

ValueField *ValueField::left() const {
	return bestNeighbor([](const QPoint &nPos, const ValueField *left, const QPoint &fPos)
						{ return nPos.y() == fPos.y() && nPos.x() < fPos.x() && (!left || left->x() < nPos.x()); }
						);
}

ValueField *ValueField::right() const {
	return bestNeighbor([](const QPoint &nPos, const ValueField *right, const QPoint &fPos)
			{ return nPos.y() == fPos.y() && nPos.x() > fPos.x() && (!right || right->x() > nPos.x()); }
						);
}

QString ValueField::text() const {
	return valueFormatter(FieldWidget::text());
}

bool ValueField::changed() const {
	if (!index.isValid())
		return true;
	return VALID_VARIANT(index.data(RegisterViewModelBase::Model::RegisterChangedRole)).toBool();
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
	if (!rV.isValid())
		return;
	auto r = rV.value<Register>();
	if (!r)
		return;

	if ((r.type() != Register::TYPE_SIMD) && r.bitSize() <= 64) {

		const auto gprEdit = regView()->gprEditDialog();
		gprEdit->set_value(r);
		if (gprEdit->exec() == QDialog::Accepted) {
			r = gprEdit->value();
			model()->setData(indexToEdit, QVariant::fromValue(r), Model::ValueAsRegisterRole);
		}
	} else if (r.type() == Register::TYPE_SIMD) {
		const auto simdEdit = regView()->simdEditDialog();
		simdEdit->set_value(r);
		const int size         = VALID_VARIANT(indexToEdit.parent().data(Model::ChosenSIMDSizeRole)).toInt();
		const int format       = VALID_VARIANT(indexToEdit.parent().data(Model::ChosenSIMDFormatRole)).toInt();
		const int elementIndex = clickedIndex.row();
		simdEdit->set_current_element(static_cast<Model::ElementSize>(size), static_cast<NumberDisplayMode>(format), elementIndex);
		if (simdEdit->exec() == QDialog::Accepted) {
			r = simdEdit->value();
			model()->setData(indexToEdit, QVariant::fromValue(r), Model::ValueAsRegisterRole);
		}
	}
#if defined EDB_X86 || defined EDB_X86_64
	else if (r.type() == Register::TYPE_FPU) {
		const auto fpuEdit = regView()->fpuEditDialog();
		fpuEdit->set_value(r);
		if (fpuEdit->exec() == QDialog::Accepted) {
			r = fpuEdit->value();
			model()->setData(indexToEdit, QVariant::fromValue(r), Model::ValueAsRegisterRole);
		}
	}
#endif
}

QModelIndex ValueField::regIndex() const {
	using namespace RegisterViewModelBase;

	if (index.data(Model::IsBitFieldRole).toBool())
		return index;
	if (index.data(Model::IsNormalRegisterRole).toBool())
		return index.sibling(index.row(), MODEL_NAME_COLUMN);
	return {};
}

void ValueField::defaultAction() {
	using namespace RegisterViewModelBase;
	if (index.data(Model::IsBitFieldRole).toBool() && index.data(Model::BitFieldLengthRole).toInt() == 1) {
		// toggle
		// TODO: Model: make it possible to set bit field itself, without manipulating parent directly
		//              I.e. set value without knowing field offset, then setData(fieldIndex,word)
		const auto regIndex = index.parent().sibling(index.parent().row(), MODEL_VALUE_COLUMN);
		auto       byteArr  = regIndex.data(Model::RawValueRole).toByteArray();
		if (byteArr.isEmpty())
			return;
		std::uint64_t word(0);
		std::memcpy(&word, byteArr.constData(), byteArr.size());
		const auto offset = VALID_VARIANT(index.data(Model::BitFieldOffsetRole)).toInt();
		word ^= 1ull << offset;
		std::memcpy(byteArr.data(), &word, byteArr.size());
		model()->setData(regIndex, byteArr, Model::RawValueRole);
	} else if (index.data(Model::IsNormalRegisterRole).toBool())
		editNormalReg(index, index);
	else if (index.data(Model::IsSIMDElementRole).toBool())
		editNormalReg(index.parent().parent(), index);
	else if (index.parent().data(Model::IsFPURegisterRole).toBool())
		editNormalReg(index.parent(), index);
}

void ValueField::adjustToData() {
	if (index.parent().data().toString() == GPRCategoryName) {
		using RegisterViewModelBase::Model;
		auto byteArr = index.data(Model::RawValueRole).toByteArray();
		if (byteArr.isEmpty())
			return;
		std::uint64_t value(0);
		assert(byteArr.size() <= int(sizeof(value)));
		std::memcpy(&value, byteArr.constData(), byteArr.size());

		setToOneAction->setVisible(value != 1u);
		setToZeroAction->setVisible(value != 0u);
	}
	FieldWidget::adjustToData();
	updatePalette();
}

void ValueField::updatePalette() {
	if (changed()) {
		auto         palette        = this->palette();
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
	if (selected_)
		return;
	selected_ = true;
	model()->setActiveIndex(regIndex());
	Q_EMIT selected();
	updatePalette();
}

void ValueField::showMenu(const QPoint &position) {
	group()->showMenu(position, menuItems);
}

void ValueField::mousePressEvent(QMouseEvent *event) {
	if (event->button() & (Qt::LeftButton | Qt::RightButton))
		select();
	if (event->button() == Qt::RightButton && event->type() != QEvent::MouseButtonDblClick)
		showMenu(event->globalPos());
}

void ValueField::unselect() {
	if (!selected_)
		return;
	selected_ = false;
	updatePalette();
}

void ValueField::mouseDoubleClickEvent(QMouseEvent *event) {
	mousePressEvent(event);
	defaultAction();
}

void ValueField::paintEvent(QPaintEvent *) {
	const auto            regView = this->regView();
	QPainter              painter(this);

	QStyleOptionViewItem option;

	option.rect                   = rect();
	option.showDecorationSelected = true;
	option.text                   = text();
	option.font                   = font();
	option.palette                = palette();
	option.textElideMode          = Qt::ElideNone;
	option.state |= QStyle::State_Enabled;
	option.displayAlignment = alignment();
	if (selected_)
		option.state |= QStyle::State_Selected;
	if (hovered_)
		option.state |= QStyle::State_MouseOver;
	if (regView->hasFocus())
		option.state |= QStyle::State_Active;
	QApplication::style()->drawControl(QStyle::CE_ItemViewItem, &option, &painter);
}

namespace {

void addToTOP(RegisterViewModelBase::Model *model, const QModelIndex &fsrIndex, std::int16_t delta) {

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

#if defined EDB_X86 || defined EDB_X86_64
void ValueField::pushFPUStack() {
	assert(index.sibling(index.row(), MODEL_NAME_COLUMN).data().toString() == FSR_NAME);
	addToTOP(model(), index, -1);
}

void ValueField::popFPUStack() {
	assert(index.sibling(index.row(), MODEL_NAME_COLUMN).data().toString() == FSR_NAME);
	addToTOP(model(), index, +1);
}
#endif

void ValueField::copyToClipboard() const {
	QApplication::clipboard()->setText(text());
}

namespace {

template <typename Op>
void changeGPR(const QModelIndex &index, RegisterViewModelBase::Model *const model, const Op &change) {
	if (index.parent().data().toString() != GPRCategoryName)
		return;
	using RegisterViewModelBase::Model;
	auto byteArr = index.data(Model::RawValueRole).toByteArray();
	if (byteArr.isEmpty())
		return;
	std::uint64_t value(0);
	assert(byteArr.size() <= int(sizeof(value)));
	std::memcpy(&value, byteArr.constData(), byteArr.size());
	value = change(value);
	std::memcpy(byteArr.data(), &value, byteArr.size());
	model->setData(index, byteArr, Model::RawValueRole);
}
void getRegisterValue(const QModelIndex &index, std::uint64_t* value)
{
	using RegisterViewModelBase::Model;
	auto byteArr = index.data(Model::RawValueRole).toByteArray();
	if (byteArr.isEmpty())
		return;
	assert(byteArr.size() <= int(sizeof(*value)));
	std::memcpy(value, byteArr.constData(), byteArr.size());
}
}

void ValueField::decrement() {
	changeGPR(index, model(), [](std::uint64_t v) { return v - 1; });
}

void ValueField::increment() {
	changeGPR(index, model(), [](std::uint64_t v) { return v + 1; });
}

void ValueField::invert() {
	changeGPR(index, model(), [](std::uint64_t v) { return ~v; });
}
void ValueField::followRegisterInDump() {
	std::uint64_t value(0);
	getRegisterValue(index, &value);
	edb::v1::dump_data(value,true);
}
void ValueField::followRegisterInStack() {
	std::uint64_t value(0);
	getRegisterValue(index, &value);
	edb::v1::dump_stack(value);
}

void ValueField::setZero() {
	changeGPR(index, model(), [](int) { return 0; });
}

void ValueField::setToOne() {
	changeGPR(index, model(), [](int) { return 1; });
}

}
