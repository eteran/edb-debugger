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
