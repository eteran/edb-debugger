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

#include "RegisterView.h"
#include <QMouseEvent>
#include <QLabel>
#include <QApplication>
#include <QMessageBox>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QVBoxLayout>
#include <algorithm>
#include <QDebug>
#include <iostream>
#include "RegisterViewModelBase.h"

namespace ODbgRegisterView {

// TODO: Right click => select too
// TODO: Menu key => open menu
// TODO: Enter key => modify/toggle
// TODO: Tooltips support
// TODO: GPR menu: Increment, Decrement, Invert, Zero(if not already), Set to 1(if not already)
// TODO: rFLAGS menu: Set Condition (O,P,NAE etc. - see ODB)
// TODO: FPU tags: toggle - set valid/empty
// TODO: FSR: Set Condition: L,E,Unordered
// TODO: PC: set 24/53/64-bit mantissa
// TODO: RC: round up/down/nearest

namespace
{

static const int MODEL_NAME_COLUMN=RegisterViewModelBase::Model::NAME_COLUMN;
static const int MODEL_VALUE_COLUMN=RegisterViewModelBase::Model::VALUE_COLUMN;
static const int MODEL_COMMENT_COLUMN=RegisterViewModelBase::Model::COMMENT_COLUMN;

template<typename T>
T sqr(T v) { return v*v; }

// Square of Euclidean distance between two widgets
int distSqr(QWidget const* w1, QWidget const* w2)
{
    return sqr(w1->x()-w2->x())+sqr(w1->y()-w2->y());
}

QSize letterSize(QFont const& font)
{
    const QFontMetrics fontMetrics(font);
    const int width=fontMetrics.width('w');
    const int height=fontMetrics.height();
    return QSize(width,height);
}

}

// --------------------- FieldWidget impl ----------------------------------
QColor FieldWidget::fgColorForChangedField() const
{
	return Qt::red; // TODO: read from user palette
}

void FieldWidget::setNeighbors(FieldWidget* up,FieldWidget* down,FieldWidget* left,FieldWidget* right)
{
	up_=up;
	down_=down;
	left_=left;
	right_=right;
}

QString FieldWidget::text() const
{
	Q_ASSERT(index.isValid());
	const auto text=index.data();
	Q_ASSERT(text.isValid());
	return text.toString();
}

bool FieldWidget::changed() const
{
	Q_ASSERT(index.isValid());
	const auto changed=index.data(RegisterViewModelBase::Model::RegisterChangedRole);
	Q_ASSERT(changed.isValid());
	return changed.toBool();
}

FieldWidget::FieldWidget(const int fieldWidth, const bool uneditable, QModelIndex const& index, QWidget* const parent)
	: QLabel("FiWi???",parent),
	  uneditable_(uneditable),
	  index(index)
{
	setFont(parent->font());

	const auto size=letterSize(font());
	const int height=size.height();
	setFixedHeight(height);
	if(fieldWidth>0)
	{
		fixedWidth=true;
		const int width=fieldWidth*size.width();
		setFixedWidth(width);
	}

	auto palette=this->palette();

	if(uneditable_)
	{
		const QColor uneditableFGColor=palette.color(QPalette::Disabled,QPalette::Text);
		palette.setColor(foregroundRole(),uneditableFGColor);
	}
	else
	{
		setMouseTracking(true);
		unchangedFieldFGColor=palette.color(foregroundRole());
	}

	setPalette(palette);
}

bool FieldWidget::isEditable() const
{
	return !uneditable_;
}

void FieldWidget::update()
{
	QLabel::setText(text());
	adjustSize();
	updatePalette();
}

bool FieldWidget::isInRange(QModelIndex const& topLeft, QModelIndex const& bottomRight) const
{
	return topLeft.row() <= index.row() && index.row() <= bottomRight.row() &&
		   topLeft.column() <= index.column() && index.column() <= bottomRight.column();
}

bool FieldWidget::isSelected() const
{
	return selected_;
}

void FieldWidget::updatePalette()
{
	if(uneditable_) return;

	auto palette=this->palette();
	const auto appPalette=static_cast<QWidget*>(parent())->palette();

	const QColor selectedFGColor=appPalette.color(QPalette::HighlightedText);
	const QColor normalFGColor=appPalette.color(QPalette::WindowText);
	const QColor hoveredFGColor=normalFGColor;
	const QColor changedFGColor=fgColorForChangedField();

	palette.setColor(foregroundRole(),changed()  ? changedFGColor  : 
									  selected_ ? selectedFGColor :
									  hovered_  ? hoveredFGColor  : normalFGColor);

	setPalette(palette);
	QLabel::update();
}

void FieldWidget::enterEvent(QEvent*)
{
	if(uneditable_) return;
	hovered_=true;
	updatePalette();
}

void FieldWidget::leaveEvent(QEvent*)
{
	if(uneditable_) return;
	hovered_=false;
	updatePalette();
}

void FieldWidget::select()
{
	selected_=true;
	Q_EMIT selected();
	updatePalette();
}

void FieldWidget::mousePressEvent(QMouseEvent* event)
{
	if(uneditable_) return;
	if(event->button()==Qt::LeftButton) select();
}

void FieldWidget::unselect()
{
	selected_=false;
	updatePalette();
}

void FieldWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
	if(uneditable_) return;
	mousePressEvent(event);
	QMessageBox::information(this,"Double-clicked",QString("Double-clicked field %1 with contents \"%2\"")
								.arg(QString().sprintf("%p",static_cast<void*>(this))).arg(text()));
}

void FieldWidget::paintEvent(QPaintEvent* event)
{
	if(uneditable_)
	{
		QLabel::paintEvent(event);
		return;
	}

	QPainter painter(this);
	QStyleOptionViewItemV4 option;
	option.state |= QStyle::State_Enabled;
	if(selected_) option.state |= QStyle::State_Selected;
	if(hovered_)  option.state |= QStyle::State_MouseOver;
	if(static_cast<QWidget*>(parent())->palette().currentColorGroup()==QPalette::Active)
		option.state |= QStyle::State_Active;
	option.rect=rect();
	option.showDecorationSelected=true;
	style()->drawControl(QStyle::CE_ItemViewItem, &option, &painter);

	QLabel::paintEvent(event); // FIXME: Is it OK? Or better draw text manually, and call this one at the beginning?
}

// -------------------------------- RegisterGroup impl ----------------------------
RegisterGroup::RegisterGroup(QWidget* parent)
	: QWidget(parent)
{
	QFont font("Monospace");
	font.setStyleHint(QFont::TypeWriter);
	setFont(font);

	setBackgroundRole(QPalette::Base);
	setAutoFillBackground(true);
}

void RegisterGroup::insert(const int line, const int column, const int width, const bool uneditable, QModelIndex const& index)
{
	const auto widget=new FieldWidget(width,uneditable,index,this);
	widget->update();
	connect(widget,SIGNAL(selected()),this,SLOT(fieldSelected()));

	const auto charSize=letterSize(font());
	const auto charWidth=charSize.width();
	const auto charHeight=charSize.height();
	const auto margins(parentWidget()->parentWidget()->contentsMargins());
	// extra space for highlighting rectangle, so that single-digit fields are easier to target
	const auto marginLeft=charWidth/2;
	const auto marginRight=charWidth-marginLeft;

	QPoint position(charWidth*column,charHeight*line);
	position += QPoint(margins.left(),margins.top());
	position -= QPoint(marginLeft,0);

	QSize size(widget->size());
	size += QSize(marginLeft+marginRight,0);

	widget->setMinimumSize(size);
	widget->move(position);
	// FIXME: why are e.g. regnames like FSR truncated without the -1?
	widget->setContentsMargins(marginLeft,0,marginRight-1,0);

	const auto potentialNewWidth=widget->pos().x()+widget->width()+margins.right();
	const auto potentialNewHeight=widget->pos().y()+widget->height()+margins.bottom();
	const auto oldMinSize=minimumSize();
	if(potentialNewWidth > oldMinSize.width() || potentialNewHeight > oldMinSize.height())
		setMinimumSize(std::max(potentialNewWidth,oldMinSize.width()),std::max(potentialNewHeight,oldMinSize.height()));

	widget->show();
}

int RegisterGroup::lineAfterLastField() const
{
	const auto fields=this->fields();
	const auto bottomField=std::max_element(fields.begin(),fields.end(),
												[](FieldWidget* l,FieldWidget* r)
												{ return l->pos().y()<r->pos().y(); });
	return bottomField==fields.end() ? 0 : (*bottomField)->pos().y()/(*bottomField)->height()+1;
}

void RegisterGroup::appendNameValueComment(QModelIndex const& nameIndex, bool insertComment)
{
	Q_ASSERT(nameIndex.isValid());
	using namespace RegisterViewModelBase;
	const auto nameWidth=nameIndex.data(Model::FixedLengthRole).toInt();
	Q_ASSERT(nameWidth>0);
	const auto valueWidth=nameIndex.sibling(nameIndex.row(),Model::VALUE_COLUMN).data(Model::FixedLengthRole).toInt();
	Q_ASSERT(valueWidth>0);

	
	const int line=lineAfterLastField();
	int column=0;
	insert(line,column,nameWidth,true,nameIndex);
	column+=nameWidth+1;
	insert(line,column,valueWidth,false,nameIndex.sibling(nameIndex.row(),Model::VALUE_COLUMN));
	if(insertComment)
	{
		column+=valueWidth+1;
		insert(line,column,0,true,nameIndex.sibling(nameIndex.row(),Model::COMMENT_COLUMN));
	}
}

QList<FieldWidget*> RegisterGroup::fields() const
{
	const auto children=this->children();
	QList<FieldWidget*> fields;
	for(auto* const child : children)
	{
		const auto field=dynamic_cast<FieldWidget*>(child);
		if(field) fields.append(field);
	}
	return fields;
}

void RegisterGroup::mousePressEvent(QMouseEvent* event)
{
	if(event->type()!=QEvent::MouseButtonPress) return;
	for(auto* const field : fields()) field->unselect();
}

void RegisterGroup::fieldSelected()
{
	for(auto* const field : fields()) if(sender()!=field) field->unselect();
}

void RegisterGroup::adjustWidth()
{
	int widthNeeded=0;
	for(auto* const field : fields())
	{
		const auto widthToRequire=field->pos().x()+field->width();
		if(widthToRequire>widthNeeded) widthNeeded=widthToRequire;
	}
	setMinimumWidth(widthNeeded);
}

ODBRegView::ODBRegView(QWidget* parent)
    : QScrollArea(parent)
{
	auto* const canvas=new QWidget(this);
	auto* const canvasLayout=new QVBoxLayout(canvas);
	canvasLayout->setSpacing(0);
	canvas->setLayout(canvasLayout);
    setWidget(canvas);
    setWidgetResizable(true);
	// TODO: make this list user-selectable
	regGroupTypes={RegisterGroupType::GPR,
				   RegisterGroupType::rIP,
				   RegisterGroupType::Segment,
				   RegisterGroupType::EFL};
}

void ODBRegView::setModel(QAbstractItemModel* model)
{
	model_=model;
	connect(model,SIGNAL(modelReset()),this,SLOT(modelReset()));
	connect(model,SIGNAL(dataChanged(QModelIndex const&,QModelIndex const&)),this,SLOT(modelUpdated(QModelIndex const&,QModelIndex const&)));
	modelReset();
}

// TODO: switch from string-based search to enum-based one (add a new Role to model data)
QModelIndex findModelCategory(QAbstractItemModel const*const model,
							  QString const& catToFind)
{
	for(int row=0;row<model->rowCount();++row)
	{
		const auto cat=model->index(row,0).data(MODEL_NAME_COLUMN);
		if(cat.isValid() && cat.toString()==catToFind)
			return model->index(row,0);
	}
	return QModelIndex();
}

// TODO: switch from string-based search to enum-based one (add a new Role to model data)
QModelIndex findModelRegister(QModelIndex categoryIndex,
							  QString const& regToFind)
{
	auto* const model=categoryIndex.model();
	for(int row=0;row<model->rowCount(categoryIndex);++row)
	{
		const auto regIndex=model->index(row,MODEL_NAME_COLUMN,categoryIndex);
		const auto name=model->data(regIndex).toString();
		if(name.toUpper()==regToFind)
			return regIndex;
	}
	return QModelIndex();
}

void ODBRegView::addGroup(RegisterGroupType type)
{
	if(!model_->rowCount()) return;
	std::vector<QModelIndex> nameValCommentIndices;
	switch(type)
	{
	case RegisterGroupType::GPR:
	{
		const auto catIndex=findModelCategory(model_,"General Purpose");
		if(!catIndex.isValid()) break;
		for(int row=0;row<model_->rowCount(catIndex);++row)
			nameValCommentIndices.emplace_back(model_->index(row,MODEL_NAME_COLUMN,catIndex));
		break;
	}
	case RegisterGroupType::Segment:
	{
		const auto catIndex=findModelCategory(model_,"Segment");
		if(!catIndex.isValid()) break;
		for(int row=0;row<model_->rowCount(catIndex);++row)
			nameValCommentIndices.emplace_back(model_->index(row,MODEL_NAME_COLUMN,catIndex));
		break;
	}
	case RegisterGroupType::rIP:
	{
		const auto catIndex=findModelCategory(model_,"General Status");
		if(!catIndex.isValid()) break;
		nameValCommentIndices.emplace_back(findModelRegister(catIndex,"RIP"));
		nameValCommentIndices.emplace_back(findModelRegister(catIndex,"EIP"));
		break;
	}
	case RegisterGroupType::EFL:
	{
		const auto catIndex=findModelCategory(model_,"General Status");
		if(!catIndex.isValid()) break;
		nameValCommentIndices.emplace_back(findModelRegister(catIndex,"RFLAGS"));
		nameValCommentIndices.emplace_back(findModelRegister(catIndex,"EFLAGS"));
		break;
	}
	default: return;
	}
	nameValCommentIndices.erase(std::remove_if(nameValCommentIndices.begin(),
											   nameValCommentIndices.end(),
											   [](QModelIndex const& index){ return !index.isValid(); })
								,nameValCommentIndices.end());
	if(nameValCommentIndices.empty())
	{
		qWarning() << "Warning: failed to get any useful register indices for regGroupType" << static_cast<long>(type);
		return;
	}
	groups.push_back(new RegisterGroup(this));
	auto* const group=groups.back();
	for(const auto& index : nameValCommentIndices)
		group->appendNameValueComment(index);
	static_cast<QVBoxLayout*>(widget()->layout())->addWidget(group);
}

void ODBRegView::modelReset()
{
	setWidget(nullptr);
	for(auto* const group : groups)
		group->deleteLater();
	groups.clear();
	for(auto groupType : regGroupTypes)
		addGroup(groupType);
}

void ODBRegView::modelUpdated(QModelIndex const& topLeft,QModelIndex const& bottomRight)
{
	for(auto* const field : fields())
		if(field->isInRange(topLeft,bottomRight))
			field->update();
	for(auto* const group : groups)
		group->adjustWidth();
}

QList<FieldWidget*> ODBRegView::fields() const
{
	QList<FieldWidget*> allFields;
	for(auto* const group : groups)
		allFields.append(group->fields());
	return allFields;
}

void ODBRegView::finalize()
{
    for(auto* const field : fields())
    {
        if(!field->isEditable()) continue;

        FieldWidget *up=0, *down=0, *left=0, *right=0;
        // now look for unaligned neighbors if no aligned were found
        for(auto* const neighbor : fields())
        {
            if(!neighbor->isEditable()) continue;
            if(neighbor==field) continue;

            if(field->y()==neighbor->y())
            {
                if(neighbor->x()<field->x() && (!left || left->x()<neighbor->y()))
                    left=neighbor;
                if(neighbor->x()>field->x() && (!right || right->x()>neighbor->x()))
                    right=neighbor;
            }
            else if(neighbor->y() < field->y() && (!up || distSqr(neighbor,field) < distSqr(up,field)))
                up=neighbor;
            else if(neighbor->y() > field->y() && (!down || distSqr(neighbor,field) < distSqr(down,field)))
                down=neighbor;
        }
        field->setNeighbors(up,down,left,right);
    }
}

void ODBRegView::updateFieldsPalette()
{
    for(auto* const field : fields())
		field->updatePalette();
}

FieldWidget* ODBRegView::selectedField() const
{
    for(auto* const field : fields())
        if(field->isSelected()) return field;
    return nullptr;
}

void ODBRegView::focusOutEvent(QFocusEvent*)
{
	for(auto* const group : groups)
	{
		auto palette=group->palette();
		palette.setCurrentColorGroup(QPalette::Inactive);
		group->setPalette(palette);
	}
	updateFieldsPalette();
}

void ODBRegView::focusInEvent(QFocusEvent*)
{
	for(auto* const group : groups)
	{
		auto palette=group->palette();
		palette.setCurrentColorGroup(QPalette::Active);
		group->setPalette(palette);
	}
    updateFieldsPalette();
}

void ODBRegView::keyPressEvent(QKeyEvent* event)
{
    FieldWidget* selected=selectedField();
    if(!selected)
    {
        QScrollArea::keyPressEvent(event);
        return;
    }
    switch(event->key())
    {
    case Qt::Key_Up:
        if(selected->up())
            selected->up()->select();
        break;
    case Qt::Key_Down:
        if(selected->down())
            selected->down()->select();
        break;
    case Qt::Key_Left:
        if(selected->left())
            selected->left()->select();
        break;
    case Qt::Key_Right:
        if(selected->right())
            selected->right()->select();
        break;
    default:
        QScrollArea::keyPressEvent(event);
    }
}

}
