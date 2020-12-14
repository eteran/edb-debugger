/*
Copyright (C) 2020 - 2020 Evan Teran
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

#ifndef THEME_H_
#define THEME_H_

#include <QTextCharFormat>
#include <QColor>

struct Theme {

	enum Palette {
		Window,
		WindowText,
		Base,
		AlternateBase,
		ToolTipBase,
		ToolTipText,
		Text,
		Button,
		ButtonText,
		BrightText,
		Highlight,
		HighlightedText,
		Link,
		LinkVisited,

		Light,
		Midlight,
		Dark,
		Mid,
		Shadow,

		WindowDisabled,
		WindowTextDisabled,
		BaseDisabled,
		AlternateBaseDisabled,
		ToolTipBaseDisabled,
		ToolTipTextDisabled,
		TextDisabled,
		ButtonDisabled,
		ButtonTextDisabled,
		BrightTextDisabled,
		HighlightDisabled,
		HighlightedTextDisabled,
		LinkDisabled,
		LinkVisitedDisabled,

		LightDisabled,
		MidlightDisabled,
		DarkDisabled,
		MidDisabled,
		ShadowDisabled,

		PaletteCount
	};

	QColor palette[PaletteCount];
	
	enum Text {
		Address,
		AlternatingByte,
		Arithmetic,
		Brackets,
		Comma,
		Comparison,
		Data,
		Constant,
		DataXfer,
		FlowCtrl,
		Function,
		Logic,
		NonPrintingCharacter,
		Operator,
		Prefix,
		Ptr,
		Register,
		Shift,
		Stack,
		System,
		Filling,
		TakenJump,

		TextCount
	};
	
	QTextCharFormat text[TextCount];
	
	enum Misc {
		Badge,

		MiscCount
	};
	
	QTextCharFormat misc[MiscCount];
	
public:
	static Theme load();
	static QStringList userThemes();
	static QString themeName(const QString &theme_file);
};

#endif
