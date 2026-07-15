/*
 * Copyright (C) 2017 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "NavigationHistory.h"

/**
 * @brief Constructs a NavigationHistory object with the specified maximum count.
 *
 * @param count The maximum number of addresses to store in the navigation history.
 */
NavigationHistory::NavigationHistory(int count)
	: maxCount_(count) {
}

/**
 * @brief Adds an address to the navigation history.
 *
 * @param address The address to add to the history.
 */
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

		for (auto i = static_cast<int>(list_.size()) - 1; i >= pos_; i--) {
			if (address == list_.at(i)) {
				return;
			}
		}
	}

	pos_ = static_cast<int>(list_.size());
	list_.append(address);
	lastOp_ = LastOp::None;
}

/**
 * @brief Retrieves the next address in the navigation history.
 *
 * @return The next address in the history, or std::nullopt if there is no next address.
 */
std::optional<edb::address_t> NavigationHistory::getNext() {
	if (list_.isEmpty()) {
		return std::nullopt;
	}

	if (pos_ != (list_.size() - 1)) {
		++pos_;
	}

	lastOp_ = LastOp::Next;
	return list_.at(pos_);
}

/**
 * @brief Retrieves the previous address in the navigation history.
 *
 * @return The previous address in the history, or std::nullopt if there is no previous address.
 */
std::optional<edb::address_t> NavigationHistory::getPrev() {
	if (list_.isEmpty()) {
		return std::nullopt;
	}

	if (pos_ != 0) {
		--pos_;
	}

	lastOp_ = LastOp::Prev;
	return list_.at(pos_);
}
