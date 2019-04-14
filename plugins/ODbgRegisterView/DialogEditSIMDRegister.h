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

#ifndef DIALOG_EDIT_MMX_H_20151010
#define DIALOG_EDIT_MMX_H_20151010

#include "Register.h"
#include "RegisterViewModelBase.h"
#include "Util.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <array>
#include <cstddef>
#include <cstdint>

class QHBoxLayout;
class QLongValidator;
class QRadioButton;
class QRegExpValidator;
class QULongValidator;
class QValidator;

namespace ODbgRegisterView {

class NumberEdit;

class DialogEditSIMDRegister : public QDialog {
	Q_OBJECT

private:
	static constexpr int numBytes = 256 / 8;

	enum EntriesRows {
		BYTE_INDICES_ROW,
		BYTES_ROW,
		ENTRIES_FIRST_ROW = BYTES_ROW,
		WORDS_ROW,
		DWORDS_ROW,
		QWORDS_ROW,
		FLOATS32_ROW,
		FLOATS64_ROW,

		ROW_AFTER_ENTRIES
	};

	enum EntriesCols {
		LABELS_COL,
		ENTRIES_FIRST_COL,
		YMM_FIRST_COL = ENTRIES_FIRST_COL,
		XMM_FIRST_COL = YMM_FIRST_COL + 16,
		MMX_FIRST_COL = XMM_FIRST_COL + 8,
		TOTAL_COLS = MMX_FIRST_COL + 8
	};


public:
    explicit DialogEditSIMDRegister(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	void set_value(const Register &value);
	void set_current_element(RegisterViewModelBase::Model::ElementSize size, NumberDisplayMode format, int elementIndex);
	Register value() const;

protected:
	bool eventFilter(QObject*, QEvent*) override;
private:
	template <std::size_t numEntries>
	void setupEntries(const QString &label, std::array<NumberEdit *, numEntries> &entries, int row, const char *slot, int naturalWidthInChars);

	std::uint64_t readInteger(const NumberEdit *edit) const;

	template <typename Integer>
	void formatInteger(NumberEdit *edit, Integer integer) const;

	void updateAllEntriesExcept(NumberEdit *notUpdated);
	void hideColumns(EntriesCols preLast);
	void hideRows(EntriesRows rowToHide);
	void resetLayout();

private:
	template <typename Integer>
	void onIntegerEdited(QObject *sender, const std::array<NumberEdit *, numBytes / sizeof(Integer)> &elements);

	template <typename Float>
	void onFloatEdited(QObject *sender, const std::array<NumberEdit *, numBytes / sizeof(Float)> &elements);

	template <typename T>
	void updateIntegralEntries(const std::array<NumberEdit *, numBytes / sizeof(T)> &entries, NumberEdit *notUpdated);

	template <typename T>
	void updateFloatEntries(const std::array<NumberEdit *, numBytes / sizeof(T)> &entries, NumberEdit *notUpdated);

private Q_SLOTS	:
	void onByteEdited();
	void onWordEdited();
	void onDwordEdited();
	void onQwordEdited();
	void onFloat32Edited();
	void onFloat64Edited();
	void onHexToggled(bool checked);
	void onSignedToggled(bool checked);
	void onUnsignedToggled(bool checked);

private:
	QHBoxLayout *     hexSignOKCancelLayout;
	QDialogButtonBox *okCancel;
	QRadioButton *    radioHex;
	QRadioButton *    radioSigned;
	QRadioButton *    radioUnsigned;
	std::array<NumberEdit *, numBytes / 8> floats64;
	std::array<NumberEdit *, numBytes / 4> floats32;
	std::array<NumberEdit *, numBytes / 8> qwords;
	std::array<NumberEdit *, numBytes / 4> dwords;
	std::array<NumberEdit *, numBytes / 2> words;
	std::array<NumberEdit *, numBytes>     bytes;
	std::array<QLabel *, numBytes>         columnLabels;

	QRegExpValidator *byteHexValidator;
	QRegExpValidator *wordHexValidator;
	QRegExpValidator *dwordHexValidator;
	QRegExpValidator *qwordHexValidator;

	QLongValidator *byteSignedValidator;
	QLongValidator *wordSignedValidator;
	QLongValidator *dwordSignedValidator;
	QLongValidator *qwordSignedValidator;

	QULongValidator *byteUnsignedValidator;
	QULongValidator *wordUnsignedValidator;
	QULongValidator *dwordUnsignedValidator;
	QULongValidator *qwordUnsignedValidator;

	QValidator *float32Validator;
	QValidator *float64Validator;

	NumberDisplayMode intMode;
	std::array<std::uint8_t, numBytes> value_;
	Register reg;
};

}

#endif
