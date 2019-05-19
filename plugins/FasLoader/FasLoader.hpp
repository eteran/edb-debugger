#pragma once

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
	explicit FasLoader (QObject *parent = nullptr);
	~FasLoader () override = default;

public:
	QMenu *menu(QWidget *parent = nullptr) override;
  QWidget *options_page() override;

public Q_SLOTS:
	void show_menu();
	// void requestFinished(QNetworkReply *reply);

protected:
	void private_init() override;

private:
	void do_check();
	// void set_proxy(const QUrl &url);

private:
	QMenu                 *menu_          = nullptr;
	// QNetworkAccessManager *network_       = nullptr;
  bool                   initial_check_ = true;
};

}
