#ifndef DIALOG_EDIT_GPR_H_20151011
#define DIALOG_EDIT_GPR_H_20151011

#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QSpacerItem>
#include <QVBoxLayout>
#include <array>
#include <string>
#include <cstddef>
#include <cstdint>
#include "Register.h"

class GPREdit;

class DialogEditGPR : public QDialog
{
	Q_OBJECT

	enum Column
	{
		FORMAT_LABELS_COL,
		FIRST_ENTRY_COL,
		GPR64_COL=FIRST_ENTRY_COL,
		GPR32_COL,
		GPR16_COL,
		GPR8H_COL,
		GPR8L_COL,

		TOTAL_COLS,
		ENTRY_COLS=TOTAL_COLS-1,
		CHAR_COLS=2
	};
	enum Row
	{
		GPR_LABELS_ROW,
		FIRST_ENTRY_ROW,
		HEX_ROW=FIRST_ENTRY_ROW,
		SIGNED_ROW,
		UNSIGNED_ROW,
		LAST_FULL_LENGTH_ROW=UNSIGNED_ROW,
		CHAR_ROW,

		ROW_AFTER_ENTRIES,

		FULL_LENGTH_ROWS=LAST_FULL_LENGTH_ROW-FIRST_ENTRY_ROW+1,
		ENTRY_ROWS=ROW_AFTER_ENTRIES-FIRST_ENTRY_ROW
	};

	std::array<QLabel*,ENTRY_COLS+ENTRY_ROWS> labels={{0}};
    std::array<GPREdit*,FULL_LENGTH_ROWS*ENTRY_COLS+CHAR_COLS> entries={{0}};
    std::uint64_t value_;
	std::size_t bitSize_=0;
	Register reg;

	void updateAllEntriesExcept(GPREdit* notUpdated);
	void hideColumn(Column col);
	void hideRow(Row row);
	void setupEntriesAndLabels();
	void resetLayout();
	QLabel*& columnLabel(Column col);
	QLabel*& rowLabel(Row row);
	GPREdit*& entry(Row row, Column col);
	void setupFocus();
public:
	DialogEditGPR(QWidget* parent=nullptr);
	Register value() const;
	void set_value(const Register& reg);
private Q_SLOTS:
    void onTextEdited(const QString&);
};

#endif
