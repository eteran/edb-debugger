/*
 * Copyright (C) 2015 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "GraphicsScene.h"

#include <QGraphicsSceneHelpEvent>
#include <QtDebug>

/**
 * @brief Constructor for the GraphicsScene class.
 *
 * @param parent The parent object.
 */
GraphicsScene::GraphicsScene(QObject *parent)
	: QGraphicsScene(parent) {
}

/**
 * @brief Constructor for the GraphicsScene class.
 *
 * @param sceneRect The rectangle of the scene.
 * @param parent The parent object.
 */
GraphicsScene::GraphicsScene(const QRectF &sceneRect, QObject *parent)
	: QGraphicsScene(sceneRect, parent) {
}

/**
 * @brief Constructor for the GraphicsScene class.
 *
 * @param x The x-coordinate of the scene.
 * @param y The y-coordinate of the scene.
 * @param width The width of the scene.
 * @param height The height of the scene.
 * @param parent The parent object.
 */
GraphicsScene::GraphicsScene(qreal x, qreal y, qreal width, qreal height, QObject *parent)
	: QGraphicsScene(x, y, width, height, parent) {
}

/**
 * @brief Handles the help event for the scene.
 *
 * @param helpEvent The help event.
 */
void GraphicsScene::helpEvent(QGraphicsSceneHelpEvent *helpEvent) {

	QGraphicsItem *const item = itemAt(helpEvent->scenePos(), QTransform());
	Q_EMIT itemHelpEvent(helpEvent, item);
	helpEvent->ignore();
}
