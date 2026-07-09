/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DIALOG_REFERENCES_H_20061101_
#define DIALOG_REFERENCES_H_20061101_

#include "IRegion.h"
#include "Types.h"
#include "ui_DialogReferences.h"
#include <QDialog>
#include <QFutureWatcher>
#include <QTimer>

#include <atomic>
#include <vector>

class QListWidgetItem;
class IProcess;

namespace ReferencesPlugin {

class DialogReferences : public QDialog {
	Q_OBJECT

public:
	explicit DialogReferences(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogReferences() override = default;

public Q_SLOTS:
	void on_listWidget_itemDoubleClicked(QListWidgetItem *item);

Q_SIGNALS:
	void updateProgress(int);

private:
	void reject() override;
	void showEvent(QShowEvent *event) override;

private:
	struct RegionScan {
		edb::address_t start = 0;
		edb::address_t end   = 0;
		bool accessible      = false;
	};

	struct ReferenceResult {
		char type              = 0;
		edb::address_t address = 0;
	};

	struct SearchResult {
		std::vector<ReferenceResult> matches;
		bool cancelled        = false;
		bool allocationFailed = false;
	};

	void onFindClicked();
	void onFindFinished();
	void updateProgressBar();
	void setSearchRunning(bool running);
	SearchResult doFind(edb::address_t address, const IProcess *process, const std::vector<RegionScan> &regions, bool skipNoAccess);

private:
	Ui::DialogReferences ui;
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
