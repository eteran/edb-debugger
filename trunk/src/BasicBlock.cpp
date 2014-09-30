/*
Copyright (C) 2006 - 2014 Evan Teran
                          eteran@alum.rit.edu

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

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
BasicBlock::BasicBlock() {

}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
BasicBlock::BasicBlock(const BasicBlock &other) : instructions_(other.instructions_) {

}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
BasicBlock &BasicBlock::operator=(const BasicBlock &rhs) {
	BasicBlock(rhs).swap(*this);
	return *this;
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
BasicBlock::~BasicBlock() {

}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void BasicBlock::swap(BasicBlock &other) {
	qSwap(instructions_, other.instructions_);
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void BasicBlock::push_back(const instruction_pointer &inst) {
	instructions_.push_back(inst);
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
BasicBlock::const_iterator BasicBlock::begin() const {
	return instructions_.begin();
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
BasicBlock::const_iterator BasicBlock::end() const {
	return instructions_.end();
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
BasicBlock::iterator BasicBlock::begin() {
	return instructions_.begin();
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
BasicBlock::iterator BasicBlock::end() {
	return instructions_.end();
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
BasicBlock::const_reverse_iterator BasicBlock::rbegin() const {
	return const_reverse_iterator(instructions_.end());
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
BasicBlock::const_reverse_iterator BasicBlock::rend() const {
	return const_reverse_iterator(instructions_.begin());
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
BasicBlock::reverse_iterator BasicBlock::rbegin() {
	return reverse_iterator(instructions_.end());
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
BasicBlock::reverse_iterator BasicBlock::rend() {
	return reverse_iterator(instructions_.begin());
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
BasicBlock::size_type BasicBlock::size() const {
	return instructions_.size();
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
bool BasicBlock::empty() const {
	return instructions_.isEmpty();
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
BasicBlock::reference BasicBlock::operator[](size_type pos) {
	return instructions_[pos];
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
BasicBlock::const_reference BasicBlock::operator[](size_type pos) const {
	return instructions_[pos];
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
BasicBlock::reference BasicBlock::front() {
	Q_ASSERT(!empty());
	return *begin();
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
BasicBlock::const_reference BasicBlock::front() const {
	Q_ASSERT(!empty());
	return *begin();
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
BasicBlock::reference BasicBlock::back() {
	Q_ASSERT(!empty());
	return *rbegin();
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
BasicBlock::const_reference BasicBlock::back() const {
	Q_ASSERT(!empty());
	return *rbegin();
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
BasicBlock::size_type BasicBlock::byte_size() const {
	size_type n = 0;
	Q_FOREACH(const instruction_pointer &inst, instructions_) {
		n += inst->size();
	}
	return n;
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
edb::address_t BasicBlock::first_address() const {
	Q_ASSERT(!empty());
	return front()->rva();
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
edb::address_t BasicBlock::last_address() const {
	Q_ASSERT(!empty());
	return back()->rva() + back()->size();
}
