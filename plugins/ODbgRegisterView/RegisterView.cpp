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
#include <QPlastiqueStyle>
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
// TODO: FSR: Set Condition: G,L,E,Unordered
// TODO: PC: set 24/53/64-bit mantissa
// TODO: RC: round up/down/nearest

namespace
{

static const int MODEL_NAME_COLUMN=RegisterViewModelBase::Model::NAME_COLUMN;
static const int MODEL_VALUE_COLUMN=RegisterViewModelBase::Model::VALUE_COLUMN;
static const int MODEL_COMMENT_COLUMN=RegisterViewModelBase::Model::COMMENT_COLUMN;

template<typename T>
inline T sqr(T v) { return v*v; }

inline QPoint fieldPos(const FieldWidget* const field)
{
	// NOTE: mapToGlobal() is VERY slow, don't use it. Here we map to canvas, it's enough for all fields.
	return field->mapTo(field->parentWidget()->parentWidget(),QPoint());
}

// Square of Euclidean distance between two points
inline int distSqr(QPoint const& w1, QPoint const& w2)
{
	return sqr(w1.x()-w2.x())+sqr(w1.y()-w2.y());
}

inline QSize letterSize(QFont const& font)
{
	const QFontMetrics fontMetrics(font);
	const int width=fontMetrics.width('w');
	const int height=fontMetrics.height();
	return QSize(width,height);
}

static QPlastiqueStyle plastiqueStyle;

}

// --------------------- FieldWidget impl ----------------------------------
QString FieldWidget::text() const
{
	if(!index.isValid() && !this->isEnabled())
		return QLabel::text();
	const auto text=index.data();
	if(!text.isValid())
		return QString(width()/letterSize(font()).width()-1,QChar('?'));
	return text.toString();
}

void FieldWidget::init(int const fieldWidth)
{
	setObjectName("FieldWidget");
	const auto charSize=letterSize(font());
	setFixedHeight(charSize.height());
	if(fieldWidth>0)
		setFixedWidth(fieldWidth*charSize.width());
	setDisabled(true);
}

FieldWidget::FieldWidget(int const fieldWidth, QModelIndex const& index, QWidget* const parent)
	: QLabel("Fw???",parent),
	  index(index)
{
	init(fieldWidth);
}

FieldWidget::FieldWidget(int const fieldWidth, QString const& fixedText, QWidget* const parent)
	: QLabel(fixedText,parent)
{
	init(fieldWidth); // NOTE: fieldWidth!=fixedText.length() in general
}

FieldWidget::FieldWidget(QString const& fixedText, QWidget* const parent)
	: QLabel(fixedText,parent)
{
	init(fixedText.length());
}

void FieldWidget::update()
{
	QLabel::setText(text());
	adjustSize();
}

ODBRegView* FieldWidget::regView() const
{
	auto* const parent=parentWidget() // group
						->parentWidget() // canvas
						->parentWidget() // viewport
						->parentWidget(); // regview
	Q_ASSERT(dynamic_cast<ODBRegView*>(parent));
	return static_cast<ODBRegView*>(parent);
}

// --------------------- ValueField impl ----------------------------------
ValueField::ValueField(int const fieldWidth,
					   QModelIndex const& index,
					   QWidget* const parent,
					   std::function<QString(QString const&)> const& valueFormatter
					  )
	: FieldWidget(fieldWidth,index,parent),
	  valueFormatter(valueFormatter)
{
	setObjectName("ValueField");
	setDisabled(false);
	setMouseTracking(true);
	// Set some known style to avoid e.g. Oxygen's label transition animations, which
	// break updating of colors such as "register changed" when single-stepping frequently
	setStyle(&plastiqueStyle);
}

ValueField* ValueField::bestNeighbor(std::function<bool(QPoint const&,ValueField const*,QPoint const&)>const& firstIsBetter) const
{
	ValueField* result=nullptr;
	for(auto* const neighbor : regView()->valueFields())
	{
		if(neighbor->isVisible() && firstIsBetter(fieldPos(neighbor),result,fieldPos(this)))
			result=neighbor;
	}
	return result;
}

ValueField* ValueField::up() const
{
	return bestNeighbor([](QPoint const& nPos,ValueField const*up,QPoint const& fPos)
			{ return nPos.y() < fPos.y() && (!up || distSqr(nPos,fPos) < distSqr(fieldPos(up),fPos)); });
}

ValueField* ValueField::down() const
{
	return bestNeighbor([](QPoint const& nPos,ValueField const*down,QPoint const& fPos)
			{ return nPos.y() > fPos.y() && (!down || distSqr(nPos,fPos) < distSqr(fieldPos(down),fPos)); });
}

ValueField* ValueField::left() const
{
	return bestNeighbor([](QPoint const& nPos,ValueField const*left,QPoint const& fPos)
			{ return nPos.y()==fPos.y() && nPos.x()<fPos.x() && (!left || left->x()<nPos.x()); });
}

ValueField* ValueField::right() const
{
	return bestNeighbor([](QPoint const& nPos,ValueField const*right,QPoint const& fPos)
			{ return nPos.y()==fPos.y() && nPos.x()>fPos.x() && (!right || right->x()>nPos.x()); });
}

QString ValueField::text() const
{
	return valueFormatter(FieldWidget::text());
}

bool ValueField::changed() const
{
	if(!index.isValid()) return true;
	const auto changed=index.data(RegisterViewModelBase::Model::RegisterChangedRole);
	Q_ASSERT(changed.isValid());
	return changed.toBool();
}

QColor ValueField::fgColorForChangedField() const
{
	return Qt::red; // TODO: read from user palette
}

bool ValueField::isSelected() const
{
	return selected_;
}

void ValueField::update()
{
	FieldWidget::update();
	updatePalette();
}

void ValueField::updatePalette()
{
	auto palette=this->palette();
	const auto appPalette=QApplication::palette();

	// TODO: make selected items still red if they've changed
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

void ValueField::enterEvent(QEvent*)
{
	hovered_=true;
	updatePalette();
}

void ValueField::leaveEvent(QEvent*)
{
	hovered_=false;
	updatePalette();
}

void ValueField::select()
{
	if(selected_) return;
	selected_=true;
	Q_EMIT selected();
	updatePalette();
}

void ValueField::mousePressEvent(QMouseEvent* event)
{
	if(event->button()==Qt::LeftButton)
		select();
}

void ValueField::unselect()
{
	if(!selected_) return;
	selected_=false;
	updatePalette();
}

void ValueField::mouseDoubleClickEvent(QMouseEvent* event)
{
	mousePressEvent(event);
	QMessageBox::information(this,"Double-clicked",QString("Double-clicked field %1 with contents \"%2\"")
								.arg(QString().sprintf("%p",static_cast<void*>(this))).arg(text()));
}

void ValueField::paintEvent(QPaintEvent*)
{
	QPainter painter(this);
	QStyleOptionViewItemV4 option;
	option.rect=rect();
	option.showDecorationSelected=true;
	option.text=text();
	option.font=font();
	option.palette=palette();
	option.textElideMode=Qt::ElideNone;
	option.state |= QStyle::State_Enabled;
	if(selected_) option.state |= QStyle::State_Selected;
	if(hovered_)  option.state |= QStyle::State_MouseOver;
	if(palette().currentColorGroup()==QPalette::Active)
		option.state |= QStyle::State_Active;
	QApplication::style()->drawControl(QStyle::CE_ItemViewItem, &option, &painter);
}

// -------------------------------- RegisterGroup impl ----------------------------
RegisterGroup::RegisterGroup(QWidget* parent)
	: QWidget(parent)
{
	setObjectName("RegisterGroup");
}

void RegisterGroup::mousePressEvent(QMouseEvent* event)
{
	event->ignore();
}

void RegisterGroup::insert(int const line, int const column, FieldWidget* const widget)
{
	widget->update();
	if(auto* const value=dynamic_cast<ValueField*>(widget))
		connect(value,SIGNAL(selected()),parent(),SLOT(fieldSelected()));

	const auto charSize=letterSize(font());
	const auto charWidth=charSize.width();
	const auto charHeight=charSize.height();
	// extra space for highlighting rectangle, so that single-digit fields are easier to target
	const auto marginLeft=charWidth/2;
	const auto marginRight=charWidth-marginLeft;

	QPoint position(charWidth*column,charHeight*line);
	position -= QPoint(marginLeft,0);

	QSize size(widget->size());
	size += QSize(marginLeft+marginRight,0);

	widget->setMinimumSize(size);
	widget->move(position);
	// FIXME: why are e.g. regnames like FSR truncated without the -1?
	widget->setContentsMargins(marginLeft,0,marginRight-1,0);

	const auto potentialNewWidth=widget->pos().x()+widget->width();
	const auto potentialNewHeight=widget->pos().y()+widget->height();
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
	const auto valueIndex=nameIndex.sibling(nameIndex.row(),Model::VALUE_COLUMN);
	const auto valueWidth=valueIndex.data(Model::FixedLengthRole).toInt();
	Q_ASSERT(valueWidth>0);

	const int line=lineAfterLastField();
	int column=0;
	insert(line,column,new FieldWidget(nameWidth,nameIndex.data().toString(),this));
	column+=nameWidth+1;
	insert(line,column,new ValueField(valueWidth,valueIndex,this));
	if(insertComment)
	{
		column+=valueWidth+1;
		const auto commentIndex=nameIndex.sibling(nameIndex.row(),Model::COMMENT_COLUMN);
		insert(line,column,new FieldWidget(0,commentIndex,this));
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

QList<ValueField*> RegisterGroup::valueFields() const
{
	QList<ValueField*> allValues;
	for(auto* const field : fields())
	{
		auto* const value=dynamic_cast<ValueField*>(field);
		if(value) allValues.push_back(value);
	}
	return allValues;
}

void ODBRegView::mousePressEvent(QMouseEvent* event)
{
	if(event->type()!=QEvent::MouseButtonPress) return;
	for(auto* const field : valueFields()) field->unselect();
}

void ODBRegView::fieldSelected()
{
	for(auto* const field : valueFields())
		if(sender()!=field)
			field->unselect();
	ensureWidgetVisible(static_cast<QWidget*>(sender()),0,0);
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
	setObjectName("ODBRegView");
	QFont font("Monospace");
	font.setStyleHint(QFont::TypeWriter);
	setFont(font);

	auto* const canvas=new QWidget(this);
	canvas->setObjectName("RegViewCanvas");
	auto* const canvasLayout=new QVBoxLayout(canvas);
	canvasLayout->setSpacing(letterSize(this->font()).height()/2);
	canvasLayout->setContentsMargins(contentsMargins());
	canvasLayout->setAlignment(Qt::AlignTop);
	canvas->setLayout(canvasLayout);
	canvas->setBackgroundRole(QPalette::Base);
	canvas->setAutoFillBackground(true);

	setWidget(canvas);
	setWidgetResizable(true);
	// TODO: make this list user-selectable
	regGroupTypes={RegisterGroupType::GPR,
				   RegisterGroupType::rIP,
				   RegisterGroupType::ExpandedEFL,
				   RegisterGroupType::Segment,
				   RegisterGroupType::EFL,
				   RegisterGroupType::FPUData,
				   RegisterGroupType::FPUWords,
				   RegisterGroupType::FPULastOp,
				   RegisterGroupType::Debug,
				   RegisterGroupType::MXCSR
				  };
}

void ODBRegView::setModel(QAbstractItemModel* model)
{
	model_=model;
	connect(model,SIGNAL(modelReset()),this,SLOT(modelReset()));
	connect(model,SIGNAL(dataChanged(QModelIndex const&,QModelIndex const&)),this,SLOT(modelUpdated()));
	modelReset();
}

namespace
{
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
							  QString const& regToFind,
							  int column=MODEL_NAME_COLUMN)
{
	auto* const model=categoryIndex.model();
	for(int row=0;row<model->rowCount(categoryIndex);++row)
	{
		const auto regIndex=model->index(row,MODEL_NAME_COLUMN,categoryIndex);
		const auto name=model->data(regIndex).toString();
		if(name.toUpper()==regToFind)
		{
			if(column==MODEL_NAME_COLUMN)
				return regIndex;
			return regIndex.sibling(regIndex.row(),column);
		}
	}
	return QModelIndex();
}

QModelIndex getCommentIndex(QModelIndex const& nameIndex)
{
	Q_ASSERT(nameIndex.isValid());
	return nameIndex.sibling(nameIndex.row(),MODEL_COMMENT_COLUMN);
}

QModelIndex getValueIndex(QModelIndex const& nameIndex)
{
	Q_ASSERT(nameIndex.isValid());
	return nameIndex.sibling(nameIndex.row(),MODEL_VALUE_COLUMN);
}

void addRoundingMode(RegisterGroup* const group, QModelIndex const& index, int const row, int const column)
{
	Q_ASSERT(index.isValid());
	group->insert(row,column,new ValueField(4,index,group,[](QString const& str)
				{
					Q_ASSERT(str.length());
					if(str[0]=='?') return "????";
					bool roundModeParseOK=false;
					const int value=str.toInt(&roundModeParseOK);
					if(!roundModeParseOK) return "????";
					Q_ASSERT(0<=value && value<=3);
					static const char* strings[]={"NEAR","DOWN","  UP","ZERO"};
					return strings[value];
				}));
}

void addPrecisionMode(RegisterGroup* const group, QModelIndex const& index, int const row, int const column)
{
	Q_ASSERT(index.isValid());
	group->insert(row,column,new ValueField(2,index,group,[](QString const& str)
				{
					Q_ASSERT(str.length());
					if(str[0]=='?') return "??";
					bool precModeParseOK=false;
					const int value=str.toInt(&precModeParseOK);
					if(!precModeParseOK) return "??";
					Q_ASSERT(0<=value && value<=3);
					static const char* strings[]={"24","??","53","64"};
					return strings[value];
				}));
}

void addPUOZDI(RegisterGroup* const group, QModelIndex const& excRegIndex, QModelIndex const& maskRegIndex, int const startRow, int const startColumn)
{
	QString const exceptions="PUOZDI";
	for(int exN=0;exN<exceptions.length();++exN)
	{
		QString const ex=exceptions[exN];
		const auto excIndex=findModelRegister(excRegIndex,ex+"E");
		Q_ASSERT(excIndex.isValid());
		const auto maskIndex=findModelRegister(maskRegIndex,ex+"M");
		Q_ASSERT(maskIndex.isValid());
		const int column=startColumn+exN*2;
		group->insert(startRow,column,new FieldWidget(ex,group));
		group->insert(startRow+1,column,new ValueField(1,getValueIndex(excIndex),group));
		group->insert(startRow+2,column,new ValueField(1,getValueIndex(maskIndex),group));
	}
}

}

RegisterGroup* fillEFL(RegisterGroup* group, QAbstractItemModel* model)
{
	const auto catIndex=findModelCategory(model,"General Status");
	if(!catIndex.isValid()) return nullptr;
	auto nameIndex=findModelRegister(catIndex,"RFLAGS");
	if(!nameIndex.isValid())
		nameIndex=findModelRegister(catIndex,"EFLAGS");
	if(!nameIndex.isValid()) return nullptr;
	const int nameWidth=3;
	int column=0;
	group->insert(0,column,new FieldWidget("EFL",group));
	const auto valueWidth=8;
	const auto valueIndex=nameIndex.sibling(nameIndex.row(),MODEL_VALUE_COLUMN);
	column+=nameWidth+1;
	group->insert(0,column,new ValueField(valueWidth,valueIndex,group,[](QString const& v){return v.right(8);}));
	const auto commentIndex=nameIndex.sibling(nameIndex.row(),MODEL_COMMENT_COLUMN);
	column+=valueWidth+1;
	group->insert(0,column,new FieldWidget(0,commentIndex,group));

	return group;
}

RegisterGroup* fillExpandedEFL(RegisterGroup* group, QAbstractItemModel* model)
{
	const auto catIndex=findModelCategory(model,"General Status");
	if(!catIndex.isValid()) return nullptr;
	auto regNameIndex=findModelRegister(catIndex,"RFLAGS");
	if(!regNameIndex.isValid())
		regNameIndex=findModelRegister(catIndex,"EFLAGS");
	if(!regNameIndex.isValid()) return nullptr;
	for(int row=0,groupRow=0;row<model->rowCount(regNameIndex);++row)
	{
		const auto flagNameIndex=model->index(row,MODEL_NAME_COLUMN,regNameIndex);
		const auto flagValueIndex=model->index(row,MODEL_VALUE_COLUMN,regNameIndex);
		const auto flagName=model->data(flagNameIndex).toString().toUpper();
		if(flagName.length()!=2 || flagName[1]!='F') continue;
		static const int flagNameWidth=1;
		static const int valueWidth=1;
		const char name=flagName[0].toLatin1();
		switch(name)
		{
		case 'C':
		case 'P':
		case 'A':
		case 'Z':
		case 'S':
		case 'T':
		case 'D':
		case 'O':
			group->insert(groupRow,0,new FieldWidget(QChar(name),group));
			group->insert(groupRow,flagNameWidth+1,new ValueField(valueWidth,flagValueIndex,group));
			++groupRow;
			break;
		default:
			continue;
		}
	}
	return group;
}

RegisterGroup* fillFPUData(RegisterGroup* group, QAbstractItemModel* model)
{
	using RegisterViewModelBase::Model;

	const auto catIndex=findModelCategory(model,"FPU");
	if(!catIndex.isValid()) return nullptr;
	const auto tagsIndex=findModelRegister(catIndex,"FTR");
	if(!tagsIndex.isValid())
	{
		qWarning() << "Warning: failed to find FTR in the model, refusing to continue making FPUData group";
		return nullptr;
	}
	static const int FPU_REG_COUNT=8;
	static const int nameWidth=3;
	static const int tagWidth=7;
	for(int row=0;row<FPU_REG_COUNT;++row)
	{
		int column=0;
		const auto nameIndex=model->index(row,MODEL_NAME_COLUMN,catIndex);
		const auto nameV=nameIndex.data();
		Q_ASSERT(nameV.isValid());
		group->insert(row,column,new FieldWidget(nameWidth,nameV.toString(),group));
		column+=nameWidth+1;
		const auto tagCommentIndex=model->index(row,MODEL_COMMENT_COLUMN,tagsIndex);
		Q_ASSERT(tagCommentIndex.isValid());
		group->insert(row,column,new ValueField(tagWidth,tagCommentIndex,group,
												[](QString const&s){return s.toLower();}));
		column+=tagWidth+1;
		// Always show float-formatted value, not raw
		const auto regValueIndex=findModelRegister(nameIndex,"FLOAT",MODEL_VALUE_COLUMN);
		const int regValueWidth=regValueIndex.data(Model::FixedLengthRole).toInt();
		Q_ASSERT(regValueWidth>0);
		group->insert(row,column,new ValueField(regValueWidth,regValueIndex,group));
		column+=regValueWidth+1;
		const auto regCommentIndex=model->index(row,MODEL_COMMENT_COLUMN,catIndex);
		group->insert(row,column,new FieldWidget(0,regCommentIndex,group));
	}
	return group;
}

RegisterGroup* fillFPUWords(RegisterGroup* group, QAbstractItemModel* model)
{
	const auto catIndex=findModelCategory(model,"FPU");
	if(!catIndex.isValid()) return nullptr;
	group->appendNameValueComment(findModelRegister(catIndex,"FTR"),false);
	const int fsrRow=1;
	const auto fsrIndex=findModelRegister(catIndex,"FSR");
	group->appendNameValueComment(fsrIndex,false);
	const int fcrRow=2;
	const auto fcrIndex=findModelRegister(catIndex,"FCR");
	group->appendNameValueComment(fcrIndex,false);

	const int wordNameWidth=3, wordValWidth=4;
	const int condPrecLabelColumn=wordNameWidth+1+wordValWidth+1+1;
	const int condPrecLabelWidth=4;
	group->insert(fsrRow,condPrecLabelColumn,new FieldWidget("Cond",group));
	group->insert(fcrRow,condPrecLabelColumn,new FieldWidget("Prec",group));
	const int condPrecValColumn=condPrecLabelColumn+condPrecLabelWidth+1;
	const int roundModeWidth=4, precModeWidth=2;
	const int roundModeColumn=condPrecValColumn;
	const int precModeColumn=roundModeColumn+roundModeWidth+1;
	// This must be inserted before precision&rounding value fields, since they overlap this label
	group->insert(fcrRow,precModeColumn-1,new FieldWidget(",",group));
	for(int condN=3;condN>=0;--condN)
	{
		const auto condNNameIndex=findModelRegister(fsrIndex,QString("C%1").arg(condN));
		Q_ASSERT(condNNameIndex.isValid());
		const auto condNIndex=condNNameIndex.sibling(condNNameIndex.row(),MODEL_VALUE_COLUMN);
		Q_ASSERT(condNIndex.isValid());
		const int column=condPrecValColumn+2*(3-condN);
		group->insert(fsrRow-1,column,new FieldWidget(QString("%1").arg(condN),group));
		group->insert(fsrRow,  column,new ValueField(1,condNIndex,group));
	}
	addRoundingMode(group,findModelRegister(fcrIndex,"RC",MODEL_VALUE_COLUMN),fcrRow,roundModeColumn);
	addPrecisionMode(group,findModelRegister(fcrIndex,"PC",MODEL_VALUE_COLUMN),fcrRow,precModeColumn);
	const int errMaskColumn=precModeColumn+precModeWidth+2;
	const int errLabelWidth=3;
	group->insert(fsrRow,errMaskColumn,new FieldWidget("Err",group));
	group->insert(fcrRow,errMaskColumn,new FieldWidget("Mask",group));
	const int ESColumn=errMaskColumn+errLabelWidth+1;
	const int SFColumn=ESColumn+2;
	group->insert(fsrRow-1,ESColumn,new FieldWidget("E",group));
	group->insert(fsrRow-1,SFColumn,new FieldWidget("S",group));
	group->insert(fsrRow,ESColumn,new ValueField(1,findModelRegister(fsrIndex,"ES",MODEL_VALUE_COLUMN),group));
	group->insert(fsrRow,SFColumn,new ValueField(1,findModelRegister(fsrIndex,"SF",MODEL_VALUE_COLUMN),group));
	const int PEPMColumn=SFColumn+2;
	addPUOZDI(group,fsrIndex,fcrIndex,fsrRow-1,PEPMColumn);
	const int PUOZDIWidth=6*2-1;
	group->insert(fsrRow,PEPMColumn+PUOZDIWidth+1,new FieldWidget(0,getCommentIndex(fsrIndex),group));

	return group;
}

RegisterGroup* fillFPULastOp(RegisterGroup* group, QAbstractItemModel* model)
{
	using RegisterViewModelBase::Model;

	enum {lastInsnRow, lastDataRow, lastOpcodeRow};
	const QString lastInsnLabel="Last insn";
	const QString lastDataLabel="Last data";
	const QString lastOpcodeLabel="Last opcode";
	group->insert(lastInsnRow,0,new FieldWidget(lastInsnLabel,group));
	group->insert(lastDataRow,0,new FieldWidget(lastDataLabel,group));
	group->insert(lastOpcodeRow,0,new FieldWidget(lastOpcodeLabel,group));

	const auto catIndex=findModelCategory(model,"FPU");
	const auto FIPIndex=findModelRegister(catIndex,"FIP",MODEL_VALUE_COLUMN);
	const auto FDPIndex=findModelRegister(catIndex,"FDP",MODEL_VALUE_COLUMN);

	// FIS & FDS are not maintained in 64-bit mode; Linux64 always saves state from
	// 64-bit mode, losing the values for 32-bit apps even if the CPU doesn't deprecate them
	// We'll show zero offsets in 32 bit mode for consistency with 32-bit kernels
	// In 64-bit mode, since segments are not maintained, we'll just show offsets
	const auto FIPwidth=FDPIndex.data(Model::FixedLengthRole).toInt();
	const auto segWidth = FIPwidth==8/*8chars=>32bit*/ ? 4 : 0;
	const auto segColumn=lastInsnLabel.length()+1;
	if(segWidth)
	{
		// these two must be inserted first, because seg & offset value fields overlap these labels
		group->insert(lastInsnRow,segColumn+segWidth,new FieldWidget(":",group));
		group->insert(lastDataRow,segColumn+segWidth,new FieldWidget(":",group));

		group->insert(lastInsnRow,segColumn,
				new ValueField(segWidth,findModelRegister(catIndex,"FIS",MODEL_VALUE_COLUMN),group));
		group->insert(lastDataRow,segColumn,
				new ValueField(segWidth,findModelRegister(catIndex,"FDS",MODEL_VALUE_COLUMN),group));
	}
	const auto offsetWidth=FIPIndex.data(Model::FixedLengthRole).toInt();
	Q_ASSERT(offsetWidth>0);
	const auto offsetColumn=segColumn+segWidth+(segWidth?1:0);
	group->insert(lastInsnRow,offsetColumn,new ValueField(offsetWidth,FIPIndex,group));
	group->insert(lastDataRow,offsetColumn,new ValueField(offsetWidth,FDPIndex,group));

	QPersistentModelIndex const FOPIndex=findModelRegister(catIndex,"FOP",MODEL_VALUE_COLUMN);
	QPersistentModelIndex const FSRIndex=findModelRegister(catIndex,"FSR",MODEL_VALUE_COLUMN);
	QPersistentModelIndex const FCRIndex=findModelRegister(catIndex,"FCR",MODEL_VALUE_COLUMN);
	const auto FOPFormatter=[FOPIndex,FSRIndex,FCRIndex](QString const& str)
	{
		if(str.isEmpty() || str[0]=='?') return str;

		const auto rawFCR=FCRIndex.data(Model::RawValueRole).toByteArray();
		Q_ASSERT(rawFCR.size()<=long(sizeof(edb::value16)));
		if(rawFCR.isEmpty()) return str;
		edb::value16 fcr(0);
		std::memcpy(&fcr,rawFCR.constData(),rawFCR.size());

		const auto rawFSR=FSRIndex.data(Model::RawValueRole).toByteArray();
		Q_ASSERT(rawFSR.size()<=long(sizeof(edb::value16)));
		if(rawFSR.isEmpty()) return str;
		edb::value16 fsr(0);
		std::memcpy(&fsr,rawFSR.constData(),rawFSR.size());

		const auto rawFOP=FOPIndex.data(Model::RawValueRole).toByteArray();
		edb::value16 fop(0);
		Q_ASSERT(rawFOP.size()<=long(sizeof(edb::value16)));
		if(rawFOP.isEmpty()) return str;
		if(rawFOP.size()!=sizeof(edb::value16))
			return QString("????");
		std::memcpy(&fop,rawFOP.constData(),rawFOP.size());

		const auto excMask=fcr&0x3f;
		const auto excActive=fsr&0x3f;
		const auto excActiveUnmasked=excActive&~excMask;
		// TODO: check whether fopcode is actually not updated. This behavior starts 
		// from Pentium 4 & Xeon, but not true for e.g. Atom and other P6-based CPUs
		// TODO: move this to ArchProcessor/RegisterViewModel?
		if(fop==0 && !excActiveUnmasked)
			return QString("00 00");
		return edb::value8(0xd8+rawFOP[1]).toHexString()+
				' '+edb::value8(rawFOP[0]).toHexString();
	};
	group->insert(lastOpcodeRow,lastOpcodeLabel.length()+1,
					new ValueField(5,FOPIndex,group,FOPFormatter));

	return group;
}

RegisterGroup* fillDebugGroup(RegisterGroup* group, QAbstractItemModel* model)
{
	using RegisterViewModelBase::Model;

	const auto catIndex=findModelCategory(model,"Debug");
	if(!catIndex.isValid()) return nullptr;
	const auto dr6Index=findModelRegister(catIndex,"DR6");
	const auto dr7Index=findModelRegister(catIndex,"DR7");
	Q_ASSERT(dr6Index.isValid());
	Q_ASSERT(dr7Index.isValid());
	const auto nameWidth=3;
	const auto valueWidth=getValueIndex(dr6Index).data(Model::FixedLengthRole).toInt();
	Q_ASSERT(valueWidth>0);
	int row=0;
	const auto bitsSpacing=1;
	{
		int column=nameWidth+1+valueWidth+2;
		group->insert(row,column,new FieldWidget("B",group));
		column+=bitsSpacing+1;
		group->insert(row,column,new FieldWidget("L",group));
		column+=bitsSpacing+1;
		group->insert(row,column,new FieldWidget("G",group));
		column+=bitsSpacing+1;
		group->insert(row,column,new FieldWidget("Type",group));
		column+=bitsSpacing+4;
		group->insert(row,column,new FieldWidget("Len",group));
		column+=bitsSpacing+3;

		++row;
	}
	for(int drI=0;drI<4;++drI,++row)
	{
		const auto name=QString("DR%1").arg(drI);
		const auto DRiValueIndex=findModelRegister(catIndex,name,MODEL_VALUE_COLUMN);
		Q_ASSERT(DRiValueIndex.isValid());
		int column=0;
		group->insert(row,column,new FieldWidget(name,group));
		column+=nameWidth+1;
		group->insert(row,column,new ValueField(valueWidth,DRiValueIndex,group));
		column+=valueWidth+2;
		{
			const auto BiName=QString("B%1").arg(drI);
			const auto BiIndex=findModelRegister(dr6Index,BiName,MODEL_VALUE_COLUMN);
			Q_ASSERT(BiIndex.isValid());
			group->insert(row,column,new ValueField(1,BiIndex,group));
			column+=bitsSpacing+1;
		}
		{
			const auto LiName=QString("L%1").arg(drI);
			const auto LiIndex=findModelRegister(dr7Index,LiName,MODEL_VALUE_COLUMN);
			Q_ASSERT(LiIndex.isValid());
			group->insert(row,column,new ValueField(1,LiIndex,group));
			column+=bitsSpacing+1;
		}
		{
			const auto GiName=QString("G%1").arg(drI);
			const auto GiIndex=findModelRegister(dr7Index,GiName,MODEL_VALUE_COLUMN);
			Q_ASSERT(GiIndex.isValid());
			group->insert(row,column,new ValueField(1,GiIndex,group));
			column+=bitsSpacing+1;
		}
		{
			const auto RWiName=QString("R/W%1").arg(drI);
			const QPersistentModelIndex RWiIndex=findModelRegister(dr7Index,RWiName,MODEL_VALUE_COLUMN);
			Q_ASSERT(RWiIndex.isValid());
			const auto width=5;
			group->insert(row,column,new ValueField(width,RWiIndex,group,[RWiIndex](QString const& str)->QString
						{
							if(str.isEmpty() || str[0]=='?') return "??";
							Q_ASSERT(str.size()==1);
							switch(str[0].toLatin1())
							{
							case '0': return "EXEC";
							case '1': return "WRITE";
							case '2': return " IO";
							case '3': return " R/W";
							default: return "???";
							}
						}));
			column+=bitsSpacing+width;
		}
		{
			const auto LENiName=QString("LEN%1").arg(drI);
			const QPersistentModelIndex LENiIndex=findModelRegister(dr7Index,LENiName,MODEL_VALUE_COLUMN);
			Q_ASSERT(LENiIndex.isValid());
			group->insert(row,column,new ValueField(1,LENiIndex,group,[LENiIndex](QString const& str)->QString
						{
							if(str.isEmpty() || str[0]=='?') return "??";
							Q_ASSERT(str.size()==1);
							switch(str[0].toLatin1())
							{
							case '0': return "1";
							case '1': return "2";
							case '2': return "8";
							case '3': return "4";
							default: return "???";
							}
						}));
		}
	}
	{
		int column=0;
		group->insert(row,column,new FieldWidget("DR6",group));
		column+=nameWidth+1;
		group->insert(row,column,new ValueField(valueWidth,getValueIndex(dr6Index),group));
		column+=valueWidth+2;
		const QString bsName="BS";
		const auto bsWidth=bsName.length();
		group->insert(row,column,new FieldWidget(bsName,group));
		column+=bsWidth+1;
		const auto bsIndex=findModelRegister(dr6Index,bsName,MODEL_VALUE_COLUMN);
		group->insert(row,column,new ValueField(1,bsIndex,group));

		++row;
	}
	{
		int column=0;
		group->insert(row,column,new FieldWidget("DR7",group));
		column+=nameWidth+1;
		group->insert(row,column,new ValueField(valueWidth,getValueIndex(dr7Index),group));
		column+=valueWidth+2;
		{
			const QString leName="LE";
			const auto leWidth=leName.length();
			group->insert(row,column,new FieldWidget(leName,group));
			column+=leWidth+1;
			const auto leIndex=findModelRegister(dr7Index,leName,MODEL_VALUE_COLUMN);
			const auto leValueWidth=1;
			group->insert(row,column,new ValueField(leValueWidth,leIndex,group));
			column+=leValueWidth+1;
		}
		{
			const QString geName="GE";
			const auto geWidth=geName.length();
			group->insert(row,column,new FieldWidget(geName,group));
			column+=geWidth+1;
			const auto geIndex=findModelRegister(dr7Index,geName,MODEL_VALUE_COLUMN);
			const auto geValueWidth=1;
			group->insert(row,column,new ValueField(geValueWidth,geIndex,group));
			column+=geValueWidth+1;
		}
	}

	return group;
}

RegisterGroup* fillMXCSR(RegisterGroup* group, QAbstractItemModel* model)
{
	using namespace RegisterViewModelBase;

	const auto catIndex=findModelCategory(model,"SSE");
	if(!catIndex.isValid()) return nullptr;
	const QString mxcsrName="MXCSR";
	int column=0;
	const int mxcsrRow=1, fzRow=mxcsrRow,dazRow=mxcsrRow,excRow=mxcsrRow;
	const int rndRow=fzRow+1, maskRow=rndRow;
	group->insert(mxcsrRow,column,new FieldWidget(mxcsrName,group));
	column+=mxcsrName.length()+1;
	const auto mxcsrIndex=findModelRegister(catIndex,"MXCSR",MODEL_VALUE_COLUMN);
	const auto mxcsrValueWidth=mxcsrIndex.data(Model::FixedLengthRole).toInt();
	Q_ASSERT(mxcsrValueWidth>0);
	group->insert(mxcsrRow,column,new ValueField(mxcsrValueWidth,mxcsrIndex,group));
	column+=mxcsrValueWidth+2;
	// XXX: Sacrificing understandability of DAZ->DZ to align PUOZDI with FPU's.
	// Also FZ value is one char away from DAZ name, which is also no good.
	// Maybe following OllyDbg example here isn't a good idea.
	const QString fzName="FZ", dazName="DZ";
	const auto fzColumn=column;
	group->insert(fzRow,fzColumn,new FieldWidget(fzName,group));
	column+=fzName.length()+1;
	const auto fzIndex=findModelRegister(mxcsrIndex,"FZ",MODEL_VALUE_COLUMN);
	const auto fzValueWidth=1;
	group->insert(fzRow,column,new ValueField(fzValueWidth,fzIndex,group));
	column+=fzValueWidth+1;
	group->insert(dazRow,column,new FieldWidget(dazName,group));
	column+=dazName.length()+1;
	const auto dazIndex=findModelRegister(mxcsrIndex,"DAZ",MODEL_VALUE_COLUMN);
	const auto dazValueWidth=1;
	group->insert(dazRow,column,new ValueField(dazValueWidth,dazIndex,group));
	column+=dazValueWidth+2;
	const QString excName="Err";
	group->insert(excRow,column,new FieldWidget(excName,group));
	const QString maskName="Mask";
	group->insert(maskRow,column,new FieldWidget(maskName,group));
	column+=maskName.length()+1;
	addPUOZDI(group,mxcsrIndex,mxcsrIndex,excRow-1,column);
	const auto rndNameColumn=fzColumn;
	const QString rndName="Rnd";
	group->insert(rndRow,rndNameColumn,new FieldWidget(rndName,group));
	const auto rndColumn=rndNameColumn+rndName.length()+1;
	addRoundingMode(group,findModelRegister(mxcsrIndex,"RC",MODEL_VALUE_COLUMN),rndRow,rndColumn);

	return group;
}

RegisterGroup* ODBRegView::makeGroup(RegisterGroupType type)
{
	if(!model_->rowCount()) return nullptr;
	std::vector<QModelIndex> nameValCommentIndices;
	using RegisterViewModelBase::Model;
	groups.push_back(new RegisterGroup(this));
	auto* const group=groups.back();
	switch(type)
	{
	case RegisterGroupType::EFL: return fillEFL(group,model_);
	case RegisterGroupType::ExpandedEFL: return fillExpandedEFL(group,model_);
	case RegisterGroupType::FPUData: return fillFPUData(group,model_);
	case RegisterGroupType::FPUWords: return fillFPUWords(group,model_);
	case RegisterGroupType::FPULastOp: return fillFPULastOp(group,model_);
	case RegisterGroupType::Debug: return fillDebugGroup(group,model_);
	case RegisterGroupType::MXCSR: return fillMXCSR(group,model_);
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
	default:
		qWarning() << "Warning: unexpected register group type requested in" << Q_FUNC_INFO;
		groups.pop_back();
		return nullptr;
	}
	nameValCommentIndices.erase(std::remove_if(nameValCommentIndices.begin(),
											   nameValCommentIndices.end(),
											   [](QModelIndex const& index){ return !index.isValid(); })
								,nameValCommentIndices.end());
	if(nameValCommentIndices.empty())
	{
		qWarning() << "Warning: failed to get any useful register indices for regGroupType" << static_cast<long>(type);
		groups.pop_back();
		return nullptr;
	}
	for(const auto& index : nameValCommentIndices)
		group->appendNameValueComment(index);
	return group;
}

void ODBRegView::modelReset()
{
	widget()->hide(); // prevent flicker while groups are added to/removed from the layout
	// not all groups may be in the layout, so delete them individually
	for(auto* const group : groups)
		group->deleteLater();
	groups.clear();

	auto* const layout=static_cast<QVBoxLayout*>(widget()->layout());

	// layout contains not only groups, so delete all items too
	while(auto* const item=layout->takeAt(0))
		delete item;

	auto* const flagsAndSegments=new QHBoxLayout();
	// (3/2+1/2)-letter — Total of 2-letter spacing. Fourth half-letter is from flag values extension.
	// Segment extensions at LHS of the widget don't influence minimumSize request, so no need to take
	// them into account.
	flagsAndSegments->setSpacing(letterSize(this->font()).width()*3/2);
	flagsAndSegments->setContentsMargins(QMargins());
	flagsAndSegments->setAlignment(Qt::AlignLeft);

	bool flagsAndSegsInserted=false;
	for(auto groupType : regGroupTypes)
	{
		auto*const group=makeGroup(groupType);
		if(!group) continue;
		if(groupType==RegisterGroupType::Segment || groupType==RegisterGroupType::ExpandedEFL)
		{
			flagsAndSegments->addWidget(group);
			if(!flagsAndSegsInserted)
			{
				layout->addLayout(flagsAndSegments);
				flagsAndSegsInserted=true;
			}
		}
		else layout->addWidget(group);
	}
	widget()->show();
}

void ODBRegView::modelUpdated()
{
	for(auto* const field : fields())
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

QList<ValueField*> ODBRegView::valueFields() const
{
	QList<ValueField*> allValues;
	for(auto* const group : groups)
		allValues.append(group->valueFields());
	return allValues;

}

void ODBRegView::updateFieldsPalette()
{
	for(auto* const field : valueFields())
		field->updatePalette();
}

ValueField* ODBRegView::selectedField() const
{
	for(auto* const field : valueFields())
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
	auto* const selected=selectedField();
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
