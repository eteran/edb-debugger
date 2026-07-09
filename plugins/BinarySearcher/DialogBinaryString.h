/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DIALOG_BINARY_STRING_H_20061101_
#define DIALOG_BINARY_STRING_H_20061101_

#include "Types.h"
#include "ui_DialogBinaryString.h"
#include <QDialog>
#include <QFutureWatcher>
#include <QPointer>
#include <QTimer>

#include <atomic>
#include <vector>

class QListWidgetItem;
class IProcess;

namespace BinarySearcherPlugin {

class DialogResults;

class DialogBinaryString : public QDialog {
	Q_OBJECT

public:
	explicit DialogBinaryString(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogBinaryString() override = default;

private:
	void reject() override;

private:
	struct RegionScan {
		edb::address_t start;
		size_t size;
		bool accessible;
	};

	struct SearchResult {
		std::vector<edb::address_t> matches;
		bool cancelled        = false;
		bool allocationFailed = false;
	};

private:
	void onFindClicked();
	void onFindFinished();
	void updateProgress();
	void setSearchRunning(bool running);
	SearchResult doFind(const QByteArray &needle, const IProcess *process, const std::vector<RegionScan> &regions, size_t pageSize, edb::address_t align, bool skipNoAccess);

private:
	Ui::DialogBinaryString ui;
	QPushButton *buttonFind_ = nullptr;
	QFutureWatcher<SearchResult> searchWatcher_;
	QTimer progressTimer_;
	std::atomic_bool cancelRequested_ = false;
	std::atomic_size_t progressDone_  = 0;
	std::atomic_size_t progressTotal_ = 0;
	bool searchRunning_               = false;
};

}

#endif
