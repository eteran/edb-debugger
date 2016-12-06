#include "DialogEditFPU.h"
#include "Float80Edit.h"
#include <QDebug>
#include <QValidator>
#include <QRegExp>
#include <QLabel>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QVBoxLayout>
#include <cstring>
#include <cmath>
#include <array>
#include <iostream>
#include <iomanip>

DialogEditFPU::DialogEditFPU(QWidget* parent)
	: QDialog(parent),
	  floatEntry(new Float80Edit(this)),
	  hexEntry(new QLineEdit(this))
{
	setWindowTitle(tr("Modify Register"));
	setModal(true);
	const auto allContentsGrid = new QGridLayout();

	allContentsGrid->addWidget(new QLabel(tr("Float"),this), 0, 0);
	allContentsGrid->addWidget(new QLabel(tr("Hex"),this), 1, 0);

	allContentsGrid->addWidget(floatEntry,0,1);
	allContentsGrid->addWidget(hexEntry,1,1);
	connect(floatEntry,SIGNAL(textEdited(const QString&)),this,SLOT(onFloatEdited(const QString&)));
	connect(hexEntry,SIGNAL(textEdited(const QString&)),this,SLOT(onHexEdited(const QString&)));
	hexEntry->setValidator(new QRegExpValidator(QRegExp("[0-9a-fA-F ]{,20}"),this));
	connect(floatEntry,SIGNAL(defocussed()),this,SLOT(updateFloatEntry()));

	const auto okCancel = new QDialogButtonBox(this);
	okCancel->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);
	connect(okCancel, SIGNAL(accepted()), this, SLOT(accept()));
	connect(okCancel, SIGNAL(rejected()), this, SLOT(reject()));

	const auto dialogLayout = new QVBoxLayout(this);
	dialogLayout->addLayout(allContentsGrid);
	dialogLayout->addWidget(okCancel);

	setTabOrder(floatEntry,hexEntry);
	setTabOrder(hexEntry,okCancel);
}

void DialogEditFPU::updateFloatEntry()
{
	floatEntry->setValue(value_);
}

void DialogEditFPU::updateHexEntry()
{
	hexEntry->setText(value_.toHexString());
}

void DialogEditFPU::set_value(const Register& newReg)
{
	reg=newReg;
	value_=reg.value<edb::value80>();
	updateFloatEntry();
	updateHexEntry();
	setWindowTitle(tr("Modify %1").arg(reg.name().toUpper()));
	floatEntry->setFocus(Qt::OtherFocusReason);
}

Register DialogEditFPU::value() const
{
	Register ret(reg);
	ret.setValueFrom(value_);
	return ret;
}

void DialogEditFPU::onHexEdited(const QString& input)
{
	QString readable(input.trimmed());
	readable.replace(' ',"");
	while(readable.size()<20)
		readable='0'+readable;
	const auto byteArray=QByteArray::fromHex(readable.toLatin1());
	auto source=byteArray.constData();
	auto dest=reinterpret_cast<unsigned char*>(&value_);
	for(std::size_t i=0;i<sizeof value_; ++i)
		dest[i]=source[sizeof value_-i-1];
	updateFloatEntry();
}

namespace
{
long double readFloat(const QString& strInput,bool& ok)
{
	ok=false;
	const QString str(strInput.toLower().trimmed());
	std::istringstream stream(str.toStdString());
	long double value;
	stream >> value;
	if(stream)
	{
		// Check that no trailing chars are left
		char c;
		stream >> c;
		if(!stream)
		{
			ok=true;
			return value;
		}
	}
	// OK, so either it is invalid/unfinished, or it's some special value
	// We still do want the user to be able to enter common special values

	static std::array<std::uint8_t,10> positiveInf {0,0,0,0,0,0,0,0x80,0xff,0x7f};
	static std::array<std::uint8_t,10> negativeInf {0,0,0,0,0,0,0,0x80,0xff,0xff};
	static std::array<std::uint8_t,10> positiveSNaN{0,0,0,0,0,0,0,0x90,0xff,0x7f};
	static std::array<std::uint8_t,10> negativeSNaN{0,0,0,0,0,0,0,0x90,0xff,0xff};
	// Indefinite values are used for QNaN
	static std::array<std::uint8_t,10> positiveQNaN{0,0,0,0,0,0,0,0xc0,0xff,0x7f};
	static std::array<std::uint8_t,10> negativeQNaN{0,0,0,0,0,0,0,0xc0,0xff,0xff};
	if(str=="+snan"||str=="snan")
		std::memcpy(&value,&positiveSNaN,sizeof value);
	else if(str=="-snan")
		std::memcpy(&value,&negativeSNaN,sizeof value);
	else if(str=="+qnan"||str=="qnan"||str=="nan")
		std::memcpy(&value,&positiveQNaN,sizeof value);
	else if(str=="-qnan")
		std::memcpy(&value,&negativeQNaN,sizeof value);
	else if(str=="+inf"||str=="inf")
		std::memcpy(&value,&positiveInf,sizeof value);
	else if(str=="-inf")
		std::memcpy(&value,&negativeInf,sizeof value);
	else return 0;

	ok=true;
	return value;
}
}

void DialogEditFPU::onFloatEdited(const QString& str)
{
	bool ok;
	const auto value=readFloat(str,ok);
	if(ok) value_=edb::value80(value);
	updateHexEntry();
}
