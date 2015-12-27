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

#include "GraphWidget.h"
#include "GraphNode.h"
#include "GraphEdge.h"
#include "GraphicsScene.h"

#include <QAbstractAnimation>
#include <QDebug>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QGraphicsSceneMouseEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QScrollBar>
#include <QWheelEvent>
#include <graphviz/gvc.h>
#include <graphviz/cgraph.h>

namespace {

const int ScenePadding   = 30000;
const float NodeDistance = 100.0;
const float ZoomFactor   = 1.2;
const float MinimumZoom  = 0.001;
const float MaximumZoom  = 8.000;
const int NodeZValue     = 1;
const int NodeWidth      = 100;
const int NodeHeight     = 50;
}

namespace {

Agraph_t* _agopen(QString name, Agdesc_t kind) {
    return agopen(name.toLocal8Bit().data(), kind, 0);
}

/// Add an alternative value parameter to the method for getting an object's attribute
QString _agget(void *object, QString attr, QString alt = QString())
{
    QString str = agget(object, attr.toLocal8Bit().data());

    if(str == QString())
        return alt;
    else
        return str;
}

/// Directly use agsafeset which always works, contrarily to agset
int _agset(void *object, QString attr, QString value) {
	return agsafeset(
		object, 
		attr.toLocal8Bit().data(),
		value.toLocal8Bit().data(),
		value.toLocal8Bit().data());
}


Agsym_t *_agnodeattr(Agraph_t * g, QString name, QString value) {
	return agattr(
		g, 
		AGNODE, 
		name.toLocal8Bit().data(), 
		value.toLocal8Bit().data());
}

Agsym_t *_agedgeattr(Agraph_t * g, QString name, QString value) {
	return agattr(
		g, 
		AGEDGE, 
		name.toLocal8Bit().data(), 
		value.toLocal8Bit().data());
}

qreal graphHeight(Agraph_t *graph) {
    return GD_bb(graph).UR.y;
}

QPointF toPoint(pointf p, qreal gheight) {
    return QPointF(p.x, gheight - p.y);
}

QPointF centerToOrigin(const QPointF &p, qreal width, qreal height) {
    return QPointF(p.x() - width/2, p.y() - height/2);
}

}

//------------------------------------------------------------------------------
// Name: GraphWidget
// Desc:
//------------------------------------------------------------------------------
GraphWidget::GraphWidget(QWidget *parent) : QGraphicsView(parent), inLayout_(false), HUDLayout_(nullptr), HUDLabel_(nullptr) {

#if 0
	setViewport(new QGLWidget(QGLFormat(QGL::SampleBuffers)));
#endif
	setDragMode(ScrollHandDrag);

	setScene(new GraphicsScene(this));

	// Setup the HUD
	HUDLabel_ = new QLabel(this);
	HUDLabel_->hide();
	HUDLabel_->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
	HUDLabel_->setFont(QFont("FreeSans", 32));
	HUDLabel_->setAttribute(Qt::WA_TransparentForMouseEvents);
	
	HUDLayout_ = new QHBoxLayout(this);
	HUDLayout_->addWidget(HUDLabel_);
	
	context_ = gvContext();
	graph_   = _agopen("GraphName", Agstrictdirected);
	

	//Set graph attributes
	setGraphAttribute("overlap", "prism");
	setGraphAttribute("pad",     "0,2");	
	setGraphAttribute("dpi",     "96,0");
	setGraphAttribute("nodesep", "2,5");	
	setGraphAttribute("nslimit", "1");
	setGraphAttribute("nslimit1", "1");
	setGraphAttribute("splines", "line"); // ugly but should be much faster
		
#if 1
	//Set default attributes for the future nodes
	setNodeAttribute("fixedsize", "false");
	setNodeAttribute("label", "");
	setNodeAttribute("regular", "true");	
#endif
	
    //Divide the wanted width by the DPI to get the value in points
    QString nodePtsWidth = QString("%1").arg(NodeWidth/_agget(graph_, "dpi", "96,0").toDouble());
    //GV uses , instead of . for the separator in floats
    setNodeAttribute("width", nodePtsWidth.replace('.', ","));
	
	
	// set font
	QFont font = QFont("Arial");
    setGraphAttribute("fontname", font.family());
    setGraphAttribute("fontsize", QString("%1").arg(font.pointSizeF()));

    setNodeAttribute("fontname", font.family());
    setNodeAttribute("fontsize", QString("%1").arg(font.pointSizeF()));

    setEdgeAttribute("fontname", font.family());
    setEdgeAttribute("fontsize", QString("%1").arg(font.pointSizeF()));			
}

//------------------------------------------------------------------------------
// Name: setGraphAttribute
// Desc:
//------------------------------------------------------------------------------
void GraphWidget::setGraphAttribute(const QString name, const QString value) {
	_agset(graph_, name, value);
}

//------------------------------------------------------------------------------
// Name: setNodeAttribute
// Desc:
//------------------------------------------------------------------------------
void GraphWidget::setNodeAttribute(const QString name, const QString value) {
	_agnodeattr(graph_, name, value);		
}

//------------------------------------------------------------------------------
// Name: setEdgeAttribute
// Desc:
//------------------------------------------------------------------------------
void GraphWidget::setEdgeAttribute(const QString name, const QString value) {
	_agedgeattr(graph_, name, value);		
}

//------------------------------------------------------------------------------
// Name: layout
// Desc:
//------------------------------------------------------------------------------
void GraphWidget::setHUDNotification(const QString &s, int duration) {

	HUDLabel_->setText(s);
	HUDLabel_->show();

	auto effect = new QGraphicsOpacityEffect(this);
	effect->setOpacity(1);
	HUDLabel_->setGraphicsEffect(effect);
	
	auto animation = new QPropertyAnimation(this);
	animation->setTargetObject(effect);
	animation->setPropertyName("opacity");
	animation->setDuration(duration);
	animation->setStartValue(effect->opacity());
	animation->setEndValue(0);
	animation->setEasingCurve(QEasingCurve::OutQuad);
	animation->start(QAbstractAnimation::DeleteWhenStopped);

	connect(animation, SIGNAL(finished()), HUDLabel_, SLOT(hide()));
}

//------------------------------------------------------------------------------
// Name: layout
// Desc:
//------------------------------------------------------------------------------
void GraphWidget::layout() {

	inLayout_ = true;

	qDebug() << "Starting Layout Engine";
	
#if 1
	gvFreeLayout(context_, graph_);
	gvLayout(context_, graph_, "dot");
	

	Q_FOREACH(QGraphicsItem *item, items()) {
		if(auto node = qgraphicsitem_cast<GraphNode *>(item)) {		
			qreal gheight = graphHeight(graph_);
			if(auto internalNode = node->node_) {	
				QPointF point = toPoint(ND_coord(internalNode), gheight);		
				node->setPos(centerToOrigin(point, node->boundingRect().width(), node->boundingRect().height()));
			}
		}
	}
	
	Q_FOREACH(QGraphicsItem *item, items()) {
		if(auto edge = qgraphicsitem_cast<GraphEdge *>(item)) {		
			edge->syncState();
		}
	}
#endif
	qDebug() << "Layout Complete";
	
	// make the scene HUGE so it feels like you can just scroll forever
	scene()->setSceneRect(sceneRect().adjusted(-ScenePadding, -ScenePadding, +ScenePadding, +ScenePadding));	

	inLayout_ = false;
}

//------------------------------------------------------------------------------
// Name: ~GraphWidget
// Desc:
//------------------------------------------------------------------------------
GraphWidget::~GraphWidget() {
	gvFreeLayout(context_, graph_);
	agclose(graph_);
	gvFreeContext(context_);
}

//------------------------------------------------------------------------------
// Name: keyPressEvent
// Desc:
//------------------------------------------------------------------------------
void GraphWidget::keyPressEvent(QKeyEvent* event) {

	switch(event->key()) {
	case Qt::Key_Plus:
		setScale(ZoomFactor);
		break;
	case Qt::Key_Minus:
		setScale(1.0 / ZoomFactor);
		break;
	case Qt::Key_Asterisk:
		rotate(10.0);
		break;
	case Qt::Key_Slash:
		rotate(-10.0);
		break;
	case Qt::Key_Home:
		centerOn(scene()->sceneRect().center());
		break;
	case Qt::Key_L:
		layout();
		break;
	case Qt::Key_Control:
		if(!event->isAutoRepeat()) {
			setDragMode(QGraphicsView::RubberBandDrag);
			break;
		}
	default:
		QGraphicsView::keyPressEvent(event);
	}
}

//------------------------------------------------------------------------------
// Name: keyReleaseEvent
// Desc:
//------------------------------------------------------------------------------
void GraphWidget::keyReleaseEvent(QKeyEvent *event) {

	switch(event->key()) {
	case Qt::Key_Control:
		if(!event->isAutoRepeat()) {
			setDragMode(QGraphicsView::ScrollHandDrag);
			break;
		}
	default:
		QGraphicsView::keyPressEvent(event);
	}

	QGraphicsView::keyPressEvent(event);
}

//------------------------------------------------------------------------------
// Name: wheelEvent
// Desc:
//------------------------------------------------------------------------------
void GraphWidget::wheelEvent(QWheelEvent* event) {
	setScale(std::pow(2.0, +event->delta() / 240.0));
}

//------------------------------------------------------------------------------
// Name: zoom
// Desc:
//------------------------------------------------------------------------------
void GraphWidget::setScale(qreal factor) {
	const qreal f = std::sqrt(matrix().determinant());
	factor = qBound(MinimumZoom / f, factor, MaximumZoom / f);
	scale(factor, factor);
	zoomEvent(factor, f);
}

//------------------------------------------------------------------------------
// Name: contextMenuEvent
// Desc:
//------------------------------------------------------------------------------
void GraphWidget::contextMenuEvent(QContextMenuEvent* event) {

	QGraphicsItem *const item = itemAt(event->pos());
	
	if(auto node = qgraphicsitem_cast<GraphNode*>(item)) {
		Q_EMIT nodeContextMenuEvent(event, node);
	} else if(const auto node = qgraphicsitem_cast<QGraphicsLineItem *>(item)) {	
		auto parent = node->parentItem();
		if(const auto edge = qgraphicsitem_cast<GraphEdge *>(parent)) {
		}
	} else {
		Q_EMIT backgroundContextMenuEvent(event);
	}
}

//------------------------------------------------------------------------------
// Name: mouseDoubleClickEvent
// Desc:
//------------------------------------------------------------------------------
void GraphWidget::mouseDoubleClickEvent(QMouseEvent* event) {

	QGraphicsItem *const item = itemAt(event->pos());

	if(auto node = qgraphicsitem_cast<GraphNode*>(item)) {
		Q_EMIT nodeDoubleClickEvent(event, node);
	} else if(const auto node = qgraphicsitem_cast<QGraphicsLineItem *>(item)) {	
		auto parent = node->parentItem();
		if(const auto edge = qgraphicsitem_cast<GraphEdge *>(parent)) {
		}
	} else {
	}
}

//------------------------------------------------------------------------------
// Name: clear
// Desc:
//------------------------------------------------------------------------------
void GraphWidget::clear() {
	qDeleteAll(scene()->items());
}


