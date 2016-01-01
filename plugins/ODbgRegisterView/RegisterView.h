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

class QAbstractItemModel;

namespace ODbgRegisterView {

class RegisterGroup;
class FieldWidget;

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
    FieldWidget* selectedField() const;
    void updateFieldsPalette();
    void focusOutEvent(QFocusEvent*) override;
    void focusInEvent(QFocusEvent*) override;
    void keyPressEvent(QKeyEvent* event) override;
    QList<FieldWidget*> fields() const;

	QList<RegisterGroup*> groups;
private Q_SLOTS:
	void modelReset();
	void modelUpdated(QModelIndex const& topLeft,QModelIndex const& bottomRight);
};

class FieldWidget : public QLabel
{
    Q_OBJECT

    bool selected_=false;
    bool uneditable_;
    bool hovered_=false;
	bool fixedWidth=false;
	QPersistentModelIndex index;
    FieldWidget *up_=0, *down_=0, *left_=0, *right_=0;
	QColor unchangedFieldFGColor=Qt::green; // initialized to make bugs visible

	QString text() const;
	bool changed() const;
    QColor fgColorForChangedField() const;
public:
    void setNeighbors(FieldWidget* up,FieldWidget* down,FieldWidget* left,FieldWidget* right);
    FieldWidget* up() const { return up_; }
    FieldWidget* down() const { return down_; }
    FieldWidget* left() const { return left_; }
    FieldWidget* right() const { return right_; }

    FieldWidget(int fieldWidth,bool uneditable,QModelIndex const& index,QWidget* parent=nullptr);
    bool isEditable() const;
    bool isSelected() const;
	bool isInRange(QModelIndex const& topLeft, QModelIndex const& bottomRight) const;
    void updatePalette();
    void enterEvent(QEvent*) override;
    void leaveEvent(QEvent*) override;
    void select();
    void mousePressEvent(QMouseEvent* event) override;
    void unselect();
    void mouseDoubleClickEvent(QMouseEvent* event) override;
protected:
    void paintEvent(QPaintEvent* event) override;
Q_SIGNALS:
    void selected();
public Q_SLOTS:
	void update();
};

class RegisterGroup : public QWidget
{
    Q_OBJECT

	int lineAfterLastField() const;
public:
    RegisterGroup(QWidget* parent=nullptr);
    QList<FieldWidget*> fields() const;
	void setIndices(QList<QModelIndex> const& indices);
    void insert(int line, int column, int width, bool uneditable, QModelIndex const& index);
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
