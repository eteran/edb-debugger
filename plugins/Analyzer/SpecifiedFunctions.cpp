/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
 * @brief Constructs the specified functions dialog and sets up its list model, filter, and refresh button.
 *
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

	buttonRefresh_ = new QPushButton(QIcon::fromTheme(QStringLiteral("view-refresh")), tr("Refresh"));
	connect(buttonRefresh_, &QPushButton::clicked, this, [this]() {
		buttonRefresh_->setEnabled(false);
		doFind();
		buttonRefresh_->setEnabled(true);
	});

	ui.buttonBox->addButton(buttonRefresh_, QDialogButtonBox::ActionRole);
}

/**
 * @brief Jumps the disassembly view to the address corresponding to the double-clicked list entry.
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
 * @brief Refreshes the list with all currently user-specified function addresses.
 */
void SpecifiedFunctions::doFind() {

	IAnalyzer *const analyzer      = edb::v1::analyzer();
	QSet<edb::address_t> functions = analyzer->specifiedFunctions();

	QStringList results;
	results.reserve(static_cast<int>(functions.size()));
	for (edb::address_t address : functions) {
		results.push_back(edb::v1::format_pointer(address));
	}
	model_->setStringList(results);
}

/**
 * @brief Refreshes the user-specified function list every time the dialog becomes visible.
 */
void SpecifiedFunctions::showEvent(QShowEvent *) {
	buttonRefresh_->setEnabled(false);
	doFind();
	buttonRefresh_->setEnabled(true);
}

}
