
#include "DialogXRefs.h"

#include "ui_DialogXRefs.h"

namespace AnalyzerPlugin {

DialogXRefs::DialogXRefs(QWidget *parent) : QDialog(parent), ui(new Ui::DialogXRefs) {
	ui->setupUi(this);
}

DialogXRefs::~DialogXRefs() {
	delete ui;
}

void DialogXRefs::on_listReferences_itemDoubleClicked(QListWidgetItem *item) {

	edb::address_t site = item->data(Qt::UserRole).toULongLong();
	edb::v1::jump_to_address(site);

}

void DialogXRefs::addReference(const std::pair<edb::address_t, edb::address_t> &ref) {

	int offset;
	QString sym = edb::v1::find_function_symbol(ref.first, ref.first.toPointerString(), &offset);


	auto string = tr("%1. %2 -> %3").arg(ui->listReferences->count() + 1, 2, 10, QChar('0')).arg(sym).arg(ref.second.toPointerString());

	auto item = new QListWidgetItem(string, ui->listReferences);
	item->setData(Qt::UserRole, static_cast<qlonglong>(ref.first));
}

}
