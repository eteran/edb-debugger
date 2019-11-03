/*
Copyright (C) 2013 - 2015 Evan Teran
                          evan.teran@gmail.com

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
