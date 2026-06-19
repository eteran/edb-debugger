/*
 * Copyright (C) 2015 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "GraphEdge.h"
#include "GraphNode.h"
#include "GraphWidget.h"

#include <QGraphicsPolygonItem>
#include <QPainter>
#include <QtDebug>

namespace {

constexpr int LineThickness = 2;
constexpr int EdgeZValue    = 1;

/**
 * @brief
 * @brief Creates an arrow head for the edge.
 * @param line The line to create the arrow for.
 * @param size The size of the arrow.
 * @return The created arrow polygon.
 */
QPolygonF create_arrow(QLineF line, int size) {
	// we want a short line at the END of this line
	line = QLineF(line.p2(), line.p1());
	line.setLength(size);
	line = QLineF(line.p2(), line.p1());

	QLineF n(line.normalVector());
	QPointF o(n.dx() / 3.0, n.dy() / 3.0);

	QPolygonF polygon;
	polygon.append(line.p1() + o);
	polygon.append(line.p2());
	polygon.append(line.p1() - o);
	return polygon;
}

}

/**
 * @brief Constructor for the GraphEdge class.
 * @param from The node the edge starts from.
 * @param to The node the edge points to.
 * @param color The color of the edge.
 * @param parent The parent item for the edge.
 */
GraphEdge::GraphEdge(GraphNode *from, GraphNode *to, QColor color, QGraphicsItem *parent)
	: QGraphicsItemGroup(parent), from_(from), to_(to), graph_(from->graph_), color_(std::move(color)) {

	setFlag(QGraphicsItem::ItemHasNoContents, true);

	Q_ASSERT(from->graph_ == to->graph_);

	// we don't want this object to interfere with the line's events
	setAcceptHoverEvents(false);

	from_->addEdge(this);
	to_->addEdge(this);

	graph_->scene()->addItem(this);

	edge_ = agedge(graph_->graph_, from->node_, to->node_, nullptr, true);
}

/**
 * @brief Destructor for the GraphEdge class.
 */
GraphEdge::~GraphEdge() {

	from_->removeEdge(this);
	to_->removeEdge(this);

	clear();
}

/**
 * @brief
 * @brief Gets the node the edge starts from.
 * @return The node the edge starts from.
 */
GraphNode *GraphEdge::from() const {
	return from_;
}

/**
 * @brief Gets the node the edge points to.
 * @return The node the edge points to.
 */
GraphNode *GraphEdge::to() const {
	return to_;
}

/**
 * @brief Clears the edge.
 */
void GraphEdge::clear() {
	const QList<QGraphicsItem *> items = childItems();
	qDeleteAll(items);
}

/**
 * @brief Shortens the line to the node.
 * @param line The line to shorten.
 * @return The shortened line.
 */
QLineF GraphEdge::shortenLineToNode(QLineF line) {
	QRectF nodeRect = to_->sceneBoundingRect();

	// get the 4 lines tha tmake up the rect
	const QLineF polyLines[4] = {
		QLineF(nodeRect.topLeft(), nodeRect.bottomLeft()),
		QLineF(nodeRect.topLeft(), nodeRect.topRight()),
		QLineF(nodeRect.topRight(), nodeRect.bottomRight()),
		QLineF(nodeRect.bottomRight(), nodeRect.bottomLeft()),
	};

	// for any that intersect, shorten the line appropriately
	for (const QLineF &polyLine : polyLines) {
		QPointF intersectPoint;
		QLineF::IntersectType intersectType = polyLine.intersects(line, &intersectPoint);
		if (intersectType == QLineF::BoundedIntersection) {
			line.setP2(intersectPoint);
		}
	}

	return line;
}

/**
 * @brief Creates a line segment for the edge.
 * @param line The line to create the segment for.
 * @param pen The pen to use for the line.
 * @return The created line item.
 */
QGraphicsLineItem *GraphEdge::createLineSegment(const QLineF &line, const QPen &pen) {
	auto l = new QGraphicsLineItem(line, this);
	l->setPen(pen);
	l->setAcceptHoverEvents(true);
	l->setZValue(EdgeZValue);
	return l;
}

/**
 * @brief Creates a line segment for the edge.
 * @param p1 The start point of the line.
 * @param p2 The end point of the line.
 * @param pen The pen to use for the line.
 * @return The created line item.
 */
QGraphicsLineItem *GraphEdge::createLineSegment(const QPointF &p1, const QPointF &p2, const QPen &pen) {
	return createLineSegment(QLineF(p1, p2), pen);
}

/**
 * @brief Creates the line for the edge.
 */
void GraphEdge::createLine() {
	clear();

	const QPointF p1 = from_->sceneBoundingRect().center();
	const QPointF p2 = to_->sceneBoundingRect().center();
	const QPen pen(lineColor(), lineThickness(), Qt::SolidLine, Qt::RoundCap);
	const QLineF line = shortenLineToNode(QLineF(p1, p2));
	auto segment      = createLineSegment(line, pen);
	addToGroup(segment);
	addArrowHead(line);
}

/**
 * @brief Synchronizes the state of the edge.
 */
void GraphEdge::syncState() {
	createLine();
}

/**
 * @brief Adds an arrow head to the edge.
 * @param line The line to add the arrow head to.
 * @param lineThickness The thickness of the line.
 * @param color The color of the arrow head.
 * @param ZValue The Z value of the arrow head.
 * @return The arrow head item.
 */
QGraphicsPolygonItem *GraphEdge::addArrowHead(const QLineF &line, int lineThickness, const QColor &color, int ZValue) {

	const int arrowHeadSize = std::max(lineThickness * 5, 20);

	QPolygonF arrow = create_arrow(line, arrowHeadSize);
	auto arrowHead  = new QGraphicsPolygonItem(this);
	arrowHead->setPolygon(arrow);
	arrowHead->setPen(QPen(color));
	arrowHead->setBrush(QBrush(color));
	arrowHead->setZValue(ZValue);
	addToGroup(arrowHead);
	return arrowHead;
}

/**
 * @brief Adds an arrow head to the edge.
 * @param line The line to add the arrow head to.
 * @return The arrow head item.
 */
QGraphicsPolygonItem *GraphEdge::addArrowHead(const QLineF &line) {
	return addArrowHead(line, lineThickness(), lineColor(), EdgeZValue);
}

/**
 * @brief Updates the lines of the edge. replaces all of the lines with a single straight line
 */
void GraphEdge::updateLines() {

	if (!childItems().empty()) {
		createLine();
	}
}

/**
 * @brief Gets the thickness of the line.
 * @return The line thickness.
 */
int GraphEdge::lineThickness() const {
	return LineThickness;
}

/**
 * @brief Gets the color of the line.
 * @return The line color.
 */
QColor GraphEdge::lineColor() const {
	return color_;
}
