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

#ifndef DIALOG_ATTACH_20091218_H_
#define DIALOG_ATTACH_20091218_H_

#include "Types.h"
#include <QDialog>

class ProcessModel;
class QSortFilterProxyModel;

namespace Ui { class DialogAttach; }

class DialogAttach : public QDialog {
	Q_OBJECT

public:
	DialogAttach(QWidget *parent = 0);
	virtual ~DialogAttach();

private:
	virtual void showEvent(QShowEvent *event);

private:
	void update_list();

public Q_SLOTS:
	void on_filter_uid_clicked(bool checked);

public:
	edb::pid_t selected_pid(bool *ok) const;

private:
	Ui::DialogAttach *const ui;
	ProcessModel          *process_model_;
	QSortFilterProxyModel *process_filter_;
};

#endif

