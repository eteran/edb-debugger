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

#include "GraphNode.h"
#include "Configuration.h"
#include "GraphEdge.h"
#include "GraphWidget.h"
#include "GraphvizHelper.h"
#include "SyntaxHighlighter.h"
#include "edb.h"

#include <QAbstractTextDocumentLayout>
#include <QGraphicsColorizeEffect>
#include <QPainter>
#include <QPainterPath>
#include <QtDebug>

#include <cmath>

namespace {

const QColor TextColor   = Qt::black;
const QColor BorderColor = Qt::blue;
const QColor SelectColor = Qt::lightGray;
const QString NodeFont   = "Monospace";

}

//------------------------------------------------------------------------------
// Name: GraphNode
// Desc:
//------------------------------------------------------------------------------
GraphNode::GraphNode(GraphWidget *graph, const QString &text, const QColor &color)
	: color_(color), graph_(graph) {

	setFlag(QGraphicsItem::ItemIsMovable, true);
	setFlag(QGraphicsItem::ItemIsSelectable, true);
	setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
	setAcceptHoverEvents(true);
	setCacheMode(QGraphicsItem::DeviceCoordinateCache);
	setZValue(NodeZValue);

	drawLabel(text);

	graph->scene()->addItem(this);

	QString name = QString("Node%1").arg(reinterpret_cast<uintptr_t>(this));
	node_        = _agnode(graph->graph_, name);

	_agset(node_, "fixedsize", "0");
	_agset(node_, "width", QString("%1").arg(boundingRect().width() / 96.0));
	_agset(node_, "height", QString("%1").arg(boundingRect().height() / 96.0));
}

//------------------------------------------------------------------------------
// Name: ~GraphNode
// Desc:
//------------------------------------------------------------------------------
GraphNode::~GraphNode() {

	// NOTE(eteran): we use Q_FOREACH because it operates on a *copy*
	// of the list, which is important because deleting an
	// edge removes it from the list
	Q_FOREACH (GraphEdge *const edge, edges_) {
		delete edge;
	}
}

//------------------------------------------------------------------------------
// Name: boundingRect
// Desc:
//------------------------------------------------------------------------------
QRectF GraphNode::boundingRect() const {
	constexpr int weight = 2;
	const int width      = std::log2(weight) * BorderScaleFactor;
	return picture_.boundingRect().adjusted(-width, -width, +width, +width);
}

//------------------------------------------------------------------------------
// Name: paint
// Desc:
//------------------------------------------------------------------------------
void GraphNode::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {

	Q_UNUSED(option)
	Q_UNUSED(widget)

	painter->save();

	// draw border
	painter->setPen(BorderColor);
	painter->setBrush(BorderColor);
	painter->drawRect(boundingRect());

	// draw background
	painter->setPen(QPen(color_));
	painter->setBrush(QBrush(color_));
	painter->drawRect(picture_.boundingRect());

	if (isSelected()) {
		painter->setPen(QPen(Qt::DashLine));
		painter->drawRect(boundingRect().adjusted(+4, +4, -4, -4));
	}

	// draw contents
	painter->restore();
	picture_.play(painter);
}

//------------------------------------------------------------------------------
// Name: paint
// Desc:
//------------------------------------------------------------------------------
QVariant GraphNode::itemChange(GraphicsItemChange change, const QVariant &value) {
	if (!graph_->inLayout_) {

		switch (change) {
		case ItemPositionChange:
			for (const auto edge : edges_) {
				edge->updateLines();
			}
			break;
		default:
			break;
		}
	}

	return QGraphicsItem::itemChange(change, value);
}

//------------------------------------------------------------------------------
// Name: addEdge
// Desc:
//------------------------------------------------------------------------------
void GraphNode::addEdge(GraphEdge *edge) {
	edges_.insert(edge);
}

//------------------------------------------------------------------------------
// Name: removeEdge
// Desc:
//------------------------------------------------------------------------------
void GraphNode::removeEdge(GraphEdge *edge) {
	edges_.remove(edge);
}

//------------------------------------------------------------------------------
// Name: drawLabel
// Desc:
//------------------------------------------------------------------------------
void GraphNode::drawLabel(const QString &text) {

	const bool syntax_highlighting_enabled = edb::v1::config().syntax_highlighting_enabled;

	QPainter painter(&picture_);
	painter.setBrush(QBrush(color_));
	painter.setPen(TextColor);

	// Since I always just take the points from graph_ and pass them to Qt
	// as pixel I also have to set the pixel size of the font.
	QFont font(NodeFont);
	font.setPixelSize(LabelFontSize);

	if (!font.exactMatch()) {
		QFontInfo fontinfo(font);
		qWarning("replacing font '%s' by font '%s'", qPrintable(font.family()), qPrintable(fontinfo.family()));
	}

	painter.setFont(font);

	QFontMetricsF fm(painter.font());

	// just to calculate the proper bounding box
	QRectF textBoundingRect = fm.boundingRect(QRectF(), Qt::AlignLeft | Qt::AlignTop, text);

	// set some reasonable minimums
	if (textBoundingRect.width() < NodeWidth) {
		textBoundingRect.setWidth(NodeWidth);
	}

	if (textBoundingRect.height() < NodeHeight) {
		textBoundingRect.setHeight(NodeHeight);
	}

	QRectF adjustedBoundingBox = textBoundingRect.adjusted(-2, -2, +2, +2);

	// set the bounding box and then really draw it
	picture_.setBoundingRect(adjustedBoundingBox.toRect());

	if (syntax_highlighting_enabled) {

		// create the text layout
		QTextLayout textLayout(text, painter.font());

		textLayout.setTextOption(QTextOption(Qt::AlignLeft | Qt::AlignTop));

		textLayout.beginLayout();

		int y             = 0;
		QStringList lines = text.split(QLatin1Char('\n'));
		for (const QString &l : lines) {
			QTextLine line = textLayout.createLine();

			if (!line.isValid()) {
				break;
			}

			line.setNumColumns(l.length());
			line.setPosition(QPoint(0, y));
			y += fm.lineSpacing();
		}

		textLayout.endLayout();

		// now the render the text at the location given
		SyntaxHighlighter highlighter;
		textLayout.draw(&painter, adjustedBoundingBox.topLeft(), highlighter.highlightBlock(text), adjustedBoundingBox);
	} else {
		painter.drawText(adjustedBoundingBox, Qt::AlignLeft | Qt::AlignTop, text);
	}
}

//------------------------------------------------------------------------------
// Name: hoverEnterEvent
// Desc:
//------------------------------------------------------------------------------
void GraphNode::hoverEnterEvent(QGraphicsSceneHoverEvent *e) {
	Q_UNUSED(e)
}

//------------------------------------------------------------------------------
// Name: hoverLeaveEvent
// Desc:
//------------------------------------------------------------------------------
void GraphNode::hoverLeaveEvent(QGraphicsSceneHoverEvent *e) {
	Q_UNUSED(e)
}
