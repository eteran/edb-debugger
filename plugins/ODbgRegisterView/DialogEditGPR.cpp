#include "DialogEditGPR.h"
#include <QDebug>
#include <QRegExp>
#include <QApplication>
#include <QLineEdit>
#include <cstring>
#include <type_traits>
#include <map>
#include <cmath>
#include <tuple>
#include "GPREdit.h"

DialogEditGPR::DialogEditGPR(QWidget* parent)
	: QDialog(parent)
{
	setWindowTitle(tr("Modify Register"));
	setModal(true);
	const auto allContentsGrid = new QGridLayout();

	// Register name labels
	for(std::size_t c=0;c<ENTRY_COLS;++c)
	{
		auto& label=columnLabel(static_cast<Column>(FIRST_ENTRY_COL+c));
		label=new QLabel(this);
		label->setAlignment(Qt::AlignCenter);
		allContentsGrid->addWidget(label, GPR_LABELS_ROW, FIRST_ENTRY_COL+c);
	}

	{
		static const auto formatNames=util::make_array(tr("Hexadecimal"),
												   tr("Signed"),
												   tr("Unsigned"),
												   tr("Character"));
		// Format labels
		for(std::size_t f=0;f<formatNames.size();++f)
		{
			auto& label=rowLabel(static_cast<Row>(FIRST_ENTRY_ROW+f));
			label=new QLabel(formatNames[f],this);
			allContentsGrid->addWidget(label, FIRST_ENTRY_ROW+f, FORMAT_LABELS_COL);
		}
	}
	// All entries but char
	{
		static const auto offsetsInInteger=util::make_array<std::size_t>(0u,0u,0u,1u,0u);
		static const auto integerSizes=util::make_array<std::size_t>(8u,4u,2u,1u,1u);
		static_assert(std::tuple_size<decltype(integerSizes)>::value==DialogEditGPR::ENTRY_COLS,"integerSizes length doesn't equal ENTRY_COLS");
		static_assert(std::tuple_size<decltype(offsetsInInteger)>::value==DialogEditGPR::ENTRY_COLS,"offsetsInInteger length doesn't equal ENTRY_COLS");
		static const auto formats=util::make_array(GPREdit::Format::Hex,
												   GPREdit::Format::Signed,
												   GPREdit::Format::Unsigned);
		for(std::size_t f=0;f<formats.size();++f)
		{
			for(std::size_t c=0;c<ENTRY_COLS;++c)
			{
				auto& entry=this->entry(static_cast<Row>(FIRST_ENTRY_ROW+f),static_cast<Column>(FIRST_ENTRY_COL+c));
				entry=new GPREdit(offsetsInInteger[c], integerSizes[c], formats[f], this);
				connect(entry,SIGNAL(textEdited(const QString&)),this,SLOT(onTextEdited(const QString&)));
				allContentsGrid->addWidget(entry, FIRST_ENTRY_ROW+f, FIRST_ENTRY_COL+c);
			}
		}
	}
	// High byte char
	{
		auto& charHigh=entry(CHAR_ROW,GPR8H_COL);
		charHigh=new GPREdit(1,1,GPREdit::Format::Character,this);
		connect(charHigh,SIGNAL(textEdited(const QString&)),this,SLOT(onTextEdited(const QString&)));
		allContentsGrid->addWidget(charHigh,CHAR_ROW,GPR8H_COL);
	}
	// Low byte char
	{
		auto& charLow=entry(CHAR_ROW,GPR8L_COL);
		charLow=new GPREdit(0,1,GPREdit::Format::Character,this);
		connect(charLow,SIGNAL(textEdited(const QString&)),this,SLOT(onTextEdited(const QString&)));
		allContentsGrid->addWidget(charLow,CHAR_ROW,GPR8L_COL);
	}
	resetLayout();

	const auto okCancel = new QDialogButtonBox(this);
	okCancel->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);
	connect(okCancel, SIGNAL(accepted()), this, SLOT(accept()));
	connect(okCancel, SIGNAL(rejected()), this, SLOT(reject()));

	const auto dialogLayout = new QVBoxLayout(this);
	dialogLayout->addLayout(allContentsGrid);
	dialogLayout->addWidget(okCancel);

	for(std::size_t entry=1;entry<entries.size();++entry)
		setTabOrder(entries[entry-1], entries[entry]);
}

GPREdit*& DialogEditGPR::entry(DialogEditGPR::Row row, DialogEditGPR::Column col)
{
	if(row<ENTRY_ROWS)
		return entries.at((row-FIRST_ENTRY_ROW)*ENTRY_COLS+(col-FIRST_ENTRY_COL));
	if(col==GPR8H_COL)
		return *(entries.end()-2);
	if(col==GPR8L_COL)
		return entries.back();
	Q_ASSERT("Invalid row&col specified" && 0);
	return entries.front(); // silence the compiler
}

void DialogEditGPR::updateAllEntriesExcept(GPREdit* notUpdated)
{
	for(auto entry : entries)
		if(entry!=notUpdated && !entry->isHidden())
			entry->setGPRValue(value_);
}

QLabel*& DialogEditGPR::columnLabel(DialogEditGPR::Column col)
{
	return labels.at(col-FIRST_ENTRY_COL);
}

QLabel*& DialogEditGPR::rowLabel(DialogEditGPR::Row row)
{
	return labels.at(ENTRY_COLS+row-FIRST_ENTRY_ROW);
}

void DialogEditGPR::hideColumn(DialogEditGPR::Column col)
{
	Row fMax = col==GPR8L_COL||col==GPR8H_COL ? ENTRY_ROWS : FULL_LENGTH_ROWS;
	for(std::size_t f=0;f<fMax;++f)
		entry(static_cast<Row>(FIRST_ENTRY_ROW+f),col)->hide();
	columnLabel(col)->hide();
}

void DialogEditGPR::hideRow(Row row)
{
	rowLabel(row)->hide();
	if(row==CHAR_ROW)
	{
		entry(row,GPR8L_COL)->hide();
		entry(row,GPR8H_COL)->hide();
	}
	else
	{
		for(std::size_t c=0;c<FULL_LENGTH_ROWS;++c)
			entry(row,static_cast<Column>(FIRST_ENTRY_COL+c))->hide();
	}
}

void DialogEditGPR::resetLayout()
{
	for(auto entry : entries)
		entry->show();
	for(auto label : labels)
		label->show();
	static const auto colLabelStrings=util::make_array("R?X","E?X","?X","?H","?L");
	static_assert(std::tuple_size<decltype(colLabelStrings)>::value==ENTRY_COLS,"Number of labels not equal to number of entry columns");
	for(std::size_t c=0;c<ENTRY_COLS;++c)
		columnLabel(static_cast<Column>(GPR64_COL+c))->setText(colLabelStrings[c]);
}

void DialogEditGPR::setupEntriesAndLabels()
{
	resetLayout();
	switch(bitSize_)
	{
	case 8:
		hideColumn(GPR8H_COL);
		hideColumn(GPR16_COL);
	case 16:
		hideColumn(GPR32_COL);
	case 32:
		hideColumn(GPR64_COL);
	case 64:
		break;
	default:
		Q_ASSERT("Unsupported bitSize" && 0);
	}

	const QString regName=reg.name().toUpper();
	if(bitSize_==64)
		columnLabel(GPR64_COL)->setText(regName);
	else if(bitSize_==32)
		columnLabel(GPR32_COL)->setText(regName);
	else if(bitSize_==16)
		columnLabel(GPR16_COL)->setText(regName);
	else
		columnLabel(GPR8L_COL)->setText(regName);

	static const auto x86GPRsWithHighBytesAddressable=util::make_array<QString>("EAX","ECX","EDX","EBX","RAX","RCX","RDX","RBX");
	static const auto x86GPRsWithHighBytesNotAddressable=util::make_array<QString>("ESP","EBP","ESI","EDI","RSP","RBP","RSI","RDI");
	static const auto upperGPRs64=util::make_array<QString>("R8","R9","R10","R11","R12","R13","R14","R15");
	static const auto segmentRegs=util::make_array<QString>("ES","CS","SS","DS","FS","GS");
	static const auto rSP=util::make_array<QString>("ESP","RSP");
	static const auto rIP=util::make_array<QString>("EIP","RIP");
	static const auto rFLAGS=util::make_array<QString>("EFLAGS","RFLAGS");
	static const auto mxcsr="MXCSR";

	bool x86GPR=false, upperGPR64=false, stackPtr=false, segmentReg=false, instPtr=false, flags=false;
	using util::contains;
	if(contains(rSP,regName))
		stackPtr=true;
	else if(contains(x86GPRsWithHighBytesNotAddressable,regName))
	{
		x86GPR=true;
		hideColumn(GPR8H_COL);
		if(bitSize_==32)
		{
			hideColumn(GPR8L_COL); // In 32 bit mode low bytes also can't be addressed
			hideRow(CHAR_ROW);
		}
	}
	else if(contains(x86GPRsWithHighBytesAddressable,regName))
		x86GPR=true;
	else if(contains(upperGPRs64,regName))
		upperGPR64=true;
	else if(contains(segmentRegs,regName))
		segmentReg=true;
	else if(contains(rIP,regName))
		instPtr=true;
	else if(contains(rFLAGS,regName))
		flags=true;
	else if(regName==mxcsr)
		flags=true;

	if(x86GPR)
	{
		if(bitSize_==64)
			columnLabel(GPR32_COL)->setText("E"+regName.mid(1));
		columnLabel(GPR16_COL)->setText(regName.mid(1));
		columnLabel(GPR8H_COL)->setText(regName.mid(1,1)+"H");
		if(bitSize_==64 && !contains(x86GPRsWithHighBytesAddressable,regName))
			columnLabel(GPR8L_COL)->setText(regName.mid(1)+"L");
		else
			columnLabel(GPR8L_COL)->setText(regName.mid(1,1)+"L");
	}
	else if(upperGPR64)
	{
		columnLabel(GPR32_COL)->setText(regName+"D");
		columnLabel(GPR16_COL)->setText(regName+"W");
		columnLabel(GPR8L_COL)->setText(regName+"B");
		hideColumn(GPR8H_COL);
	}
	else if(segmentReg || instPtr || flags || stackPtr)
	{
		// These have hex only format
		hideColumn(GPR8H_COL);
		if(bitSize_!=8)
			hideColumn(GPR8L_COL);
		if(bitSize_!=16)
			hideColumn(GPR16_COL);
		if(bitSize_!=32)
			hideColumn(GPR32_COL);
		hideRow(SIGNED_ROW);
		hideRow(UNSIGNED_ROW);
		hideRow(CHAR_ROW);
	}
	else
		qWarning() << "Warning: " << regName << " not found in any list\n";
}

void DialogEditGPR::setupFocus()
{
	for(auto entry : entries)
		if(!entry->isHidden())
		{
			entry->setFocus(Qt::OtherFocusReason);
			break;
		}
}

void DialogEditGPR::set_value(const Register& newReg)
{
	reg=newReg;
	value_=reg.valueAsInteger();
	bitSize_=reg.bitSize();
	setupEntriesAndLabels();
	setWindowTitle(tr("Modify %1").arg(reg.name().toUpper()));
	updateAllEntriesExcept(nullptr);
	setupFocus();
}

Register DialogEditGPR::value() const
{
	Register ret(reg);
	ret.setScalarValue(value_);
	return ret;
}

void DialogEditGPR::onTextEdited(const QString&)
{
	GPREdit* edit=dynamic_cast<GPREdit*>(sender());
	edit->updateGPRValue(value_);
	updateAllEntriesExcept(edit);
}

