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

#include "QDisassemblyView.h"
#include "Configuration.h"
#include "edb.h"
#include "IAnalyzer.h"
#include "ArchProcessor.h"
#include "IDebugger.h"
#include "ISymbolManager.h"
#include "Instruction.h"
#include "MemoryRegions.h"
#include "SyntaxHighlighter.h"
#include "Util.h"

#include <QAbstractItemDelegate>
#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QDebug>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QScrollBar>
#include <QTextDocument>
#include <QTextLayout>
#include <QToolTip>
#include <QtGlobal>
#include <climits>
#include <QElapsedTimer>
#include <algorithm>

namespace {

struct WidgetState1 {
	int version;
	int line1;
	int line2;
	int line3;
};

const int default_byte_width   = 8;
const QColor filling_dis_color = Qt::gray;
const QColor default_dis_color = Qt::blue;
const QColor invalid_dis_color = Qt::blue;
const QColor data_dis_color    = Qt::blue;

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
void draw_rich_text(QPainter *painter, int x, int y, const QTextDocument &text) {
	painter->save();
	painter->translate(x, y);
	QAbstractTextDocumentLayout::PaintContext context;
	context.palette.setColor(QPalette::Text, painter->pen().color());
	text.documentLayout()->draw(painter, context);
	painter->restore();
}

struct show_separator_tag {};

template <class T, size_t N>
struct address_format {};

template <class T>
struct address_format<T, 4> {
	static QString format_address(T address, const show_separator_tag&) {
		static char buffer[10];
		qsnprintf(buffer, sizeof(buffer), "%04x:%04x", (address >> 16) & 0xffff, address & 0xffff);
		return QString::fromLatin1(buffer, sizeof(buffer) - 1);
	}

	static QString format_address(T address) {
		static char buffer[9];
		qsnprintf(buffer, sizeof(buffer), "%04x%04x", (address >> 16) & 0xffff, address & 0xffff);
		return QString::fromLatin1(buffer, sizeof(buffer) - 1);
	}
};

template <class T>
struct address_format<T, 8> {
	static QString format_address(T address, const show_separator_tag&) {
		return edb::value32(address >> 32).toHexString()+":"+edb::value32(address).toHexString();
	}

	static QString format_address(T address) {
		return edb::value64(address).toHexString();
	}
};

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
template <class T>
QString format_address(T address, bool show_separator) {
	if(show_separator) return address_format<T, sizeof(T)>::format_address(address, show_separator_tag());
	else               return address_format<T, sizeof(T)>::format_address(address);
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
bool near_line(int x, int linex) {
	return qAbs(x - linex) < 3;
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
int instruction_size(const quint8 *buffer, std::size_t size) {
	edb::Instruction inst(buffer, buffer + size, 0);
	return inst.size();
}

//------------------------------------------------------------------------------
// Name: length_disasm_back
// Desc:
//------------------------------------------------------------------------------
size_t length_disasm_back(const quint8 *buf,const size_t bufSize,const size_t curInstOffset) {

	// stage 1: try to find longest instruction which ends exactly before current one
	size_t finalSize=0;
	for(size_t offs = curInstOffset-1;offs!=size_t(-1);--offs) {
		const edb::Instruction inst(buf+offs, buf+curInstOffset, 0);
		if(inst && offs+inst.size()==curInstOffset) {
			finalSize=inst.size();
		}
	}
	if(finalSize) return finalSize;

	// stage2: try to find a combination of previous instruction + new current instruction
	// such that it would end exactly at the end of original current instruction
	// we still want previous instruction to be the longest possible
	const edb::Instruction originalCurrentInst(buf+curInstOffset,buf+bufSize,0);
	for(size_t offs = curInstOffset-1;offs!=size_t(-1);--offs) {
		const edb::Instruction instPrev(buf+offs, buf+curInstOffset,0);
		if(!instPrev) continue;
		const edb::Instruction instNewCur(buf+offs+instPrev.size(),buf+bufSize,0);
		if(instNewCur && offs+instPrev.size()+instNewCur.size()==curInstOffset+originalCurrentInst.size()) {
			finalSize=curInstOffset-offs;
		}
	}
	if(finalSize) return finalSize;

	// stage 3: try to make sure the invalid single-byte won't eat the next line becoming
	// a valid instruction: we want exactly one _new_ line above
	for(size_t offs = curInstOffset-1;offs!=size_t(-1);--offs) {
		const edb::Instruction inst(buf+offs, buf+bufSize, 0);
		// all next bytes start with valid insturctions, and this one is one invalid byte
		if(!inst) return curInstOffset-offs;
	}
	// all our tries were fruitless, return failure
	return 0;
}

//------------------------------------------------------------------------------
// Name: format_instruction_bytes
// Desc:
//------------------------------------------------------------------------------
QString format_instruction_bytes(const edb::Instruction &inst, int maxStringPx, const QFontMetricsF &metrics) {
	const QString byte_buffer =
	edb::v1::format_bytes(QByteArray::fromRawData(reinterpret_cast<const char *>(inst.bytes()), inst.size()));
	return metrics.elidedText(byte_buffer, Qt::ElideRight, maxStringPx);
}

//------------------------------------------------------------------------------
// Name: format_instruction_bytes
// Desc:
//------------------------------------------------------------------------------
QString format_instruction_bytes(const edb::Instruction &inst) {
	return edb::v1::format_bytes(QByteArray::fromRawData(reinterpret_cast<const char *>(inst.bytes()), inst.size()));
}

}

//------------------------------------------------------------------------------
// Name: QDisassemblyView
// Desc: constructor
//------------------------------------------------------------------------------
QDisassemblyView::QDisassemblyView(QWidget * parent) : QAbstractScrollArea(parent),
		highlighter_(new SyntaxHighlighter(this)),
		address_offset_(0),
		selected_instruction_address_(0),
		current_address_(0),
        font_height_(0),
        font_width_(0.0),
        icon_width_(0.0),
		line1_(0),
		line2_(0),
		line3_(0),
		selected_instruction_size_(0),
		moving_line1_(false),
		moving_line2_(false),
		moving_line3_(false),
		selecting_address_(false),
        breakpoint_renderer_(QLatin1String(":/debugger/images/breakpoint.svg")),
        current_renderer_(QLatin1String(":/debugger/images/arrow-right.svg")),
        current_bp_renderer_(QLatin1String(":/debugger/images/arrow-right-red.svg")) {

	setShowAddressSeparator(true);

	setFont(QFont("Monospace", 8));
	setMouseTracking(true);
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

	connect(verticalScrollBar(), SIGNAL(actionTriggered(int)), this, SLOT(scrollbar_action_triggered(int)));
}

//------------------------------------------------------------------------------
// Name: ~QDisassemblyView
// Desc:
//------------------------------------------------------------------------------
QDisassemblyView::~QDisassemblyView() {
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void QDisassemblyView::resetColumns() {
	line1_ = 0;
	line2_ = 0;
	line3_ = 0;
	update();
}

//------------------------------------------------------------------------------
// Name: keyPressEvent
//------------------------------------------------------------------------------
void QDisassemblyView::keyPressEvent(QKeyEvent *event) {
    if (event->matches(QKeySequence::MoveToStartOfDocument)) {
        verticalScrollBar()->setValue(0);
    } else if (event->matches(QKeySequence::MoveToEndOfDocument)) {
        verticalScrollBar()->setValue(verticalScrollBar()->maximum());
    } else if (event->matches(QKeySequence::MoveToNextLine)) {
        const edb::address_t next_address = following_instructions(selectedAddress()-address_offset_, 1) + address_offset_;
        if (!addressShown(next_address))
            scrollTo(next_address);
        setSelectedAddress(next_address);
    } else if (event->matches(QKeySequence::MoveToPreviousLine)) {
        const edb::address_t prev_address = previous_instructions(selectedAddress()-address_offset_, 1) + address_offset_;
        if (!addressShown(prev_address))
            scrollTo(prev_address);
        setSelectedAddress(prev_address);
    } else if (event->matches(QKeySequence::MoveToNextPage)) {
        scrollbar_action_triggered(QAbstractSlider::SliderPageStepAdd);
    } else if (event->matches(QKeySequence::MoveToPreviousPage)) {
        scrollbar_action_triggered(QAbstractSlider::SliderPageStepSub);
    } else if (event->key() == Qt::Key_Return) {
        const edb::address_t address = selectedAddress();
        if (address == 0)
            return;
        quint8 buf[edb::Instruction::MAX_SIZE + 1];
        int buf_size = sizeof(buf);
        if (edb::v1::get_instruction_bytes(address, buf, &buf_size)) {
            edb::Instruction inst(buf, buf + buf_size, address);
            if (inst) {
                if(is_call(inst) || is_jump(inst)) {
                    if(inst.operand_count() != 1) {
                        return;
                    }
                    const edb::Operand &oper = inst.operands()[0];
                    if(oper.type() == edb::Operand::TYPE_REL) {
                        const edb::address_t target = oper.relative_target();
                        edb::v1::jump_to_address(target);
                    }
                }
            }
        }
    } else if (event->key() == Qt::Key_Minus) {
        edb::address_t prev_addr = history_.getPrev();
        if (prev_addr != 0) {
            edb::v1::jump_to_address(prev_addr);
        }
    } else if (event->key() == Qt::Key_Plus) {
        edb::address_t next_addr = history_.getNext();
        if (next_addr != 0) {
            edb::v1::jump_to_address(next_addr);
        }
    }
}

//------------------------------------------------------------------------------
// Name: previous_instructions
// Desc: attempts to find the address of the instruction <count> instructions
//       before <current_address>
// Note: <current_address> is a 0 based value relative to the begining of the
//       current region, not an absolute address within the program
//------------------------------------------------------------------------------
edb::address_t QDisassemblyView::previous_instructions(edb::address_t current_address, int count) {

	IAnalyzer *const analyzer = edb::v1::analyzer();
	
	static edb::address_t last_found_function = 0;
	

	for(int i = 0; i < count; ++i) {

		// If we have an analyzer, and the current address is within a function
		// then first we find the begining of that function.
		// Then, we attempt to disassemble from there until we run into
		// the address we were on (stopping one instruction early).
		// this allows us to identify with good accuracy where the
		// previous instruction was making upward scrolling more functional.
		//
		// If all else fails, fall back on the old heuristic which works "ok"
		if(analyzer) {
			edb::address_t address = address_offset_ + current_address;

			// find the containing function
			if(Result<edb::address_t> function_address = analyzer->find_containing_function(address, last_found_function)) {
			
				const IAnalyzer::AddressCategory cat = analyzer->category(address, last_found_function);
			
				last_found_function = *function_address;
			
				if(cat != IAnalyzer::ADDRESS_FUNC_START) {

					edb::address_t function_start = *function_address;

					// disassemble from function start until the NEXT address is where we started
					while(true) {				
						quint8 buf[edb::Instruction::MAX_SIZE];

						int buf_size = sizeof(buf);
						if(region_) {
							buf_size = qMin<edb::address_t>((function_start - region_->base()), sizeof(buf));
						}

						if(edb::v1::get_instruction_bytes(function_start, buf, &buf_size)) {
							const edb::Instruction inst(buf, buf + buf_size, function_start);
							if(!inst) {
								break;
							}

							// if the NEXT address would be our target, then
							// we are at the previous instruction!
							if(function_start + inst.size() >= current_address + address_offset_) {
								break;
							}

							function_start += inst.size();
						} else {
							break;
						}
					}

					current_address = (function_start - address_offset_);
					continue;
				}
			}
		}


		// fall back on the old heuristic
		// iteration goal: to get exactly one new line above current instruction line
		static const auto instSize=edb::Instruction::MAX_SIZE;
		quint8 buf[instSize*2];

		int prevInstBytesSize=instSize, curInstBytesSize=instSize;
		if(region_) {
			prevInstBytesSize = qMin<edb::address_t>((current_address - region_->base()), prevInstBytesSize);
		}

		if(!edb::v1::get_instruction_bytes(address_offset_+current_address-prevInstBytesSize,buf,&prevInstBytesSize) ||
		   !edb::v1::get_instruction_bytes(address_offset_+current_address,buf+prevInstBytesSize,&curInstBytesSize)) {
			current_address -= 1;
			break;
		}
		const auto buf_size=prevInstBytesSize+curInstBytesSize;

		const size_t size = length_disasm_back(buf, buf_size, prevInstBytesSize);
		if(!size) {
			current_address -= 1;
			break;
		}

		current_address -= size;
	}

	return current_address;
}

//------------------------------------------------------------------------------
// Name: following_instructions
// Desc:
//------------------------------------------------------------------------------
edb::address_t QDisassemblyView::following_instructions(edb::address_t current_address, int count) {

	for(int i = 0; i < count; ++i) {

		quint8 buf[edb::Instruction::MAX_SIZE + 1];

		// do the longest read we can while still not passing the region end
		int buf_size = sizeof(buf);
		if(region_) {
			buf_size = qMin<edb::address_t>((region_->end() - current_address), sizeof(buf));
		}

		// read in the bytes...
		if(!edb::v1::get_instruction_bytes(address_offset_ + current_address, buf, &buf_size)) {
			current_address += 1;
			break;
		} else {
			const edb::Instruction inst(buf, buf + buf_size, current_address);
			if(inst) {
				current_address += inst.size();
			} else {
				current_address += 1;
				break;
			}
		}
	}

	return current_address;
}

//------------------------------------------------------------------------------
// Name: wheelEvent
// Desc:
//------------------------------------------------------------------------------
void QDisassemblyView::wheelEvent(QWheelEvent *e) {

	const int dy = e->delta();
	const int scroll_count = dy / 120;

	// Ctrl+Wheel scrolls by single bytes
	if(e->modifiers() & Qt::ControlModifier) {
		edb::address_t address = verticalScrollBar()->value();
		verticalScrollBar()->setValue(address - scroll_count);
		e->accept();
		return;
	}

	const int abs_scroll_count = qAbs(scroll_count);

	if(e->delta() > 0) {
		// scroll up
		edb::address_t address = verticalScrollBar()->value();
		address = previous_instructions(address, abs_scroll_count);
		verticalScrollBar()->setValue(address);
	} else {
		// scroll down
		edb::address_t address = verticalScrollBar()->value();
		address = following_instructions(address, abs_scroll_count);
		verticalScrollBar()->setValue(address);
	}
}

//------------------------------------------------------------------------------
// Name: scrollbar_action_triggered
// Desc:
//------------------------------------------------------------------------------
void QDisassemblyView::scrollbar_action_triggered(int action) {

	if(QApplication::keyboardModifiers() & Qt::ControlModifier) {
		return;
	}

	switch(action) {
	case QAbstractSlider::SliderSingleStepSub:
		{
			edb::address_t address = verticalScrollBar()->value();
			address = previous_instructions(address, 1);
			verticalScrollBar()->setSliderPosition(address);
		}
		break;
	case QAbstractSlider::SliderPageStepSub:
		{
			edb::address_t address = verticalScrollBar()->value();
			address = previous_instructions(address, verticalScrollBar()->pageStep());
			verticalScrollBar()->setSliderPosition(address);
		}
		break;
	case QAbstractSlider::SliderSingleStepAdd:
		{
			edb::address_t address = verticalScrollBar()->value();
			address = following_instructions(address, 1);
			verticalScrollBar()->setSliderPosition(address);
		}
		break;
	case QAbstractSlider::SliderPageStepAdd:
		{
			edb::address_t address = verticalScrollBar()->value();
			address = following_instructions(address, verticalScrollBar()->pageStep());
			verticalScrollBar()->setSliderPosition(address);
		}
		break;

	case QAbstractSlider::SliderToMinimum:
	case QAbstractSlider::SliderToMaximum:
	case QAbstractSlider::SliderMove:
	case QAbstractSlider::SliderNoAction:
	default:
		break;
	}
}

//------------------------------------------------------------------------------
// Name: setShowAddressSeparator
// Desc:
//------------------------------------------------------------------------------
void QDisassemblyView::setShowAddressSeparator(bool value) {
	show_address_separator_ = value;
}

//------------------------------------------------------------------------------
// Name: formatAddress
// Desc:
//------------------------------------------------------------------------------
QString QDisassemblyView::formatAddress(edb::address_t address) const {
	if(edb::v1::debuggeeIs32Bit())
		return format_address<quint32>(address.toUint(), show_address_separator_);
	else
		return format_address(address, show_address_separator_);
}

//------------------------------------------------------------------------------
// Name: update
// Desc:
//------------------------------------------------------------------------------
void QDisassemblyView::update() {
	viewport()->update();
	Q_EMIT signal_updated();
}

//------------------------------------------------------------------------------
// Name: addressShown
// Desc: returns true if a given address is in the visible range
//------------------------------------------------------------------------------
bool QDisassemblyView::addressShown(edb::address_t address) const {
	return show_addresses_.contains(address);
}

//------------------------------------------------------------------------------
// Name: setCurrentAddress
// Desc: sets the 'current address' (where EIP is usually)
//------------------------------------------------------------------------------
void QDisassemblyView::setCurrentAddress(edb::address_t address) {
	current_address_ = address;
}

//------------------------------------------------------------------------------
// Name: setRegion
// Desc: sets the memory region we are viewing
//------------------------------------------------------------------------------
void QDisassemblyView::setRegion(const IRegion::pointer &r) {

	// You may wonder when we use r's compare instead of region_
	// well, the compare function will test if the parameter is NULL
	// so if we it this way, region_ can be NULL and this code is still
	// correct :-)
	// We also check for !r here because we want to be able to reset the
	// the region to nothing. It's fairly harmless to reset an already
	// reset region, so we don't bother check that condition
	if((r && r->compare(region_) != 0) || (!r)) {
		region_ = r;
		updateScrollbars();
		Q_EMIT regionChanged();

		if(line1_ != 0 && line1_ < auto_line1()) {
			line1_ = 0;
		}
	}
	update();
}

//------------------------------------------------------------------------------
// Name: clear
// Desc: clears the display
//------------------------------------------------------------------------------
void QDisassemblyView::clear() {
	setRegion(IRegion::pointer());
}

//------------------------------------------------------------------------------
// Name: setAddressOffset
// Desc:
//------------------------------------------------------------------------------
void QDisassemblyView::setAddressOffset(edb::address_t address) {
	address_offset_ = address;
}

//------------------------------------------------------------------------------
// Name: scrollTo
// Desc:
//------------------------------------------------------------------------------
void QDisassemblyView::scrollTo(edb::address_t address) {
	verticalScrollBar()->setValue(address - address_offset_);
}

bool targetIsLocal(edb::address_t targetAddress,edb::address_t insnAddress) {

	const auto insnRegion=edb::v1::memory_regions().find_region(insnAddress);
	const auto targetRegion=edb::v1::memory_regions().find_region(targetAddress);
	return !insnRegion->name().isEmpty() && targetRegion && insnRegion->name()==targetRegion->name();
}

//------------------------------------------------------------------------------
// Name: draw_instruction
// Desc:
//------------------------------------------------------------------------------
int QDisassemblyView::draw_instruction(QPainter &painter, const edb::Instruction &inst, int y, int line_height, int l2, int l3) const {

	const bool is_filling = edb::v1::arch_processor().is_filling(inst);
	int x                 = font_width_ + font_width_ + l2 + (font_width_ / 2);
	const int ret         = inst.size();

	if(inst) {
		QString opcode = QString::fromStdString(edb::v1::formatter().to_string(inst));

		//return metrics.elidedText(byte_buffer, Qt::ElideRight, maxStringPx);

		const bool syntax_highlighting_enabled = edb::v1::config().syntax_highlighting_enabled && inst.rva() != selectedAddress();

		if(is_filling) {
			if(is_filling && syntax_highlighting_enabled) {
				painter.setPen(filling_dis_color);
			}

			opcode = painter.fontMetrics().elidedText(opcode, Qt::ElideRight, (l3 - l2) - font_width_ * 2);

			painter.drawText(
				x,
				y,
				opcode.length() * font_width_,
				line_height,
				Qt::AlignVCenter,
				opcode);
		} else {

			if(is_call(inst) || is_jump(inst)) {
				if(inst.operand_count() == 1) {
					const edb::Operand &oper = inst.operands()[0];
					if(oper.type() == edb::Operand::TYPE_REL) {

						const bool showSymbolicAddresses=edb::v1::config().show_symbolic_addresses;

						static const QRegExp addrPattern("0x[0-9a-fA-F]+");
						const edb::address_t target = oper.relative_target();

						const bool showLocalModuleNames=edb::v1::config().show_local_module_name_in_jump_targets;
						const bool prefixed=showLocalModuleNames || !targetIsLocal(target,inst.rva());
						QString sym = edb::v1::symbol_manager().find_address_name(target,prefixed);
						if(sym.isEmpty() && target==inst.size()+inst.rva())
							sym=(showSymbolicAddresses?"<":"")+QString("next instruction")+(showSymbolicAddresses?">":"");
						else if(sym.isEmpty() && target==inst.rva())
							sym=showSymbolicAddresses ? "$" : "current instruction";

						if(!sym.isEmpty()) {
							if(showSymbolicAddresses)
								opcode.replace(addrPattern,sym);
							else
								opcode.append(QString(" <%2>").arg(sym));
						}
					}
				}
			}

			opcode = painter.fontMetrics().elidedText(opcode, Qt::ElideRight, (l3 - l2) - font_width_ * 2);

			if(syntax_highlighting_enabled) {
				painter.setPen(default_dis_color);
			}

			QTextDocument doc;
			doc.setDefaultFont(font());
			doc.setDocumentMargin(0);
			doc.setPlainText(opcode);
			highlighter_->setDocument(&doc);
			if(syntax_highlighting_enabled)
				draw_rich_text(&painter, x, y, doc);
			else
				painter.drawText(x, y, opcode.length() * font_width_, line_height, Qt::AlignVCenter, opcode);
		}

	} else {
		QString asm_buffer = format_invalid_instruction_bytes(inst, painter);
		asm_buffer = painter.fontMetrics().elidedText(asm_buffer, Qt::ElideRight, (l3 - l2) - font_width_ * 2);

		painter.drawText(
			x,
			y,
			asm_buffer.length() * font_width_,
			line_height,
			Qt::AlignVCenter,
			asm_buffer);
	}

	return ret;
}

//------------------------------------------------------------------------------
// Name: format_invalid_instruction_bytes
// Desc:
//------------------------------------------------------------------------------
QString QDisassemblyView::format_invalid_instruction_bytes(const edb::Instruction &inst, QPainter &painter) const {
	char byte_buffer[32];
	const quint8 *const buf = inst.bytes();

	switch(inst.size()) {
	case 1:
		qsnprintf(byte_buffer, sizeof(byte_buffer), "db 0x%02x", buf[0] & 0xff);
		break;
	case 2:
		qsnprintf(byte_buffer, sizeof(byte_buffer), "dw 0x%02x%02x", buf[1] & 0xff, buf[0] & 0xff);
		break;
	case 4:
		qsnprintf(byte_buffer, sizeof(byte_buffer), "dd 0x%02x%02x%02x%02x", buf[3] & 0xff, buf[2] & 0xff, buf[1] & 0xff, buf[0] & 0xff);
		break;
	case 8:
		qsnprintf(byte_buffer, sizeof(byte_buffer), "dq 0x%02x%02x%02x%02x%02x%02x%02x%02x", buf[7] & 0xff, buf[6] & 0xff, buf[5] & 0xff, buf[4] & 0xff, buf[3] & 0xff, buf[2] & 0xff, buf[1] & 0xff, buf[0] & 0xff);
		break;
	default:
		// we tried...didn't we?
		if(edb::v1::config().syntax_highlighting_enabled)
			painter.setPen(invalid_dis_color);
		return tr("invalid");
	}
	if(edb::v1::config().syntax_highlighting_enabled)
		painter.setPen(data_dis_color);
	return byte_buffer;
}

//------------------------------------------------------------------------------
// Name: paint_line_bg
// Desc: A helper function for painting a rectangle representing a background
// color of one or more lines in the disassembly view.
//------------------------------------------------------------------------------

void QDisassemblyView::paint_line_bg(QPainter& painter, QBrush brush, int line, int num_lines) {
	const auto lh = line_height();
	painter.fillRect(0, lh*line, width(), lh*num_lines, brush);
}

//------------------------------------------------------------------------------
// Name: paintEvent
// Desc:
//------------------------------------------------------------------------------

void QDisassemblyView::paintEvent(QPaintEvent *) {
#if 0
	QElapsedTimer timer;
	timer.start();
#endif

	QPainter painter(viewport());

	const int line_height = this->line_height();
	unsigned int lines_to_render = 1 + (viewport()->height() / line_height);
	const edb::address_t start_address = address_offset_ + verticalScrollBar()->value();
	const int l1 = line1();
	const int l2 = line2();
	const int l3 = line3();

	if(!region_) {
		return;
	}

	const int region_size = region_->size();

	if(region_size == 0) {
		return;
	}

	std::vector<edb::Instruction> instructions;
	show_addresses_.clear();

	edb::address_t end_address = start_address;
	const auto binary_info = edb::v1::get_binary_info(region_);
	unsigned int selected_line = 65535; // can't accidentally hit this
	const auto group= hasFocus() ? QPalette::Active : QPalette::Inactive;


	{ // DISASSEMBLE STEP
		int bufsize      = instruction_buffer_.size();
		quint8 *inst_buf = &instruction_buffer_[0];



		if (!edb::v1::get_instruction_bytes(start_address, inst_buf, &bufsize)) {
			qDebug() << "Failed to read" << bufsize << "bytes from" << QString::number(start_address, 16);
			lines_to_render = 0;
		}

		instructions.reserve(lines_to_render);
		show_addresses_.reserve(lines_to_render);

		unsigned int line = 0;
		int offset = 0;
		while (line < lines_to_render && offset < bufsize) {
			edb::address_t address = start_address + offset;
			instructions.emplace_back(
				&inst_buf[offset], // instruction bytes
				&inst_buf[bufsize], // end of buffer
				address // address of instruction
			);
			show_addresses_.push_back(address);
			if (address == selectedAddress()) {
				selected_line = line;
			}
			// FIXME
			// disassemble the instruction, if it happens that the next byte is the start of a known function
			// then we should treat this like a one byte instruction
			offset += instructions[line].size();
			line++;
		}
		lines_to_render = line;
		end_address += offset;
	}

	{ // HEADER & ALTERNATION BACKGROUND PAINTING STEP
		// paint the header gray
		unsigned int line = 0;
		if (binary_info) {
			auto header_size = binary_info->header_size();
			edb::address_t header_end_address = region_->start() + header_size;
			// Find the number of lines we need to paint with the header
			while (line < lines_to_render && header_end_address > show_addresses_[line]) {
				line++;
			}
			paint_line_bg(painter, QBrush(Qt::lightGray), 0, line);
		}


		line += 1;
		if (line != lines_to_render) {
			const QBrush alternated_base_color = palette().alternateBase();
			if (alternated_base_color != palette().base()) {
				while (line < lines_to_render) {
					paint_line_bg(painter, alternated_base_color, line);
					line += 2;
				}
			}
		}
	}

	{ // SYMBOL NAMES
		painter.setPen(palette().color(group,QPalette::Text));
		const int x = auto_line1();
		const int width = l1 - x;
		if (width > 0) {
			for (unsigned int line = 0; line < lines_to_render; line++) {
				auto address = show_addresses_[line];
				const QString sym = edb::v1::symbol_manager().find_address_name(address);
				if(!sym.isEmpty()) {
					const QString symbol_buffer = painter.fontMetrics().elidedText(sym, Qt::ElideRight, width);

					painter.drawText(
						x,
						line * line_height,
						width,
						line_height,
						Qt::AlignVCenter,
						symbol_buffer
					);
				}
			}
		}
	}

	{ // SELECTION, BREAKPOINT, EIP & ADDRESS
		const QPen address_pen(Qt::red);
		painter.setPen(address_pen);

		const auto addr_width = l1 - icon_width_ - 1;
		for (unsigned int line = 0; line < lines_to_render; line++) {
			auto address = show_addresses_[line];

			const bool has_breakpoint = (edb::v1::find_breakpoint(address) != 0);
			const bool is_selected = line == selected_line;
			const bool is_eip = address == current_address_;

			if (is_selected) {
				paint_line_bg(
					painter, palette().color(group, QPalette::Highlight), line
				);
			}

			QSvgRenderer* icon = nullptr;
			if (is_eip) {
				icon = has_breakpoint ? &current_bp_renderer_ : &current_renderer_;
			} else if (has_breakpoint) {
				icon = &breakpoint_renderer_;
			}

			if (icon) {
				icon->render(&painter, QRectF(1, line*line_height + 1, icon_width_, icon_height_));
			}

			const QString address_buffer = formatAddress(address);
			// draw the address
			painter.drawText(
				icon_width_ + 1,
				line * line_height,
				addr_width,
				line_height,
				Qt::AlignVCenter,
				address_buffer
			);
		}
	}

	{ // INSTRUCTION BYTES AND RELJMP INDICATOR RENDERING
		const int bytes_width = l2 - l1 - font_width_ / 2;
		const auto metrics = painter.fontMetrics();

		auto painter_lambda = [&](edb::Instruction inst, int line) {
			// for relative jumps draw the jump direction indicators
			if(is_jump(inst) && inst.operands()[0].type() == edb::Operand::TYPE_REL) {
				const edb::address_t target = inst.operands()[0].relative_target();

				if(target!=inst.rva()) {
					painter.drawText(
						l2 + font_width_ + (font_width_ / 2),
						line * line_height,
						font_width_,
						line_height,
						Qt::AlignVCenter,
						QString((target > inst.rva()) ? QChar(0x2304) : QChar(0x2303))
					);
				}
			}
			const QString byte_buffer = format_instruction_bytes(
				inst,
				bytes_width,
				metrics
			);

			painter.drawText(
				l1 + (font_width_ / 2),
				line * line_height,
				bytes_width,
				line_height,
				Qt::AlignVCenter,
				byte_buffer
			);
		};

		painter.setPen(palette().color(group,QPalette::Text));

		for (unsigned int line = 0; line < lines_to_render; line++) {
		
			auto inst = instructions[line];
			if (selected_line != line) {
				painter_lambda(inst, line);
			}
		}

		if (selected_line < lines_to_render) {
			painter.setPen(palette().color(group,QPalette::HighlightedText));
			painter_lambda(instructions[selected_line], selected_line);
		}
	}

	{ // FUNCTION MARKER RENDERING
		IAnalyzer *const analyzer = edb::v1::analyzer();
		if (analyzer) {
			const int x = l2 + font_width_;
			painter.setPen(QPen(palette().shadow().color(), 2));
			int next_line = 0;
			analyzer->for_funcs_in_range(show_addresses_[0], show_addresses_[lines_to_render-1], [&](const Function* func) {
				auto entry_addr = func->entry_address();
				auto end_addr = func->end_address();
				unsigned int start_line;

				// Find the start and draw the corner
				for (start_line = next_line; start_line < lines_to_render; start_line++) {
					if (show_addresses_[start_line] == entry_addr) {
						auto y = start_line * line_height;
						// half of a horizontal
						painter.drawLine(
							x,
							y + line_height / 2,
							l2 + (font_width_ / 2) + font_width_,
							y + line_height / 2
						);

						// half of a vertical
						painter.drawLine(
							x,
							y + line_height / 2,
							x,
							y + line_height
						);

						start_line++;
						break;
					}
					if (show_addresses_[start_line] > entry_addr) {
						break;
					}
				}

				unsigned int end_line;

				// find the end and draw the other corner
				for (end_line = start_line; end_line < lines_to_render; end_line++) {
					auto adjusted_end_addr = show_addresses_[end_line] + instructions[end_line].size() - 1;
					if (adjusted_end_addr == end_addr) {
						auto y = end_line * line_height;
						// half of a vertical
						painter.drawLine(
							x,
							y,
							x,
							y + line_height / 2
						);

						// half of a horizontal
						painter.drawLine(
							x,
							y + line_height / 2,
							l2 + (font_width_ / 2) + font_width_,
							y + line_height / 2
						);
						next_line = end_line;
						break;

					}
					if (adjusted_end_addr > end_addr) {
						next_line = end_line;
						break;
					}
				}

				// draw the straight line between them
				painter.drawLine(x, start_line*line_height, x, end_line*line_height);
				return true;

			});
		}
	}

	{ // COMMENT / ANNOTATION RENDERING
		auto x_pos = l3 + font_width_ + (font_width_ / 2);
		auto comment_width = width() - x_pos;

		painter.setPen(palette().color(group, QPalette::Text));

		for (unsigned int line = 0; line < lines_to_render; line++) {
			auto address = show_addresses_[line];

			QString annotation = comments_.value(address, QString(""));
			auto inst = instructions[line];
			if (annotation.isEmpty() && inst && !is_jump(inst) && !is_call(inst)) {
				// draw ascii representations of immediate constants
				unsigned int op_count = inst.operand_count();
				for (unsigned int op_idx = 0; op_idx < op_count; op_idx++) {
					auto oper = inst.operands()[op_idx];
					edb::address_t ascii_address = 0;
					if (oper.type() == edb::Operand::TYPE_REL || oper.type() == edb::Operand::TYPE_IMMEDIATE) {
						ascii_address = oper.relative_target();
					} else if (
						oper.type() == edb::Operand::TYPE_EXPRESSION &&
						oper.expression().index == edb::Operand::Register::X86_REG_INVALID &&
						oper.expression().displacement_present)
					{
						if (oper.expression().base == edb::Operand::Register::X86_REG_RIP) {
							ascii_address += address + oper.owner()->size() + oper.expression().displacement;
						} else if (oper.expression().base == edb::Operand::Register::X86_REG_INVALID && oper.expression().displacement > 0) {
							ascii_address = oper.expression().displacement;
						}
					}

					QString string_param;
					if (edb::v1::get_human_string_at_address(ascii_address, string_param)) {
						annotation.append(string_param);
					}
				}
			}
			painter.drawText(
				x_pos,
				line * line_height,
				comment_width,
				line_height,
				Qt::AlignLeft,
				annotation
			);
		}

	}

	{ // DISASSEMBLY RENDERING
		for (unsigned int line = 0; line < lines_to_render; line++) {
		
			if (selected_line == line) {
				painter.setPen(palette().color(group,QPalette::HighlightedText));
			}
		
			draw_instruction(painter, instructions[line], line * line_height, line_height, l2, l3);
		}
	}

	{ // DIVIDER LINES
		const QPen divider_pen = palette().shadow().color();
		painter.setPen(divider_pen);
		painter.drawLine(l1, 0, l1, height());
		painter.drawLine(l2, 0, l2, height());
		painter.drawLine(l3, 0, l3, height());
	}
#if 0
	qDebug() << "paint:" << timer.elapsed() << "ms";
#endif
}

//------------------------------------------------------------------------------
// Name: setFont
// Desc: overloaded version of setFont, calculates font metrics for later
//------------------------------------------------------------------------------
void QDisassemblyView::setFont(const QFont &f) {

	// TODO: assert that we are using a fixed font & find out if we care?
	QAbstractScrollArea::setFont(f);

	// recalculate all of our metrics/offsets
	const QFontMetricsF metrics(f);
	font_width_  = metrics.width('X');
	font_height_ = metrics.lineSpacing();
    
    // NOTE(eteran): we let the icons be a bit wider than the font itself, since things
    // like arrows don't tend to have square bounds. A ratio of 2:1 seems to look pretty 
    // good on my setup.     
    icon_width_  = font_width_ * 2;
    icon_height_ = font_height_;
      
	updateScrollbars();
}

//------------------------------------------------------------------------------
// Name: resizeEvent
// Desc:
//------------------------------------------------------------------------------
void QDisassemblyView::resizeEvent(QResizeEvent *) {
	updateScrollbars();
	
	const int line_height = this->line_height();
	unsigned int lines_to_render = 1 + (viewport()->height() / line_height);

	instruction_buffer_.resize(edb::Instruction::MAX_SIZE * lines_to_render);
}

//------------------------------------------------------------------------------
// Name: line_height
// Desc:
//------------------------------------------------------------------------------
int QDisassemblyView::line_height() const {
	return std::max({font_height_, icon_height_});
}

//------------------------------------------------------------------------------
// Name: updateScrollbars
// Desc:
//------------------------------------------------------------------------------
void QDisassemblyView::updateScrollbars() {
	if(region_) {
		const unsigned int total_lines    = region_->size();
		const unsigned int viewable_lines = viewport()->height() / line_height();
		const unsigned int scroll_max     = (total_lines > viewable_lines) ? total_lines - 1 : 0;

		verticalScrollBar()->setMaximum(scroll_max);
	} else {
		verticalScrollBar()->setMaximum(0);
	}
}

//------------------------------------------------------------------------------
// Name: auto_line1
// Desc:
//------------------------------------------------------------------------------
int QDisassemblyView::auto_line1() const {
	const unsigned int elements = address_length();
	return (elements * font_width_) + (font_width_ / 2) + icon_width_ + 1;
}

//------------------------------------------------------------------------------
// Name: line1
// Desc:
//------------------------------------------------------------------------------
int QDisassemblyView::line1() const {
	if(line1_ == 0) {
		return auto_line1();
	} else {
		return line1_;
	}
}

//------------------------------------------------------------------------------
// Name: line2
// Desc:
//------------------------------------------------------------------------------
int QDisassemblyView::line2() const {
	if(line2_ == 0) {
		return line1() + (default_byte_width * 3) * font_width_;
	} else {
		return line2_;
	}
}

//------------------------------------------------------------------------------
// Name: line3
// Desc:
//------------------------------------------------------------------------------
int QDisassemblyView::line3() const {
	if(line3_ == 0) {
		return line2() + 50 * font_width_;
	} else {
		return line3_;
	}
}

//------------------------------------------------------------------------------
// Name: address_length
// Desc:
//------------------------------------------------------------------------------
int QDisassemblyView::address_length() const {
	const unsigned int address_len = edb::v1::pointer_size() * CHAR_BIT / 4;
	return address_len + (show_address_separator_ ? 1 : 0);
}

//------------------------------------------------------------------------------
// Name: addressFromPoint
// Desc:
//------------------------------------------------------------------------------
edb::address_t QDisassemblyView::addressFromPoint(const QPoint &pos) const {

	Q_ASSERT(region_);

	const edb::address_t address = address_from_coord(pos.x(), pos.y()) + address_offset_;
	if(address >= region_->end()) {
		return 0;
	}
	return address;
}

//------------------------------------------------------------------------------
// Name: get_instruction_size
// Desc:
//------------------------------------------------------------------------------
Result<int> QDisassemblyView::get_instruction_size(edb::address_t address, quint8 *buf, int *size) const {

	Q_ASSERT(buf);
	Q_ASSERT(size);


	if(*size >= 0) {
		bool ok = edb::v1::get_instruction_bytes(address, buf, size);

		if(ok) {
			return edb::v1::make_result(instruction_size(buf, *size));
		}
	}
	
	return Result<int>(tr("Failed to get instruciton size"), 0);
}

//------------------------------------------------------------------------------
// Name: get_instruction_size
// Desc:
//------------------------------------------------------------------------------
Result<int> QDisassemblyView::get_instruction_size(edb::address_t address) const {

	Q_ASSERT(region_);

	quint8 buf[edb::Instruction::MAX_SIZE];

	// do the longest read we can while still not crossing region end
	int buf_size = sizeof(buf);
	if(region_->end() != 0 && address + buf_size > region_->end()) {

		if(address <= region_->end()) {
			buf_size = region_->end() - address;
		} else {
			buf_size = 0;
		}
	}

	return get_instruction_size(address, buf, &buf_size);
}

//------------------------------------------------------------------------------
// Name: address_from_coord
// Desc:
//------------------------------------------------------------------------------
edb::address_t QDisassemblyView::address_from_coord(int x, int y) const {
	Q_UNUSED(x);

	const int line = y / line_height();
	edb::address_t address = verticalScrollBar()->value();

	// add up all the instructions sizes up to the line we want
	for(int i = 0; i < line; ++i) {

		Result<int> size = get_instruction_size(address_offset_ + address);
		if(size) {
			address += (*size != 0) ? *size : 1;
		} else {
			address += 1;
		}
	}

	return address;
}

//------------------------------------------------------------------------------
// Name: mouseDoubleClickEvent
// Desc:
//------------------------------------------------------------------------------
void QDisassemblyView::mouseDoubleClickEvent(QMouseEvent *event) {
	if(region_) {
		if(event->button() == Qt::LeftButton) {
			if(event->x() < line1()) {
				const edb::address_t address = addressFromPoint(event->pos());

				if(region_->contains(address)) {
					Q_EMIT breakPointToggled(address);
					update();
				}
			}
		}
	}
}

//------------------------------------------------------------------------------
// Name: event
// Desc:
//------------------------------------------------------------------------------
bool QDisassemblyView::event(QEvent *event) {

	if(region_) {
		if(event->type() == QEvent::ToolTip) {
			bool show = false;

			auto helpEvent = static_cast<QHelpEvent *>(event);

			if(helpEvent->x() >= line1() && helpEvent->x() < line2()) {

				const edb::address_t address = addressFromPoint(helpEvent->pos());

				quint8 buf[edb::Instruction::MAX_SIZE];

				// do the longest read we can while still not passing the region end
				int buf_size = qMin<edb::address_t>((region_->end() - address), sizeof(buf));
				if(edb::v1::get_instruction_bytes(address, buf, &buf_size)) {
					const edb::Instruction inst(buf, buf + buf_size, address);
					const QString byte_buffer = format_instruction_bytes(inst);

					if((line1() + byte_buffer.size() * font_width_) > line2()) {
						QToolTip::showText(helpEvent->globalPos(), byte_buffer);
						show = true;
					}
				}
			}

			if(!show) {
				QToolTip::showText(QPoint(), QString());
				event->ignore();
				return true;
			}
		}
	}

	return QAbstractScrollArea::event(event);
}

//------------------------------------------------------------------------------
// Name: mouseReleaseEvent
// Desc:
//------------------------------------------------------------------------------
void QDisassemblyView::mouseReleaseEvent(QMouseEvent *event) {

	Q_UNUSED(event);

	moving_line1_      = false;
	moving_line2_      = false;
	moving_line3_      = false;
	selecting_address_ = false;

	setCursor(Qt::ArrowCursor);
	update();
}

//------------------------------------------------------------------------------
// Name: updateSelectedAddress
// Desc:
//------------------------------------------------------------------------------
void QDisassemblyView::updateSelectedAddress(QMouseEvent *event) {

	if(region_) {
		setSelectedAddress(addressFromPoint(event->pos()));
	}
}

//------------------------------------------------------------------------------
// Name: mousePressEvent
// Desc:
//------------------------------------------------------------------------------
void QDisassemblyView::mousePressEvent(QMouseEvent *event) {

	if(region_) {
		if(event->button() == Qt::LeftButton) {
			if(near_line(event->x(), line1())) {
				moving_line1_ = true;
			} else if(near_line(event->x(), line2())) {
				moving_line2_ = true;
			} else if(near_line(event->x(), line3())) {
				moving_line3_ = true;
			} else {
				updateSelectedAddress(event);
				selecting_address_ = true;
			}
		} else {
			updateSelectedAddress(event);
		}
	}
}

//------------------------------------------------------------------------------
// Name: mouseMoveEvent
// Desc:
//------------------------------------------------------------------------------
void QDisassemblyView::mouseMoveEvent(QMouseEvent *event) {

	if(region_) {
		const int x_pos = event->x();

		if(moving_line1_) {
			if(x_pos > icon_width_ && x_pos + font_width_ < line2()) {
				if(line2_ == 0) {
					line2_ = line2();
				}
				line1_ = x_pos;
			}
			update();
		} else if(moving_line2_) {
			if(x_pos > line1() + font_width_ + font_width_/2 && x_pos + 1 < line3()) {
				if(line3_ == 0) {
					line3_ = line3();
				}
				line2_ = x_pos;
			}
			update();
		} else if(moving_line3_) {
			if(x_pos > line2() + font_width_ && x_pos + 1 < width() - (verticalScrollBar()->width() + 3)) {
				line3_ = x_pos;
			}
			update();
		} else {
			if(near_line(x_pos, line1()) || near_line(x_pos, line2()) || near_line(x_pos, line3())) {
				setCursor(Qt::SplitHCursor);
			} else {
				setCursor(Qt::ArrowCursor);
				if(selecting_address_) {
					updateSelectedAddress(event);
				}
			}
		}
	}
}

//------------------------------------------------------------------------------
// Name: selectedAddress
// Desc:
//------------------------------------------------------------------------------
edb::address_t QDisassemblyView::selectedAddress() const {
	return selected_instruction_address_;
}

//------------------------------------------------------------------------------
// Name: setSelectedAddress
// Desc:
//------------------------------------------------------------------------------
void QDisassemblyView::setSelectedAddress(edb::address_t address) {

	if(region_) {
                history_.add(address);
		const Result<int> size = get_instruction_size(address);

		if(size) {
			selected_instruction_address_ = address;
			selected_instruction_size_    = *size;
		} else {
			selected_instruction_address_ = 0;
			selected_instruction_size_    = 0;
		}

		update();
	}
}

//------------------------------------------------------------------------------
// Name: selectedSize
// Desc:
//------------------------------------------------------------------------------
int QDisassemblyView::selectedSize() const {
	return selected_instruction_size_;
}

//------------------------------------------------------------------------------
// Name: region
// Desc:
//------------------------------------------------------------------------------
IRegion::pointer QDisassemblyView::region() const {
	return region_;
}

//------------------------------------------------------------------------------
// Name: add_comment
// Desc: Adds a comment to the comment hash.
//------------------------------------------------------------------------------
void QDisassemblyView::add_comment(edb::address_t address, QString comment) {
	comments_.insert(address, comment);
}

//------------------------------------------------------------------------------
// Name: remove_comment
// Desc: Removes a comment from the comment hash and returns the number of comments removed.
//------------------------------------------------------------------------------
int QDisassemblyView::remove_comment(edb::address_t address) {
	return comments_.remove(address);
}

//------------------------------------------------------------------------------
// Name: get_comment
// Desc: Returns a comment assigned for an address or a blank string if there is none.
//------------------------------------------------------------------------------
QString QDisassemblyView::get_comment(edb::address_t address) {
	return comments_.value(address, QString(""));
}

//------------------------------------------------------------------------------
// Name: clear_comments
// Desc: Clears all comments in the comment hash.
//------------------------------------------------------------------------------
void QDisassemblyView::clear_comments() {
	comments_.clear();
}

//------------------------------------------------------------------------------
// Name: saveState
// Desc:
//------------------------------------------------------------------------------
QByteArray QDisassemblyView::saveState() const {

	const WidgetState1 state = {
		sizeof(WidgetState1),
		line1_,
		line2_,
		line3_
	};

	char buf[sizeof(WidgetState1)];
	memcpy(buf, &state, sizeof(buf));

	return QByteArray(buf, sizeof(buf));
}

//------------------------------------------------------------------------------
// Name: restoreState
// Desc:
//------------------------------------------------------------------------------
void QDisassemblyView::restoreState(const QByteArray &stateBuffer) {

	WidgetState1 state;

	if(stateBuffer.size() >= static_cast<int>(sizeof(WidgetState1))) {
		memcpy(&state, stateBuffer.data(), sizeof(WidgetState1));

		if(state.version >= static_cast<int>(sizeof(WidgetState1))) {
			line1_ = state.line1;
			line2_ = state.line2;
			line3_ = state.line3;
		}
	}
}
