/*
Copyright (C) 2020 Victorien Molle <biche@biche.re>

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

#ifndef COMMENTS_VIEWER_H_20200519_
#define COMMENTS_VIEWER_H_20200519_

#include "IPlugin.h"

class QMenu;
class QDialog;

namespace CommentsViewerPlugin {

class CommentsViewer : public QObject, public IPlugin {
	Q_OBJECT
	Q_INTERFACES(IPlugin)
	Q_PLUGIN_METADATA(IID "edb.IPlugin/1.0")
	Q_CLASSINFO("author", "Victorien Molle")
	Q_CLASSINFO("email", "biche@biche.re")

public:
	explicit CommentsViewer(QObject *parent = nullptr);
	~CommentsViewer() override;

public:
	QMenu *menu(QWidget *parent = nullptr) override;

public Q_SLOTS:
    void showMenu();

private:
	QMenu *menu_                = nullptr;
    QPointer<QDialog> dialog_ = nullptr;
};

}

#endif
