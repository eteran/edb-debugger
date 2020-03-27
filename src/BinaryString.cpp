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

#include "BinaryString.h"
#include "HexStringValidator.h"

#include <QStringList>

#include "ui_BinaryString.h"

namespace {

constexpr auto CharHexLength = 3; // "hh "

// magic numerator from Qt defaults
constexpr auto UnlimitedMaxLength = 32767 / CharHexLength;

}

/**
 * @brief BinaryString::setEntriesMaxLength
 * @param n
 */
void BinaryString::setEntriesMaxLength(int n) {

	ui->txtAscii->setMaxLength(n);
	ui->txtUTF16->setMaxLength(n / 2);
	ui->txtHex->setMaxLength(n * CharHexLength);
}

/**
 * @brief BinaryString::setMaxLength
 * @param n
 */
void BinaryString::setMaxLength(int n) {
	requestedMaxLength_ = n;
	if (n) {
		mode_ = Mode::LengthLimited;
		ui->keepSize->hide();
	} else {
		mode_ = Mode::MemoryEditing;
		n     = UnlimitedMaxLength;
		ui->keepSize->show();
	}
	setEntriesMaxLength(n);
}

/**
 * @brief BinaryString::BinaryString
 * @param parent
 * @param f
 */
BinaryString::BinaryString(QWidget *parent, Qt::WindowFlags f)
	: QWidget(parent, f), ui(new Ui::BinaryStringWidget) {

	ui->setupUi(this);
	ui->txtHex->setValidator(new HexStringValidator(this));
	ui->keepSize->setFocusPolicy(Qt::TabFocus);
	ui->txtHex->setFocus(Qt::OtherFocusReason);
	connect(ui->keepSize, &QCheckBox::stateChanged, this, &BinaryString::on_keepSize_stateChanged);
}

/**
 * @brief BinaryString::~BinaryString
 */
BinaryString::~BinaryString() {
	// NOTE(eteran): we CAN'T use std::unique_ptr here because it doesn't
	// support incomplete types
	delete ui;
}

/**
 * @brief BinaryString::on_keepSize_stateChanged
 * @param state
 */
void BinaryString::on_keepSize_stateChanged(int state) {

	Q_UNUSED(state)

	if (mode_ != Mode::MemoryEditing) return;

	// There's a comment in get_binary_string_from_user(), that max length must be set before value.
	// FIXME: do we need this here? What does "truncate incorrectly" mean there?
	// NOTE: not doing this for now
	if (ui->keepSize->checkState() == Qt::Unchecked)
		setEntriesMaxLength(UnlimitedMaxLength);
	else
		setEntriesMaxLength(valueOriginalLength_);
}

/**
 * @brief BinaryString::on_txtAscii_textEdited
 * @param text
 */
void BinaryString::on_txtAscii_textEdited(const QString &text) {

	const QByteArray p = text.toLatin1();
	QString textHex;
	QString textUTF16;
	QString temp;
	uint16_t utf16Char = 0;

	int counter = 0;

	for (uint8_t ch : p) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 5, 0)
		textHex += QString::asprintf("%02x ", ch & 0xff);
#else
		textHex += temp.sprintf("%02x ", ch & 0xff);
#endif
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
		utf16Char = (utf16Char >> 8) | (ch << 8);
#else
		utf16Char = (utf16Char << 8) | ch;
#endif
		if (counter++ & 1) {
			textUTF16 += QChar(utf16Char);
		}
	}

	ui->txtHex->setText(textHex.simplified());
	ui->txtUTF16->setText(textUTF16);
}

/**
 * @brief BinaryString::on_txtUTF16_textEdited
 * @param text
 */
void BinaryString::on_txtUTF16_textEdited(const QString &text) {

	QString textAscii;
	QString textHex;
	QString temp;

	for (QChar i : text) {
		const uint16_t ch = i.unicode();

#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
		textAscii += ch & 0xff;
		textAscii += (ch >> 8) & 0xff;
#if QT_VERSION >= QT_VERSION_CHECK(5, 5, 0)
		textHex += QString::asprintf("%02x %02x ", ch & 0xff, (ch >> 8) & 0xff);
#else
		textHex += temp.sprintf("%02x %02x ", ch & 0xff, (ch >> 8) & 0xff);
#endif
#else
		textAscii += (ch >> 8) & 0xff;
		textAscii += ch & 0xff;
#if QT_VERSION >= QT_VERSION_CHECK(5, 5, 0)
		textHex += QString::asprintf("%02x %02x ", (ch >> 8) & 0xff, ch & 0xff);
#else
		textHex += temp.sprintf("%02x %02x ", (ch >> 8) & 0xff, ch & 0xff);
#endif
#endif
	}

	ui->txtHex->setText(textHex.simplified());
	ui->txtAscii->setText(textAscii);
}

/**
 * @brief BinaryString::on_txtHex_textEdited
 * @param text
 */
void BinaryString::on_txtHex_textEdited(const QString &text) {

	uint16_t utf16Char = 0;
	int counter        = 0;

	QString textAscii;
	QString textUTF16;

	const QStringList list1 = text.split(" ", QString::SkipEmptyParts);

	for (const QString &s : list1) {

		const uint8_t ch = s.toUInt(nullptr, 16);

#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
		utf16Char = (utf16Char >> 8) | (ch << 8);
#else
		utf16Char = (utf16Char << 8) | ch;
#endif

		textAscii += ch;

		if (counter++ & 1) {
			textUTF16 += QChar(utf16Char);
		}
	}

	ui->txtUTF16->setText(textUTF16);
	ui->txtAscii->setText(textAscii);
}

/**
 * @brief BinaryString::value
 * @return
 */
QByteArray BinaryString::value() const {

	QByteArray ret;
	const QStringList list1 = ui->txtHex->text().split(" ", QString::SkipEmptyParts);

	for (const QString &i : list1) {
		ret += static_cast<uint8_t>(i.toUInt(nullptr, 16));
	}

	return ret;
}

/**
 * @brief BinaryString::setValue
 * @param data
 */
void BinaryString::setValue(const QByteArray &data) {

	valueOriginalLength_ = data.size();
	on_keepSize_stateChanged(ui->keepSize->checkState());
	const auto temp = QString::fromLatin1(data.data(), data.size());

	ui->txtAscii->setText(temp);
	on_txtAscii_textEdited(temp);
}

/**
 * @brief BinaryString::setShowKeepSize
 * @param visible
 */
void BinaryString::setShowKeepSize(bool visible) {
	ui->keepSize->setVisible(visible);
}

/**
 * @brief BinaryString::showKeepSize
 * @return
 */
bool BinaryString::showKeepSize() const {
	return ui->keepSize->isVisible();
}
