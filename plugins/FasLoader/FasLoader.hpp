#pragma once

#include "IPlugin.h"
#include "Fas/Core.hpp"


class QMenu;

namespace FasLoaderPlugin {

class FasLoader : public QObject, public IPlugin {
	Q_OBJECT
	Q_INTERFACES(IPlugin)
	Q_PLUGIN_METADATA(IID "edb.IPlugin/1.0")
	Q_CLASSINFO("author", "darkprof")
	Q_CLASSINFO("url", "http://www.codef00.com")

public:
	explicit FasLoader (QObject *parent = nullptr);
	~FasLoader () override = default;

public:
	QMenu *menu(QWidget *parent = nullptr) override;
  // QWidget *options_page() override;

public Q_SLOTS:
	void show_menu();

protected:
	void private_init() override;

private:
	void load();

private:
	QMenu                 *menu_          = nullptr;
  // bool                   initial_check_ = true;
  Fas::Core fasCore;
};

}
