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

static const auto charHexLength=3; // "hh "
// magic numerator from Qt defaults
static const auto UNLIMITED_MAX_LENGTH=32767/charHexLength;

//------------------------------------------------------------------------------
// Name: setEntriesMaxLength
// Desc:
//------------------------------------------------------------------------------
void BinaryString::setEntriesMaxLength(int n) {

	ui->txtAscii->setMaxLength(n);
	ui->txtUTF16->setMaxLength(n / 2);
	ui->txtHex->setMaxLength(n * charHexLength);
}

//------------------------------------------------------------------------------
// Name: setMaxLength
// Desc:
//------------------------------------------------------------------------------
void BinaryString::setMaxLength(int n) {
	requestedMaxLength=n;
	if(n) {
		mode=Mode::LengthLimited;
		ui->keepSize->hide();
	} else {
		mode=Mode::MemoryEditing;
		n=UNLIMITED_MAX_LENGTH;
		ui->keepSize->show();
	}
	setEntriesMaxLength(n);
}

//------------------------------------------------------------------------------
// Name: BinaryString
// Desc: constructor
//------------------------------------------------------------------------------
BinaryString::BinaryString(QWidget *parent) : QWidget(parent),
											  ui(new Ui::BinaryStringWidget),
											  mode(Mode::MemoryEditing),
											  requestedMaxLength(0),
											  valueOriginalLength(0)
{
	ui->setupUi(this);
	ui->txtHex->setValidator(new HexStringValidator(this));
	ui->keepSize->setFocusPolicy(Qt::TabFocus);
	ui->txtHex->setFocus(Qt::OtherFocusReason);
	connect(ui->keepSize,SIGNAL(stateChanged(int)),this,SLOT(on_keepSize_stateChanged()));
}

//------------------------------------------------------------------------------
// Name: ~BinaryString
// Desc: destructor
//------------------------------------------------------------------------------
BinaryString::~BinaryString() {
	delete ui;
}

//------------------------------------------------------------------------------
// Name: on_keepSize_stateChanged
// Desc:
//------------------------------------------------------------------------------
void BinaryString::on_keepSize_stateChanged() {

	if(mode!=Mode::MemoryEditing) return;

	// There's a comment in get_binary_string_from_user(), that max length must be set before value.
	// FIXME: do we need this here? What does "truncate incorrectly" mean there?
	// NOTE: not doing this for now
	if(ui->keepSize->checkState()==Qt::Unchecked)
		setEntriesMaxLength(UNLIMITED_MAX_LENGTH);
	else
		setEntriesMaxLength(valueOriginalLength);
}

//------------------------------------------------------------------------------
// Name: on_txtAscii_textEdited
// Desc:
//------------------------------------------------------------------------------
void BinaryString::on_txtAscii_textEdited(const QString &text) {

	const QByteArray p = text.toLatin1();
	QString textHex;
	QString textUTF16;
	QString temp;
	quint16 utf16Char = 0;

	int counter = 0;

	for(quint8 ch: p) {

		textHex += temp.sprintf("%02x ", ch & 0xff);

#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
		utf16Char = (utf16Char >> 8) | (ch << 8);
#else
		utf16Char = (utf16Char << 8) | ch;
#endif
		if(counter++ & 1) {
			textUTF16 += QChar(utf16Char);
		}
	}

	ui->txtHex->setText(textHex.simplified());
	ui->txtUTF16->setText(textUTF16);
}

//------------------------------------------------------------------------------
// Name: on_txtUTF16_textEdited
// Desc:
//------------------------------------------------------------------------------
void BinaryString::on_txtUTF16_textEdited(const QString &text) {

	QString textAscii;
	QString textHex;
	QString temp;

	for(QChar i: text) {
		const quint16 ch = i.unicode();

#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
		textAscii += ch & 0xff;
		textAscii += (ch >> 8) & 0xff;
		textHex += temp.sprintf("%02x %02x ", ch & 0xff, (ch >> 8) & 0xff);
#else
		textAscii += (ch >> 8) & 0xff;
		textAscii += ch & 0xff;
		textHex += temp.sprintf("%02x %02x ", (ch >> 8) & 0xff, ch & 0xff);
#endif
	}

	ui->txtHex->setText(textHex.simplified());
	ui->txtAscii->setText(textAscii);
}

//------------------------------------------------------------------------------
// Name: on_txtHex_textEdited
// Desc:
//------------------------------------------------------------------------------
void BinaryString::on_txtHex_textEdited(const QString &text) {

	quint16 utf16Char = 0;
	int counter = 0;

	QString textAscii;
	QString textUTF16;

	const QStringList list1 = text.split(" ", QString::SkipEmptyParts);

	for(const QString &s: list1) {

		const quint8 ch = s.toUInt(0, 16);

#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
		utf16Char = (utf16Char >> 8) | (ch << 8);
#else
		utf16Char = (utf16Char << 8) | ch;
#endif

		textAscii += ch;

		if(counter++ & 1) {
			textUTF16 += QChar(utf16Char);
		}
	}

	ui->txtUTF16->setText(textUTF16);
	ui->txtAscii->setText(textAscii);
}

//------------------------------------------------------------------------------
// Name: value
// Desc:
//------------------------------------------------------------------------------
QByteArray BinaryString::value() const {

	QByteArray ret;
	const QStringList list1 = ui->txtHex->text().split(" ", QString::SkipEmptyParts);

	for(const QString &i: list1) {
		ret += static_cast<quint8>(i.toUInt(0, 16));
	}

	return ret;
}

//------------------------------------------------------------------------------
// Name: setValue
// Desc:
//------------------------------------------------------------------------------
void BinaryString::setValue(const QByteArray &data) {

	valueOriginalLength=data.size();
	on_keepSize_stateChanged();
	const QString temp = QString::fromLatin1(data.data(), data.size());

	ui->txtAscii->setText(temp);
	on_txtAscii_textEdited(temp);
}
