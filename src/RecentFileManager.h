/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef RECENT_FILE_MANAGER_H_20070430_
#define RECENT_FILE_MANAGER_H_20070430_

#include <QByteArray>
#include <QMetaType>
#include <QWidget>
#include <utility>

class QMenu;
class QWidget;
class QString;

class RecentFileManager : public QWidget {
	Q_OBJECT
public:
	using RecentFile = std::pair<QString, QList<QByteArray>>;

public:
	explicit RecentFileManager(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~RecentFileManager() override;

public:
	void addFile(const QString &file, const QList<QByteArray> &args);
	[[nodiscard]] QMenu *createMenu();
	[[nodiscard]] RecentFile mostRecent() const;
	[[nodiscard]] int entryCount() const;

public Q_SLOTS:
	void clear();
	void itemSelected();

Q_SIGNALS:
	void fileSelected(const QString &, const QList<QByteArray> &);

private:
	void update();
	static QString formatEntry(const RecentFile &file);

private:
	QList<RecentFile> files_;
	QMenu *menu_ = nullptr;
};

Q_DECLARE_METATYPE(RecentFileManager::RecentFile)

#endif
