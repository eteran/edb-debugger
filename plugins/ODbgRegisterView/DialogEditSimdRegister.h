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

#ifndef DIALOG_EDIT_SIMD_REGISTER_H_20151010_
#define DIALOG_EDIT_SIMD_REGISTER_H_20151010_

#include "Register.h"
#include "RegisterViewModelBase.h"
#include "Util.h"
#include <QDialog>
#include <array>
#include <cstddef>
#include <cstdint>

class QLabel;
class QDialogButtonBox;
class QHBoxLayout;
class QLongValidator;
class QRadioButton;
class QRegExpValidator;
class QULongValidator;
class QValidator;

namespace ODbgRegisterView {

class NumberEdit;

class DialogEditSimdRegister : public QDialog {
	Q_OBJECT

private:
	static constexpr int NumBytes = 256 / 8;

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
		TOTAL_COLS    = MMX_FIRST_COL + 8
	};

public:
	explicit DialogEditSimdRegister(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	void setValue(const Register &value);
	void set_current_element(RegisterViewModelBase::Model::ElementSize size, NumberDisplayMode format, int elementIndex);
	Register value() const;

protected:
	bool eventFilter(QObject *, QEvent *) override;

private:
	template <std::size_t NumEntries, class Func>
	void setupEntries(const QString &label, std::array<NumberEdit *, NumEntries> &entries, int row, Func slot, int naturalWidthInChars);

	std::uint64_t readInteger(const NumberEdit *edit) const;

	template <typename Integer>
	void formatInteger(NumberEdit *edit, Integer integer) const;

	void updateAllEntriesExcept(NumberEdit *notUpdated);
	void hideColumns(EntriesCols preLast);
	void hideRows(EntriesRows rowToHide);
	void resetLayout();

private:
	template <typename Integer>
	void onIntegerEdited(QObject *sender, const std::array<NumberEdit *, NumBytes / sizeof(Integer)> &elements);

	template <typename Float>
	void onFloatEdited(QObject *sender, const std::array<NumberEdit *, NumBytes / sizeof(Float)> &elements);

	template <typename T>
	void updateIntegralEntries(const std::array<NumberEdit *, NumBytes / sizeof(T)> &entries, NumberEdit *notUpdated);

	template <typename T>
	void updateFloatEntries(const std::array<NumberEdit *, NumBytes / sizeof(T)> &entries, NumberEdit *notUpdated);

private Q_SLOTS:
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
	QHBoxLayout *hexSignOKCancelLayout_;
	QDialogButtonBox *okCancel_;
	QRadioButton *radioHex_;
	QRadioButton *radioSigned_;
	QRadioButton *radioUnsigned_;
	std::array<NumberEdit *, NumBytes / 8> floats64_;
	std::array<NumberEdit *, NumBytes / 4> floats32_;
	std::array<NumberEdit *, NumBytes / 8> qwords_;
	std::array<NumberEdit *, NumBytes / 4> dwords_;
	std::array<NumberEdit *, NumBytes / 2> words_;
	std::array<NumberEdit *, NumBytes> bytes_;
	std::array<QLabel *, NumBytes> columnLabels_;

	QRegExpValidator *byteHexValidator_;
	QRegExpValidator *wordHexValidator_;
	QRegExpValidator *dwordHexValidator_;
	QRegExpValidator *qwordHexValidator_;

	QLongValidator *byteSignedValidator_;
	QLongValidator *wordSignedValidator_;
	QLongValidator *dwordSignedValidator_;
	QLongValidator *qwordSignedValidator_;

	QULongValidator *byteUnsignedValidator_;
	QULongValidator *wordUnsignedValidator_;
	QULongValidator *dwordUnsignedValidator_;
	QULongValidator *qwordUnsignedValidator_;

	QValidator *float32Validator_;
	QValidator *float64Validator_;

	NumberDisplayMode intMode_;
	std::array<std::uint8_t, NumBytes> value_;
	Register reg_;
};

}

#endif
