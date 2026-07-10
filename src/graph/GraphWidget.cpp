/*
 * Copyright (C) 2015 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "GraphWidget.h"
#include "GraphEdge.h"
#include "GraphNode.h"
#include "GraphicsScene.h"
#include "GraphvizHelper.h"

#include <QAbstractAnimation>
#include <QDebug>
#include <QGraphicsOpacityEffect>
#include <QGraphicsSceneMouseEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPropertyAnimation>
#include <QScrollBar>
#include <QWheelEvent>

#include <graphviz/cgraph.h>
#include <graphviz/gvc.h>

#include <cmath>

namespace {

constexpr int ScenePadding  = 30000;
constexpr qreal ZoomFactor  = 1.2;
constexpr qreal MinimumZoom = 0.001;
constexpr qreal MaximumZoom = 8.000;
}

namespace {

qreal graph_height(Agraph_t *graph) {
	return GD_bb(graph).UR.y;
}

QPointF to_point(pointf p, qreal gheight) {
	return QPointF(p.x, gheight - p.y);
}

QPointF center_to_origin(const QPointF &p, qreal width, qreal height) {
	return QPointF(p.x() - width / 2, p.y() - height / 2);
}

}

/**
 * @brief Constructs a GraphWidget with the specified parent widget.
 *
 * @param parent The parent widget for this GraphWidget.
 */
GraphWidget::GraphWidget(QWidget *parent)
	: QGraphicsView(parent) {

#if 0
	setViewport(new QGLWidget(QGLFormat(QGL::SampleBuffers)));
#endif
	setDragMode(ScrollHandDrag);

	setScene(new GraphicsScene(this));

	// Setup the HUD
	HUDLabel_ = new QLabel(this);
	HUDLabel_->hide();
	HUDLabel_->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
	HUDLabel_->setFont(QFont(QStringLiteral("FreeSans"), 32));
	HUDLabel_->setAttribute(Qt::WA_TransparentForMouseEvents);

	HUDLayout_ = new QHBoxLayout(this);
	HUDLayout_->addWidget(HUDLabel_);

	context_ = gvContext();
	graph_   = _agopen(QStringLiteral("GraphName"), Agstrictdirected);

	// Set graph attributes
	setGraphAttribute(QStringLiteral("overlap"), QStringLiteral("prism"));
	setGraphAttribute(QStringLiteral("pad"), QStringLiteral("0,2"));
	setGraphAttribute(QStringLiteral("dpi"), QStringLiteral("96,0"));
	setGraphAttribute(QStringLiteral("nodesep"), QStringLiteral("2,5"));
	setGraphAttribute(QStringLiteral("nslimit"), QStringLiteral("1"));
	setGraphAttribute(QStringLiteral("nslimit1"), QStringLiteral("1"));
	setGraphAttribute(QStringLiteral("splines"), QStringLiteral("line")); // ugly but should be much faster

	// Set default attributes for the future nodes
	setNodeAttribute(QStringLiteral("fixedsize"), QStringLiteral("false"));
	setNodeAttribute(QStringLiteral("label"), QStringLiteral(""));
	setNodeAttribute(QStringLiteral("regular"), QStringLiteral("true"));

	// Divide the wanted width by the DPI to get the value in points
	QString nodePtsWidth = QString::number(NodeWidth / _agget(graph_, QStringLiteral("dpi"), QStringLiteral("96,0")).toDouble());
	// GV uses , instead of . for the separator in floats
	setNodeAttribute(QStringLiteral("width"), nodePtsWidth.replace(QLatin1Char('.'), QLatin1String(",")));

	// set font
	QFont font = QFont(QStringLiteral("Arial"));
	setGraphAttribute(QStringLiteral("fontname"), font.family());
	setGraphAttribute(QStringLiteral("fontsize"), QString::number(font.pointSizeF()));

	setNodeAttribute(QStringLiteral("fontname"), font.family());
	setNodeAttribute(QStringLiteral("fontsize"), QString::number(font.pointSizeF()));

	setEdgeAttribute(QStringLiteral("fontname"), font.family());
	setEdgeAttribute(QStringLiteral("fontsize"), QString::number(font.pointSizeF()));
}

/**
 * @brief Sets an attribute for the graph.
 *
 * @param name The name of the attribute.
 * @param value The value of the attribute.
 */
void GraphWidget::setGraphAttribute(QString name, QString value) {
	_agset(graph_, name, value);
}

/**
 * @brief Sets an attribute for the nodes in the graph.
 *
 * @param name The name of the attribute.
 * @param value The value of the attribute.
 */
void GraphWidget::setNodeAttribute(QString name, QString value) {
	_agnodeattr(graph_, name, value);
}

/**
 * @brief Sets an attribute for the edges in the graph.
 *
 * @param name The name of the attribute.
 * @param value The value of the attribute.
 */
void GraphWidget::setEdgeAttribute(QString name, QString value) {
	_agedgeattr(graph_, name, value);
}

/**
 * @brief Sets a notification message to be displayed on the HUD for a specified duration.
 *
 * @param s The notification message to display.
 * @param duration The duration for which the notification should be displayed. Default is 1000 ms.
 */
void GraphWidget::setHUDNotification(const QString &s, std::chrono::milliseconds duration) {

	HUDLabel_->setText(s);
	HUDLabel_->show();

	auto effect = new QGraphicsOpacityEffect(this);
	effect->setOpacity(1);
	HUDLabel_->setGraphicsEffect(effect);

	auto animation = new QPropertyAnimation(this);
	animation->setTargetObject(effect);
	animation->setPropertyName("opacity");
	animation->setDuration(static_cast<int>(duration.count()));
	animation->setStartValue(effect->opacity());
	animation->setEndValue(0);
	animation->setEasingCurve(QEasingCurve::OutQuad);
	animation->start(QAbstractAnimation::DeleteWhenStopped);

	connect(animation, &QPropertyAnimation::finished, HUDLabel_, &QLabel::hide);
}

/**
 * @brief Lays out the graph using the specified layout engine.
 */
void GraphWidget::layout() {

	inLayout_ = true;

	qDebug() << "Starting Layout Engine";

	gvFreeLayout(context_, graph_);
	gvLayout(context_, graph_, "dot");

	for (QGraphicsItem *item : items()) {
		if (auto node = qgraphicsitem_cast<GraphNode *>(item)) {
			qreal gheight = graph_height(graph_);
			if (auto internalNode = node->node_) {
				QPointF point = to_point(ND_coord(internalNode), gheight);
				node->setPos(center_to_origin(point, node->boundingRect().width(), node->boundingRect().height()));
			}
		}
	}

	for (QGraphicsItem *item : items()) {
		if (auto edge = qgraphicsitem_cast<GraphEdge *>(item)) {
			edge->syncState();
		}
	}

	qDebug() << "Layout Complete";

	// make the scene HUGE so it feels like you can just scroll forever
	scene()->setSceneRect(sceneRect().adjusted(-ScenePadding, -ScenePadding, +ScenePadding, +ScenePadding));

	inLayout_ = false;
}

/**
 * @brief Destroys the GraphWidget, freeing the associated graph and context resources.
 */
GraphWidget::~GraphWidget() {
	gvFreeLayout(context_, graph_);
	agclose(graph_);
	gvFreeContext(context_);
}

/**
 * @brief Handles key press events for the GraphWidget, allowing for zooming, rotating, centering, and layout actions based on specific key inputs.
 *
 * @param event The key press event to handle.
 */
void GraphWidget::keyPressEvent(QKeyEvent *event) {

	switch (event->key()) {
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
		if (!event->isAutoRepeat()) {
			setDragMode(QGraphicsView::RubberBandDrag);
		}
		break;
	default:
		break;
	}

	QGraphicsView::keyPressEvent(event);
}

/**
 * @brief Handles key release events for the GraphWidget, allowing for resetting the drag mode when the Control key is released.
 *
 * @param event The key release event to handle.
 */
void GraphWidget::keyReleaseEvent(QKeyEvent *event) {

	switch (event->key()) {
	case Qt::Key_Control:
		if (!event->isAutoRepeat()) {
			setDragMode(QGraphicsView::ScrollHandDrag);
		}
		break;
	default:
		break;
	}

	QGraphicsView::keyReleaseEvent(event);
}

/**
 * @brief Handles wheel events for the GraphWidget, allowing for zooming in and out based on the wheel rotation.
 *
 * @param event The wheel event to handle.
 */
void GraphWidget::wheelEvent(QWheelEvent *event) {
	setScale(std::pow(2.0, +event->angleDelta().y() / 240.0));
}

/**
 * @brief Sets the scale of the view by the specified factor, ensuring that the scale remains within defined minimum and maximum bounds.
 * Emits a zoom event with the new scale factor and the current scale.
 *
 * @param factor The scale factor to apply to the view.
 */
void GraphWidget::setScale(qreal factor) {
	const qreal f = std::sqrt(transform().determinant());
	factor        = qBound(MinimumZoom / f, factor, MaximumZoom / f);
	scale(factor, factor);
	Q_EMIT zoomEvent(factor, f);
}

/**
 * @brief Handles context menu events for the GraphWidget, emitting signals based on the type of item under the cursor (node, edge, or background).
 *
 * @param event The context menu event to handle.
 */
void GraphWidget::contextMenuEvent(QContextMenuEvent *event) {

	QGraphicsItem *const item = itemAt(event->pos());

	if (auto node = qgraphicsitem_cast<GraphNode *>(item)) {
		Q_EMIT nodeContextMenuEvent(event, node);
	} else if (const auto node = qgraphicsitem_cast<QGraphicsLineItem *>(item)) {
		auto parent = node->parentItem();
		if (const auto edge = qgraphicsitem_cast<GraphEdge *>(parent)) {
			Q_UNUSED(edge)
		}
	} else {
		Q_EMIT backgroundContextMenuEvent(event);
	}
}

/**
 * @brief Handles mouse double-click events for the GraphWidget, emitting signals based on the type of item under the cursor (node, edge, or background).
 *
 * @param event The mouse double-click event to handle.
 */
void GraphWidget::mouseDoubleClickEvent(QMouseEvent *event) {

	QGraphicsItem *const item = itemAt(event->pos());

	if (auto node = qgraphicsitem_cast<GraphNode *>(item)) {
		Q_EMIT nodeDoubleClickEvent(event, node);
	} else if (const auto node = qgraphicsitem_cast<QGraphicsLineItem *>(item)) {
		auto parent = node->parentItem();
		if (const auto edge = qgraphicsitem_cast<GraphEdge *>(parent)) {
			Q_UNUSED(edge)
		}
	} else {
	}
}

/**
 * @brief Clears the graph by deleting all items in the scene, effectively removing all nodes and edges from the view.
 */
void GraphWidget::clear() {
	qDeleteAll(scene()->items());
}
