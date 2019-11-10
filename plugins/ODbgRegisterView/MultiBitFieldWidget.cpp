
#include "MultiBitFieldWidget.h"
#include "BitFieldDescription.h"
#include "BitFieldFormatter.h"
#include "ODbgRV_Util.h"
#include "RegisterView.h"

namespace ODbgRegisterView {

MultiBitFieldWidget::MultiBitFieldWidget(const QModelIndex &index, const BitFieldDescription &bfd, QWidget *parent, Qt::WindowFlags f)
	: ValueField(bfd.textWidth, index, BitFieldFormatter(bfd), parent, f), equal_(bfd.valueEqualComparator) {

	menuItems_.push_front(new_action_separator(this));

	for (std::size_t i = bfd.valueNames.size(); i-- > 0;) {
		const auto &text = bfd.setValueTexts[i];
		if (!text.isEmpty()) {
			auto action = new_action(text, this, [this, i]() {
				setValue(i);
			});

			menuItems_.push_front(action);
			valueActions_.push_front(menuItems_.front());
		} else
			valueActions_.push_front(nullptr);
	}
}

void MultiBitFieldWidget::setValue(int value) {

	using namespace RegisterViewModelBase;

	// TODO: Model: make it possible to set bit field itself, without manipulating parent directly
	//              I.e. set value without knowing field offset, then setData(fieldIndex,word)
	const auto regIndex = index_.parent().sibling(index_.parent().row(), ModelValueColumn);
	auto byteArr        = regIndex.data(Model::RawValueRole).toByteArray();

	if (byteArr.isEmpty())
		return;

	std::uint64_t word(0);
	std::memcpy(&word, byteArr.constData(), byteArr.size());
	const auto mask   = (1ull << (valid_variant(index_.data(Model::BitFieldLengthRole)).toInt() - 1)) * 2 - 1;
	const auto offset = valid_variant(index_.data(Model::BitFieldOffsetRole)).toInt();
	word              = (word & ~(mask << offset)) | (std::uint64_t(value) << offset);
	std::memcpy(byteArr.data(), &word, byteArr.size());
	model()->setData(regIndex, byteArr, Model::RawValueRole);
}

void MultiBitFieldWidget::adjustToData() {

	using namespace RegisterViewModelBase;

	ValueField::adjustToData();

	const auto byteArr = index_.data(Model::RawValueRole).toByteArray();
	std::uint64_t word(0);
	assert(unsigned(byteArr.size()) <= sizeof(word));
	std::memcpy(&word, byteArr.constData(), byteArr.size());

	for (int value = 0; value < valueActions_.size(); ++value) {
		const auto action = valueActions_[value];
		if (!action)
			continue;
		if (byteArr.isEmpty() || equal_(word, value))
			action->setVisible(false);
		else
			action->setVisible(true);
	}
}

}
