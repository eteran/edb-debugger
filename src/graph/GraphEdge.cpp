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

#include "GraphEdge.h"
#include "GraphWidget.h"
#include "GraphNode.h"

#include <QtDebug>
#include <QGraphicsPolygonItem>
#include <QPainter>

namespace {

const int LineThickness  = 2;
const int EdgeZValue     = 1;

//------------------------------------------------------------------------------
// Name: createArrow
// Desc:
//------------------------------------------------------------------------------
QPolygonF createArrow(QLineF line, int size) {
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

//------------------------------------------------------------------------------
// Name: GraphEdge
// Desc:
//------------------------------------------------------------------------------
GraphEdge::GraphEdge(GraphNode *from, GraphNode *to, const QColor &color, QGraphicsItem *parent) : QGraphicsItemGroup(parent), from_(from), to_(to), graph_(from->graph_), color_(color) {

	setFlag(QGraphicsItem::ItemHasNoContents, true);

	Q_ASSERT(from->graph_ == to->graph_);

	// we don't want this object to interfere with the line's events
	setAcceptHoverEvents(false);


	from_->addEdge(this);
	to_->addEdge(this);

	graph_->scene()->addItem(this);
	
	edge_ = agedge(graph_->graph_, from->node_, to->node_, nullptr, true);
}

//------------------------------------------------------------------------------
// Name: ~GraphEdge
// Desc:
//------------------------------------------------------------------------------
GraphEdge::~GraphEdge() {

	from_->removeEdge(this);
	to_->removeEdge(this);

	clear();
}

//------------------------------------------------------------------------------
// Name: from
// Desc:
//------------------------------------------------------------------------------
GraphNode *GraphEdge::from() const {
	return from_;
}

//------------------------------------------------------------------------------
// Name: to
// Desc:
//------------------------------------------------------------------------------
GraphNode *GraphEdge::to() const {
	return to_;
}

//------------------------------------------------------------------------------
// Name: GraphEdge
// Desc:
//------------------------------------------------------------------------------
void GraphEdge::clear() {
	const QList<QGraphicsItem *> items = childItems();
	qDeleteAll(items);
}

//------------------------------------------------------------------------------
// Name: shortenLineToNode
// Desc:
//------------------------------------------------------------------------------
QLineF GraphEdge::shortenLineToNode(QLineF line) {
	QRectF nodeRect = to_->sceneBoundingRect();

	// get the 4 lines tha tmake up the rect
	const QLineF polyLines[4] = {
		QLineF(nodeRect.topLeft(),     nodeRect.bottomLeft()),
		QLineF(nodeRect.topLeft(),     nodeRect.topRight()),
		QLineF(nodeRect.topRight(),    nodeRect.bottomRight()),
		QLineF(nodeRect.bottomRight(), nodeRect.bottomLeft())
	};

	// for any that intersect, shorten the line appropriately
	for(int i = 0; i < 4; ++i) {
		QPointF intersectPoint;
		QLineF::IntersectType intersectType = polyLines[i].intersect(line, &intersectPoint);
		if (intersectType == QLineF::BoundedIntersection) {
			line.setP2(intersectPoint);
		}
	}

	return line;
}

//------------------------------------------------------------------------------
// Name: createLineSegment
// Desc:
//------------------------------------------------------------------------------
QGraphicsLineItem *GraphEdge::createLineSegment(const QLineF &line, const QPen &pen) {
	auto l = new QGraphicsLineItem(line, this);
	l->setPen(pen);
	l->setAcceptHoverEvents(true);
	l->setZValue(EdgeZValue);
	return l;
}

//------------------------------------------------------------------------------
// Name: createLineSegment
// Desc:
//------------------------------------------------------------------------------
QGraphicsLineItem *GraphEdge::createLineSegment(const QPointF &p1, const QPointF &p2, const QPen &pen) {
	return createLineSegment(QLineF(p1, p2), pen);
}

//------------------------------------------------------------------------------
// Name: createLine
// Desc:
//------------------------------------------------------------------------------
void GraphEdge::createLine() {
	clear();

	const QPointF p1 = from_->sceneBoundingRect().center();
	const QPointF p2 = to_->sceneBoundingRect().center();
	const QPen pen(lineColor(), lineThickness(), Qt::SolidLine, Qt::RoundCap);
	const QLineF line = shortenLineToNode(QLineF(p1, p2));
	auto segment = createLineSegment(line, pen);
	addToGroup(segment);
	addArrowHead(line);
}

//------------------------------------------------------------------------------
// Name: syncState
// Desc:
//------------------------------------------------------------------------------
void GraphEdge::syncState() {
	createLine();
}

//------------------------------------------------------------------------------
// Name: addArrowHead
// Desc:
//------------------------------------------------------------------------------
QGraphicsPolygonItem *GraphEdge::addArrowHead(const QLineF &line, int lineThickness, const QColor &color, int ZValue) {

	const int arrowHeadSize = qMax(lineThickness * 5, 20);

	QPolygonF arrow = createArrow(line, arrowHeadSize);
	auto arrowHead = new QGraphicsPolygonItem(this);
	arrowHead->setPolygon(arrow);
	arrowHead->setPen(QPen(color));
	arrowHead->setBrush(QBrush(color));
	arrowHead->setZValue(ZValue);
	addToGroup(arrowHead);
	return arrowHead;
}

//------------------------------------------------------------------------------
// Name: addArrowHead
// Desc:
//------------------------------------------------------------------------------
QGraphicsPolygonItem *GraphEdge::addArrowHead(const QLineF &line) {
	return addArrowHead(line, lineThickness(), lineColor(), EdgeZValue);
}

//------------------------------------------------------------------------------
// Name: updateLines
// Desc: replaces all of the lines with a single straight line
//------------------------------------------------------------------------------
void GraphEdge::updateLines() {

	if(!childItems().empty()) {
		createLine();
	}
}

//------------------------------------------------------------------------------
// Name: lineThickness
// Desc:
//------------------------------------------------------------------------------
int GraphEdge::lineThickness() const {
	return LineThickness;
}

//------------------------------------------------------------------------------
// Name: lineColor
// Desc:
//------------------------------------------------------------------------------
QColor GraphEdge::lineColor() const {
	return color_;
}
