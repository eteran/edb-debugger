
#include "DialogResults.h"
#include "edb.h"
#include <QSortFilterProxyModel>

namespace ProcessPropertiesPlugin {

/**
 * @brief DialogResults::DialogResults
 * @param parent
 * @param f
 */
DialogResults::DialogResults(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {
	ui.setupUi(this);
	ui.tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

	model_       = new ResultsModel(this);
	filterModel_ = new QSortFilterProxyModel(this);

	filterModel_->setFilterKeyColumn(2);
	filterModel_->setSourceModel(model_);
	ui.tableView->setModel(filterModel_);

	connect(ui.textFilter, &QLineEdit::textChanged, filterModel_, &QSortFilterProxyModel::setFilterFixedString);
}

/**
 * @brief DialogResults::addResult
 * @param result
 */
void DialogResults::addResult(const ResultsModel::Result &result) {
	model_->addResult(result);
}

/**
 * @brief DialogResults::on_tableView_doubleClicked
 * @param index
 */
void DialogResults::on_tableView_doubleClicked(const QModelIndex &index) {
	if (index.isValid()) {
		const QModelIndex realIndex = filterModel_->mapToSource(index);
		if (realIndex.isValid()) {
			if (auto item = static_cast<ResultsModel::Result *>(realIndex.internalPointer())) {
				edb::v1::dump_data(item->address, false);
			}
		}
	}
}

/**
 * @brief DialogResults::resultCount
 * @return
 */
int DialogResults::resultCount() const {
	return model_->rowCount();
}

}
