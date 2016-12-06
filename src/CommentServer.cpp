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

#include "CommentServer.h"
#include "Configuration.h"
#include "IDebugger.h"
#include "Instruction.h"
#include "edb.h"

#include <QString>

//------------------------------------------------------------------------------
// Name: CommentServer
// Desc:
//------------------------------------------------------------------------------
CommentServer::CommentServer(QObject *parent) : QObject(parent) {

}

//------------------------------------------------------------------------------
// Name: ~CommentServer
// Desc:
//------------------------------------------------------------------------------
CommentServer::~CommentServer() {
}

//------------------------------------------------------------------------------
// Name: set_comment
// Desc:
//------------------------------------------------------------------------------
void CommentServer::set_comment(QHexView::address_t address, const QString &comment) {
	custom_comments_[address] = comment;
}

//------------------------------------------------------------------------------
// Name: clear
// Desc:
//------------------------------------------------------------------------------
void CommentServer::clear() {
	custom_comments_.clear();
}

// a call can be anywhere from 2 to 7 bytes long depends on if there is a Mod/RM byte
// or a SIB byte, etc
// this is ignoring prefixes, fortunately, no calls have mandatory prefixes
// TODO(eteran): this is an arch specific concept
#define CALL_MAX_SIZE 7
#define CALL_MIN_SIZE 2

//------------------------------------------------------------------------------
// Name: resolve_function_call
// Desc:
//------------------------------------------------------------------------------
Result<QString> CommentServer::resolve_function_call(QHexView::address_t address) const {
	
	// ok, we now want to locate the instruction before this one
	// so we need to look back a few bytes
	quint8 buffer[edb::Instruction::MAX_SIZE];

	// TODO(eteran): portability warning, makes assumptions on the size of a call
	if(IProcess *process = edb::v1::debugger_core->process()) {
		if(process->read_bytes(address - CALL_MAX_SIZE, buffer, sizeof(buffer))) {
			for(int i = (CALL_MAX_SIZE - CALL_MIN_SIZE); i >= 0; --i) {
				edb::Instruction inst(buffer + i, buffer + sizeof(buffer), 0);
				if(is_call(inst)) {
					const QString symname = edb::v1::find_function_symbol(address);
					
					if(!symname.isEmpty()) {
						return edb::v1::make_result(tr("return to %1 <%2>").arg(edb::v1::format_pointer(address)).arg(symname));
					} else {
						return edb::v1::make_result(tr("return to %1").arg(edb::v1::format_pointer(address)));
					}
				}
			}
		}
	}

	return Result<QString>(tr("Failed to resolve function call"), tr(""));
}

//------------------------------------------------------------------------------
// Name: resolve_string
// Desc:
//------------------------------------------------------------------------------
Result<QString> CommentServer::resolve_string(QHexView::address_t address) const {

	const int min_string_length = edb::v1::config().min_string_length;
	const int max_string_length = 256;

	int stringLen;
	QString temp;
	
	if(edb::v1::get_ascii_string_at_address(address, temp, min_string_length, max_string_length, stringLen)) {
		return edb::v1::make_result(tr("ASCII \"%1\"").arg(temp));
	} else if(edb::v1::get_utf16_string_at_address(address, temp, min_string_length, max_string_length, stringLen)) {
		return edb::v1::make_result(tr("UTF16 \"%1\"").arg(temp));
	}

	return Result<QString>(tr("Failed to resolve string"), tr(""));
}

//------------------------------------------------------------------------------
// Name: comment
// Desc:
//------------------------------------------------------------------------------
QString CommentServer::comment(QHexView::address_t address, int size) const {

	if(IProcess *process = edb::v1::debugger_core->process()) {
		// if the view is currently looking at words which are a pointer in size
		// then see if it points to anything...
		if(size == edb::v1::pointer_size()) {
			edb::address_t value(0);
			if(process->read_bytes(address, &value, edb::v1::pointer_size())) {

				auto it = custom_comments_.find(value);
				if(it != custom_comments_.end()) {
					return it.value();
				} else {
					if(Result<QString> ret = resolve_function_call(value)) {
						return *ret;
					} else if(Result<QString> ret = resolve_string(value)) {
						return *ret;
					}
				}
			}
		}
	}

	return QString();
}
