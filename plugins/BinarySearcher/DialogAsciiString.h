/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DIALOG_ASCII_STRING_H_20082201_
#define DIALOG_ASCII_STRING_H_20082201_

#include "Types.h"
#include "ui_DialogAsciiString.h"
#include <QDialog>
#include <QFutureWatcher>
#include <QTimer>

#include <atomic>
#include <vector>

class QListWidgetItem;
class IProcess;

namespace BinarySearcherPlugin {

class DialogAsciiString : public QDialog {
	Q_OBJECT

public:
	explicit DialogAsciiString(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogAsciiString() override = default;

protected:
	void reject() override;
	void showEvent(QShowEvent *event) override;

private:
	struct SearchResult {
		std::vector<edb::address_t> matches;
		bool cancelled        = false;
		bool allocationFailed = false;
	};

	void onFindClicked();
	void onFindFinished();
	void updateProgress();
	void setSearchRunning(bool running);
	SearchResult doFind(const QByteArray &needle, const IProcess *process, edb::address_t start, edb::address_t end);

private:
	Ui::DialogAsciiString ui;
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
