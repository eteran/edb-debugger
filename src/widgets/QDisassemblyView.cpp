/*
Copyright (C) 2006 - 2016 Evan Teran
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
#include "ArchProcessor.h"
#include "Configuration.h"
#include "Function.h"
#include "IAnalyzer.h"
#include "IDebugger.h"
#include "IProcess.h"
#include "IRegion.h"
#include "ISymbolManager.h"
#include "Instruction.h"
#include "MemoryRegions.h"
#include "SessionManager.h"
#include "State.h"
#include "SyntaxHighlighter.h"
#include "Util.h"
#include "edb.h"

#include <QAbstractItemDelegate>
#include <QApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QScrollBar>
#include <QTextLayout>
#include <QToolTip>
#include <QtGlobal>

#include <QDebug>

#include <algorithm>
#include <climits>

namespace {

struct WidgetState1 {
	int version;
	int line1;
	int line2;
	int line3;
};

constexpr int default_byte_width = 8;
const QColor filling_dis_color   = Qt::gray;
const QColor default_dis_color   = Qt::blue;
const QColor invalid_dis_color   = Qt::blue;
const QColor data_dis_color      = Qt::blue;

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
	return std::abs(x - linex) < 3;
}

//------------------------------------------------------------------------------
// Name:
// Desc:
//------------------------------------------------------------------------------
int instruction_size(const quint8 *buffer, std::size_t size) {
	edb::Instruction inst(buffer, buffer + size, 0);
	return inst.byte_size();
}

//------------------------------------------------------------------------------
// Name: format_instruction_bytes
// Desc:
//------------------------------------------------------------------------------
QString format_instruction_bytes(const edb::Instruction &inst) {
	auto bytes = QByteArray::fromRawData(reinterpret_cast<const char *>(inst.bytes()), inst.byte_size());
	return edb::v1::format_bytes(bytes);
}

//------------------------------------------------------------------------------
// Name: format_instruction_bytes
// Desc:
//------------------------------------------------------------------------------
QString format_instruction_bytes(const edb::Instruction &inst, int maxStringPx, const QFontMetrics &metrics) {
	const QString byte_buffer = format_instruction_bytes(inst);
	return metrics.elidedText(byte_buffer, Qt::ElideRight, maxStringPx);
}

}

//------------------------------------------------------------------------------
// Name: QDisassemblyView
// Desc: constructor
//------------------------------------------------------------------------------
QDisassemblyView::QDisassemblyView(QWidget * parent) : QAbstractScrollArea(parent),
		highlighter_(new SyntaxHighlighter(this)),
		breakpoint_renderer_(QLatin1String(":/debugger/images/breakpoint.svg")),
		current_renderer_(QLatin1String(":/debugger/images/arrow-right.svg")),
		current_bp_renderer_(QLatin1String(":/debugger/images/arrow-right-red.svg")),
		syntax_cache_(256) {

	setShowAddressSeparator(true);

	setFont(QFont("Monospace", 8));
	setMouseTracking(true);
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

	connect(verticalScrollBar(), &QScrollBar::actionTriggered, this, &QDisassemblyView::scrollbar_action_triggered);
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
		const edb::address_t selected = selectedAddress();
		const int idx = show_addresses_.indexOf(selected);
		if (selected != 0 && idx > 0 && idx < show_addresses_.size() - 1 - partial_last_line_) {
			setSelectedAddress(show_addresses_[idx + 1]);
		} else {
			const int current_offset = selected - address_offset_;
			if(current_offset + 1 >= region_->size()) {
				return ;
			}

			const edb::address_t next_address = address_offset_ + following_instructions(current_offset, 1);
			if (!addressShown(next_address)) {
				scrollTo(show_addresses_.size() > 1 ? show_addresses_[show_addresses_.size() / 3] : next_address);
			}

			setSelectedAddress(next_address);
		}
	} else if (event->matches(QKeySequence::MoveToPreviousLine)) {
		const edb::address_t selected = selectedAddress();
		const int idx = show_addresses_.indexOf(selected);
		if (selected != 0 && idx > 0) {
			// we already know the previous instruction
			setSelectedAddress(show_addresses_[idx - 1]);
		} else {
			const int current_offset = selected - address_offset_;
			if(current_offset <= 0) {
				return;
			}

			const edb::address_t new_address = address_offset_ + previous_instructions(current_offset, 1);
			if (!addressShown(new_address)) {
				scrollTo(new_address);
			}
			setSelectedAddress(new_address);
		}
	} else if (event->matches(QKeySequence::MoveToNextPage) || event->matches(QKeySequence::MoveToPreviousPage)) {
		const int selectedLine = getSelectedLineNumber();
		if(event->matches(QKeySequence::MoveToNextPage)) {
			scrollbar_action_triggered(QAbstractSlider::SliderPageStepAdd);
		} else {
			scrollbar_action_triggered(QAbstractSlider::SliderPageStepSub);
		}
		updateDisassembly(instructions_.size());

		if(unsigned(show_addresses_.size()) > selectedLine) {
			setSelectedAddress(show_addresses_[selectedLine]);
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
	} else if (event->key() == Qt::Key_Down && (event->modifiers() & Qt::ControlModifier)) {
		const int address = verticalScrollBar()->value();
		verticalScrollBar()->setValue(address + 1);
	} else if (event->key() == Qt::Key_Up && (event->modifiers() & Qt::ControlModifier)) {
		const int address = verticalScrollBar()->value();
		verticalScrollBar()->setValue(address - 1);
	}
}

//------------------------------------------------------------------------------
// Name: previous_instructions
// Desc: attempts to find the address of the instruction 1 instructions
//       before <current_address>
// Note: <current_address> is a 0 based value relative to the begining of the
//       current region, not an absolute address within the program
//------------------------------------------------------------------------------
int QDisassemblyView::previous_instruction(IAnalyzer *analyzer, int current_address) {

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
		if(Result<edb::address_t> function_address = analyzer->find_containing_function(address)) {

			if(address != *function_address) {
				edb::address_t function_start = *function_address;

				// disassemble from function start until the NEXT address is where we started
				while(true) {
					uint8_t buf[edb::Instruction::MAX_SIZE];

					size_t buf_size = sizeof(buf);
					if(region_) {
						buf_size = std::min<size_t>((function_start - region_->base()), sizeof(buf));
					}

					if(edb::v1::get_instruction_bytes(function_start, buf, &buf_size)) {
						const edb::Instruction inst(buf, buf + buf_size, function_start);
						if(!inst) {
							break;
						}

						// if the NEXT address would be our target, then
						// we are at the previous instruction!
						if(function_start + inst.byte_size() >= current_address + address_offset_) {
							break;
						}

						function_start += inst.byte_size();
					} else {
						break;
					}
				}

				current_address = (function_start - address_offset_);
				return current_address;
			}
		}
	}


	// fall back on the old heuristic
	// iteration goal: to get exactly one new line above current instruction line
	edb::address_t address = address_offset_ + current_address;
	for(int i = 1; i < edb::Instruction::MAX_SIZE; ++i) {
		edb::address_t prev_address = address - i;
		if(address >= address_offset_) {

			uint8_t buf[edb::Instruction::MAX_SIZE];
			int size = sizeof(buf);
			Result<int> n = get_instruction_size(prev_address, buf, &size);
			if(n && *n == i) {
				return current_address - i;
			}
		}
	}

	// ensure that we make progress even if no instruction could be decoded
	return current_address - 1;
}

//------------------------------------------------------------------------------
// Name: previous_instructions
// Desc: attempts to find the address of the instruction <count> instructions
//       before <current_address>
// Note: <current_address> is a 0 based value relative to the begining of the
//       current region, not an absolute address within the program
//------------------------------------------------------------------------------
int QDisassemblyView::previous_instructions(int current_address, int count) {

	IAnalyzer *const analyzer = edb::v1::analyzer();

	for(int i = 0; i < count; ++i) {
		current_address = previous_instruction(analyzer, current_address);
	}

	return current_address;
}

int QDisassemblyView::following_instruction(int current_address) {
	quint8 buf[edb::Instruction::MAX_SIZE + 1];

	// do the longest read we can while still not passing the region end
	size_t buf_size = sizeof(buf);
	if(region_) {
		buf_size = std::min<size_t>((region_->end() - current_address), sizeof(buf));
	}

	// read in the bytes...
	if(!edb::v1::get_instruction_bytes(address_offset_ + current_address, buf, &buf_size)) {
		return current_address + 1;
	} else {
		const edb::Instruction inst(buf, buf + buf_size, current_address);
		return current_address + inst.byte_size();
	}
}

//------------------------------------------------------------------------------
// Name: following_instructions
// Note: <current_address> is a 0 based value relative to the begining of the
//       current region, not an absolute address within the program
//------------------------------------------------------------------------------
int QDisassemblyView::following_instructions(int current_address, int count) {

	for(int i = 0; i < count; ++i) {
		current_address = following_instruction(current_address);
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
		int address = verticalScrollBar()->value();
		verticalScrollBar()->setValue(address - scroll_count);
		e->accept();
		return;
	}

	const int abs_scroll_count = std::abs(scroll_count);

	if(e->delta() > 0) {
		// scroll up
		int address = verticalScrollBar()->value();
		address = previous_instructions(address, abs_scroll_count);
		verticalScrollBar()->setValue(address);
	} else {
		// scroll down
		int address = verticalScrollBar()->value();
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
		    int address = verticalScrollBar()->value();
			address = previous_instructions(address, 1);
			verticalScrollBar()->setSliderPosition(address);
		}
		break;
	case QAbstractSlider::SliderPageStepSub:
		{
		    int address = verticalScrollBar()->value();
			address = previous_instructions(address, verticalScrollBar()->pageStep());
			verticalScrollBar()->setSliderPosition(address);
		}
		break;
	case QAbstractSlider::SliderSingleStepAdd:
		{
		    int address = verticalScrollBar()->value();
			address = following_instructions(address, 1);
			verticalScrollBar()->setSliderPosition(address);
		}
		break;
	case QAbstractSlider::SliderPageStepAdd:
		{
		    int address = verticalScrollBar()->value();
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
	const auto idx = show_addresses_.indexOf(address);
	// if the last line is only partially rendered, consider it outside the
	// viewport.
	return (idx > 0 && idx < show_addresses_.size() - 1 - partial_last_line_);
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
void QDisassemblyView::setRegion(const std::shared_ptr<IRegion> &r) {

	// You may wonder when we use r's compare instead of region_
	// well, the compare function will test if the parameter is NULL
	// so if we it this way, region_ can be NULL and this code is still
	// correct :-)
	// We also check for !r here because we want to be able to reset the
	// the region to nothing. It's fairly harmless to reset an already
	// reset region, so we don't bother check that condition
	if((r && !r->equals(region_)) || (!r)) {
		region_ = r;		
		setAddressOffset(region_ ? region_->start() : edb::address_t(0));
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
	setRegion(nullptr);
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

	const auto insnRegion   = edb::v1::memory_regions().find_region(insnAddress);
	const auto targetRegion = edb::v1::memory_regions().find_region(targetAddress);
	return !insnRegion->name().isEmpty() && targetRegion && insnRegion->name() == targetRegion->name();
}

//------------------------------------------------------------------------------
// Name: instructionString
// Desc:
//------------------------------------------------------------------------------
QString QDisassemblyView::instructionString(const edb::Instruction &inst) const {
    QString opcode = QString::fromStdString(edb::v1::formatter().to_string(inst));

    if(is_call(inst) || is_jump(inst)) {
        if(inst.operand_count() == 1) {
            const auto oper = inst[0];
            if(is_immediate(oper)) {

                const bool showSymbolicAddresses=edb::v1::config().show_symbolic_addresses;

                static const QRegExp addrPattern(QLatin1String("#?0x[0-9a-fA-F]+"));
                const edb::address_t target = oper->imm;

                const bool showLocalModuleNames=edb::v1::config().show_local_module_name_in_jump_targets;
                const bool prefixed=showLocalModuleNames || !targetIsLocal(target,inst.rva());
                QString sym = edb::v1::symbol_manager().find_address_name(target, prefixed);

                if(sym.isEmpty() && target == inst.byte_size() + inst.rva()) {
                    sym = showSymbolicAddresses ? tr("<next instruction>") : tr("next instruction");
                } else if(sym.isEmpty() && target == inst.rva()) {
                    sym = showSymbolicAddresses ? tr("$") : tr("current instruction");
                }

                if(!sym.isEmpty()) {
                    if(showSymbolicAddresses)
                        opcode.replace(addrPattern, sym);
                    else
                        opcode.append(QString(" <%2>").arg(sym));
                }
            }
        }
    }

    return opcode;
}

//------------------------------------------------------------------------------
// Name: draw_instruction
// Desc:
//------------------------------------------------------------------------------
int QDisassemblyView::draw_instruction(QPainter &painter, const edb::Instruction &inst, int y, int line_height, int l2, int l3, bool selected) {

	const bool is_filling = edb::v1::arch_processor().is_filling(inst);
	int x                 = font_width_ + font_width_ + l2 + (font_width_ / 2);
	const int ret         = inst.byte_size();
	const int inst_pixel_width = l3 - x;

	const bool syntax_highlighting_enabled = edb::v1::config().syntax_highlighting_enabled && !selected;

    QString opcode = instructionString(inst);

	if(is_filling) {
        if(syntax_highlighting_enabled) {
			painter.setPen(filling_dis_color);
		}

		opcode = painter.fontMetrics().elidedText(opcode, Qt::ElideRight, inst_pixel_width);

		painter.drawText(
			x,
			y,
			opcode.length() * font_width_,
			line_height,
			Qt::AlignVCenter,
			opcode);
	} else {

        // NOTE(eteran): do this early, so that elided text still gets the part shown
        // properly highlighted
        QVector<QTextLayout::FormatRange> highlightData;
        if(syntax_highlighting_enabled) {
            highlightData = highlighter_->highlightBlock(opcode);
        }

		opcode = painter.fontMetrics().elidedText(opcode, Qt::ElideRight, inst_pixel_width);

		if(syntax_highlighting_enabled) {
			if(!inst) {
				painter.setPen(invalid_dis_color);
			} else {
				painter.setPen(default_dis_color);
			}

            QPixmap* map = syntax_cache_[opcode];
            if (!map) {

				// create the text layout
				QTextLayout textLayout(opcode, painter.font());

				textLayout.setTextOption(QTextOption(Qt::AlignVCenter));

				textLayout.beginLayout();

				// generate the lines one at a time
				// setting the positions as we go
				Q_FOREVER {
					QTextLine line = textLayout.createLine();

					if (!line.isValid()) {
						break;
					}

					line.setPosition(QPoint(0, 0));
				}

				textLayout.endLayout();

				map = new QPixmap(QSize(opcode.length() * font_width_, line_height) * devicePixelRatio());
				map->setDevicePixelRatio(devicePixelRatio());
				map->fill(Qt::transparent);
				QPainter cache_painter(map);
				cache_painter.setPen(painter.pen());
				cache_painter.setFont(painter.font());

				// now the render the text at the location given
                textLayout.draw(&cache_painter, QPoint(0, 0), highlightData);
				syntax_cache_.insert(opcode, map);
			}
			painter.drawPixmap(x, y, *map);
		} else {
			QRectF rectangle(x, y, opcode.length() * font_width_, line_height);
			painter.drawText(rectangle, Qt::AlignVCenter, opcode);
		}
	}

	return ret;
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
// Name: get_line_of_address
// Desc: A helper function which sets line to the line on which addr appears,
// or returns false if that line does not appear to exist.
//------------------------------------------------------------------------------
boost::optional<unsigned int> QDisassemblyView::get_line_of_address(edb::address_t addr) const {

	if(!show_addresses_.isEmpty()) {
		if (addr >= show_addresses_[0] && addr <= show_addresses_[show_addresses_.size() - 1]) {
			int pos = std::find(show_addresses_.begin(), show_addresses_.end(), addr) - show_addresses_.begin();
			if (pos < show_addresses_.size()) { // address was found
				return pos;
			}
		}
	}

	return boost::none;
}

//------------------------------------------------------------------------------
// Name: updateDisassembly
// Desc: Updates instructions_, show_addresses_, partial_last_line_
//		 Returns update for number of lines_to_render
//------------------------------------------------------------------------------
int QDisassemblyView::updateDisassembly(int lines_to_render) {
	instructions_.clear();
	show_addresses_.clear();

	int bufsize = instruction_buffer_.size();
	quint8 *inst_buf = &instruction_buffer_[0];
	const edb::address_t start_address = address_offset_ + verticalScrollBar()->value();

	if (!edb::v1::get_instruction_bytes(start_address, inst_buf, &bufsize)) {
		qDebug() << "Failed to read" << bufsize << "bytes from" << QString::number(start_address, 16);
		lines_to_render = 0;
	}

	instructions_.reserve(lines_to_render);
	show_addresses_.reserve(lines_to_render);

	const int max_offset = std::min(int(region_->end() - start_address), bufsize);

	int line = 0;
	int offset = 0;

	while (line < lines_to_render && offset < max_offset) {
		edb::address_t address = start_address + offset;
		instructions_.emplace_back(
			&inst_buf[offset], // instruction bytes
			&inst_buf[bufsize], // end of buffer
			address // address of instruction
		);
		show_addresses_.push_back(address);
;
		if(instructions_[line].valid()) {
			offset += instructions_[line].byte_size();
		} else {
			++offset;
		}
		line++;
	}
	Q_ASSERT(line <= lines_to_render);
	if (lines_to_render != line) {
		lines_to_render = line;
		partial_last_line_ = false;
	}

	lines_to_render = line;
	return lines_to_render;
}

int QDisassemblyView::getSelectedLineNumber() const {

	int selected_line = 65535; // can't accidentally hit this
	for(size_t line = 0; line < instructions_.size(); ++line) {
		if (instructions_[line].rva() == selectedAddress()) {
			selected_line = static_cast<int>(line);
		}
	}
	return selected_line;
}

//------------------------------------------------------------------------------
// Name: paintEvent
// Desc:
//------------------------------------------------------------------------------
void QDisassemblyView::paintEvent(QPaintEvent *) {

	QElapsedTimer timer;
	timer.start();


	QPainter painter(viewport());

	const int line_height = this->line_height();
	int lines_to_render = viewport()->height() / line_height;
	// Possibly render another instruction just outside the viewport
	if (viewport()->height() % line_height > 0) {
		lines_to_render++;
		partial_last_line_ = true;
	} else {
		partial_last_line_ = false;
	}

	if(!region_) {
		return;
	}

	const size_t region_size = region_->size();
	if(region_size == 0) {
		return;
	}

	const auto binary_info = edb::v1::get_binary_info(region_);
	const auto group = hasFocus() ? QPalette::Active : QPalette::Inactive;


	lines_to_render = updateDisassembly(lines_to_render);
	const int selected_line = getSelectedLineNumber();

	{ // HEADER & ALTERNATION BACKGROUND PAINTING STEP
		// paint the header gray
		int line = 0;
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
		if (selected_line < lines_to_render) {
			paint_line_bg(painter, palette().color(group, QPalette::Highlight), selected_line);
		}

	}

	// This represents extra space allocated between x=0 and x=line1
	int l0 = 0;

	if(edb::v1::config().show_register_badges) { // REGISTER BADGES

		if(edb::v1::debugger_core->process()->isPaused()) {

			// a reasonable guess for the width of a single register is 3 chars + overhead
			// we do this to prevent "jumpiness"
			l0 = (4 * font_width_ + font_width_/2);

			State state;
			edb::v1::debugger_core->get_state(&state);

			const int badge_x = 1;

			std::vector<QString> badge_labels(lines_to_render);
			{
				unsigned int reg_num = 0;
				Register reg;
				reg = state.gp_register(reg_num);

				while (reg.valid()) {
					// Does addr appear here?
					edb::address_t addr = reg.valueAsAddress();


					if (boost::optional<unsigned int> line = get_line_of_address(addr)) {
						if (!badge_labels[*line].isEmpty()) {
							badge_labels[*line].append(", ");
						}
						badge_labels[*line].append(reg.name());
					}

					// what about [addr]?
					if(IProcess *process = edb::v1::debugger_core->process()) {
						if (process->read_bytes(addr, &addr, edb::v1::pointer_size())) {
							if (boost::optional<unsigned int> line = get_line_of_address(addr)) {
								if (!badge_labels[*line].isEmpty()) {
									badge_labels[*line].append(", ");
								}
								badge_labels[*line].append("[" + reg.name() + "]");
							}
						}
					}

					reg = state.gp_register(++reg_num);
				}
			}

			painter.setPen(Qt::white);
			for (int line = 0; line < lines_to_render; line++) {
				if (!badge_labels[line].isEmpty()) {
					QRect bounds(badge_x, line * line_height, badge_labels[line].length() * font_width_ + font_width_/2, line_height);

					// draw a rectangle + box around text
					QPainterPath path;
					path.addRect(bounds);
					path.moveTo(bounds.x() + bounds.width(), bounds.y()); // top right
					const int largest_x = bounds.x() + bounds.width() + bounds.height()/2;
					if (largest_x > l0) {
						l0 = largest_x;
					}
					path.lineTo(largest_x, bounds.y() + bounds.height()/2); // triangle point
					path.lineTo(bounds.x() + bounds.width(), bounds.y() + bounds.height()); // bottom right
					painter.fillPath(path, Qt::blue);

					painter.drawText(
						badge_x + font_width_/4,
						line * line_height,
						font_width_ * badge_labels[line].size(),
						line_height,
						Qt::AlignVCenter,
						(edb::v1::config().uppercase_disassembly ? badge_labels[line].toUpper() : badge_labels[line])
					);
				}
			}
		}
	}

	line0_ = l0;
	const int l1 = line1() + l0;
	const int l2 = line2() + l0;
	const int l3 = line3() + l0;

	{ // SYMBOL NAMES
		painter.setPen(palette().color(group,QPalette::Text));
		const int x = l0 + auto_line1();
		const int width = l1 - x;
		if (width > 0) {
			for (int line = 0; line < lines_to_render; line++) {
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

		const auto icon_x = l0 + 1;
		const auto addr_x = icon_x + icon_width_;
		const auto addr_width = l1 - addr_x;
		for (int line = 0; line < lines_to_render; line++) {
			auto address = show_addresses_[line];

			const bool has_breakpoint = (edb::v1::find_breakpoint(address) != nullptr);
			const bool is_eip = address == current_address_;

			QSvgRenderer* icon = nullptr;
			if (is_eip) {
				icon = has_breakpoint ? &current_bp_renderer_ : &current_renderer_;
			} else if (has_breakpoint) {
				icon = &breakpoint_renderer_;
			}

			if (icon) {
				icon->render(&painter, QRectF(icon_x, line*line_height + 1, icon_width_, icon_height_));
			}

			const QString address_buffer = formatAddress(address);
			// draw the address
			painter.drawText(
				addr_x,
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

		auto painter_lambda = [&](const edb::Instruction &inst, int line) {
			// for relative jumps draw the jump direction indicators
			if(is_jump(inst) && is_immediate(inst[0])) {
				const edb::address_t target = inst[0]->imm;

				if(target != inst.rva()) {
					painter.drawText(
						l2,
						line * line_height,
						l3 - l2,
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

		for (int line = 0; line < lines_to_render; line++) {

			auto &&inst = instructions_[line];
			if (selected_line != line) {
				painter_lambda(inst, line);
			}
		}

		if (selected_line < lines_to_render) {
			painter.setPen(palette().color(group,QPalette::HighlightedText));
			painter_lambda(instructions_[selected_line], selected_line);
		}
	}

	{ // FUNCTION MARKER RENDERING
		IAnalyzer *const analyzer = edb::v1::analyzer();
		const int x = l2 + font_width_;
		if (analyzer && l3-x > font_width_ / 2) {
			painter.setPen(QPen(palette().shadow().color(), 2));
			int next_line = 0;

			if(lines_to_render != 0 && !show_addresses_.isEmpty()) {
				analyzer->for_funcs_in_range(show_addresses_[0], show_addresses_[lines_to_render-1], [&](const Function* func) {
					auto entry_addr = func->entry_address();
					auto end_addr   = func->end_address();
					int start_line;

					// Find the start and draw the corner
					for (start_line = next_line; start_line < lines_to_render; start_line++) {
						if (show_addresses_[start_line] == entry_addr) {
							auto y = start_line * line_height;
							// half of a horizontal
							painter.drawLine(
							    x,
							    y + line_height / 2,
							    x + font_width_ / 2,
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

					int end_line;

					// find the end and draw the other corner
					for (end_line = start_line; end_line < lines_to_render; end_line++) {
						auto adjusted_end_addr = show_addresses_[end_line] + instructions_[end_line].byte_size() - 1;
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
	}

	{ // COMMENT / ANNOTATION RENDERING
		auto x_pos = l3 + font_width_ + (font_width_ / 2);
		auto comment_width = width() - x_pos;

		for (int line = 0; line < lines_to_render; line++) {
			auto address = show_addresses_[line];

			if (selected_line == line) {
				painter.setPen(palette().color(group, QPalette::HighlightedText));
			} else {
				painter.setPen(palette().color(group, QPalette::Text));
			}

			QString annotation = comments_.value(address, QString(""));
			auto && inst = instructions_[line];
			if (annotation.isEmpty() && inst && !is_jump(inst) && !is_call(inst)) {
				// draw ascii representations of immediate constants
				size_t op_count = inst.operand_count();
				for (size_t op_idx = 0; op_idx < op_count; op_idx++) {
					auto oper = inst[op_idx];
					edb::address_t ascii_address = 0;
					if (is_immediate(oper)) {
						ascii_address = oper->imm;
					} else if (
						is_expression(oper) &&
						oper->mem.index == X86_REG_INVALID &&
						oper->mem.disp != 0)
					{
						if (oper->mem.base == X86_REG_RIP) {
							ascii_address += address + inst.byte_size() + oper->mem.disp;
						} else if (oper->mem.base == X86_REG_INVALID && oper->mem.disp > 0) {
							ascii_address = oper->mem.disp;
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
		for (int line = 0; line < lines_to_render; line++) {

			// we set the pen here to sensible defaults for the case where it doesn't get overridden by
			// syntax highlighting
			if (selected_line == line) {
				painter.setPen(palette().color(group, QPalette::HighlightedText));
				draw_instruction(painter, instructions_[line], line * line_height, line_height, l2, l3, true);
			} else {
				painter.setPen(palette().color(group, QPalette::Text));
				draw_instruction(painter, instructions_[line], line * line_height, line_height, l2, l3, false);
			}
		}
	}

	{ // DIVIDER LINES
		const QPen divider_pen = palette().shadow().color();
		painter.setPen(divider_pen);
		painter.drawLine(l1, 0, l1, height());
		painter.drawLine(l2, 0, l2, height());
		painter.drawLine(l3, 0, l3, height());
	}

	const int64_t renderTime = timer.elapsed();
	if(renderTime > 50) {
		qDebug() << "Painting took longer than desired: " << renderTime << "ms";
	}
}

//------------------------------------------------------------------------------
// Name: setFont
// Desc: overloaded version of setFont, calculates font metrics for later
//------------------------------------------------------------------------------
void QDisassemblyView::setFont(const QFont &f) {
	syntax_cache_.clear();

	QFont newFont(f);

	// NOTE(eteran): fix for #414 ?
	newFont.setStyleStrategy(QFont::ForceIntegerMetrics);

	// TODO: assert that we are using a fixed font & find out if we care?
	QAbstractScrollArea::setFont(newFont);

	// recalculate all of our metrics/offsets
	const QFontMetrics metrics(newFont);
	font_width_  = metrics.width('X');
	font_height_ = metrics.lineSpacing() + 1;

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

	const int line_height     = this->line_height();
	const int lines_to_render = 1 + (viewport()->height() / line_height);

	instruction_buffer_.resize(edb::Instruction::MAX_SIZE * lines_to_render);

	// Make PageUp/PageDown scroll through the whole page, but leave the line at
	// the top/bottom visible
	verticalScrollBar()->setPageStep(lines_to_render - 1);
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
		const int total_lines    = region_->size();
		const int viewable_lines = viewport()->height() / line_height();
		const int scroll_max     = (total_lines > viewable_lines) ? total_lines - 1 : 0;

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
	const int elements = address_length();
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
	const int address_len = edb::v1::pointer_size() * CHAR_BIT / 4;
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
				size_t buf_size = std::min<edb::address_t>((region_->end() - address), sizeof(buf));
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
	const int event_x = event->x() - line0_;
	if(region_) {
		if(event->button() == Qt::LeftButton) {
			if(near_line(event_x, line1())) {
				moving_line1_ = true;
			} else if(near_line(event_x, line2())) {
				moving_line2_ = true;
			} else if(near_line(event_x, line3())) {
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
		const int x_pos = event->x() - line0_;

		if(moving_line1_) {
			if(line2_ == 0) {
				line2_ = line2();
			}
			const int min_line1 = icon_width_ + font_width_ * 5;
			const int max_line1 = line2() - font_width_;
			line1_ = std::min(std::max(min_line1, x_pos), max_line1);
			update();
		} else if(moving_line2_) {
			if(line3_ == 0) {
				line3_ = line3();
			}
			const int min_line2 = line1() + font_width_ + font_width_/2;
			const int max_line2 = line3() - font_width_;
			line2_ = std::min(std::max(min_line2, x_pos), max_line2);
			update();
		} else if(moving_line3_) {
			const int min_line3 = line2() + font_width_;
			const int max_line3 = width() - 1 - (verticalScrollBar()->width() + 3);
			line3_ = std::min(std::max(min_line3, x_pos), max_line3);
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
std::shared_ptr<IRegion> QDisassemblyView::region() const {
	return region_;
}

//------------------------------------------------------------------------------
// Name: add_comment
// Desc: Adds a comment to the comment hash.
//------------------------------------------------------------------------------
void QDisassemblyView::add_comment(edb::address_t address, QString comment) {
	qDebug("Insert Comment");
	Comment temp_comment = {
		address,
		comment
	};
	SessionManager::instance().add_comment(temp_comment);
	comments_.insert(address, comment);
}

//------------------------------------------------------------------------------
// Name: remove_comment
// Desc: Removes a comment from the comment hash and returns the number of comments removed.
//------------------------------------------------------------------------------
int QDisassemblyView::remove_comment(edb::address_t address) {
	SessionManager::instance().remove_comment(address);
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
//------------------------------------------------------------------------------
// Name: restoreComments
// Desc:
//------------------------------------------------------------------------------
void QDisassemblyView::restoreComments(QVariantList &comments_data) {
	qDebug("restoreComments");
	for(auto it = comments_data.begin(); it != comments_data.end(); ++it) {
		QVariantMap data = it->toMap();
		if(const Result<edb::address_t> addr = edb::v1::string_to_address(data["address"].toString())) {
			comments_.insert(*addr, data["comment"].toString());
		}
	}
}
