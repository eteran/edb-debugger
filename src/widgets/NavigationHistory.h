/*
Copyright (C) 2017 - 2023 Evan Teran
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

#ifndef NAVIGATION_HISTORY_H_20191119_
#define NAVIGATION_HISTORY_H_20191119_

#include "edb.h"

#include <QList>

class NavigationHistory {
	enum class LastOp {
		None = 0,
		Prev,
		Next
	};

public:
	explicit NavigationHistory(int count = 100);
	void add(edb::address_t address);
	[[nodiscard]] edb::address_t getNext();
	[[nodiscard]] edb::address_t getPrev();

private:
	QList<edb::address_t> list_;
	int pos_ = 0;
	int maxCount_;
	LastOp lastOp_ = LastOp::None;
};

#endif
