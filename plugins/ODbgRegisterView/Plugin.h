/*
Copyright (C) 2015 Ruslan Kabatsayev <b7.10110111@gmail.com>

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

#ifndef ODBG_REGISTER_VIEW_PLUGIN_H_20151230
#define ODBG_REGISTER_VIEW_PLUGIN_H_20151230

#include "IPlugin.h"
#include <vector>

class QMainWindow;
class QDockWidget;
class QSettings;

namespace ODbgRegisterView {

class ODBRegView;

class Plugin : public QObject, public IPlugin {
	Q_OBJECT
	Q_INTERFACES(IPlugin)
#if QT_VERSION >= 0x050000
	Q_PLUGIN_METADATA(IID "edb.IPlugin/1.0")
#endif
	Q_CLASSINFO("author", "Ruslan Kabatsayev")
	Q_CLASSINFO("email", "b7.10110111@gmail.com")

	void setupDocks();
	
public:
	Plugin();
	virtual QMenu* menu(QWidget* parent = 0) override;
	virtual QList<QAction*> cpu_context_menu() override;
private:
	QMenu* menu_;
	std::vector<ODBRegView*> registerViews_;
	std::vector<QAction*> menuDeleteRegViewActions_;

	void createRegisterView(QString const& settingsGroup);
	void renumerateDocks() const;
private Q_SLOTS:
	void createRegisterView();
	void saveState() const;
	void expandRSUp(bool checked) const;
	void expandRSDown(bool checked) const;
	void expandLSUp(bool checked) const;
	void expandLSDown(bool checked) const;
	void removeDock(QWidget*);
};

}

#endif
