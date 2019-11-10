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
#include "IProcess.h"
#include "Instruction.h"
#include "edb.h"

namespace {

// a call can be anywhere from 2 to 7 bytes long depends on if there is a Mod/RM byte
// or a SIB byte, etc
// this is ignoring prefixes, fortunately, no calls have mandatory prefixes
// TODO(eteran): this is an arch specific concept
constexpr int CallMaxSize = 7;
constexpr int CallMinSize = 2;

}

/**
 * @brief CommentServer::setComment
 * @param address
 * @param comment
 */
void CommentServer::setComment(edb::address_t address, const QString &comment) {
	customComments_[address] = comment;
}

/**
 * @brief CommentServer::clear
 */
void CommentServer::clear() {
	customComments_.clear();
}

/**
 * @brief CommentServer::resolveFunctionCall
 * @param address
 * @return
 */
Result<QString, QString> CommentServer::resolveFunctionCall(edb::address_t address) const {

	// ok, we now want to locate the instruction before this one
	// so we need to look back a few bytes

	// TODO(eteran): portability warning, makes assumptions on the size of a call
	if (IProcess *process = edb::v1::debugger_core->process()) {

		uint8_t buffer[edb::Instruction::MaxSize];

		if (process->readBytes(address - CallMaxSize, buffer, sizeof(buffer))) {
			for (int i = (CallMaxSize - CallMinSize); i >= 0; --i) {
				edb::Instruction inst(buffer + i, buffer + sizeof(buffer), 0);
				if (is_call(inst)) {
					const QString symname = edb::v1::find_function_symbol(address);

					if (!symname.isEmpty()) {
						return tr("return to %1 <%2>").arg(edb::v1::format_pointer(address), symname);
					} else {
						return tr("return to %1").arg(edb::v1::format_pointer(address));
					}
				}
			}
		}
	}

	return make_unexpected(tr("Failed to resolve function call"));
}

/**
 * @brief CommentServer::resolveString
 * @param address
 * @return
 */
Result<QString, QString> CommentServer::resolveString(edb::address_t address) const {

	const int min_string_length   = edb::v1::config().min_string_length;
	constexpr int MaxStringLength = 256;

	int stringLen;
	QString temp;

	if (edb::v1::get_ascii_string_at_address(address, temp, min_string_length, MaxStringLength, stringLen)) {
		return tr("ASCII \"%1\"").arg(temp);
	} else if (edb::v1::get_utf16_string_at_address(address, temp, min_string_length, MaxStringLength, stringLen)) {
		return tr("UTF16 \"%1\"").arg(temp);
	}

	return make_unexpected(tr("Failed to resolve string"));
}

/**
 * @brief CommentServer::comment
 * @param address
 * @param size
 * @return
 */
QString CommentServer::comment(edb::address_t address, int size) const {

	if (IProcess *process = edb::v1::debugger_core->process()) {
		// if the view is currently looking at words which are a pointer in size
		// then see if it points to anything...
		if (size == static_cast<int>(edb::v1::pointer_size())) {
			edb::address_t value(0);
			if (process->readBytes(address, &value, edb::v1::pointer_size())) {

				auto it = customComments_.find(value);
				if (it != customComments_.end()) {
					return it.value();
				} else {
					if (Result<QString, QString> ret = resolveFunctionCall(value)) {
						return *ret;
					} else if (Result<QString, QString> ret = resolveString(value)) {
						return *ret;
					}
				}
			}
		}
	}

	return QString();
}
