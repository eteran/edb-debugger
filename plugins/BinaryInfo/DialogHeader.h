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

#ifndef DIALOG_HEADER_H_20190403_
#define DIALOG_HEADER_H_20190403_

#include "IRegion.h"
#include "Types.h"
#include "ui_DialogHeader.h"
#include <QDialog>
#include <memory>

class QStringListModel;
class QSortFilterProxyModel;
class QModelIndex;

namespace BinaryInfoPlugin {

class DialogHeader : public QDialog {
	Q_OBJECT

public:
	explicit DialogHeader(const std::shared_ptr<IRegion> &region, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogHeader() override = default;

private:
	Ui::DialogHeader ui;
};

}

#endif
