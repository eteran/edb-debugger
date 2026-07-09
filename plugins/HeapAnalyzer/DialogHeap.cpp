/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "DialogHeap.h"
#include "Configuration.h"
#include "IDebugger.h"
#include "IProcess.h"
#include "IRegion.h"
#include "ISymbolManager.h"
#include "MemoryRegions.h"
#include "Module.h"
#include "ResultViewModel.h"
#include "Symbol.h"
#include "edb.h"
#include "util/Math.h"

#ifdef ENABLE_GRAPH
#include "GraphEdge.h"
#include "GraphNode.h"
#include "GraphWidget.h"
#endif

#include <QFileInfo>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QStack>
#include <QString>
#include <QVector>
#include <QtConcurrent/QtConcurrentRun>
#include <QtDebug>
#include <algorithm>
#include <functional>

namespace HeapAnalyzerPlugin {
namespace {

constexpr int PreviousInUse = 0x1;
constexpr int IsMMapped     = 0x2;
constexpr int NonMainArena  = 0x4;

constexpr int SizeBits = (PreviousInUse | IsMMapped | NonMainArena);

// NOTE: the details of this structure are 32/64-bit sensitive!
template <class MallocChunkPtr>
struct malloc_chunk {
	using ULong = MallocChunkPtr; // ulong has the same size

	ULong prevSize; /* Size of previous chunk (if free).  */
	ULong size;     /* Size in bytes, including overhead. */

	MallocChunkPtr fd; /* double links -- used only if free. */
	MallocChunkPtr bk;

	[[nodiscard]] edb::address_t chunkSize() const { return edb::address_t::fromZeroExtended(size & ~(SizeBits)); }
	[[nodiscard]] bool prevInUse() const { return size & PreviousInUse; }
};

template <class Addr>
edb::address_t next_chunk(edb::address_t p, const malloc_chunk<Addr> &c) {
	return p + c.chunkSize();
}

/**
 * @brief Returns the start address of the memory block associated with the given malloc_chunk.
 *
 * @param pointer The address of the malloc_chunk.
 * @return The start address of the memory block.
 */
edb::address_t block_start(edb::address_t pointer) {
	return pointer + edb::v1::pointer_size() * 2; // pointer_size() is malloc_chunk*
}

/**
 * @brief Returns the start address of the memory block associated with the given result.
 *
 * @param result The result for which to get the block start address.
 * @return The start address of the memory block.
 */
edb::address_t block_start(const ResultViewModel::Result &result) {
	return block_start(result.address);
}

}

/**
 * @brief Constructs a DialogHeap object with the specified parent widget and window flags.
 *
 * @param parent The parent widget for this dialog.
 * @param f The window flags for this dialog.
 */
DialogHeap::DialogHeap(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {
	ui.setupUi(this);

	model_ = new ResultViewModel(this);

	filterModel_ = new QSortFilterProxyModel(this);
	connect(ui.lineEdit, &QLineEdit::textChanged, filterModel_, &QSortFilterProxyModel::setFilterFixedString);

	filterModel_->setFilterKeyColumn(3);
	filterModel_->setSourceModel(model_);
	ui.tableView->setModel(filterModel_);

	ui.tableView->verticalHeader()->hide();
	ui.tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

	buttonAnalyze_ = new QPushButton(QIcon::fromTheme(QStringLiteral("edit-find")), tr("Analyze"));
	buttonGraph_   = new QPushButton(QIcon::fromTheme(QStringLiteral("distribute-graph")), tr("&Graph Selected Blocks"));
	connect(buttonAnalyze_, &QPushButton::clicked, this, &DialogHeap::onFindClicked);
	connect(&searchWatcher_, &QFutureWatcher<SearchResult>::finished, this, &DialogHeap::onFindFinished);

	progressTimer_.setInterval(50);
	connect(&progressTimer_, &QTimer::timeout, this, &DialogHeap::updateProgressBar);

	connect(buttonGraph_, &QPushButton::clicked, this, [this]() {
#ifdef ENABLE_GRAPH
		constexpr int MaxNodes = 5000;

		auto graph = new GraphWidget(nullptr);
		graph->setAttribute(Qt::WA_DeleteOnClose);

		do {
			QMap<edb::address_t, GraphNode *> nodes;
			QStack<const ResultViewModel::Result *> result_stack;
			QSet<const ResultViewModel::Result *> seen_results;

			QMap<edb::address_t, const ResultViewModel::Result *> result_map = createResultMap();

			// seed our search with the selected blocks
			const QItemSelectionModel *const selModel = ui.tableView->selectionModel();
			const QModelIndexList sel                 = selModel->selectedRows();
			if (!sel.isEmpty()) {
				for (const QModelIndex &index : sel) {
					const QModelIndex idx = filterModel_->mapToSource(index);
					auto item             = static_cast<ResultViewModel::Result *>(idx.internalPointer());
					result_stack.push(item);
					seen_results.insert(item);
				}
			}

			while (!result_stack.isEmpty()) {
				const ResultViewModel::Result *const result = result_stack.pop();

				auto node = new GraphNode(graph, edb::v1::format_pointer(result->address), result->type == ResultViewModel::Result::Busy ? Qt::lightGray : Qt::red);

				nodes.insert(result->address, node);

				for (edb::address_t pointer : result->pointers) {
					const ResultViewModel::Result *next_result = result_map[pointer];
					if (!seen_results.contains(next_result)) {
						seen_results.insert(next_result);
						result_stack.push(next_result);
					}
				}
			}
			qDebug("[Heap Analyzer] Done Processing %d Nodes", static_cast<int>(nodes.size()));

			if (nodes.size() > MaxNodes) {
				qDebug("[Heap Analyzer] Too Many Nodes! (%d)", static_cast<int>(nodes.size()));
				delete graph;
				return;
			}

			for (const ResultViewModel::Result *result : result_map) {
				const edb::address_t addr = result->address;
				if (nodes.contains(addr)) {
					for (edb::address_t pointer : result->pointers) {
						new GraphEdge(nodes[addr], nodes[pointer]);
					}
				}
			}
			qDebug("[Heap Analyzer] Done Processing Edges");
		} while (0);

		graph->layout();
		graph->show();
#endif
	});

	ui.buttonBox->addButton(buttonGraph_, QDialogButtonBox::ActionRole);
	ui.buttonBox->addButton(buttonAnalyze_, QDialogButtonBox::ActionRole);

#ifdef ENABLE_GRAPH
	buttonGraph_->setEnabled(true);
#else
	buttonGraph_->setEnabled(false);
#endif
}

/**
 * @brief Cancels the active heap analysis instead of closing immediately.
 */
void DialogHeap::reject() {
	if (searchRunning_) {
		cancelRequested_.store(true);
		buttonAnalyze_->setEnabled(false);
		buttonAnalyze_->setText(tr("Cancelling..."));
		return;
	}

	QDialog::reject();
}

/**
 * @brief Handles the heap analyze button click, either starting a new search or cancelling the active one.
 */
void DialogHeap::onFindClicked() {
	if (searchRunning_) {
		cancelRequested_.store(true);
		buttonAnalyze_->setEnabled(false);
		buttonAnalyze_->setText(tr("Cancelling..."));
		return;
	}

	ui.progressBar->setValue(0);
	if (edb::v1::debuggeeIs32Bit()) {
		doFind<edb::value32>();
	} else {
		doFind<edb::value64>();
	}
}

/**
 * @brief Updates the progress bar for the background heap analysis.
 */
void DialogHeap::updateProgressBar() {
	const size_t done  = progressDone_.load();
	const size_t total = progressTotal_.load();

	if (total == 0) {
		ui.progressBar->setValue(0);
		return;
	}

	ui.progressBar->setValue(util::percentage(done, total));
}

/**
 * @brief Handles completion of the background heap analysis.
 */
void DialogHeap::onFindFinished() {
	progressTimer_.stop();
	updateProgressBar();

	const SearchResult result = searchWatcher_.result();
	setSearchRunning(false);

	if (result.cancelled) {
		return;
	}

	model_->clearResults();
	for (const ResultViewModel::Result &entry : result.results) {
		model_->addResult(entry);
	}

	ui.labelFree->setText(tr("Free Blocks: %1").arg(result.freeBlocks));
	ui.labelBusy->setText(tr("Busy Blocks: %1").arg(result.busyBlocks));
	ui.labelTotal->setText(tr("Total: %1").arg(result.freeBlocks + result.busyBlocks));
}

/**
 * @brief Sets the search state and updates the analyze button accordingly.
 */
void DialogHeap::setSearchRunning(bool running) {
	searchRunning_ = running;

	if (searchRunning_) {
		buttonAnalyze_->setEnabled(true);
		buttonAnalyze_->setIcon(QIcon::fromTheme(QStringLiteral("process-stop")));
		buttonAnalyze_->setText(tr("Cancel"));
	} else {
		buttonAnalyze_->setEnabled(true);
		buttonAnalyze_->setIcon(QIcon::fromTheme(QStringLiteral("edit-find")));
		buttonAnalyze_->setText(tr("Analyze"));
		ui.progressBar->setValue(100);
	}
}

/**
 * @brief Handles the show event for the DialogHeap. Clears the results and resets the progress bar.
 */
void DialogHeap::showEvent(QShowEvent *) {
	model_->clearResults();
	ui.progressBar->setValue(0);
}

/**
 * @brief Handles the double-click event on the table view. Dumps the data range of the selected result.
 *
 * @param index The index of the double-clicked item in the table view.
 */
void DialogHeap::on_tableView_doubleClicked(const QModelIndex &index) {
	const QModelIndex idx = filterModel_->mapToSource(index);
	if (auto item = static_cast<ResultViewModel::Result *>(idx.internalPointer())) {
		edb::v1::dump_data_range(item->address, item->address + item->size, false);
	}
}

/**
 * @brief Processes potential pointers in the given targets and updates the model with the found pointers for the specified index.
 *
 * @param targets The hash map of potential pointer targets.
 * @param index The index of the result for which to process pointers.
 */
void DialogHeap::processPotentialPointers(const QHash<edb::address_t, edb::address_t> &targets, const QModelIndex &index) {

	if (auto result = static_cast<ResultViewModel::Result *>(index.internalPointer())) {

		std::vector<edb::address_t> pointers;

		if (IProcess *process = edb::v1::debugger_core->process()) {
			if (result->dataType == ResultViewModel::Result::Unknown) {
				edb::address_t pointer(0);
				edb::address_t block_ptr = block_start(*result);
				edb::address_t block_end = block_ptr + result->size;

				while (block_ptr < block_end) {

					if (process->readBytes(block_ptr, &pointer, edb::v1::pointer_size())) {
						auto it = targets.find(pointer);
						if (it != targets.end()) {
							pointers.push_back(it.value());
						}
					}

					block_ptr += edb::v1::pointer_size();
				}

				if (!pointers.empty()) {
					model_->setPointerData(index, pointers);
				}
			}
		}
	}
}

/**
 * @brief Detects pointers in the heap blocks and updates the model with the found pointers.
 */
void DialogHeap::detectPointers() {

	qDebug() << "[Heap Analyzer] detecting pointers in heap blocks";

	QHash<edb::address_t, edb::address_t> targets;

	qDebug() << "[Heap Analyzer] collecting possible targets addresses";
	for (int row = 0; row < model_->rowCount(); ++row) {
		QModelIndex index = model_->index(row, 0);
		if (auto result = static_cast<ResultViewModel::Result *>(index.internalPointer())) {
			edb::address_t block_ptr = block_start(*result);
			edb::address_t block_end = block_ptr + result->size;
			while (block_ptr < block_end) {
				targets.insert(block_ptr, result->address);
				block_ptr += edb::v1::pointer_size();
			}
		}
	}

	qDebug() << "[Heap Analyzer] linking blocks to target addresses";
	for (int row = 0; row < model_->rowCount(); ++row) {
		QModelIndex index = model_->index(row, 0);
		processPotentialPointers(targets, index);
	}
}

/**
 * @brief Collects heap blocks between the specified start and end addresses and returns them for UI application.
 *
 * @param start_address The starting address of the memory range to collect blocks from.
 * @param end_address The ending address of the memory range to collect blocks from.
 */
template <class Addr>
SearchResult DialogHeap::collectBlocks(edb::address_t start_address, edb::address_t end_address) {
	SearchResult result;

	if (IProcess *process = edb::v1::debugger_core->process()) {
		const int min_string_length = edb::v1::config().min_string_length;

		if (start_address != 0 && end_address != 0) {
#if defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD) || defined(Q_OS_OPENBSD)
			malloc_chunk<Addr> currentChunk;
			malloc_chunk<Addr> nextChunk;
			edb::address_t currentChunkAddress = start_address;

			const edb::address_t how_many = end_address - start_address;
			progressTotal_.store(static_cast<size_t>(how_many));

			while (currentChunkAddress != end_address) {
				if (cancelRequested_.load()) {
					result.cancelled = true;
					break;
				}

				process->readBytes(currentChunkAddress, &currentChunk, sizeof(currentChunk));

				const edb::address_t nextChunkAddress = next_chunk(currentChunkAddress, currentChunk);

				if (nextChunkAddress == end_address) {
					result.results.push_back({currentChunkAddress, currentChunk.chunkSize(), ResultViewModel::Result::Top, ResultViewModel::Result::Unknown, {}, {}});
				} else {
					if (nextChunkAddress > end_address || nextChunkAddress < start_address) {
						break;
					}

					QString data;
					ResultViewModel::Result::DataType data_type = ResultViewModel::Result::Unknown;

					process->readBytes(nextChunkAddress, &nextChunk, sizeof(nextChunk));

					QString asciiData;
					QString utf16Data;
					int asciisz;
					int utf16sz;
					if (edb::v1::get_ascii_string_at_address(
							block_start(currentChunkAddress),
							asciiData,
							min_string_length,
							static_cast<int>(currentChunk.chunkSize()),
							asciisz)) {

						data      = asciiData;
						data_type = ResultViewModel::Result::Ascii;
					} else if (edb::v1::get_utf16_string_at_address(
								   block_start(currentChunkAddress),
								   utf16Data,
								   min_string_length,
								   static_cast<int>(currentChunk.chunkSize()),
								   utf16sz)) {
						data      = utf16Data;
						data_type = ResultViewModel::Result::Utf16;
					} else {
						using std::memcmp;

						uint8_t bytes[16];
						process->readBytes(block_start(currentChunkAddress), bytes, sizeof(bytes));

						if (memcmp(bytes, "\x89\x50\x4e\x47", 4) == 0) {
							data_type = ResultViewModel::Result::Png;
						} else if (memcmp(bytes, R"(/* XPM */)", 9) == 0) {
							data_type = ResultViewModel::Result::Xpm;
						} else if (memcmp(bytes, R"(BZ)", 2) == 0) {
							data_type = ResultViewModel::Result::Bzip;
						} else if (memcmp(bytes, "\x1f\x9d", 2) == 0) {
							data_type = ResultViewModel::Result::Compress;
						} else if (memcmp(bytes, "\x1f\x8b", 2) == 0) {
							data_type = ResultViewModel::Result::Gzip;
						}
					}

					const ResultViewModel::Result r{
						currentChunkAddress,
						currentChunk.chunkSize() + (edb::v1::debuggeeIs64Bit() ? sizeof(uint64_t) : sizeof(uint32_t)),
						nextChunk.prevInUse() ? ResultViewModel::Result::Busy : ResultViewModel::Result::Free,
						data_type,
						data,
						{}};

					if (nextChunk.prevInUse()) {
						++result.busyBlocks;
					} else {
						++result.freeBlocks;
					}

					result.results.push_back(r);
				}

				if (currentChunkAddress == nextChunkAddress) {
					break;
				}

				currentChunkAddress = nextChunkAddress;
				progressDone_.store(static_cast<size_t>(currentChunkAddress - start_address));
			}

			progressDone_.store(static_cast<size_t>(how_many));

			QHash<edb::address_t, edb::address_t> targets;
			for (const ResultViewModel::Result &entry : result.results) {
				edb::address_t block_ptr       = block_start(entry);
				const edb::address_t block_end = block_ptr + entry.size;
				while (block_ptr < block_end) {
					targets.insert(block_ptr, entry.address);
					block_ptr += edb::v1::pointer_size();
				}
			}

			for (ResultViewModel::Result &entry : result.results) {
				if (entry.dataType != ResultViewModel::Result::Unknown) {
					continue;
				}

				edb::address_t block_ptr       = block_start(entry);
				const edb::address_t block_end = block_ptr + entry.size;
				std::vector<edb::address_t> pointers;
				while (block_ptr < block_end) {
					edb::address_t pointer = 0;
					if (process->readBytes(block_ptr, &pointer, edb::v1::pointer_size())) {
						auto it = targets.find(pointer);
						if (it != targets.end()) {
							pointers.push_back(it.value());
						}
					}
					block_ptr += edb::v1::pointer_size();
				}

				if (!pointers.empty()) {
					entry.pointers = pointers;
					entry.dataType = ResultViewModel::Result::Pointer;
				}
			}

#else
#error "Unsupported Platform"
#endif
		}
	}

	return result;
}

/**
 * @brief Finds and analyzes heap blocks in the current process, collecting information about free and busy blocks.
 *        This function uses the appropriate address type (32-bit or 64-bit) based on the architecture of the debuggee process.
 */
template <class Addr>
void DialogHeap::doFind() {
	if (IProcess *process = edb::v1::debugger_core->process()) {
		edb::address_t start_address = 0;
		edb::address_t end_address   = 0;

		edb::address_t heap_symbol_start = 0;
		edb::address_t heap_symbol_end   = 0;

#if defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD) || defined(Q_OS_OPENBSD)
		std::vector<Symbol> curbrk_symbols = edb::v1::symbol_manager().findAll("__curbrk");
		for (const Symbol &sym : curbrk_symbols) {
			if (sym.file.contains("libc")) {
				heap_symbol_end = sym.address;
			} else if (sym.file.contains("ld")) {
				heap_symbol_start = sym.address;
			}
		}

		qDebug() << "[Heap Analyzer] ld __curbrk symbol   : " << edb::v1::format_pointer(heap_symbol_start);
		qDebug() << "[Heap Analyzer] libc __curbrk symbol : " << edb::v1::format_pointer(heap_symbol_end);

		if (heap_symbol_start != 0) {
			process->readBytes(heap_symbol_start, &start_address, edb::v1::pointer_size());
		} else {
			qDebug() << "[Heap Analyzer] __curbrk symbol not found in ld, falling back on heuristic! This may or may not work.";
		}

		if (heap_symbol_end != 0) {
			process->readBytes(heap_symbol_end, &end_address, edb::v1::pointer_size());
		} else {
			qDebug() << "[Heap Analyzer] __curbrk symbol not found in libc, falling back on heuristic! This may or may not work.";
		}

		if (start_address != 0 && end_address != 0) {
			process->readBytes(end_address, &end_address, edb::v1::pointer_size());
			process->readBytes(start_address, &start_address, edb::v1::pointer_size());
		}

		if (start_address == 0 || end_address == 0) {
			const QList<std::shared_ptr<IRegion>> &regions = edb::v1::memory_regions().regions();

			auto it = std::find_if(regions.begin(), regions.end(), [](const std::shared_ptr<IRegion> &region) {
				return region->name() == "[heap]";
			});

			if (it != regions.end()) {
				qDebug() << "Found a memory region named '[heap]', assuming that it provides sane bounds";

				if (start_address == 0) {
					start_address = (*it)->start();
				}

				if (end_address == 0) {
					end_address = (*it)->end();
				}
			}
		}

		if (start_address == 0 || end_address == 0) {
			QMessageBox::critical(this, tr("Could not calculate heap bounds"), tr("Failed to calculate the bounds of the heap."));
			return;
		}

#else
#error "Unsupported Platform"
#endif

		qDebug() << "[Heap Analyzer] heap start : " << edb::v1::format_pointer(start_address);
		qDebug() << "[Heap Analyzer] heap end   : " << edb::v1::format_pointer(end_address);

		ui.progressBar->setValue(0);
		cancelRequested_.store(false);
		progressDone_.store(0);
		progressTotal_.store(static_cast<size_t>(end_address - start_address));
		setSearchRunning(true);
		progressTimer_.start();
		searchWatcher_.setFuture(QtConcurrent::run([this, start_address, end_address]() {
			return collectBlocks<Addr>(start_address, end_address);
		}));
	}
}

/**
 * @brief Creates a map of result addresses to their corresponding ResultViewModel::Result pointers.
 *
 * @return A QMap where the keys are result addresses and the values are pointers to the corresponding ResultViewModel::Result objects.
 */
QMap<edb::address_t, const ResultViewModel::Result *> DialogHeap::createResultMap() const {

	const QVector<ResultViewModel::Result> &results = model_->results();
	QMap<edb::address_t, const ResultViewModel::Result *> result_map;

	// first we make a nice index for our results, this is likely redundant,
	// but won't take long
	for (const ResultViewModel::Result &result : results) {
		result_map.insert(result.address, &result);
	}

	return result_map;
}

}
