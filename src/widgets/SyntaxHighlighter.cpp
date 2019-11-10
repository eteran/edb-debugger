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

#include "SyntaxHighlighter.h"
#include <QSettings>

namespace {

/**
 * @brief create_rule
 * @param foreground
 * @param background
 * @param weight
 * @param italic
 * @param underline
 * @return
 */
QTextCharFormat create_rule(const QBrush &foreground, const QBrush &background, int weight, bool italic, bool underline) {
	QTextCharFormat format;
	format.setForeground(foreground);
	format.setBackground(background);
	format.setFontWeight(weight);
	format.setFontItalic(italic);
	format.setFontUnderline(underline);
	return format;
}

}

/**
 * @brief SyntaxHighlighter::SyntaxHighlighter
 * @param parent
 */
SyntaxHighlighter::SyntaxHighlighter(QObject *parent)
	: QObject(parent) {

	createRules();
}

/**
 * @brief SyntaxHighlighter::HighlightingRule::HighlightingRule
 */
SyntaxHighlighter::HighlightingRule::HighlightingRule() {
}

/**
 * @brief SyntaxHighlighter::HighlightingRule::HighlightingRule
 * @param regex
 * @param foreground
 * @param background
 * @param weight
 * @param italic
 * @param underline
 */
SyntaxHighlighter::HighlightingRule::HighlightingRule(const QString &regex, const QBrush &foreground, const QBrush &background, int weight, bool italic, bool underline)
	: pattern(regex) {

	pattern.setCaseSensitivity(Qt::CaseInsensitive);
	format = create_rule(foreground, background, weight, italic, underline);
}

/**
 * @brief SyntaxHighlighter::createRules
 */
void SyntaxHighlighter::createRules() {

	// TODO: make these rules be implemented in a portable way
	// right now things are very much hard coded

	// TODO: support segments

	QSettings settings;
	settings.beginGroup("Theme");

	// comma
	rules_.emplace_back(HighlightingRule(
		"(?:,)",
		QColor(settings.value("theme.brackets.foreground", "blue").toString()),
		QColor(settings.value("theme.brackets.background", "transparent").toString()),
		settings.value("theme.brackets.weight", QFont::Normal).toInt(),
		settings.value("theme.brackets.italic", false).toBool(),
		settings.value("theme.brackets.underline", false).toBool()));

	// expression brackets
	rules_.emplace_back(HighlightingRule(
		"(?:[\\(?:\\)\\[\\]])",
		QColor(settings.value("theme.brackets.foreground", "blue").toString()),
		QColor(settings.value("theme.brackets.background", "transparent").toString()),
		settings.value("theme.brackets.weight", QFont::Normal).toInt(),
		settings.value("theme.brackets.italic", false).toBool(),
		settings.value("theme.brackets.underline", false).toBool()));

	// math operators
	rules_.emplace_back(HighlightingRule(
		"\\b(?:[\\+\\-\\*])\\b",
		QColor(settings.value("theme.operator.foreground", "blue").toString()),
		QColor(settings.value("theme.operator.background", "transparent").toString()),
		settings.value("theme.operator.weight", QFont::Normal).toInt(),
		settings.value("theme.operator.italic", false).toBool(),
		settings.value("theme.operator.underline", false).toBool()));

	// registers
	// TODO: support ST(N)
	rules_.emplace_back(HighlightingRule(
#if defined EDB_X86 || defined EDB_X86_64
		"\\b(?:(?:(?:e|r)?(?:ax|bx|cx|dx|bp|sp|si|di|ip))|(?:[abcd](?:l|h))|(?:sp|bp|si|di)l|(?:[cdefgs]s)|[xyz]?mm(?:[0-9]|[12][0-9]|3[01])|r(?:8|9|(?:1[0-5]))[dwb]?)\\b",
#elif defined EDB_ARM32
		"\\b(?:r(?:[0-9]|1[0-5])|sb|sl|fp|ip|sp|lr|pc|[sd][0-9]|[sdf](?:[12][0-9]|3[01])|q(?:[0-9]|1[0-5]))\\b",
#elif defined EDB_ARM64
		"\\b(?:[xw](?:[12]?[0-9]|3[01]))\\b" /* FIXME: stub, only GPRs here */,
#else
#error "What string should be here?"
#endif
		QColor(settings.value("theme.register.foreground", "red").toString()),
		QColor(settings.value("theme.register.background", "transparent").toString()),
		settings.value("theme.register.weight", QFont::Bold).toInt(),
		settings.value("theme.register.italic", false).toBool(),
		settings.value("theme.register.underline", false).toBool()));

	// constants
	rules_.emplace_back(HighlightingRule(
#if defined EDB_ARM32 || defined EDB_ARM64
		"#?" /* concatenated with general number pattern */
#endif
		"\\b(?:(?:0[0-7]*)|(?:0(?:x|X)[0-9a-fA-F]+)|(?:[1-9][0-9]*))\\b",
		QColor(settings.value("theme.constant.foreground", "black").toString()),
		QColor(settings.value("theme.constant.background", "transparent").toString()),
		settings.value("theme.constant.weight", QFont::Normal).toInt(),
		settings.value("theme.constant.italic", false).toBool(),
		settings.value("theme.constant.underline", false).toBool()));

#if defined EDB_X86 || defined EDB_X86_64
	// pointer modifiers
	rules_.emplace_back(HighlightingRule(
		"\\b(?:t?byte|(?:[xyz]mm|[qdf]?)word)(?: ptr)?\\b",
		QColor(settings.value("theme.ptr.foreground", "darkGreen").toString()),
		QColor(settings.value("theme.ptr.background", "transparent").toString()),
		settings.value("theme.ptr.weight", QFont::Normal).toInt(),
		settings.value("theme.ptr.italic", false).toBool(),
		settings.value("theme.ptr.underline", false).toBool()));

	// prefix
	rules_.emplace_back(HighlightingRule(
		"\\b(?:lock|rep(?:ne)?)\\b",
		QColor(settings.value("theme.prefix.foreground", "black").toString()),
		QColor(settings.value("theme.prefix.background", "transparent").toString()),
		settings.value("theme.prefix.weight", QFont::Bold).toInt(),
		settings.value("theme.prefix.italic", false).toBool(),
		settings.value("theme.prefix.underline", false).toBool()));
#endif

	// flow control
	rules_.emplace_back(HighlightingRule(
#if defined EDB_X86 || defined EDB_X86_64
		"\\b(?:l?jmp[bswlqt]?|loopn?[ez]|(?:jn?(?:a|ae|b|be|c|e|g|ge|l|le|o|p|s|z)|j(?:pe|po|cxz|ecxz)))\\b",
#elif defined EDB_ARM32 || defined EDB_ARM64
		/* FIXME(ARM): there are also instructions like `add pc, pc, #5`, which
		 *             should also be considered flow control */
		"\\b(?:b(?:x|xj)?(?:eq|ne|cs|hs|cc|lo|mi|pl|vs|vc|hi|ls|ge|lt|gt|le)?)\\b",
#else
#error "What string should be here?"
#endif
		QColor(settings.value("theme.flow_ctrl.foreground", "blue").toString()),
		QColor(settings.value("theme.flow_ctrl.background", "yellow").toString()),
		settings.value("theme.flow_ctrl.weight", QFont::Normal).toInt(),
		settings.value("theme.flow_ctrl.italic", false).toBool(),
		settings.value("theme.flow_ctrl.underline", false).toBool()));

	// function call
	rules_.emplace_back(HighlightingRule(
#if defined EDB_X86 || defined EDB_X86_64
		"\\b(?:call|ret[nf]?)[bswlqt]?\\b",
#elif defined EDB_ARM32 || defined EDB_ARM64
		"\\b(?:b(?:l|lx)(?:eq|ne|cs|hs|cc|lo|mi|pl|vs|vc|hi|ls|ge|lt|gt|le)?)\\b",
#else
#error "What string should be here?"
#endif
		QColor(settings.value("theme.function.foreground", "blue").toString()),
		QColor(settings.value("theme.function.background", "yellow").toString()),
		settings.value("theme.function.weight", QFont::Normal).toInt(),
		settings.value("theme.function.italic", false).toBool(),
		settings.value("theme.function.underline", false).toBool()));

#if defined EDB_X86 || defined EDB_X86_64
	// FIXME(ARM): this is stubbed out

	// stack operations
	rules_.emplace_back(HighlightingRule(
		"\\b(?:pushf?|popf?|enter|leave)\\b",
		QColor(settings.value("theme.stack.foreground", "blue").toString()),
		QColor(settings.value("theme.stack.background", "transparent").toString()),
		settings.value("theme.stack.weight", QFont::Normal).toInt(),
		settings.value("theme.stack.italic", false).toBool(),
		settings.value("theme.stack.underline", false).toBool()));

	// comparison
	rules_.emplace_back(HighlightingRule(
		"\\b(?:cmp|test)[bswlqt]?\\b",
		QColor(settings.value("theme.comparison.foreground", "blue").toString()),
		QColor(settings.value("theme.comparison.background", "transparent").toString()),
		settings.value("theme.comparison.weight", QFont::Normal).toInt(),
		settings.value("theme.comparison.italic", false).toBool(),
		settings.value("theme.comparison.underline", false).toBool()));

	// data transfer
	rules_.emplace_back(HighlightingRule(
		"\\b(?:c?movs[bw]|lea|xchg|mov(?:[zs]x?)?)[bswlqt]?\\b",
		QColor(settings.value("theme.data_xfer.foreground", "blue").toString()),
		QColor(settings.value("theme.data_xfer.background", "transparent").toString()),
		settings.value("theme.data_xfer.weight", QFont::Normal).toInt(),
		settings.value("theme.data_xfer.italic", false).toBool(),
		settings.value("theme.data_xfer.underline", false).toBool()));

	// arithmetic
	rules_.emplace_back(HighlightingRule(
		"\\b(?:add|sub|i?mul|i?div|neg|adc|sbb|inc|dec)[bswlqt]?\\b",
		QColor(settings.value("theme.arithmetic.foreground", "blue").toString()),
		QColor(settings.value("theme.arithmetic.background", "transparent").toString()),
		settings.value("theme.arithmetic.weight", QFont::Normal).toInt(),
		settings.value("theme.arithmetic.italic", false).toBool(),
		settings.value("theme.arithmetic.underline", false).toBool()));

	// logic
	rules_.emplace_back(HighlightingRule(
		"\\b(?:and|x?or|not)[bswlqt]?\\b",
		QColor(settings.value("theme.logic.foreground", "blue").toString()),
		QColor(settings.value("theme.logic.background", "transparent").toString()),
		settings.value("theme.logic.weight", QFont::Normal).toInt(),
		settings.value("theme.logic.italic", false).toBool(),
		settings.value("theme.logic.underline", false).toBool()));

	// shift
	rules_.emplace_back(HighlightingRule(
		"\\b(?:sh|sa|sc|ro)[rl][bswlqt]?\\b",
		QColor(settings.value("theme.shift.foreground", "blue").toString()),
		QColor(settings.value("theme.shift.background", "transparent").toString()),
		settings.value("theme.shift.weight", QFont::Normal).toInt(),
		settings.value("theme.shift.italic", false).toBool(),
		settings.value("theme.shift.underline", false).toBool()));

	// system
	rules_.emplace_back(HighlightingRule(
		"\\b(?:sti|cli|hlt|in|out|sysenter|sysexit|syscall|sysret|int)\\b",
		QColor(settings.value("theme.system.foreground", "blue").toString()),
		QColor(settings.value("theme.system.background", "transparent").toString()),
		settings.value("theme.system.weight", QFont::Bold).toInt(),
		settings.value("theme.system.italic", false).toBool(),
		settings.value("theme.system.underline", false).toBool()));
#endif

	// data bytes
	rules_.emplace_back(HighlightingRule(
		"\\b(?:db|dw|dd|dq)\\b",
		QColor(settings.value("theme.data.foreground", "black").toString()),
		QColor(settings.value("theme.data.background", "transparent").toString()),
		settings.value("theme.data.weight", QFont::Normal).toInt(),
		settings.value("theme.data.italic", false).toBool(),
		settings.value("theme.data.underline", false).toBool()));
}

/**
 * @brief SyntaxHighlighter::highlightBlock
 * @param text
 * @return
 */
QVector<QTextLayout::FormatRange> SyntaxHighlighter::highlightBlock(const QString &text) {

	QVector<QTextLayout::FormatRange> ranges;

	for (const HighlightingRule &rule : rules_) {
		int index = rule.pattern.indexIn(text);
		while (index >= 0) {
			const int length = rule.pattern.matchedLength();

			QTextLayout::FormatRange range;

			range.format = rule.format;
			range.start  = index;
			range.length = length;

			ranges.push_back(range);

			index = rule.pattern.indexIn(text, index + length);
		}
	}

	return ranges;
}
