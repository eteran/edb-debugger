#include "NavigationHistory.h"

NavigationHistory::NavigationHistory(int count)
{
    max_count_ = count;
    pos_   = 0;
}

void NavigationHistory::add(edb::address_t address)
{
    if (list_.size() == max_count_)
    {
        list_.removeFirst();
        --pos_;
    }

    if (!list_.isEmpty())
    {
        if (lastop_ == LASTOP_PREV && address == list_.at(pos_))
            return;
        if (lastop_ == LASTOP_NEXT && address == list_.at(pos_-1))
            return;
        for (int i = list_.size() - 1; i >= pos_; i--)
        {
            if (address == list_.at(i))
                return;
        }
    }

    pos_ = list_.size();
    list_.append(address);
    lastop_ = LASTOP_NONE;
}

edb::address_t NavigationHistory::getNext()
{
    if (list_.isEmpty())
    {
        return (edb::address_t)0;
    }

    if (pos_ != (list_.size() - 1))
    {
        ++pos_;
    }

    lastop_ = LASTOP_NEXT;
    return list_.at(pos_);

}

edb::address_t NavigationHistory::getPrev()
{
    if (list_.isEmpty())
    {
        return (edb::address_t)0;
    }

    if (pos_ != 0)
    {
        --pos_;
    }

    lastop_ = LASTOP_PREV;
    return list_.at(pos_);
}
