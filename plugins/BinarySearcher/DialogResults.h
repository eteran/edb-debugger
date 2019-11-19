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

#ifndef DIALOG_RESULTS_H_20190403_
#define DIALOG_RESULTS_H_20190403_

#include "edb.h"
#include "ui_DialogResults.h"
#include <QDialog>

class QListWidgetItem;

namespace BinarySearcherPlugin {

class DialogResults : public QDialog {
	Q_OBJECT

public:
	enum class RegionType {
		Code,
		Stack,
		Data
	};

public:
	explicit DialogResults(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogResults() override = default;

public:
	void addResult(RegionType region, edb::address_t address);
	int resultCount() const;

public Q_SLOTS:
	void on_listWidget_itemDoubleClicked(QListWidgetItem *);

private:
	Ui::DialogResults ui;
};

}

#endif
