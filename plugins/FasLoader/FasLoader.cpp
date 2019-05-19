/*
Copyright (C) 2006 - 2015 * Evan Teran evan.teran@gmail.com
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

#include "FasLoader.hpp"
#include "OptionsPage.hpp"
#include "edb.h"

#include <QMenu>
#include <QSettings>

namespace FasLoaderPlugin {

FasLoader::FasLoader ( QObject* parent ) 
  : QObject ( parent )
{
  
}

void 
FasLoader::private_init () 
{
  QSettings settings;
	if(settings.value("FasLoader/load_on_start.enabled", true).toBool()) {
		do_check();
	}
}

QMenu*
FasLoader::menu(QWidget* parent) {

	Q_ASSERT(parent);

	if(!menu_) {
		menu_ = new QMenu(tr("FasLoader"), parent);
		menu_->addAction(tr("&Load *.fas symbols"), this, SLOT(show_menu()));
	}

	return menu_;
}

QWidget*
FasLoader::options_page() {
	return new OptionsPage;
}

void 
FasLoader::do_check () 
{
  
}

void 
FasLoader::show_menu() {
	initial_check_ = false;
	do_check();
}

}
