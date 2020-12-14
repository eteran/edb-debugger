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

#include "ArchProcessor.h"
#include "Configuration.h"
#include "Function.h"
#include "IAnalyzer.h"
#include "IDebugger.h"
#include "IProcess.h"
#include "IRegion.h"
#include "ISymbolManager.h"
#include "IThread.h"
#include "Instruction.h"
#include "MemoryRegions.h"
#include "QDisassemblyView.h"
#include "SessionManager.h"
#include "State.h"
#include "SyntaxHighlighter.h"
#include "Theme.h"
#include "edb.h"
#include "util/Font.h"

#include <QAbstractItemDelegate>
#include <QApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QScrollBar>
#include <QSettings>
#include <QTextLayout>
#include <QToolTip>
#include <QtGlobal>

#include <QDebug>

#include <algorithm>
#include <climits>
#include <cmath>

namespace {

struct WidgetState1 {
	int version;
	int line1;
	int line2;
	int line3;
	int line4;
};

constexpr int DefaultByteWidth = 8;

struct show_separator_tag {};

template <class T, size_t N>
struct address_format {};

template <class T>
struct address_format<T, 4> {
	static QString format_address(T address, const show_separator_tag &) {
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
	static QString format_address(T address, const show_separator_tag &) {
		return edb::value32(address >> 32).toHexString() + ":" + edb::value32(address).toHexString();
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
	if (show_separator)
		return address_format<T, sizeof(T)>::format_address(address, show_separator_tag());
	else
		return address_format<T, sizeof(T)>::format_address(address);
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
int instruction_size(const uint8_t *buffer, std::size_t size) {
	edb::Instruction inst(buffer, buffer + size, 0);
	return inst.byteSize();
}

//------------------------------------------------------------------------------
// Name: format_instruction_bytes
// Desc:
//------------------------------------------------------------------------------
QString format_instruction_bytes(const edb::Instruction &inst) {
	auto bytes = QByteArray::fromRawData(reinterpret_cast<const char *>(inst.bytes()), inst.byteSize());
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

bool target_is_local(edb::address_t targetAddress, edb::address_t insnAddress) {

	const auto insnRegion   = edb::v1::memory_regions().findRegion(insnAddress);
	const auto targetRegion = edb::v1::memory_regions().findRegion(targetAddress);
	return !insnRegion->name().isEmpty() && targetRegion && insnRegion->name() == targetRegion->name();
}

}

//------------------------------------------------------------------------------
// Name: QDisassemblyView
// Desc: constructor
//------------------------------------------------------------------------------
QDisassemblyView::QDisassemblyView(QWidget *parent)
	: QAbstractScrollArea(parent),
	  highlighter_(new SyntaxHighlighter(this)),
	  breakpointRenderer_(QLatin1String(":/debugger/images/breakpoint.svg")),
	  currentRenderer_(QLatin1String(":/debugger/images/arrow-right.svg")),
	  currentBpRenderer_(QLatin1String(":/debugger/images/arrow-right-red.svg")),
	  syntaxCache_(256) {

	// TODO(eteran): it makes more sense for these to have setters/getters and it just be told
	// by the parent what these colors should be
	Theme theme = Theme::load();

	takenJumpColor_         = theme.text[Theme::TakenJump].foreground().color();
	fillingBytesColor_      = theme.text[Theme::Filling].foreground().color();
	addressForegroundColor_ = theme.text[Theme::Address].foreground().color();
	badgeBackgroundColor_   = theme.misc[Theme::Badge].background().color();
	badgeForegroundColor_   = theme.misc[Theme::Badge].foreground().color();

	setShowAddressSeparator(true);

	setFont(QFont("Monospace", 8));
	setMouseTracking(true);
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

	connect(verticalScrollBar(), &QScrollBar::actionTriggered, this, &QDisassemblyView::scrollbarActionTriggered);
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void QDisassemblyView::resetColumns() {
	line1_ = 0;
	line2_ = 0;
	line3_ = 0;
	line4_ = 0;
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
		const int idx                 = showAddresses_.indexOf(selected);
		if (selected != 0 && idx > 0 && idx < showAddresses_.size() - 1 - partialLastLine_) {
			setSelectedAddress(showAddresses_[idx + 1]);
		} else {
			const int current_offset = selected - addressOffset_;
			if (current_offset + 1 >= static_cast<int>(region_->size())) {
				return;
			}

			const edb::address_t next_address = addressOffset_ + followingInstructions(current_offset, 1);
			if (!addressShown(next_address)) {
				scrollTo(showAddresses_.size() > 1 ? showAddresses_[showAddresses_.size() / 3] : next_address);
			}

			setSelectedAddress(next_address);
		}
	} else if (event->matches(QKeySequence::MoveToPreviousLine)) {
		const edb::address_t selected = selectedAddress();
		const int idx                 = showAddresses_.indexOf(selected);
		if (selected != 0 && idx > 0) {
			// we already know the previous instruction
			setSelectedAddress(showAddresses_[idx - 1]);
		} else {
			const int current_offset = selected - addressOffset_;
			if (current_offset <= 0) {
				return;
			}

			const edb::address_t new_address = addressOffset_ + previousInstructions(current_offset, 1);
			if (!addressShown(new_address)) {
				scrollTo(new_address);
			}
			setSelectedAddress(new_address);
		}
	} else if (event->matches(QKeySequence::MoveToNextPage) || event->matches(QKeySequence::MoveToPreviousPage)) {
		const int selectedLine = getSelectedLineNumber();
		if (event->matches(QKeySequence::MoveToNextPage)) {
			scrollbarActionTriggered(QAbstractSlider::SliderPageStepAdd);
		} else {
			scrollbarActionTriggered(QAbstractSlider::SliderPageStepSub);
		}
		updateDisassembly(instructions_.size());

		if (showAddresses_.size() > selectedLine) {
			setSelectedAddress(showAddresses_[selectedLine]);
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
int QDisassemblyView::previousInstruction(IAnalyzer *analyzer, int current_address) {

	// If we have an analyzer, and the current address is within a function
	// then first we find the begining of that function.
	// Then, we attempt to disassemble from there until we run into
	// the address we were on (stopping one instruction early).
	// this allows us to identify with good accuracy where the
	// previous instruction was making upward scrolling more functional.
	//
	// If all else fails, fall back on the old heuristic which works "ok"
	if (analyzer) {
		edb::address_t address = addressOffset_ + current_address;

		// find the containing function
		if (Result<edb::address_t, QString> function_address = analyzer->findContainingFunction(address)) {

			if (address != *function_address) {
				edb::address_t function_start = *function_address;

				// disassemble from function start until the NEXT address is where we started
				while (true) {
					uint8_t buf[edb::Instruction::MaxSize];

					size_t buf_size = sizeof(buf);
					if (region_) {
						buf_size = std::min<size_t>((function_start - region_->base()), sizeof(buf));
					}

					if (edb::v1::get_instruction_bytes(function_start, buf, &buf_size)) {
						const edb::Instruction inst(buf, buf + buf_size, function_start);
						if (!inst) {
							break;
						}

						// if the NEXT address would be our target, then
						// we are at the previous instruction!
						if (function_start + inst.byteSize() >= current_address + addressOffset_) {
							break;
						}

						function_start += inst.byteSize();
					} else {
						break;
					}
				}

				current_address = (function_start - addressOffset_);
				return current_address;
			}
		}
	}

	// fall back on the old heuristic
	// iteration goal: to get exactly one new line above current instruction line
	edb::address_t address = addressOffset_ + current_address;

	for (int i = static_cast<int>(edb::Instruction::MaxSize); i > 0; --i) {
		edb::address_t prev_address = address - i;
		if (address >= addressOffset_) {

			uint8_t buf[edb::Instruction::MaxSize];
			int size               = sizeof(buf);
			Result<int, QString> n = getInstructionSize(prev_address, buf, &size);
			if (n && *n == i) {
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
int QDisassemblyView::previousInstructions(int current_address, int count) {

	IAnalyzer *const analyzer = edb::v1::analyzer();

	for (int i = 0; i < count; ++i) {
		current_address = previousInstruction(analyzer, current_address);
	}

	return current_address;
}

int QDisassemblyView::followingInstruction(int current_address) {
	uint8_t buf[edb::Instruction::MaxSize + 1];

	// do the longest read we can while still not passing the region end
	size_t buf_size = sizeof(buf);
	if (region_) {
		buf_size = std::min<size_t>((region_->end() - current_address), sizeof(buf));
	}

	// read in the bytes...
	if (!edb::v1::get_instruction_bytes(addressOffset_ + current_address, buf, &buf_size)) {
		return current_address + 1;
	} else {
		const edb::Instruction inst(buf, buf + buf_size, current_address);
		return current_address + inst.byteSize();
	}
}

//------------------------------------------------------------------------------
// Name: following_instructions
// Note: <current_address> is a 0 based value relative to the begining of the
//       current region, not an absolute address within the program
//------------------------------------------------------------------------------
int QDisassemblyView::followingInstructions(int current_address, int count) {

	for (int i = 0; i < count; ++i) {
		current_address = followingInstruction(current_address);
	}

	return current_address;
}

//------------------------------------------------------------------------------
// Name: wheelEvent
// Desc:
//------------------------------------------------------------------------------
void QDisassemblyView::wheelEvent(QWheelEvent *e) {

	const int dy           = e->delta();
	const int scroll_count = dy / 120;

	// Ctrl+Wheel scrolls by single bytes
	if (e->modifiers() & Qt::ControlModifier) {
		int address = verticalScrollBar()->value();
		verticalScrollBar()->setValue(address - scroll_count);
		e->accept();
		return;
	}

	const int abs_scroll_count = std::abs(scroll_count);

	if (e->delta() > 0) {
		// scroll up
		int address = verticalScrollBar()->value();
		address     = previousInstructions(address, abs_scroll_count);
		verticalScrollBar()->setValue(address);
	} else {
		// scroll down
		int address = verticalScrollBar()->value();
		address     = followingInstructions(address, abs_scroll_count);
		verticalScrollBar()->setValue(address);
	}
}

//------------------------------------------------------------------------------
// Name: scrollbar_action_triggered
// Desc:
//------------------------------------------------------------------------------
void QDisassemblyView::scrollbarActionTriggered(int action) {

	if (QApplication::keyboardModifiers() & Qt::ControlModifier) {
		return;
	}

	switch (action) {
	case QAbstractSlider::SliderSingleStepSub: {
		int address = verticalScrollBar()->value();
		address     = previousInstructions(address, 1);
		verticalScrollBar()->setSliderPosition(address);
	} break;
	case QAbstractSlider::SliderPageStepSub: {
		int address = verticalScrollBar()->value();
		address     = previousInstructions(address, verticalScrollBar()->pageStep());
		verticalScrollBar()->setSliderPosition(address);
	} break;
	case QAbstractSlider::SliderSingleStepAdd: {
		int address = verticalScrollBar()->value();
		address     = followingInstructions(address, 1);
		verticalScrollBar()->setSliderPosition(address);
	} break;
	case QAbstractSlider::SliderPageStepAdd: {
		int address = verticalScrollBar()->value();
		address     = followingInstructions(address, verticalScrollBar()->pageStep());
		verticalScrollBar()->setSliderPosition(address);
	} break;

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
	showAddressSeparator_ = value;
}

//------------------------------------------------------------------------------
// Name: formatAddress
// Desc:
//------------------------------------------------------------------------------
QString QDisassemblyView::formatAddress(edb::address_t address) const {
	if (edb::v1::debuggeeIs32Bit())
		return format_address<quint32>(address.toUint(), showAddressSeparator_);
	else
		return format_address(address, showAddressSeparator_);
}

//------------------------------------------------------------------------------
// Name: update
// Desc:
//------------------------------------------------------------------------------
void QDisassemblyView::update() {
	viewport()->update();
	Q_EMIT signalUpdated();
}

//------------------------------------------------------------------------------
// Name: addressShown
// Desc: returns true if a given address is in the visible range
//------------------------------------------------------------------------------
bool QDisassemblyView::addressShown(edb::address_t address) const {
	const auto idx = showAddresses_.indexOf(address);
	// if the last line is only partially rendered, consider it outside the
	// viewport.
	return (idx > 0 && idx < showAddresses_.size() - 1 - partialLastLine_);
}

//------------------------------------------------------------------------------
// Name: setCurrentAddress
// Desc: sets the 'current address' (where EIP is usually)
//------------------------------------------------------------------------------
void QDisassemblyView::setCurrentAddress(edb::address_t address) {
	currentAddress_ = address;
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
	if ((r && !r->equals(region_)) || (!r)) {
		region_ = r;
		setAddressOffset(region_ ? region_->start() : edb::address_t(0));
		updateScrollbars();
		Q_EMIT regionChanged();

		if (line2_ != 0 && line2_ < autoLine2()) {
			line2_ = 0;
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
	addressOffset_ = address;
}

//------------------------------------------------------------------------------
// Name: scrollTo
// Desc:
//------------------------------------------------------------------------------
void QDisassemblyView::scrollTo(edb::address_t address) {
	verticalScrollBar()->setValue(address - addressOffset_);
}

//------------------------------------------------------------------------------
// Name: instructionString
// Desc:
//------------------------------------------------------------------------------
QString QDisassemblyView::instructionString(const edb::Instruction &inst) const {
	QString opcode = QString::fromStdString(edb::v1::formatter().toString(inst));

	if (is_call(inst) || is_jump(inst)) {
		if (inst.operandCount() == 1) {
			const auto oper = inst[0];
			if (is_immediate(oper)) {

				const bool showSymbolicAddresses = edb::v1::config().show_symbolic_addresses;

				static const QRegExp addrPattern(QLatin1String("#?0x[0-9a-fA-F]+"));
				const edb::address_t target = oper->imm;

				const bool showLocalModuleNames = edb::v1::config().show_local_module_name_in_jump_targets;
				const bool prefixed             = showLocalModuleNames || !target_is_local(target, inst.rva());
				QString sym                     = edb::v1::symbol_manager().findAddressName(target, prefixed);

				if (sym.isEmpty() && target == inst.byteSize() + inst.rva()) {
					sym = showSymbolicAddresses ? tr("<next instruction>") : tr("next instruction");
				} else if (sym.isEmpty() && target == inst.rva()) {
					sym = showSymbolicAddresses ? tr("$") : tr("current instruction");
				}

				if (!sym.isEmpty()) {
					if (showSymbolicAddresses)
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
// Name: drawInstruction
// Desc:
//------------------------------------------------------------------------------
void QDisassemblyView::drawInstruction(QPainter &painter, const edb::Instruction &inst, const DrawingContext *ctx, int y, bool selected) {

	painter.save();

	const bool is_filling      = edb::v1::arch_processor().isFilling(inst);
	const int x                = fontWidth_ + fontWidth_ + ctx->l3 + (fontWidth_ / 2);
	const int inst_pixel_width = ctx->l4 - x;

	const bool syntax_highlighting_enabled = edb::v1::config().syntax_highlighting_enabled && !selected;

	QString opcode = instructionString(inst);

	if (is_filling) {
		if (syntax_highlighting_enabled) {
			painter.setPen(fillingBytesColor_);
		}

		opcode = painter.fontMetrics().elidedText(opcode, Qt::ElideRight, inst_pixel_width);

		painter.drawText(
			x,
			y,
			opcode.length() * fontWidth_,
			ctx->lineHeight,
			Qt::AlignVCenter,
			opcode);
	} else {

		// NOTE(eteran): do this early, so that elided text still gets the part shown
		// properly highlighted
		QVector<QTextLayout::FormatRange> highlightData;
		if (syntax_highlighting_enabled) {
			highlightData = highlighter_->highlightBlock(opcode);
		}

		opcode = painter.fontMetrics().elidedText(opcode, Qt::ElideRight, inst_pixel_width);

		if (syntax_highlighting_enabled) {

			QPixmap *map = syntaxCache_[opcode];
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

				map = new QPixmap(QSize(opcode.length() * fontWidth_, ctx->lineHeight) * devicePixelRatio());
				map->setDevicePixelRatio(devicePixelRatio());
				map->fill(Qt::transparent);
				QPainter cache_painter(map);
				cache_painter.setPen(painter.pen());
				cache_painter.setFont(painter.font());

				// now the render the text at the location given
				textLayout.draw(&cache_painter, QPoint(0, 0), highlightData);
				syntaxCache_.insert(opcode, map);
			}
			painter.drawPixmap(x, y, *map);
		} else {
			QRectF rectangle(x, y, opcode.length() * fontWidth_, ctx->lineHeight);
			painter.drawText(rectangle, Qt::AlignVCenter, opcode);
		}
	}

	painter.restore();
}

//------------------------------------------------------------------------------
// Name: paint_line_bg
// Desc: A helper function for painting a rectangle representing a background
// color of one or more lines in the disassembly view.
//------------------------------------------------------------------------------
void QDisassemblyView::paintLineBg(QPainter &painter, QBrush brush, int line, int num_lines) {
	const auto lh = lineHeight();
	painter.fillRect(0, lh * line, width(), lh * num_lines, brush);
}

//------------------------------------------------------------------------------
// Name: get_line_of_address
// Desc: A helper function which sets line to the line on which addr appears,
// or returns false if that line does not appear to exist.
//------------------------------------------------------------------------------
boost::optional<unsigned int> QDisassemblyView::getLineOfAddress(edb::address_t addr) const {

	if (!showAddresses_.isEmpty()) {
		if (addr >= showAddresses_[0] && addr <= showAddresses_[showAddresses_.size() - 1]) {
			int pos = std::find(showAddresses_.begin(), showAddresses_.end(), addr) - showAddresses_.begin();
			if (pos < showAddresses_.size()) { // address was found
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
	showAddresses_.clear();

	int bufsize                        = instructionBuffer_.size();
	uint8_t *inst_buf                  = &instructionBuffer_[0];
	const edb::address_t start_address = addressOffset_ + verticalScrollBar()->value();

	if (!edb::v1::get_instruction_bytes(start_address, inst_buf, &bufsize)) {
		qDebug() << "Failed to read" << bufsize << "bytes from" << QString::number(start_address, 16);
		lines_to_render = 0;
	}

	instructions_.reserve(lines_to_render);
	showAddresses_.reserve(lines_to_render);

	const int max_offset = std::min(int(region_->end() - start_address), bufsize);

	int line   = 0;
	int offset = 0;

	while (line < lines_to_render && offset < max_offset) {
		edb::address_t address = start_address + offset;
		instructions_.emplace_back(
			&inst_buf[offset],  // instruction bytes
			&inst_buf[bufsize], // end of buffer
			address             // address of instruction
		);
		showAddresses_.push_back(address);

		if (instructions_[line].valid()) {
			offset += instructions_[line].byteSize();
		} else {
			++offset;
		}
		line++;
	}
	Q_ASSERT(line <= lines_to_render);
	if (lines_to_render != line) {
		lines_to_render  = line;
		partialLastLine_ = false;
	}

	lines_to_render = line;
	return lines_to_render;
}

//------------------------------------------------------------------------------
// Name: getSelectedLineNumber
// Desc:
//------------------------------------------------------------------------------
int QDisassemblyView::getSelectedLineNumber() const {

	for (size_t line = 0; line < instructions_.size(); ++line) {
		if (instructions_[line].rva() == selectedAddress()) {
			return static_cast<int>(line);
		}
	}

	return 65535; // can't accidentally hit this;
}

//------------------------------------------------------------------------------
// Name: drawHeaderAndBackground
// Desc:
//------------------------------------------------------------------------------
void QDisassemblyView::drawHeaderAndBackground(QPainter &painter, const DrawingContext *ctx, const std::unique_ptr<IBinary> &binary_info) {

	painter.save();

	// HEADER & ALTERNATION BACKGROUND PAINTING STEP
	// paint the header gray
	int line = 0;
	if (binary_info) {
		auto header_size                  = binary_info->headerSize();
		edb::address_t header_end_address = region_->start() + header_size;
		// Find the number of lines we need to paint with the header
		while (line < ctx->linesToRender && header_end_address > showAddresses_[line]) {
			line++;
		}
		paintLineBg(painter, QBrush(Qt::lightGray), 0, line);
	}

	line += 1;
	if (line != ctx->linesToRender) {
		const QBrush alternated_base_color = palette().alternateBase();
		if (alternated_base_color != palette().base()) {
			while (line < ctx->linesToRender) {
				paintLineBg(painter, alternated_base_color, line);
				line += 2;
			}
		}
	}

	if (ctx->selectedLines < ctx->linesToRender) {
		paintLineBg(painter, palette().color(ctx->group, QPalette::Highlight), ctx->selectedLines);
	}

	painter.restore();
}

//------------------------------------------------------------------------------
// Name: drawRegiserBadges
// Desc:
//------------------------------------------------------------------------------
void QDisassemblyView::drawRegiserBadges(QPainter &painter, DrawingContext *ctx) {

	painter.save();
	if (IProcess *process = edb::v1::debugger_core->process()) {

		if (process->isPaused()) {

			State state;
			process->currentThread()->getState(&state);

			std::vector<QString> badge_labels(ctx->linesToRender);
			{
				unsigned int reg_num = 0;
				Register reg;
				reg = state.gpRegister(reg_num);

				while (reg.valid()) {
					// Does addr appear here?
					edb::address_t addr = reg.valueAsAddress();

					if (boost::optional<unsigned int> line = getLineOfAddress(addr)) {
						if (!badge_labels[*line].isEmpty()) {
							badge_labels[*line].append(", ");
						}
						badge_labels[*line].append(reg.name());
					}

					// what about [addr]?
					if (process->readBytes(addr, &addr, edb::v1::pointer_size())) {
						if (boost::optional<unsigned int> line = getLineOfAddress(addr)) {
							if (!badge_labels[*line].isEmpty()) {
								badge_labels[*line].append(", ");
							}
							badge_labels[*line].append("[" + reg.name() + "]");
						}
					}

					reg = state.gpRegister(++reg_num);
				}
			}

			painter.setPen(badgeForegroundColor_);

			for (int line = 0; line < ctx->linesToRender; line++) {
				if (!badge_labels[line].isEmpty()) {

					int width          = badge_labels[line].length() * fontWidth_ + fontWidth_ / 2;
					int height         = ctx->lineHeight;
					int triangle_point = line1() - 3;
					int x              = triangle_point - (height / 2) - width;
					int y              = line * ctx->lineHeight;

					// if badge is not in viewpoint, then don't draw
					if (x < 0) {
						continue;
					}

					ctx->lineBadgeWidth[line] = line1() - x;

					QRect bounds(x, y, width, height);

					// draw a rectangle + box around text
					QPainterPath path;
					path.addRect(bounds);
					path.moveTo(bounds.x() + bounds.width(), bounds.y());                   // top right
					path.lineTo(triangle_point, bounds.y() + bounds.height() / 2);          // triangle point
					path.lineTo(bounds.x() + bounds.width(), bounds.y() + bounds.height()); // bottom right
					painter.fillPath(path, badgeBackgroundColor_);

					painter.drawText(
						bounds.x() + fontWidth_ / 4,
						line * ctx->lineHeight,
						fontWidth_ * badge_labels[line].size(),
						ctx->lineHeight,
						Qt::AlignVCenter,
						(edb::v1::config().uppercase_disassembly ? badge_labels[line].toUpper() : badge_labels[line]));
				}
			}
		}
	}
	painter.restore();
}

//------------------------------------------------------------------------------
// Name: drawSymbolNames
// Desc:
//------------------------------------------------------------------------------
void QDisassemblyView::drawSymbolNames(QPainter &painter, const DrawingContext *ctx) {
	painter.save();

	painter.setPen(palette().color(ctx->group, QPalette::Text));
	const int x     = ctx->l1 + autoLine2();
	const int width = ctx->l2 - x;
	if (width > 0) {
		for (int line = 0; line < ctx->linesToRender; line++) {

			if (ctx->selectedLines != line) {
				auto address      = showAddresses_[line];
				const QString sym = edb::v1::symbol_manager().findAddressName(address);
				if (!sym.isEmpty()) {
					const QString symbol_buffer = painter.fontMetrics().elidedText(sym, Qt::ElideRight, width);

					painter.drawText(
						x,
						line * ctx->lineHeight,
						width,
						ctx->lineHeight,
						Qt::AlignVCenter,
						symbol_buffer);
				}
			}
		}

		if (ctx->selectedLines < ctx->linesToRender) {
			int line = ctx->selectedLines;
			painter.setPen(palette().color(ctx->group, QPalette::HighlightedText));
			auto address      = showAddresses_[line];
			const QString sym = edb::v1::symbol_manager().findAddressName(address);
			if (!sym.isEmpty()) {
				const QString symbol_buffer = painter.fontMetrics().elidedText(sym, Qt::ElideRight, width);

				painter.drawText(
					x,
					line * ctx->lineHeight,
					width,
					ctx->lineHeight,
					Qt::AlignVCenter,
					symbol_buffer);
			}
		}
	}

	painter.restore();
}

//------------------------------------------------------------------------------
// Name: drawSidebarElements
// Desc:
//------------------------------------------------------------------------------
void QDisassemblyView::drawSidebarElements(QPainter &painter, const DrawingContext *ctx) {

	painter.save();
	painter.setPen(addressForegroundColor_);

	const auto icon_x     = ctx->l1 + 1;
	const auto addr_x     = icon_x + iconWidth_;
	const auto addr_width = ctx->l2 - addr_x;

	auto paint_address_lambda = [&](int line) {
		auto address = showAddresses_[line];

		const bool has_breakpoint = (edb::v1::find_breakpoint(address) != nullptr);
		const bool is_eip         = address == currentAddress_;

		// TODO(eteran): if highlighted render the BP/Arrow in a more readable color!
		QSvgRenderer *icon = nullptr;
		if (is_eip) {
			icon = has_breakpoint ? &currentBpRenderer_ : &currentRenderer_;
		} else if (has_breakpoint) {
			icon = &breakpointRenderer_;
		}

		if (icon) {
			icon->render(&painter, QRectF(icon_x, line * ctx->lineHeight + 1, iconWidth_, iconHeight_));
		}

		const QString address_buffer = formatAddress(address);
		// draw the address
		painter.drawText(
			addr_x,
			line * ctx->lineHeight,
			addr_width,
			ctx->lineHeight,
			Qt::AlignVCenter,
			address_buffer);
	};

	// paint all but the highlighted address
	for (int line = 0; line < ctx->linesToRender; line++) {
		if (ctx->selectedLines != line) {
			paint_address_lambda(line);
		}
	}

	// paint the highlighted address
	if (ctx->selectedLines < ctx->linesToRender) {
		painter.setPen(palette().color(ctx->group, QPalette::HighlightedText));
		paint_address_lambda(ctx->selectedLines);
	}

	painter.restore();
}

//------------------------------------------------------------------------------
// Name: drawInstructionBytes
// Desc:
//------------------------------------------------------------------------------
void QDisassemblyView::drawInstructionBytes(QPainter &painter, const DrawingContext *ctx) {

	painter.save();

	const int bytes_width = ctx->l3 - ctx->l2 - fontWidth_ / 2;
	const auto metrics    = painter.fontMetrics();

	auto painter_lambda = [&](const edb::Instruction &inst, int line) {
		const QString byte_buffer = format_instruction_bytes(
			inst,
			bytes_width,
			metrics);

		painter.drawText(
			ctx->l2 + (fontWidth_ / 2),
			line * ctx->lineHeight,
			bytes_width,
			ctx->lineHeight,
			Qt::AlignVCenter,
			byte_buffer);
	};

	painter.setPen(palette().color(ctx->group, QPalette::Text));

	for (int line = 0; line < ctx->linesToRender; line++) {

		auto &&inst = instructions_[line];
		if (ctx->selectedLines != line) {
			painter_lambda(inst, line);
		}
	}

	if (ctx->selectedLines < ctx->linesToRender) {
		painter.setPen(palette().color(ctx->group, QPalette::HighlightedText));
		painter_lambda(instructions_[ctx->selectedLines], ctx->selectedLines);
	}

	painter.restore();
}

//------------------------------------------------------------------------------
// Name: drawFunctionMarkers
// Desc:
//------------------------------------------------------------------------------
void QDisassemblyView::drawFunctionMarkers(QPainter &painter, const DrawingContext *ctx) {

	painter.save();

	IAnalyzer *const analyzer = edb::v1::analyzer();
	const int x               = ctx->l3 + fontWidth_;
	if (analyzer && ctx->l4 - x > fontWidth_ / 2) {
		painter.setPen(QPen(palette().color(ctx->group, QPalette::WindowText), 2));
		int next_line = 0;

		if (ctx->linesToRender != 0 && !showAddresses_.isEmpty()) {
			analyzer->forFuncsInRange(showAddresses_[0], showAddresses_[ctx->linesToRender - 1], [&](const Function *func) {
				auto entry_addr = func->entryAddress();
				auto end_addr   = func->endAddress();
				int start_line;

				// Find the start and draw the corner
				for (start_line = next_line; start_line < ctx->linesToRender; start_line++) {
					if (showAddresses_[start_line] == entry_addr) {
						auto y = start_line * ctx->lineHeight;
						// half of a horizontal
						painter.drawLine(
							x,
							y + ctx->lineHeight / 2,
							x + fontWidth_ / 2,
							y + ctx->lineHeight / 2);

						// half of a vertical
						painter.drawLine(
							x,
							y + ctx->lineHeight / 2,
							x,
							y + ctx->lineHeight);

						start_line++;
						break;
					}
					if (showAddresses_[start_line] > entry_addr) {
						break;
					}
				}

				int end_line;

				// find the end and draw the other corner
				for (end_line = start_line; end_line < ctx->linesToRender; end_line++) {
					auto adjusted_end_addr = showAddresses_[end_line] + instructions_[end_line].byteSize() - 1;
					if (adjusted_end_addr == end_addr) {
						auto y = end_line * ctx->lineHeight;

						// half of a vertical
						painter.drawLine(
							x,
							y,
							x,
							y + ctx->lineHeight / 2);

						// half of a horizontal
						painter.drawLine(
							x,
							y + ctx->lineHeight / 2,
							ctx->l3 + (fontWidth_ / 2) + fontWidth_,
							y + ctx->lineHeight / 2);

						next_line = end_line;
						break;
					}

					if (adjusted_end_addr > end_addr) {
						next_line = end_line;
						break;
					}
				}

				// draw the straight line between them
				if (start_line != end_line) {
					painter.drawLine(x, start_line * ctx->lineHeight, x, end_line * ctx->lineHeight);
				}
				return true;
			});
		}
	}

	painter.restore();
}

//------------------------------------------------------------------------------
// Name: drawComments
// Desc:
//------------------------------------------------------------------------------
void QDisassemblyView::drawComments(QPainter &painter, const DrawingContext *ctx) {

	painter.save();

	auto x_pos         = ctx->l4 + fontWidth_ + (fontWidth_ / 2);
	auto comment_width = width() - x_pos;

	for (int line = 0; line < ctx->linesToRender; line++) {
		auto address = showAddresses_[line];

		if (ctx->selectedLines == line) {
			painter.setPen(palette().color(ctx->group, QPalette::HighlightedText));
		} else {
			painter.setPen(palette().color(ctx->group, QPalette::Text));
		}

		QString annotation = comments_.value(address, QString(""));
		auto &&inst        = instructions_[line];
		if (annotation.isEmpty() && inst && !is_jump(inst) && !is_call(inst)) {
			// draw ascii representations of immediate constants
			size_t op_count = inst.operandCount();
			for (size_t op_idx = 0; op_idx < op_count; op_idx++) {
				auto oper                    = inst[op_idx];
				edb::address_t ascii_address = 0;
				if (is_immediate(oper)) {
					ascii_address = oper->imm;
				} else if (
					is_expression(oper) &&
					oper->mem.index == X86_REG_INVALID &&
					oper->mem.disp != 0) {
					if (oper->mem.base == X86_REG_RIP) {
						ascii_address += address + inst.byteSize() + oper->mem.disp;
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
			line * ctx->lineHeight,
			comment_width,
			ctx->lineHeight,
			Qt::AlignLeft,
			annotation);
	}

	painter.restore();
}

//------------------------------------------------------------------------------
// Name: drawJumpArrows
// Desc:
//------------------------------------------------------------------------------
void QDisassemblyView::drawJumpArrows(QPainter &painter, const DrawingContext *ctx) {

	std::vector<JumpArrow> jump_arrow_vec;

	for (int line = 0; line < ctx->linesToRender; ++line) {

		auto &&inst = instructions_[line];
		if (is_jump(inst) && is_immediate(inst[0])) {

			const edb::address_t target = inst[0]->imm;
			if (target != inst.rva()) {           // TODO: draw small arrow if jmp points to itself
				if (region()->contains(target)) { // make sure jmp target is in current memory region

					JumpArrow jump_arrow;
					jump_arrow.sourceLine                = line;
					jump_arrow.target                    = target;
					jump_arrow.destInViewport            = false;
					jump_arrow.destInMiddleOfInstruction = false;
					jump_arrow.destLine                  = INT_MAX;

					// check if dst address is in viewport
					for (int i = 0; i < ctx->linesToRender; ++i) {

						if (instructions_[i].rva() == target) {
							jump_arrow.destLine       = i;
							jump_arrow.destInViewport = true;
							break;
						}

						if (i < ctx->linesToRender - 1) {
							// if target is in middle of instruction
							if (target > instructions_[i].rva() && target < instructions_[i + 1].rva()) {
								jump_arrow.destLine                  = i + 1;
								jump_arrow.destInMiddleOfInstruction = true;
								jump_arrow.destInViewport            = true;
								break;
							}
						}
					}

					// if jmp target not in viewpoint, its value should be near INT_MAX
					jump_arrow.distance         = std::abs(jump_arrow.destLine - jump_arrow.sourceLine);
					jump_arrow.horizontalLength = -1; // will be recalculate back below

					jump_arrow_vec.push_back(jump_arrow);
				}
			}
		}
	}

	// sort all jmp data in ascending order
	std::sort(jump_arrow_vec.begin(), jump_arrow_vec.end(),
			  [](const JumpArrow &a, const JumpArrow &b) -> bool {
				  return a.distance < b.distance;
			  });

	auto isLineOverlap = [&](int line1_head, int line1_tail, int line2_head, int line2_tail, bool edge_overlap) -> bool {
		int jump1_arrow_min = std::min(line1_head, line1_tail);
		int jump1_arrow_max = std::max(line1_head, line1_tail);
		int jump2_arrow_min = std::min(line2_head, line2_tail);
		int jump2_arrow_max = std::max(line2_head, line2_tail);

		bool prevArrowAboveCurrArrow;
		bool prevArrowBelowCurrArrow;

		if (edge_overlap) {
			prevArrowAboveCurrArrow = jump2_arrow_max > jump1_arrow_max && jump2_arrow_min >= jump1_arrow_max;
			prevArrowBelowCurrArrow = jump2_arrow_min < jump1_arrow_min && jump2_arrow_max <= jump1_arrow_min;
		} else {
			prevArrowAboveCurrArrow = jump2_arrow_max > jump1_arrow_max && jump2_arrow_min > jump1_arrow_max;
			prevArrowBelowCurrArrow = jump2_arrow_min < jump1_arrow_min && jump2_arrow_max < jump1_arrow_min;
		}

		// are both conditions false? (which means these two jump arrows overlap)
		return !(prevArrowAboveCurrArrow || prevArrowBelowCurrArrow);
	};

	// find suitable arrow horizontal length
	for (size_t jump_arrow_idx = 0; jump_arrow_idx < jump_arrow_vec.size(); jump_arrow_idx++) {

		JumpArrow &jump_arrow = jump_arrow_vec[jump_arrow_idx];
		bool is_dst_upward    = jump_arrow.target < instructions_[jump_arrow.sourceLine].rva();
		int jump_arrow_dst    = jump_arrow.destInViewport ? jump_arrow.destLine * ctx->lineHeight : (is_dst_upward ? 0 : viewport()->height());

		int size_block     = fontWidth_ * 2;
		int start_at_block = size_block;
		int badge_line     = -1;

		if (ctx->lineBadgeWidth.find(jump_arrow.sourceLine) != ctx->lineBadgeWidth.end()) {
			badge_line = jump_arrow.sourceLine;
		} else if (ctx->lineBadgeWidth.find(jump_arrow.destLine) != ctx->lineBadgeWidth.end()) {
			badge_line = jump_arrow.destLine;
		}

		// check if current arrow overlaps with register badge
		for (const auto &each_badge : ctx->lineBadgeWidth) {

			bool is_overlap_with_badge = isLineOverlap(
				jump_arrow.sourceLine * ctx->lineHeight + 1,
				jump_arrow_dst - 1,
				each_badge.first * ctx->lineHeight,
				(each_badge.first + 1) * ctx->lineHeight,
				true);

			if (is_overlap_with_badge) {
				badge_line = each_badge.first;
				break;
			}
		}

		if (badge_line != -1) {
			start_at_block = size_block + size_block * (ctx->lineBadgeWidth.at(badge_line) / size_block);
		}

		// first-fit search for horizontal length position to place new arrow
		for (int current_selected_len = start_at_block;; current_selected_len += size_block) {

			bool is_length_good = true;

			// check if current arrow overlaps with previous arrow
			for (size_t jump_arrow_prev_idx = 0; jump_arrow_prev_idx < jump_arrow_idx && is_length_good; jump_arrow_prev_idx++) {

				const JumpArrow &jump_arrow_prev = jump_arrow_vec[jump_arrow_prev_idx];

				bool is_dst_upward_prev = jump_arrow_prev.target < instructions_[jump_arrow_prev.sourceLine].rva();
				int jump_arrow_prev_dst = jump_arrow_prev.destInViewport ? jump_arrow_prev.destLine * ctx->lineHeight : (is_dst_upward_prev ? 0 : viewport()->height());

				bool jumps_overlap = isLineOverlap(
					jump_arrow.sourceLine * ctx->lineHeight,
					jump_arrow_dst,
					jump_arrow_prev.sourceLine * ctx->lineHeight,
					jump_arrow_prev_dst,
					false);

				// if jump blocks overlap and this horizontal length has been taken before
				if (jumps_overlap && current_selected_len == jump_arrow_prev.horizontalLength) {
					is_length_good = false;
				}
			}

			// current_selected_len is not good, search next
			if (!is_length_good) {
				continue;
			}

			jump_arrow.horizontalLength = current_selected_len;
			break;
		}
	}

	// get current process state
	State state;
	IProcess *process = edb::v1::debugger_core->process();
	process->currentThread()->getState(&state);

	painter.save();
	painter.setRenderHint(QPainter::Antialiasing, true);

	for (const JumpArrow &jump_arrow : jump_arrow_vec) {

		bool is_dst_upward = jump_arrow.target < instructions_[jump_arrow.sourceLine].rva();

		// horizontal line
		int end_x   = ctx->l1 - 3;
		int start_x = end_x - jump_arrow.horizontalLength;

		// vertical line
		int src_y = jump_arrow.sourceLine * ctx->lineHeight + (fontHeight_ / 2);
		int dst_y;

		if (jump_arrow.destInMiddleOfInstruction) {
			dst_y = jump_arrow.destLine * ctx->lineHeight;
		} else {
			dst_y = jump_arrow.destLine * ctx->lineHeight + (fontHeight_ / 2);
		}

		QColor arrow_color = palette().color(ctx->group, QPalette::Text);
		double arrow_width = 1.0;
		auto arrow_style   = Qt::DashLine;

		if (ctx->selectedLines == jump_arrow.sourceLine || ctx->selectedLines == jump_arrow.destLine) {
			arrow_width = 2.0; // enlarge arrow width
		}

		bool conditional_jmp   = is_conditional_jump(instructions_[jump_arrow.sourceLine]);
		bool unconditional_jmp = is_unconditional_jump(instructions_[jump_arrow.sourceLine]);

		// if direct jmp, then draw in solid line
		if (unconditional_jmp) {
			arrow_style = Qt::SolidLine;
		}

		// if direct jmp (src) is selected, then draw arrow in red
		if (unconditional_jmp && ctx->selectedLines == jump_arrow.sourceLine) {
			arrow_color = takenJumpColor_;
		}

		// if direct jmp (dst) is selected, then draw arrow in red
		if (unconditional_jmp && ctx->selectedLines == jump_arrow.destLine) {
			if (showAddresses_[jump_arrow.destLine] != currentAddress_) { // if eip
				arrow_color = takenJumpColor_;
			}
		}

		// if current conditional jump is taken, then draw arrow in red
		if (showAddresses_[jump_arrow.sourceLine] == currentAddress_) { // if eip
			if (conditional_jmp) {
				if (edb::v1::arch_processor().isExecuted(instructions_[jump_arrow.sourceLine], state)) {
					arrow_color = takenJumpColor_;
				}
			}
		}

		// Align both 1px and 2px lines to pixel grid. This requires different offset in even-width and odd-width case.
		const auto arrow_pixel_offset = std::fmod(arrow_width, 2.) == 1 ? 0.5 : 0;
		painter.save();
		painter.translate(arrow_pixel_offset, arrow_pixel_offset);

		painter.setPen(QPen(arrow_color, arrow_width, arrow_style));

		int src_reg_badge_width = 0;
		int dst_reg_badge_width = 0;

		if (ctx->lineBadgeWidth.find(jump_arrow.sourceLine) != ctx->lineBadgeWidth.end()) {
			src_reg_badge_width = ctx->lineBadgeWidth.at(jump_arrow.sourceLine);
		} else if (ctx->lineBadgeWidth.find(jump_arrow.destLine) != ctx->lineBadgeWidth.end()) {
			dst_reg_badge_width = ctx->lineBadgeWidth.at(jump_arrow.destLine);
		}

		if (jump_arrow.destInViewport) {

			QPoint points[] = {
				QPoint(end_x - src_reg_badge_width, src_y),
				QPoint(start_x, src_y),
				QPoint(start_x, dst_y),
				QPoint(end_x - dst_reg_badge_width - fontWidth_ / 3, dst_y)};

			painter.drawPolyline(points, 4);

			// draw arrow tips
			QPainterPath path;
			path.moveTo(end_x - dst_reg_badge_width, dst_y);
			path.lineTo(end_x - dst_reg_badge_width - (fontWidth_ / 2), dst_y - (fontHeight_ / 3));
			path.lineTo(end_x - dst_reg_badge_width - (fontWidth_ / 2), dst_y + (fontHeight_ / 3));
			path.lineTo(end_x - dst_reg_badge_width, dst_y);
			painter.fillPath(path, QBrush(arrow_color));

		} else if (is_dst_upward) { // if dst out of viewport, and arrow facing upward

			QPoint points[] = {
				QPoint(end_x - src_reg_badge_width, src_y),
				QPoint(start_x, src_y),
				QPoint(start_x, fontWidth_ / 3)};

			painter.drawPolyline(points, 3);

			// draw arrow tips
			QPainterPath path;
			path.moveTo(start_x, 0);
			path.lineTo(start_x - (fontWidth_ / 2), fontHeight_ / 3);
			path.lineTo(start_x + (fontWidth_ / 2), fontHeight_ / 3);
			path.lineTo(start_x, 0);
			painter.fillPath(path, QBrush(arrow_color));

		} else { // if dst out of viewport, and arrow facing downward

			QPoint points[] = {
				QPoint(end_x - src_reg_badge_width, src_y),
				QPoint(start_x, src_y),
				QPoint(start_x, viewport()->height() - fontWidth_ / 3)};

			painter.drawPolyline(points, 3);

			// draw arrow tips
			QPainterPath path;
			path.moveTo(start_x, viewport()->height() - 1);
			path.lineTo(start_x - (fontWidth_ / 2), viewport()->height() - (fontHeight_ / 3) - 1);
			path.lineTo(start_x + (fontWidth_ / 2), viewport()->height() - (fontHeight_ / 3) - 1);
			path.lineTo(start_x, viewport()->height() - 1);
			painter.fillPath(path, QBrush(arrow_color));
		}

		painter.restore();
	}

	painter.restore();
}

//------------------------------------------------------------------------------
// Name: drawDisassembly
// Desc:
//------------------------------------------------------------------------------
void QDisassemblyView::drawDisassembly(QPainter &painter, const DrawingContext *ctx) {

	painter.save();

	painter.setPen(palette().color(ctx->group, QPalette::Text));
	for (int line = 0; line < ctx->linesToRender; line++) {
		if (ctx->selectedLines == line) {
			QPen prevPen = painter.pen();
			painter.setPen(palette().color(ctx->group, QPalette::HighlightedText));
			drawInstruction(painter, instructions_[line], ctx, line * ctx->lineHeight, true);
			painter.setPen(prevPen);
		} else {
			drawInstruction(painter, instructions_[line], ctx, line * ctx->lineHeight, false);
		}
	}

	painter.restore();
}

//------------------------------------------------------------------------------
// Name: paintEvent
// Desc:
//------------------------------------------------------------------------------
void QDisassemblyView::drawDividers(QPainter &painter, const DrawingContext *ctx) {

	painter.save();

	const QPen divider_pen = palette().color(ctx->group, QPalette::WindowText);
	painter.setPen(divider_pen);

	if (edb::v1::config().show_jump_arrow || edb::v1::config().show_register_badges) {
		painter.drawLine(ctx->l1, 0, ctx->l1, height());
	}

	painter.drawLine(ctx->l2, 0, ctx->l2, height());
	painter.drawLine(ctx->l3, 0, ctx->l3, height());
	painter.drawLine(ctx->l4, 0, ctx->l4, height());

	painter.restore();
}

//------------------------------------------------------------------------------
// Name: paintEvent
// Desc:
//------------------------------------------------------------------------------
void QDisassemblyView::paintEvent(QPaintEvent *) {

	if (!region_) {
		return;
	}

	const size_t region_size = region_->size();
	if (region_size == 0) {
		return;
	}

	QElapsedTimer timer;
	timer.start();

	QPainter painter(viewport());

	const int line_height = this->lineHeight();
	int lines_to_render   = viewport()->height() / line_height;

	// Possibly render another instruction just outside the viewport
	if (viewport()->height() % line_height > 0) {
		lines_to_render++;
		partialLastLine_ = true;
	} else {
		partialLastLine_ = false;
	}

	const auto binary_info = edb::v1::get_binary_info(region_);
	const auto group       = hasFocus() ? QPalette::Active : QPalette::Inactive;

	lines_to_render         = updateDisassembly(lines_to_render);
	const int selected_line = getSelectedLineNumber();

	DrawingContext context = {
		line1(),
		line2(),
		line3(),
		line4(),
		lines_to_render,
		selected_line,
		line_height,
		group,
		std::map<int, int>()};

	drawHeaderAndBackground(painter, &context, binary_info);

	if (edb::v1::config().show_register_badges) {
		drawRegiserBadges(painter, &context);
	}

	drawSymbolNames(painter, &context);

	// SELECTION, BREAKPOINT, EIP & ADDRESS
	drawSidebarElements(painter, &context);

	// INSTRUCTION BYTES AND RELJMP INDICATOR RENDERING
	drawInstructionBytes(painter, &context);

	drawFunctionMarkers(painter, &context);
	drawComments(painter, &context);

	if (edb::v1::config().show_jump_arrow) {
		drawJumpArrows(painter, &context);
	}

	drawDisassembly(painter, &context);
	drawDividers(painter, &context);

	const int64_t renderTime = timer.elapsed();
	if (renderTime > 50) {
		qDebug() << "Painting took longer than desired: " << renderTime << "ms";
	}
}

//------------------------------------------------------------------------------
// Name: setFont
// Desc: overloaded version of setFont, calculates font metrics for later
//------------------------------------------------------------------------------
void QDisassemblyView::setFont(const QFont &f) {
	syntaxCache_.clear();

	QFont newFont(f);

	// NOTE(eteran): fix for #414 ?
	newFont.setStyleStrategy(QFont::ForceIntegerMetrics);

	// TODO: assert that we are using a fixed font & find out if we care?
	QAbstractScrollArea::setFont(newFont);

	// recalculate all of our metrics/offsets
	const QFontMetrics metrics(newFont);
	fontWidth_  = Font::maxWidth(metrics);
	fontHeight_ = metrics.lineSpacing() + 1;

	// NOTE(eteran): we let the icons be a bit wider than the font itself, since things
	// like arrows don't tend to have square bounds. A ratio of 2:1 seems to look pretty
	// good on my setup.
	iconWidth_  = fontWidth_ * 2;
	iconHeight_ = fontHeight_;

	updateScrollbars();
}

//------------------------------------------------------------------------------
// Name: resizeEvent
// Desc:
//------------------------------------------------------------------------------
void QDisassemblyView::resizeEvent(QResizeEvent *) {
	updateScrollbars();

	const int line_height     = this->lineHeight();
	const int lines_to_render = 1 + (viewport()->height() / line_height);

	instructionBuffer_.resize(edb::Instruction::MaxSize * lines_to_render);

	// Make PageUp/PageDown scroll through the whole page, but leave the line at
	// the top/bottom visible
	verticalScrollBar()->setPageStep(lines_to_render - 1);
}

//------------------------------------------------------------------------------
// Name: line_height
// Desc:
//------------------------------------------------------------------------------
int QDisassemblyView::lineHeight() const {
	return std::max({fontHeight_, iconHeight_});
}

//------------------------------------------------------------------------------
// Name: updateScrollbars
// Desc:
//------------------------------------------------------------------------------
void QDisassemblyView::updateScrollbars() {
	if (region_) {
		const int total_lines    = region_->size();
		const int viewable_lines = viewport()->height() / lineHeight();
		const int scroll_max     = (total_lines > viewable_lines) ? total_lines - 1 : 0;

		verticalScrollBar()->setMaximum(scroll_max);
	} else {
		verticalScrollBar()->setMaximum(0);
	}
}

//------------------------------------------------------------------------------
// Name: line0
// Desc:
//------------------------------------------------------------------------------
int QDisassemblyView::line0() const {
	return line0_;
}

//------------------------------------------------------------------------------
// Name: line1
// Desc:
//------------------------------------------------------------------------------
int QDisassemblyView::line1() const {

	if (!edb::v1::config().show_jump_arrow) {

		// allocate space for register badge
		// 4 (maximum register name for GPR) + overhead
		return edb::v1::config().show_register_badges ? (5 * fontWidth_ + fontWidth_ / 2) : 0;

	} else if (line1_ == 0) {
		return 15 * fontWidth_;
	} else {
		return line1_;
	}
}

//------------------------------------------------------------------------------
// Name: auto_line2
// Desc:
//------------------------------------------------------------------------------
int QDisassemblyView::autoLine2() const {
	const int elements = addressLength();
	return (elements * fontWidth_) + (fontWidth_ / 2) + iconWidth_ + 1;
}

//------------------------------------------------------------------------------
// Name: line2
// Desc:
//------------------------------------------------------------------------------
int QDisassemblyView::line2() const {
	if (line2_ == 0) {
		return line1() + autoLine2();
	} else {
		return line2_;
	}
}

//------------------------------------------------------------------------------
// Name: line3
// Desc:
//------------------------------------------------------------------------------
int QDisassemblyView::line3() const {
	if (line3_ == 0) {
		return line2() + (DefaultByteWidth * 3) * fontWidth_;
	} else {
		return line3_;
	}
}

//------------------------------------------------------------------------------
// Name: line4
// Desc:
//------------------------------------------------------------------------------
int QDisassemblyView::line4() const {
	if (line4_ == 0) {
		return line3() + 50 * fontWidth_;
	} else {
		return line4_;
	}
}

//------------------------------------------------------------------------------
// Name: address_length
// Desc:
//------------------------------------------------------------------------------
int QDisassemblyView::addressLength() const {
	const int address_len = edb::v1::pointer_size() * CHAR_BIT / 4;
	return address_len + (showAddressSeparator_ ? 1 : 0);
}

//------------------------------------------------------------------------------
// Name: addressFromPoint
// Desc:
//------------------------------------------------------------------------------
edb::address_t QDisassemblyView::addressFromPoint(const QPoint &pos) const {

	Q_ASSERT(region_);

	const edb::address_t address = addressFromCoord(pos.x(), pos.y()) + addressOffset_;
	if (address >= region_->end()) {
		return 0;
	}
	return address;
}

//------------------------------------------------------------------------------
// Name: get_instruction_size
// Desc:
//------------------------------------------------------------------------------
Result<int, QString> QDisassemblyView::getInstructionSize(edb::address_t address, uint8_t *buf, int *size) const {

	Q_ASSERT(buf);
	Q_ASSERT(size);

	if (*size >= 0) {
		bool ok = edb::v1::get_instruction_bytes(address, buf, size);

		if (ok) {
			return instruction_size(buf, *size);
		}
	}

	return make_unexpected(tr("Failed to get instruciton size"));
}

//------------------------------------------------------------------------------
// Name: get_instruction_size
// Desc:
//------------------------------------------------------------------------------
Result<int, QString> QDisassemblyView::getInstructionSize(edb::address_t address) const {

	Q_ASSERT(region_);

	uint8_t buf[edb::Instruction::MaxSize];

	// do the longest read we can while still not crossing region end
	int buf_size = sizeof(buf);
	if (region_->end() != 0 && address + buf_size > region_->end()) {

		if (address <= region_->end()) {
			buf_size = region_->end() - address;
		} else {
			buf_size = 0;
		}
	}

	return getInstructionSize(address, buf, &buf_size);
}

//------------------------------------------------------------------------------
// Name: address_from_coord
// Desc:
//------------------------------------------------------------------------------
edb::address_t QDisassemblyView::addressFromCoord(int x, int y) const {
	Q_UNUSED(x)

	const int line         = y / lineHeight();
	edb::address_t address = verticalScrollBar()->value();

	// add up all the instructions sizes up to the line we want
	for (int i = 0; i < line; ++i) {

		Result<int, QString> size = getInstructionSize(addressOffset_ + address);
		if (size) {
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
	if (region_) {
		if (event->button() == Qt::LeftButton) {
			if (event->x() < line2()) {
				const edb::address_t address = addressFromPoint(event->pos());

				if (region_->contains(address)) {
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

	if (region_) {
		if (event->type() == QEvent::ToolTip) {
			bool show = false;

			auto helpEvent = static_cast<QHelpEvent *>(event);

			if (helpEvent->x() >= line2() && helpEvent->x() < line3()) {

				const edb::address_t address = addressFromPoint(helpEvent->pos());

				uint8_t buf[edb::Instruction::MaxSize];

				// do the longest read we can while still not passing the region end
				size_t buf_size = std::min<edb::address_t>((region_->end() - address), sizeof(buf));
				if (edb::v1::get_instruction_bytes(address, buf, &buf_size)) {
					const edb::Instruction inst(buf, buf + buf_size, address);
					const QString byte_buffer = format_instruction_bytes(inst);

					if ((line2() + byte_buffer.size() * fontWidth_) > line3()) {
						QToolTip::showText(helpEvent->globalPos(), byte_buffer);
						show = true;
					}
				}
			}

			if (!show) {
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

	Q_UNUSED(event)

	movingLine1_      = false;
	movingLine2_      = false;
	movingLine3_      = false;
	movingLine4_      = false;
	selectingAddress_ = false;

	setCursor(Qt::ArrowCursor);
	update();
}

//------------------------------------------------------------------------------
// Name: updateSelectedAddress
// Desc:
//------------------------------------------------------------------------------
void QDisassemblyView::updateSelectedAddress(QMouseEvent *event) {

	if (region_) {
		setSelectedAddress(addressFromPoint(event->pos()));
	}
}

//------------------------------------------------------------------------------
// Name: mousePressEvent
// Desc:
//------------------------------------------------------------------------------
void QDisassemblyView::mousePressEvent(QMouseEvent *event) {
	const int event_x = event->x() - line0();
	if (region_) {
		if (event->button() == Qt::LeftButton) {
			if (near_line(event_x, line1()) && edb::v1::config().show_jump_arrow) {
				movingLine1_ = true;
			} else if (near_line(event_x, line2())) {
				movingLine2_ = true;
			} else if (near_line(event_x, line3())) {
				movingLine3_ = true;
			} else if (near_line(event_x, line4())) {
				movingLine4_ = true;
			} else {
				updateSelectedAddress(event);
				selectingAddress_ = true;
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

	if (region_) {
		const int x_pos = event->x() - line0();

		if (movingLine1_) {
			if (line2_ == 0) {
				line2_ = line2();
			}
			const int min_line1 = 0;
			const int max_line1 = line2() - fontWidth_;
			line1_              = std::min(std::max(min_line1, x_pos), max_line1);
			update();
		} else if (movingLine2_) {
			if (line3_ == 0) {
				line3_ = line3();
			}
			const int min_line2 = line1() + iconWidth_;
			const int max_line2 = line3() - fontWidth_;
			line2_              = std::min(std::max(min_line2, x_pos), max_line2);
			update();
		} else if (movingLine3_) {
			if (line4_ == 0) {
				line4_ = line4();
			}
			const int min_line3 = line2() + fontWidth_ + fontWidth_ / 2;
			const int max_line3 = line4() - fontWidth_;
			line3_              = std::min(std::max(min_line3, x_pos), max_line3);
			update();
		} else if (movingLine4_) {
			const int min_line4 = line3() + fontWidth_;
			const int max_line4 = width() - 1 - (verticalScrollBar()->width() + 3);
			line4_              = std::min(std::max(min_line4, x_pos), max_line4);
			update();
		} else {
			if ((near_line(x_pos, line1()) && edb::v1::config().show_jump_arrow) ||
				near_line(x_pos, line2()) ||
				near_line(x_pos, line3()) ||
				near_line(x_pos, line4())) {
				setCursor(Qt::SplitHCursor);
			} else {
				setCursor(Qt::ArrowCursor);
				if (selectingAddress_) {
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
	return selectedInstructionAddress_;
}

//------------------------------------------------------------------------------
// Name: setSelectedAddress
// Desc:
//------------------------------------------------------------------------------
void QDisassemblyView::setSelectedAddress(edb::address_t address) {

	if (region_) {
		history_.add(address);
		const Result<int, QString> size = getInstructionSize(address);

		if (size) {
			selectedInstructionAddress_ = address;
			selectedInstructionSize_    = *size;
		} else {
			selectedInstructionAddress_ = 0;
			selectedInstructionSize_    = 0;
		}

		update();
	}
}

//------------------------------------------------------------------------------
// Name: selectedSize
// Desc:
//------------------------------------------------------------------------------
int QDisassemblyView::selectedSize() const {
	return selectedInstructionSize_;
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
void QDisassemblyView::addComment(edb::address_t address, QString comment) {
	qDebug("Insert Comment");
	Comment temp_comment = {
		address,
		comment};
	SessionManager::instance().addComment(temp_comment);
	comments_.insert(address, comment);
}

//------------------------------------------------------------------------------
// Name: remove_comment
// Desc: Removes a comment from the comment hash and returns the number of comments removed.
//------------------------------------------------------------------------------
int QDisassemblyView::removeComment(edb::address_t address) {
	SessionManager::instance().removeComment(address);
	return comments_.remove(address);
}

//------------------------------------------------------------------------------
// Name: get_comment
// Desc: Returns a comment assigned for an address or a blank string if there is none.
//------------------------------------------------------------------------------
QString QDisassemblyView::getComment(edb::address_t address) {
	return comments_.value(address, QString(""));
}

//------------------------------------------------------------------------------
// Name: clear_comments
// Desc: Clears all comments in the comment hash.
//------------------------------------------------------------------------------
void QDisassemblyView::clearComments() {
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
		line3_,
		line4_,
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

	if (stateBuffer.size() >= static_cast<int>(sizeof(WidgetState1))) {
		memcpy(&state, stateBuffer.data(), sizeof(WidgetState1));

		if (state.version >= static_cast<int>(sizeof(WidgetState1))) {
			line1_ = state.line1;
			line2_ = state.line2;
			line3_ = state.line3;
			line4_ = state.line4;
		}
	}
}
//------------------------------------------------------------------------------
// Name: restoreComments
// Desc:
//------------------------------------------------------------------------------
void QDisassemblyView::restoreComments(QVariantList &comments_data) {
	qDebug("restoreComments");
	for (auto it = comments_data.begin(); it != comments_data.end(); ++it) {
		QVariantMap data = it->toMap();
		if (const Result<edb::address_t, QString> addr = edb::v1::string_to_address(data["address"].toString())) {
			comments_.insert(*addr, data["comment"].toString());
		}
	}
}
