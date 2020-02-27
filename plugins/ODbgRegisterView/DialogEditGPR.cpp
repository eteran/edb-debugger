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

#include "DialogEditGPR.h"
#include "EntryGridKeyUpDownEventFilter.h"
#include "GprEdit.h"
#include "util/Container.h"

#include <QDebug>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <cmath>
#include <cstring>
#include <tuple>
#include <type_traits>

namespace ODbgRegisterView {

DialogEditGPR::DialogEditGPR(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {

	setWindowTitle(tr("Modify Register"));
	setModal(true);
	const auto allContentsGrid = new QGridLayout();

	// Register name labels
	for (std::size_t c = 0; c < ENTRY_COLS; ++c) {
		auto &label = columnLabel(static_cast<Column>(FIRST_ENTRY_COL + c));
		label       = new QLabel(this);
		label->setAlignment(Qt::AlignCenter);
		allContentsGrid->addWidget(label, GPR_LABELS_ROW, FIRST_ENTRY_COL + c);
	}

	{
		static const auto formatNames = util::make_array(tr("Hexadecimal"), tr("Signed"), tr("Unsigned"), tr("Character"));
		// Format labels
		for (std::size_t f = 0; f < formatNames.size(); ++f) {
			auto &label = rowLabel(static_cast<Row>(FIRST_ENTRY_ROW + f));
			label       = new QLabel(formatNames[f], this);
			allContentsGrid->addWidget(label, FIRST_ENTRY_ROW + f, FORMAT_LABELS_COL);
		}
	}

	// All entries but char
	{
		static const auto offsetsInInteger = util::make_array<std::size_t>(0u, 0u, 0u, 1u, 0u);
		static const auto integerSizes     = util::make_array<std::size_t>(8u, 4u, 2u, 1u, 1u);
		static_assert(std::tuple_size<decltype(integerSizes)>::value == DialogEditGPR::ENTRY_COLS, "integerSizes length doesn't equal ENTRY_COLS");
		static_assert(std::tuple_size<decltype(offsetsInInteger)>::value == DialogEditGPR::ENTRY_COLS, "offsetsInInteger length doesn't equal ENTRY_COLS");
		static const auto formats = util::make_array(GprEdit::Format::Hex, GprEdit::Format::Signed, GprEdit::Format::Unsigned);
		for (std::size_t f = 0; f < formats.size(); ++f) {
			for (std::size_t c = 0; c < ENTRY_COLS; ++c) {
				auto &entry = this->entry(static_cast<Row>(FIRST_ENTRY_ROW + f), static_cast<Column>(FIRST_ENTRY_COL + c));
				entry       = new GprEdit(offsetsInInteger[c], integerSizes[c], formats[f], this);
				connect(entry, &GprEdit::textEdited, this, &DialogEditGPR::onTextEdited);
				entry->installEventFilter(this);
				allContentsGrid->addWidget(entry, FIRST_ENTRY_ROW + f, FIRST_ENTRY_COL + c);
			}
		}
	}

	// High byte char
	{
		auto &charHigh = entry(CHAR_ROW, GPR8H_COL);
		charHigh       = new GprEdit(1, 1, GprEdit::Format::Character, this);
		connect(charHigh, &GprEdit::textEdited, this, &DialogEditGPR::onTextEdited);
		charHigh->installEventFilter(this);
		allContentsGrid->addWidget(charHigh, CHAR_ROW, GPR8H_COL);
	}

	// Low byte char
	{
		auto &charLow = entry(CHAR_ROW, GPR8L_COL);
		charLow       = new GprEdit(0, 1, GprEdit::Format::Character, this);
		connect(charLow, &GprEdit::textEdited, this, &DialogEditGPR::onTextEdited);
		charLow->installEventFilter(this);
		allContentsGrid->addWidget(charLow, CHAR_ROW, GPR8L_COL);
	}

	resetLayout();

	const auto okCancel = new QDialogButtonBox(this);
	okCancel->setStandardButtons(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
	connect(okCancel, &QDialogButtonBox::accepted, this, &DialogEditGPR::accept);
	connect(okCancel, &QDialogButtonBox::rejected, this, &DialogEditGPR::reject);

	const auto dialogLayout = new QVBoxLayout(this);
	dialogLayout->addLayout(allContentsGrid);
	dialogLayout->addWidget(okCancel);

	for (std::size_t entry = 1; entry < entries_.size(); ++entry) {
		setTabOrder(entries_[entry - 1], entries_[entry]);
	}
}

GprEdit *&DialogEditGPR::entry(DialogEditGPR::Row row, DialogEditGPR::Column col) {

	if (row < ENTRY_ROWS)
		return entries_.at((row - FIRST_ENTRY_ROW) * ENTRY_COLS + (col - FIRST_ENTRY_COL));
	if (col == GPR8H_COL)
		return *(entries_.end() - 2);
	if (col == GPR8L_COL)
		return entries_.back();

	Q_ASSERT("Invalid row&col specified" && 0);
	return entries_.front(); // silence the compiler
}

void DialogEditGPR::updateAllEntriesExcept(GprEdit *notUpdated) {

	for (auto entry : entries_) {
		if (entry != notUpdated && !entry->isHidden()) {
			entry->setGPRValue(value_);
		}
	}
}

QLabel *&DialogEditGPR::columnLabel(DialogEditGPR::Column col) {
	return labels_.at(col - FIRST_ENTRY_COL);
}

QLabel *&DialogEditGPR::rowLabel(DialogEditGPR::Row row) {
	return labels_.at(ENTRY_COLS + row - FIRST_ENTRY_ROW);
}

void DialogEditGPR::hideColumn(DialogEditGPR::Column col) {
	Row fMax = col == GPR8L_COL || col == GPR8H_COL ? ENTRY_ROWS : FULL_LENGTH_ROWS;
	for (std::size_t f = 0; f < fMax; ++f) {
		entry(static_cast<Row>(FIRST_ENTRY_ROW + f), col)->hide();
	}
	columnLabel(col)->hide();
}

void DialogEditGPR::hideRow(Row row) {
	rowLabel(row)->hide();
	if (row == CHAR_ROW) {
		entry(row, GPR8L_COL)->hide();
		entry(row, GPR8H_COL)->hide();
	} else {
		for (std::size_t c = 0; c < FULL_LENGTH_ROWS; ++c) {
			entry(row, static_cast<Column>(FIRST_ENTRY_COL + c))->hide();
		}
	}
}

void DialogEditGPR::resetLayout() {
	for (auto entry : entries_) {
		entry->show();
	}

	for (auto label : labels_) {
		label->show();
	}

	static const auto colLabelStrings = util::make_array("R?X", "E?X", "?X", "?H", "?L");
	static_assert(std::tuple_size<decltype(colLabelStrings)>::value == ENTRY_COLS, "Number of labels not equal to number of entry columns");

	for (std::size_t c = 0; c < ENTRY_COLS; ++c) {
		columnLabel(static_cast<Column>(GPR64_COL + c))->setText(colLabelStrings[c]);
	}
}

void DialogEditGPR::setupEntriesAndLabels() {

	resetLayout();

	switch (bitSize_) {
	case 8:
		hideColumn(GPR8H_COL);
		hideColumn(GPR16_COL);
		/* fallthrough */
	case 16:
		hideColumn(GPR32_COL);
		/* fallthrough */
	case 32:
		hideColumn(GPR64_COL);
		/* fallthrough */
	case 64:
		break;
	default:
		Q_ASSERT("Unsupported bitSize" && 0);
	}

	const QString regName = reg_.name().toUpper();

	if (bitSize_ == 64)
		columnLabel(GPR64_COL)->setText(regName);
	else if (bitSize_ == 32)
		columnLabel(GPR32_COL)->setText(regName);
	else if (bitSize_ == 16)
		columnLabel(GPR16_COL)->setText(regName);
	else
		columnLabel(GPR8L_COL)->setText(regName);

	static const auto x86GPRsWithHighBytesAddressable    = util::make_array<QString>("EAX", "ECX", "EDX", "EBX", "RAX", "RCX", "RDX", "RBX");
	static const auto x86GPRsWithHighBytesNotAddressable = util::make_array<QString>("ESP", "EBP", "ESI", "EDI", "RSP", "RBP", "RSI", "RDI");
	static const auto upperGPRs64                        = util::make_array<QString>("R8", "R9", "R10", "R11", "R12", "R13", "R14", "R15");

	bool x86GPR     = false;
	bool upperGPR64 = false;
	using util::contains;

	if (contains(x86GPRsWithHighBytesNotAddressable, regName)) {
		x86GPR = true;
		hideColumn(GPR8H_COL);
		if (bitSize_ == 32) {
			hideColumn(GPR8L_COL); // In 32 bit mode low bytes also can't be addressed
			hideRow(CHAR_ROW);
		}
	} else if (contains(x86GPRsWithHighBytesAddressable, regName)) {
		x86GPR = true;
	} else if (contains(upperGPRs64, regName)) {
		upperGPR64 = true;
	}

	if (x86GPR) {
		if (bitSize_ == 64) {
			columnLabel(GPR32_COL)->setText("E" + regName.mid(1));
		}

		columnLabel(GPR16_COL)->setText(regName.mid(1));
		columnLabel(GPR8H_COL)->setText(regName.mid(1, 1) + "H");

		if (bitSize_ == 64 && !contains(x86GPRsWithHighBytesAddressable, regName)) {
			columnLabel(GPR8L_COL)->setText(regName.mid(1) + "L");
		} else {
			columnLabel(GPR8L_COL)->setText(regName.mid(1, 1) + "L");
		}
	} else if (upperGPR64) {
		columnLabel(GPR32_COL)->setText(regName + "D");
		columnLabel(GPR16_COL)->setText(regName + "W");
		columnLabel(GPR8L_COL)->setText(regName + "B");
		hideColumn(GPR8H_COL);
	} else {
		// These have hex only format
		hideColumn(GPR8H_COL);

		if (bitSize_ != 8) {
			hideColumn(GPR8L_COL);
		}

		if (bitSize_ != 16) {
			hideColumn(GPR16_COL);
		}

		if (bitSize_ != 32) {
			hideColumn(GPR32_COL);
		}

		hideRow(SIGNED_ROW);
		hideRow(UNSIGNED_ROW);
		hideRow(CHAR_ROW);
	}
}

void DialogEditGPR::setupFocus() {
	for (auto entry : entries_) {
		if (!entry->isHidden()) {
			entry->setFocus(Qt::OtherFocusReason);
			break;
		}
	}
}

bool DialogEditGPR::eventFilter(QObject *obj, QEvent *event) {
	return entry_grid_key_event_filter(this, obj, event);
}

void DialogEditGPR::setValue(const Register &newReg) {
	reg_     = newReg;
	value_   = reg_.valueAsInteger();
	bitSize_ = reg_.bitSize();
	setupEntriesAndLabels();
	setWindowTitle(tr("Modify %1").arg(reg_.name().toUpper()));
	updateAllEntriesExcept(nullptr);
	setupFocus();
}

Register DialogEditGPR::value() const {
	Register ret(reg_);
	ret.setScalarValue(value_);
	return ret;
}

void DialogEditGPR::onTextEdited(const QString &) {
	auto edit = dynamic_cast<GprEdit *>(sender());
	edit->updateGPRValue(value_);
	updateAllEntriesExcept(edit);
}

}
