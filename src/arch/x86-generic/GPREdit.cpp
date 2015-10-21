#include "GPREdit.h"
#include <cstring>
#include <QApplication>
#include <QRegExpValidator>
#include "QLongValidator.h"
#include "QULongValidator.h"

namespace
{
const QRegExpValidator byteHexValidator(QRegExp("[0-9a-fA-F]{0,2}"));
const QRegExpValidator wordHexValidator(QRegExp("[0-9a-fA-F]{0,4}"));
const QRegExpValidator dwordHexValidator(QRegExp("[0-9a-fA-F]{0,8}"));
const QRegExpValidator qwordHexValidator(QRegExp("[0-9a-fA-F]{0,16}"));
const QLongValidator byteSignedValidator(INT8_MIN,INT8_MAX);
const QLongValidator wordSignedValidator(INT16_MIN,INT16_MAX);
const QLongValidator dwordSignedValidator(INT32_MIN,INT32_MAX);
const QLongValidator qwordSignedValidator(INT64_MIN,INT64_MAX);
const QULongValidator byteUnsignedValidator(0,UINT8_MAX);
const QULongValidator wordUnsignedValidator(0,UINT16_MAX);
const QULongValidator dwordUnsignedValidator(0,UINT32_MAX);
const QULongValidator qwordUnsignedValidator(0,UINT64_MAX);

const std::map<int,const QRegExpValidator*> hexValidators={{1,&byteHexValidator},{2,&wordHexValidator},{4,&dwordHexValidator},{8,&qwordHexValidator}};
const std::map<int,const QLongValidator*> signedValidators={{1,&byteSignedValidator},{2,&wordSignedValidator},{4,&dwordSignedValidator},{8,&qwordSignedValidator}};
const std::map<int,const QULongValidator*> unsignedValidators={{1,&byteUnsignedValidator},{2,&wordUnsignedValidator},{4,&dwordUnsignedValidator},{8,&qwordUnsignedValidator}};
}

void GPREdit::setupFormat(Format newFormat)
{
	format=newFormat;
	switch(format)
	{
	case Format::Hex:
		setValidator(hexValidators.at(integerSize));
		naturalWidthInChars=2*integerSize;
		break;
	case Format::Signed:
		setValidator(signedValidators.at(integerSize));
		naturalWidthInChars=1+std::lround(integerSize*std::log10(256.));
		break;
	case Format::Unsigned:
		setValidator(unsignedValidators.at(integerSize));
		naturalWidthInChars=std::lround(integerSize*std::log10(256.));
		break;
	case Format::Character:
		setMaxLength(1);
		break;
	default:
		Q_ASSERT("Unexpected format value" && 0);
	}
}

GPREdit::GPREdit(std::size_t offsetInInteger, std::size_t integerSize, Format newFormat, QWidget* parent)
	: QLineEdit(parent),
	  naturalWidthInChars(2*integerSize),
	  integerSize(integerSize),
	  offsetInInteger(offsetInInteger)
{
	setupFormat(newFormat);
}

void GPREdit::setGPRValue(std::uint64_t gprValue)
{
	std::uint64_t value(0);
	signBit = format==Format::Signed ? 1ull<<(8*integerSize-1) : 0;
	if((gprValue>>8*offsetInInteger)&signBit)
		value=-1;
	std::memcpy(&value,reinterpret_cast<char*>(&gprValue)+offsetInInteger,integerSize);
	switch(format)
	{
	case Format::Hex:
		setText(QString("%1").arg(value,naturalWidthInChars,16,QChar('0')));
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
void GPREdit::updateGPRValue(std::uint64_t& gpr) const
{
	bool ok;
	std::uint64_t value;
	switch(format)
	{
	case Format::Hex:
		value=text().toLongLong(&ok,16);
		break;
	case Format::Signed:
		value=text().toLongLong(&ok);
		break;
	case Format::Unsigned:
		value=text().toULongLong(&ok);
		break;
	case Format::Character:
		value=text().toStdString()[0];
		break;
	default:
	Q_ASSERT("Unexpected format value" && 0);
	}
	std::memcpy(reinterpret_cast<char*>(&gpr)+offsetInInteger,&value,integerSize);
}

QSize GPREdit::sizeHint() const
{
	const auto baseHint=QLineEdit::sizeHint();
	// taking long enough reference char to make enough room even in presence of inner shadows like in Oxygen style
	const auto charWidth = QFontMetrics(font()).width(QLatin1Char('w'));
	const auto textMargins=this->textMargins();
	const auto contentsMargins=this->contentsMargins();
	int customWidth = charWidth * naturalWidthInChars +
		textMargins.left() + contentsMargins.left() + textMargins.right() + contentsMargins.right() +
		1*charWidth; // additional char to make edit field not too tight
	return QSize(customWidth,baseHint.height()).expandedTo(QApplication::globalStrut());
}

