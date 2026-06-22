/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "BasicBlock.h"
#include "edb.h"

#include <QString>
#include <QTextStream>

/**
 * @brief Swaps the contents of this BasicBlock with another BasicBlock.
 *
 * @param other The other BasicBlock to swap with.
 */
void BasicBlock::swap(BasicBlock &other) noexcept {
	using std::swap;
	swap(instructions_, other.instructions_);
	swap(references_, other.references_);
}

/**
 * @brief Adds an instruction to the end of the BasicBlock.
 *
 * @param inst The instruction to add.
 */
void BasicBlock::push_back(const instruction_pointer &inst) {
	instructions_.push_back(inst);
}

/**
 * @brief Returns an iterator to the beginning of the BasicBlock.
 *
 * @return An iterator to the beginning of the BasicBlock.
 */
BasicBlock::const_iterator BasicBlock::begin() const {
	return instructions_.begin();
}

/**
 * @brief Returns an iterator to the end of the BasicBlock.
 *
 * @return An iterator to the end of the BasicBlock.
 */
BasicBlock::const_iterator BasicBlock::end() const {
	return instructions_.end();
}

/**
 * @brief Returns a reverse iterator to the beginning of the BasicBlock.
 *
 * @return A reverse iterator to the beginning of the BasicBlock.
 */
BasicBlock::iterator BasicBlock::begin() {
	return instructions_.begin();
}

/**
 * @brief Returns an iterator to the end of the BasicBlock.
 *
 * @return An iterator to the end of the BasicBlock.
 */
BasicBlock::iterator BasicBlock::end() {
	return instructions_.end();
}

/**
 * @brief Returns a reverse iterator to the beginning of the BasicBlock.
 *
 * @return A reverse iterator to the beginning of the BasicBlock.
 */
BasicBlock::const_reverse_iterator BasicBlock::rbegin() const {
	return const_reverse_iterator(instructions_.end());
}

/**
 * @brief Returns a reverse iterator to the end of the BasicBlock.
 *
 * @return A reverse iterator to the end of the BasicBlock.
 */
BasicBlock::const_reverse_iterator BasicBlock::rend() const {
	return const_reverse_iterator(instructions_.begin());
}

/**
 * @brief Returns a reverse iterator to the beginning of the BasicBlock.
 *
 * @return A reverse iterator to the beginning of the BasicBlock.
 */
BasicBlock::reverse_iterator BasicBlock::rbegin() {
	return reverse_iterator(instructions_.end());
}

/**
 * @brief Returns a reverse iterator to the end of the BasicBlock.
 *
 * @return A reverse iterator to the end of the BasicBlock.
 */
BasicBlock::reverse_iterator BasicBlock::rend() {
	return reverse_iterator(instructions_.begin());
}

/**
 * @brief Returns the number of instructions in the BasicBlock.
 *
 * @return The number of instructions in the BasicBlock.
 */
BasicBlock::size_type BasicBlock::size() const {
	return instructions_.size();
}

/**
 * @brief Returns whether the BasicBlock is empty.
 *
 * @return True if the BasicBlock is empty, false otherwise.
 */
bool BasicBlock::empty() const {
	return instructions_.empty();
}

/**
 * @brief Returns a reference to the instruction at the specified position in the BasicBlock.
 *
 * @param pos The position of the instruction to return.
 * @return A reference to the instruction at the specified position in the BasicBlock.
 */
BasicBlock::reference BasicBlock::operator[](size_type pos) {
	Q_ASSERT(pos < instructions_.size());
	return instructions_[pos];
}

/**
 * @brief Returns a const reference to the instruction at the specified position in the BasicBlock.
 *
 * @param pos The position of the instruction to return.
 * @return A const reference to the instruction at the specified position in the BasicBlock.
 */
BasicBlock::const_reference BasicBlock::operator[](size_type pos) const {
	Q_ASSERT(pos < instructions_.size());
	return instructions_[pos];
}

/**
 * @brief Returns a reference to the first instruction in the BasicBlock.
 *
 * @return A reference to the first instruction in the BasicBlock.
 */
BasicBlock::reference BasicBlock::front() {
	Q_ASSERT(!empty());
	return *begin();
}

/**
 * @brief Returns a const reference to the first instruction in the BasicBlock.
 *
 * @return A const reference to the first instruction in the BasicBlock.
 */
BasicBlock::const_reference BasicBlock::front() const {
	Q_ASSERT(!empty());
	return *begin();
}

/**
 * @brief Returns a reference to the last instruction in the BasicBlock.
 *
 * @return A reference to the last instruction in the BasicBlock.
 */
BasicBlock::reference BasicBlock::back() {
	Q_ASSERT(!empty());
	return *rbegin();
}

/**
 * @brief Returns a const reference to the last instruction in the BasicBlock.
 *
 * @return A const reference to the last instruction in the BasicBlock.
 */
BasicBlock::const_reference BasicBlock::back() const {
	Q_ASSERT(!empty());
	return *rbegin();
}

/**
 * @brief Returns the sum of the byte sizes of all instructions in the BasicBlock.
 *
 * @return The number of bytes in the BasicBlock.
 */
BasicBlock::size_type BasicBlock::byteSize() const {
	size_type n = 0;
	for (const instruction_pointer &inst : instructions_) {
		n += inst->byteSize();
	}
	return n;
}

/**
 * @brief Returns the address of the first instruction in the BasicBlock.
 *
 * @return The address of the first instruction in the BasicBlock.
 */
edb::address_t BasicBlock::firstAddress() const {
	Q_ASSERT(!empty());
	return edb::address_t(front()->rva());
}

/**
 * @brief Returns the address immediately following the last instruction in the BasicBlock.
 *
 * @return The address immediately following the last instruction in the BasicBlock.
 */
edb::address_t BasicBlock::lastAddress() const {
	Q_ASSERT(!empty());
	return edb::address_t(back()->rva() + back()->byteSize());
}

/**
 * @brief Returns a string representation of the BasicBlock. This is the disassembly of all instructions in the BasicBlock, one per line, with their addresses.
 *
 * @return A string representing the BasicBlock.
 */
QString BasicBlock::toString() const {
	QString text;
	QTextStream ts(&text);

	for (const instruction_pointer &inst : instructions_) {
		ts << edb::address_t(inst->rva()).toPointerString() << ": " << edb::v1::formatter().toString(*inst).c_str() << "\n";
	}

	return text;
}

/**
 * @brief Adds a reference from one address to another. This is used to track control flow between BasicBlocks.
 *
 * @param refsite The address of the instruction that makes the reference.
 * @param target The address that is being referenced.
 */
void BasicBlock::addReference(edb::address_t refsite, edb::address_t target) {
	references_.emplace_back(refsite, target);
}

/**
 * @brief Returns the references from this BasicBlock.
 *
 * @return A vector of pairs representing the references.
 */
std::vector<std::pair<edb::address_t, edb::address_t>> BasicBlock::references() const {
	return references_;
}

/**
 * @brief Splits the BasicBlock at the specified instruction.
 *
 * @param inst The instruction at which to split the BasicBlock.
 * @return A pair of BasicBlocks representing the split.
 */
std::pair<BasicBlock, BasicBlock> BasicBlock::splitBlock(const instruction_pointer &inst) {
	BasicBlock block1;
	BasicBlock block2;

	auto it = begin();
	for (; it != end(); ++it) {

		block1.push_back(*it);
		if (*it == inst) {
			++it;
			break;
		}
	}

	for (; it != end(); ++it) {
		block2.push_back(*it);
	}

	for (const auto &[from, to] : references_) {
		if (from >= block1.firstAddress() && from < block1.lastAddress()) {
			block1.addReference(from, to);
		} else {
			block2.addReference(from, to);
		}
	}

	return {block1, block2};
}
