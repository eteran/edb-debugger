#include "DialogEditSIMDRegister.h"
#include <QRegExpValidator>
#include <QRegExp>
#include <QApplication>
#include <QLineEdit>
#include <QDebug>
#include "QULongValidator.h"
#include "QLongValidator.h"
#include "FloatX.h"
#include <cstring>
#include <type_traits>
#include <limits>

class NumberEdit : public QLineEdit
{
	int naturalWidthInChars=17; // default roughly as in QLineEdit
	int column_;
	int colSpan_;
public:
	NumberEdit(int column, int colSpan, QWidget* parent=nullptr)
		: QLineEdit(parent),
		  column_(column),
		  colSpan_(colSpan)
	{
	}
	int column() const { return column_; }
	int colSpan() const { return colSpan_; }
	void setNaturalWidthInChars(int nChars)
	{
		naturalWidthInChars=nChars;
	}
	QSize minimumSizeHint() const override
	{ return sizeHint(); }
	QSize sizeHint() const override
	{
		const auto baseHint=QLineEdit::sizeHint();
		// taking long enough reference char to make enough room even in presence of inner shadows like in Oxygen style
		const auto charWidth = QFontMetrics(font()).width(QLatin1Char('w'));
		const auto textMargins=this->textMargins();
		const auto contentsMargins=this->contentsMargins();
		int customWidth = charWidth * naturalWidthInChars +
			textMargins.left() + contentsMargins.left() + textMargins.right() + contentsMargins.right();
		return QSize(customWidth,baseHint.height()).expandedTo(QApplication::globalStrut());
	}
};

template<std::size_t numEntries>
void DialogEditSIMDRegister::setupEntries(const QString& label, std::array<NumberEdit*,numEntries>& entries, int row, const char* slot, int naturalWidthInChars)
{
	QGridLayout& contentsGrid = dynamic_cast<QGridLayout&>(*layout());
	contentsGrid.addWidget(new QLabel(label,this), row, ENTRIES_FIRST_COL-1);
	for(std::size_t entryIndex=0;entryIndex<numEntries;++entryIndex)
	{
		auto& entry=entries[entryIndex];
		const int bytesPerEntry=numBytes/numEntries;
		entry = new NumberEdit(ENTRIES_FIRST_COL+bytesPerEntry*(numEntries-1-entryIndex),bytesPerEntry,this);
		entry->setNaturalWidthInChars(naturalWidthInChars);
		connect(entry,SIGNAL(textEdited(const QString&)),this,slot);
	}
}

DialogEditSIMDRegister::DialogEditSIMDRegister(QWidget* parent)
	: QDialog(parent),
	  byteHexValidator (new QRegExpValidator(QRegExp("[0-9a-fA-F]{0,2}"),this)),
	  wordHexValidator (new QRegExpValidator(QRegExp("[0-9a-fA-F]{0,4}"),this)),
	  dwordHexValidator(new QRegExpValidator(QRegExp("[0-9a-fA-F]{0,8}"),this)),
	  qwordHexValidator(new QRegExpValidator(QRegExp("[0-9a-fA-F]{0,16}"),this)),
	  byteSignedValidator (new QLongValidator(INT8_MIN,INT8_MAX,this)),
	  wordSignedValidator (new QLongValidator(INT16_MIN,INT16_MAX,this)),
	  dwordSignedValidator(new QLongValidator(INT32_MIN,INT32_MAX,this)),
	  qwordSignedValidator(new QLongValidator(INT64_MIN,INT64_MAX,this)),
	  byteUnsignedValidator (new QULongValidator(0,UINT8_MAX,this)),
	  wordUnsignedValidator (new QULongValidator(0,UINT16_MAX,this)),
	  dwordUnsignedValidator(new QULongValidator(0,UINT32_MAX,this)),
	  qwordUnsignedValidator(new QULongValidator(0,UINT64_MAX,this)),
	  float32Validator(new FloatXValidator<float>(this)),
	  float64Validator(new FloatXValidator<double>(this)),
	  mode(IntDisplayMode::Hex)
{
	setWindowTitle(tr("Edit SIMD Register"));
	setModal(true);
	const auto allContentsGrid = new QGridLayout(this);

	for(int byteIndex=0;byteIndex<numBytes;++byteIndex)
	{
		columnLabels[byteIndex] = new QLabel(std::to_string(byteIndex).c_str(),this);
		columnLabels[byteIndex]->setAlignment(Qt::AlignCenter);
		allContentsGrid->addWidget(columnLabels[byteIndex], BYTE_INDICES_ROW, ENTRIES_FIRST_COL+numBytes-1-byteIndex);
	}

	setupEntries(tr("Byte"),bytes,BYTES_ROW,SLOT(onByteEdited()),4);
	setupEntries(tr("Word"),words,WORDS_ROW,SLOT(onWordEdited()),6);
	setupEntries(tr("Doubleword"),dwords,DWORDS_ROW,SLOT(onDwordEdited()),11);
	setupEntries(tr("Quadword"),qwords,QWORDS_ROW,SLOT(onQwordEdited()),21);
	setupEntries(tr("float32"),floats32,FLOATS32_ROW,SLOT(onFloat32Edited()),14);
	setupEntries(tr("float64"),floats64,FLOATS64_ROW,SLOT(onFloat64Edited()),24);

	for(const auto& entry : floats32)
		entry->setValidator(float32Validator);
	for(const auto& entry : floats64)
		entry->setValidator(float64Validator);

	hexSignOKCancelLayout = new QHBoxLayout();
	{
		const auto hexSignRadiosLayout = new QVBoxLayout();
		radioHex = new QRadioButton(tr("Hexadecimal"),this);
		connect(radioHex,SIGNAL(toggled(bool)),this,SLOT(onHexToggled(bool)));
		radioHex->setChecked(true); // must be after connecting of toggled()
		hexSignRadiosLayout->addWidget(radioHex);

		radioSigned = new QRadioButton(tr("Signed"),this);
		connect(radioSigned,SIGNAL(toggled(bool)),this,SLOT(onSignedToggled(bool)));
		hexSignRadiosLayout->addWidget(radioSigned);

		radioUnsigned = new QRadioButton(tr("Unsigned"),this);
		connect(radioUnsigned,SIGNAL(toggled(bool)),this,SLOT(onUnsignedToggled(bool)));
		hexSignRadiosLayout->addWidget(radioUnsigned);

		hexSignOKCancelLayout->addLayout(hexSignRadiosLayout);
	}
	{
		const auto okCancelLayout = new QVBoxLayout();
		okCancelLayout->addItem(new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));

		okCancel = new QDialogButtonBox(QDialogButtonBox::Cancel|QDialogButtonBox::Ok,Qt::Horizontal,this);
		connect(okCancel, SIGNAL(accepted()), this, SLOT(accept()));
		connect(okCancel, SIGNAL(rejected()), this, SLOT(reject()));
		okCancelLayout->addWidget(okCancel);

		hexSignOKCancelLayout->addLayout(okCancelLayout);
	}
	resetLayout();

	for(int byte=numBytes-1;byte>0;--byte)
		setTabOrder(bytes[byte], bytes[byte-1]);
	setTabOrder(bytes.back(), words.front());
	for(int word=numBytes/2-1;word>0;--word)
		setTabOrder(words[word], words[word-1]);
	setTabOrder(words.back(), dwords.front());
	for(int dword=numBytes/4-1;dword>0;--dword)
		setTabOrder(dwords[dword], dwords[dword-1]);
	setTabOrder(dwords.back(), qwords.front());
	for(int qword=numBytes/4-1;qword>0;--qword)
		setTabOrder(qwords[qword], qwords[qword-1]);
	setTabOrder(qwords.back(), floats32.front());
	for(int float32=numBytes/4-1;float32>0;--float32)
		setTabOrder(floats32[float32], floats32[float32-1]);
	setTabOrder(floats32.back(), floats64.front());
	for(int float64=numBytes/4-1;float64>0;--float64)
		setTabOrder(floats64[float64], floats64[float64-1]);
	setTabOrder(floats64.front(), radioHex);
	setTabOrder(radioHex, radioSigned);
	setTabOrder(radioSigned, radioUnsigned);
	setTabOrder(radioUnsigned, okCancel);
}

template<typename T>
void DialogEditSIMDRegister::updateFloatEntries(const std::array<NumberEdit*,numBytes/sizeof(T)>& entries,NumberEdit* notUpdated)
{
	for(std::size_t i=0;i<entries.size();++i)
	{
		if(entries[i]==notUpdated)
			continue;
		T value;
		std::memcpy(&value,&value_[i*sizeof(value)],sizeof(value));
		entries[i]->setText(formatFloat(value));
	}
}

template<typename T>
void DialogEditSIMDRegister::updateIntegralEntries(const std::array<NumberEdit*,numBytes/sizeof(T)>& entries,NumberEdit* notUpdated)
{
	for(std::size_t i=0;i<entries.size();++i)
	{
		if(entries[i]==notUpdated)
			continue;
		T value;
		std::memcpy(&value,&value_[i*sizeof(value)],sizeof(value));
		formatInteger(entries[i],value);
	}
}

void DialogEditSIMDRegister::updateAllEntriesExcept(NumberEdit* notUpdated)
{
	updateIntegralEntries<std::uint8_t>(bytes,notUpdated);
	updateIntegralEntries<std::uint16_t>(words,notUpdated);
	updateIntegralEntries<std::uint32_t>(dwords,notUpdated);
	updateIntegralEntries<std::uint64_t>(qwords,notUpdated);
	updateFloatEntries<edb::value32>(floats32,notUpdated);
	updateFloatEntries<edb::value64>(floats64,notUpdated);
}

void DialogEditSIMDRegister::resetLayout()
{
	QGridLayout* layout = dynamic_cast<QGridLayout*>(this->layout());
	for(int col=ENTRIES_FIRST_COL;col<TOTAL_COLS;++col)
	{
		int i=numBytes-1-(col-ENTRIES_FIRST_COL);

		columnLabels[i]->show();

		const auto& byte=bytes[i];
		layout->addWidget(byte,BYTES_ROW,byte->column(),1,byte->colSpan());
		byte->show();

		const auto& word=words[i/2];
		layout->addWidget(word,WORDS_ROW,word->column(),1,word->colSpan());
		word->show();

		const auto& dword=dwords[i/4];
		layout->addWidget(dword,DWORDS_ROW,dword->column(),1,dword->colSpan());
		dword->show();

		const auto& qword=qwords[i/8];
		layout->addWidget(qword,QWORDS_ROW,qword->column(),1,qword->colSpan());
		qword->show();

		const auto& float32=floats32[i/4];
		layout->addWidget(float32,FLOATS32_ROW,float32->column(),1,float32->colSpan());
		float32->show();

		const auto& float64=floats64[i/8];
		layout->addWidget(float64,FLOATS64_ROW,float64->column(),1,float64->colSpan());
		float64->show();
	}

	for(int row=ENTRIES_FIRST_ROW;row<ROW_AFTER_ENTRIES;++row)
		layout->itemAtPosition(row,LABELS_COL)->widget()->show();

	layout->removeItem(hexSignOKCancelLayout);
	hexSignOKCancelLayout->setParent(nullptr);
	layout->addLayout(hexSignOKCancelLayout, ROW_AFTER_ENTRIES, ENTRIES_FIRST_COL, 1, numBytes);
}

void DialogEditSIMDRegister::hideColumns(EntriesCols afterLastToHide)
{
	QGridLayout* layout = dynamic_cast<QGridLayout*>(this->layout());
	for(int col=ENTRIES_FIRST_COL;col<afterLastToHide;++col)
	{
		int i=numBytes-1-(col-ENTRIES_FIRST_COL);
		Q_ASSERT(0<i && std::size_t(i)<bytes.size());

		columnLabels[i]->hide();

		// Spanned entries shouldn't just be hidden. If they are still in the grid,
		// then we get extra spacing between invisible columns, which is unwanted.
		// So we have to also remove them from the layout.
		layout->removeWidget(bytes[i]);
		bytes[i]->hide();

		layout->removeWidget(words[i/2]);
		words[i/2]->hide();

		layout->removeWidget(dwords[i/4]);
		dwords[i/4]->hide();

		layout->removeWidget(qwords[i/8]);
		qwords[i/8]->hide();

		layout->removeWidget(floats32[i/4]);
		floats32[i/4]->hide();

		layout->removeWidget(floats64[i/8]);
		floats64[i/8]->hide();
	}
	layout->removeItem(hexSignOKCancelLayout);
	hexSignOKCancelLayout->setParent(nullptr);
	layout->addLayout(hexSignOKCancelLayout, ROW_AFTER_ENTRIES, afterLastToHide, 1, TOTAL_COLS-afterLastToHide);
}

void DialogEditSIMDRegister::hideRows(EntriesRows rowToHide)
{
	QGridLayout* layout = dynamic_cast<QGridLayout*>(this->layout());
	for(int col=0;col<TOTAL_COLS;++col)
	{
		const auto item=layout->itemAtPosition(rowToHide,col);
		if(item && item->widget())
			item->widget()->hide();
	}
}

void DialogEditSIMDRegister::set_value(const Register& newReg)
{
	resetLayout();
	assert(newReg.bitSize()<=8*sizeof value_);
	reg=newReg;
	util::markMemory(&value_,value_.size());
	if(QRegExp("mm[0-7]").exactMatch(reg.name()))
	{
		const auto value=reg.value<edb::value64>();
		std::memcpy(&value_,&value,sizeof value);
		hideColumns(MMX_FIRST_COL);
		// MMX registers are never used in float computations, so hide useless rows
		hideRows(FLOATS32_ROW);
		hideRows(FLOATS64_ROW);
	}
	else if(QRegExp("xmm[0-9]+").exactMatch(reg.name()))
	{
		const auto value=reg.value<edb::value128>();
		std::memcpy(&value_,&value,sizeof value);
		hideColumns(XMM_FIRST_COL);
	}
	else if(QRegExp("ymm[0-9]+").exactMatch(reg.name()))
	{
		const auto value=reg.value<edb::value256>();
		std::memcpy(&value_,&value,sizeof value);
		hideColumns(YMM_FIRST_COL);
	}
	else qCritical() << "DialogEditSIMDRegister::set_value(" << reg.name() << "): register type unsupported";
	setWindowTitle(tr("Modify %1").arg(reg.name().toUpper()));
	updateAllEntriesExcept(nullptr);
}

std::uint64_t DialogEditSIMDRegister::readInteger(const NumberEdit* const edit) const
{
	bool ok;
	switch(mode)
	{
	case IntDisplayMode::Hex:
		return edit->text().toULongLong(&ok,16);
		break;
	case IntDisplayMode::Signed:
		return edit->text().toLongLong(&ok);
		break;
	case IntDisplayMode::Unsigned:
		return edit->text().toULongLong(&ok);
		break;
	}
	Q_ASSERT("Unexpected state of radio buttons" && 0);
	return -1;
}
template<typename Integer>
void DialogEditSIMDRegister::formatInteger(NumberEdit* const edit, Integer integer) const
{
	switch(mode)
	{
	case IntDisplayMode::Hex:
		edit->setText(QString("%1").arg(integer,2*sizeof integer,16,QChar('0')));
		break;
	case IntDisplayMode::Signed:
		typedef typename std::remove_reference<Integer>::type Int;
		typedef typename std::make_signed<Int>::type Signed;
		edit->setText(QString("%1").arg(static_cast<Signed>(integer)));
		break;
	case IntDisplayMode::Unsigned:
		edit->setText(QString("%1").arg(integer));
		break;
	}
}

template<typename Integer>
void DialogEditSIMDRegister::onIntegerEdited(QObject* sender,const std::array<NumberEdit*,numBytes/sizeof(Integer)>& elements)
{
	const auto changedElementEdit=dynamic_cast<NumberEdit*>(sender);
	std::size_t elementIndex=std::find(elements.begin(),elements.end(),changedElementEdit)-elements.begin();
	Integer value=readInteger(elements[elementIndex]);
	std::memcpy(&value_[elementIndex*sizeof(value)],&value,sizeof(value));
	updateAllEntriesExcept(elements[elementIndex]);
}
template<typename Float>
void DialogEditSIMDRegister::onFloatEdited(QObject* sender,const std::array<NumberEdit*,numBytes/sizeof(Float)>& elements)
{
	const auto changedFloatEdit=dynamic_cast<NumberEdit*>(sender);
	std::size_t floatIndex=std::find(elements.begin(),elements.end(),changedFloatEdit)-elements.begin();
	bool ok=false;
	auto value=readFloat<Float>(elements[floatIndex]->text(),ok);
	if(ok)
	{
		std::memcpy(&value_[floatIndex*sizeof(value)],&value,sizeof(value));
		updateAllEntriesExcept(elements[floatIndex]);
	}
}

void DialogEditSIMDRegister::onByteEdited()
{
	onIntegerEdited<std::uint8_t>(sender(),bytes);
}
void DialogEditSIMDRegister::onWordEdited()
{
	onIntegerEdited<std::uint16_t>(sender(),words);
}
void DialogEditSIMDRegister::onDwordEdited()
{
	onIntegerEdited<std::uint32_t>(sender(),dwords);
}
void DialogEditSIMDRegister::onQwordEdited()
{
	onIntegerEdited<std::uint64_t>(sender(),qwords);
}
void DialogEditSIMDRegister::onFloat32Edited()
{
	onFloatEdited<float>(sender(),floats32);
}
void DialogEditSIMDRegister::onFloat64Edited()
{
	onFloatEdited<double>(sender(),floats64);
}

void DialogEditSIMDRegister::onHexToggled(bool checked)
{
	if((checked && mode!=IntDisplayMode::Hex) || !bytes.front()->validator())
	{
		mode=IntDisplayMode::Hex;
		for(const auto& byte : bytes)
			byte->setValidator(byteHexValidator);
		for(const auto& word : words)
			word->setValidator(wordHexValidator);
		for(const auto& dword : dwords)
			dword->setValidator(dwordHexValidator);
		for(const auto& qword : qwords)
			qword->setValidator(qwordHexValidator);
		updateAllEntriesExcept(nullptr);
	}
}

void DialogEditSIMDRegister::onSignedToggled(bool checked)
{
	if((checked && mode!=IntDisplayMode::Signed) || !bytes.front()->validator())
	{
		mode=IntDisplayMode::Signed;
		for(const auto& byte : bytes)
			byte->setValidator(byteSignedValidator);
		for(const auto& word : words)
			word->setValidator(wordSignedValidator);
		for(const auto& dword : dwords)
			dword->setValidator(dwordSignedValidator);
		for(const auto& qword : qwords)
			qword->setValidator(qwordSignedValidator);
		updateAllEntriesExcept(nullptr);
	}
}

void DialogEditSIMDRegister::onUnsignedToggled(bool checked)
{
	if((checked && mode!=IntDisplayMode::Unsigned) || !bytes.front()->validator())
	{
		mode=IntDisplayMode::Unsigned;
		for(const auto& byte : bytes)
			byte->setValidator(byteUnsignedValidator);
		for(const auto& word : words)
			word->setValidator(wordUnsignedValidator);
		for(const auto& dword : dwords)
			dword->setValidator(dwordUnsignedValidator);
		for(const auto& qword : qwords)
			qword->setValidator(qwordUnsignedValidator);
		updateAllEntriesExcept(nullptr);
	}
}

Register DialogEditSIMDRegister::value() const
{
	Register out(reg);
	out.setValueFrom(value_);
	return out;
}
