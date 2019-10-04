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

#ifndef DIALOGMEMORYREGIONS_20061101_H_
#define DIALOGMEMORYREGIONS_20061101_H_

#include <QDialog>

#include <memory>

#include "ui_DialogMemoryRegions.h"

class IRegion;

class QSortFilterProxyModel;
class QModelIndex;

class DialogMemoryRegions : public QDialog {
	Q_OBJECT
public:
    explicit DialogMemoryRegions(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogMemoryRegions() override = default;

private:
    void showEvent(QShowEvent *event) override;

private Q_SLOTS:
	void on_regions_table_customContextMenuRequested(const QPoint &pos);
	void on_regions_table_doubleClicked(const QModelIndex &index);
	void set_access_none();
	void set_access_r();
	void set_access_w();
	void set_access_x();
	void set_access_rw();
	void set_access_rx();
	void set_access_wx();
	void set_access_rwx();
	void view_in_cpu();
	void view_in_stack();
	void view_in_dump();

private:
	std::shared_ptr<IRegion> selected_region() const;
	void set_permissions(bool read, bool write, bool execute);

private:
	Ui::DialogMemoryRegions ui;
	QSortFilterProxyModel * filter_model_;
};

#endif
