/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "util/Font.h"
#include <QFont>
#include <QFontMetrics>

namespace Font {

QFont fromString(const QString &fontName) {
	QFont font;
	font.fromString(fontName);
	font.setStyleName(QString());
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	QT_WARNING_PUSH
	QT_WARNING_DISABLE_DEPRECATED
	font.setStyleStrategy(QFont::ForceIntegerMetrics);
	QT_WARNING_POP
#else
	font.setHintingPreference(QFont::PreferFullHinting);
	font.setStyleStrategy(QFont::NoFontMerging);
#endif
	return font;
}

int maxWidth(const QFontMetrics &fm) {
	return Font::characterWidth(fm, QLatin1Char('X'));
}

int characterWidth(const QFontMetrics &fm, QChar ch) {
	return fm.horizontalAdvance(ch);
}

int stringWidth(const QFontMetrics &fm, const QString &s) {
	return fm.horizontalAdvance(s);
}

}
