/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef BINARY_SEARCHER_H_20060430_
#define BINARY_SEARCHER_H_20060430_

#include "IPlugin.h"

class QMenu;
class QDialog;

namespace BinarySearcherPlugin {

class BinarySearcher : public QObject, public IPlugin {
	Q_OBJECT
	Q_INTERFACES(IPlugin)
	Q_PLUGIN_METADATA(IID "edb.IPlugin/1.0")
	Q_CLASSINFO("author", "Evan Teran")
	Q_CLASSINFO("url", "http://www.codef00.com")

public:
	explicit BinarySearcher(QObject *parent = nullptr);
	~BinarySearcher() override = default;

public:
	[[nodiscard]] QMenu *menu(QWidget *parent = nullptr) override;
	[[nodiscard]] QList<QAction *> stackContextMenu() override;

public Q_SLOTS:
	void showMenu();
	void mnuStackFindAscii();

private:
	QMenu *menu_ = nullptr;
};

}

#endif
