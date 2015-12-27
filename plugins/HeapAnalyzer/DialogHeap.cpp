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

#include "DialogHeap.h"
#include "Configuration.h"
#include "edb.h"
#include "IDebugger.h"
#include "ISymbolManager.h"
#include "MemoryRegions.h"
#include "Util.h"
#include <QFileInfo>
#include <QHeaderView>
#include <QMessageBox>
#include <QSortFilterProxyModel>
#include <QStack>
#include <QString>
#include <QVector>
#include <QtDebug>
#include <algorithm>
#include <functional>

#ifdef ENABLE_GRAPH
#include "GraphWidget.h"
#include "GraphNode.h"
#include "GraphEdge.h"
#endif

#if QT_VERSION >= 0x050000
#include <QtConcurrent>
#elif QT_VERSION >= 0x040800
#include <QtConcurrentMap>
#endif

#include "ui_DialogHeap.h"

namespace HeapAnalyzer {

#define PREV_INUSE     0x1
#define IS_MMAPPED     0x2
#define NON_MAIN_ARENA 0x4
#define SIZE_BITS (PREV_INUSE|IS_MMAPPED|NON_MAIN_ARENA)

#define next_chunk(p, c) ((p) + ((c).chunk_size()))
#define prev_chunk(p, c) ((p) - ((c).prev_size))

namespace {

// NOTE: the details of this structure are 32/64-bit sensitive!

template<class MallocChunkPtr>
struct malloc_chunk {
	typedef MallocChunkPtr ULong; // ulong has the same size

	ULong prev_size; /* Size of previous chunk (if free).  */
	ULong size;      /* Size in bytes, including overhead. */

	MallocChunkPtr fd; /* double links -- used only if free. */
	MallocChunkPtr bk;

	edb::address_t chunk_size() const { return edb::address_t::fromZeroExtended(size & ~(SIZE_BITS)); }
	bool prev_inuse() const  { return size & PREV_INUSE; }
};

//------------------------------------------------------------------------------
// Name: block_start
// Desc:
//------------------------------------------------------------------------------
edb::address_t block_start(edb::address_t pointer) {
	return pointer + edb::v1::pointer_size() * 2; // pointer_size() is malloc_chunk*
}

//------------------------------------------------------------------------------
// Name: block_start
// Desc:
//------------------------------------------------------------------------------
edb::address_t block_start(const Result &result) {
	return block_start(result.block);
}

}

//------------------------------------------------------------------------------
// Name: DialogHeap
// Desc:
//------------------------------------------------------------------------------
DialogHeap::DialogHeap(QWidget *parent) : QDialog(parent), ui(new Ui::DialogHeap) {
	ui->setupUi(this);

	model_ = new ResultViewModel(this);
	ui->tableView->setModel(model_);

	ui->tableView->verticalHeader()->hide();
#if QT_VERSION >= 0x050000
	ui->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
#else
	ui->tableView->horizontalHeader()->setResizeMode(QHeaderView::ResizeToContents);
#endif

#ifdef ENABLE_GRAPH
	ui->btnGraph->setEnabled(true);
#else
	ui->btnGraph->setEnabled(false);
#endif
}

//------------------------------------------------------------------------------
// Name: ~DialogHeap
// Desc:
//------------------------------------------------------------------------------
DialogHeap::~DialogHeap() {
	delete ui;
}

//------------------------------------------------------------------------------
// Name: showEvent
// Desc:
//------------------------------------------------------------------------------
void DialogHeap::showEvent(QShowEvent *) {
	model_->clearResults();
	ui->progressBar->setValue(0);
}

//------------------------------------------------------------------------------
// Name: on_resultTable_cellDoubleClicked
// Desc:
//------------------------------------------------------------------------------
void DialogHeap::on_tableView_doubleClicked(const QModelIndex &index) {

	// NOTE: remember that if we use a sort filter, we need to map the indexes
	// to get at the data we need

	if(auto item = static_cast<Result *>(index.internalPointer())) {
		edb::v1::dump_data_range(item->block, item->block + item->size, false);
	}
}

//------------------------------------------------------------------------------
// Name: get_library_names
// Desc:
//------------------------------------------------------------------------------
void DialogHeap::get_library_names(QString *libcName, QString *ldName) const {
	
	Q_ASSERT(libcName);
	Q_ASSERT(ldName);
	
	if(edb::v1::debugger_core) {
		if(IProcess *process = edb::v1::debugger_core->process()) {	
			const QList<Module> libs = process->loaded_modules();

			for(const Module &module: libs) {
				if(!ldName->isEmpty() && !libcName->isEmpty()) {
					break;
				}

				const QFileInfo fileinfo(module.name);

				// this tries its best to cover all possible libc library versioning
				// possibilities we need to find out if this is 100% accurate, so far
				// seems correct based on my system

				if(fileinfo.completeBaseName().startsWith("libc-")) {
					*libcName = fileinfo.completeBaseName() + "." + fileinfo.suffix();
					qDebug() << "[Heap Analyzer] libc library appears to be:" << *libcName;
					continue;
				}

				if(fileinfo.completeBaseName().startsWith("libc.so")) {
					*libcName = fileinfo.completeBaseName() + "." + fileinfo.suffix();
					qDebug() << "[Heap Analyzer] libc library appears to be:" << *libcName;
					continue;
				}

				if(fileinfo.completeBaseName().startsWith("ld-")) {
					*ldName = fileinfo.completeBaseName() + "." + fileinfo.suffix();
					qDebug() << "[Heap Analyzer] ld library appears to be:" << *ldName;
					continue;
				}
			}
		}
	}
}

//------------------------------------------------------------------------------
// Name: process_potential_pointer
// Desc:
//------------------------------------------------------------------------------
void DialogHeap::process_potential_pointer(const QHash<edb::address_t, edb::address_t> &targets, Result &result) {
	
	if(IProcess *process = edb::v1::debugger_core->process()) {
		if(result.data.isEmpty()) {
			edb::address_t pointer(0);
			edb::address_t block_ptr = block_start(result);
			edb::address_t block_end = block_ptr + result.size;

			while(block_ptr < block_end) {

				if(process->read_bytes(block_ptr, &pointer, edb::v1::pointer_size())) {
					auto it = targets.find(pointer);
					if(it != targets.end()) {
					#if QT_POINTER_SIZE == 4
						result.data += QString("dword ptr [%1] |").arg(edb::v1::format_pointer(it.key()));
					#elif QT_POINTER_SIZE == 8
						result.data += QString("qword ptr [%1] |").arg(edb::v1::format_pointer(it.key()));
					#endif
					result.points_to.push_back(it.value());
					}
				}

				block_ptr += edb::v1::pointer_size();
			}

			result.data.truncate(result.data.size() - 2);
		}
	}
}

//------------------------------------------------------------------------------
// Name: detect_pointers
// Desc:
//------------------------------------------------------------------------------
void DialogHeap::detect_pointers() {

	qDebug() << "[Heap Analyzer] detecting pointers in heap blocks";

	QHash<edb::address_t, edb::address_t> targets;
	QVector<Result> &results = model_->results();

	// the potential targets
	qDebug() << "[Heap Analyzer] collecting possible targets addresses";
	for(const Result &result: results) {
		edb::address_t block_ptr = block_start(result);
		edb::address_t block_end = block_ptr + result.size;
		while(block_ptr < block_end) {
			targets.insert(block_ptr, result.block);
			block_ptr += edb::v1::pointer_size();
		}
	}

#if QT_VERSION >= 0x040800
	QtConcurrent::blockingMap(results, [this, targets](Result &result) {
		process_potential_pointer(targets, result);
	});

#else
	std::for_each(results.begin(), results.end(), [this, targets](Result &result) {
		process_potential_pointer(targets, result);
	});
#endif

	model_->update();
}

//------------------------------------------------------------------------------
// Name: collect_blocks
// Desc:
//------------------------------------------------------------------------------
template<class Addr>
void DialogHeap::collect_blocks(edb::address_t start_address, edb::address_t end_address) {
	model_->clearResults();

	if(IProcess *process = edb::v1::debugger_core->process()) {
		const int min_string_length = edb::v1::config().min_string_length;

		if(start_address != 0 && end_address != 0) {
	#if defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD) || defined(Q_OS_OPENBSD)
			malloc_chunk<Addr> currentChunk;
			malloc_chunk<Addr> nextChunk;
			edb::address_t currentChunkAddress = start_address;

			model_->setUpdatesEnabled(false);

			const edb::address_t how_many = end_address - start_address;
			while(currentChunkAddress != end_address) {
				// read in the current chunk..
				process->read_bytes(currentChunkAddress, &currentChunk, sizeof(currentChunk));

				// figure out the address of the next chunk
				const edb::address_t nextChunkAddress = next_chunk(currentChunkAddress, currentChunk);

				// is this the last chunk (if so, it's the 'top')
				if(nextChunkAddress == end_address) {

					const Result r(
						currentChunkAddress,
						currentChunk.chunk_size(),
						tr("Top"));

					model_->addResult(r);

				} else {

					// make sure we aren't following a broken heap...
					if(nextChunkAddress > end_address || nextChunkAddress < start_address) {
						break;
					}

					QString data;

					// read in the next chunk
					process->read_bytes(nextChunkAddress, &nextChunk, sizeof(nextChunk));

					// if this block is a container for an ascii string, display it...
					// there is a lot of room for improvement here, but it's a start
					QString asciiData;
					QString utf16Data;
					int asciisz;
					int utf16sz;
					if(edb::v1::get_ascii_string_at_address(
							block_start(currentChunkAddress),
							asciiData,
							min_string_length,
							currentChunk.chunk_size(),
							asciisz)) {

						data = QString("ASCII \"%1\"").arg(asciiData);
					} else if(edb::v1::get_utf16_string_at_address(
							block_start(currentChunkAddress),
							utf16Data,
							min_string_length,
							currentChunk.chunk_size(),
							utf16sz)) {
						data = QString("UTF-16 \"%1\"").arg(utf16Data);
					} else {

						using std::memcmp;

						quint8 bytes[16];
						process->read_bytes(block_start(currentChunkAddress), bytes, sizeof(bytes));

						if(memcmp(bytes, "\x89\x50\x4e\x47", 4) == 0) {
							data = "PNG IMAGE";
						} else if(memcmp(bytes, "\x2f\x2a\x20\x58\x50\x4d\x20\x2a\x2f", 9) == 0) {
							data = "XPM IMAGE";
						} else if(memcmp(bytes, "\x42\x5a", 2) == 0) {
							data = "BZIP FILE";
						} else if(memcmp(bytes, "\x1f\x9d", 2) == 0) {
							data = "COMPRESS FILE";
						} else if(memcmp(bytes, "\x1f\x8b", 2) == 0) {
							data = "GZIP FILE";
						}

					}

					const Result r(
						currentChunkAddress,
						currentChunk.chunk_size() + sizeof(unsigned int),
						nextChunk.prev_inuse() ? tr("Busy") : tr("Free"),
						data);

					model_->addResult(r);
				}

				// avoif self referencing blocks
				if(currentChunkAddress == nextChunkAddress) {
					break;
				}

				currentChunkAddress = nextChunkAddress;

				ui->progressBar->setValue(util::percentage(currentChunkAddress - start_address, how_many));
			}

			detect_pointers();
			model_->setUpdatesEnabled(true);


	#else
		#error "Unsupported Platform"
	#endif
		}
	}
}

//------------------------------------------------------------------------------
// Name: find_heap_start_heuristic
// Desc:
//------------------------------------------------------------------------------
edb::address_t DialogHeap::find_heap_start_heuristic(edb::address_t end_address, size_t offset) const {
	const edb::address_t start_address = end_address - offset;

	const edb::address_t heap_symbol = start_address - 4*edb::v1::pointer_size();

	edb::address_t test_addr(0);
	if(IProcess *process = edb::v1::debugger_core->process()) {
		process->read_bytes(heap_symbol, &test_addr, edb::v1::pointer_size());
	
		if(test_addr != edb::v1::debugger_core->page_size()) {
			return 0;
		}
	
		return start_address;
	}
	
	return 0;
}

//------------------------------------------------------------------------------
// Name: do_find
// Desc:
//------------------------------------------------------------------------------
template<class Addr>
void DialogHeap::do_find() {
	// get both the libc and ld symbols of __curbrk
	// this will be the 'before/after libc' addresses

	if(IProcess *process = edb::v1::debugger_core->process()) {
		edb::address_t start_address = 0;
		edb::address_t end_address   = 0;

		QString libcName;
		QString ldName;

		get_library_names(&libcName, &ldName);
	#if defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD) || defined(Q_OS_OPENBSD)
		Symbol::pointer s = edb::v1::symbol_manager().find(libcName + "::__curbrk");
		if(s) {
			end_address = s->address;
		} else {
			qDebug() << "[Heap Analyzer] __curbrk symbol not found in libc, falling back on heuristic! This may or may not work.";	
		}

		s = edb::v1::symbol_manager().find(ldName + "::__curbrk");
		if(s) {
			start_address = s->address;
		} else {

			qDebug() << "[Heap Analyzer] __curbrk symbol not found in ld, falling back on heuristic! This may or may not work.";

			for(edb::address_t offset = 0x0000; offset != 0x1000; offset += edb::v1::pointer_size()) {
				start_address = find_heap_start_heuristic(end_address, offset);
				if(start_address != 0) {
					break;
				}
			}
		}

		if(start_address != 0 && end_address != 0) {
			qDebug() << "[Heap Analyzer] heap start symbol : " << edb::v1::format_pointer(start_address);
			qDebug() << "[Heap Analyzer] heap end symbol   : " << edb::v1::format_pointer(end_address);

			// read the contents of those symbols
			process->read_bytes(end_address, &end_address, edb::v1::pointer_size());
			process->read_bytes(start_address, &start_address, edb::v1::pointer_size());	
		}

		// just assume it's the bounds of the [heap] memory region for now
		if(start_address == 0 || end_address == 0) {

			const QList<IRegion::pointer> &regions = edb::v1::memory_regions().regions();
			for(IRegion::pointer region: regions) {

				if(region->name() == "[heap]") {

					qDebug() << "Found a memory region named '[heap]', assuming that it provides sane bounds";

					if(start_address == 0) {
						start_address = region->start();
					}

					if(end_address == 0) {
						end_address = region->end();
					}

					break;
				}
			}
		}

		// ok, I give up
		if(start_address == 0 || end_address == 0) {
			QMessageBox::information(this, tr("Could not calculate heap bounds"), tr("Failed to calculate the bounds of the heap."));
			return;
		}	

	#else
		#error "Unsupported Platform"
	#endif

		qDebug() << "[Heap Analyzer] heap start : " << edb::v1::format_pointer(start_address);
		qDebug() << "[Heap Analyzer] heap end   : " << edb::v1::format_pointer(end_address);

		collect_blocks<Addr>(start_address, end_address);
	}
}

//------------------------------------------------------------------------------
// Name: on_btnFind_clicked
// Desc:
//------------------------------------------------------------------------------
void DialogHeap::on_btnFind_clicked() {
	ui->btnFind->setEnabled(false);
	ui->progressBar->setValue(0);
	if(edb::v1::debuggeeIs32Bit())
		do_find<edb::value32>();
	else
		do_find<edb::value64>();
	ui->progressBar->setValue(100);
	ui->btnFind->setEnabled(true);
}

//------------------------------------------------------------------------------
// Name: on_btnGraph_clicked
// Desc:
//------------------------------------------------------------------------------
void DialogHeap::on_btnGraph_clicked() {
#ifdef ENABLE_GRAPH

	auto graph = new GraphWidget(nullptr);
	graph->setAttribute(Qt::WA_DeleteOnClose);


	const QVector<Result> &results = model_->results();

	do {
		QMap<edb::address_t, GraphNode *>     nodes;
		QHash<edb::address_t, const Result *> result_map;
		QStack<const Result *>                result_stack;
		QSet<const Result *>                  seen_results;

		// first we make a nice index for our results, this is likely redundant,
		// but won't take long
		for(const Result &result: results) {
			result_map.insert(result.block, &result);
		}

		// seed our search with the selected blocks
		const QItemSelectionModel *const selModel = ui->tableView->selectionModel();
		const QModelIndexList sel = selModel->selectedRows();
		if(sel.size() != 0) {
			for(const QModelIndex &index: sel) {
				auto item = static_cast<Result *>(index.internalPointer());
				result_stack.push(item);
				seen_results.insert(item);
			}
		}

		while(!result_stack.isEmpty()) {
			const Result *const result = result_stack.pop();
			
			GraphNode *node;
			if(result->type == tr("Busy")) {
				node = new GraphNode(graph, edb::v1::format_pointer(result->block), Qt::lightGray);
			} else {
				node = new GraphNode(graph, edb::v1::format_pointer(result->block), Qt::red);
			}

			nodes.insert(result->block, node);

			for(edb::address_t pointer: result->points_to) {
				const Result *next_result = result_map[pointer];
				if(!seen_results.contains(next_result)) {
					seen_results.insert(next_result);
					result_stack.push(next_result);
				}
			}
		}

		qDebug("[Heap Analyzer] Done Processing %d Nodes", nodes.size());

#if 1
		if(nodes.size() > 5000) {
			qDebug("[Heap Analyzer] Too Many Nodes! (%d)", nodes.size());
			delete graph;
			return;
		}
#endif

		for(const Result *result: result_map) {
			const edb::address_t addr = result->block;
			if(nodes.contains(addr)) {
				for(edb::address_t pointer: result->points_to) {
					new GraphEdge(nodes[addr], nodes[pointer]);
				}
			}
		}

		qDebug("[Heap Analyzer] Done Processing Edges");
	} while(0);


	graph->layout();
	graph->show();
#endif
}

}
