/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "Function.h"

/**
 * @brief Swap the contents of this function with another
 *
 * @param other The function to swap with
 */
void Function::swap(Function &other) noexcept {
	using std::swap;
	swap(referenceCount_, other.referenceCount_);
	swap(type, other.type);
	swap(blocks_, other.blocks_);
}

/**
 * @brief Insert a basic block into the function
 *
 * @param bb The basic block to insert
 */
void Function::insert(const BasicBlock &bb) {
	blocks_[bb.firstAddress()] = bb;
}

/**
 * @brief Insert a basic block into the function using move semantics
 *
 * @param bb The basic block to insert
 */
void Function::insert(BasicBlock &&bb) {
	blocks_[bb.firstAddress()] = std::move(bb);
}

/**
 * @brief Get the entry address of the function
 *
 * @return The entry address of the function
 */
edb::address_t Function::entryAddress() const {
	Q_ASSERT(!empty());
	return front().firstAddress();
}

/**
 * @brief Get the end address of the function
 *
 * @return The end address of the function
 */
edb::address_t Function::endAddress() const {
	Q_ASSERT(!empty());
	return back().lastAddress() - 1;
}

/**
 * @brief Get the address of the last instruction in the function
 *
 * @return The address of the last instruction in the function
 */
edb::address_t Function::lastInstruction() const {
	Q_ASSERT(!empty());
	return back().back()->rva();
}

/**
 * @brief Get the reference count of the function. This is the number of times this function is called from other functions.
 *
 * @return The reference count of the function
 */
int Function::referenceCount() const {
	return referenceCount_;
}

/**
 * @brief Get the last basic block in the function
 *
 * @return The last basic block in the function
 */
Function::const_reference Function::back() const {
	Q_ASSERT(!empty());
	const auto &[address, block] = *rbegin();
	Q_UNUSED(address)
	return block;
}

/**
 * @brief Get the first basic block in the function
 *
 * @return The first basic block in the function
 */
Function::const_reference Function::front() const {
	Q_ASSERT(!empty());
	const auto &[address, block] = *begin();
	Q_UNUSED(address)
	return block;
}

/**
 * @brief Get the last basic block in the function
 *
 * @return The last basic block in the function
 */
Function::reference Function::back() {
	Q_ASSERT(!empty());
	auto &[address, block] = *rbegin();
	Q_UNUSED(address)
	return block;
}

/**
 * @brief Get the first basic block in the function
 *
 * @return The first basic block in the function
 */
Function::reference Function::front() {
	Q_ASSERT(!empty());
	auto &[address, block] = *begin();
	Q_UNUSED(address)
	return block;
}

/**
 * @brief Return an iterator to the beginning of the function's basic blocks
 *
 * @return An iterator to the beginning of the function's basic blocks
 */
Function::const_iterator Function::begin() const {
	return blocks_.begin();
}

/**
 * @brief Return an iterator to the end of the function's basic blocks
 *
 * @return An iterator to the end of the function's basic blocks
 */
Function::const_iterator Function::end() const {
	return blocks_.end();
}

/**
 * @brief Return a reverse iterator to the beginning of the function's basic blocks
 *
 * @return A reverse iterator to the beginning of the function's basic blocks
 */
Function::const_reverse_iterator Function::rbegin() const {
	return const_reverse_iterator(blocks_.end());
}

/**
 * @brief Return a reverse iterator to the end of the function's basic blocks
 *
 * @return A reverse iterator to the end of the function's basic blocks
 */
Function::const_reverse_iterator Function::rend() const {
	return const_reverse_iterator(blocks_.begin());
}

/**
 * @brief Return an iterator to the beginning of the function's basic blocks
 *
 * @return An iterator to the beginning of the function's basic blocks
 */
Function::iterator Function::begin() {
	return blocks_.begin();
}

/**
 * @brief Return an iterator to the end of the function's basic blocks
 *
 * @return An iterator to the end of the function's basic blocks
 */
Function::iterator Function::end() {
	return blocks_.end();
}

/**
 * @brief Return a reverse iterator to the beginning of the function's basic blocks
 *
 * @return A reverse iterator to the beginning of the function's basic blocks
 */
Function::reverse_iterator Function::rbegin() {
	return reverse_iterator(blocks_.end());
}

/**
 * @brief Return a reverse iterator to the end of the function's basic blocks
 *
 * @return A reverse iterator to the end of the function's basic blocks
 */
Function::reverse_iterator Function::rend() {
	return reverse_iterator(blocks_.begin());
}

/**
 * @brief Check if the function has no basic blocks
 *
 * @return true if the function has no basic blocks, false otherwise
 */
bool Function::empty() const {
	return blocks_.empty();
}

/**
 * @brief Get the number of basic blocks in the function
 *
 * @return The number of basic blocks in the function
 */
Function::size_type Function::size() const {
	return blocks_.size();
}

/**
 * @brief Increment the reference count of the function. This should be called whenever this function is called from another function.
 */
void Function::addReference() {
	++referenceCount_;
}

/**
 * @brief Erase a basic block from the function
 *
 * @param it An iterator to the basic block to erase
 */
void Function::erase(const_iterator it) {
	blocks_.erase(it);
}

/**
 * @brief Check if the function contains a specific basic block
 *
 * @param bb The basic block to check for
 * @return true if the function contains the basic block, false otherwise
 */
bool Function::containsBlock(const BasicBlock &bb) const {
	return containsBlock(bb.firstAddress());
}

/**
 * @brief Check if the function contains a specific basic block by its starting address
 *
 * @param address The starting address of the basic block to check for
 * @return true if the function contains the basic block, false otherwise
 */
bool Function::containsBlock(edb::address_t address) const {
	auto it = blocks_.find(address);
	if (it == blocks_.end()) {
		return false;
	}
	return true;
}
