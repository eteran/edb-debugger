
#include "DialogResults.h"
#include "edb.h"
#include <QSortFilterProxyModel>

namespace ProcessPropertiesPlugin {

/**
 * @brief DialogResults::DialogResults
 * @param parent
 * @param f
 */
DialogResults::DialogResults(QWidget *parent, Qt::WindowFlags f) : QDialog(parent, f) {
	ui.setupUi(this);
	ui.tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

	model_        = new ResultsModel(this);
	filter_model_ = new QSortFilterProxyModel(this);

	filter_model_->setFilterKeyColumn(2);
	filter_model_->setSourceModel(model_);
	ui.tableView->setModel(filter_model_);

	connect(ui.textFilter, &QLineEdit::textChanged, filter_model_, &QSortFilterProxyModel::setFilterFixedString);
}

/**
 * @brief DialogResults::addResult
 * @param result
 */
void DialogResults::addResult(const Result &result) {
	model_->addResult(result);
}


/**
 * @brief DialogResults::on_tableView_doubleClicked
 * @param index
 */
void DialogResults::on_tableView_doubleClicked(const QModelIndex &index) {
	if(index.isValid()) {
		const QModelIndex realIndex = filter_model_->mapToSource(index);
		if(realIndex.isValid()) {
			if(auto item = static_cast<Result *>(realIndex.internalPointer())) {
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
