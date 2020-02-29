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

#ifndef BINARY_SEARCHER_H_20060430_
#define BINARY_SEARCHER_H_20060430_

#include "IPlugin.h"

class QMenu;
class QDialog;

namespace BinarySearcherPlugin {

class BinarySearcher : public QObject, public IPlugin {
	Q_OBJECT
	Q_INTERFACES(IPlugin)
	Q_PLUGIN_METADATA(IID "edb.IPlugin/1.0")
	Q_CLASSINFO("author", "Evan Teran")
	Q_CLASSINFO("url", "http://www.codef00.com")

public:
	explicit BinarySearcher(QObject *parent = nullptr);
	~BinarySearcher() override = default;

public:
	QMenu *menu(QWidget *parent = nullptr) override;
	QList<QAction *> stackContextMenu(QMenu *parent) override;

public Q_SLOTS:
	void showMenu();
	void mnuStackFindAscii();

private:
	QMenu *menu_ = nullptr;
};

}

#endif
