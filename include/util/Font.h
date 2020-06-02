
#ifndef FONT_H_
#define FONT_H_

#include "API.h"

class QString;
class QFont;
class QFontMetrics;
class QChar;

namespace Font {

enum Type {
	Plain  = 0,
	Italic = 1,
	Bold   = 2,
};

EDB_EXPORT QFont fromString(const QString &fontName);
EDB_EXPORT int maxWidth(const QFontMetrics &fm);
EDB_EXPORT int characterWidth(const QFontMetrics &fm, QChar ch);
EDB_EXPORT int stringWidth(const QFontMetrics &fm, const QString &s);

}

#endif
