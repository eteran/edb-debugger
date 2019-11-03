
#include "DialogXRefs.h"

namespace AnalyzerPlugin {

/**
 * @brief DialogXRefs::DialogXRefs
 * @param parent
 */
DialogXRefs::DialogXRefs(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {
	ui.setupUi(this);
}

/**
 * @brief DialogXRefs::on_listReferences_itemDoubleClicked
 * @param item
 */
void DialogXRefs::on_listReferences_itemDoubleClicked(QListWidgetItem *item) {

	edb::address_t site = item->data(Qt::UserRole).toULongLong();
	edb::v1::jump_to_address(site);
}

/**
 * @brief DialogXRefs::addReference
 * @param ref
 */
void DialogXRefs::addReference(const std::pair<edb::address_t, edb::address_t> &ref) {

	int offset;
	QString sym = edb::v1::find_function_symbol(ref.first, ref.first.toPointerString(), &offset);

	auto string = tr("%1. %2 -> %3").arg(ui.listReferences->count() + 1, 2, 10, QChar('0')).arg(sym).arg(ref.second.toPointerString());

	auto item = new QListWidgetItem(string, ui.listReferences);
	item->setData(Qt::UserRole, static_cast<qlonglong>(ref.first));
}

}
