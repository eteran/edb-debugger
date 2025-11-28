/*
 * Copyright (C) 2020- 2025 Evan Teran
 *                          evan.teran@gmail.com
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef THEME_H_
#define THEME_H_

#include <QColor>
#include <QTextCharFormat>

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
