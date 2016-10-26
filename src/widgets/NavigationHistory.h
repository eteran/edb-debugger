#ifndef NAVIGATIONHISTORY_H_
#define NAVIGATIONHISTORY_H_

#include "edb.h"

#include <QList>

class NavigationHistory
{
    enum LASTOP {LASTOP_NONE = 0, LASTOP_PREV, LASTOP_NEXT};
public:
    NavigationHistory(int count = 100);
    void add(edb::address_t address);
    edb::address_t getNext();
    edb::address_t getPrev();

private:
    QList<edb::address_t> list_;
    int pos_;
    int max_count_;
    int lastop_;
};

#endif // NAVIGATIONHISTORY_H_
