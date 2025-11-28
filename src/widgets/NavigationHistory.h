/*
 * Copyright (C) 2017 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
