/*
Copyright (C) 2015 - 2015 Evan Teran
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

#ifndef GRAPH_WIDGET_H_20090903_
#define GRAPH_WIDGET_H_20090903_

#include <QGraphicsView>
#include <graphviz/cgraph.h>
#include <graphviz/gvcext.h>

class GraphNode;
class QContextMenuEvent;
class QGraphicsScene;
class QLabel;
class QMouseEvent;
class QString;

class GraphWidget final : public QGraphicsView {
	Q_OBJECT
	friend class GraphNode;
	friend class GraphEdge;

public:
	GraphWidget(QWidget *parent = nullptr);
	GraphWidget(const GraphWidget &) = delete;
	GraphWidget &operator=(const GraphWidget &) = delete;
	~GraphWidget() override;

public:
	void clear();
	void layout();

public Q_SLOTS:
	void setScale(qreal factor);
	void setHUDNotification(const QString &s, int duration = 1000);

Q_SIGNALS:
	void backgroundContextMenuEvent(QContextMenuEvent *event);
	void nodeContextMenuEvent(QContextMenuEvent *event, GraphNode *node);
	void nodeDoubleClickEvent(QMouseEvent *event, GraphNode *node);
	void zoomEvent(qreal factor, qreal currentScale);

protected:
	void keyPressEvent(QKeyEvent *event) override;
	void keyReleaseEvent(QKeyEvent *event) override;
	void wheelEvent(QWheelEvent *event) override;
	void contextMenuEvent(QContextMenuEvent *event) override;
	void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
	void setGraphAttribute(const QString name, const QString value);
	void setNodeAttribute(const QString name, const QString value);
	void setEdgeAttribute(const QString name, const QString value);

private:
	bool inLayout_      = false;
	QLayout *HUDLayout_ = nullptr;
	QLabel *HUDLabel_   = nullptr;
	GVC_t *context_     = nullptr;
	Agraph_t *graph_    = nullptr;
};

#endif
