/*
 * Copyright (C) 2018 - 2025  Evan Teran <evan.teran@gmail.com>
 * Copyright (C) 2018 darkprof <dark_prof@mail.ru>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
