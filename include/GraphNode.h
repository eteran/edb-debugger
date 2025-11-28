/*
 * Copyright (C) 2015 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef GRAPH_NODE_H_20090903_
#define GRAPH_NODE_H_20090903_

#include <QGraphicsItem>
#include <QPicture>
#include <QSet>
#include <graphviz/cgraph.h>
#include <graphviz/gvc.h>

class QVariant;

class GraphWidget;
class GraphEdge;

constexpr int NodeZValue        = 1;
constexpr int NodeWidth         = 100;
constexpr int NodeHeight        = 50;
constexpr int LabelFontSize     = 10;
constexpr int BorderScaleFactor = 4;

class GraphNode final : public QGraphicsItem {
	friend class GraphWidget;
	friend class GraphEdge;

public:
	GraphNode(GraphWidget *graph, const QString &text, QColor color = Qt::white);
	GraphNode(const GraphNode &)            = delete;
	GraphNode &operator=(const GraphNode &) = delete;
	~GraphNode() override;

public:
	enum { Type = UserType + 2 };

	[[nodiscard]] int type() const override {
		// Enable the use of qgraphicsitem_cast with this item.
		return Type;
	}

public:
	void setFont(const QFont &font);
	[[nodiscard]] QFont font() const;

public:
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
	QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
	[[nodiscard]] QRectF boundingRect() const override;

protected:
	void hoverEnterEvent(QGraphicsSceneHoverEvent *e) override;
	void hoverLeaveEvent(QGraphicsSceneHoverEvent *e) override;

private:
	void addEdge(GraphEdge *edge);
	void removeEdge(GraphEdge *edge);
	void drawLabel(const QString &text);

protected:
	QPicture picture_;
	QColor color_;
	GraphWidget *graph_ = nullptr;
	QSet<GraphEdge *> edges_;
	Agnode_t *node_ = nullptr;
};

#endif
