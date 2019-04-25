/*
Copyright (C) 2006 - 2019 Evan Teran
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

#include "DialogResults.h"
#include "edb.h"
#include "IAnalyzer.h"
#include "MemoryRegions.h"
#include "ISymbolManager.h"
#include "ResultsModel.h"
#ifdef ENABLE_GRAPH
#include "GraphWidget.h"
#include "GraphNode.h"
#include "GraphEdge.h"
#endif
#include <QDialog>
#include <QHeaderView>
#include <QMenu>
#include <QMessageBox>
#include <QSortFilterProxyModel>
#include <QPushButton>

namespace FunctionFinderPlugin {

/**
 * @brief DialogResults::DialogResults
 * @param parent
 * @param f
 */
DialogResults::DialogResults(QWidget *parent, Qt::WindowFlags f) : QDialog(parent, f) {
	ui.setupUi(this);
	ui.tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

	resultsModel_ = new ResultsModel(this);

	filter_model_ = new QSortFilterProxyModel(this);
	filter_model_->setFilterKeyColumn(5);
	filter_model_->setSourceModel(resultsModel_);
	connect(ui.textFilter, &QLineEdit::textChanged, filter_model_, &QSortFilterProxyModel::setFilterFixedString);
	ui.tableView->setModel(filter_model_);

	btnGraph_ = new QPushButton(QIcon::fromTheme("distribute-graph"), tr("Graph Selected Function"));
#if defined(ENABLE_GRAPH)
	connect(btnGraph_, &QPushButton::clicked, this, [this]() {

		// this code is not very pretty...
		// but it works!
		qDebug("[FunctionFinder] Constructing Graph...");

		const QItemSelectionModel *const selModel = ui.tableView->selectionModel();
		const QModelIndexList sel = selModel->selectedRows();

		if(sel.size() == 1) {
			const QModelIndex index = filter_model_->mapToSource(sel[0]);

			if(auto item = static_cast<Result *>(index.internalPointer())) {
				const edb::address_t addr = item->start_address;

				if(IAnalyzer *const analyzer = edb::v1::analyzer()) {
					const IAnalyzer::FunctionMap &functions = analyzer->functions();

					auto it = functions.find(addr);
					if(it != functions.end()) {
						Function f = *it;

						auto graph = new GraphWidget(nullptr);
						graph->setAttribute(Qt::WA_DeleteOnClose);

						QMap<edb::address_t, GraphNode *> nodes;

						// first create all of the nodes
						for(const auto &pair : f) {
							const BasicBlock &bb = pair.second;
							auto node = new GraphNode(graph, bb.toString(), Qt::lightGray);
							nodes.insert(bb.firstAddress(), node);
						}

						// then connect them!
						for(const auto &pair : f) {
							const BasicBlock &bb = pair.second;

							if(!bb.empty()) {

								auto term = bb.back();
								auto &inst = *term;

								if(is_unconditional_jump(inst)) {

									Q_ASSERT(inst.operand_count() >= 1);
									const auto op = inst[0];

									// TODO: we need some heuristic for detecting when this is
									//       a call/ret -> jmp optimization
									if(is_immediate(op)) {
										const edb::address_t ea = op->imm;

										auto from = nodes.find(bb.firstAddress());
										auto to = nodes.find(ea);
										if(to != nodes.end() && from != nodes.end()) {
											new GraphEdge(from.value(), to.value(), Qt::black);
										}
									}
								} else if(is_conditional_jump(inst)) {

									Q_ASSERT(inst.operand_count() == 1);
									const auto op = inst[0];

									if(is_immediate(op)) {

										auto from = nodes.find(bb.firstAddress());

										auto to_taken = nodes.find(op->imm);
										if(to_taken != nodes.end() && from != nodes.end()) {
											new GraphEdge(from.value(), to_taken.value(), Qt::green);
										}

										auto to_skipped = nodes.find(inst.rva() + inst.byte_size());
										if(to_taken != nodes.end() && from != nodes.end()) {
											new GraphEdge(from.value(), to_skipped.value(), Qt::red);
										}
									}
								} else if(is_terminator(inst)) {
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

	ui.buttonBox->addButton(btnGraph_, QDialogButtonBox::ActionRole);

#ifdef ENABLE_GRAPH
	btnGraph_->setEnabled(true);
#else
	btnGraph_->setEnabled(false);
#endif
}

/**
 * @brief DialogResults::on_tableView_doubleClicked
 * @param index
 */
void DialogResults::on_tableView_doubleClicked(const QModelIndex &index) {

	if(index.isValid()) {
		const QModelIndex realIndex = filter_model_->mapToSource(index);
		if(auto item = static_cast<Result *>(realIndex.internalPointer())) {
			edb::v1::jump_to_address(item->start_address);
		}
	}
}

/**
 * @brief DialogResults::addResult
 * @param function
 */
void DialogResults::addResult(const Function &function) {

	Result result;

	// entry point
	result.start_address = function.entry_address();

	// upper bound of the function
	result.end_address = function.end_address();
	result.size = function.end_address() - function.entry_address() + 1;

	// reference count
	result.score = function.reference_count();

	// type
	result.type = function.type();

	QString symbol_name = edb::v1::symbol_manager().find_address_name(function.entry_address());
	if(!symbol_name.isEmpty()) {
		result.symbol = symbol_name;
	}

	resultsModel_->addResult(result);
}

/**
 * @brief DialogResults::resultCount
 * @return
 */
int DialogResults::resultCount() const {
	return resultsModel_->rowCount();
}

}


