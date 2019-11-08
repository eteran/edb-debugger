/*
Copyright (C) 2017 - 2017 Evan Teran
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

#include "NavigationHistory.h"

NavigationHistory::NavigationHistory(int count)
	: maxCount_(count) {
}

void NavigationHistory::add(edb::address_t address) {
	if (list_.size() == maxCount_) {
		list_.removeFirst();
		--pos_;
	}

	if (!list_.isEmpty()) {
		if (lastop_ == LASTOP_PREV && address == list_.at(pos_))
			return;
		if (lastop_ == LASTOP_NEXT && address == list_.at(pos_ - 1))
			return;
		for (int i = list_.size() - 1; i >= pos_; i--) {
			if (address == list_.at(i))
				return;
		}
	}

	pos_ = list_.size();
	list_.append(address);
	lastop_ = LASTOP_NONE;
}

edb::address_t NavigationHistory::getNext() {
	if (list_.isEmpty()) {
		return edb::address_t(0);
	}

	if (pos_ != (list_.size() - 1)) {
		++pos_;
	}

	lastop_ = LASTOP_NEXT;
	return list_.at(pos_);
}

edb::address_t NavigationHistory::getPrev() {
	if (list_.isEmpty()) {
		return edb::address_t(0);
	}

	if (pos_ != 0) {
		--pos_;
	}

	lastop_ = LASTOP_PREV;
	return list_.at(pos_);
}
