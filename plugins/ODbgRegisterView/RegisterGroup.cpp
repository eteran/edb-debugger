
#include "RegisterGroup.h"
#include "ODbgRV_Util.h"
#include "RegisterView.h"
#include "ValueField.h"

#include <QMouseEvent>

namespace ODbgRegisterView {

RegisterGroup::RegisterGroup(const QString &name, QWidget *parent, Qt::WindowFlags f)
	: QWidget(parent, f), name_(name) {

	setObjectName("RegisterGroup_" + name);
	{
		menuItems_.push_back(new_action_separator(this));
		menuItems_.push_back(new_action(tr("Hide %1", "register group").arg(name), this, [this]() {
			hide();
			regView()->groupHidden(this);
		}));
	}
}

void RegisterGroup::adjustWidth() {

	int widthNeeded = 0;

	Q_FOREACH (FieldWidget *field, fields()) {
		const auto widthToRequire = field->pos().x() + field->width();
		if (widthToRequire > widthNeeded)
			widthNeeded = widthToRequire;
	}

	setMinimumWidth(widthNeeded);
}

void RegisterGroup::showMenu(const QPoint &position, const QList<QAction *> &additionalItems) const {
	return regView()->showMenu(position, additionalItems + menuItems_);
}

void RegisterGroup::mousePressEvent(QMouseEvent *event) {
	if (event->button() == Qt::RightButton)
		showMenu(event->globalPos(), menuItems_);
	else
		event->ignore();
}

ODBRegView *RegisterGroup::regView() const {
	return checked_cast<ODBRegView>(parent()       // canvas
										->parent() // viewport
										->parent() // regview
	);
}

QMargins RegisterGroup::getFieldMargins() const {
	const auto charSize  = letter_size(font());
	const auto charWidth = charSize.width();
	// extra space for highlighting rectangle, so that single-digit fields are easier to target
	const auto marginLeft  = charWidth / 2;
	const auto marginRight = charWidth - marginLeft;
	return {marginLeft, 0, marginRight, 0};
}

void RegisterGroup::insert(FieldWidget *widget) {
	if (const auto value = qobject_cast<ValueField *>(widget)) {
		connect(value, &ValueField::selected, regView(), &ODBRegView::fieldSelected);
	}
}

void RegisterGroup::insert(int line, int column, FieldWidget *widget) {
	insert(widget);
	setupPositionAndSize(line, column, widget);

	widget->show();
}

void RegisterGroup::setupPositionAndSize(int line, int column, FieldWidget *widget) {
	widget->adjustToData();

	const auto margins = getFieldMargins();

	const auto charSize = letter_size(font());
	QPoint position(charSize.width() * column, charSize.height() * line);
	position -= QPoint(margins.left(), 0);

	QSize size(widget->size());
	size += QSize(margins.left() + margins.right(), 0);

	widget->setMinimumSize(size);
	widget->move(position);
	// FIXME: why are e.g. regnames like FSR truncated without the -1?
	widget->setContentsMargins({margins.left(), margins.top(), margins.right() - 1, margins.bottom()});

	const auto potentialNewWidth  = widget->pos().x() + widget->width();
	const auto potentialNewHeight = widget->pos().y() + widget->height();
	const auto oldMinSize         = minimumSize();
	if (potentialNewWidth > oldMinSize.width() || potentialNewHeight > oldMinSize.height()) {
		setMinimumSize(std::max(potentialNewWidth, oldMinSize.width()), std::max(potentialNewHeight, oldMinSize.height()));
	}
}

int RegisterGroup::lineAfterLastField() const {
	const auto fields      = this->fields();
	const auto bottomField = std::max_element(fields.begin(), fields.end(), [](FieldWidget *l, FieldWidget *r) { return l->pos().y() < r->pos().y(); });
	return bottomField == fields.end() ? 0 : (*bottomField)->pos().y() / (*bottomField)->height() + 1;
}

void RegisterGroup::appendNameValueComment(const QModelIndex &nameIndex,
										   const QString &tooltip,
										   bool insertComment) {

	assert(nameIndex.isValid());

	using namespace RegisterViewModelBase;

	const auto nameWidth = nameIndex.data(Model::FixedLengthRole).toInt();
	assert(nameWidth > 0);
	const auto valueIndex = nameIndex.sibling(nameIndex.row(), Model::VALUE_COLUMN);
	const auto valueWidth = valueIndex.data(Model::FixedLengthRole).toInt();
	assert(valueWidth > 0);

	const int line       = lineAfterLastField();
	int column           = 0;
	const auto nameField = new FieldWidget(nameWidth, nameIndex.data().toString(), this);
	insert(line, column, nameField);
	column += nameWidth + 1;
	const auto valueField = new ValueField(valueWidth, valueIndex, this);
	insert(line, column, valueField);
	if (!tooltip.isEmpty()) {
		nameField->setToolTip(tooltip);
		valueField->setToolTip(tooltip);
	}
	if (insertComment) {
		column += valueWidth + 1;
		const auto commentIndex = nameIndex.sibling(nameIndex.row(), Model::COMMENT_COLUMN);
		insert(line, column, new FieldWidget(0, commentIndex, this));
	}
}

QList<FieldWidget *> RegisterGroup::fields() const {
	const QObjectList children = this->children();
	QList<FieldWidget *> fields;
	for (QObject *child : children) {
		const auto field = qobject_cast<FieldWidget *>(child);
		if (field) {
			fields.append(field);
		}
	}
	return fields;
}

QList<ValueField *> RegisterGroup::valueFields() const {
	QList<ValueField *> allValues;

	Q_FOREACH (FieldWidget *field, fields()) {
		const auto value = qobject_cast<ValueField *>(field);
		if (value) {
			allValues.push_back(value);
		}
	}

	return allValues;
}

}
