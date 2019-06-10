#pragma once

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
	QMenu *menu(QWidget *parent = nullptr) override;

private Q_SLOTS:
	void load();

private:
	QMenu *menu_ = nullptr;
};

}
