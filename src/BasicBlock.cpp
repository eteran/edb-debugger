/*
Copyright (C) 2006 - 2015 Evan Teran
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

#include "BasicBlock.h"
#include "edb.h"

#include <QString>
#include <QTextStream>

/**
 * @brief BasicBlock::swap
 * @param other
 */
void BasicBlock::swap(BasicBlock &other) {
	using std::swap;
	swap(instructions_, other.instructions_);
	swap(references_, other.references_);
}

/**
 * @brief BasicBlock::push_back
 * @param inst
 */
void BasicBlock::push_back(const instruction_pointer &inst) {
	instructions_.push_back(inst);
}

/**
 * @brief BasicBlock::begin
 * @return
 */
BasicBlock::const_iterator BasicBlock::begin() const {
	return instructions_.begin();
}

/**
 * @brief BasicBlock::end
 * @return
 */
BasicBlock::const_iterator BasicBlock::end() const {
	return instructions_.end();
}

/**
 * @brief BasicBlock::begin
 * @return
 */
BasicBlock::iterator BasicBlock::begin() {
	return instructions_.begin();
}

/**
 * @brief BasicBlock::end
 * @return
 */
BasicBlock::iterator BasicBlock::end() {
	return instructions_.end();
}

/**
 * @brief BasicBlock::rbegin
 * @return
 */
BasicBlock::const_reverse_iterator BasicBlock::rbegin() const {
	return const_reverse_iterator(instructions_.end());
}

/**
 * @brief BasicBlock::rend
 * @return
 */
BasicBlock::const_reverse_iterator BasicBlock::rend() const {
	return const_reverse_iterator(instructions_.begin());
}

/**
 * @brief BasicBlock::rbegin
 * @return
 */
BasicBlock::reverse_iterator BasicBlock::rbegin() {
	return reverse_iterator(instructions_.end());
}

/**
 * @brief BasicBlock::rend
 * @return
 */
BasicBlock::reverse_iterator BasicBlock::rend() {
	return reverse_iterator(instructions_.begin());
}

/**
 * @brief BasicBlock::size
 * @return
 */
BasicBlock::size_type BasicBlock::size() const {
	return instructions_.size();
}

/**
 * @brief BasicBlock::empty
 * @return
 */
bool BasicBlock::empty() const {
	return instructions_.empty();
}

/**
 * @brief BasicBlock::operator []
 * @param pos
 * @return
 */
BasicBlock::reference BasicBlock::operator[](size_type pos) {
	Q_ASSERT(pos < instructions_.size());
	return instructions_[pos];
}

/**
 * @brief BasicBlock::operator []
 * @param pos
 * @return
 */
BasicBlock::const_reference BasicBlock::operator[](size_type pos) const {
	Q_ASSERT(pos < instructions_.size());
	return instructions_[pos];
}

/**
 * @brief BasicBlock::front
 * @return
 */
BasicBlock::reference BasicBlock::front() {
	Q_ASSERT(!empty());
	return *begin();
}

/**
 * @brief BasicBlock::front
 * @return
 */
BasicBlock::const_reference BasicBlock::front() const {
	Q_ASSERT(!empty());
	return *begin();
}

/**
 * @brief BasicBlock::back
 * @return
 */
BasicBlock::reference BasicBlock::back() {
	Q_ASSERT(!empty());
	return *rbegin();
}

/**
 * @brief BasicBlock::back
 * @return
 */
BasicBlock::const_reference BasicBlock::back() const {
	Q_ASSERT(!empty());
	return *rbegin();
}

/**
 * @brief BasicBlock::byteSize
 * @return
 */
BasicBlock::size_type BasicBlock::byteSize() const {
	size_type n = 0;
	for (const instruction_pointer &inst : instructions_) {
		n += inst->byteSize();
	}
	return n;
}

/**
 * @brief BasicBlock::firstAddress
 * @return
 */
edb::address_t BasicBlock::firstAddress() const {
	Q_ASSERT(!empty());
	return edb::address_t(front()->rva());
}

/**
 * @brief BasicBlock::lastAddress
 * @return
 */
edb::address_t BasicBlock::lastAddress() const {
	Q_ASSERT(!empty());
	return edb::address_t(back()->rva() + back()->byteSize());
}

/**
 * @brief BasicBlock::toString
 * @return
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
 * @brief BasicBlock::addReference
 * @param refsite
 * @param target
 */
void BasicBlock::addReference(edb::address_t refsite, edb::address_t target) {
	references_.push_back(std::make_pair(refsite, target));
}

/**
 * @brief BasicBlock::references
 * @return
 */
std::vector<std::pair<edb::address_t, edb::address_t>> BasicBlock::references() const {
	return references_;
}
