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

#ifndef ODBG_REGISTER_VIEW_PLUGIN_H_20151230_
#define ODBG_REGISTER_VIEW_PLUGIN_H_20151230_

#include "IPlugin.h"
#include <vector>

namespace ODbgRegisterView {

class ODBRegView;

class Plugin : public QObject, public IPlugin {
	Q_OBJECT
	Q_INTERFACES(IPlugin)
	Q_PLUGIN_METADATA(IID "edb.IPlugin/1.0")
	Q_CLASSINFO("author", "Ruslan Kabatsayev")
	Q_CLASSINFO("email", "b7.10110111@gmail.com")

public:
	explicit Plugin(QObject *parent = nullptr);
	QMenu *menu(QWidget *parent = nullptr) override;

private:
	void setupDocks();
	void createRegisterView(const QString &settingsGroup);
	void renumerateDocks() const;
	void removeDock(QWidget *);
	void saveSettings() const;
	void expandRSUp(bool checked) const;
	void expandRSDown(bool checked) const;
	void expandLSUp(bool checked) const;
	void expandLSDown(bool checked) const;
	QString dockName() const;

private Q_SLOTS:
	void createRegisterView();

private:
	QMenu *menu_ = nullptr;
	std::vector<ODBRegView *> registerViews_;
	std::vector<QAction *> menuDeleteRegViewActions_;
};

}

#endif
