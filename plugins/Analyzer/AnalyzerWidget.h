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

#include <QWidget>
#include <QPixmap>

#ifndef ANALYZER_WIDGET_H_20190412_
#define ANALYZER_WIDGET_H_20190412_

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
	bool mouse_pressed_;
	QPixmap* cache_;
	int cache_num_funcs_;
};

}

#endif
