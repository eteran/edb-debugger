/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
#include "IThread.h"
#include "Instruction.h"
#include "MemoryRegions.h"
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
#include <QFontMetricsF>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QScrollBar>
#include <QSettings>
#include <QTextLayout>
#include <QToolTip>
#include <QtGlobal>
#include <QtMath>

#include <algorithm>
#include <climits>
#include <cmath>

namespace {

struct WidgetStateV1 {
	int version;
	int line1;
	int line2;
	int line3;
	int line4;
};

struct WidgetStateV2 {
	int version;
	double line1;
	double line2;
	double line3;
	double line4;
};

static_assert(sizeof(WidgetStateV1) != sizeof(WidgetStateV2), "WidgetStateV1 and WidgetStateV2 must not be the same size");

constexpr int DefaultByteWidth = 8;

/**
 * @brief Formats the given address as a string, optionally including a separator.
 *
 * @param address The address to format.
 * @param show_separator If true, includes a separator in the formatted address.
 */
template <class T>
QString format_address(T address, bool show_separator) {
	if (show_separator) {
		if constexpr (sizeof(T) == sizeof(uint32_t)) {
			static char buffer[10];
			qsnprintf(buffer, sizeof(buffer), "%04x:%04x", (address >> 16) & 0xffff, address & 0xffff);
			return QString::fromLatin1(buffer, sizeof(buffer) - 1);
		} else if constexpr (sizeof(T) == sizeof(uint64_t)) {
			return edb::value32(address >> 32).toHexString() + QLatin1Char(':') + edb::value32(address).toHexString();
		}
	}

	if constexpr (sizeof(T) == sizeof(uint32_t)) {
		static char buffer[9];
		qsnprintf(buffer, sizeof(buffer), "%04x%04x", (address >> 16) & 0xffff, address & 0xffff);
		return QString::fromLatin1(buffer, sizeof(buffer) - 1);
	} else if constexpr (sizeof(T) == sizeof(uint64_t)) {
		return edb::value64(address).toHexString();
	}
}

/**
 * @brief Checks if the given x-coordinate is near the specified line x-coordinate.
 *
 * @param x The x-coordinate to check.
 * @param linex The x-coordinate of the line to compare against.
 */
bool near_line(qreal x, qreal linex) {
	return std::abs(x - linex) < 3;
}

/**
 * @brief Retrieves the position of a mouse event, accounting for differences between Qt versions.
 *
 * @param event The mouse event from which to retrieve the position.
 * @return The position of the mouse event.
 */
QPointF event_position(const QMouseEvent *event) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	return event->position();
#else
	return event->localPos();
#endif
}

/**
 * @brief Calculates the size of the instruction at the given buffer.
 *
 * @param buffer Pointer to the buffer containing the instruction bytes.
 * @param size Size of the buffer.
 * @return The size of the instruction in bytes.
 */
int instruction_size(const uint8_t *buffer, std::size_t size) {
	edb::Instruction inst(buffer, buffer + size, 0);
	return static_cast<int>(inst.byteSize());
}

/**
 * @brief Formats the bytes of the given instruction as a string.
 *
 * @param inst The instruction whose bytes are to be formatted.
 * @return A QString representing the formatted bytes of the instruction.
 */
QString format_instruction_bytes(const edb::Instruction &inst) {
	auto bytes = QByteArray::fromRawData(reinterpret_cast<const char *>(inst.bytes()), static_cast<int>(inst.byteSize()));
	return edb::v1::format_bytes(bytes);
}

/**
 * @brief Formats the bytes of the given instruction as a string, ensuring that the resulting string fits within the specified maximum pixel width.
 *
 * @param inst The instruction whose bytes are to be formatted.
 * @param maxStringPx The maximum pixel width for the resulting string.
 * @param metrics The QFontMetrics object used to measure the pixel width of the string.
 * @return A QString representing the formatted bytes of the instruction, truncated with an ellipsis if necessary to fit within the specified pixel width.
 */
QString format_instruction_bytes(const edb::Instruction &inst, qreal maxStringPx, const QFontMetricsF &metrics) {
	const QString byte_buffer = format_instruction_bytes(inst);
	return metrics.elidedText(byte_buffer, Qt::ElideRight, qFloor(maxStringPx));
}

/**
 * @brief Determines if the target address is within the same memory region as the instruction address.
 *
 * @param targetAddress The target address to check.
 * @param insnAddress The instruction address to compare against.
 * @return true if the target address is in the same memory region as the instruction address, false otherwise.
 */
bool target_is_local(edb::address_t targetAddress, edb::address_t insnAddress) {

	const auto insnRegion   = edb::v1::memory_regions().findRegion(insnAddress);
	const auto targetRegion = edb::v1::memory_regions().findRegion(targetAddress);
	return !insnRegion->name().isEmpty() && targetRegion && insnRegion->name() == targetRegion->name();
}

}

/**
 * @brief Constructs a QDisassemblyView object with the given parent widget.
 *
 * @param parent The parent widget of the QDisassemblyView.
 */
QDisassemblyView::QDisassemblyView(QWidget *parent)
	: QAbstractScrollArea(parent),
	  highlighter_(new SyntaxHighlighter(this)),
	  breakpointRenderer_(QStringLiteral(":/debugger/images/breakpoint.svg")),
	  currentRenderer_(QStringLiteral(":/debugger/images/arrow-right.svg")),
	  currentBpRenderer_(QStringLiteral(":/debugger/images/arrow-right-red.svg")),
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

	setFont(QFont(QStringLiteral("Monospace"), 8));
	setMouseTracking(true);
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

	connect(verticalScrollBar(), &QScrollBar::actionTriggered, this, &QDisassemblyView::scrollbarActionTriggered);
}

/**
 * @brief Resets the line columns to their default values and updates the disassembly view.
 */
void QDisassemblyView::resetColumns() {
	line1_ = 0.0;
	line2_ = 0.0;
	line3_ = 0.0;
	line4_ = 0.0;
	update();
}

/**
 * @brief Handles key press events for the disassembly view.
 */
void QDisassemblyView::keyPressEvent(QKeyEvent *event) {
	if (event->matches(QKeySequence::MoveToStartOfDocument)) {
		verticalScrollBar()->setValue(0);
	} else if (event->matches(QKeySequence::MoveToEndOfDocument)) {
		verticalScrollBar()->setValue(verticalScrollBar()->maximum());
	} else if (event->matches(QKeySequence::MoveToNextLine)) {
		const edb::address_t selected = selectedAddress();
		const auto idx                = static_cast<int>(showAddresses_.indexOf(selected));
		if (selected != 0 && idx > 0 && idx < showAddresses_.size() - 1 - partialLastLine_) {
			setSelectedAddress(showAddresses_[idx + 1]);
		} else {
			const auto current_offset = static_cast<int>(selected - addressOffset_);
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
		const auto idx                = static_cast<int>(showAddresses_.indexOf(selected));
		if (selected != 0 && idx > 0) {
			// we already know the previous instruction
			setSelectedAddress(showAddresses_[idx - 1]);
		} else {
			const auto current_offset = static_cast<int>(selected - addressOffset_);
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
		updateDisassembly(static_cast<int>(instructions_.size()));

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

/**
 * @brief Attempts to find the address of the instruction one instruction before the given current address.
 *
 * @param analyzer A pointer to the IAnalyzer instance used for analysis.
 * @param current_address The current address (0-based value relative to the beginning of the current region).
 * @return The address of the previous instruction, or the current address minus one if no previous instruction could be found.
 *
 * @note <current_address> is a 0 based value relative to the beginning of the current region, not an absolute address within the program.
 */
int QDisassemblyView::previousInstruction(IAnalyzer *analyzer, int current_address) const {

	// If we have an analyzer, and the current address is within a function
	// then first we find the beginning of that function.
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

				current_address = static_cast<int>(function_start - addressOffset_);
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

/**
 * @brief Attempts to find the address of the instruction <count> instructions before <current_address>.
 *
 * @param current_address The current address (0-based value relative to the beginning of the current region).
 * @param count The number of instructions to go back.
 * @return The address of the instruction <count> instructions before <current_address>.
 *
 * @note <current_address> is a 0 based value relative to the beginning of the current region, not an absolute address within the program.
 */
int QDisassemblyView::previousInstructions(int current_address, int count) const {

	IAnalyzer *const analyzer = edb::v1::analyzer();

	for (int i = 0; i < count; ++i) {
		current_address = previousInstruction(analyzer, current_address);
	}

	return current_address;
}

/**
 * @brief Attempts to find the address of the instruction one instruction after the given current address.
 *
 * @param current_address The current address (0-based value relative to the beginning of the current region).
 * @return The address of the next instruction, or the current address plus one if no next
 */
int QDisassemblyView::followingInstruction(int current_address) const {
	uint8_t buf[edb::Instruction::MaxSize + 1];

	// do the longest read we can while still not passing the region end
	size_t buf_size = sizeof(buf);
	if (region_) {
		buf_size = std::min<size_t>((region_->end() - current_address), sizeof(buf));
	}

	// read in the bytes...
	if (!edb::v1::get_instruction_bytes(addressOffset_ + current_address, buf, &buf_size)) {
		return current_address + 1;
	}

	const edb::Instruction inst(buf, buf + buf_size, current_address);
	return static_cast<int>(current_address + inst.byteSize());
}

/**
 * @brief Attempts to find the address of the instruction <count> instructions after <current_address>.
 *
 * @param current_address The current address (0-based value relative to the beginning of the current region).
 * @param count The number of instructions to go forward.
 * @return The address of the instruction <count> instructions after <current_address>.
 */
int QDisassemblyView::followingInstructions(int current_address, int count) const {

	for (int i = 0; i < count; ++i) {
		current_address = followingInstruction(current_address);
	}

	return current_address;
}

/**
 * @brief Handles wheel events for scrolling the disassembly view.
 *
 * @param e The wheel event.
 */
void QDisassemblyView::wheelEvent(QWheelEvent *e) {

	const int dy           = e->angleDelta().y();
	const int scroll_count = dy / 120;

	// Ctrl+Wheel scrolls by single bytes
	if (e->modifiers() & Qt::ControlModifier) {
		int address = verticalScrollBar()->value();
		verticalScrollBar()->setValue(address - scroll_count);
		e->accept();
		return;
	}

	const int abs_scroll_count = std::abs(scroll_count);

	if (dy > 0) {
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

/**
 * @brief Handles scrollbar action triggered events for the disassembly view.
 *
 * @param action The action triggered on the scrollbar.
 */
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
	default:
		break;
	}
}

/**
 * @brief Sets whether to show the address separator in the disassembly view.
 *
 * @param value true to show the address separator, false to hide it.
 */
void QDisassemblyView::setShowAddressSeparator(bool value) {
	showAddressSeparator_ = value;
}

/**
 * @brief Formats the given address as a string, taking into account whether to show the address separator.
 *
 * @param address The address to format.
 * @return The formatted address as a QString.
 */
QString QDisassemblyView::formatAddress(edb::address_t address) const {
	if (edb::v1::debuggeeIs32Bit()) {
		return format_address<quint32>(static_cast<quint32>(address.toUint()), showAddressSeparator_);
	}
	return format_address(address, showAddressSeparator_);
}

/**
 * @brief Updates the disassembly view and emits the signalUpdated signal.
 */
void QDisassemblyView::update() {
	viewport()->update();
	Q_EMIT signalUpdated();
}

/**
 * @brief Checks if the given address is currently visible in the disassembly view.
 *
 * @param address The address to check.
 * @return true if the address is visible, false otherwise.
 */
bool QDisassemblyView::addressShown(edb::address_t address) const {
	const auto idx = showAddresses_.indexOf(address);
	// if the last line is only partially rendered, consider it outside the
	// viewport.
	return (idx > 0 && idx < showAddresses_.size() - 1 - partialLastLine_);
}

/**
 * @brief Sets the current address in the disassembly view (where EIP is usually).
 *
 * @param address The address to set as the current address.
 */
void QDisassemblyView::setCurrentAddress(edb::address_t address) {
	currentAddress_ = address;
}

/**
 * @brief Sets the memory region to be viewed in the disassembly view.
 *
 * @param r A shared pointer to the IRegion representing the memory region to be viewed.
 */
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

		if (!qFuzzyIsNull(line2_) && line2_ < autoLine2()) {
			line2_ = 0.0;
		}
	}
	update();
}

/**
 * @brief Clears the disassembly view.
 */
void QDisassemblyView::clear() {
	setRegion(nullptr);
}

/**
 * @brief Sets the address offset for the disassembly view.
 *
 * @param address The address offset to set.
 */
void QDisassemblyView::setAddressOffset(edb::address_t address) {
	addressOffset_ = address;
}

/**
 * @brief Scrolls the disassembly view to the specified address.
 *
 * @param address The address to scroll to.
 */
void QDisassemblyView::scrollTo(edb::address_t address) {
	verticalScrollBar()->setValue(static_cast<int>(address - addressOffset_));
}

/**
 * @brief Formats the instruction as a string, including symbolic names for jump and call targets if applicable.
 *
 * @param inst The instruction to format.
 * @return A QString representing the formatted instruction.
 */
QString QDisassemblyView::instructionString(const edb::Instruction &inst) const {
	auto opcode = QString::fromStdString(edb::v1::formatter().toString(inst));

	if (is_call(inst) || is_jump(inst)) {
		if (inst.operandCount() == 1) {
			const auto oper = inst[0];
			if (is_immediate(oper)) {

				const bool showSymbolicAddresses = edb::v1::config().show_symbolic_addresses;

				static const QRegularExpression addrPattern(QStringLiteral("#?0x[0-9a-fA-F]+"));
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
					if (showSymbolicAddresses) {
						opcode.replace(addrPattern, sym);
					} else {
						opcode.append(QStringLiteral(" <%2>").arg(sym));
					}
				}
			}
		}
	}

	return opcode;
}

/**
 * @brief Draws the given instruction on the disassembly view using the specified QPainter.
 *
 * @param painter The QPainter object used for drawing.
 * @param inst The instruction to be drawn.
 * @param ctx The drawing context containing layout information.
 * @param y The y-coordinate where the instruction should be drawn.
 * @param selected A boolean indicating whether the instruction is currently selected.
 */
void QDisassemblyView::drawInstruction(QPainter &painter, const edb::Instruction &inst, const DrawingContext *ctx, qreal y, bool selected) {

	painter.save();

	const bool is_filling        = edb::v1::arch_processor().isFilling(inst);
	const qreal x                = fontWidth_ + fontWidth_ + ctx->l3 + (fontWidth_ / 2.0);
	const qreal inst_pixel_width = ctx->l4 - x;

	const bool syntax_highlighting_enabled = edb::v1::config().syntax_highlighting_enabled && !selected;

	QString opcode           = instructionString(inst);
	const auto opcode_length = static_cast<int>(opcode.length());

	if (is_filling) {
		if (syntax_highlighting_enabled) {
			painter.setPen(fillingBytesColor_);
		}

		opcode = painter.fontMetrics().elidedText(opcode, Qt::ElideRight, qFloor(inst_pixel_width));

		painter.drawText(QRectF(x, y, opcode_length * fontWidth_, ctx->lineHeight), Qt::AlignVCenter, opcode);
	} else {

		// NOTE(eteran): do this early, so that elided text still gets the part shown
		// properly highlighted
		QVector<QTextLayout::FormatRange> highlightData;
		if (syntax_highlighting_enabled) {
			highlightData = highlighter_->highlightBlock(opcode);
		}

		opcode = painter.fontMetrics().elidedText(opcode, Qt::ElideRight, qFloor(inst_pixel_width));

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

				map = new QPixmap(QSize(qCeil(opcode_length * fontWidth_), qCeil(ctx->lineHeight)) * devicePixelRatio());
				map->setDevicePixelRatio(devicePixelRatio());
				map->fill(Qt::transparent);
				QPainter cache_painter(map);
				cache_painter.setPen(painter.pen());
				cache_painter.setFont(painter.font());

				// now the render the text at the location given
				textLayout.draw(&cache_painter, QPoint(0, 0), highlightData);
				syntaxCache_.insert(opcode, map);
			}
			painter.drawPixmap(QPointF(x, y), *map);
		} else {
			painter.drawText(QRectF(x, y, opcode_length * fontWidth_, ctx->lineHeight), Qt::AlignVCenter, opcode);
		}
	}

	painter.restore();
}

/**
 * @brief Paints the background of one or more lines in the disassembly view with the specified brush.
 *
 * @param painter The QPainter object used for painting.
 * @param brush The QBrush object specifying the background color or pattern.
 * @param line The starting line number to paint the background.
 * @param num_lines The number of lines to paint the background.
 */
void QDisassemblyView::paintLineBg(QPainter &painter, QBrush brush, int line, int num_lines) {
	const auto lh = lineHeight();
	painter.fillRect(QRectF(0.0, lh * line, width(), lh * num_lines), brush);
}

/**
 * @brief Returns the line number corresponding to the given address in the disassembly view, if it exists.
 *
 * @param addr The address to find the corresponding line number for.
 * @return An optional containing the line number if the address is found, or an empty optional if the address is not found in the disassembly view.
 */
std::optional<unsigned int> QDisassemblyView::getLineOfAddress(edb::address_t addr) const {

	if (!showAddresses_.isEmpty()) {
		if (addr >= showAddresses_[0] && addr <= showAddresses_[showAddresses_.size() - 1]) {
			auto pos = static_cast<int>(std::find(showAddresses_.begin(), showAddresses_.end(), addr) - showAddresses_.begin());
			if (pos < showAddresses_.size()) { // address was found
				return pos;
			}
		}
	}

	return {};
}

/**
 * @brief Updates the disassembly view by reading instruction bytes from memory and populating the instructions_ and showAddresses_ vectors.
 *
 * @param lines_to_render The number of lines to render in the disassembly view.
 * @return The actual number of lines rendered after updating the disassembly view.
 */
int QDisassemblyView::updateDisassembly(int lines_to_render) {
	instructions_.clear();
	showAddresses_.clear();

	auto bufsize                       = static_cast<int>(instructionBuffer_.size());
	uint8_t *inst_buf                  = instructionBuffer_.data();
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
			offset += static_cast<int>(instructions_[line].byteSize());
		} else {
			++offset;
		}
		line++;
	}
	Q_ASSERT(line <= lines_to_render);
	if (lines_to_render != line) {
		partialLastLine_ = false;
	}

	lines_to_render = line;
	return lines_to_render;
}

/**
 * @brief Returns the line number of the currently selected instruction in the disassembly view.
 * This is in units of visible lines in the UI, so there is NO CHANCE of a widget being tall enough
 * that it can show more than 65535 lines of disassembly.
 *
 * @return The line number of the selected instruction, or 65535 if no instruction is selected.
 */
int QDisassemblyView::getSelectedLineNumber() const {

	for (size_t line = 0; line < instructions_.size(); ++line) {
		if (instructions_[line].rva() == selectedAddress()) {
			return static_cast<int>(line);
		}
	}

	return 65535; // can't accidentally hit this;
}

/**
 * @brief Draws the header and background of the disassembly view, including the header gray area, alternated background colors, and highlighting for selected lines.
 *
 * @param painter The QPainter object used for drawing.
 * @param ctx The drawing context containing layout information.
 * @param binary_info A unique pointer to the IBinary object containing information about the binary being
 */
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

/**
 * @brief Draws register badges on the disassembly view, indicating which registers contain values that correspond to addresses in the currently displayed instructions.
 *
 * @param painter The QPainter object used for drawing.
 * @param ctx The drawing context containing layout information.
 */
void QDisassemblyView::drawRegisterBadges(QPainter &painter, DrawingContext *ctx) {

	painter.save();
	if (IProcess *process = edb::v1::debugger_core->process()) {

		if (process->isPaused()) {

			if (std::shared_ptr<IThread> thread = process->currentThread()) {
				State state;
				thread->getState(&state);

				std::vector<QString> badge_labels(ctx->linesToRender);
				{
					unsigned int reg_num = 0;
					Register reg;
					reg = state.gpRegister(reg_num);

					while (reg.valid()) {
						// Does addr appear here?
						edb::address_t addr = reg.valueAsAddress();

						if (std::optional<unsigned int> line = getLineOfAddress(addr)) {
							if (!badge_labels[*line].isEmpty()) {
								badge_labels[*line].append(QStringLiteral(", "));
							}
							badge_labels[*line].append(reg.name());
						}

						// what about [addr]?
						if (process->readBytes(addr, &addr, edb::v1::pointer_size())) {
							if (std::optional<unsigned int> line = getLineOfAddress(addr)) {
								if (!badge_labels[*line].isEmpty()) {
									badge_labels[*line].append(QStringLiteral(", "));
								}
								badge_labels[*line].append(QStringLiteral("[") + reg.name() + QStringLiteral("]"));
							}
						}

						reg = state.gpRegister(++reg_num);
					}
				}

				painter.setPen(badgeForegroundColor_);

				for (int line = 0; line < ctx->linesToRender; line++) {
					if (!badge_labels[line].isEmpty()) {

						auto badge_length = static_cast<int>(badge_labels[line].size());

						const qreal width          = badge_length * fontWidth_ + fontWidth_ / 2.0;
						const qreal height         = ctx->lineHeight;
						const qreal triangle_point = line1() - 3.0;
						const qreal x              = triangle_point - (height / 2.0) - width;
						const qreal y              = line * ctx->lineHeight;

						// if badge is not in viewpoint, then don't draw
						if (x < 0) {
							continue;
						}

						ctx->lineBadgeWidth[line] = line1() - x;

						QRectF bounds(x, y, width, height);

						// draw a rectangle + box around text
						QPainterPath path;
						path.addRect(bounds);
						path.moveTo(bounds.x() + bounds.width(), bounds.y());                   // top right
						path.lineTo(triangle_point, bounds.y() + bounds.height() / 2.);         // triangle point
						path.lineTo(bounds.x() + bounds.width(), bounds.y() + bounds.height()); // bottom right
						painter.fillPath(path, badgeBackgroundColor_);

						painter.drawText(
							QRectF(
								bounds.x() + fontWidth_ / 4.0,
								line * ctx->lineHeight,
								fontWidth_ * badge_length,
								ctx->lineHeight),
							Qt::AlignVCenter,
							(edb::v1::config().uppercase_disassembly ? badge_labels[line].toUpper() : badge_labels[line]));
					}
				}
			}
		}
	}
	painter.restore();
}

/**
 * @brief Draws symbol names for the instructions in the disassembly view, if available, using the specified QPainter.
 *
 * @param painter The QPainter object used for drawing.
 * @param ctx The drawing context containing layout information.
 */
void QDisassemblyView::drawSymbolNames(QPainter &painter, const DrawingContext *ctx) {
	painter.save();

	painter.setPen(palette().color(ctx->group, QPalette::Text));
	const qreal x     = ctx->l1 + autoLine2();
	const qreal width = ctx->l2 - x;
	if (width > 0) {
		for (int line = 0; line < ctx->linesToRender; line++) {

			if (ctx->selectedLines != line) {
				auto address      = showAddresses_[line];
				const QString sym = edb::v1::symbol_manager().findAddressName(address);
				if (!sym.isEmpty()) {
					const QString symbol_buffer = painter.fontMetrics().elidedText(sym, Qt::ElideRight, qFloor(width));

					painter.drawText(QRectF(x, line * ctx->lineHeight, width, ctx->lineHeight), Qt::AlignVCenter, symbol_buffer);
				}
			}
		}

		if (ctx->selectedLines < ctx->linesToRender) {
			int line = ctx->selectedLines;
			painter.setPen(palette().color(ctx->group, QPalette::HighlightedText));
			auto address      = showAddresses_[line];
			const QString sym = edb::v1::symbol_manager().findAddressName(address);
			if (!sym.isEmpty()) {
				const QString symbol_buffer = painter.fontMetrics().elidedText(sym, Qt::ElideRight, qFloor(width));

				painter.drawText(QRectF(x, line * ctx->lineHeight, width, ctx->lineHeight), Qt::AlignVCenter, symbol_buffer);
			}
		}
	}

	painter.restore();
}

/**
 * @brief Draws the sidebar elements of the disassembly view, including addresses and icons for breakpoints and the current instruction pointer (EIP), using the specified QPainter.
 *
 * @param painter The QPainter object used for drawing.
 * @param ctx The drawing context containing layout information.
 */
void QDisassemblyView::drawSidebarElements(QPainter &painter, const DrawingContext *ctx) {

	painter.save();
	painter.setPen(addressForegroundColor_);

	const auto icon_x     = ctx->l1 + 1.0;
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
			icon->render(&painter, QRectF(icon_x, line * ctx->lineHeight + 1.0, iconWidth_, iconHeight_));
		}

		const QString address_buffer = formatAddress(address);
		// draw the address
		painter.drawText(QRectF(addr_x, line * ctx->lineHeight, addr_width, ctx->lineHeight), Qt::AlignVCenter, address_buffer);
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

/**
 * @brief Draws the instruction bytes for each instruction in the disassembly view, using the specified QPainter and drawing context.
 *
 * @param painter The QPainter object used for drawing.
 * @param ctx The drawing context containing layout information.
 */
void QDisassemblyView::drawInstructionBytes(QPainter &painter, const DrawingContext *ctx) {

	painter.save();

	const qreal bytes_width = ctx->l3 - ctx->l2 - fontWidth_ / 2.0;
	const QFontMetricsF metrics(painter.font());

	auto painter_lambda = [&](const edb::Instruction &inst, int line) {
		const QString byte_buffer = format_instruction_bytes(
			inst,
			bytes_width,
			metrics);

		painter.drawText(
			QRectF(
				ctx->l2 + (fontWidth_ / 2.0),
				line * ctx->lineHeight,
				bytes_width,
				ctx->lineHeight),
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

/**
 * @brief Draws function markers in the disassembly view, indicating the start and end of functions, using the specified QPainter and drawing context.
 *
 * @param painter The QPainter object used for drawing.
 * @param ctx The drawing context containing layout information.
 */
void QDisassemblyView::drawFunctionMarkers(QPainter &painter, const DrawingContext *ctx) {

	painter.save();

	IAnalyzer *const analyzer = edb::v1::analyzer();
	const qreal x             = ctx->l3 + fontWidth_;
	if (analyzer && ctx->l4 - x > fontWidth_ / 2.0) {
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
						const qreal y = start_line * ctx->lineHeight;
						// half of a horizontal
						painter.drawLine(QLineF(
							x,
							y + ctx->lineHeight / 2.0,
							x + fontWidth_ / 2.0,
							y + ctx->lineHeight / 2.0));

						// half of a vertical
						painter.drawLine(QLineF(
							x,
							y + ctx->lineHeight / 2.0,
							x,
							y + ctx->lineHeight));

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
						const qreal y = end_line * ctx->lineHeight;

						// half of a vertical
						painter.drawLine(QLineF(
							x,
							y,
							x,
							y + ctx->lineHeight / 2.0));

						// half of a horizontal
						painter.drawLine(QLineF(
							x,
							y + ctx->lineHeight / 2.0,
							ctx->l3 + (fontWidth_ / 2.0) + fontWidth_,
							y + ctx->lineHeight / 2.0));

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
					painter.drawLine(QLineF(x, start_line * ctx->lineHeight, x, end_line * ctx->lineHeight));
				}
				return true;
			});
		}
	}

	painter.restore();
}

/**
 * @brief Draws comments for each instruction in the disassembly view, using the specified QPainter and drawing context.
 * If no comment is available for an instruction, it attempts to draw ASCII representations of immediate constants.
 *
 * @param painter The QPainter object used for drawing.
 * @param ctx The drawing context containing layout information.
 */
void QDisassemblyView::drawComments(QPainter &painter, const DrawingContext *ctx) {

	painter.save();

	const qreal x_pos         = ctx->l4 + fontWidth_ + (fontWidth_ / 2.0);
	const qreal comment_width = width() - x_pos;

	for (int line = 0; line < ctx->linesToRender; line++) {
		auto address = showAddresses_[line];

		if (ctx->selectedLines == line) {
			painter.setPen(palette().color(ctx->group, QPalette::HighlightedText));
		} else {
			painter.setPen(palette().color(ctx->group, QPalette::Text));
		}

		QString annotation = comments_.value(address, QStringLiteral(""));
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

		painter.drawText(QRectF(x_pos, line * ctx->lineHeight, comment_width, ctx->lineHeight), Qt::AlignLeft, annotation);
	}

	painter.restore();
}

/**
 * @brief Draws jump arrows in the disassembly view, indicating the flow of control between instructions, using the specified QPainter and drawing context.
 *
 * @param painter The QPainter object used for drawing.
 * @param ctx The drawing context containing layout information.
 */
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

	auto isLineOverlap = [&](qreal line1_head, qreal line1_tail, qreal line2_head, qreal line2_tail, bool edge_overlap) -> bool {
		const qreal jump1_arrow_min = std::min(line1_head, line1_tail);
		const qreal jump1_arrow_max = std::max(line1_head, line1_tail);
		const qreal jump2_arrow_min = std::min(line2_head, line2_tail);
		const qreal jump2_arrow_max = std::max(line2_head, line2_tail);

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

		JumpArrow &jump_arrow      = jump_arrow_vec[jump_arrow_idx];
		bool is_dst_upward         = jump_arrow.target < instructions_[jump_arrow.sourceLine].rva();
		const qreal jump_arrow_dst = jump_arrow.destInViewport ? jump_arrow.destLine * ctx->lineHeight : (is_dst_upward ? 0.0 : static_cast<qreal>(viewport()->height()));

		const qreal size_block = fontWidth_ * 2.0;
		qreal start_at_block   = size_block;
		int badge_line         = -1;

		if (ctx->lineBadgeWidth.find(jump_arrow.sourceLine) != ctx->lineBadgeWidth.end()) {
			badge_line = jump_arrow.sourceLine;
		} else if (ctx->lineBadgeWidth.find(jump_arrow.destLine) != ctx->lineBadgeWidth.end()) {
			badge_line = jump_arrow.destLine;
		}

		// check if current arrow overlaps with register badge
		for (const auto &[line, badgeWidth] : ctx->lineBadgeWidth) {
			Q_UNUSED(badgeWidth)

			bool is_overlap_with_badge = isLineOverlap(
				jump_arrow.sourceLine * ctx->lineHeight + 1.0,
				jump_arrow_dst - 1.0,
				line * ctx->lineHeight,
				(line + 1) * ctx->lineHeight,
				true);

			if (is_overlap_with_badge) {
				badge_line = line;
				break;
			}
		}

		if (badge_line != -1) {
			start_at_block = size_block + size_block * std::floor(ctx->lineBadgeWidth.at(badge_line) / size_block);
		}

		// first-fit search for horizontal length position to place new arrow
		for (qreal current_selected_len = start_at_block;; current_selected_len += size_block) {

			bool is_length_good = true;

			// check if current arrow overlaps with previous arrow
			for (size_t jump_arrow_prev_idx = 0; jump_arrow_prev_idx < jump_arrow_idx && is_length_good; jump_arrow_prev_idx++) {

				const JumpArrow &jump_arrow_prev = jump_arrow_vec[jump_arrow_prev_idx];

				bool is_dst_upward_prev         = jump_arrow_prev.target < instructions_[jump_arrow_prev.sourceLine].rva();
				const qreal jump_arrow_prev_dst = jump_arrow_prev.destInViewport ? jump_arrow_prev.destLine * ctx->lineHeight : (is_dst_upward_prev ? 0.0 : static_cast<qreal>(viewport()->height()));

				bool jumps_overlap = isLineOverlap(
					jump_arrow.sourceLine * ctx->lineHeight,
					jump_arrow_dst,
					jump_arrow_prev.sourceLine * ctx->lineHeight,
					jump_arrow_prev_dst,
					false);

				// if jump blocks overlap and this horizontal length has been taken before
				if (jumps_overlap && qFuzzyCompare(current_selected_len, jump_arrow_prev.horizontalLength)) {
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

	if (IProcess *process = edb::v1::debugger_core->process()) {
		if (std::shared_ptr<IThread> thread = process->currentThread()) {
			State state;
			thread->getState(&state);

			painter.save();
			painter.setRenderHint(QPainter::Antialiasing, true);

			for (const JumpArrow &jump_arrow : jump_arrow_vec) {

				bool is_dst_upward = jump_arrow.target < instructions_[jump_arrow.sourceLine].rva();

				// horizontal line
				const qreal end_x   = ctx->l1 - 3.0;
				const qreal start_x = end_x - jump_arrow.horizontalLength;

				// vertical line
				const qreal src_y = jump_arrow.sourceLine * ctx->lineHeight + (fontHeight_ / 2.0);
				qreal dst_y;

				if (jump_arrow.destInMiddleOfInstruction) {
					dst_y = jump_arrow.destLine * ctx->lineHeight;
				} else {
					dst_y = jump_arrow.destLine * ctx->lineHeight + (fontHeight_ / 2.0);
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
				const qreal arrow_pixel_offset = std::fmod(arrow_width, 2.) == 1 ? 0.5 : 0.0;
				painter.save();
				painter.translate(arrow_pixel_offset, arrow_pixel_offset);

				painter.setPen(QPen(arrow_color, arrow_width, arrow_style));

				qreal src_reg_badge_width = 0.0;
				qreal dst_reg_badge_width = 0.0;

				if (ctx->lineBadgeWidth.find(jump_arrow.sourceLine) != ctx->lineBadgeWidth.end()) {
					src_reg_badge_width = ctx->lineBadgeWidth.at(jump_arrow.sourceLine);
				} else if (ctx->lineBadgeWidth.find(jump_arrow.destLine) != ctx->lineBadgeWidth.end()) {
					dst_reg_badge_width = ctx->lineBadgeWidth.at(jump_arrow.destLine);
				}

				if (jump_arrow.destInViewport) {

					QPointF points[] = {
						QPointF(end_x - src_reg_badge_width, src_y),
						QPointF(start_x, src_y),
						QPointF(start_x, dst_y),
						QPointF(end_x - dst_reg_badge_width - fontWidth_ / 3.0, dst_y)};

					painter.drawPolyline(points, 4);

					// draw arrow tips
					QPainterPath path;
					path.moveTo(end_x - dst_reg_badge_width, dst_y);
					path.lineTo(end_x - dst_reg_badge_width - (fontWidth_ / 2.0), dst_y - (fontHeight_ / 3.0));
					path.lineTo(end_x - dst_reg_badge_width - (fontWidth_ / 2.0), dst_y + (fontHeight_ / 3.0));
					path.lineTo(end_x - dst_reg_badge_width, dst_y);
					painter.fillPath(path, QBrush(arrow_color));

				} else if (is_dst_upward) { // if dst out of viewport, and arrow facing upward

					QPointF points[] = {
						QPointF(end_x - src_reg_badge_width, src_y),
						QPointF(start_x, src_y),
						QPointF(start_x, fontWidth_ / 3.0)};

					painter.drawPolyline(points, 3);

					// draw arrow tips
					QPainterPath path;
					path.moveTo(start_x, 0);
					path.lineTo(start_x - (fontWidth_ / 2.0), fontHeight_ / 3.0);
					path.lineTo(start_x + (fontWidth_ / 2.0), fontHeight_ / 3.0);
					path.lineTo(start_x, 0);
					painter.fillPath(path, QBrush(arrow_color));

				} else { // if dst out of viewport, and arrow facing downward

					QPointF points[] = {
						QPointF(end_x - src_reg_badge_width, src_y),
						QPointF(start_x, src_y),
						QPointF(start_x, viewport()->height() - fontWidth_ / 3.0)};

					painter.drawPolyline(points, 3);

					// draw arrow tips
					QPainterPath path;
					path.moveTo(start_x, viewport()->height() - 1);
					path.lineTo(start_x - (fontWidth_ / 2.0), viewport()->height() - (fontHeight_ / 3.0) - 1.0);
					path.lineTo(start_x + (fontWidth_ / 2.0), viewport()->height() - (fontHeight_ / 3.0) - 1.0);
					path.lineTo(start_x, viewport()->height() - 1);
					painter.fillPath(path, QBrush(arrow_color));
				}

				painter.restore();
			}
		}
	}
	painter.restore();
}

/**
 * @brief Draws the disassembly instructions in the disassembly view, using the specified QPainter and drawing context.
 *
 * @param painter The QPainter object used for drawing.
 * @param ctx The drawing context containing layout information.
 */
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

/**
 * @brief Draws vertical dividers in the disassembly view, separating different sections of the view, using the specified QPainter and drawing context.
 *
 * @param painter The QPainter object used for drawing.
 * @param ctx The drawing context containing layout information.
 */
void QDisassemblyView::drawDividers(QPainter &painter, const DrawingContext *ctx) {

	painter.save();

	const QPen divider_pen = palette().color(ctx->group, QPalette::WindowText);
	painter.setPen(divider_pen);

	if (edb::v1::config().show_jump_arrow || edb::v1::config().show_register_badges) {
		painter.drawLine(QLineF(ctx->l1, 0.0, ctx->l1, height()));
	}

	painter.drawLine(QLineF(ctx->l2, 0.0, ctx->l2, height()));
	painter.drawLine(QLineF(ctx->l3, 0.0, ctx->l3, height()));
	painter.drawLine(QLineF(ctx->l4, 0.0, ctx->l4, height()));

	painter.restore();
}

/**
 * @brief Handles the paint event for the disassembly view, rendering the disassembly instructions, addresses, comments, and other visual elements using a QPainter.

 * @param event The paint event.
 */
void QDisassemblyView::paintEvent(QPaintEvent * /*event*/) {

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

	const qreal line_height = this->lineHeight();
	int lines_to_render     = qFloor(viewport()->height() / line_height);

	// Possibly render another instruction just outside the viewport
	if (std::fmod(static_cast<qreal>(viewport()->height()), line_height) > 0.0) {
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
		std::map<int, qreal>()};

	drawHeaderAndBackground(painter, &context, binary_info);

	if (edb::v1::config().show_register_badges) {
		drawRegisterBadges(painter, &context);
	}

	drawSymbolNames(painter, &context);

	// SELECTION, BREAKPOINT, EIP & ADDRESS
	drawSidebarElements(painter, &context);

	// INSTRUCTION BYTES AND REL-JMP INDICATOR RENDERING
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

/**
 * @brief Sets the font for the disassembly view and recalculates metrics.
 *
 * @param f The new font to set for the disassembly view.
 */
void QDisassemblyView::setFont(const QFont &f) {
	syntaxCache_.clear();

	QFont newFont(f);

	// NOTE(eteran): fix for #414 ?
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	QT_WARNING_PUSH
	QT_WARNING_DISABLE_DEPRECATED
	newFont.setStyleStrategy(QFont::ForceIntegerMetrics);
	QT_WARNING_POP
#else
	newFont.setHintingPreference(QFont::PreferFullHinting);
	newFont.setStyleStrategy(QFont::NoFontMerging);
#endif

	// TODO: assert that we are using a fixed font & find out if we care?
	QAbstractScrollArea::setFont(newFont);

	// recalculate all of our metrics/offsets
	const QFontMetricsF metrics(newFont);
	fontWidth_  = metrics.horizontalAdvance(QLatin1Char('M'));
	fontHeight_ = metrics.lineSpacing() + 1.0;

	// NOTE(eteran): we let the icons be a bit wider than the font itself, since things
	// like arrows don't tend to have square bounds. A ratio of 2:1 seems to look pretty
	// good on my setup.
	iconWidth_  = fontWidth_ * 2.0;
	iconHeight_ = fontHeight_;

	updateScrollbars();
}

/**
 * @brief Handles the resize event for the disassembly view, updating scrollbars and resizing the instruction buffer based on the new viewport size.
 *
 * @param event The resize event.
 */
void QDisassemblyView::resizeEvent(QResizeEvent * /*event*/) {
	updateScrollbars();

	const qreal line_height   = this->lineHeight();
	const int lines_to_render = 1 + qFloor(viewport()->height() / line_height);

	instructionBuffer_.resize(edb::Instruction::MaxSize * lines_to_render);

	// Make PageUp/PageDown scroll through the whole page, but leave the line at
	// the top/bottom visible
	verticalScrollBar()->setPageStep(std::max(0, lines_to_render - 1));
}

/**
 * @brief Returns the height of a single line in the disassembly view, which is the maximum of the font height and icon height.
 *
 * @return The height of a single line in pixels.
 */
qreal QDisassemblyView::lineHeight() const {
	return std::max({fontHeight_, iconHeight_});
}

/**
 * @brief Updates the scrollbars of the disassembly view based on the current region and the number of lines that can be displayed in the viewport.
 * If a region is set, it calculates the total number of lines and the maximum value for the vertical scrollbar.
 * If no region is set, it sets the maximum value of the vertical scrollbar to 0.
 */
void QDisassemblyView::updateScrollbars() {
	if (region_) {
		const auto total_lines   = static_cast<int>(region_->size());
		const int viewable_lines = qFloor(viewport()->height() / lineHeight());
		const int scroll_max     = (total_lines > viewable_lines) ? total_lines - 1 : 0;

		verticalScrollBar()->setMaximum(scroll_max);
	} else {
		verticalScrollBar()->setMaximum(0);
	}
}

/**
 * @brief Returns the starting position of the first line in the disassembly view.
 *
 * @return The starting position of the first line in pixels.
 */
qreal QDisassemblyView::line0() const {
	return line0_;
}

/**
 * @brief Returns the starting position of the second line in the disassembly view, which is calculated based on the configuration settings for jump arrows and register badges.
 *
 * @return The starting position of the second line in pixels.
 */
qreal QDisassemblyView::line1() const {

	if (!edb::v1::config().show_jump_arrow) {
		// allocate space for register badge
		// 4 (maximum register name for GPR) + overhead
		return edb::v1::config().show_register_badges ? (5 * fontWidth_ + fontWidth_ / 2.0) : 0.0;
	}

	if (qFuzzyIsNull(line1_)) {
		return 15 * fontWidth_;
	}

	return line1_;
}

/**
 * @brief Returns the position to use for the third line if none is currently set. This is calculated based on the address length, font width, and icon width.
 *
 * @return The starting position of the third line in pixels.
 */
qreal QDisassemblyView::autoLine2() const {
	const int elements = addressLength();
	return (elements * fontWidth_) + (fontWidth_ / 2.0) + iconWidth_ + 1.0;
}

/**
 * @brief Returns the starting position of the third line in the disassembly view.
 *
 * @return The starting position of the third line in pixels.
 */
qreal QDisassemblyView::line2() const {
	if (qFuzzyIsNull(line2_)) {
		return line1() + autoLine2();
	}
	return line2_;
}

/**
 * @brief Returns the starting position of the fourth line in the disassembly view, which is calculated based on the position of the third line and a default byte width multiplied by 3 and the font width.
 *
 * @return The starting position of the fourth line in pixels.
 */
qreal QDisassemblyView::line3() const {
	if (qFuzzyIsNull(line3_)) {
		return line2() + (DefaultByteWidth * 3) * fontWidth_;
	}
	return line3_;
}

/**
 * @brief Returns the starting position of the fifth line in the disassembly view, which is calculated based on the position of the fourth line and a default byte width multiplied by 50 and the font width.
 *
 * @return The starting position of the fifth line in pixels.
 */
qreal QDisassemblyView::line4() const {
	if (qFuzzyIsNull(line4_)) {
		return line3() + 50 * fontWidth_;
	}
	return line4_;
}

/**
 * @brief Returns the length of the address field in the disassembly view, which is calculated based on the pointer size and whether an address separator is shown.
 *
 * @return The length of the address field in characters.
 */
int QDisassemblyView::addressLength() const {
	const auto address_len = static_cast<int>(edb::v1::pointer_size() * CHAR_BIT / 4);
	return address_len + (showAddressSeparator_ ? 1 : 0);
}

/**
 * @brief Returns the address corresponding to a given point in the disassembly view, based on the x and y coordinates of the point.
 *
 * @param pos The point in the disassembly view for which to retrieve the corresponding address.
 * @return The address corresponding to the given point, or 0 if the address is outside
 */
edb::address_t QDisassemblyView::addressFromPoint(const QPointF &pos) const {

	Q_ASSERT(region_);

	const edb::address_t address = addressFromCoord(pos.x(), pos.y()) + addressOffset_;
	if (address >= region_->end()) {
		return 0;
	}
	return address;
}

/**
 * @brief Returns the size of the instruction at a given address in the disassembly view, using a provided buffer to store the instruction bytes.
 *
 * @param address The address of the instruction for which to retrieve the size.
 * @param buf A pointer to a buffer where the instruction bytes will be stored.
 * @param size A pointer to an integer that specifies the size of the buffer and will be updated with the actual size of the instruction.
 * @return The size of the instruction in bytes, or an error message if the instruction size could not be determined.
 */
Result<int, QString> QDisassemblyView::getInstructionSize(edb::address_t address, uint8_t *buf, int *size) const {

	Q_ASSERT(buf);
	Q_ASSERT(size);

	if (*size >= 0) {
		bool ok = edb::v1::get_instruction_bytes(address, buf, size);

		if (ok) {
			return instruction_size(buf, *size);
		}
	}

	return make_unexpected(tr("Failed to get instruction size"));
}

/**
 * @brief Returns the size of the instruction at a given address in the disassembly view, using an internal buffer to store the instruction bytes.
 *
 * @param address The address of the instruction for which to retrieve the size.
 * @return The size of the instruction in bytes, or an error message if the instruction size could not be determined.
 */
Result<int, QString> QDisassemblyView::getInstructionSize(edb::address_t address) const {

	Q_ASSERT(region_);

	uint8_t buf[edb::Instruction::MaxSize];

	// do the longest read we can while still not crossing region end
	int buf_size = sizeof(buf);
	if (region_->end() != 0 && address + buf_size > region_->end()) {

		if (address <= region_->end()) {
			buf_size = static_cast<int>(region_->end() - address);
		} else {
			buf_size = 0;
		}
	}

	return getInstructionSize(address, buf, &buf_size);
}

/**
 * @brief Returns the address corresponding to a given coordinate in the disassembly view, based on the x and y coordinates of the point.
 *
 * @param x The x-coordinate of the point in the disassembly view.
 * @param y The y-coordinate of the point in the disassembly view.
 * @return The address corresponding to the given coordinate, or 0 if the address is outside the current region.
 */
edb::address_t QDisassemblyView::addressFromCoord(qreal x, qreal y) const {
	Q_UNUSED(x)

	const int line         = qFloor(y / lineHeight());
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

/**
 * @brief Handles the mouse double-click event in the disassembly view, toggling breakpoints for instructions within the current region when the left mouse button is double-clicked.
 *
 * @param event The mouse event containing information about the double-click action.
 */
void QDisassemblyView::mouseDoubleClickEvent(QMouseEvent *event) {
	const QPointF pos = event_position(event);

	if (region_) {
		if (event->button() == Qt::LeftButton) {
			if (pos.x() < line2()) {
				const edb::address_t address = addressFromPoint(pos);

				if (region_->contains(address)) {
					Q_EMIT breakPointToggled(address);
					update();
				}
			}
		}
	}
}

/**
 * @brief Handles events in the disassembly view, including tooltip events for displaying instruction bytes when hovering over certain areas of the view.
 *
 * @param event The event to be handled.
 * @return true if the event was handled, false otherwise.
 */
bool QDisassemblyView::event(QEvent *event) {

	if (region_) {
		if (event->type() == QEvent::ToolTip) {
			bool show = false;

			auto helpEvent = static_cast<QHelpEvent *>(event);
			const QPointF helpPos(helpEvent->pos());

			if (helpPos.x() >= line2() && helpPos.x() < line3()) {

				const edb::address_t address = addressFromPoint(helpPos);

				uint8_t buf[edb::Instruction::MaxSize];

				// do the longest read we can while still not passing the region end
				size_t buf_size = std::min<edb::address_t>((region_->end() - address), sizeof(buf));
				if (edb::v1::get_instruction_bytes(address, buf, &buf_size)) {
					const edb::Instruction inst(buf, buf + buf_size, address);
					const QString byte_buffer = format_instruction_bytes(inst);

					if ((line2() + static_cast<qreal>(byte_buffer.size()) * fontWidth_) > line3()) {
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

/**
 * @brief Handles the mouse release event in the disassembly view, resetting the state of line
 * movement and address selection, and updating the cursor to the default arrow cursor.
 *
 * @param event The mouse event containing information about the release action.
 */
void QDisassemblyView::mouseReleaseEvent(QMouseEvent * /*event*/) {

	movingLine1_      = false;
	movingLine2_      = false;
	movingLine3_      = false;
	movingLine4_      = false;
	selectingAddress_ = false;

	setCursor(Qt::ArrowCursor);
	update();
}

/**
 * @brief Updates the selected address in the disassembly view based on the position of a mouse event,
 * setting the selected address to the address corresponding to the point where the event occurred.
 *
 * @param event The mouse event containing information about the position where the address selection should be updated.
 */
void QDisassemblyView::updateSelectedAddress(QMouseEvent *event) {

	if (region_) {
		setSelectedAddress(addressFromPoint(event_position(event)));
	}
}

/**
 * @brief Handles the mouse press event in the disassembly view, allowing users to select addresses or move
 * vertical divider lines based on the position of the mouse click and the button pressed.
 *
 * @param event The mouse event containing information about the press action.
 */
void QDisassemblyView::mousePressEvent(QMouseEvent *event) {

	if (region_) {
		const qreal event_x = event_position(event).x() - line0();

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

/**
 * @brief Handles the mouse move event in the disassembly view, allowing users to move vertical divider lines or update the selected
 * address based on the position of the mouse cursor and whether a line is being moved or an address is being selected.
 *
 * @param event The mouse event containing information about the position of the cursor.
 */
void QDisassemblyView::mouseMoveEvent(QMouseEvent *event) {

	if (region_) {
		const qreal x_pos = event_position(event).x() - line0();

		if (movingLine1_) {
			if (qFuzzyIsNull(line2_)) {
				line2_ = line2();
			}
			const qreal min_line1 = 0.0;
			const qreal max_line1 = line2() - fontWidth_;
			line1_                = std::clamp(x_pos, min_line1, max_line1);
			update();
		} else if (movingLine2_) {
			if (qFuzzyIsNull(line3_)) {
				line3_ = line3();
			}
			const qreal min_line2 = line1() + iconWidth_;
			const qreal max_line2 = line3() - fontWidth_;
			line2_                = std::clamp(x_pos, min_line2, max_line2);
			update();
		} else if (movingLine3_) {
			if (qFuzzyIsNull(line4_)) {
				line4_ = line4();
			}
			const qreal min_line3 = line2() + fontWidth_ + fontWidth_ / 2.0;
			const qreal max_line3 = line4() - fontWidth_;
			line3_                = std::clamp(x_pos, min_line3, max_line3);
			update();
		} else if (movingLine4_) {
			const qreal min_line4 = line3() + fontWidth_;
			const qreal max_line4 = width() - 1.0 - (verticalScrollBar()->width() + 3.0);
			line4_                = std::clamp(x_pos, min_line4, max_line4);
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

/**
 * @brief Returns the currently selected address in the disassembly view.
 *
 * @return The currently selected address.
 */
edb::address_t QDisassemblyView::selectedAddress() const {
	return selectedInstructionAddress_;
}

/**
 * @brief Sets the selected address in the disassembly view to the specified address, updating the instruction size and history accordingly.
 *
 * @param address The address to set as the selected address.
 */
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

/**
 * @brief Returns the size of the currently selected instruction in the disassembly view.
 *
 * @return The size of the currently selected instruction.
 */
int QDisassemblyView::selectedSize() const {
	return selectedInstructionSize_;
}

/**
 * @brief Returns the current region being displayed in the disassembly view.
 *
 * @return The current region being displayed in the disassembly view.
 */
std::shared_ptr<IRegion> QDisassemblyView::region() const {
	return region_;
}

/**
 * @brief Adds a comment to the comment hash and persists it in the session manager.
 *
 * @param address The address to associate the comment with.
 * @param comment The comment text to add.
 */
void QDisassemblyView::addComment(edb::address_t address, QString comment) {
	Comment temp_comment = {
		address,
		comment};
	SessionManager::instance().addComment(temp_comment);
	comments_.insert(address, comment);
}

/**
 * @brief Removes a comment associated with the given address from the comment hash and the session manager.
 *
 * @param address The address of the comment to remove.
 * @return The number of comments removed (0 if no comment was found for the address, 1 if a comment was successfully removed).
 */
int QDisassemblyView::removeComment(edb::address_t address) {
	SessionManager::instance().removeComment(address);
	return comments_.remove(address);
}

/**
 * @brief Retrieves the comment associated with the specified address.
 *
 * @param address The address for which to retrieve the comment.
 * @return The comment string associated with the address, or an empty string if no comment exists for that address.
 */
QString QDisassemblyView::getComment(edb::address_t address) const {
	return comments_.value(address, QStringLiteral(""));
}

/**
 * @brief Clears all comments from the comment hash and the session manager.
 */
void QDisassemblyView::clearComments() {
	comments_.clear();
}

/**
 * @brief Saves the current state of the disassembly view, including the positions of vertical divider lines, into a QByteArray for later restoration.
 *
 * @return A QByteArray containing the serialized state of the disassembly view.
 */
QByteArray QDisassemblyView::saveState() const {

	const WidgetStateV2 state = {
		static_cast<int>(sizeof(WidgetStateV2)),
		line1_,
		line2_,
		line3_,
		line4_,
	};

	char buf[sizeof(WidgetStateV2)];
	memcpy(buf, &state, sizeof(buf));

	return QByteArray(buf, sizeof(buf));
}

/**
 * @brief Restores the state of the disassembly view from a QByteArray, updating the positions of vertical divider lines based on the serialized state.
 *
 * @param stateBuffer The QByteArray containing the serialized state of the disassembly view.
 */
void QDisassemblyView::restoreState(const QByteArray &stateBuffer) {
	if (stateBuffer.size() < static_cast<int>(sizeof(int))) {
		return;
	}

	int version = 0;
	memcpy(&version, stateBuffer.data(), sizeof(version));

	if (version >= static_cast<int>(sizeof(WidgetStateV2)) && stateBuffer.size() >= static_cast<int>(sizeof(WidgetStateV2))) {
		WidgetStateV2 state;
		memcpy(&state, stateBuffer.data(), sizeof(WidgetStateV2));
		line1_ = state.line1;
		line2_ = state.line2;
		line3_ = state.line3;
		line4_ = state.line4;
	} else if (version >= static_cast<int>(sizeof(WidgetStateV1)) && stateBuffer.size() >= static_cast<int>(sizeof(WidgetStateV1))) {
		WidgetStateV1 state;
		memcpy(&state, stateBuffer.data(), sizeof(WidgetStateV1));
		line1_ = state.line1;
		line2_ = state.line2;
		line3_ = state.line3;
		line4_ = state.line4;
	}
}
/**
 * @brief Restores comments from a QVariantList containing comment data, inserting them into the comment hash based on their associated addresses.
 *
 * @param comments_data A QVariantList containing comment data, where each entry is a QVariantMap with "address" and "comment" keys.
 */
void QDisassemblyView::restoreComments(QVariantList &comments_data) {
	for (const QVariant &entry : comments_data) {
		QVariantMap data = entry.toMap();
		if (const Result<edb::address_t, QString> addr = edb::v1::string_to_address(data[QStringLiteral("address")].toString())) {
			comments_.insert(*addr, data[QStringLiteral("comment")].toString());
		}
	}
}
