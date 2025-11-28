/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef BOOKMARKS_H_20061122_
#define BOOKMARKS_H_20061122_

#include "BookmarksModel.h"
#include "IPlugin.h"
#include "Types.h"
#include <QVariantList>

namespace BookmarksPlugin {

class BookmarkWidget;

class Bookmarks : public QObject, public IPlugin {
	Q_OBJECT
	Q_INTERFACES(IPlugin)
	Q_PLUGIN_METADATA(IID "edb.IPlugin/1.0")
	Q_CLASSINFO("author", "Evan Teran")
	Q_CLASSINFO("url", "http://www.codef00.com")

public:
	explicit Bookmarks(QObject *parent = nullptr);

public:
	[[nodiscard]] QMenu *menu(QWidget *parent = nullptr) override;
	[[nodiscard]] QList<QAction *> cpuContextMenu() override;

public:
	[[nodiscard]] QVariantMap saveState() const override;
	void restoreState(const QVariantMap &) override;

private:
	void addBookmarkMenu();

private:
	QMenu *menu_                    = nullptr;
	BookmarkWidget *bookmarkWidget_ = nullptr;
};

}

#endif
