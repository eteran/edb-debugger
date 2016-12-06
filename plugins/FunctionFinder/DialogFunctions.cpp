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

#include "DialogFunctions.h"
#include "edb.h"
#include "IAnalyzer.h"
#include "MemoryRegions.h"
#include "ISymbolManager.h"
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

#include "ui_DialogFunctions.h"

#define MIN_REFCOUNT 2

namespace FunctionFinder {

//------------------------------------------------------------------------------
// Name: DialogFunctions
// Desc:
//------------------------------------------------------------------------------
DialogFunctions::DialogFunctions(QWidget *parent) : QDialog(parent), ui(new Ui::DialogFunctions) {
	ui->setupUi(this);
	
#if QT_VERSION >= 0x050000
	ui->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
	ui->tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
#else
	ui->tableView->horizontalHeader()->setResizeMode(QHeaderView::ResizeToContents);
	ui->tableWidget->horizontalHeader()->setResizeMode(QHeaderView::ResizeToContents);
#endif

	filter_model_ = new QSortFilterProxyModel(this);
	connect(ui->txtSearch, SIGNAL(textChanged(const QString &)), filter_model_, SLOT(setFilterFixedString(const QString &)));
	
#ifdef ENABLE_GRAPH
	ui->btnGraph->setEnabled(true);
#else
	ui->btnGraph->setEnabled(false);
#endif	
}

//------------------------------------------------------------------------------
// Name: ~DialogFunctions
// Desc:
//------------------------------------------------------------------------------
DialogFunctions::~DialogFunctions() {
	delete ui;
}

//------------------------------------------------------------------------------
// Name: on_tableWidget_cellDoubleClicked
// Desc: follows the found item in the data view
//------------------------------------------------------------------------------
void DialogFunctions::on_tableWidget_cellDoubleClicked(int row, int column) {
	Q_UNUSED(column);

	QTableWidgetItem *const item = ui->tableWidget->item(row, 0);
	const edb::address_t addr = item->data(Qt::UserRole).toULongLong();
	edb::v1::jump_to_address(addr);
}

//------------------------------------------------------------------------------
// Name: showEvent
// Desc:
//------------------------------------------------------------------------------
void DialogFunctions::showEvent(QShowEvent *) {
	filter_model_->setFilterKeyColumn(3);
	filter_model_->setSourceModel(&edb::v1::memory_regions());
	ui->tableView->setModel(filter_model_);

	ui->progressBar->setValue(0);
	ui->tableWidget->setRowCount(0);
}

//------------------------------------------------------------------------------
// Name: do_find
// Desc:
//------------------------------------------------------------------------------
void DialogFunctions::do_find() {

	if(IAnalyzer *const analyzer = edb::v1::analyzer()) {
		const QItemSelectionModel *const selModel = ui->tableView->selectionModel();
		const QModelIndexList sel = selModel->selectedRows();

		if(sel.size() == 0) {
			QMessageBox::critical(this, tr("No Region Selected"), tr("You must select a region which is to be scanned for functions."));
			return;
		}

		auto analyzer_object = dynamic_cast<QObject *>(analyzer);

		if(analyzer_object) {
			connect(analyzer_object, SIGNAL(update_progress(int)), ui->progressBar, SLOT(setValue(int)));
		}

		ui->tableWidget->setRowCount(0);
		ui->tableWidget->setSortingEnabled(false);

		for(const QModelIndex &selected_item: sel) {

			const QModelIndex index = filter_model_->mapToSource(selected_item);

			// do the search for this region!
			if(auto region = *reinterpret_cast<const IRegion::pointer *>(index.internalPointer())) {

				analyzer->analyze(region);

				const IAnalyzer::FunctionMap &results = analyzer->functions(region);

				for(const Function &info: results) {

					const int row = ui->tableWidget->rowCount();
					ui->tableWidget->insertRow(row);

					// entry point
					auto p = new QTableWidgetItem(edb::v1::format_pointer(info.entry_address()));
					p->setData(Qt::UserRole, info.entry_address());
					ui->tableWidget->setItem(row, 0, p);

					// upper bound of the function
					if(info.reference_count() >= MIN_REFCOUNT) {
						ui->tableWidget->setItem(row, 1, new QTableWidgetItem(edb::v1::format_pointer(info.end_address())));

						auto size_item = new QTableWidgetItem;
						size_item->setData(Qt::DisplayRole, info.end_address() - info.entry_address() + 1);

						ui->tableWidget->setItem(row, 2, size_item);
					}

					// reference count
					auto itemCount = new QTableWidgetItem;
					itemCount->setData(Qt::DisplayRole, info.reference_count());
					ui->tableWidget->setItem(row, 3, itemCount);

					// type
					switch(info.type()) {
					case Function::FUNCTION_THUNK:
						ui->tableWidget->setItem(row, 4, new QTableWidgetItem(tr("Thunk")));
						break;
					case Function::FUNCTION_STANDARD:
						ui->tableWidget->setItem(row, 4, new QTableWidgetItem(tr("Standard Function")));
						break;
					}
					
					
					QString symbol_name = edb::v1::symbol_manager().find_address_name(info.entry_address());
					if(!symbol_name.isEmpty()) {
						ui->tableWidget->setItem(row, 5, new QTableWidgetItem(symbol_name));
					}
				}
			}
		}
		ui->tableWidget->setSortingEnabled(true);

		if(analyzer_object) {
			disconnect(analyzer_object, SIGNAL(update_progress(int)), ui->progressBar, SLOT(setValue(int)));
		}
	}
}

//------------------------------------------------------------------------------
// Name: on_btnFind_clicked
// Desc:
//------------------------------------------------------------------------------
void DialogFunctions::on_btnFind_clicked() {
	ui->btnFind->setEnabled(false);
	ui->progressBar->setValue(0);
	do_find();
	ui->progressBar->setValue(100);
	ui->btnFind->setEnabled(true);
}

//------------------------------------------------------------------------------
// Name: on_btnGraph_clicked
// Desc:
//------------------------------------------------------------------------------
void DialogFunctions::on_btnGraph_clicked() {

	// this code is not ery pretty...
	// but it works!

#ifdef ENABLE_GRAPH

	qDebug("[FunctionFinder] Constructing Graph...");

	QList<QTableWidgetItem *> items = ui->tableWidget->selectedItems();
	
	// each column counts as an item	
	if(items.size() == ui->tableWidget->columnCount()) {
		if(QTableWidgetItem *item = items[0]) {
			const edb::address_t addr = item->data(Qt::UserRole).toULongLong();
			if(IAnalyzer *const analyzer = edb::v1::analyzer()) {
				const IAnalyzer::FunctionMap &functions = analyzer->functions();
				
				
				auto it = functions.find(addr);
				if(it != functions.end()) {
					Function f = *it;
					
					auto graph = new GraphWidget(nullptr);
					graph->setAttribute(Qt::WA_DeleteOnClose);
					
					QMap<edb::address_t, GraphNode *> nodes;
					
					// first create all of the nodes
					for(const BasicBlock &bb : f) {				
						auto node = new GraphNode(graph, bb.toString(), Qt::lightGray);
						nodes.insert(bb.first_address(), node);
					}
					
					// then connect them!
					for(const BasicBlock &bb : f) {
					
					
						if(!bb.empty()) {
						
							auto term = bb.back();							
							auto inst = *term;
							
							if(is_unconditional_jump(inst)) {

								Q_ASSERT(inst.operand_count() >= 1);
								const edb::Operand &op = inst.operands()[0];

								// TODO: we need some heuristic for detecting when this is
								//       a call/ret -> jmp optimization
								if(op.type() == edb::Operand::TYPE_REL) {
									const edb::address_t ea = op.relative_target();
									
									auto from = nodes.find(bb.first_address());
									auto to = nodes.find(ea);
									if(to != nodes.end() && from != nodes.end()) {
										new GraphEdge(from.value(), to.value(), Qt::black);
									}
								}
							} else if(is_conditional_jump(inst)) {

								Q_ASSERT(inst.operand_count() == 1);
								const edb::Operand &op = inst.operands()[0];

								if(op.type() == edb::Operand::TYPE_REL) {
								
									auto from = nodes.find(bb.first_address());
									
									auto to_taken = nodes.find(op.relative_target());
									if(to_taken != nodes.end() && from != nodes.end()) {
										new GraphEdge(from.value(), to_taken.value(), Qt::green);
									}
									
									auto to_skipped = nodes.find(inst.rva() + inst.size());
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
#endif
}

}
