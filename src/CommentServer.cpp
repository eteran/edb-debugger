/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
 * @brief Sets a comment for the specified address.
 *
 * @param address The address to set the comment for.
 * @param comment The comment to set for the specified address.
 */
void CommentServer::setComment(edb::address_t address, const QString &comment) {
	customComments_[address] = comment;
}

/**
 * @brief Clears all custom comments.
 */
void CommentServer::clear() {
	customComments_.clear();
}

/**
 * @brief Resolves a function call at the specified address and returns a comment string.
 *
 * @param address The address of the function call to resolve.
 * @return A Result containing the comment string if successful, or an error message if failed.
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
					}

					return tr("return to %1").arg(edb::v1::format_pointer(address));
				}
			}
		}
	}

	return make_unexpected(tr("Failed to resolve function call"));
}

/**
 * @brief Resolves a string at the specified address and returns a comment string.
 *
 * @param address The address of the string to resolve.
 * @return A Result containing the comment string if successful, or an error message if failed.
 */
Result<QString, QString> CommentServer::resolveString(edb::address_t address) const {

	const int min_string_length   = edb::v1::config().min_string_length;
	constexpr int MaxStringLength = 256;

	int stringLen;
	QString temp;

	if (edb::v1::get_ascii_string_at_address(address, temp, min_string_length, MaxStringLength, stringLen)) {
		return tr("ASCII \"%1\"").arg(temp);
	}

	if (edb::v1::get_utf16_string_at_address(address, temp, min_string_length, MaxStringLength, stringLen)) {
		return tr("UTF16 \"%1\"").arg(temp);
	}

	return make_unexpected(tr("Failed to resolve string"));
}

/**
 * @brief Gets the comment for the specified address and size.
 *
 * @param address The address to get the comment for.
 * @param size The size of the data at the address.
 * @return The comment for the specified address, or an empty string if no comment is found.
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
					return it->second;
				}

				if (Result<QString, QString> ret = resolveFunctionCall(value)) {
					return *ret;
				}

				if (Result<QString, QString> ret = resolveString(value)) {
					return *ret;
				}
			}
		}
	}

	return QString();
}
