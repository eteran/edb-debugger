/*
 * Copyright (C) 2015 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
	GraphWidget(const GraphWidget &)            = delete;
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
	void setGraphAttribute(QString name, QString value);
	void setNodeAttribute(QString name, QString value);
	void setEdgeAttribute(QString name, QString value);

private:
	bool inLayout_      = false;
	QLayout *HUDLayout_ = nullptr;
	QLabel *HUDLabel_   = nullptr;
	GVC_t *context_     = nullptr;
	Agraph_t *graph_    = nullptr;
};

#endif
