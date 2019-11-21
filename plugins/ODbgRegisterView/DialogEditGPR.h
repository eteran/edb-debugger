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

#ifndef DIALOG_EDIT_GPR_H_20151011_
#define DIALOG_EDIT_GPR_H_20151011_

#include "Register.h"
#include <QDialog>
#include <array>
#include <cstddef>
#include <cstdint>

class QLabel;

namespace ODbgRegisterView {

class GprEdit;

class DialogEditGPR : public QDialog {
	Q_OBJECT

public:
	explicit DialogEditGPR(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());

public:
	Register value() const;
	void setValue(const Register &reg);

private Q_SLOTS:
	void onTextEdited(const QString &);

private:
	enum Column {
		FORMAT_LABELS_COL,
		FIRST_ENTRY_COL,
		GPR64_COL = FIRST_ENTRY_COL,
		GPR32_COL,
		GPR16_COL,
		GPR8H_COL,
		GPR8L_COL,

		TOTAL_COLS,
		ENTRY_COLS = TOTAL_COLS - 1,
		CHAR_COLS  = 2
	};

	enum Row {
		GPR_LABELS_ROW,
		FIRST_ENTRY_ROW,
		HEX_ROW = FIRST_ENTRY_ROW,
		SIGNED_ROW,
		UNSIGNED_ROW,
		LAST_FULL_LENGTH_ROW = UNSIGNED_ROW,
		CHAR_ROW,

		ROW_AFTER_ENTRIES,

		FULL_LENGTH_ROWS = LAST_FULL_LENGTH_ROW - FIRST_ENTRY_ROW + 1,
		ENTRY_ROWS       = ROW_AFTER_ENTRIES - FIRST_ENTRY_ROW
	};

protected:
	bool eventFilter(QObject *, QEvent *) override;

private:
	void updateAllEntriesExcept(GprEdit *notUpdated);
	void hideColumn(Column col);
	void hideRow(Row row);
	void setupEntriesAndLabels();
	void resetLayout();
	QLabel *&columnLabel(Column col);
	QLabel *&rowLabel(Row row);
	GprEdit *&entry(Row row, Column col);
	void setupFocus();

private:
	std::array<QLabel *, ENTRY_COLS + ENTRY_ROWS> labels_                    = {{nullptr}};
	std::array<GprEdit *, FULL_LENGTH_ROWS *ENTRY_COLS + CHAR_COLS> entries_ = {{nullptr}};
	std::uint64_t value_;
	std::size_t bitSize_ = 0;
	Register reg_;
};

}

#endif
