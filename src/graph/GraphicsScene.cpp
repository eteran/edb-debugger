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

#include "GraphicsScene.h"
#include <QtDebug>
#include <QGraphicsSceneHelpEvent>

//------------------------------------------------------------------------------
// Name: GraphicsScene
// Desc: constructor
//------------------------------------------------------------------------------
GraphicsScene::GraphicsScene(QObject *parent) : QGraphicsScene(parent) {
}

//------------------------------------------------------------------------------
// Name: GraphicsScene
// Desc: constructor
//------------------------------------------------------------------------------
GraphicsScene::GraphicsScene(const QRectF &sceneRect, QObject *parent) : QGraphicsScene(sceneRect, parent) {
}

//------------------------------------------------------------------------------
// Name: GraphicsScene
// Desc: constructor
//------------------------------------------------------------------------------
GraphicsScene::GraphicsScene(qreal x, qreal y, qreal width, qreal height, QObject *parent) : QGraphicsScene(x, y, width, height, parent) {
}

//------------------------------------------------------------------------------
// Name: GraphicsScene
// Desc: destructor
//------------------------------------------------------------------------------
GraphicsScene::~GraphicsScene() {
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
