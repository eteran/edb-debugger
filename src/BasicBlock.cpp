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

//------------------------------------------------------------------------------
// Name: swap
//------------------------------------------------------------------------------
void BasicBlock::swap(BasicBlock &other) {
	std::swap(instructions_, other.instructions_);
	std::swap(refs_, other.refs_);
}

//------------------------------------------------------------------------------
// Name: push_back
//------------------------------------------------------------------------------
void BasicBlock::push_back(const instruction_pointer &inst) {
	instructions_.push_back(inst);
}

//------------------------------------------------------------------------------
// Name: begin
//------------------------------------------------------------------------------
BasicBlock::const_iterator BasicBlock::begin() const {
	return instructions_.begin();
}

//------------------------------------------------------------------------------
// Name: end
//------------------------------------------------------------------------------
BasicBlock::const_iterator BasicBlock::end() const {
	return instructions_.end();
}

//------------------------------------------------------------------------------
// Name: begin
//------------------------------------------------------------------------------
BasicBlock::iterator BasicBlock::begin() {
	return instructions_.begin();
}

//------------------------------------------------------------------------------
// Name: end
//------------------------------------------------------------------------------
BasicBlock::iterator BasicBlock::end() {
	return instructions_.end();
}

//------------------------------------------------------------------------------
// Name: rbegin
//------------------------------------------------------------------------------
BasicBlock::const_reverse_iterator BasicBlock::rbegin() const {
	return const_reverse_iterator(instructions_.end());
}

//------------------------------------------------------------------------------
// Name: rend
//------------------------------------------------------------------------------
BasicBlock::const_reverse_iterator BasicBlock::rend() const {
	return const_reverse_iterator(instructions_.begin());
}

//------------------------------------------------------------------------------
// Name: rbegin
//------------------------------------------------------------------------------
BasicBlock::reverse_iterator BasicBlock::rbegin() {
	return reverse_iterator(instructions_.end());
}

//------------------------------------------------------------------------------
// Name: rend
//------------------------------------------------------------------------------
BasicBlock::reverse_iterator BasicBlock::rend() {
	return reverse_iterator(instructions_.begin());
}

//------------------------------------------------------------------------------
// Name: size
//------------------------------------------------------------------------------
BasicBlock::size_type BasicBlock::size() const {
	return instructions_.size();
}

//------------------------------------------------------------------------------
// Name: empty
//------------------------------------------------------------------------------
bool BasicBlock::empty() const {
	return instructions_.empty();
}

//------------------------------------------------------------------------------
// Name: operator[]
//------------------------------------------------------------------------------
BasicBlock::reference BasicBlock::operator[](size_type pos) {
	Q_ASSERT(pos < instructions_.size());
	return instructions_[pos];
}

//------------------------------------------------------------------------------
// Name: operator[]
//------------------------------------------------------------------------------
BasicBlock::const_reference BasicBlock::operator[](size_type pos) const {
	Q_ASSERT(pos < instructions_.size());
	return instructions_[pos];
}

//------------------------------------------------------------------------------
// Name: front
//------------------------------------------------------------------------------
BasicBlock::reference BasicBlock::front() {
	Q_ASSERT(!empty());
	return *begin();
}

//------------------------------------------------------------------------------
// Name: front
//------------------------------------------------------------------------------
BasicBlock::const_reference BasicBlock::front() const {
	Q_ASSERT(!empty());
	return *begin();
}

//------------------------------------------------------------------------------
// Name: back
//------------------------------------------------------------------------------
BasicBlock::reference BasicBlock::back() {
	Q_ASSERT(!empty());
	return *rbegin();
}

//------------------------------------------------------------------------------
// Name: back
//------------------------------------------------------------------------------
BasicBlock::const_reference BasicBlock::back() const {
	Q_ASSERT(!empty());
	return *rbegin();
}

//------------------------------------------------------------------------------
// Name: byte_size
//------------------------------------------------------------------------------
BasicBlock::size_type BasicBlock::byteSize() const {
	size_type n = 0;
	for(const instruction_pointer &inst: instructions_) {
		n += inst->byte_size();
	}
	return n;
}

//------------------------------------------------------------------------------
// Name: first_address
//------------------------------------------------------------------------------
edb::address_t BasicBlock::firstAddress() const {
	Q_ASSERT(!empty());
	return front()->rva();
}

//------------------------------------------------------------------------------
// Name: last_address
//------------------------------------------------------------------------------
edb::address_t BasicBlock::lastAddress() const {
	Q_ASSERT(!empty());
	return back()->rva() + back()->byte_size();
}

//------------------------------------------------------------------------------
// Name: toString
//------------------------------------------------------------------------------
QString BasicBlock::toString() const {
	QString text;
	QTextStream ts(&text);

	for(const instruction_pointer &inst : instructions_) {
		ts << edb::address_t(inst->rva()).toPointerString() << ": " << edb::v1::formatter().to_string(*inst).c_str() << "\n";
	}

	return text;
}


void BasicBlock::addRef(edb::address_t refsite, edb::address_t target) {
	refs_.push_back(std::make_pair(refsite, target));
}

std::vector<std::pair<edb::address_t, edb::address_t>> BasicBlock::refs() const {
	return refs_;
}
