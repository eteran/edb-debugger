/*
Copyright (C) 2006 - 2015 Evan Teran
                          evan.teran@gmail.com

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

#include "SpecifiedFunctions.h"
#include "edb.h"
#include "IAnalyzer.h"

#include <QStringListModel>
#include <QSortFilterProxyModel>
#include <QHeaderView>
#include <QtDebug>
#include <QPushButton>

namespace AnalyzerPlugin {

//------------------------------------------------------------------------------
// Name: SpecifiedFunctions
// Desc:
//------------------------------------------------------------------------------
SpecifiedFunctions::SpecifiedFunctions(QWidget *parent, Qt::WindowFlags f) : QDialog(parent, f) {
	ui.setupUi(this);

	model_        = new QStringListModel(this);
	filter_model_ = new QSortFilterProxyModel(this);

	filter_model_->setFilterKeyColumn(0);
	filter_model_->setSourceModel(model_);
	ui.function_list->setModel(filter_model_);

	connect(ui.filter, &QLineEdit::textChanged, filter_model_, &QSortFilterProxyModel::setFilterFixedString);

	btnRefresh_ = new QPushButton(QIcon::fromTheme("view-refresh"), tr("Refresh"));
	connect(btnRefresh_, &QPushButton::clicked, this, [this]() {
		btnRefresh_->setEnabled(false);
		do_find();
		btnRefresh_->setEnabled(true);
	});

	ui.buttonBox->addButton(btnRefresh_, QDialogButtonBox::ActionRole);
}

//------------------------------------------------------------------------------
// Name: on_function_list_doubleClicked
// Desc: follows the found item in the data view
//------------------------------------------------------------------------------
void SpecifiedFunctions::on_function_list_doubleClicked(const QModelIndex &index) {

	const QString s = index.data().toString();
	if(const Result<edb::address_t, QString> addr = edb::v1::string_to_address(s)) {
		edb::v1::jump_to_address(*addr);
	}
}

//------------------------------------------------------------------------------
// Name: do_find
// Desc:
//------------------------------------------------------------------------------
void SpecifiedFunctions::do_find() {

	IAnalyzer *const analyzer = edb::v1::analyzer();
	QSet<edb::address_t> functions = analyzer->specified_functions();
	QStringList results;
	for(edb::address_t address: functions) {
		results << QString("%1").arg(edb::v1::format_pointer(address));
	}
	model_->setStringList(results);
}

//------------------------------------------------------------------------------
// Name: showEvent
// Desc:
//------------------------------------------------------------------------------
void SpecifiedFunctions::showEvent(QShowEvent *) {
	btnRefresh_->setEnabled(false);
	do_find();
	btnRefresh_->setEnabled(true);
}

}
