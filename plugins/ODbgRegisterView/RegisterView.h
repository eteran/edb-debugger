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

class QAbstractItemModel;

namespace ODbgRegisterView {

class RegisterGroup;
class FieldWidget;
class ValueField;

class ODBRegView : public QScrollArea
{
	Q_OBJECT

	QAbstractItemModel* model_=nullptr;
	enum class RegisterGroupType
	{
		GPR,
		rIP,
		EFL,
		ExpandedEFL,
		Segment,
		FPUData,
		FPUWords,
		FPULastOp,
		SSEData,
		AVXData,
		MXCSR
	};
	std::vector<RegisterGroupType> regGroupTypes;
	void addGroup(RegisterGroupType type);
public:
    ODBRegView(QWidget* parent=nullptr);
    void finalize();
	void setModel(QAbstractItemModel* model);
private:
	ValueField* selectedField() const;
    void updateFieldsPalette();
    void focusOutEvent(QFocusEvent*) override;
    void focusInEvent(QFocusEvent*) override;
    void keyPressEvent(QKeyEvent* event) override;
    QList<ValueField*> valueFields() const;
    QList<FieldWidget*> fields() const;

	QList<RegisterGroup*> groups;
private Q_SLOTS:
	void modelReset();
	void modelUpdated();
};

class FieldWidget : public QLabel
{
    Q_OBJECT

	void init(int fieldWidth);
protected:
	QPersistentModelIndex index;
	virtual QString text() const;
public:
    FieldWidget(int fieldWidth,QModelIndex const& index,QWidget* parent=nullptr);
    FieldWidget(int fieldWidth,QString const& fixedText,QWidget* parent=nullptr);
public Q_SLOTS:
	virtual void update();
};

class ValueField : public FieldWidget
{
	Q_OBJECT

    bool selected_=false;
    bool hovered_=false;
    ValueField *up_=0, *down_=0, *left_=0, *right_=0;
	std::function<QString(QString)> valueFormatter;

	void init();
	bool changed() const;
    QColor fgColorForChangedField() const;
protected:
	QString text() const override;
    void enterEvent(QEvent*) override;
    void leaveEvent(QEvent*) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
public:
    ValueField(int fieldWidth,
			   QModelIndex const& index,
			   QWidget* parent=nullptr,
			   std::function<QString(QString const&)> const& valueFormatter=[](QString const&s){return s;}
			   );
    void setNeighbors(ValueField* up,ValueField* down,ValueField* left,ValueField* right);
    ValueField* up() const { return up_; }
    ValueField* down() const { return down_; }
    ValueField* left() const { return left_; }
    ValueField* right() const { return right_; }

    bool isSelected() const;
public Q_SLOTS:
	void update() override;
    void select();
    void unselect();
    void updatePalette();
Q_SIGNALS:
    void selected();
};

class RegisterGroup : public QWidget
{
    Q_OBJECT

	int lineAfterLastField() const;
public:
    RegisterGroup(QWidget* parent=nullptr);
    QList<FieldWidget*> fields() const;
    QList<ValueField*> valueFields() const;
	void setIndices(QList<QModelIndex> const& indices);
    void insert(int line, int column, FieldWidget* widget);
	void appendNameValueComment(QModelIndex const& nameIndex,bool insertComment=true);
protected:
    void mousePressEvent(QMouseEvent* event);
private Q_SLOTS:
    void fieldSelected();
public Q_SLOTS:
	void adjustWidth();
};

}

#endif
