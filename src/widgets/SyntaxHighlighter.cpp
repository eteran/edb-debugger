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
#include "Theme.h"

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
 * @param regex
 * @param format
 */
SyntaxHighlighter::HighlightingRule::HighlightingRule(const QString &regex, const QTextCharFormat &fmt)
	: pattern(regex) {

	pattern.setCaseSensitivity(Qt::CaseInsensitive);
	format = fmt;
}

/**
 * @brief SyntaxHighlighter::createRules
 */
void SyntaxHighlighter::createRules() {

	Theme theme = Theme::load();

	// TODO: make these rules be implemented in a portable way
	// right now things are very much hard coded

	// TODO: support segments

	// comma
	rules_.emplace_back(
		"(?:,)",
		theme.text[Theme::Comma]);

	// expression brackets
	rules_.emplace_back(
		"(?:[\\(?:\\)\\[\\]])",
		theme.text[Theme::Brackets]);

	// math operators
	rules_.emplace_back(
		"\\b(?:[\\+\\-\\*])\\b",
		theme.text[Theme::Operator]);

	// registers
	// TODO: support ST(N)
	rules_.emplace_back(
#if defined(EDB_X86) || defined(EDB_X86_64)
		"\\b(?:(?:(?:e|r)?(?:ax|bx|cx|dx|bp|sp|si|di|ip))|(?:[abcd](?:l|h))|(?:sp|bp|si|di)l|(?:[cdefgs]s)|[xyz]?mm(?:[0-9]|[12][0-9]|3[01])|r(?:8|9|(?:1[0-5]))[dwb]?)\\b",
#elif defined(EDB_ARM32)
		"\\b(?:r(?:[0-9]|1[0-5])|sb|sl|fp|ip|sp|lr|pc|[sd][0-9]|[sdf](?:[12][0-9]|3[01])|q(?:[0-9]|1[0-5]))\\b",
#elif defined(EDB_ARM64)
		"\\b(?:[xw](?:[12]?[0-9]|3[01]))\\b" /* FIXME: stub, only GPRs here */,
#else
#error "What string should be here?"
#endif
		theme.text[Theme::Register]);

	// constants
	rules_.emplace_back(
#if defined(EDB_ARM32) || defined(EDB_ARM64)
		"#?" /* concatenated with general number pattern */
#endif
		"\\b(?:(?:0[0-7]*)|(?:0(?:x|X)[0-9a-fA-F]+)|(?:[1-9][0-9]*))\\b",
		theme.text[Theme::Constant]);

#if defined(EDB_X86) || defined(EDB_X86_64)
	// pointer modifiers
	rules_.emplace_back(
		"\\b(?:t?byte|(?:[xyz]mm|[qdf]?)word)(?: ptr)?\\b",
		theme.text[Theme::Ptr]);

	// prefix
	rules_.emplace_back(
		"\\b(?:lock|rep(?:ne)?)\\b",
		theme.text[Theme::Prefix]);
#endif

	// flow control
	rules_.emplace_back(
#if defined(EDB_X86) || defined(EDB_X86_64)
		"\\b(?:l?jmp[bswlqt]?|loopn?[ez]|(?:jn?(?:a|ae|b|be|c|e|g|ge|l|le|o|p|s|z)|j(?:pe|po|cxz|ecxz)))\\b",
#elif defined(EDB_ARM32) || defined(EDB_ARM64)
		/* FIXME(ARM): there are also instructions like `add pc, pc, #5`, which
		 *             should also be considered flow control */
		"\\b(?:b(?:x|xj)?(?:eq|ne|cs|hs|cc|lo|mi|pl|vs|vc|hi|ls|ge|lt|gt|le)?)\\b",
#else
#error "What string should be here?"
#endif
		theme.text[Theme::FlowCtrl]);

	// function call
	rules_.emplace_back(
#if defined(EDB_X86) || defined(EDB_X86_64)
		"\\b(?:call|ret[nf]?)[bswlqt]?\\b",
#elif defined(EDB_ARM32) || defined(EDB_ARM64)
		"\\b(?:b(?:l|lx)(?:eq|ne|cs|hs|cc|lo|mi|pl|vs|vc|hi|ls|ge|lt|gt|le)?)\\b",
#else
#error "What string should be here?"
#endif
		theme.text[Theme::Function]);

#if defined(EDB_X86) || defined(EDB_X86_64)
	// FIXME(ARM): this is stubbed out

	// stack operations
	rules_.emplace_back(
		"\\b(?:pushf?|popf?|enter|leave)\\b",
		theme.text[Theme::Stack]);

	// comparison
	rules_.emplace_back(
		"\\b(?:cmp|test)[bswlqt]?\\b",
		theme.text[Theme::Comparison]);

	// data transfer
	rules_.emplace_back(
		"\\b(?:c?movs[bw]|lea|xchg|mov(?:[zs]x?)?)[bswlqt]?\\b",
		theme.text[Theme::DataXfer]);

	// arithmetic
	rules_.emplace_back(
		"\\b(?:add|sub|i?mul|i?div|neg|adc|sbb|inc|dec)[bswlqt]?\\b",
		theme.text[Theme::Arithmetic]);

	// logic
	rules_.emplace_back(
		"\\b(?:and|x?or|not)[bswlqt]?\\b",
		theme.text[Theme::Logic]);

	// shift
	rules_.emplace_back(
		"\\b(?:sh|sa|sc|ro)[rl][bswlqt]?\\b",
		theme.text[Theme::Shift]);

	// system
	rules_.emplace_back(
		"\\b(?:sti|cli|hlt|in|out|sysenter|sysexit|syscall|sysret|int)\\b",
		theme.text[Theme::System]);
#endif

	// data bytes
	rules_.emplace_back(
		"\\b(?:db|dw|dd|dq)\\b",
		theme.text[Theme::Data]);
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
