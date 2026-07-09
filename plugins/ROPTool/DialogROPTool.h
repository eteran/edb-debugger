/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DIALOG_ROPTOOL_H_20100817_
#define DIALOG_ROPTOOL_H_20100817_

#include "Instruction.h"
#include "ResultsModel.h"
#include "Types.h"
#include "ui_DialogROPTool.h"

#include <QDialog>
#include <QFutureWatcher>
#include <QList>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QTimer>
#include <memory>
#include <vector>

#include <atomic>

class QListWidgetItem;
class QModelIndex;
class QSortFilterProxyModel;
class QStandardItem;
class QStandardItemModel;
class IProcess;

namespace ROPToolPlugin {

class ResultFilterProxy;
class DialogResults;

class DialogROPTool : public QDialog {
	Q_OBJECT

public:
	explicit DialogROPTool(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogROPTool() override = default;

private:
	using InstructionList = std::vector<std::shared_ptr<edb::Instruction>>;

	struct RegionScan {
		edb::address_t start = 0;
		edb::address_t end   = 0;
		bool accessible      = false;
	};

	struct SearchResult {
		QVector<ResultsModel::Result> results;
		bool cancelled        = false;
		bool allocationFailed = false;
	};

private:
	void reject() override;
	void onFindClicked();
	void onFindFinished();
	void updateProgress();
	void setSearchRunning(bool running);
	SearchResult doFind(const IProcess *process, const QVector<RegionScan> &regions, bool uniqueOnly);

private:
	void showEvent(QShowEvent *event) override;

private:
	Ui::DialogROPTool ui;
	QSortFilterProxyModel *filterModel_ = nullptr;
	QPushButton *buttonFind_            = nullptr;
	QFutureWatcher<SearchResult> searchWatcher_;
	QTimer progressTimer_;
	std::atomic_bool cancelRequested_ = false;
	std::atomic_size_t progressDone_  = 0;
	std::atomic_size_t progressTotal_ = 0;
	bool searchRunning_               = false;
};

}

#endif
