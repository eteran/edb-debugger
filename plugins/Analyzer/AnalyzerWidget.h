/*
 * Copyright (C) 2013 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef ANALYZER_WIDGET_H_20190412_
#define ANALYZER_WIDGET_H_20190412_

#include <QWidget>
#include <memory>

class QPixmap;

namespace AnalyzerPlugin {

class AnalyzerWidget final : public QWidget {
	Q_OBJECT
public:
	AnalyzerWidget(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());

protected:
	void paintEvent(QPaintEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;

private:
	std::unique_ptr<QPixmap> cache_;
	bool mousePressed_ = false;
	int cacheNumFuncs_ = 0;
};

}

#endif
