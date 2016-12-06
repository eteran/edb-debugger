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

#ifndef GRAPHWIDGET_20090903_H_
#define GRAPHWIDGET_20090903_H_

#include <QGraphicsView>
#include <QMap>
#include <QString>
#include <graphviz/cgraph.h>
#include <graphviz/gvcext.h>

class QGraphicsScene;
class QContextMenuEvent;
class QMouseEvent;
class QLabel;
class GraphNode;

class GraphWidget : public QGraphicsView {
	Q_OBJECT
	Q_DISABLE_COPY(GraphWidget)

private:
	friend class GraphNode;
	friend class GraphEdge;

public:
	GraphWidget(QWidget* parent = 0);
	virtual ~GraphWidget() override;

public:
	void clear();
	void layout();

public Q_SLOTS:
	void setScale(qreal factor);
	void setHUDNotification(const QString &s, int duration = 1000);

Q_SIGNALS:
	void backgroundContextMenuEvent(QContextMenuEvent* event);
	void nodeContextMenuEvent(QContextMenuEvent* event, GraphNode *node);
	void nodeDoubleClickEvent(QMouseEvent* event, GraphNode *node);
	void zoomEvent(qreal factor, qreal currentScale);

protected:
	virtual void keyPressEvent(QKeyEvent* event) override;
	virtual void keyReleaseEvent(QKeyEvent *event) override;
	virtual void wheelEvent(QWheelEvent* event) override;
	virtual void contextMenuEvent(QContextMenuEvent* event) override;
	virtual void mouseDoubleClickEvent(QMouseEvent* event) override;
	
private:
	void setGraphAttribute(const QString name, const QString value);
	void setNodeAttribute(const QString name, const QString value);
	void setEdgeAttribute(const QString name, const QString value);

private:
	bool                  inLayout_;
	QLayout              *HUDLayout_;
	QLabel               *HUDLabel_;
	GVC_t                *context_;
	Agraph_t             *graph_;
};

#endif

