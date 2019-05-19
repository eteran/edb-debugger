#include "FasLoader.hpp"
#include "edb.h"

#include <QMenu>

namespace FasLoaderPlugin {

FasLoader::FasLoader ( QObject* parent ) 
  : QObject ( parent )
{
  
}

void 
FasLoader::private_init () 
{
  
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
