/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "DialogResults.h"
#include "IAnalyzer.h"
#include "ISymbolManager.h"
#include "MemoryRegions.h"
#include "ResultsModel.h"
#include "edb.h"
#ifdef ENABLE_GRAPH
#include "GraphEdge.h"
#include "GraphNode.h"
#include "GraphWidget.h"
#endif
#include <QDialog>
#include <QHeaderView>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSortFilterProxyModel>

namespace FunctionFinderPlugin {

/**
 * @brief Constructs a DialogResults object with the specified parent widget and window flags.
 *
 * @param parent The parent widget for this dialog.
 * @param f The window flags for this dialog.
 */
DialogResults::DialogResults(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {

	ui.setupUi(this);
	ui.tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

	resultsModel_ = new ResultsModel(this);

	filterModel_ = new QSortFilterProxyModel(this);
	filterModel_->setFilterKeyColumn(5);
	filterModel_->setSourceModel(resultsModel_);
	connect(ui.textFilter, &QLineEdit::textChanged, filterModel_, &QSortFilterProxyModel::setFilterFixedString);
	ui.tableView->setModel(filterModel_);

	buttonGraph_ = new QPushButton(QIcon::fromTheme("distribute-graph"), tr("Graph Selected Function"));
#if defined(ENABLE_GRAPH)
	connect(buttonGraph_, &QPushButton::clicked, this, [this]() {
		// this code is not very pretty...
		// but it works!
		qDebug("[FunctionFinder] Constructing Graph...");

		const QItemSelectionModel *const selModel = ui.tableView->selectionModel();
		const QModelIndexList sel                 = selModel->selectedRows();

		if (sel.size() == 1) {
			const QModelIndex index = filterModel_->mapToSource(sel[0]);

			if (auto item = static_cast<ResultsModel::Result *>(index.internalPointer())) {
				const edb::address_t addr = item->startAddress;

				if (IAnalyzer *const analyzer = edb::v1::analyzer()) {
					const IAnalyzer::FunctionMap &functions = analyzer->functions();

					auto it = functions.find(addr);
					if (it != functions.end()) {
						Function f = *it;

						auto graph = new GraphWidget(nullptr);
						graph->setAttribute(Qt::WA_DeleteOnClose);

						QMap<edb::address_t, GraphNode *> nodes;

						// first create all of the nodes
						for (const auto &pair : f) {
							const auto &[address, bb] = pair;
							Q_UNUSED(address)
							auto node = new GraphNode(graph, bb.toString(), Qt::lightGray);
							nodes.insert(bb.firstAddress(), node);
						}

						// then connect them!
						for (const auto &pair : f) {
							const auto &[address, bb] = pair;
							Q_UNUSED(address)

							if (!bb.empty()) {

								auto term  = bb.back();
								auto &inst = *term;

								if (is_unconditional_jump(inst)) {

									Q_ASSERT(inst.operandCount() >= 1);
									const auto op = inst[0];

									// TODO: we need some heuristic for detecting when this is
									//       a call/ret -> jmp optimization
									if (is_immediate(op)) {
										const edb::address_t ea = op->imm;

										auto from = nodes.find(bb.firstAddress());
										auto to   = nodes.find(ea);
										if (to != nodes.end() && from != nodes.end()) {
											new GraphEdge(from.value(), to.value(), Qt::black);
										}
									}
								} else if (is_conditional_jump(inst)) {

									Q_ASSERT(inst.operandCount() == 1);
									const auto op = inst[0];

									if (is_immediate(op)) {

										auto from = nodes.find(bb.firstAddress());

										auto to_taken = nodes.find(op->imm);
										if (to_taken != nodes.end() && from != nodes.end()) {
											new GraphEdge(from.value(), to_taken.value(), Qt::green);
										}

										auto to_skipped = nodes.find(inst.rva() + inst.byteSize());
										if (to_taken != nodes.end() && from != nodes.end()) {
											new GraphEdge(from.value(), to_skipped.value(), Qt::red);
										}
									}
								} else if (is_terminator(inst)) {
								} else {
									// if the bb's last address is another blocks first address
									// connect them because they run into each other

									auto to = nodes.find(bb.lastAddress());
									if (to != nodes.end()) {
										auto from = nodes.find(bb.firstAddress());
										if (to != nodes.end() && from != nodes.end()) {
											new GraphEdge(from.value(), to.value(), Qt::blue);
										}
									}
								}
							}
						}

						graph->layout();
						graph->show();
					}
				}
			}
		}
	});
#endif

	ui.buttonBox->addButton(buttonGraph_, QDialogButtonBox::ActionRole);

#ifdef ENABLE_GRAPH
	buttonGraph_->setEnabled(true);
#else
	buttonGraph_->setEnabled(false);
#endif
}

/**
 * @brief Handles the double-click event on the table view, jumping to the selected function's start address in the debugger.
 *
 * @param index The model index of the double-clicked item in the table view.
 */
void DialogResults::on_tableView_doubleClicked(const QModelIndex &index) {

	if (index.isValid()) {
		const QModelIndex realIndex = filterModel_->mapToSource(index);
		if (auto item = static_cast<ResultsModel::Result *>(realIndex.internalPointer())) {
			edb::v1::jump_to_address(item->startAddress);
		}
	}
}

/**
 * @brief Adds a function result to the dialog.
 *
 * @param function The function to add.
 */
void DialogResults::addResult(const Function &function) {

	ResultsModel::Result result;

	// entry point
	result.startAddress = function.entryAddress();

	// upper bound of the function
	result.endAddress = function.endAddress();
	result.size       = function.endAddress() - function.entryAddress() + 1;

	// reference count
	result.score = function.referenceCount();

	// type
	result.type = function.type;

	QString symbol_name = edb::v1::symbol_manager().findAddressName(function.entryAddress());
	if (!symbol_name.isEmpty()) {
		result.symbol = symbol_name;
	}

	resultsModel_->addResult(result);
}

/**
 * @brief Returns the number of results in the dialog.
 *
 * @return The number of results in the dialog.
 */
int DialogResults::resultCount() const {
	return resultsModel_->rowCount();
}

}
