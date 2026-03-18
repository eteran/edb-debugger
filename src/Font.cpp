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
	// NOTE(eteran): unfortunately, this line seems to matter
	// despite being marked as deprecated, and Qt doesn't suggest
	// a meaningful alternative
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
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
	return fm.horizontalAdvance(ch);
#else
	return fm.width(ch);
#endif
}

int stringWidth(const QFontMetrics &fm, const QString &s) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
	return fm.horizontalAdvance(s);
#else
	return fm.width(s);
#endif
}

}
