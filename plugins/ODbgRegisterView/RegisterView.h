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

#include <QScrollArea>
#include <QString>
#include <QLabel>
#include <array>
#include <deque>
#include <QPersistentModelIndex>
#include <functional>
#include "RegisterViewModelBase.h"

class QSettings;
class DialogEditGPR;
class DialogEditSIMDRegister;
class DialogEditFPU;

namespace ODbgRegisterView {

class RegisterGroup;
class FieldWidget;
class ValueField;

class ODBRegView : public QScrollArea
{
	Q_OBJECT

	RegisterViewModelBase::Model* model_=nullptr;
public:
	struct RegisterGroupType
	{
		enum T
		{
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

			NUM_GROUPS
		} value;
		RegisterGroupType(T v):value(v){}
		explicit RegisterGroupType(int v):value(static_cast<T>(v)){}
		operator T() const {return value;}
	};
private:
	std::vector<RegisterGroupType> visibleGroupTypes;
	QList<QAction*> menuItems;
	DialogEditGPR* dialogEditGPR;
	DialogEditSIMDRegister* dialogEditSIMDReg;
	DialogEditFPU* dialogEditFPU;

	RegisterGroup* makeGroup(RegisterGroupType type);
public:
	ODBRegView(QString const& settings, QWidget* parent=nullptr);
	void setModel(RegisterViewModelBase::Model* model);
	QList<ValueField*> valueFields() const;
	QList<FieldWidget*> fields() const;
	void showMenu(QPoint const& position,QList<QAction*>const& additionalItems={}) const;
	void saveState(QString const& settings) const;
	void groupHidden(RegisterGroup* group);
	DialogEditGPR* gprEditDialog() const;
	DialogEditSIMDRegister* simdEditDialog() const;
	DialogEditFPU* fpuEditDialog() const;
	void selectAField();
private:
	ValueField* selectedField() const;
	void updateFieldsPalette();
	void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

	QList<RegisterGroup*> groups;
private Q_SLOTS:
	void fieldSelected();
	void modelReset();
	void modelUpdated();
	void copyAllRegisters();
	void copyRegisterToClipboard() const;
};

class Canvas : public QWidget
{
public:
	Canvas(QWidget* parent=nullptr);
protected:
    void mousePressEvent(QMouseEvent* event) override;
};

class FieldWidget : public QLabel
{
	Q_OBJECT

	void init(int fieldWidth);
protected:
	QPersistentModelIndex index;
	int fieldWidth_;

	ODBRegView* regView() const;
	RegisterGroup* group() const;
public:
	FieldWidget(int fieldWidth,QModelIndex const& index,QWidget* parent=nullptr);
	FieldWidget(int fieldWidth,QString const& fixedText,QWidget* parent=nullptr);
	FieldWidget(QString const& fixedText,QWidget* parent=nullptr);
	virtual QString text() const;
	int lineNumber() const;
	int columnNumber() const;
	int fieldWidth() const;
public Q_SLOTS:
	virtual void adjustToData();
};

class VolatileNameField : public FieldWidget
{
	std::function<QString()> valueFormatter;
public:
	VolatileNameField(int fieldWidth, std::function<QString()> const& valueFormatter,QWidget* parent=nullptr);
	QString text() const override;
};

class ValueField : public FieldWidget
{
	Q_OBJECT

	bool selected_=false;
	bool hovered_=false;
	std::function<QString(QString)> valueFormatter;

	// For GPR
	QAction* setToZeroAction=nullptr;
	QAction* setToOneAction=nullptr;
protected:
	QList<QAction*> menuItems;

private:
	void init();
	QColor fgColorForChangedField() const;
	void editNormalReg(QModelIndex const& indexToEdit, QModelIndex const& clickedIndex) const;
protected:
	RegisterViewModelBase::Model* model() const;
	bool changed() const;

	void enterEvent(QEvent*) override;
	void leaveEvent(QEvent*) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseDoubleClickEvent(QMouseEvent* event) override;
	void paintEvent(QPaintEvent* event) override;

	ValueField* bestNeighbor(std::function<bool(QPoint const& neighborPos,
												ValueField const*curResult,
												QPoint const& selfPos)>const& firstIsBetter) const;
public:
	ValueField(int fieldWidth,
			   QModelIndex const& index,
			   QWidget* parent=nullptr,
			   std::function<QString(QString const&)> const& valueFormatter=[](QString const&s){return s;}
			   );
	ValueField* up() const;
	ValueField* down() const;
	ValueField* left() const;
	ValueField* right() const;

	bool isSelected() const;
	void showMenu(QPoint const& position);
	QString text() const override;
	QModelIndex regIndex() const;
public Q_SLOTS:
	void defaultAction();
	void pushFPUStack();
	void popFPUStack();
	void adjustToData() override;
	void select();
	void unselect();
	virtual void updatePalette();
	void copyToClipboard() const;
	void setZero();
	void setToOne();
	void increment();
	void decrement();
	void invert();
Q_SIGNALS:
	void selected();
};

class FPUValueField : public ValueField
{
	Q_OBJECT
	int showAsRawActionIndex;
	int showAsFloatActionIndex;

	FieldWidget* commentWidget;
	int row;
	int column;

	QPersistentModelIndex tagValueIndex;

	bool groupDigits=false;
public:
	// Will add itself and commentWidget to the group and renew their positions as needed
	FPUValueField(int fieldWidth,
				  QModelIndex const& regValueIndex,
				  QModelIndex const& tagValueIndex,
				  RegisterGroup* group,
				  FieldWidget* commentWidget,
				  int row,
				  int column
				  );
public Q_SLOTS:
	void showFPUAsRaw();
	void showFPUAsFloat();
	void displayFormatChanged();
	void updatePalette() override;
};

struct BitFieldDescription
{
	int textWidth;
	std::vector<QString> valueNames;
	std::vector<QString> setValueTexts;
	std::function<bool(unsigned,unsigned)>const valueEqualComparator;
	BitFieldDescription(int textWidth,
						std::vector<QString>const& valueNames,
						std::vector<QString>const& setValueTexts,
						std::function<bool(unsigned,unsigned)>const& valueEqualComparator=[](unsigned a,unsigned b){return a==b;});
};

class BitFieldFormatter
{
	std::vector<QString> valueNames;
public:
	BitFieldFormatter(BitFieldDescription const& bfd);
	QString operator()(QString const& text);
};

class MultiBitFieldWidget : public ValueField
{
	Q_OBJECT

	QList<QAction*> valueActions;
	std::function<bool(unsigned,unsigned)> equal;
public:
	MultiBitFieldWidget( QModelIndex const& index,
					BitFieldDescription const& bfd,
					QWidget* parent=nullptr);
public Q_SLOTS:
	void setValue(int value);
	void adjustToData() override;
};

class SIMDValueManager : public QObject
{
	Q_OBJECT
	QPersistentModelIndex regIndex;
	int lineInGroup;
	QList<ValueField*> elements;
	QList<QAction*> menuItems;
	NumberDisplayMode intMode;
	enum MenuItemNumbers
	{
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

	using Model=RegisterViewModelBase::Model;
	Model* model() const;
	RegisterGroup* group() const;
	Model::ElementSize currentSize() const;
	NumberDisplayMode currentFormat() const;
	void setupMenu();
	void updateMenu();
	void fillGroupMenu();
public:
	SIMDValueManager(int lineInGroup,
					 QModelIndex const& nameIndex,
					 RegisterGroup* parent=nullptr);
public Q_SLOTS:
	void displayFormatChanged();
private Q_SLOTS:
	void showAsInt(int size);
	void showAsFloat(int size);
	void setIntFormat(int format);
};

class RegisterGroup : public QWidget
{
	Q_OBJECT

	QList<QAction*> menuItems;
	QString name;

	int lineAfterLastField() const;
	ODBRegView* regView() const;

public:
	RegisterGroup(QString const& name, QWidget* parent=nullptr);
	QList<FieldWidget*> fields() const;
	QList<ValueField*> valueFields() const;
	void setIndices(QList<QModelIndex> const& indices);
	void insert(int line, int column, FieldWidget* widget);
	// Insert, but without moving to its place
	void insert(FieldWidget* widget);
	void setupPositionAndSize(int line, int column, FieldWidget* widget);
	void appendNameValueComment(QModelIndex const& nameIndex,QString const& tooltip="",bool insertComment=true);
	void showMenu(QPoint const& position,QList<QAction*>const& additionalItems={}) const;
	QMargins getFieldMargins() const;
protected:
	void mousePressEvent(QMouseEvent* event) override;
public Q_SLOTS:
	void adjustWidth();
	void hideAndReport();

	friend SIMDValueManager;
};

}

#endif
