/*
 * Copyright (C) 2017 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
		if (lastOp_ == LastOp::Prev && address == list_.at(pos_)) {
			return;
		}

		if (lastOp_ == LastOp::Next && address == list_.at(pos_ - 1)) {
			return;
		}

		for (int i = list_.size() - 1; i >= pos_; i--) {
			if (address == list_.at(i)) {
				return;
			}
		}
	}

	pos_ = list_.size();
	list_.append(address);
	lastOp_ = LastOp::None;
}

edb::address_t NavigationHistory::getNext() {
	if (list_.isEmpty()) {
		return edb::address_t(0);
	}

	if (pos_ != (list_.size() - 1)) {
		++pos_;
	}

	lastOp_ = LastOp::Next;
	return list_.at(pos_);
}

edb::address_t NavigationHistory::getPrev() {
	if (list_.isEmpty()) {
		return edb::address_t(0);
	}

	if (pos_ != 0) {
		--pos_;
	}

	lastOp_ = LastOp::Prev;
	return list_.at(pos_);
}
