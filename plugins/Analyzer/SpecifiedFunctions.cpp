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
#include "IAnalyzer.h"
#include "edb.h"

#include <QHeaderView>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QStringListModel>
#include <QtDebug>

namespace AnalyzerPlugin {

/**
 * @brief SpecifiedFunctions::SpecifiedFunctions
 * @param parent
 * @param f
 */
SpecifiedFunctions::SpecifiedFunctions(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {

	ui.setupUi(this);

	model_       = new QStringListModel(this);
	filterModel_ = new QSortFilterProxyModel(this);

	filterModel_->setFilterKeyColumn(0);
	filterModel_->setSourceModel(model_);
	ui.function_list->setModel(filterModel_);

	connect(ui.filter, &QLineEdit::textChanged, filterModel_, &QSortFilterProxyModel::setFilterFixedString);

	buttonRefresh_ = new QPushButton(QIcon::fromTheme("view-refresh"), tr("Refresh"));
	connect(buttonRefresh_, &QPushButton::clicked, this, [this]() {
		buttonRefresh_->setEnabled(false);
		doFind();
		buttonRefresh_->setEnabled(true);
	});

	ui.buttonBox->addButton(buttonRefresh_, QDialogButtonBox::ActionRole);
}

/**
 * @brief SpecifiedFunctions::on_function_list_doubleClicked
 *
 * follows the found item in the data view
 *
 * @param index
 */
void SpecifiedFunctions::on_function_list_doubleClicked(const QModelIndex &index) {

	const QString s = index.data().toString();
	if (const Result<edb::address_t, QString> addr = edb::v1::string_to_address(s)) {
		edb::v1::jump_to_address(*addr);
	}
}

/**
 * @brief SpecifiedFunctions::doFind
 */
void SpecifiedFunctions::doFind() {

	IAnalyzer *const analyzer      = edb::v1::analyzer();
	QSet<edb::address_t> functions = analyzer->specifiedFunctions();

	QStringList results;
	for (edb::address_t address : functions) {
		results << QString("%1").arg(edb::v1::format_pointer(address));
	}
	model_->setStringList(results);
}

/**
 * @brief SpecifiedFunctions::showEvent
 */
void SpecifiedFunctions::showEvent(QShowEvent *) {
	buttonRefresh_->setEnabled(false);
	doFind();
	buttonRefresh_->setEnabled(true);
}

}
