
#ifndef ENTRY_GRID_KEY_UP_DOWN_EVENT_FILTER_H_20170705
#define ENTRY_GRID_KEY_UP_DOWN_EVENT_FILTER_H_20170705

class QWidget;
class QObject;
class QEvent;

namespace ODbgRegisterView {
bool entryGridKeyUpDownEventFilter(QWidget* parent, QObject* obj, QEvent* event);
}

#endif
