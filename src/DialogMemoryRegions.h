/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DIALOG_MEMORY_REGIONS_H_20061101_
#define DIALOG_MEMORY_REGIONS_H_20061101_

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
	void setAccessNone();
	void setAccessR();
	void setAccessW();
	void setAccessX();
	void setAccessRW();
	void setAccessRX();
	void setAccessWX();
	void setAccessRWX();
	void viewInCpu();
	void viewInStack();
	void viewInDump();

private:
	[[nodiscard]] std::shared_ptr<IRegion> selectedRegion() const;
	void setPermissions(bool read, bool write, bool execute);

private:
	Ui::DialogMemoryRegions ui;
	QSortFilterProxyModel *filterModel_ = nullptr;
};

#endif
