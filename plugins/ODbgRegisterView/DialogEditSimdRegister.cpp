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

#include "DialogEditSimdRegister.h"
#include "EntryGridKeyUpDownEventFilter.h"
#include "FloatX.h"
#include "NumberEdit.h"
#include "QLongValidator.h"
#include "QULongValidator.h"

#include <QApplication>
#include <QDebug>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QRadioButton>
#include <QRegExp>
#include <QRegExpValidator>
#include <cstring>
#include <limits>
#include <type_traits>

namespace ODbgRegisterView {

template <std::size_t NumEntries, class Func>
void DialogEditSimdRegister::setupEntries(const QString &label, std::array<NumberEdit *, NumEntries> &entries, int row, Func slot, int naturalWidthInChars) {

	auto contentsGrid = qobject_cast<QGridLayout *>(layout());

	contentsGrid->addWidget(new QLabel(label, this), row, ENTRIES_FIRST_COL - 1);
	for (std::size_t entryIndex = 0; entryIndex < NumEntries; ++entryIndex) {
		auto &entry             = entries[entryIndex];
		const int bytesPerEntry = NumBytes / NumEntries;
		entry                   = new NumberEdit(ENTRIES_FIRST_COL + bytesPerEntry * (NumEntries - 1 - entryIndex), bytesPerEntry, this);
		entry->setNaturalWidthInChars(naturalWidthInChars);
		connect(entry, &NumberEdit::textEdited, this, slot);
		entry->installEventFilter(this);
	}
}

DialogEditSimdRegister::DialogEditSimdRegister(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f),
	  byteHexValidator_(new QRegExpValidator(QRegExp("[0-9a-fA-F]{0,2}"), this)),
	  wordHexValidator_(new QRegExpValidator(QRegExp("[0-9a-fA-F]{0,4}"), this)),
	  dwordHexValidator_(new QRegExpValidator(QRegExp("[0-9a-fA-F]{0,8}"), this)),
	  qwordHexValidator_(new QRegExpValidator(QRegExp("[0-9a-fA-F]{0,16}"), this)),
	  byteSignedValidator_(new QLongValidator(INT8_MIN, INT8_MAX, this)),
	  wordSignedValidator_(new QLongValidator(INT16_MIN, INT16_MAX, this)),
	  dwordSignedValidator_(new QLongValidator(INT32_MIN, INT32_MAX, this)),
	  qwordSignedValidator_(new QLongValidator(INT64_MIN, INT64_MAX, this)),
	  byteUnsignedValidator_(new QULongValidator(0, UINT8_MAX, this)),
	  wordUnsignedValidator_(new QULongValidator(0, UINT16_MAX, this)),
	  dwordUnsignedValidator_(new QULongValidator(0, UINT32_MAX, this)),
	  qwordUnsignedValidator_(new QULongValidator(0, UINT64_MAX, this)),
	  float32Validator_(new FloatXValidator<float>(this)),
	  float64Validator_(new FloatXValidator<double>(this)),
	  intMode_(NumberDisplayMode::Hex) {

	setWindowTitle(tr("Edit SIMD Register"));
	setModal(true);
	const auto allContentsGrid = new QGridLayout(this);

	for (int byteIndex = 0; byteIndex < NumBytes; ++byteIndex) {
		columnLabels_[byteIndex] = new QLabel(std::to_string(byteIndex).c_str(), this);
		columnLabels_[byteIndex]->setAlignment(Qt::AlignCenter);
		allContentsGrid->addWidget(columnLabels_[byteIndex], BYTE_INDICES_ROW, ENTRIES_FIRST_COL + NumBytes - 1 - byteIndex);
	}

	setupEntries(
		tr("Byte"), bytes_, BYTES_ROW, [this]() {
			onByteEdited();
		},
		4);

	setupEntries(
		tr("Word"), words_, WORDS_ROW, [this]() {
			onWordEdited();
		},
		6);

	setupEntries(
		tr("Doubleword"), dwords_, DWORDS_ROW, [this]() {
			onDwordEdited();
		},
		11);
	setupEntries(
		tr("Quadword"), qwords_, QWORDS_ROW, [this]() {
			onQwordEdited();
		},
		21);
	setupEntries(
		tr("float32"), floats32_, FLOATS32_ROW, [this]() {
			onFloat32Edited();
		},
		14);
	setupEntries(
		tr("float64"), floats64_, FLOATS64_ROW, [this]() {
			onFloat64Edited();
		},
		24);

	for (const auto &entry : floats32_) {
		entry->setValidator(float32Validator_);
	}

	for (const auto &entry : floats64_) {
		entry->setValidator(float64Validator_);
	}

	hexSignOKCancelLayout_ = new QHBoxLayout();

	{
		const auto hexSignRadiosLayout = new QVBoxLayout();
		radioHex_                      = new QRadioButton(tr("Hexadecimal"), this);
		connect(radioHex_, &QRadioButton::toggled, this, &DialogEditSimdRegister::onHexToggled);
		// setChecked must be called after connecting of toggled()
		// in order to set validators for integer editors
		radioHex_->setChecked(true);

		hexSignRadiosLayout->addWidget(radioHex_);

		radioSigned_ = new QRadioButton(tr("Signed"), this);
		connect(radioSigned_, &QRadioButton::toggled, this, &DialogEditSimdRegister::onSignedToggled);
		hexSignRadiosLayout->addWidget(radioSigned_);

		radioUnsigned_ = new QRadioButton(tr("Unsigned"), this);
		connect(radioUnsigned_, &QRadioButton::toggled, this, &DialogEditSimdRegister::onUnsignedToggled);
		hexSignRadiosLayout->addWidget(radioUnsigned_);

		hexSignOKCancelLayout_->addLayout(hexSignRadiosLayout);
	}

	{
		const auto okCancelLayout = new QVBoxLayout();
		okCancelLayout->addItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));

		okCancel_ = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok, Qt::Horizontal, this);
		connect(okCancel_, &QDialogButtonBox::accepted, this, &DialogEditSimdRegister::accept);
		connect(okCancel_, &QDialogButtonBox::rejected, this, &DialogEditSimdRegister::reject);
		okCancelLayout->addWidget(okCancel_);

		hexSignOKCancelLayout_->addLayout(okCancelLayout);
	}

	resetLayout();

	for (int byte = NumBytes - 1; byte > 0; --byte) {
		setTabOrder(bytes_[byte], bytes_[byte - 1]);
	}
	setTabOrder(bytes_.back(), words_.front());

	for (int word = NumBytes / 2 - 1; word > 0; --word) {
		setTabOrder(words_[word], words_[word - 1]);
	}
	setTabOrder(words_.back(), dwords_.front());

	for (int dword = NumBytes / 4 - 1; dword > 0; --dword) {
		setTabOrder(dwords_[dword], dwords_[dword - 1]);
	}
	setTabOrder(dwords_.back(), qwords_.front());

	for (int qword = NumBytes / 8 - 1; qword > 0; --qword) {
		setTabOrder(qwords_[qword], qwords_[qword - 1]);
	}
	setTabOrder(qwords_.back(), floats32_.front());

	for (int float32 = NumBytes / 4 - 1; float32 > 0; --float32) {
		setTabOrder(floats32_[float32], floats32_[float32 - 1]);
	}
	setTabOrder(floats32_.back(), floats64_.front());

	for (int float64 = NumBytes / 8 - 1; float64 > 0; --float64) {
		setTabOrder(floats64_[float64], floats64_[float64 - 1]);
	}
	setTabOrder(floats64_.front(), radioHex_);

	setTabOrder(radioHex_, radioSigned_);
	setTabOrder(radioSigned_, radioUnsigned_);
	setTabOrder(radioUnsigned_, okCancel_);
}

template <typename T>
void DialogEditSimdRegister::updateFloatEntries(const std::array<NumberEdit *, NumBytes / sizeof(T)> &entries, NumberEdit *notUpdated) {

	for (std::size_t i = 0; i < entries.size(); ++i) {
		if (entries[i] == notUpdated) {
			continue;
		}

		T value;
		std::memcpy(&value, &value_[i * sizeof(value)], sizeof(value));
		entries[i]->setText(format_float(value));
	}
}

template <typename T>
void DialogEditSimdRegister::updateIntegralEntries(const std::array<NumberEdit *, NumBytes / sizeof(T)> &entries, NumberEdit *notUpdated) {

	for (std::size_t i = 0; i < entries.size(); ++i) {
		if (entries[i] == notUpdated) {
			continue;
		}

		T value;
		std::memcpy(&value, &value_[i * sizeof(value)], sizeof(value));
		formatInteger(entries[i], value);
	}
}

void DialogEditSimdRegister::updateAllEntriesExcept(NumberEdit *notUpdated) {

	if (!reg_) {
		return;
	}

	updateIntegralEntries<std::uint8_t>(bytes_, notUpdated);
	updateIntegralEntries<std::uint16_t>(words_, notUpdated);
	updateIntegralEntries<std::uint32_t>(dwords_, notUpdated);
	updateIntegralEntries<std::uint64_t>(qwords_, notUpdated);
	updateFloatEntries<edb::value32>(floats32_, notUpdated);
	updateFloatEntries<edb::value64>(floats64_, notUpdated);
}

void DialogEditSimdRegister::resetLayout() {

	auto layout = qobject_cast<QGridLayout *>(this->layout());

	for (int col = ENTRIES_FIRST_COL; col < TOTAL_COLS; ++col) {
		int i = NumBytes - 1 - (col - ENTRIES_FIRST_COL);

		columnLabels_[i]->show();

		const auto &byte = bytes_[i];
		layout->addWidget(byte, BYTES_ROW, byte->column(), 1, byte->colSpan());
		byte->show();

		const auto &word = words_[i / 2];
		layout->addWidget(word, WORDS_ROW, word->column(), 1, word->colSpan());
		word->show();

		const auto &dword = dwords_[i / 4];
		layout->addWidget(dword, DWORDS_ROW, dword->column(), 1, dword->colSpan());
		dword->show();

		const auto &qword = qwords_[i / 8];
		layout->addWidget(qword, QWORDS_ROW, qword->column(), 1, qword->colSpan());
		qword->show();

		const auto &float32 = floats32_[i / 4];
		layout->addWidget(float32, FLOATS32_ROW, float32->column(), 1, float32->colSpan());
		float32->show();

		const auto &float64 = floats64_[i / 8];
		layout->addWidget(float64, FLOATS64_ROW, float64->column(), 1, float64->colSpan());
		float64->show();
	}

	for (int row = ENTRIES_FIRST_ROW; row < ROW_AFTER_ENTRIES; ++row)
		layout->itemAtPosition(row, LABELS_COL)->widget()->show();

	layout->removeItem(hexSignOKCancelLayout_);
	hexSignOKCancelLayout_->setParent(nullptr);
	layout->addLayout(hexSignOKCancelLayout_, ROW_AFTER_ENTRIES, ENTRIES_FIRST_COL, 1, NumBytes);
}

void DialogEditSimdRegister::hideColumns(EntriesCols afterLastToHide) {

	auto layout = qobject_cast<QGridLayout *>(this->layout());

	for (int col = ENTRIES_FIRST_COL; col < afterLastToHide; ++col) {
		int i = NumBytes - 1 - (col - ENTRIES_FIRST_COL);
		Q_ASSERT(0 < i && std::size_t(i) < bytes_.size());

		columnLabels_[i]->hide();

		// Spanned entries shouldn't just be hidden. If they are still in the grid,
		// then we get extra spacing between invisible columns, which is unwanted.
		// So we have to also remove them from the layout.
		layout->removeWidget(bytes_[i]);
		bytes_[i]->hide();

		layout->removeWidget(words_[i / 2]);
		words_[i / 2]->hide();

		layout->removeWidget(dwords_[i / 4]);
		dwords_[i / 4]->hide();

		layout->removeWidget(qwords_[i / 8]);
		qwords_[i / 8]->hide();

		layout->removeWidget(floats32_[i / 4]);
		floats32_[i / 4]->hide();

		layout->removeWidget(floats64_[i / 8]);
		floats64_[i / 8]->hide();
	}

	layout->removeItem(hexSignOKCancelLayout_);
	hexSignOKCancelLayout_->setParent(nullptr);
	layout->addLayout(hexSignOKCancelLayout_, ROW_AFTER_ENTRIES, afterLastToHide, 1, TOTAL_COLS - afterLastToHide);
}

void DialogEditSimdRegister::hideRows(EntriesRows rowToHide) {

	auto layout = qobject_cast<QGridLayout *>(this->layout());

	for (int col = 0; col < TOTAL_COLS; ++col) {
		const auto item = layout->itemAtPosition(rowToHide, col);
		if (item && item->widget()) {
			item->widget()->hide();
		}
	}
}

bool DialogEditSimdRegister::eventFilter(QObject *obj, QEvent *event) {
	return entry_grid_key_event_filter(this, obj, event);
}

void DialogEditSimdRegister::setValue(const Register &newReg) {
	resetLayout();
	assert(newReg.bitSize() <= 8 * sizeof(value_));
	reg_ = newReg;
	util::mark_memory(&value_, value_.size());
	if (QRegExp("mm[0-7]").exactMatch(reg_.name())) {
		const auto value = reg_.value<edb::value64>();
		std::memcpy(&value_, &value, sizeof(value));
		hideColumns(MMX_FIRST_COL);
		// MMX registers are never used in float computations, so hide useless rows
		hideRows(FLOATS32_ROW);
		hideRows(FLOATS64_ROW);
	} else if (QRegExp("xmm[0-9]+").exactMatch(reg_.name())) {
		const auto value = reg_.value<edb::value128>();
		std::memcpy(&value_, &value, sizeof(value));
		hideColumns(XMM_FIRST_COL);
	} else if (QRegExp("ymm[0-9]+").exactMatch(reg_.name())) {
		const auto value = reg_.value<edb::value256>();
		std::memcpy(&value_, &value, sizeof(value));
		hideColumns(YMM_FIRST_COL);
	} else
		qCritical() << "DialogEditSimdRegister::setValue(" << reg_.name() << "): register type unsupported";
	setWindowTitle(tr("Modify %1").arg(reg_.name().toUpper()));
	updateAllEntriesExcept(nullptr);
}

void DialogEditSimdRegister::set_current_element(RegisterViewModelBase::Model::ElementSize size, NumberDisplayMode format, int elementIndex) {
	using namespace RegisterViewModelBase;
	if (format != intMode_ && format != NumberDisplayMode::Float) {
		switch (format) {
		case NumberDisplayMode::Hex:
			radioHex_->setChecked(true);
			break;
		case NumberDisplayMode::Signed:
			radioSigned_->setChecked(true);
			break;
		case NumberDisplayMode::Unsigned:
			radioUnsigned_->setChecked(true);
			break;
		case NumberDisplayMode::Float:
			break; // silence the compiler, we'll never get here
		}
	}

	NumberEdit *edit = bytes_[0];

	if (format == NumberDisplayMode::Float) {
		edit = floats32_[0];
		if (size == Model::ElementSize::DWORD)
			edit = floats32_[elementIndex];
		else if (size == Model::ElementSize::QWORD)
			edit = floats64_[elementIndex];
	} else {
		switch (size) {
		case Model::ElementSize::BYTE:
			edit = bytes_[elementIndex];
			break;
		case Model::ElementSize::WORD:
			edit = words_[elementIndex];
			break;
		case Model::ElementSize::DWORD:
			edit = dwords_[elementIndex];
			break;
		case Model::ElementSize::QWORD:
			edit = qwords_[elementIndex];
			break;
		default:
			EDB_PRINT_AND_DIE("Unexpected size ", static_cast<long>(size));
		}
	}
	edit->setFocus(Qt::OtherFocusReason);
}

std::uint64_t DialogEditSimdRegister::readInteger(const NumberEdit *const edit) const {
	bool ok;
	switch (intMode_) {
	case NumberDisplayMode::Hex:
		return edit->text().toULongLong(&ok, 16);
	case NumberDisplayMode::Signed:
		return edit->text().toLongLong(&ok);
	case NumberDisplayMode::Unsigned:
		return edit->text().toULongLong(&ok);
	default:
		Q_ASSERT("Unexpected integer display mode" && 0);
		return 0xbadbadbadbadbad1;
	}
}

template <typename Integer>
void DialogEditSimdRegister::formatInteger(NumberEdit *const edit, Integer integer) const {
	switch (intMode_) {
	case NumberDisplayMode::Hex:
		edit->setText(QString("%1").arg(integer, 2 * sizeof(integer), 16, QChar('0')));
		break;
	case NumberDisplayMode::Signed:
		using Int    = typename std::remove_reference<Integer>::type;
		using Signed = typename std::make_signed<Int>::type;

		edit->setText(QString("%1").arg(static_cast<Signed>(integer)));
		break;
	case NumberDisplayMode::Unsigned:
		edit->setText(QString("%1").arg(integer));
		break;
	default:
		Q_ASSERT("Unexpected integer display mode" && 0);
		return;
	}
}

template <typename Integer>
void DialogEditSimdRegister::onIntegerEdited(QObject *sender, const std::array<NumberEdit *, NumBytes / sizeof(Integer)> &elements) {
	const auto changedElementEdit = qobject_cast<NumberEdit *>(sender);
	std::size_t elementIndex      = std::find(elements.begin(), elements.end(), changedElementEdit) - elements.begin();
	Integer value                 = readInteger(elements[elementIndex]);
	std::memcpy(&value_[elementIndex * sizeof(value)], &value, sizeof(value));
	updateAllEntriesExcept(elements[elementIndex]);
}

template <typename Float>
void DialogEditSimdRegister::onFloatEdited(QObject *sender, const std::array<NumberEdit *, NumBytes / sizeof(Float)> &elements) {
	const auto changedFloatEdit = qobject_cast<NumberEdit *>(sender);
	std::size_t floatIndex      = std::find(elements.begin(), elements.end(), changedFloatEdit) - elements.begin();
	bool ok                     = false;
	auto value                  = read_float<Float>(elements[floatIndex]->text(), ok);
	if (ok) {
		std::memcpy(&value_[floatIndex * sizeof(value)], &value, sizeof(value));
		updateAllEntriesExcept(elements[floatIndex]);
	}
}

void DialogEditSimdRegister::onByteEdited() {
	onIntegerEdited<std::uint8_t>(sender(), bytes_);
}

void DialogEditSimdRegister::onWordEdited() {
	onIntegerEdited<std::uint16_t>(sender(), words_);
}

void DialogEditSimdRegister::onDwordEdited() {
	onIntegerEdited<std::uint32_t>(sender(), dwords_);
}

void DialogEditSimdRegister::onQwordEdited() {
	onIntegerEdited<std::uint64_t>(sender(), qwords_);
}

void DialogEditSimdRegister::onFloat32Edited() {
	onFloatEdited<float>(sender(), floats32_);
}

void DialogEditSimdRegister::onFloat64Edited() {
	onFloatEdited<double>(sender(), floats64_);
}

void DialogEditSimdRegister::onHexToggled(bool checked) {
	if ((checked && intMode_ != NumberDisplayMode::Hex) || !bytes_.front()->validator()) {
		intMode_ = NumberDisplayMode::Hex;
		for (const auto &byte : bytes_)
			byte->setValidator(byteHexValidator_);
		for (const auto &word : words_)
			word->setValidator(wordHexValidator_);
		for (const auto &dword : dwords_)
			dword->setValidator(dwordHexValidator_);
		for (const auto &qword : qwords_)
			qword->setValidator(qwordHexValidator_);
		updateAllEntriesExcept(nullptr);
	}
}

void DialogEditSimdRegister::onSignedToggled(bool checked) {
	if ((checked && intMode_ != NumberDisplayMode::Signed) || !bytes_.front()->validator()) {
		intMode_ = NumberDisplayMode::Signed;
		for (const auto &byte : bytes_)
			byte->setValidator(byteSignedValidator_);
		for (const auto &word : words_)
			word->setValidator(wordSignedValidator_);
		for (const auto &dword : dwords_)
			dword->setValidator(dwordSignedValidator_);
		for (const auto &qword : qwords_)
			qword->setValidator(qwordSignedValidator_);
		updateAllEntriesExcept(nullptr);
	}
}

void DialogEditSimdRegister::onUnsignedToggled(bool checked) {
	if ((checked && intMode_ != NumberDisplayMode::Unsigned) || !bytes_.front()->validator()) {
		intMode_ = NumberDisplayMode::Unsigned;
		for (const auto &byte : bytes_)
			byte->setValidator(byteUnsignedValidator_);
		for (const auto &word : words_)
			word->setValidator(wordUnsignedValidator_);
		for (const auto &dword : dwords_)
			dword->setValidator(dwordUnsignedValidator_);
		for (const auto &qword : qwords_)
			qword->setValidator(qwordUnsignedValidator_);
		updateAllEntriesExcept(nullptr);
	}
}

Register DialogEditSimdRegister::value() const {
	Register out(reg_);
	out.setValueFrom(value_);
	return out;
}

}
