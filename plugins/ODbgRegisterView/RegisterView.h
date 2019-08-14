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

#ifndef ODBG_REGISTER_VIEW_H_20151230
#define ODBG_REGISTER_VIEW_H_20151230

#include "RegisterViewModelBase.h"
#include "ValueField.h"
#include <QLabel>
#include <QPersistentModelIndex>
#include <QScrollArea>
#include <QString>
#include <functional>

namespace ODbgRegisterView {

class DialogEditSIMDRegister;
class DialogEditGPR;
class DialogEditFPU;
class RegisterGroup;

class ODBRegView : public QScrollArea {
	Q_OBJECT

public:
	struct RegisterGroupType {
		enum T : int {
#if defined EDB_X86 || defined EDB_X86_64
			GPR,
			rIP,
			ExpandedEFL,
			Segment,
			EFL,
			FPUData,
			FPUWords,
			FPULastOp,
			Debug,
			MMX,
			SSEData,
			AVXData,
			MXCSR,
#elif defined EDB_ARM32
			GPR,
			CPSR,
			ExpandedCPSR,
			FPSCR,
#else
#	error "Not implemented"
#endif

			NUM_GROUPS
		} value;

		RegisterGroupType(T v) : value(v) {
		}

		explicit RegisterGroupType(int v) : value(static_cast<T>(v)) {
		}

		operator T() const {
			return value;
		}
	};

private:
	std::vector<RegisterGroupType> visibleGroupTypes;
	QList<QAction *>               menuItems;
	DialogEditGPR *                dialogEditGPR;
	DialogEditSIMDRegister *       dialogEditSIMDReg;
	DialogEditFPU *                dialogEditFPU;

	RegisterGroup *makeGroup(RegisterGroupType type);

public:
	explicit ODBRegView(const QString &settings, QWidget *parent = nullptr);
	void setModel(RegisterViewModelBase::Model *model);
	QList<ValueField *>  valueFields() const;
	QList<FieldWidget *> fields() const;
	void showMenu(const QPoint &position, const QList<QAction *> &additionalItems = {}) const;
	void saveState(const QString &settings) const;
	void groupHidden(RegisterGroup *group);
	DialogEditGPR *         gprEditDialog() const;
	DialogEditSIMDRegister *simdEditDialog() const;
	DialogEditFPU *         fpuEditDialog() const;
	void                    selectAField();

private:
	ValueField *selectedField() const;
	void        updateFieldsPalette();
	void keyPressEvent(QKeyEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void updateFont();

private:
	QList<RegisterGroup *> groups;

public Q_SLOTS:
	void fieldSelected();

private Q_SLOTS:
	void modelReset();
	void modelUpdated();
	void copyAllRegisters();
	void copyRegisterToClipboard() const;
	void settingsUpdated();

private:
	RegisterViewModelBase::Model *model_ = nullptr;
};

class Canvas : public QWidget {
	Q_OBJECT
public:
    explicit Canvas(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());

protected:
	void mousePressEvent(QMouseEvent *event) override;
};

class VolatileNameField : public FieldWidget {
	Q_OBJECT
	
private:
	std::function<QString()> valueFormatter;

public:
	VolatileNameField(int fieldWidth, const std::function<QString()> &valueFormatter, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	QString text() const override;
};

#if defined EDB_X86 || defined EDB_X86_64
class FPUValueField final : public ValueField {
	Q_OBJECT

private:
	int showAsRawActionIndex;
	int showAsFloatActionIndex;

	FieldWidget *commentWidget;
	int          row;
	int          column;

	QPersistentModelIndex tagValueIndex;

	bool groupDigits = false;

public:
	// Will add itself and commentWidget to the group and renew their positions as needed
	FPUValueField(int fieldWidth, const QModelIndex &regValueIndex, const QModelIndex &tagValueIndex, RegisterGroup *group, FieldWidget *commentWidget, int row, int column);

public Q_SLOTS:
	void showFPUAsRaw();
	void showFPUAsFloat();
	void displayFormatChanged();
	void updatePalette() override;
};
#endif

struct BitFieldDescription {
	int                  textWidth;
	std::vector<QString> valueNames;
	std::vector<QString> setValueTexts;
	std::function<bool(unsigned, unsigned)> const valueEqualComparator;
	BitFieldDescription(int textWidth, const std::vector<QString> &valueNames, const std::vector<QString> &setValueTexts, const std::function<bool(unsigned, unsigned)> &valueEqualComparator = [](unsigned a, unsigned b) { return a == b; });
};

class BitFieldFormatter {
public:
	explicit BitFieldFormatter(const BitFieldDescription &bfd);
	QString operator()(const QString &text);

private:
	std::vector<QString> valueNames;
};

class MultiBitFieldWidget final : public ValueField {
	Q_OBJECT

public:
	MultiBitFieldWidget(const QModelIndex &index, const BitFieldDescription &bfd, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());

public Q_SLOTS:
	void setValue(int value);
	void adjustToData() override;

private:
	QList<QAction *> valueActions;
	std::function<bool(unsigned, unsigned)> equal;
};

class SIMDValueManager : public QObject {
	Q_OBJECT
private:
	QPersistentModelIndex regIndex;
	int                   lineInGroup;
	QList<ValueField *>   elements;
	QList<QAction *>      menuItems;
	NumberDisplayMode     intMode;
	enum MenuItemNumbers {
		VIEW_AS_BYTES,
		VIEW_AS_WORDS,
		VIEW_AS_DWORDS,
		VIEW_AS_QWORDS,

		VIEW_AS_FLOAT32,
		VIEW_AS_FLOAT64,

		VIEW_INT_AS_HEX,
		VIEW_INT_AS_SIGNED,
		VIEW_INT_AS_UNSIGNED,

		MENU_ITEMS_COUNT
	};

	using Model = RegisterViewModelBase::Model;
	Model *            model() const;
	RegisterGroup *    group() const;
	Model::ElementSize currentSize() const;
	NumberDisplayMode  currentFormat() const;
	void               setupMenu();
	void               updateMenu();
	void               fillGroupMenu();

public:
	SIMDValueManager(int lineInGroup, const QModelIndex &nameIndex, RegisterGroup *parent = nullptr);

public Q_SLOTS:
	void displayFormatChanged();

private Q_SLOTS:
	void showAsInt(int size);
	void showAsFloat(int size);
	void setIntFormat(int format);
};

class RegisterGroup : public QWidget {
	Q_OBJECT
	friend SIMDValueManager;

private:
	QList<QAction *> menuItems;
	QString          name;

	int         lineAfterLastField() const;
	ODBRegView *regView() const;

public:
	explicit RegisterGroup(const QString &name, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	QList<FieldWidget *> fields() const;
	QList<ValueField *>  valueFields() const;
	void setIndices(const QList<QModelIndex> &indices);
	void insert(int line, int column, FieldWidget *widget);
	// Insert, but without moving to its place
	void insert(FieldWidget *widget);
	void setupPositionAndSize(int line, int column, FieldWidget *widget);
	void appendNameValueComment(const QModelIndex &nameIndex, const QString &tooltip = "", bool insertComment = true);
	void showMenu(const QPoint &position, const QList<QAction *> &additionalItems = {}) const;
	QMargins getFieldMargins() const;

protected:
	void mousePressEvent(QMouseEvent *event) override;

public Q_SLOTS:
	void adjustWidth();
	void hideAndReport();
};

}

#endif
