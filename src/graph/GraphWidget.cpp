/*
Copyright (C) 2009 - 2014 Evan Teran
                          eteran@alum.rit.edu

Copyright (C) 2009        Arvin Schnell
                          aschnell@suse.de

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


#include <cmath>
#include <climits>

#include <QObject>
#include <QDebug>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QGraphicsSceneMouseEvent>

#include "GraphWidget.h"
#include "GraphNode.h"
#include "GraphEdge.h"

//------------------------------------------------------------------------------
// Name: GraphWidget
// Desc:
//------------------------------------------------------------------------------
GraphWidget::GraphWidget(const QString& filename, const QString& layout, QWidget* parent) : QGraphicsView(parent) {

	setRenderHint(QPainter::Antialiasing);
	setRenderHint(QPainter::TextAntialiasing);
	setTransformationAnchor(AnchorUnderMouse);
	setResizeAnchor(AnchorUnderMouse);

	scene_ = new QGraphicsScene(this);
	scene_->setItemIndexMethod(QGraphicsScene::BspTreeIndex);
	setScene(scene_);

	render_graph(filename, layout);
}

//------------------------------------------------------------------------------
// Name: GraphWidget
// Desc:
//------------------------------------------------------------------------------
GraphWidget::GraphWidget(GVC_t *gvc, graph_t *graph, const QString& layout, QWidget* parent) : QGraphicsView(parent) {

	setRenderHint(QPainter::Antialiasing);
	setRenderHint(QPainter::TextAntialiasing);
	setTransformationAnchor(AnchorUnderMouse);
	setResizeAnchor(AnchorUnderMouse);

	scene_ = new QGraphicsScene(this);
	scene_->setItemIndexMethod(QGraphicsScene::BspTreeIndex);
	setScene(scene_);

	render_graph(gvc, graph, layout);
}

//------------------------------------------------------------------------------
// Name: ~GraphWidget
// Desc:
//------------------------------------------------------------------------------
GraphWidget::~GraphWidget() {
}

//------------------------------------------------------------------------------
// Name: keyPressEvent
// Desc:
//------------------------------------------------------------------------------
void GraphWidget::keyPressEvent(QKeyEvent* event) {
	switch(event->key()) {
	case Qt::Key_Plus:
		scale_view(1.2);
		break;
	case Qt::Key_Minus:
		scale_view(1.0 / 1.2);
		break;
	case Qt::Key_Asterisk:
		rotate(10.0);
		break;
	case Qt::Key_Slash:
		rotate(-10.0);
		break;
	default:
		QGraphicsView::keyPressEvent(event);
	}
}

//------------------------------------------------------------------------------
// Name: wheelEvent
// Desc:
//------------------------------------------------------------------------------
void GraphWidget::wheelEvent(QWheelEvent* event) {
	scale_view(std::pow(2.0, +event->delta() / 240.0));
}

//------------------------------------------------------------------------------
// Name: scale_view
// Desc:
//------------------------------------------------------------------------------
void GraphWidget::scale_view(qreal scaleFactor) {
	const qreal f = std::sqrt(matrix().det());

	scaleFactor = qBound(0.1 / f, scaleFactor, 8.0 / f);

	scale(scaleFactor, scaleFactor);
}

//------------------------------------------------------------------------------
// Name: contextMenuEvent
// Desc:
//------------------------------------------------------------------------------
void GraphWidget::contextMenuEvent(QContextMenuEvent* event) {

	if(GraphNode *const node = qgraphicsitem_cast<GraphNode*>(itemAt(event->pos()))) {
		emit nodeContextMenuEvent(event, node->name);
	} else {
		emit backgroundContextMenuEvent(event);
	}
}

//------------------------------------------------------------------------------
// Name: mouseDoubleClickEvent
// Desc:
//------------------------------------------------------------------------------
void GraphWidget::mouseDoubleClickEvent(QMouseEvent* event) {

	if(GraphNode *const node = qgraphicsitem_cast<GraphNode*>(itemAt(event->pos()))) {
		emit nodeDoubleClickEvent(event, node->name);
	}
}

//------------------------------------------------------------------------------
// Name: gToQ
// Desc:
//------------------------------------------------------------------------------
QPointF GraphWidget::gToQ(const pointf& p, bool upside_down) const {
	return upside_down ? QPointF(p.x, graph_rect_.height() - p.y) : QPointF(p.x, -p.y);
}

//------------------------------------------------------------------------------
// Name: gToQ
// Desc:
//------------------------------------------------------------------------------
QPointF GraphWidget::gToQ(const point& p, bool upside_down) const {
	return upside_down ? QPointF(p.x, graph_rect_.height() - p.y) : QPointF(p.x, -p.y);
}

//------------------------------------------------------------------------------
// Name: aggetToQColor
// Desc:
//------------------------------------------------------------------------------
QColor GraphWidget::aggetToQColor(void *obj, const char *name, const QColor &fallback) const {
	const char *const tmp = agget(obj, const_cast<char*>(name));
	if(!tmp|| strlen(tmp) == 0) {
		return fallback;
	}

	return QColor(tmp);
}

//------------------------------------------------------------------------------
// Name: aggetToQPenStyle
// Desc:
//------------------------------------------------------------------------------
Qt::PenStyle GraphWidget::aggetToQPenStyle(void *obj, const char *name, const Qt::PenStyle fallback) const {
	const char *tmp = agget(obj, const_cast<char*>(name));
	if(!tmp || strlen(tmp) == 0) {
		return fallback;
	}

	if(strcmp(tmp, "dashed") == 0) {
		return Qt::DashLine;
	}

	if(strcmp(tmp, "dotted") == 0) {
		return Qt::DotLine;
	}

	if(strcmp(tmp, "solid") == 0) {
		return Qt::SolidLine;
	}

	return fallback;
}

//------------------------------------------------------------------------------
// Name: render_graph
// Desc:
//------------------------------------------------------------------------------
void GraphWidget::render_graph(const QString& filename, const QString& layout) {
	if(FILE *const fp = fopen(qPrintable(filename), "r")) {
		if(GVC_t *const gvc = gvContext()) {
			if(graph_t *const graph = agread(fp)) {
				render_graph(gvc, graph, layout);
				agclose(graph);
			} else {
				qCritical("agread() failed");
			}
			gvFreeContext(gvc);
		} else {
			qCritical("gvContext() failed");
		}
		fclose(fp);
	} else {
		qCritical("failed to open %s", qPrintable(filename));
	}
}

//------------------------------------------------------------------------------
// Name: render_sub_graph
// Desc:
//------------------------------------------------------------------------------
void GraphWidget::render_sub_graph(graph_t *graph) {

	if(graph) {
		for(int i = 1; i <= GD_n_cluster(graph); ++i) {
			if(graph_t *const sub_graph = GD_clust(graph)[i]) {
				render_sub_graph(sub_graph);
			}
		}

		render_label(graph);

		const QRectF sub_graph_rect(
			gToQ(GD_bb(graph).LL),
			gToQ(GD_bb(graph).UR)
			);

		QPen graphPen(aggetToQColor(graph, "color", Qt::white));

		const QString style = QString::fromUtf8(agget(graph, const_cast<char *>("style")));
		const QBrush graphBrush(
			style.contains("filled") ? aggetToQColor(graph, "color", Qt::white) : QBrush()
			);

		QGraphicsRectItem *const item = scene_->addRect(sub_graph_rect, graphPen, graphBrush);
		item->setZValue(INT_MIN);
	}
}

//------------------------------------------------------------------------------
// Name: render_edge
// Desc:
//------------------------------------------------------------------------------
void GraphWidget::render_edge(edge_t *edge) {

	if(const splines *const spl = ED_spl(edge)) {
		for (int i = 0; i < spl->size; ++i) {
			const bezier &bz = spl->list[i];
			const QColor color(aggetToQColor(edge, "color", Qt::black));

			GraphEdge *const item = new GraphEdge(this, bz, color);

			QPen pen(color);
			pen.setStyle(aggetToQPenStyle(edge, "style", Qt::SolidLine));
			pen.setWidthF(1.0);
			item->setPen(pen);

			item->setZValue(-1.0);
			scene_->addItem(item);
		}
	}
}

//------------------------------------------------------------------------------
// Name: render_node
// Desc:
//------------------------------------------------------------------------------
void GraphWidget::render_node(graph_t *graph, node_t *node) {

	const QString node_style = QString::fromUtf8(agget(node, const_cast<char *>("style")));
	const QStringList node_styles = node_style.split(",");

	if(!node_styles.contains("invisible")) {

		GraphNode *const item = new GraphNode(this, node);
		item->setZValue(1.0);
#ifdef ND_coord_i
		item->setPos(gToQ(ND_coord_i(node)));
#else
		item->setPos(gToQ(ND_coord(node)));
#endif

		QPen pen(aggetToQColor(node, "color", Qt::black));
		pen.setWidthF(1.0);
		item->setPen(pen);

		if(node_styles.contains("filled")) {
			QString fill_color = QString::fromUtf8(agget(node, const_cast<char *>("fillcolor")));
			if(fill_color.isEmpty()) {
				fill_color = QString::fromUtf8(agget(node, const_cast<char *>("color")));
			}

			QColor color(fill_color);
			if(color.isValid()) {
				item->setBrush(QBrush(color));
			} else {
				item->setBrush(QBrush(Qt::gray));
			}
		}

		QString tooltip = QString::fromUtf8(agget(node, const_cast<char *>("tooltip")));
		if(!tooltip.isEmpty()) {
			tooltip.replace("\\n", "\n");
			item->setToolTip(tooltip);
		}

		scene_->addItem(item);

		for(edge_t *e = agfstout(graph, node); e; e = agnxtout(graph, e)) {
			render_edge(e);
		}
	}
}

//------------------------------------------------------------------------------
// Name: render_graph
// Desc:
//------------------------------------------------------------------------------
void GraphWidget::render_graph(GVC_t *gvc, graph_t *graph, const QString& layout) {

	if(gvLayout(gvc, graph, const_cast<char *>(qPrintable(layout))) == 0) {
		clear_graph();
		if(GD_charset(graph)) {
			qWarning("unsupported charset");
		}

		// don't use gToQ here since it adjusts the values
		graph_rect_ = QRectF(
			GD_bb(graph).LL.x,
			GD_bb(graph).LL.y,
			GD_bb(graph).UR.x,
			GD_bb(graph).UR.y);

		scene_->setSceneRect(graph_rect_.adjusted(-5, -5, +5, +5));
		scene_->setBackgroundBrush(aggetToQColor(graph, "bgcolor", Qt::white));

		render_sub_graph(graph);

		// the list of nodes includes sub-graphs
		for(node_t *n = agfstnode(graph); n; n = agnxtnode(graph, n)) {
			render_node(graph, n);
		}
	} else {
		qCritical("gvLayout() failed");
	}
}

//------------------------------------------------------------------------------
// Name: render_label
// Desc:
//------------------------------------------------------------------------------
void GraphWidget::render_label(const graph_t *graph) {

	if(const textlabel_t *const textlabel = GD_label(graph)) {
		// Since I always just take the points from graph and pass them to Qt
		// as pixel I also have to set the pixel size of the font.
		QFont font(textlabel->fontname, textlabel->fontsize);
		font.setPixelSize(textlabel->fontsize);

		if(!font.exactMatch()) {
			QFontInfo fontinfo(font);
			qWarning("replacing font \"%s\" by font \"%s\"", qPrintable(font.family()), qPrintable(fontinfo.family()));
		}

		QGraphicsTextItem *const item = scene_->addText(QString::fromUtf8(textlabel->text), font);

		const QRectF sub_graph_rect(
			gToQ(GD_bb(graph).LL, true),
			gToQ(GD_bb(graph).UR, true)
			);

		QPointF p = gToQ(textlabel->pos, true);
		
		// I don't know why this adjustment is necessary :-/
		p -= QPointF(40.0, 10.0);
				
		item->setPos(p);
		item->setZValue(INT_MAX);
	}
}

//------------------------------------------------------------------------------
// Name: clear_graph
// Desc:
//------------------------------------------------------------------------------
void GraphWidget::clear_graph() {
	qDeleteAll(scene_->items());
}
