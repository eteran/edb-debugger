
#include "DialogResults.h"
#include "edb.h"

namespace BinarySearcherPlugin {

/**
 * @brief DialogResults::DialogResults
 * @param parent
 * @param f
 */
DialogResults::DialogResults(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {

	ui.setupUi(this);
}

/**
 * follows the found item in the appropriate view
 *
 * @brief DialogResults::on_listWidget_itemDoubleClicked
 * @param item
 */
void DialogResults::on_listWidget_itemDoubleClicked(QListWidgetItem *item) {
	const edb::address_t addr = item->data(Qt::UserRole).toULongLong();
	switch (static_cast<RegionType>(item->data(Qt::UserRole + 1).toInt())) {
	case RegionType::Code:
		edb::v1::jump_to_address(addr);
		break;
	case RegionType::Stack:
		edb::v1::dump_stack(addr, true);
		break;
	case RegionType::Data:
		edb::v1::dump_data(addr);
		break;
	}
}

/**
 * @brief DialogResults::addResult
 * @param address
 */
void DialogResults::addResult(RegionType region, edb::address_t address) {
	auto item = new QListWidgetItem(edb::v1::format_pointer(address));
	item->setData(Qt::UserRole, address.toQVariant());
	item->setData(Qt::UserRole + 1, static_cast<int>(region));
	ui.listWidget->addItem(item);
}

/**
 * @brief DialogResults::resultCount
 * @return
 */
int DialogResults::resultCount() const {
	return ui.listWidget->count();
}

}
