
#ifndef THEME_H_
#define THEME_H_

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
};

#endif
