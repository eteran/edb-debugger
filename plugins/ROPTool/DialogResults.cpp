
#include "DialogResults.h"
#include "edb.h"
#include <QSortFilterProxyModel>

namespace ROPToolPlugin {

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

	result_filter_ = new ResultFilterProxy(this);
	result_filter_->setSourceModel(model_);

	filter_model_->setFilterKeyColumn(1);
	filter_model_->setSourceModel(result_filter_);
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

//------------------------------------------------------------------------------
// Name: on_chkShowALU_stateChanged
// Desc:
//------------------------------------------------------------------------------
void DialogResults::on_chkShowALU_stateChanged(int state) {
	result_filter_->setMask(0x01, state);
}

//------------------------------------------------------------------------------
// Name: on_chkShowStack_stateChanged
// Desc:
//------------------------------------------------------------------------------
void DialogResults::on_chkShowStack_stateChanged(int state) {
	result_filter_->setMask(0x02, state);
}

//------------------------------------------------------------------------------
// Name: on_chkShowLogic_stateChanged
// Desc:
//------------------------------------------------------------------------------
void DialogResults::on_chkShowLogic_stateChanged(int state) {
	result_filter_->setMask(0x04, state);
}

//------------------------------------------------------------------------------
// Name: on_chkShowData_stateChanged
// Desc:
//------------------------------------------------------------------------------
void DialogResults::on_chkShowData_stateChanged(int state) {
	result_filter_->setMask(0x08, state);
}

//------------------------------------------------------------------------------
// Name: on_chkShowOther_stateChanged
// Desc:
//------------------------------------------------------------------------------
void DialogResults::on_chkShowOther_stateChanged(int state) {
	result_filter_->setMask(0x10, state);
}

//------------------------------------------------------------------------------
// Name: on_tableView_doubleClicked
// Desc: follows the found item in the table
//------------------------------------------------------------------------------
void DialogResults::on_tableView_doubleClicked(const QModelIndex &index) {
	if(index.isValid()) {
		const QModelIndex realIndex = filter_model_->mapToSource(index);
		if(realIndex.isValid()) {
			const QModelIndex realIndex2 = result_filter_->mapToSource(realIndex);
			if(auto item = static_cast<Result *>(realIndex2.internalPointer())) {
				edb::v1::jump_to_address(item->address);
			}
		}

	}
}

}
