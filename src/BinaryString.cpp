/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
 * @brief Sets the maximum length for the entries in the BinaryString widget. This function adjusts the maximum length for the ASCII, UTF-16, and Hex input fields based on the specified value.
 *
 * @param n The maximum length to set for the entries in the BinaryString widget.
 */
void BinaryString::setEntriesMaxLength(int n) {

	ui->txtAscii->setMaxLength(n);
	ui->txtUTF16->setMaxLength(n / 2);
	ui->txtHex->setMaxLength(n * CharHexLength);
}

/**
 * @brief Sets the maximum length for the BinaryString widget. This function adjusts the mode of the widget based on the specified maximum length and updates the visibility of the "keep size" checkbox accordingly.
 *
 * @param n The maximum length to set for the BinaryString widget. If n is non-zero, the widget will be in LengthLimited mode; otherwise, it will be in MemoryEditing mode.
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
 * @brief Constructs a BinaryString widget with the specified parent and window flags.
 * The widget allows users to input binary data in ASCII, UTF-16, and Hex formats, and provides options for keeping the size of the input consistent.
 *
 * @param parent The parent widget.
 * @param f The window flags.
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
 * @brief Destructor for the BinaryString widget. Cleans up the user interface and releases any allocated resources.
 */
BinaryString::~BinaryString() {
	delete ui;
}

/**
 * @brief Handles the state change of the "keep size" checkbox.
 * This function is called when the state of the checkbox changes, and it adjusts the maximum length
 * of the entries in the BinaryString widget based on the current mode and the state of the checkbox.
 *
 * @param state The new state of the "keep size" checkbox (Qt::Checked or Qt::Unchecked).
 */
void BinaryString::on_keepSize_stateChanged(int state) {

	Q_UNUSED(state)

	if (mode_ != Mode::MemoryEditing) {
		return;
	}

	// There's a comment in get_binary_string_from_user(), that max length must be set before value.
	// FIXME: do we need this here? What does "truncate incorrectly" mean there?
	// NOTE: not doing this for now
	if (ui->keepSize->checkState() == Qt::Unchecked) {
		setEntriesMaxLength(UnlimitedMaxLength);
	} else {
		setEntriesMaxLength(valueOriginalLength_);
	}
}

/**
 * @brief Handles the event when the text in the ASCII input field is edited.
 * This function converts the ASCII input to Hex and UTF-16 formats and updates the corresponding input fields accordingly.
 *
 * @param text The text that was edited.
 */
void BinaryString::on_txtAscii_textEdited(const QString &text) {

	const QByteArray p = text.toLatin1();
	QString textHex;
	QString textUTF16;
	QString temp;
	uint16_t utf16Char = 0;

	int counter = 0;

	for (uint8_t ch : p) {
		textHex += QString::asprintf("%02x ", ch & 0xff);
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
 * @brief Handles the event when the text in the UTF-16 input field is edited.
 * This function converts the UTF-16 input to ASCII and Hex formats and updates the corresponding input fields accordingly.
 *
 * @param text The text that was edited.
 */
void BinaryString::on_txtUTF16_textEdited(const QString &text) {

	QString textAscii;
	QString textHex;
	QString temp;

	for (QChar i : text) {
		const uint16_t ch = i.unicode();

#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
		textAscii += static_cast<char>(ch & 0xff);
		textAscii += static_cast<char>((ch >> 8) & 0xff);
		textHex += QString::asprintf("%02x %02x ", ch & 0xff, (ch >> 8) & 0xff);
#else
		textAscii += static_cast<char>((ch >> 8) & 0xff);
		textAscii += static_cast<char>(ch & 0xff);
		textHex += QString::asprintf("%02x %02x ", (ch >> 8) & 0xff, ch & 0xff);
#endif
	}

	ui->txtHex->setText(textHex.simplified());
	ui->txtAscii->setText(textAscii);
}

/**
 * @brief Handles the event when the text in the Hex input field is edited.
 * This function converts the Hex input to ASCII and UTF-16 formats and updates the corresponding input fields accordingly.
 *
 * @param text The text that was edited.
 */
void BinaryString::on_txtHex_textEdited(const QString &text) {

	uint16_t utf16Char = 0;
	int counter        = 0;

	QString textAscii;
	QString textUTF16;

	const QStringList list1 = text.split(" ", Qt::SkipEmptyParts);
	for (const QString &s : list1) {

		const auto ch = static_cast<uint8_t>(s.toUInt(nullptr, 16));

#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
		utf16Char = (utf16Char >> 8) | (ch << 8);
#else
		utf16Char = (utf16Char << 8) | ch;
#endif

		textAscii += static_cast<char>(ch);

		if (counter++ & 1) {
			textUTF16 += QChar(utf16Char);
		}
	}

	ui->txtUTF16->setText(textUTF16);
	ui->txtAscii->setText(textAscii);
}

/**
 * @brief Gets the current value of the BinaryString widget as a QByteArray.
 * The value is constructed from the Hex input field, converting each hex byte to its corresponding binary representation.
 *
 * @return The current value.
 */
QByteArray BinaryString::value() const {

	QByteArray ret;
	const QStringList list1 = ui->txtHex->text().split(" ", Qt::SkipEmptyParts);
	for (const QString &i : list1) {
		ret += static_cast<uint8_t>(i.toUInt(nullptr, 16));
	}

	return ret;
}

/**
 * @brief Sets the value of the BinaryString widget.
 * This function updates the Hex, ASCII, and UTF-16 input fields with the provided data, converting it to the appropriate formats.
 *
 * @param data The data to set.
 */
void BinaryString::setValue(const QByteArray &data) {

	valueOriginalLength_ = data.size();
	on_keepSize_stateChanged(ui->keepSize->checkState());
	const auto temp = QString::fromLatin1(data.data(), data.size());

	ui->txtAscii->setText(temp);
	on_txtAscii_textEdited(temp);
}

/**
 * @brief Sets the visibility of the "keep size" checkbox in the BinaryString widget.
 *
 * @param visible A boolean value indicating whether the "keep size" checkbox should be visible (true) or hidden (false).
 */
void BinaryString::setShowKeepSize(bool visible) {
	ui->keepSize->setVisible(visible);
}

/**
 * @brief Gets the visibility state of the "keep size" checkbox in the BinaryString widget.
 *
 * @return A boolean value indicating whether the "keep size" checkbox is visible (true) or hidden (false).
 */
bool BinaryString::showKeepSize() const {
	return ui->keepSize->isVisible();
}
