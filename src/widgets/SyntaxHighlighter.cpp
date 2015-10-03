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

#include "SyntaxHighlighter.h"
#include <QSettings>

//------------------------------------------------------------------------------
// Name: SyntaxHighlighter
// Desc:
//------------------------------------------------------------------------------
SyntaxHighlighter::SyntaxHighlighter(QObject *parent) : QSyntaxHighlighter(parent) {
	create_rules();
}

//------------------------------------------------------------------------------
// Name: SyntaxHighlighter
// Desc:
//------------------------------------------------------------------------------
SyntaxHighlighter::SyntaxHighlighter(QTextDocument *parent) : QSyntaxHighlighter(parent) {
	create_rules();
}

#if QT_VERSION < 0x050000
//------------------------------------------------------------------------------
// Name: SyntaxHighlighter
// Desc:
//------------------------------------------------------------------------------
SyntaxHighlighter::SyntaxHighlighter(QTextEdit *parent) : QSyntaxHighlighter(parent) {
	create_rules();
}
#endif

//------------------------------------------------------------------------------
// Name: HighlightingRule::HighlightingRule
// Desc:
//------------------------------------------------------------------------------
SyntaxHighlighter::HighlightingRule::HighlightingRule() {

}

//------------------------------------------------------------------------------
// Name: HighlightingRule::HighlightingRule
// Desc:
//------------------------------------------------------------------------------
SyntaxHighlighter::HighlightingRule::HighlightingRule(const QString &regex, const QBrush &foreground, const QBrush &background, int weight, bool italic, bool underline) : pattern(regex) {
	pattern.setCaseSensitivity(Qt::CaseInsensitive);

	format.setForeground(foreground);
	format.setBackground(background);
	format.setFontWeight(weight);
	format.setFontItalic(italic);
	format.setFontUnderline(underline);
}

//------------------------------------------------------------------------------
// Name: create_rules
// Desc:
//------------------------------------------------------------------------------
void SyntaxHighlighter::create_rules() {

	// TODO: make these rules be implemented in a portable way
	// right now things are very much hard coded
	
	// TODO: support segments

	QSettings settings;
	settings.beginGroup("Theme");

	// comma
	rules_.append(HighlightingRule(
		"(,)",
		QColor(settings.value("theme.brackets.foreground", "blue").value<QString>()),
		QColor(settings.value("theme.brackets.background", "transparent").value<QString>()),
		settings.value("theme.brackets.weight", QFont::Normal).value<int>(),
		settings.value("theme.brackets.italic", false).value<bool>(),
		settings.value("theme.brackets.underline", false).value<bool>()
		));

	// expression brackets
	rules_.append(HighlightingRule(
		"([\\(\\)\\[\\]])",
		QColor(settings.value("theme.brackets.foreground", "blue").value<QString>()),
		QColor(settings.value("theme.brackets.background", "transparent").value<QString>()),
		settings.value("theme.brackets.weight", QFont::Normal).value<int>(),
		settings.value("theme.brackets.italic", false).value<bool>(),
		settings.value("theme.brackets.underline", false).value<bool>()
		));

	// math operators
	rules_.append(HighlightingRule(
		"\\b([\\+\\-\\*])\\b",
		QColor(settings.value("theme.operator.foreground", "blue").value<QString>()),
		QColor(settings.value("theme.operator.background", "transparent").value<QString>()),
		settings.value("theme.operator.weight", QFont::Normal).value<int>(),
		settings.value("theme.operator.italic", false).value<bool>(),
		settings.value("theme.operator.underline", false).value<bool>()
		));

	// registers
	// TODO: support ST(N)
	rules_.append(HighlightingRule(
		"\\b(((e|r)?(ax|bx|cx|dx|bp|sp|si|di|ip))|([abcd](l|h))|(sp|bp|si|di)l|([cdefgs]s)|[xyz]?mm([0-9]|[12][0-9]|3[01])|r(8|9|(1[0-5]))[dwb]?)\\b",		
		QColor(settings.value("theme.register.foreground", "red").value<QString>()),
		QColor(settings.value("theme.register.background", "transparent").value<QString>()),
		settings.value("theme.register.weight", QFont::Bold).value<int>(),
		settings.value("theme.register.italic", false).value<bool>(),
		settings.value("theme.register.underline", false).value<bool>()
		));

	// constants
	rules_.append(HighlightingRule(
		"\\b((0[0-7]*)|(0(x|X)[0-9a-fA-F]+)|([1-9][0-9]*))\\b",
		QColor(settings.value("theme.constant.foreground", "black").value<QString>()),
		QColor(settings.value("theme.constant.background", "transparent").value<QString>()),
		settings.value("theme.constant.weight", QFont::Normal).value<int>(),
		settings.value("theme.constant.italic", false).value<bool>(),
		settings.value("theme.constant.underline", false).value<bool>()
		));

	// pointer modifiers
	rules_.append(HighlightingRule(
		"\\b(t?byte|([xyz]mm|[qdf]?)word) ptr\\b",
		QColor(settings.value("theme.ptr.foreground", "darkGreen").value<QString>()),
		QColor(settings.value("theme.ptr.background", "transparent").value<QString>()),
		settings.value("theme.ptr.weight", QFont::Normal).value<int>(),
		settings.value("theme.ptr.italic", false).value<bool>(),
		settings.value("theme.ptr.underline", false).value<bool>()
		));

	// prefix
	rules_.append(HighlightingRule(
		"\\b(lock|rep(ne)?)\\b",
		QColor(settings.value("theme.prefix.foreground", "black").value<QString>()),
		QColor(settings.value("theme.prefix.background", "transparent").value<QString>()),
		settings.value("theme.prefix.weight", QFont::Bold).value<int>(),
		settings.value("theme.prefix.italic", false).value<bool>(),
		settings.value("theme.prefix.underline", false).value<bool>()
		));


	// flow control
	rules_.append(HighlightingRule(
		"\\b(l?jmp[bswlqt]?|loopn?[ez]|(jn?(a|ae|b|be|c|e|g|ge|l|le|o|p|s|z)|j(pe|po|cxz|ecxz)))\\b",
		QColor(settings.value("theme.flow_ctrl.foreground", "blue").value<QString>()),
		QColor(settings.value("theme.flow_ctrl.background", "yellow").value<QString>()),
		settings.value("theme.flow_ctrl.weight", QFont::Normal).value<int>(),
		settings.value("theme.flow_ctrl.italic", false).value<bool>(),
		settings.value("theme.flow_ctrl.underline", false).value<bool>()
		));
		

	// function call
	rules_.append(HighlightingRule(
		"\\b(call|ret[nf]?)[bswlqt]?\\b",
		QColor(settings.value("theme.function.foreground", "blue").value<QString>()),
		QColor(settings.value("theme.function.background", "yellow").value<QString>()),
		settings.value("theme.function.weight", QFont::Normal).value<int>(),
		settings.value("theme.function.italic", false).value<bool>(),
		settings.value("theme.function.underline", false).value<bool>()
		));

	// stack operations
	rules_.append(HighlightingRule(
		"\\b(pushf?|popf?|enter|leave)\\b",
		QColor(settings.value("theme.stack.foreground", "blue").value<QString>()),
		QColor(settings.value("theme.stack.background", "transparent").value<QString>()),
		settings.value("theme.stack.weight", QFont::Normal).value<int>(),
		settings.value("theme.stack.italic", false).value<bool>(),
		settings.value("theme.stack.underline", false).value<bool>()
		));

	// comparison
	rules_.append(HighlightingRule(
		"\\b(cmp|test)[bswlqt]?\\b",
		QColor(settings.value("theme.comparison.foreground", "blue").value<QString>()),
		QColor(settings.value("theme.comparison.background", "transparent").value<QString>()),
		settings.value("theme.comparison.weight", QFont::Normal).value<int>(),
		settings.value("theme.comparison.italic", false).value<bool>(),
		settings.value("theme.comparison.underline", false).value<bool>()
		));


	// data transfer
	rules_.append(HighlightingRule(
		"\\b(c?movs[bw]|lea|xchg|mov([zs]x?)?)[bswlqt]?\\b",
		QColor(settings.value("theme.data_xfer.foreground", "blue").value<QString>()),
		QColor(settings.value("theme.data_xfer.background", "transparent").value<QString>()),
		settings.value("theme.data_xfer.weight", QFont::Normal).value<int>(),
		settings.value("theme.data_xfer.italic", false).value<bool>(),
		settings.value("theme.data_xfer.underline", false).value<bool>()
		));

	// arithmetic
	rules_.append(HighlightingRule(
		"\\b(add|sub|i?mul|i?div|neg|adc|sbb|inc|dec)[bswlqt]?\\b",
		QColor(settings.value("theme.arithmetic.foreground", "blue").value<QString>()),
		QColor(settings.value("theme.arithmetic.background", "transparent").value<QString>()),
		settings.value("theme.arithmetic.weight", QFont::Normal).value<int>(),
		settings.value("theme.arithmetic.italic", false).value<bool>(),
		settings.value("theme.arithmetic.underline", false).value<bool>()
		));

	// logic
	rules_.append(HighlightingRule(
		"\\b(and|x?or|not)[bswlqt]?\\b",
		QColor(settings.value("theme.logic.foreground", "blue").value<QString>()),
		QColor(settings.value("theme.logic.background", "transparent").value<QString>()),
		settings.value("theme.logic.weight", QFont::Normal).value<int>(),
		settings.value("theme.logic.italic", false).value<bool>(),
		settings.value("theme.logic.underline", false).value<bool>()
		));

	// shift
	rules_.append(HighlightingRule(
		"\\b(sh|sa|sc|ro)[rl][bswlqt]?\\b",
		QColor(settings.value("theme.shift.foreground", "blue").value<QString>()),
		QColor(settings.value("theme.shift.background", "transparent").value<QString>()),
		settings.value("theme.shift.weight", QFont::Normal).value<int>(),
		settings.value("theme.shift.italic", false).value<bool>(),
		settings.value("theme.shift.underline", false).value<bool>()
		));

	// system
	rules_.append(HighlightingRule(
		"\\b(sti|cli|hlt|in|out|sysenter|sysexit|syscall|sysret|int)\\b",
		QColor(settings.value("theme.system.foreground", "blue").value<QString>()),
		QColor(settings.value("theme.system.background", "transparent").value<QString>()),
		settings.value("theme.system.weight", QFont::Bold).value<int>(),
		settings.value("theme.system.italic", false).value<bool>(),
		settings.value("theme.system.underline", false).value<bool>()
		));
}

//------------------------------------------------------------------------------
// Name: highlightBlock
// Desc:
//------------------------------------------------------------------------------
void SyntaxHighlighter::highlightBlock(const QString &text) {
	for(const HighlightingRule &rule: rules_) {
		int index = rule.pattern.indexIn(text);
		while(index >= 0) {
			const int length = rule.pattern.matchedLength();
			setFormat(index, length, rule.format);
			index = rule.pattern.indexIn(text, index + length);
		}
	}
}
