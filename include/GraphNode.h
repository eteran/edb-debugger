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

#ifndef GRAPH_NODE_20090903_H_
#define GRAPH_NODE_20090903_H_

#include <QSet>
#include <QStack>
#include <QGraphicsItem>
#include <QPicture>
#include <graphviz/gvc.h>
#include <graphviz/cgraph.h>

class QVariant;

class GraphWidget;
class GraphEdge;

class GraphNode : public QGraphicsItem {
	Q_DISABLE_COPY(GraphNode)

	friend GraphWidget;
	friend GraphEdge;
public:
	GraphNode(GraphWidget *graph, const QString &text, const QColor &color = Qt::white);
	virtual ~GraphNode() override;

public:
    enum { Type = UserType + 2 };

    int type() const {
        // Enable the use of qgraphicsitem_cast with this item.
        return Type;
    }

public:
	void setFont(const QFont &font);
	QFont font() const;

public:
	virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
	virtual QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
	virtual QRectF boundingRect() const override;

protected:
	virtual void hoverEnterEvent(QGraphicsSceneHoverEvent *e) override;
	virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent *e) override;

private:
	void addEdge(GraphEdge *edge);
	void removeEdge(GraphEdge *edge);
	void drawLabel(const QString &text);

protected:
	QPicture          picture_;
	QColor            color_;
	GraphWidget *     graph_;
	QSet<GraphEdge *> edges_;
	Agnode_t         *node_;
};

#endif
