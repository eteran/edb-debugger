/*
Copyright (C) 2015 Ruslan Kabatsayev <b7.10110111@gmail.com>

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

#include "GprEdit.h"
#include "QLongValidator.h"
#include "QULongValidator.h"
#include "util/Font.h"
#include <QApplication>
#include <QRegExpValidator>
#include <cmath>
#include <cstring>

namespace ODbgRegisterView {
namespace {

const QRegExpValidator byteHexValidator(QRegExp("[0-9a-fA-F]{0,2}"));
const QRegExpValidator wordHexValidator(QRegExp("[0-9a-fA-F]{0,4}"));
const QRegExpValidator dwordHexValidator(QRegExp("[0-9a-fA-F]{0,8}"));
const QRegExpValidator qwordHexValidator(QRegExp("[0-9a-fA-F]{0,16}"));
const QLongValidator byteSignedValidator(INT8_MIN, INT8_MAX);
const QLongValidator wordSignedValidator(INT16_MIN, INT16_MAX);
const QLongValidator dwordSignedValidator(INT32_MIN, INT32_MAX);
const QLongValidator qwordSignedValidator(INT64_MIN, INT64_MAX);
const QULongValidator byteUnsignedValidator(0, UINT8_MAX);
const QULongValidator wordUnsignedValidator(0, UINT16_MAX);
const QULongValidator dwordUnsignedValidator(0, UINT32_MAX);
const QULongValidator qwordUnsignedValidator(0, UINT64_MAX);

const std::map<int, const QRegExpValidator *> hexValidators = {
	{1, &byteHexValidator},
	{2, &wordHexValidator},
	{4, &dwordHexValidator},
	{8, &qwordHexValidator}};

const std::map<int, const QLongValidator *> signedValidators = {
	{1, &byteSignedValidator},
	{2, &wordSignedValidator},
	{4, &dwordSignedValidator},
	{8, &qwordSignedValidator}};

const std::map<int, const QULongValidator *> unsignedValidators = {
	{1, &byteUnsignedValidator},
	{2, &wordUnsignedValidator},
	{4, &dwordUnsignedValidator},
	{8, &qwordUnsignedValidator}};

}

void GprEdit::setupFormat(Format newFormat) {
	format_ = newFormat;
	switch (format_) {
	case Format::Hex:
		setValidator(hexValidators.at(integerSize_));
		naturalWidthInChars_ = 2 * integerSize_;
		break;
	case Format::Signed:
		setValidator(signedValidators.at(integerSize_));
		naturalWidthInChars_ = 1 + std::lround(integerSize_ * std::log10(256.));
		break;
	case Format::Unsigned:
		setValidator(unsignedValidators.at(integerSize_));
		naturalWidthInChars_ = std::lround(integerSize_ * std::log10(256.));
		break;
	case Format::Character:
		setMaxLength(1);
		break;
	default:
		Q_ASSERT("Unexpected format value" && 0);
	}
}

GprEdit::GprEdit(std::size_t offsetInInteger, std::size_t integerSize, Format format, QWidget *parent)
	: QLineEdit(parent), naturalWidthInChars_(2 * integerSize), integerSize_(integerSize), offsetInInteger_(offsetInInteger) {

	setupFormat(format);
}

void GprEdit::setGPRValue(std::uint64_t gprValue) {
	std::uint64_t value(0);
	signBit_ = format_ == Format::Signed ? 1ull << (8 * integerSize_ - 1) : 0;
	if ((gprValue >> 8 * offsetInInteger_) & signBit_)
		value = -1;
	std::memcpy(&value, reinterpret_cast<char *>(&gprValue) + offsetInInteger_, integerSize_);
	switch (format_) {
	case Format::Hex:
		setText(QString("%1").arg(value, naturalWidthInChars_, 16, QChar('0')));
		break;
	case Format::Signed:
		setText(QString("%1").arg(static_cast<std::int64_t>(value)));
		break;
	case Format::Unsigned:
		setText(QString("%1").arg(value));
		break;
	case Format::Character:
		setText(QChar(static_cast<char>(value)));
		break;
	}
}

void GprEdit::updateGPRValue(std::uint64_t &gpr) const {
	bool ok;
	std::uint64_t value;
	switch (format_) {
	case Format::Hex:
		value = text().toULongLong(&ok, 16);
		break;
	case Format::Signed:
		value = text().toLongLong(&ok);
		break;
	case Format::Unsigned:
		value = text().toULongLong(&ok);
		break;
	case Format::Character:
		value = text().toStdString()[0];
		break;
	default:
		Q_ASSERT("Unexpected format value" && 0);
	}
	std::memcpy(reinterpret_cast<char *>(&gpr) + offsetInInteger_, &value, integerSize_);
}

QSize GprEdit::sizeHint() const {

	const auto baseHint = QLineEdit::sizeHint();
	// taking long enough reference char to make enough room even in presence of inner shadows like in Oxygen style
	const auto charWidth       = Font::maxWidth(QFontMetrics(font()));
	const auto textMargins     = this->textMargins();
	const auto contentsMargins = this->contentsMargins();
	int customWidth            = charWidth * naturalWidthInChars_ + textMargins.left() + contentsMargins.left() + textMargins.right() + contentsMargins.right() + 1 * charWidth; // additional char to make edit field not too tight
	return QSize(customWidth, baseHint.height()).expandedTo(QApplication::globalStrut());
}

}
