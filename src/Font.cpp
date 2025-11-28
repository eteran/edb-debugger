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
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
	font.setStyleStrategy(QFont::ForceIntegerMetrics);
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
