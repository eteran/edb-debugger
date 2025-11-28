/*
 * Copyright (C) 2015 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef GRAPHICS_SCENE_H_20191119_
#define GRAPHICS_SCENE_H_20191119_

#include <QGraphicsScene>

class GraphicsScene : public QGraphicsScene {
	Q_OBJECT
public:
	explicit GraphicsScene(QObject *parent = nullptr);
	explicit GraphicsScene(const QRectF &sceneRect, QObject *parent = nullptr);
	GraphicsScene(qreal x, qreal y, qreal width, qreal height, QObject *parent = nullptr);
	~GraphicsScene() override = default;

Q_SIGNALS:
	void itemHelpEvent(QGraphicsSceneHelpEvent *helpEvent, QGraphicsItem *item);

protected:
	void helpEvent(QGraphicsSceneHelpEvent *helpEvent) override;
};

#endif
