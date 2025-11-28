/*
 * Copyright (C) 2015 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "GraphicsScene.h"

#include <QGraphicsSceneHelpEvent>
#include <QtDebug>

//------------------------------------------------------------------------------
// Name: GraphicsScene
// Desc: constructor
//------------------------------------------------------------------------------
GraphicsScene::GraphicsScene(QObject *parent)
	: QGraphicsScene(parent) {
}

//------------------------------------------------------------------------------
// Name: GraphicsScene
// Desc: constructor
//------------------------------------------------------------------------------
GraphicsScene::GraphicsScene(const QRectF &sceneRect, QObject *parent)
	: QGraphicsScene(sceneRect, parent) {
}

//------------------------------------------------------------------------------
// Name: GraphicsScene
// Desc: constructor
//------------------------------------------------------------------------------
GraphicsScene::GraphicsScene(qreal x, qreal y, qreal width, qreal height, QObject *parent)
	: QGraphicsScene(x, y, width, height, parent) {
}

//------------------------------------------------------------------------------
// Name: GraphicsScene
// Desc: tooltip event propagator
//------------------------------------------------------------------------------
void GraphicsScene::helpEvent(QGraphicsSceneHelpEvent *helpEvent) {

	QGraphicsItem *const item = itemAt(helpEvent->scenePos(), QTransform());
	Q_EMIT itemHelpEvent(helpEvent, item);
	helpEvent->ignore();
}
