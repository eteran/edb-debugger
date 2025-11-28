/*
 * Copyright (C) 2015 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef GRAPH_EDGE_H_20090903_
#define GRAPH_EDGE_H_20090903_

#include <QColor>
#include <QGraphicsItemGroup>
#include <graphviz/cgraph.h>
#include <graphviz/gvc.h>

class GraphWidget;
class GraphNode;
class GraphicsLineItem;

class GraphEdge final : public QGraphicsItemGroup {
public:
	GraphEdge(GraphNode *from, GraphNode *to, QColor color = Qt::black, QGraphicsItem *parent = nullptr);
	GraphEdge(const GraphEdge &)            = delete;
	GraphEdge &operator=(const GraphEdge &) = delete;
	~GraphEdge() override;

public:
	enum { Type = UserType + 3 };

	[[nodiscard]] int type() const override {
		// Enable the use of qgraphicsitem_cast with this item.
		return Type;
	}

public:
	[[nodiscard]] GraphNode *from() const;
	[[nodiscard]] GraphNode *to() const;
	void clear();
	void syncState();

public:
	[[nodiscard]] int lineThickness() const;
	[[nodiscard]] QColor lineColor() const;

public:
	void updateLines();

protected:
	void createLine();
	QGraphicsPolygonItem *addArrowHead(const QLineF &line);
	QGraphicsPolygonItem *addArrowHead(const QLineF &line, int lineThickness, const QColor &color, int ZValue);
	QLineF shortenLineToNode(QLineF line);
	QGraphicsLineItem *createLineSegment(const QPointF &p1, const QPointF &p2, const QPen &pen);
	QGraphicsLineItem *createLineSegment(const QLineF &line, const QPen &pen);

protected:
	GraphNode *from_    = nullptr;
	GraphNode *to_      = nullptr;
	GraphWidget *graph_ = nullptr;
	Agedge_t *edge_     = nullptr;
	QColor color_;
};

#endif
