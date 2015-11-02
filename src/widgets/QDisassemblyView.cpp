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
size_t length_disasm_back(const quint8 *buf, size_t size) {

	quint8 tmp[edb::Instruction::MAX_SIZE * 2];
	Q_ASSERT(size <= sizeof(tmp));

	std::size_t offs = 0;

	memcpy(tmp, buf, size);

	while(offs < edb::Instruction::MAX_SIZE) {

		const edb::Instruction inst(tmp + offs, tmp + sizeof(tmp), 0);
		if(!inst) {
			return 0;
		}

		const size_t cmdsize = inst.size();
		offs += cmdsize;

		if(offs == edb::Instruction::MAX_SIZE) {
			return cmdsize;
		}
	}
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
		breakpoint_icon_(":/debugger/images/edb14-breakpoint.png"),
		current_address_icon_(":/debugger/images/edb14-arrow.png"),
		highlighter_(new SyntaxHighlighter(this)),
		address_offset_(0),
		selected_instruction_address_(0),
		current_address_(0),
		line1_(0),
		line2_(0),
		line3_(0),
		selected_instruction_size_(0),
		moving_line1_(false),
		moving_line2_(false),
		moving_line3_(false),
		selecting_address_(false) {


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
// Name: keyPressEvent
//------------------------------------------------------------------------------
void QDisassemblyView::keyPressEvent(QKeyEvent *event) {
	if (event->matches(QKeySequence::MoveToStartOfDocument)) {
		verticalScrollBar()->setValue(0);
	} else if (event->matches(QKeySequence::MoveToEndOfDocument)) {
		verticalScrollBar()->setValue(verticalScrollBar()->maximum());
	} else if (event->matches(QKeySequence::MoveToNextLine)) {
		scrollbar_action_triggered(QAbstractSlider::SliderSingleStepAdd);
	} else if (event->matches(QKeySequence::MoveToPreviousLine)) {
		scrollbar_action_triggered(QAbstractSlider::SliderSingleStepSub);
	} else if (event->matches(QKeySequence::MoveToNextPage)) {
		scrollbar_action_triggered(QAbstractSlider::SliderPageStepAdd);
	} else if (event->matches(QKeySequence::MoveToPreviousPage)) {
		scrollbar_action_triggered(QAbstractSlider::SliderPageStepSub);
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

			const IAnalyzer::AddressCategory cat = analyzer->category(address);

			// find the containing function
			bool ok;
			address = analyzer->find_containing_function(address, &ok);

			if(ok && cat != IAnalyzer::ADDRESS_FUNC_START) {

				// disassemble from function start until the NEXT address is where we started
				while(true) {
					quint8 buf[edb::Instruction::MAX_SIZE];

					int buf_size = sizeof(buf);
					if(region_) {
						buf_size = qMin<edb::address_t>((address - region_->base()), sizeof(buf));
					}

					if(edb::v1::get_instruction_bytes(address, buf, &buf_size)) {
						const edb::Instruction inst(buf, buf + buf_size, address);
						if(!inst) {
							break;
						}

						// if the NEXT address would be our target, then
						// we are at the previous instruction!
						if(address + inst.size() >= current_address + address_offset_) {
							break;
						}

						address += inst.size();
					}
				}

				current_address = (address - address_offset_);
				continue;
			}
		}


		// fall back on the old heuristic
		quint8 buf[edb::Instruction::MAX_SIZE];

		int buf_size = sizeof(buf);
		if(region_) {
			buf_size = qMin<edb::address_t>((current_address - region_->base()), sizeof(buf));
		}

		if(!edb::v1::get_instruction_bytes(address_offset_ + current_address - buf_size, buf, &buf_size)) {
			current_address -= 1;
			break;
		}

		const size_t size = length_disasm_back(buf, buf_size);
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

	if(e->modifiers() & Qt::ControlModifier) {
		e->accept();
		return;
	}

	const int dy = qAbs(e->delta());
	const int scroll_count = dy / 120;

	if(e->delta() > 0) {
		// scroll up
		edb::address_t address = verticalScrollBar()->value();
		address = previous_instructions(address, scroll_count);
		verticalScrollBar()->setValue(address);
	} else {
		// scroll down
		edb::address_t address = verticalScrollBar()->value();
		address = following_instructions(address, scroll_count);
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


		if(is_filling) {
			painter.setPen(filling_dis_color);
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
				if(inst.operand_count() != 0) {
					const edb::Operand &oper = inst.operands()[0];
					if(oper.general_type() == edb::Operand::TYPE_REL) {
						const edb::address_t target = oper.relative_target();
						
						const QString sym = edb::v1::symbol_manager().find_address_name(target);
						if(!sym.isEmpty()) {
							opcode.append(QString(" <%2>").arg(sym));
						}
					}
				}
			}

			opcode = painter.fontMetrics().elidedText(opcode, Qt::ElideRight, (l3 - l2) - font_width_ * 2);

			painter.setPen(default_dis_color);
			QTextDocument doc;
			doc.setDefaultFont(font());
			doc.setDocumentMargin(0);
			doc.setPlainText(opcode);
			highlighter_->setDocument(&doc);
			draw_rich_text(&painter, x, y, doc);
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
		painter.setPen(data_dis_color);
		qsnprintf(byte_buffer, sizeof(byte_buffer), "db 0x%02x", buf[0] & 0xff);
		break;
	case 2:
		painter.setPen(data_dis_color);
		qsnprintf(byte_buffer, sizeof(byte_buffer), "dw 0x%02x%02x", buf[1] & 0xff, buf[0] & 0xff);
		break;
	case 4:
		painter.setPen(data_dis_color);
		qsnprintf(byte_buffer, sizeof(byte_buffer), "dd 0x%02x%02x%02x%02x", buf[3] & 0xff, buf[2] & 0xff, buf[1] & 0xff, buf[0] & 0xff);
		break;
	case 8:
		painter.setPen(data_dis_color);
		qsnprintf(byte_buffer, sizeof(byte_buffer), "dq 0x%02x%02x%02x%02x%02x%02x%02x%02x", buf[7] & 0xff, buf[6] & 0xff, buf[5] & 0xff, buf[4] & 0xff, buf[3] & 0xff, buf[2] & 0xff, buf[1] & 0xff, buf[0] & 0xff);
		break;
	default:
		// we tried...didn't we?
		painter.setPen(invalid_dis_color);
		return tr("invalid");
	}
	return byte_buffer;
}

//------------------------------------------------------------------------------
// Name: draw_function_markers
// Desc:
//------------------------------------------------------------------------------
void QDisassemblyView::draw_function_markers(QPainter &painter, edb::address_t address, int l2, int y, int inst_size, IAnalyzer *analyzer) {
	Q_ASSERT(analyzer);
	painter.setPen(QPen(palette().shadow().color(), 2));

	const IAnalyzer::AddressCategory cat = analyzer->category(address);
	const int line_height = this->line_height();
	const int x = l2 + font_width_;

	switch(cat) {
	case IAnalyzer::ADDRESS_FUNC_START:

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

		break;
	case IAnalyzer::ADDRESS_FUNC_BODY:
		if(analyzer->category(address + inst_size - 1) == IAnalyzer::ADDRESS_FUNC_END) {
			goto do_end;
		} else {

			// vertical line
			painter.drawLine(
				x,
				y,
				x,
				y + line_height);

		}
		break;
	case IAnalyzer::ADDRESS_FUNC_END:
	do_end:

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
	default:
		break;
	}
}

//------------------------------------------------------------------------------
// Name: paintEvent
// Desc:
//------------------------------------------------------------------------------
void QDisassemblyView::paintEvent(QPaintEvent *) {

	QPainter painter(viewport());

	const int line_height = qMax(this->line_height(), breakpoint_icon_.height());
	int viewable_lines    = viewport()->height() / line_height;
	int current_line      = verticalScrollBar()->value();
	int row_index         = 0;
	int y                 = 0;
	const int l1          = line1();
	const int l2          = line2();
	const int l3          = line3();


	if(!region_) {
		return;
	}

	const int region_size = region_->size();

	if(region_size == 0) {
		return;
	}
	
	show_addresses_.clear();
	show_addresses_.insert(address_offset_ + current_line);

	const int bytes_width = l2 - l1;

	const QBrush alternated_base_color = palette().alternateBase();
	const QPen divider_pen             = palette().shadow().color();
	const QPen address_pen(Qt::red);

	IAnalyzer *const analyzer = edb::v1::analyzer();
	
	auto binary_info = edb::v1::get_binary_info(region_);

	edb::address_t last_address = 0;
	
	// TODO: read larger chunks of data at a time
	//       my gut tells me that 2 pages is probably enough
	//       perhaps a page-cache in general using QCache
	//       first. I am thinking that it should be safe to
	//       store cache objects which represent 2 page blocks
	//       keyed by the page contining the address we care about
	//       so address: 0x1000 would have pages for [0x1000-0x3000)
	//       and address 0x2000 would have pages for [0x2000-0x4000)
	//       there is overlap, but i think it avoids page bounary issues

	while(viewable_lines >= 0 && current_line < region_size) {
		const edb::address_t address = address_offset_ + current_line;

		quint8 buf[edb::Instruction::MAX_SIZE + 1];

		// do the longest read we can while still not passing the region end
		int buf_size = qMin<edb::address_t>((region_->end() - address), sizeof(buf));

		// read in the bytes...
		if(!edb::v1::get_instruction_bytes(address, buf, &buf_size)) {
			// if the read failed, let's pretend that we were able to read a
			// single 0xff byte so that we have _something_ to display.
			buf_size = 1;
			*buf = 0xff;
		}

		// disassemble the instruction, if it happens that the next byte is the start of a known function
		// then we should treat this like a one byte instruction
		edb::Instruction inst(buf, buf + buf_size, address);
		if(analyzer && (analyzer->category(address + 1) == IAnalyzer::ADDRESS_FUNC_START)) {
			edb::Instruction(buf, buf + 1, address).swap(inst);
		}

		const int inst_size = inst.size();

		if(inst_size == 0) {
			return;
		}

		if(selectedAddress() == address) {
			if(hasFocus()) {
				painter.fillRect(0, y, width(), line_height, palette().color(QPalette::Active, QPalette::Highlight));
			} else {
				painter.fillRect(0, y, width(), line_height, palette().color(QPalette::Inactive, QPalette::Highlight));
			}
		} else

#if 1
		// highlight header of binary
		if(binary_info && address >= region_->start() && address - region_->start() < binary_info->header_size()) {		
			painter.fillRect(0, y, width(), line_height, QBrush(Qt::lightGray));
		} else
#endif
		if(row_index & 1) {
			// draw alternating line backgrounds
			painter.fillRect(0, y, width(), line_height, alternated_base_color);
		}

		if(analyzer) {
			draw_function_markers(painter, address, l2, y, inst_size, analyzer);
		}

		// draw breakpoint icon or eip indicator
		if(address == current_address_) {
			painter.drawPixmap(1, y + 1, current_address_icon_);
		} else if(edb::v1::find_breakpoint(address) != 0) {
			painter.drawPixmap(1, y + 1, breakpoint_icon_);

			// TODO:
			/*
			painter.setPen(Qt::red);
			painter.drawText(
				QRect(1, y, font_width_, font_height_),
				Qt::AlignVCenter,
				//QString(QChar(0x2b24)));
				//QString(QChar(0x25c9)));
				QString(QChar(0x25cf)));
			*/
		}

		// format the different components
		const QString byte_buffer    = format_instruction_bytes(inst, bytes_width, painter.fontMetrics());
		const QString address_buffer = formatAddress(address);

		// draw the address
		painter.setPen(address_pen);
		painter.drawText(
			breakpoint_icon_.width() + 1,
			y,
			address_buffer.length() * font_width_,
			line_height,
			Qt::AlignVCenter,
			address_buffer);

		// draw the data bytes
		if(selectedAddress() != address) {
			painter.setPen(palette().text().color());
		} else {
			painter.setPen(palette().highlightedText().color());
		}

		painter.drawText(
			l1 + (font_width_ / 2),
			y,
			byte_buffer.length() * font_width_,
			line_height,
			Qt::AlignVCenter,
			byte_buffer);


		// optionally draw the symbol name
		const QString sym = edb::v1::symbol_manager().find_address_name(address);
		if(!sym.isEmpty()) {

			const int maxStringPx = l1 - (breakpoint_icon_.width() + 1 + ((address_buffer.length() + 1) * font_width_));

			if(maxStringPx >= font_width_) {
				const QString symbol_buffer = painter.fontMetrics().elidedText(sym, Qt::ElideRight, maxStringPx);

				painter.drawText(
					breakpoint_icon_.width() + 1 + ((address_buffer.length() + 1) * font_width_),
					y,
					symbol_buffer.length() * font_width_,
					line_height,
					Qt::AlignVCenter,
					symbol_buffer);
			}
		}

		// for relative jumps draw the jump direction indicators
		if(is_jump(inst)) {
			if(inst.operands()[0].general_type() == edb::Operand::TYPE_REL) {
				const edb::address_t target = inst.operands()[0].relative_target();

				painter.drawText(
					l2 + font_width_ + (font_width_ / 2),
					y,
					font_width_,
					line_height,
					Qt::AlignVCenter,
					QString((target > address) ? QChar(0x02C7) : QChar(0x02C6))
					);
			}
		}

		// draw the disassembly
		current_line += draw_instruction(painter, inst, y, line_height, l2, l3);
		show_addresses_.insert(address);
		last_address = address;

		//Draw any comments
		QString comment = comments_.value(address, QString(""));
		if (!comment.isEmpty()) {
			painter.drawText(
						l3 + font_width_ + (font_width_ / 2),
						y,
						comment.length() * font_width_,
						line_height,
						Qt::AlignCenter,
						comment);
		}

		y += line_height;
		++row_index;
		--viewable_lines;
	}

	show_addresses_.remove(last_address);

	painter.setPen(divider_pen);
	painter.drawLine(l1, 0, l1, height());
	painter.drawLine(l2, 0, l2, height());
	painter.drawLine(l3, 0, l3, height());
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
	font_height_ = metrics.height();

	updateScrollbars();
}

//------------------------------------------------------------------------------
// Name: resizeEvent
// Desc:
//------------------------------------------------------------------------------
void QDisassemblyView::resizeEvent(QResizeEvent *) {
	updateScrollbars();
}

//------------------------------------------------------------------------------
// Name: line_height
// Desc:
//------------------------------------------------------------------------------
int QDisassemblyView::line_height() const {
	return qMax(font_height_, current_address_icon_.height());
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
	return (elements * font_width_) + (font_width_ / 2) + breakpoint_icon_.width() + 1;
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
int QDisassemblyView::get_instruction_size(edb::address_t address, bool *ok, quint8 *buf, int *size) const {

	Q_ASSERT(ok);
	Q_ASSERT(buf);
	Q_ASSERT(size);

	int ret = 0;

	if(*size < 0) {
		*ok = false;
	} else {
		*ok = edb::v1::get_instruction_bytes(address, buf, size);

		if(*ok) {
			ret = instruction_size(buf, *size);
		}
	}
	return ret;
}

//------------------------------------------------------------------------------
// Name: get_instruction_size
// Desc:
//------------------------------------------------------------------------------
int QDisassemblyView::get_instruction_size(edb::address_t address, bool *ok) const {

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

	return get_instruction_size(address, ok, buf, &buf_size);
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
		bool ok;

		int size = get_instruction_size(address_offset_ + address, &ok);
		if(ok) {
			address += (size != 0) ? size : 1;
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

					if((line1() + (static_cast<int>(inst.size()) * 3) * font_width_) > line2()) {
						const QString byte_buffer = format_instruction_bytes(inst);
						QToolTip::showText(helpEvent->globalPos(), byte_buffer);
						show = true;
					}
				}
			}

			if(!show) {
				QToolTip::showText(helpEvent->globalPos(), QString());
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
			if(x_pos >= auto_line1() && x_pos + font_width_ < line2()) {
				if(line2_ == 0) {
					line2_ = line2();
				}
				line1_ = x_pos;
			}
			update();
		} else if(moving_line2_) {
			if(x_pos > line1() + font_width_ && x_pos + 1 < line3()) {
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
		bool ok;
		const int size = get_instruction_size(address, &ok);

		if(ok) {
			selected_instruction_address_ = address;
			selected_instruction_size_    = size;
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
