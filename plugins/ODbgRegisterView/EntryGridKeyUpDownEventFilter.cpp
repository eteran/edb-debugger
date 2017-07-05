#include "EntryGridKeyUpDownEventFilter.h"

#include <cassert>
#include <algorithm>
#include <vector>
#include <QWidget>
#include <QKeyEvent>
#include <QLineEdit>

namespace ODbgRegisterView {

bool entryGridKeyUpDownEventFilter(QWidget* parent, QObject* obj, QEvent* event)
{

	auto*const entry=qobject_cast<QLineEdit*>(obj);
	if(!entry) return false;
	if(event->type()!=QEvent::KeyPress) return false;

	auto*const keyEvent=static_cast<QKeyEvent*>(event);
	const auto key=keyEvent->key();
	if(key!=Qt::Key_Up && key!=Qt::Key_Down) return false;

	// subtraction of 1 from x prevents selection of entry to the right-top/bottom instead directly top/bottom
	const auto pos=entry->pos()-QPoint(1,0);
	const auto children=parent->findChildren<QLineEdit*>();
	// Find the neighbors above/below the current entry
	std::vector<QLineEdit*> neighbors;
	for(auto*const child : children) {

		if(!child->isVisible()) continue;
		if(key==Qt::Key_Up && child->y() >= pos.y()) continue;
		if(key==Qt::Key_Down && child->y() <= pos.y()) continue;
		neighbors.emplace_back(child);
	}
	if(neighbors.empty()) return false;
	// Bring the vertically closest neighbors to the front
	const auto y=pos.y();
	std::sort(neighbors.begin(),neighbors.end(), [y](QLineEdit*a,QLineEdit*b)
			{ return std::abs(y - a->y()) < std::abs(y - b->y()); });
	const auto verticallyClosestY=neighbors.front()->y();
	// Remove those too far vertically, so that they don't interfere with later calculations
	neighbors.erase(std::remove_if(neighbors.begin(),neighbors.end(),[verticallyClosestY](QLineEdit*e)
					{ return e->y()!=verticallyClosestY; }),neighbors.end());
	assert(!neighbors.empty());
	const auto x=pos.x();
	const auto bestNeighbor=*std::min_element(neighbors.begin(),neighbors.end(), [x](QLineEdit*a,QLineEdit*b)
			{ return std::abs(x - a->x()) < std::abs(x - b->x()); });
	bestNeighbor->setFocus(Qt::OtherFocusReason);
	return true;
}
}
