/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "RecentFileManager.h"

#include <QAction>
#include <QFileInfo>
#include <QMenu>
#include <QSettings>

#include <algorithm>

namespace {
constexpr int MaxRecentFiles = 8;
}

/**
 * @brief Constructor for the RecentFileManager class.
 *
 * @param parent The parent widget.
 * @param f The window flags.
 */
RecentFileManager::RecentFileManager(QWidget *parent, Qt::WindowFlags f)
	: QWidget(parent, f) {

	QSettings settings;
	settings.beginGroup("Recent");
	const int size = settings.beginReadArray("recent.files");

	for (int i = 0; i < size; ++i) {
		settings.setArrayIndex(i);
		const auto file = settings.value("file").toString();

		if (file.isEmpty()) {
			continue;
		}

		const int size = settings.beginReadArray("arguments");

		QList<QByteArray> args;
		for (int i = 0; i < size; ++i) {
			settings.setArrayIndex(i);
			args.push_back(settings.value("arg").toByteArray());
		}

		settings.endArray();
		files_.push_back({file, args});
	}

	settings.endArray();
	settings.endGroup();
}

/**
 * @brief Destructor for the RecentFileManager class.
 */
RecentFileManager::~RecentFileManager() {
	QSettings settings;
	settings.beginGroup("Recent");
	settings.beginWriteArray("recent.files");

	for (int i = 0; i < files_.size(); ++i) {
		const auto &[path, arguments] = files_[i];
		settings.setArrayIndex(i);
		settings.setValue("file", QVariant::fromValue(path));
		settings.beginWriteArray("arguments");

		for (int i = 0; i < arguments.size(); ++i) {
			settings.setArrayIndex(i);
			settings.setValue("arg", arguments[i]);
		}
		settings.endArray();
	}

	settings.endArray();
	settings.endGroup();
}

/**
 * @brief Clears the recent file list.
 */
void RecentFileManager::clear() {
	files_.clear();
	if (menu_) {
		menu_->clear();
		menu_->addSeparator();
		menu_->addAction(tr("Clear &Menu"), this, SLOT(clear()));
	}
}

/**
 * @brief Creates the menu for the recent file list.
 *
 * @return The created menu.
 */
QMenu *RecentFileManager::createMenu() {

	if (!menu_) {
		menu_ = new QMenu(this);
		update();
	}

	return menu_;
}

/**
 * @brief Formats a recent file entry for display.
 *
 * @param file The recent file to format.
 * @return The formatted string.
 */
QString RecentFileManager::formatEntry(const RecentFile &file) {
	const auto &[path, args] = file;
	QString str              = path;
	for (const auto &arg : args) {
		str += QStringLiteral(" ") + QString::fromLocal8Bit(arg);
	}
	return str;
}

/**
 * @brief Updates the menu with the current recent file list.
 */
void RecentFileManager::update() {
	if (menu_) {
		menu_->clear();

		for (const auto &file : files_) {
			if (QAction *const action = menu_->addAction(formatEntry(file), this, SLOT(itemSelected()))) {
				action->setData(QVariant::fromValue(file));
			}
		}

		menu_->addSeparator();
		menu_->addAction(tr("Clear &Menu"), this, SLOT(clear()));
	}
}

/**
 * @brief Gets the most recent file from the list.
 *
 * @return The most recent file.
 */
RecentFileManager::RecentFile RecentFileManager::mostRecent() const {
	if (files_.isEmpty()) {
		return {};
	}
	return files_.front();
}

/**
 * @brief Gets the number of entries in the recent file list.
 *
 * @return The number of entries.
 */
int RecentFileManager::entryCount() const {
	return static_cast<int>(files_.size());
}

/**
 * @brief Handles the selection of a recent file from the menu.
 */
void RecentFileManager::itemSelected() {
	if (auto action = qobject_cast<QAction *>(sender())) {
		const auto file          = action->data().value<RecentFile>();
		const auto &[path, args] = file;
		Q_EMIT fileSelected(path, args);
	}
}

/**
 * @brief Adds a file to the recent file list.
 *
 * @param file The file to add.
 * @param args The arguments for the file.
 */
void RecentFileManager::addFile(const QString &file, const QList<QByteArray> &args) {

	QFileInfo fi(file);
	QString path = fi.absoluteFilePath();

	// update recent file list, we remove all entries for this file (if any)
	// and then push the file on the front, ensuring that the recently run
	// entries are higher in the list
	files_.erase(std::remove_if(files_.begin(), files_.end(), [&path](const RecentFile &file) {
					 const auto &[currentPath, args] = file;
					 Q_UNUSED(args)
					 return currentPath == path;
				 }),
				 files_.end());

	files_.push_front({path, args});

	// make sure we don't add more than the max
	while (files_.size() > MaxRecentFiles) {
		files_.pop_back();
	}
	update();
}
