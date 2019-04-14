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
#include "ODbgRV_Util.h"
#include "ODbgRV_Common.h"
#if defined EDB_X86 || defined EDB_X86_64
#	include "x86Groups.h"
#	include "ODbgRV_x86Common.h"
#	include "DialogEditFPU.h"
#elif defined EDB_ARM32
#	include "armGroups.h"
#endif
#include "Configuration.h"
#include "DialogEditGPR.h"
#include "DialogEditSIMDRegister.h"
#include "RegisterViewModelBase.h"
#include "State.h"
#include "edb.h"
#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <QMenu>
#include <QMouseEvent>
#include <QSettings>
#include <QShortcut>
#include <QVBoxLayout>
#include <algorithm>
#include <iostream>
#include <type_traits>

namespace ODbgRegisterView {
namespace {

// TODO: rFLAGS menu: Set Condition (O,P,NAE etc. - see ODB)
// TODO: FSR: Set Condition: G,L,E,Unordered
// TODO: Add option to show FPU in STi mode, both ST-ordered and R-ordered (physically)
// TODO: Update register comments after editing values
// TODO: Add a way to add back register group to RegView
// TODO: all TODOs scattered around sources
// TODO: "Undo" action, which returns to the state after last stopping of debuggee (visible only if register has been modified by the user)

constexpr auto registerGroupTypeNames = util::make_array<const char *>(
#if defined EDB_X86 || defined EDB_X86_64
		"GPR",
		"rIP",
		"ExpandedEFL",
		"Segment",
		"EFL",
		"FPUData",
		"FPUWords",
		"FPULastOp",
		"Debug",
		"MMX",
		"SSEData",
		"AVXData",
		"MXCSR"
#elif defined EDB_ARM32
		"GPR",
		"CPSR",
		"ExpandedCPSR",
		"FPSCR"
#else
#	error "Not implemented"
#endif
);
static_assert(registerGroupTypeNames.size() == ODBRegView::RegisterGroupType::NUM_GROUPS, "Mismatch between number of register group types and names");

const auto SETTINGS_GROUPS_ARRAY_NODE = QLatin1String("visibleGroups");

}

// ------------------------- BitFieldFormatter impl ------------------------------

BitFieldFormatter::BitFieldFormatter(const BitFieldDescription &bfd) : valueNames(bfd.valueNames) {
}

QString BitFieldFormatter::operator()(const QString &str) {
	assert(str.length());
	if (str.isEmpty())
		return str; // for release builds have defined behavior
	if (str[0] == '?')
		return "????";
	bool      parseOK = false;
	const int value   = str.toInt(&parseOK);
	if (!parseOK)
		return "????";
	assert(0 <= value);
	assert(std::size_t(value) < valueNames.size());
	return valueNames[value];
}

// ----------------------- BitFieldDescription impl -------------------------

BitFieldDescription::BitFieldDescription(int textWidth,
                                         const std::vector<QString> &valueNames,
                                         const std::vector<QString> &setValueTexts,
                                         const std::function<bool(unsigned, unsigned)> &valueEqualComparator)
    : textWidth(textWidth),
	  valueNames(valueNames),
	  setValueTexts(setValueTexts),
	  valueEqualComparator(valueEqualComparator)
{
}

VolatileNameField::VolatileNameField(int fieldWidth, const std::function<QString()> &valueFormatter, QWidget *parent, Qt::WindowFlags f) : FieldWidget(fieldWidth, "", parent, f), valueFormatter(valueFormatter) {
}

QString VolatileNameField::text() const {
	return valueFormatter();
}

// -------------------------------- MultiBitFieldWidget impl ---------------------------
MultiBitFieldWidget::MultiBitFieldWidget(const QModelIndex &index, const BitFieldDescription &bfd, QWidget *parent, Qt::WindowFlags f) : ValueField(bfd.textWidth, index, BitFieldFormatter(bfd), parent, f), equal(bfd.valueEqualComparator)
{
	const auto mapper = new QSignalMapper(this);
	connect(mapper, SIGNAL(mapped(int)), this, SLOT(setValue(int)));
	menuItems.push_front(newActionSeparator(this));
	for (std::size_t i = bfd.valueNames.size(); i-- > 0;) {
		const auto &text = bfd.setValueTexts[i];
		if (!text.isEmpty()) {
			menuItems.push_front(newAction(text, this, mapper, i));
			valueActions.push_front(menuItems.front());
		} else
			valueActions.push_front(nullptr);
	}
}

void MultiBitFieldWidget::setValue(int value) {

	using namespace RegisterViewModelBase;

	// TODO: Model: make it possible to set bit field itself, without manipulating parent directly
	//              I.e. set value without knowing field offset, then setData(fieldIndex,word)
	const auto regIndex = index.parent().sibling(index.parent().row(), MODEL_VALUE_COLUMN);
	auto       byteArr  = regIndex.data(Model::RawValueRole).toByteArray();

	if (byteArr.isEmpty())
		return;

	std::uint64_t word(0);
	std::memcpy(&word, byteArr.constData(), byteArr.size());
	const auto mask   = (1ull << (VALID_VARIANT(index.data(Model::BitFieldLengthRole)).toInt() - 1)) * 2 - 1;
	const auto offset = VALID_VARIANT(index.data(Model::BitFieldOffsetRole)).toInt();
	word              = (word & ~(mask << offset)) | (std::uint64_t(value) << offset);
	std::memcpy(byteArr.data(), &word, byteArr.size());
	model()->setData(regIndex, byteArr, Model::RawValueRole);
}

void MultiBitFieldWidget::adjustToData() {
	ValueField::adjustToData();

	const auto    byteArr = index.data(RegisterViewModelBase::Model::RawValueRole).toByteArray();
	std::uint64_t word(0);
	assert(unsigned(byteArr.size()) <= sizeof(word));
	std::memcpy(&word, byteArr.constData(), byteArr.size());

	for (int value = 0; value < valueActions.size(); ++value) {
		const auto action = valueActions[value];
		if (!action)
			continue;
		if (byteArr.isEmpty() || equal(word, value))
			action->setVisible(false);
		else
			action->setVisible(true);
	}
}

// -------------------------------- RegisterGroup impl ----------------------------
RegisterGroup::RegisterGroup(const QString &name, QWidget *parent, Qt::WindowFlags f) : QWidget(parent, f), name(name) {

	setObjectName("RegisterGroup_" + name);
	{
		menuItems.push_back(newActionSeparator(this));
		menuItems.push_back(newAction(tr("Hide %1", "register group").arg(name), this, this, SLOT(hideAndReport())));
	}
}

void RegisterGroup::hideAndReport() {
	hide();
	regView()->groupHidden(this);
}

void RegisterGroup::showMenu(const QPoint &position, const QList<QAction *> &additionalItems) const {
	return regView()->showMenu(position, additionalItems + menuItems);
}

void RegisterGroup::mousePressEvent(QMouseEvent *event) {
	if (event->button() == Qt::RightButton)
		showMenu(event->globalPos(), menuItems);
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
	const auto charSize  = letterSize(font());
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

	const auto charSize = letterSize(font());
	QPoint     position(charSize.width() * column, charSize.height() * line);
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

	const int  line      = lineAfterLastField();
	int        column    = 0;
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
	const auto           children = this->children();
	QList<FieldWidget *> fields;
	for (const auto child : children) {
		const auto field = qobject_cast<FieldWidget *>(child);
		if (field)
			fields.append(field);
	}
	return fields;
}

QList<ValueField *> RegisterGroup::valueFields() const {
	QList<ValueField *> allValues;
	Q_FOREACH(const auto field, fields()) {
		const auto value = qobject_cast<ValueField *>(field);
		if (value)
			allValues.push_back(value);
	}
	return allValues;
}

// ------------------------------- Canvas impl ----------------------------------------

Canvas::Canvas(QWidget *parent, Qt::WindowFlags f) : QWidget(parent, f) {
	setObjectName("RegViewCanvas");
	const auto canvasLayout = new QVBoxLayout(this);
	canvasLayout->setSpacing(letterSize(parent->font()).height() / 2);
	canvasLayout->setContentsMargins(parent->contentsMargins());
	canvasLayout->setAlignment(Qt::AlignTop);
	setLayout(canvasLayout);
	setBackgroundRole(QPalette::Base);
	setAutoFillBackground(true);
}

void Canvas::mousePressEvent(QMouseEvent *event) {
	event->ignore();
}

// -------------------------------- ODBRegView impl ----------------------------------------

void ODBRegView::mousePressEvent(QMouseEvent *event) {
	if (event->type() != QEvent::MouseButtonPress)
		return;

	if (event->button() == Qt::RightButton) {
		showMenu(event->globalPos());
		return;
	}

	if (event->button() == Qt::LeftButton) {
		Q_FOREACH(const auto field, valueFields()) {
			field->unselect();
		}
	}
}

void ODBRegView::updateFont() {
	QFont font;
	if(!font.fromString(edb::v1::config().registers_font))
	{
		font = QFont("Monospace");
		font.setStyleHint(QFont::TypeWriter);
	}
	setFont(font);
}

void ODBRegView::fieldSelected() {
	Q_FOREACH(const auto field, valueFields())
		if (sender() != field)
			field->unselect();
	ensureWidgetVisible(static_cast<QWidget *>(sender()), 0, 0);
}

void ODBRegView::showMenu(const QPoint &position, const QList<QAction *> &additionalItems) const {
	QMenu menu;
	auto  items = additionalItems + menuItems;

	if (model_->activeIndex().isValid()) {
		QList<QAction *> debuggerActions;
		QMetaObject::invokeMethod(edb::v1::debugger_ui, "getCurrentRegisterContextMenuItems", Qt::DirectConnection, Q_RETURN_ARG(QList<QAction *>, debuggerActions));
		items.push_back(nullptr);
		items.append(debuggerActions);
	}

	for (const auto action : items)
		if (action)
			menu.addAction(action);
		else
			menu.addSeparator();

	menu.exec(position);
}

void RegisterGroup::adjustWidth() {

	int widthNeeded = 0;

	Q_FOREACH(const auto field, fields()) {
		const auto widthToRequire = field->pos().x() + field->width();
		if (widthToRequire > widthNeeded)
			widthNeeded = widthToRequire;
	}

	setMinimumWidth(widthNeeded);
}

ODBRegView::RegisterGroupType findGroup(const QString &str) {
	const auto &names   = registerGroupTypeNames;
	const auto  foundIt = std::find(names.begin(), names.end(), str);

	if (foundIt == names.end())
		return ODBRegView::RegisterGroupType::NUM_GROUPS;

	return ODBRegView::RegisterGroupType(foundIt - names.begin());
}

void ODBRegView::settingsUpdated() {
	// this slot is now triggered whenever the settings dialog is closed,
	// so it's a good spot to update the fonts and anything else which
	// may be affected by user config
	updateFont();
	modelReset();
}

ODBRegView::ODBRegView(const QString &settingsGroup, QWidget *parent)
	: QScrollArea(parent),
	  dialogEditGPR(new DialogEditGPR(this)),
	  dialogEditSIMDReg(new DialogEditSIMDRegister(this)),
#if defined EDB_X86 || defined EDB_X86_64
	  dialogEditFPU(new DialogEditFPU(this))
#else
	  dialogEditFPU(nullptr)
#endif
{
	setObjectName("ODBRegView");

	connect(&edb::v1::config(), &Configuration::settingsUpdated, this, &ODBRegView::settingsUpdated);

	updateFont();

	const auto canvas = new Canvas(this);
	setWidget(canvas);
	setWidgetResizable(true);

	{
		const auto sep = new QAction(this);
		sep->setSeparator(true);
		menuItems.push_back(sep);
		menuItems.push_back(newAction(tr("Copy all registers"), this, this, SLOT(copyAllRegisters())));
	}

	QSettings settings;
	settings.beginGroup(settingsGroup);
	const auto groupListV = settings.value(SETTINGS_GROUPS_ARRAY_NODE);
	if (settings.group().isEmpty() || !groupListV.isValid()) {
		visibleGroupTypes = {
#if defined EDB_X86 || defined EDB_X86_64
			RegisterGroupType::GPR,
			RegisterGroupType::rIP,
			RegisterGroupType::ExpandedEFL,
			RegisterGroupType::Segment,
			RegisterGroupType::EFL,
			RegisterGroupType::FPUData,
			RegisterGroupType::FPUWords,
			RegisterGroupType::FPULastOp,
			RegisterGroupType::Debug,
			RegisterGroupType::MMX,
			RegisterGroupType::SSEData,
			RegisterGroupType::AVXData,
			RegisterGroupType::MXCSR,
#elif defined EDB_ARM32
			RegisterGroupType::GPR,
			RegisterGroupType::CPSR,
			RegisterGroupType::ExpandedCPSR,
			RegisterGroupType::FPSCR,
#else
#	error "Not implemented"
#endif
		};
	} else {
		Q_FOREACH(const auto &grp, groupListV.toStringList()) {
			const auto group = findGroup(grp);
			if (group >= RegisterGroupType::NUM_GROUPS) {
				qWarning() << qPrintable(QString("Warning: failed to understand group %1").arg(group));
				continue;
			}
			visibleGroupTypes.emplace_back(group);
		}
	}

	connect(new QShortcut(copyFieldShortcut, this, 0, 0, Qt::WidgetShortcut), &QShortcut::activated, this, &ODBRegView::copyRegisterToClipboard);
}

void ODBRegView::copyRegisterToClipboard() const {
	const auto selected = selectedField();
	if (selected)
		selected->copyToClipboard();
}

DialogEditGPR *ODBRegView::gprEditDialog() const {
	return dialogEditGPR;
}

DialogEditSIMDRegister *ODBRegView::simdEditDialog() const {
	return dialogEditSIMDReg;
}

DialogEditFPU *ODBRegView::fpuEditDialog() const {
	return dialogEditFPU;
}

void ODBRegView::copyAllRegisters() {
	auto allFields = fields();
	std::sort(allFields.begin(), allFields.end(), [](const FieldWidget *f1, const FieldWidget *f2) {
		const auto f1Pos = fieldPos(f1);
		const auto f2Pos = fieldPos(f2);
		if (f1Pos.y() < f2Pos.y())
			return true;
		if (f1Pos.y() > f2Pos.y())
			return false;
		return f1Pos.x() < f2Pos.x();
	});

	QString text;
	int     textLine   = 0;
	int     textColumn = 0;

	for (const auto field : allFields) {
		while (field->lineNumber() > textLine) {
			++textLine;
			textColumn = 0;
			text       = text.trimmed() + '\n';
		}
		while (field->columnNumber() > textColumn) {
			++textColumn;
			text += ' ';
		}
		const QString fieldText = field->text();
		if (field->alignment() == Qt::AlignRight) {
			const int fwidth     = field->fieldWidth();
			const int spaceWidth = fwidth - fieldText.length();
			text += QString(spaceWidth, ' ');
			textColumn += spaceWidth;
		}
		text += fieldText;
		textColumn += fieldText.length();
	}

	QApplication::clipboard()->setText(text.trimmed());
}

void ODBRegView::groupHidden(RegisterGroup *group) {
	using namespace std;
	assert(util::contains(groups, group));
	const auto groupPtrIter = std::find(groups.begin(), groups.end(), group);
	auto &     groupPtr     = *groupPtrIter;
	groupPtr->deleteLater();
	groupPtr = nullptr;

	auto &    types(visibleGroupTypes);
	const int groupType = groupPtrIter - groups.begin();
	types.erase(remove_if(types.begin(), types.end(), [=](int type) { return type == groupType; }), types.end());
}

void ODBRegView::saveState(const QString &settingsGroup) const {
	QSettings settings;
	settings.beginGroup(settingsGroup);
	settings.remove(SETTINGS_GROUPS_ARRAY_NODE);
	QStringList groupTypes;
	for (auto type : visibleGroupTypes)
		groupTypes << registerGroupTypeNames[type];
	settings.setValue(SETTINGS_GROUPS_ARRAY_NODE, groupTypes);
}

void ODBRegView::setModel(RegisterViewModelBase::Model *model) {
	model_ = model;
	connect(model, &RegisterViewModelBase::Model::modelReset,  this, &ODBRegView::modelReset);
	connect(model, &RegisterViewModelBase::Model::dataChanged, this, &ODBRegView::modelUpdated);
	modelReset();
}

RegisterGroup *createSIMDGroup(RegisterViewModelBase::Model *model, QWidget *parent, const QString &catName, const QString &regNamePrefix) {
	const auto catIndex = findModelCategory(model, catName);
	if (!catIndex.isValid())
		return nullptr;
	const auto group = new RegisterGroup(catName, parent);
	for (int row = 0; row < model->rowCount(catIndex); ++row) {
		const auto nameIndex = VALID_INDEX(model->index(row, MODEL_NAME_COLUMN, catIndex));
		const auto name      = regNamePrefix + QString("%1").arg(row);
		if (!VALID_VARIANT(nameIndex.data()).toString().toUpper().startsWith(regNamePrefix)) {
			if (row == 0)
				return nullptr; // don't want empty groups
			break;
		}
		group->insert(row, 0, new FieldWidget(name, group));
		new SIMDValueManager(row, nameIndex, group);
	}
	// This signal must be handled by group _after_ all `SIMDValueManager`s handle their connection to this signal
	QObject::connect(model, SIGNAL(SIMDDisplayFormatChanged()), group, SLOT(adjustWidth()), Qt::QueuedConnection);
	return group;
}

RegisterGroup *ODBRegView::makeGroup(RegisterGroupType type) {

	if (!model_->rowCount())
		return nullptr;

	std::vector<QModelIndex> nameValCommentIndices;

	using RegisterViewModelBase::Model;
	QString groupName;

	switch (type) {
	case RegisterGroupType::GPR: {
		groupName           = tr("GPRs");
		const auto catIndex = findModelCategory(model_, GPRCategoryName);
		if (!catIndex.isValid())
			break;
		for (int row = 0; row < model_->rowCount(catIndex); ++row)
			nameValCommentIndices.emplace_back(model_->index(row, MODEL_NAME_COLUMN, catIndex));
		break;
	}
#if defined EDB_X86 || defined EDB_X86_64
	case RegisterGroupType::EFL:
		return createEFL(model_, widget());
	case RegisterGroupType::ExpandedEFL:
		return createExpandedEFL(model_, widget());
	case RegisterGroupType::FPUData:
		return createFPUData(model_, widget());
	case RegisterGroupType::FPUWords:
		return createFPUWords(model_, widget());
	case RegisterGroupType::FPULastOp:
		return createFPULastOp(model_, widget());
	case RegisterGroupType::Debug:
		return createDebugGroup(model_, widget());
	case RegisterGroupType::MXCSR:
		return createMXCSR(model_, widget());
	case RegisterGroupType::MMX:
		return createSIMDGroup(model_, widget(), "MMX", "MM");
	case RegisterGroupType::SSEData:
		return createSIMDGroup(model_, widget(), "SSE", "XMM");
	case RegisterGroupType::AVXData:
		return createSIMDGroup(model_, widget(), "AVX", "YMM");
	case RegisterGroupType::Segment: {
		groupName           = tr("Segment Registers");
		const auto catIndex = findModelCategory(model_, "Segment");
		if (!catIndex.isValid())
			break;
		for (int row = 0; row < model_->rowCount(catIndex); ++row)
			nameValCommentIndices.emplace_back(model_->index(row, MODEL_NAME_COLUMN, catIndex));
		break;
	}
	case RegisterGroupType::rIP: {
		groupName           = tr("Instruction Pointer");
		const auto catIndex = findModelCategory(model_, "General Status");
		if (!catIndex.isValid())
			break;
		nameValCommentIndices.emplace_back(findModelRegister(catIndex, "RIP"));
		nameValCommentIndices.emplace_back(findModelRegister(catIndex, "EIP"));
		break;
	}
#elif defined EDB_ARM32
	case RegisterGroupType::CPSR:
		return createCPSR(model_, widget());
	case RegisterGroupType::ExpandedCPSR:
		return createExpandedCPSR(model_, widget());
	case RegisterGroupType::FPSCR:
		return createFPSCR(model_, widget());
#endif
	default:
		qWarning() << "Warning: unexpected register group type requested in" << Q_FUNC_INFO;
		return nullptr;
	}

	nameValCommentIndices.erase(std::remove_if(nameValCommentIndices.begin(), nameValCommentIndices.end(), [](const QModelIndex &index) { return !index.isValid(); }), nameValCommentIndices.end());

	if (nameValCommentIndices.empty()) {
		qWarning() << "Warning: failed to get any useful register indices for regGroupType" << static_cast<long>(type);
		return nullptr;
	}

	const auto group = new RegisterGroup(groupName, widget());
	for (const auto &index : nameValCommentIndices) {
		group->appendNameValueComment(index);
	}

	return group;
}

void ODBRegView::modelReset() {

	widget()->hide(); // prevent flicker while groups are added to/removed from the layout

	// not all groups may be in the layout, so delete them individually
	Q_FOREACH(const auto group, groups) {
		if (group) {
			group->deleteLater();
		}
	}

	groups.clear();

	const auto layout = static_cast<QVBoxLayout *>(widget()->layout());

	// layout contains not only groups, so delete all items too
	while (const auto item = layout->takeAt(0)) {
		delete item;
	}

	const auto flagsAndSegments = new QHBoxLayout();
	// (3/2+1/2)-letter — Total of 2-letter spacing. Fourth half-letter is from flag values extension.
	// Segment extensions at LHS of the widget don't influence minimumSize request, so no need to take
	// them into account.
	flagsAndSegments->setSpacing(letterSize(this->font()).width() * 3 / 2);
	flagsAndSegments->setContentsMargins(QMargins());
	flagsAndSegments->setAlignment(Qt::AlignLeft);

	bool flagsAndSegsInserted = false;

	for (int groupType_ = 0; groupType_ < RegisterGroupType::NUM_GROUPS; ++groupType_) {
		const auto groupType = RegisterGroupType{groupType_};
		if (util::contains(visibleGroupTypes, groupType)) {
			const auto group = makeGroup(groupType);
			groups.push_back(group);
			if (!group)
				continue;
#if defined EDB_X86 || defined EDB_X86_64
			if (groupType == RegisterGroupType::Segment || groupType == RegisterGroupType::ExpandedEFL) {
				flagsAndSegments->addWidget(group);
				if (!flagsAndSegsInserted) {
					layout->addLayout(flagsAndSegments);
					flagsAndSegsInserted = true;
				}
			} else
#endif
				layout->addWidget(group);
		} else
			groups.push_back(nullptr);
	}

	widget()->show();
}

void ODBRegView::modelUpdated() {
	Q_FOREACH(const auto field, fields()) {
		field->adjustToData();
	}

	Q_FOREACH(const auto group, groups) {
		if (group) {
			group->adjustWidth();
		}
	}
}

QList<FieldWidget *> ODBRegView::fields() const {

	QList<FieldWidget *> allFields;
	for (const auto group : groups) {
		if (group) {
			allFields.append(group->fields());
		}
	}

	return allFields;
}

QList<ValueField *> ODBRegView::valueFields() const {

	QList<ValueField *> allValues;
	for (const auto group : groups) {
		if (group) {
			allValues.append(group->valueFields());
		}
	}

	return allValues;
}

void ODBRegView::updateFieldsPalette() {
	Q_FOREACH(const auto field, valueFields())
		field->updatePalette();
}

ValueField *ODBRegView::selectedField() const {
	Q_FOREACH(const auto field, valueFields()) {
		if (field->isSelected()) {
			return field;
		}
	}

	return nullptr;
}

SIMDValueManager::SIMDValueManager(int lineInGroup,
                                   const QModelIndex &nameIndex,
								   RegisterGroup *parent)
	: QObject(parent),
	  regIndex(nameIndex),
	  lineInGroup(lineInGroup),
	  intMode(NumberDisplayMode::Hex)
{
	setupMenu();

	assert(nameIndex.isValid());
	connect(nameIndex.model(), SIGNAL(SIMDDisplayFormatChanged()), this, SLOT(displayFormatChanged()));
	displayFormatChanged();
}

void SIMDValueManager::fillGroupMenu() {
	const auto group = this->group();
	group->menuItems.push_back(newActionSeparator(this));
	group->menuItems.push_back(menuItems[VIEW_AS_BYTES]);
	group->menuItems.push_back(menuItems[VIEW_AS_WORDS]);
	group->menuItems.push_back(menuItems[VIEW_AS_DWORDS]);
	group->menuItems.push_back(menuItems[VIEW_AS_QWORDS]);
	group->menuItems.push_back(newActionSeparator(this));
	group->menuItems.push_back(menuItems[VIEW_AS_FLOAT32]);
	group->menuItems.push_back(menuItems[VIEW_AS_FLOAT64]);
	group->menuItems.push_back(newActionSeparator(this));
	group->menuItems.push_back(menuItems[VIEW_INT_AS_HEX]);
	group->menuItems.push_back(menuItems[VIEW_INT_AS_SIGNED]);
	group->menuItems.push_back(menuItems[VIEW_INT_AS_UNSIGNED]);
}

auto SIMDValueManager::model() const -> Model * {
	const auto model = static_cast<const Model *>(regIndex.model());
	// The model is not supposed to have been created as const object,
	// and our manipulations won't invalidate the index.
	// Thus cast the const away.
	return const_cast<Model *>(model);
}

void SIMDValueManager::showAsInt(int size_) {
	const auto size = static_cast<Model::ElementSize>(size_);
	model()->setChosenSIMDSize(regIndex.parent(), size);
	model()->setChosenSIMDFormat(regIndex.parent(), intMode);
}

void SIMDValueManager::showAsFloat(int size) {
	model()->setChosenSIMDFormat(regIndex.parent(), NumberDisplayMode::Float);
	switch (size) {
	case sizeof(edb::value32):
		model()->setChosenSIMDSize(regIndex.parent(), Model::ElementSize::DWORD);
		break;
	case sizeof(edb::value64):
		model()->setChosenSIMDSize(regIndex.parent(), Model::ElementSize::QWORD);
		break;
	default:
		EDB_PRINT_AND_DIE("Unexpected size: ", size);
	}
}

void SIMDValueManager::setIntFormat(int format_) {
	const auto format = static_cast<NumberDisplayMode>(format_);
	model()->setChosenSIMDFormat(regIndex.parent(), format);
}

void SIMDValueManager::setupMenu() {
	const auto group        = this->group();
	const auto validFormats = VALID_VARIANT(regIndex.parent().data(Model::ValidSIMDFormatsRole)).value<std::vector<NumberDisplayMode>>();
	// Setup menu if we're the first value field creator
	if (group->valueFields().isEmpty()) {
		const auto intSizeMapper = new QSignalMapper(this);
		connect(intSizeMapper, SIGNAL(mapped(int)), this, SLOT(showAsInt(int)));
		menuItems.push_back(newAction(tr("View %1 as bytes").arg(group->name), group, intSizeMapper, Model::ElementSize::BYTE));
		menuItems.push_back(newAction(tr("View %1 as words").arg(group->name), group, intSizeMapper, Model::ElementSize::WORD));
		menuItems.push_back(newAction(tr("View %1 as doublewords").arg(group->name), group, intSizeMapper, Model::ElementSize::DWORD));
		menuItems.push_back(newAction(tr("View %1 as quadwords").arg(group->name), group, intSizeMapper, Model::ElementSize::QWORD));

		if (util::contains(validFormats, NumberDisplayMode::Float)) {
			const auto floatMapper = new QSignalMapper(this);
			connect(floatMapper, SIGNAL(mapped(int)), this, SLOT(showAsFloat(int)));
			menuItems.push_back(newAction(tr("View %1 as 32-bit floats").arg(group->name), group, floatMapper, Model::ElementSize::DWORD));
			menuItems.push_back(newAction(tr("View %1 as 64-bit floats").arg(group->name), group, floatMapper, Model::ElementSize::QWORD));
		} else {
			// create empty elements to leave further items with correct indices
			menuItems.push_back(newActionSeparator(this));
			menuItems.push_back(newActionSeparator(this));
		}

		const auto intMapper = new QSignalMapper(this);
		connect(intMapper, SIGNAL(mapped(int)), this, SLOT(setIntFormat(int)));
		menuItems.push_back(newAction(tr("View %1 integers as hex").arg(group->name), group, intMapper, static_cast<int>(NumberDisplayMode::Hex)));
		menuItems.push_back(newAction(tr("View %1 integers as signed").arg(group->name), group, intMapper, static_cast<int>(NumberDisplayMode::Signed)));
		menuItems.push_back(newAction(tr("View %1 integers as unsigned").arg(group->name), group, intMapper, static_cast<int>(NumberDisplayMode::Unsigned)));

		fillGroupMenu();
	}
}

void SIMDValueManager::updateMenu() {
	if (menuItems.isEmpty())
		return;
	Q_FOREACH(auto item, menuItems)
		item->setVisible(true);

	using RegisterViewModelBase::Model;
	switch (currentSize()) {
	case Model::ElementSize::BYTE:
		menuItems[VIEW_AS_BYTES]->setVisible(false);
		break;
	case Model::ElementSize::WORD:
		menuItems[VIEW_AS_WORDS]->setVisible(false);
		break;
	case Model::ElementSize::DWORD:
		if (currentFormat() != NumberDisplayMode::Float)
			menuItems[VIEW_AS_DWORDS]->setVisible(false);
		else
			menuItems[VIEW_AS_FLOAT32]->setVisible(false);
		break;
	case Model::ElementSize::QWORD:
		if (currentFormat() != NumberDisplayMode::Float)
			menuItems[VIEW_AS_QWORDS]->setVisible(false);
		else
			menuItems[VIEW_AS_FLOAT64]->setVisible(false);
		break;
	default:
		EDB_PRINT_AND_DIE("Unexpected current size: ", currentSize());
	}
	switch (currentFormat()) {
	case NumberDisplayMode::Float:
		menuItems[VIEW_INT_AS_HEX]->setVisible(false);
		menuItems[VIEW_INT_AS_SIGNED]->setVisible(false);
		menuItems[VIEW_INT_AS_UNSIGNED]->setVisible(false);
		break;
	case NumberDisplayMode::Hex:
		menuItems[VIEW_INT_AS_HEX]->setVisible(false);
		break;
	case NumberDisplayMode::Signed:
		menuItems[VIEW_INT_AS_SIGNED]->setVisible(false);
		break;
	case NumberDisplayMode::Unsigned:
		menuItems[VIEW_INT_AS_UNSIGNED]->setVisible(false);
		break;
	}
}

RegisterGroup *SIMDValueManager::group() const {
	return checked_cast<RegisterGroup>(parent());
}

void SIMDValueManager::displayFormatChanged() {
	const auto newFormat = currentFormat();

	if (newFormat != NumberDisplayMode::Float) {
		intMode = newFormat;
	}

	Q_FOREACH(const auto elem, elements) {
		elem->deleteLater();
	}

	elements.clear();

	using RegisterViewModelBase::Model;
	const auto model = regIndex.model();

	const int   sizeRow   = VALID_VARIANT(regIndex.parent().data(Model::ChosenSIMDSizeRowRole)).toInt();
	QModelIndex sizeIndex = model->index(sizeRow, MODEL_NAME_COLUMN, regIndex);
	const auto  elemCount = model->rowCount(sizeIndex);

	const auto regNameWidth = VALID_VARIANT(regIndex.data(Model::FixedLengthRole)).toInt();
	int        column       = regNameWidth + 1;
	const auto elemWidth    = VALID_VARIANT(model->index(0, MODEL_VALUE_COLUMN, sizeIndex).data(Model::FixedLengthRole)).toInt();
	for (int elemN = elemCount - 1; elemN >= 0; --elemN) {
		const auto elemIndex = model->index(elemN, MODEL_VALUE_COLUMN, sizeIndex);
		const auto field     = new ValueField(elemWidth, elemIndex, group());
		elements.push_back(field);
		field->setAlignment(Qt::AlignRight);
		group()->insert(lineInGroup, column, field);
		column += elemWidth + 1;
	}

	updateMenu();
}

RegisterViewModelBase::Model::ElementSize SIMDValueManager::currentSize() const {
	using RegisterViewModelBase::Model;
	const int size = VALID_VARIANT(regIndex.parent().data(Model::ChosenSIMDSizeRole)).toInt();
	return static_cast<Model::ElementSize>(size);
}

NumberDisplayMode SIMDValueManager::currentFormat() const {
	using RegisterViewModelBase::Model;
	const int size = VALID_VARIANT(regIndex.parent().data(Model::ChosenSIMDFormatRole)).toInt();
	return static_cast<NumberDisplayMode>(size);
}

void ODBRegView::selectAField() {
	const auto fields = valueFields();
	if (!fields.isEmpty()) {
		fields.front()->select();
	}
}

void ODBRegView::keyPressEvent(QKeyEvent *event) {

	const auto selected = selectedField();

	switch (event->key()) {
	case Qt::Key_Up:
		if (selected && selected->up()) {
			selected->up()->select();
			return;
		}
		if (!selected)
			selectAField();
		break;
	case Qt::Key_Down:
		if (selected && selected->down()) {
			selected->down()->select();
			return;
		}
		if (!selected)
			selectAField();
		break;
	case Qt::Key_Left:
		if (selected && selected->left()) {
			selected->left()->select();
			return;
		}
		if (!selected)
			selectAField();
		break;
	case Qt::Key_Right:
		if (selected && selected->right()) {
			selected->right()->select();
			return;
		}
		if (!selected)
			selectAField();
		break;
	case Qt::Key_Enter:
	case Qt::Key_Return:
		if (selected) {
			selected->defaultAction();
			return;
		}
		break;
	case Qt::Key_Menu:
		if (selected)
			selected->showMenu(selected->mapToGlobal(selected->rect().bottomLeft()));
		else
			showMenu(mapToGlobal(QPoint()));
		break;
	case setToZeroKey:
		if (selected) {
			selected->setZero();
			return;
		}
		break;
	case incrementKey:
		if (selected) {
			selected->increment();
			return;
		}
		break;
	case decrementKey:
		if (selected) {
			selected->decrement();
			return;
		}
		break;
	}
	QScrollArea::keyPressEvent(event);
}

}
