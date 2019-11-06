
#ifndef ENTRY_GRID_KEY_UP_DOWN_EVENT_FILTER_H_20170705
#define ENTRY_GRID_KEY_UP_DOWN_EVENT_FILTER_H_20170705

class QWidget;
class QObject;
class QEvent;

namespace ODbgRegisterView {

bool entry_grid_key_event_filter(QWidget *parent, QObject *obj, QEvent *event);

}

#endif
