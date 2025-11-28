/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HEAP_ANALYZER_H_20060430_
#define HEAP_ANALYZER_H_20060430_

#include "IPlugin.h"

class QMenu;
class QDialog;

namespace HeapAnalyzerPlugin {

class HeapAnalyzer : public QObject, public IPlugin {
	Q_OBJECT
	Q_INTERFACES(IPlugin)
	Q_PLUGIN_METADATA(IID "edb.IPlugin/1.0")
	Q_CLASSINFO("author", "Evan Teran")
	Q_CLASSINFO("url", "http://www.codef00.com")

public:
	explicit HeapAnalyzer(QObject *parent = nullptr);
	~HeapAnalyzer() override;

public:
	[[nodiscard]] QMenu *menu(QWidget *parent = nullptr) override;

public Q_SLOTS:
	void showMenu();

private:
	QMenu *menu_ = nullptr;
	QPointer<QDialog> dialog_;
};

}

#endif
