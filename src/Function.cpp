/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "Function.h"

/**
 * @brief Function::swap
 * @param other
 */
void Function::swap(Function &other) {
	using std::swap;
	swap(referenceCount_, other.referenceCount_);
	swap(type_, other.type_);
	swap(blocks_, other.blocks_);
}

/**
 * @brief Function::insert
 * @param bb
 */
void Function::insert(const BasicBlock &bb) {
	blocks_[bb.firstAddress()] = bb;
}

/**
 * @brief Function::insert
 * @param bb
 */
void Function::insert(BasicBlock &&bb) {
	blocks_[bb.firstAddress()] = std::move(bb);
}

/**
 * @brief Function::entryAddress
 * @return
 */
edb::address_t Function::entryAddress() const {
	Q_ASSERT(!empty());
	return front().firstAddress();
}

/**
 * @brief Function::endAddress
 * @return
 */
edb::address_t Function::endAddress() const {
	Q_ASSERT(!empty());
	return back().lastAddress() - 1;
}

/**
 * @brief Function::lastInstruction
 * @return
 */
edb::address_t Function::lastInstruction() const {
	Q_ASSERT(!empty());
	return back().back()->rva();
}

/**
 * @brief Function::referenceCount
 * @return
 */
int Function::referenceCount() const {
	return referenceCount_;
}

/**
 * @brief Function::back
 * @return
 */
Function::const_reference Function::back() const {
	Q_ASSERT(!empty());
	return rbegin()->second;
}

/**
 * @brief Function::front
 * @return
 */
Function::const_reference Function::front() const {
	Q_ASSERT(!empty());
	return begin()->second;
}

/**
 * @brief Function::back
 * @return
 */
Function::reference Function::back() {
	Q_ASSERT(!empty());
	return rbegin()->second;
}

/**
 * @brief Function::front
 * @return
 */
Function::reference Function::front() {
	Q_ASSERT(!empty());
	return begin()->second;
}

/**
 * @brief Function::begin
 * @return
 */
Function::const_iterator Function::begin() const {
	return blocks_.begin();
}

/**
 * @brief Function::end
 * @return
 */
Function::const_iterator Function::end() const {
	return blocks_.end();
}

/**
 * @brief Function::rbegin
 * @return
 */
Function::const_reverse_iterator Function::rbegin() const {
	return const_reverse_iterator(blocks_.end());
}

/**
 * @brief Function::rend
 * @return
 */
Function::const_reverse_iterator Function::rend() const {
	return const_reverse_iterator(blocks_.begin());
}

/**
 * @brief Function::begin
 * @return
 */
Function::iterator Function::begin() {
	return blocks_.begin();
}

/**
 * @brief Function::end
 * @return
 */
Function::iterator Function::end() {
	return blocks_.end();
}

/**
 * @brief Function::rbegin
 * @return
 */
Function::reverse_iterator Function::rbegin() {
	return reverse_iterator(blocks_.end());
}

/**
 * @brief Function::rend
 * @return
 */
Function::reverse_iterator Function::rend() {
	return reverse_iterator(blocks_.begin());
}

/**
 * @brief Function::empty
 * @return
 */
bool Function::empty() const {
	return blocks_.empty();
}

/**
 * @brief Function::size
 * @return
 */
Function::size_type Function::size() const {
	return blocks_.size();
}

/**
 * @brief Function::addReference
 */
void Function::addReference() {
	++referenceCount_;
}
/**
 * @brief Function::type
 * @return
 */
Function::Type Function::type() const {
	return type_;
}

/**
 * @brief Function::setType
 * @param t
 */
void Function::setType(Type t) {
	type_ = t;
}

void Function::erase(const_iterator it) {
	blocks_.erase(it);
}
