/*
Copyright (C) 2006 - 2013 Evan Teran
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

BasicBlock::BasicBlock() {

}

BasicBlock::BasicBlock(const BasicBlock &other) : instructions(other.instructions) {

}

BasicBlock &BasicBlock::operator=(const BasicBlock &rhs) {
	BasicBlock(rhs).swap(*this);
	return *this;
}

BasicBlock::~BasicBlock() {

}

void BasicBlock::swap(BasicBlock &other) {
	qSwap(instructions, other.instructions);
}

void BasicBlock::push_back(const instruction_pointer &inst) {
	instructions.push_back(inst);
}

BasicBlock::const_iterator BasicBlock::begin() const {
	return instructions.begin();
}

BasicBlock::const_iterator BasicBlock::end() const {
	return instructions.end();
}

BasicBlock::iterator BasicBlock::begin() {
	return instructions.begin();
}

BasicBlock::iterator BasicBlock::end() {
	return instructions.end();
}

BasicBlock::const_reverse_iterator BasicBlock::rbegin() const {
	return const_reverse_iterator(instructions.end());
}

BasicBlock::const_reverse_iterator BasicBlock::rend() const {
	return const_reverse_iterator(instructions.begin());
}

BasicBlock::reverse_iterator BasicBlock::rbegin() {
	return reverse_iterator(instructions.end());
}

BasicBlock::reverse_iterator BasicBlock::rend() {
	return reverse_iterator(instructions.begin());
}

BasicBlock::size_type BasicBlock::size() const {
	return instructions.size();
}

bool BasicBlock::empty() const {
	return instructions.isEmpty();
}

BasicBlock::reference BasicBlock::operator[](size_type pos) {
	return instructions[pos];
}

BasicBlock::const_reference BasicBlock::operator[](size_type pos) const {
	return instructions[pos];
}

BasicBlock::reference BasicBlock::front() {
	Q_ASSERT(!empty());
	return *begin();
}

BasicBlock::const_reference BasicBlock::front() const {
	Q_ASSERT(!empty());
	return *begin();
}

BasicBlock::reference BasicBlock::back() {
	Q_ASSERT(!empty());
	return *rbegin();
}

BasicBlock::const_reference BasicBlock::back() const {
	Q_ASSERT(!empty());
	return *rbegin();
}

BasicBlock::size_type BasicBlock::byte_size() const {
	size_type n = 0;
	Q_FOREACH(const instruction_pointer &insn, instructions) {
		n += insn->size();
	}
	return n;
}

edb::address_t BasicBlock::first_address() const {
	Q_ASSERT(!empty());
	return front()->rva();
}

edb::address_t BasicBlock::last_address() const {
	Q_ASSERT(!empty());
	return back()->rva() + back()->size();
}
