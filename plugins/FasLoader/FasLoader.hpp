/*
Copyright (C) 2006 - 2023 * Evan Teran evan.teran@gmail.com
						  * darkprof dark_prof@mail.ru

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

#ifndef FAS_LOADER_HPP_
#define FAS_LOADER_HPP_

#include "Fas/Core.hpp"
#include "IPlugin.h"

class QMenu;

namespace FasLoaderPlugin {

class FasLoader : public QObject, public IPlugin {
	Q_OBJECT
	Q_INTERFACES(IPlugin)
	Q_PLUGIN_METADATA(IID "edb.IPlugin/1.0")
	Q_CLASSINFO("author", "darkprof")
	Q_CLASSINFO("url", "http://www.codef00.com")

public:
	explicit FasLoader(QObject *parent = nullptr);
	~FasLoader() override = default;

public:
	[[nodiscard]] QMenu *menu(QWidget *parent = nullptr) override;

private Q_SLOTS:
	void load();

private:
	QMenu *menu_ = nullptr;
};

}

#endif
