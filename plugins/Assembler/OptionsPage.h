/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef ASSEMBLER_OPTIONS_PAGE_H_20130611_
#define ASSEMBLER_OPTIONS_PAGE_H_20130611_

#include "ui_OptionsPage.h"
#include <QWidget>

namespace AssemblerPlugin {

class OptionsPage : public QWidget {
	Q_OBJECT

public:
	explicit OptionsPage(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~OptionsPage() override = default;

public Q_SLOTS:
	void on_assemblerName_currentIndexChanged(const QString &text);

private:
	Ui::OptionsPage ui;
};

}

#endif
