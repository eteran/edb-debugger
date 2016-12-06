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

#include "Function.h"

//------------------------------------------------------------------------------
// Name: Function
//------------------------------------------------------------------------------
Function::Function() : address_(0), reference_count_(0), type_(FUNCTION_STANDARD) {

}

//------------------------------------------------------------------------------
// Name: Function
//------------------------------------------------------------------------------
Function::Function(edb::address_t address) : address_(address), reference_count_(1), type_(FUNCTION_STANDARD) {

}

//------------------------------------------------------------------------------
// Name: Function
//------------------------------------------------------------------------------
Function::Function(const Function &other) : address_(other.address_), reference_count_(other.reference_count_), type_(other.type_), blocks_(other.blocks_) {
}

//------------------------------------------------------------------------------
// Name: operator=
//------------------------------------------------------------------------------
Function &Function::operator=(const Function &rhs) {
	Function(rhs).swap(*this);
	return *this;
}

//------------------------------------------------------------------------------
// Name: swap
//------------------------------------------------------------------------------
void Function::swap(Function &other) {
	qSwap(address_, other.address_);
	qSwap(reference_count_, other.reference_count_);
	qSwap(type_, other.type_);
	qSwap(blocks_, other.blocks_);
}

//------------------------------------------------------------------------------
// Name: insert
//------------------------------------------------------------------------------
void Function::insert(const BasicBlock &bb) {
	blocks_[bb.first_address()] = bb;
}

//------------------------------------------------------------------------------
// Name: entry_address
//------------------------------------------------------------------------------
edb::address_t Function::entry_address() const {
	return front().first_address();
}

//------------------------------------------------------------------------------
// Name: end_address
//------------------------------------------------------------------------------
edb::address_t Function::end_address() const {
	return back().last_address() - 1;
}

//------------------------------------------------------------------------------
// Name: last_instruction
//------------------------------------------------------------------------------
edb::address_t Function::last_instruction() const {
	return back().back()->rva();
}

//------------------------------------------------------------------------------
// Name: reference_count
//------------------------------------------------------------------------------
int Function::reference_count() const {
	return reference_count_;
}

//------------------------------------------------------------------------------
// Name: back
//------------------------------------------------------------------------------
Function::const_reference Function::back() const {
	Q_ASSERT(!empty());
	return *rbegin();
}

//------------------------------------------------------------------------------
// Name: front
//------------------------------------------------------------------------------
Function::const_reference Function::front() const {
	Q_ASSERT(!empty());
	return *begin();
}

//------------------------------------------------------------------------------
// Name: back
//------------------------------------------------------------------------------
Function::reference Function::back() {
	Q_ASSERT(!empty());
	return *rbegin();
}

//------------------------------------------------------------------------------
// Name: front
//------------------------------------------------------------------------------
Function::reference Function::front() {
	Q_ASSERT(!empty());
	return *begin();
}

//------------------------------------------------------------------------------
// Name: begin
//------------------------------------------------------------------------------
Function::const_iterator Function::begin() const {
	return blocks_.begin();
}

//------------------------------------------------------------------------------
// Name: end
//------------------------------------------------------------------------------
Function::const_iterator Function::end() const {
	return blocks_.end();
}

//------------------------------------------------------------------------------
// Name: rbegin
//------------------------------------------------------------------------------
Function::const_reverse_iterator Function::rbegin() const {
	return const_reverse_iterator(blocks_.end());
}

//------------------------------------------------------------------------------
// Name: rend
//------------------------------------------------------------------------------
Function::const_reverse_iterator Function::rend() const {
	return const_reverse_iterator(blocks_.begin());
}

//------------------------------------------------------------------------------
// Name: begin
//------------------------------------------------------------------------------
Function::iterator Function::begin() {
	return blocks_.begin();
}

//------------------------------------------------------------------------------
// Name: end
//------------------------------------------------------------------------------
Function::iterator Function::end() {
	return blocks_.end();
}

//------------------------------------------------------------------------------
// Name: rbegin
//------------------------------------------------------------------------------
Function::reverse_iterator Function::rbegin() {
	return reverse_iterator(blocks_.end());
}

//------------------------------------------------------------------------------
// Name: rend
//------------------------------------------------------------------------------
Function::reverse_iterator Function::rend() {
	return reverse_iterator(blocks_.begin());
}

//------------------------------------------------------------------------------
// Name: empty
//------------------------------------------------------------------------------
bool Function::empty() const {
	return blocks_.isEmpty();
}

//------------------------------------------------------------------------------
// Name: size
//------------------------------------------------------------------------------
Function::size_type Function::size() const {
	return blocks_.size();
}

//------------------------------------------------------------------------------
// Name: add_reference
//------------------------------------------------------------------------------
void Function::add_reference() {
	++reference_count_;
}

//------------------------------------------------------------------------------
// Name: type
//------------------------------------------------------------------------------
Function::Type Function::type() const {
	return type_;
}

//------------------------------------------------------------------------------
// Name: set_type
//------------------------------------------------------------------------------
void Function::set_type(Type t) {
	type_ = t;
}
