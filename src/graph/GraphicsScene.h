/*
Copyright (C) 2015 - 2023 Evan Teran
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
