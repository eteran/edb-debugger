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
#include "BitFieldFormatter.h"
#include "Canvas.h"
#include "Configuration.h"
#include "DialogEditGPR.h"
#include "DialogEditSimdRegister.h"
#include "ODbgRV_Common.h"
#include "ODbgRV_Util.h"
#include "RegisterGroup.h"
#include "RegisterViewModelBase.h"
#include "SimdValueManager.h"
#include "State.h"
#include "ValueField.h"
#include "VolatileNameField.h"
#include "edb.h"
#include "util/Container.h"

#if defined(EDB_X86) || defined(EDB_X86_64)
#include "DialogEditFPU.h"
#include "ODbgRV_x86Common.h"
#include "x86Groups.h"
#elif defined(EDB_ARM32)
#include "armGroups.h"
#endif

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

constexpr auto RegisterGroupTypeNames = util::make_array<const char *>(
#if defined(EDB_X86) || defined(EDB_X86_64)
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
#elif defined(EDB_ARM32)
	"GPR",
	"CPSR",
	"ExpandedCPSR",
	"FPSCR"
#else
#error "Not implemented"
#endif
);

static_assert(RegisterGroupTypeNames.size() == ODBRegView::RegisterGroupType::NUM_GROUPS, "Mismatch between number of register group types and names");

const auto SETTINGS_GROUPS_ARRAY_NODE = QLatin1String("visibleGroups");

ODBRegView::RegisterGroupType findGroup(const QString &str) {
	const auto &names  = RegisterGroupTypeNames;
	const auto foundIt = std::find(names.begin(), names.end(), str);

	if (foundIt == names.end())
		return ODBRegView::RegisterGroupType::NUM_GROUPS;

	return ODBRegView::RegisterGroupType(foundIt - names.begin());
}

RegisterGroup *createSIMDGroup(RegisterViewModelBase::Model *model, QWidget *parent, const QString &catName, const QString &regNamePrefix) {
	const auto catIndex = find_model_category(model, catName);
	if (!catIndex.isValid())
		return nullptr;
	const auto group = new RegisterGroup(catName, parent);
	for (int row = 0; row < model->rowCount(catIndex); ++row) {
		const auto nameIndex = valid_index(model->index(row, ModelNameColumn, catIndex));
		const auto name      = regNamePrefix + QString("%1").arg(row);
		if (!valid_variant(nameIndex.data()).toString().toUpper().startsWith(regNamePrefix)) {
			if (row == 0)
				return nullptr; // don't want empty groups
			break;
		}

		group->insert(row, 0, new FieldWidget(name, group));
		new SimdValueManager(row, nameIndex, group);
	}
	// This signal must be handled by group _after_ all `SimdValueManager`s handle their connection to this signal
	QObject::connect(
		model, &RegisterViewModelBase::Model::SIMDDisplayFormatChanged, group, [group]() {
			group->adjustWidth();
		},
		Qt::QueuedConnection);

	return group;
}

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
		Q_FOREACH (const auto field, valueFields()) {
			field->unselect();
		}
	}
}

void ODBRegView::updateFont() {
	QFont font;
	if (!font.fromString(edb::v1::config().registers_font)) {
		font = QFont("Monospace");
		font.setStyleHint(QFont::TypeWriter);
	}
	setFont(font);
}

void ODBRegView::fieldSelected() {
	Q_FOREACH (const auto field, valueFields())
		if (sender() != field)
			field->unselect();
	ensureWidgetVisible(static_cast<QWidget *>(sender()), 0, 0);
}

void ODBRegView::showMenu(const QPoint &position, const QList<QAction *> &additionalItems) const {
	QMenu menu;
	auto items = additionalItems + menuItems_;

	if (model_->activeIndex().isValid()) {
		QList<QAction *> debuggerActions;
		QMetaObject::invokeMethod(edb::v1::debugger_ui, "currentRegisterContextMenuItems", Qt::DirectConnection, Q_RETURN_ARG(QList<QAction *>, debuggerActions));
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

void ODBRegView::settingsUpdated() {
	// this slot is now triggered whenever the settings dialog is closed,
	// so it's a good spot to update the fonts and anything else which
	// may be affected by user config
	updateFont();
	modelReset();
}

ODBRegView::ODBRegView(const QString &settingsGroup, QWidget *parent)
	: QScrollArea(parent),
	  dialogEditGpr_(new DialogEditGPR(this)),
	  dialogEditSIMDReg_(new DialogEditSimdRegister(this)),
#if defined(EDB_X86) || defined(EDB_X86_64)
	  dialogEditFpu_(new DialogEditFPU(this))
#else
	  dialogEditFpu_(nullptr)
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
		menuItems_.push_back(sep);
		menuItems_.push_back(new_action(tr("Copy all registers"), this, [this](bool) {
			copyAllRegisters();
		}));
	}

	QSettings settings;
	settings.beginGroup(settingsGroup);
	const auto groupListV = settings.value(SETTINGS_GROUPS_ARRAY_NODE);
	if (settings.group().isEmpty() || !groupListV.isValid()) {
		visibleGroupTypes_ = {
#if defined(EDB_X86) || defined(EDB_X86_64)
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
#elif defined(EDB_ARM32)
			RegisterGroupType::GPR,
			RegisterGroupType::CPSR,
			RegisterGroupType::ExpandedCPSR,
			RegisterGroupType::FPSCR,
#else
#error "Not implemented"
#endif
		};
	} else {
		Q_FOREACH (const auto &grp, groupListV.toStringList()) {
			const auto group = findGroup(grp);
			if (group >= RegisterGroupType::NUM_GROUPS) {
				qWarning() << qPrintable(QString("Warning: failed to understand group %1").arg(group));
				continue;
			}
			visibleGroupTypes_.emplace_back(group);
		}
	}

	connect(new QShortcut(QKeySequence::Copy, this, nullptr, nullptr, Qt::WidgetShortcut), &QShortcut::activated, this, &ODBRegView::copyRegisterToClipboard);
}

void ODBRegView::copyRegisterToClipboard() const {
	const auto selected = selectedField();
	if (selected)
		selected->copyToClipboard();
}

DialogEditGPR *ODBRegView::gprEditDialog() const {
	return dialogEditGpr_;
}

DialogEditSimdRegister *ODBRegView::simdEditDialog() const {
	return dialogEditSIMDReg_;
}

DialogEditFPU *ODBRegView::fpuEditDialog() const {
	return dialogEditFpu_;
}

void ODBRegView::copyAllRegisters() {
	auto allFields = fields();
	std::sort(allFields.begin(), allFields.end(), [](const FieldWidget *f1, const FieldWidget *f2) {
		const auto f1Pos = field_position(f1);
		const auto f2Pos = field_position(f2);
		if (f1Pos.y() < f2Pos.y())
			return true;
		if (f1Pos.y() > f2Pos.y())
			return false;
		return f1Pos.x() < f2Pos.x();
	});

	QString text;
	int textLine   = 0;
	int textColumn = 0;

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
	assert(util::contains(groups_, group));
	const auto groupPtrIter = std::find(groups_.begin(), groups_.end(), group);
	auto &groupPtr          = *groupPtrIter;
	groupPtr->deleteLater();
	groupPtr = nullptr;

	auto &types(visibleGroupTypes_);
	const int groupType = groupPtrIter - groups_.begin();
	types.erase(remove_if(types.begin(), types.end(), [=](int type) { return type == groupType; }), types.end());
}

void ODBRegView::saveState(const QString &settingsGroup) const {
	QSettings settings;
	settings.beginGroup(settingsGroup);
	settings.remove(SETTINGS_GROUPS_ARRAY_NODE);
	QStringList groupTypes;
	for (auto type : visibleGroupTypes_)
		groupTypes << RegisterGroupTypeNames[type];
	settings.setValue(SETTINGS_GROUPS_ARRAY_NODE, groupTypes);
}

void ODBRegView::setModel(RegisterViewModelBase::Model *model) {
	model_ = model;
	connect(model, &RegisterViewModelBase::Model::modelReset, this, &ODBRegView::modelReset);
	connect(model, &RegisterViewModelBase::Model::dataChanged, this, &ODBRegView::modelUpdated);
	modelReset();
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
		const auto catIndex = find_model_category(model_, GprCategoryName);
		if (!catIndex.isValid())
			break;
		for (int row = 0; row < model_->rowCount(catIndex); ++row)
			nameValCommentIndices.emplace_back(model_->index(row, ModelNameColumn, catIndex));
		break;
	}
#if defined(EDB_X86) || defined(EDB_X86_64)
	case RegisterGroupType::EFL:
		return create_eflags(model_, widget());
	case RegisterGroupType::ExpandedEFL:
		return create_expanded_eflags(model_, widget());
	case RegisterGroupType::FPUData:
		return create_fpu_data(model_, widget());
	case RegisterGroupType::FPUWords:
		return create_fpu_words(model_, widget());
	case RegisterGroupType::FPULastOp:
		return create_fpu_last_op(model_, widget());
	case RegisterGroupType::Debug:
		return create_debug_group(model_, widget());
	case RegisterGroupType::MXCSR:
		return create_mxcsr(model_, widget());
	case RegisterGroupType::MMX:
		return createSIMDGroup(model_, widget(), "MMX", "MM");
	case RegisterGroupType::SSEData:
		return createSIMDGroup(model_, widget(), "SSE", "XMM");
	case RegisterGroupType::AVXData:
		return createSIMDGroup(model_, widget(), "AVX", "YMM");
	case RegisterGroupType::Segment: {
		groupName           = tr("Segment Registers");
		const auto catIndex = find_model_category(model_, "Segment");
		if (!catIndex.isValid())
			break;
		for (int row = 0; row < model_->rowCount(catIndex); ++row)
			nameValCommentIndices.emplace_back(model_->index(row, ModelNameColumn, catIndex));
		break;
	}
	case RegisterGroupType::rIP: {
		groupName           = tr("Instruction Pointer");
		const auto catIndex = find_model_category(model_, "General Status");
		if (!catIndex.isValid())
			break;
		nameValCommentIndices.emplace_back(find_model_register(catIndex, "RIP"));
		nameValCommentIndices.emplace_back(find_model_register(catIndex, "EIP"));
		break;
	}
#elif defined(EDB_ARM32)
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
	Q_FOREACH (const auto group, groups_) {
		if (group) {
			group->deleteLater();
		}
	}

	groups_.clear();

	const auto layout = static_cast<QVBoxLayout *>(widget()->layout());

	flagsAndSegments_ = std::make_unique<QHBoxLayout>();

	// (3/2+1/2)-letter - Total of 2-letter spacing. Fourth half-letter is from flag values extension.
	// Segment extensions at LHS of the widget don't influence minimumSize request, so no need to take
	// them into account.
	flagsAndSegments_->setSpacing(letter_size(this->font()).width() * 3 / 2);
	flagsAndSegments_->setContentsMargins(QMargins());
	flagsAndSegments_->setAlignment(Qt::AlignLeft);

	bool flagsAndSegsInserted = false;

	for (int group = 0; group < RegisterGroupType::NUM_GROUPS; ++group) {

		const auto groupType = static_cast<RegisterGroupType>(group);
		if (util::contains(visibleGroupTypes_, groupType)) {
			const auto group = makeGroup(groupType);
			groups_.push_back(group);
			if (!group)
				continue;
#if defined(EDB_X86) || defined(EDB_X86_64)
			if (groupType == RegisterGroupType::Segment || groupType == RegisterGroupType::ExpandedEFL) {
				flagsAndSegments_->addWidget(group);
				if (!flagsAndSegsInserted) {
					layout->addLayout(flagsAndSegments_.get());
					flagsAndSegsInserted = true;
				}
			} else
#endif
				layout->addWidget(group);
		} else
			groups_.push_back(nullptr);
	}

	widget()->show();
}

void ODBRegView::modelUpdated() {
	Q_FOREACH (FieldWidget *field, fields()) {
		field->adjustToData();
	}

	Q_FOREACH (RegisterGroup *group, groups_) {
		if (group) {
			group->adjustWidth();
		}
	}
}

QList<FieldWidget *> ODBRegView::fields() const {

	QList<FieldWidget *> allFields;
	for (RegisterGroup *group : groups_) {
		if (group) {
			allFields.append(group->fields());
		}
	}

	return allFields;
}

QList<ValueField *> ODBRegView::valueFields() const {

	QList<ValueField *> allValues;
	for (RegisterGroup *group : groups_) {
		if (group) {
			allValues.append(group->valueFields());
		}
	}

	return allValues;
}

void ODBRegView::updateFieldsPalette() {
	Q_FOREACH (ValueField *field, valueFields())
		field->updatePalette();
}

ValueField *ODBRegView::selectedField() const {
	Q_FOREACH (ValueField *field, valueFields()) {
		if (field->isSelected()) {
			return field;
		}
	}

	return nullptr;
}

void ODBRegView::selectAField() {
	const QList<ValueField *> fields = valueFields();
	if (!fields.isEmpty()) {
		fields.front()->select();
	}
}

void ODBRegView::keyPressEvent(QKeyEvent *event) {

	ValueField *selected = selectedField();

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
	case SetToZeroKey:
		if (selected) {
			selected->setZero();
			return;
		}
		break;
	case IncrementKey:
		if (selected) {
			selected->increment();
			return;
		}
		break;
	case DecrementKey:
		if (selected) {
			selected->decrement();
			return;
		}
		break;
	}
	QScrollArea::keyPressEvent(event);
}

}
