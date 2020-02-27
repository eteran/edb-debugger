
#ifndef FONT_H_
#define FONT_H_

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

QFont fromString(const QString &fontName);
int maxWidth(const QFontMetrics &fm);
int characterWidth(const QFontMetrics &fm, QChar ch);
int stringWidth(const QFontMetrics &fm, const QString &s);

}

#endif
