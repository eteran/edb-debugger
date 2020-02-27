
#include "SimdValueManager.h"
#include "ODbgRV_Util.h"
#include "ValueField.h"
#include "util/Container.h"

namespace ODbgRegisterView {

SimdValueManager::SimdValueManager(int lineInGroup, const QModelIndex &nameIndex, RegisterGroup *parent)
	: QObject(parent), regIndex_(nameIndex), lineInGroup_(lineInGroup) {

	setupMenu();

	assert(nameIndex.isValid());
	connect(nameIndex.model(), SIGNAL(SIMDDisplayFormatChanged()), this, SLOT(displayFormatChanged()));
	displayFormatChanged();
}

void SimdValueManager::fillGroupMenu() {
	const auto group = this->group();
	group->menuItems_.push_back(new_action_separator(this));
	group->menuItems_.push_back(menuItems_[VIEW_AS_BYTES]);
	group->menuItems_.push_back(menuItems_[VIEW_AS_WORDS]);
	group->menuItems_.push_back(menuItems_[VIEW_AS_DWORDS]);
	group->menuItems_.push_back(menuItems_[VIEW_AS_QWORDS]);
	group->menuItems_.push_back(new_action_separator(this));
	group->menuItems_.push_back(menuItems_[VIEW_AS_FLOAT32]);
	group->menuItems_.push_back(menuItems_[VIEW_AS_FLOAT64]);
	group->menuItems_.push_back(new_action_separator(this));
	group->menuItems_.push_back(menuItems_[VIEW_INT_AS_HEX]);
	group->menuItems_.push_back(menuItems_[VIEW_INT_AS_SIGNED]);
	group->menuItems_.push_back(menuItems_[VIEW_INT_AS_UNSIGNED]);
}

auto SimdValueManager::model() const -> Model * {
	const auto model = static_cast<const Model *>(regIndex_.model());
	// The model is not supposed to have been created as const object,
	// and our manipulations won't invalidate the index.
	// Thus cast the const away.
	return const_cast<Model *>(model);
}

void SimdValueManager::showAsInt(Model::ElementSize size) {
	model()->setChosenSIMDSize(regIndex_.parent(), size);
	model()->setChosenSIMDFormat(regIndex_.parent(), intMode_);
}

void SimdValueManager::showAsFloat(Model::ElementSize size) {
	model()->setChosenSIMDFormat(regIndex_.parent(), NumberDisplayMode::Float);

	switch (size) {
	case Model::ElementSize::DWORD:
		model()->setChosenSIMDSize(regIndex_.parent(), Model::ElementSize::DWORD);
		break;
	case Model::ElementSize::QWORD:
		model()->setChosenSIMDSize(regIndex_.parent(), Model::ElementSize::QWORD);
		break;
	default:
		EDB_PRINT_AND_DIE("Unexpected size: ", size);
	}
}

void SimdValueManager::setIntFormat(NumberDisplayMode format) {
	model()->setChosenSIMDFormat(regIndex_.parent(), format);
}

void SimdValueManager::setupMenu() {
	const auto group        = this->group();
	const auto validFormats = valid_variant(regIndex_.parent().data(Model::ValidSIMDFormatsRole)).value<std::vector<NumberDisplayMode>>();
	// Setup menu if we're the first value field creator
	if (group->valueFields().isEmpty()) {

		menuItems_.push_back(new_action(tr("View %1 as bytes").arg(group->name_), group, [this]() {
			showAsInt(Model::ElementSize::BYTE);
		}));

		menuItems_.push_back(new_action(tr("View %1 as words").arg(group->name_), group, [this]() {
			showAsInt(Model::ElementSize::WORD);
		}));

		menuItems_.push_back(new_action(tr("View %1 as doublewords").arg(group->name_), group, [this]() {
			showAsInt(Model::ElementSize::DWORD);
		}));

		menuItems_.push_back(new_action(tr("View %1 as quadwords").arg(group->name_), group, [this]() {
			showAsInt(Model::ElementSize::QWORD);
		}));

		if (util::contains(validFormats, NumberDisplayMode::Float)) {
			menuItems_.push_back(new_action(tr("View %1 as 32-bit floats").arg(group->name_), group, [this]() {
				showAsFloat(Model::ElementSize::DWORD);
			}));

			menuItems_.push_back(new_action(tr("View %1 as 64-bit floats").arg(group->name_), group, [this]() {
				showAsFloat(Model::ElementSize::QWORD);
			}));
		} else {
			// create empty elements to leave further items with correct indices
			menuItems_.push_back(new_action_separator(this));
			menuItems_.push_back(new_action_separator(this));
		}

		menuItems_.push_back(new_action(tr("View %1 integers as hex").arg(group->name_), group, [this]() {
			setIntFormat(NumberDisplayMode::Hex);
		}));

		menuItems_.push_back(new_action(tr("View %1 integers as signed").arg(group->name_), group, [this]() {
			setIntFormat(NumberDisplayMode::Signed);
		}));

		menuItems_.push_back(new_action(tr("View %1 integers as unsigned").arg(group->name_), group, [this]() {
			setIntFormat(NumberDisplayMode::Unsigned);
		}));

		fillGroupMenu();
	}
}

void SimdValueManager::updateMenu() {
	if (menuItems_.isEmpty())
		return;
	Q_FOREACH (auto item, menuItems_)
		item->setVisible(true);

	using RegisterViewModelBase::Model;
	switch (currentSize()) {
	case Model::ElementSize::BYTE:
		menuItems_[VIEW_AS_BYTES]->setVisible(false);
		break;
	case Model::ElementSize::WORD:
		menuItems_[VIEW_AS_WORDS]->setVisible(false);
		break;
	case Model::ElementSize::DWORD:
		if (currentFormat() != NumberDisplayMode::Float)
			menuItems_[VIEW_AS_DWORDS]->setVisible(false);
		else
			menuItems_[VIEW_AS_FLOAT32]->setVisible(false);
		break;
	case Model::ElementSize::QWORD:
		if (currentFormat() != NumberDisplayMode::Float)
			menuItems_[VIEW_AS_QWORDS]->setVisible(false);
		else
			menuItems_[VIEW_AS_FLOAT64]->setVisible(false);
		break;
	default:
		EDB_PRINT_AND_DIE("Unexpected current size: ", currentSize());
	}
	switch (currentFormat()) {
	case NumberDisplayMode::Float:
		menuItems_[VIEW_INT_AS_HEX]->setVisible(false);
		menuItems_[VIEW_INT_AS_SIGNED]->setVisible(false);
		menuItems_[VIEW_INT_AS_UNSIGNED]->setVisible(false);
		break;
	case NumberDisplayMode::Hex:
		menuItems_[VIEW_INT_AS_HEX]->setVisible(false);
		break;
	case NumberDisplayMode::Signed:
		menuItems_[VIEW_INT_AS_SIGNED]->setVisible(false);
		break;
	case NumberDisplayMode::Unsigned:
		menuItems_[VIEW_INT_AS_UNSIGNED]->setVisible(false);
		break;
	}
}

RegisterGroup *SimdValueManager::group() const {
	return checked_cast<RegisterGroup>(parent());
}

void SimdValueManager::displayFormatChanged() {
	const auto newFormat = currentFormat();

	if (newFormat != NumberDisplayMode::Float) {
		intMode_ = newFormat;
	}

	Q_FOREACH (const auto elem, elements_) {
		elem->deleteLater();
	}

	elements_.clear();

	using RegisterViewModelBase::Model;
	const auto model = regIndex_.model();

	const int sizeRow     = valid_variant(regIndex_.parent().data(Model::ChosenSIMDSizeRowRole)).toInt();
	QModelIndex sizeIndex = model->index(sizeRow, ModelNameColumn, regIndex_);
	const auto elemCount  = model->rowCount(sizeIndex);

	const auto regNameWidth = valid_variant(regIndex_.data(Model::FixedLengthRole)).toInt();
	int column              = regNameWidth + 1;
	const auto elemWidth    = valid_variant(model->index(0, ModelValueColumn, sizeIndex).data(Model::FixedLengthRole)).toInt();
	for (int elemN = elemCount - 1; elemN >= 0; --elemN) {
		const auto elemIndex = model->index(elemN, ModelValueColumn, sizeIndex);
		const auto field     = new ValueField(elemWidth, elemIndex, group());
		elements_.push_back(field);
		field->setAlignment(Qt::AlignRight);
		group()->insert(lineInGroup_, column, field);
		column += elemWidth + 1;
	}

	updateMenu();
}

RegisterViewModelBase::Model::ElementSize SimdValueManager::currentSize() const {
	using RegisterViewModelBase::Model;
	const int size = valid_variant(regIndex_.parent().data(Model::ChosenSIMDSizeRole)).toInt();
	return static_cast<Model::ElementSize>(size);
}

NumberDisplayMode SimdValueManager::currentFormat() const {
	using RegisterViewModelBase::Model;
	const int size = valid_variant(regIndex_.parent().data(Model::ChosenSIMDFormatRole)).toInt();
	return static_cast<NumberDisplayMode>(size);
}

}
