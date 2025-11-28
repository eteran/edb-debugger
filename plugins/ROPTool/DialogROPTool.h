/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DIALOG_ROPTOOL_H_20100817_
#define DIALOG_ROPTOOL_H_20100817_

#include "Instruction.h"
#include "Types.h"
#include "ui_DialogROPTool.h"

#include <QDialog>
#include <QList>
#include <QSet>
#include <QSortFilterProxyModel>
#include <memory>
#include <vector>

class QListWidgetItem;
class QModelIndex;
class QSortFilterProxyModel;
class QStandardItem;
class QStandardItemModel;

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

private:
	void doFind();
	void addGadget(DialogResults *results, const InstructionList &instructions);

private:
	void showEvent(QShowEvent *event) override;

private:
	Ui::DialogROPTool ui;
	QSortFilterProxyModel *filterModel_ = nullptr;
	QSet<QString> uniqueResults_;
	QPushButton *buttonFind_ = nullptr;
};

}

#endif
