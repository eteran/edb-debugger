
#include "NumberEdit.h"
#include "util/Font.h"
#include <QApplication>

namespace ODbgRegisterView {

NumberEdit::NumberEdit(int column, int colSpan, QWidget *parent)
	: QLineEdit(parent), column_(column), colSpan_(colSpan) {
}

int NumberEdit::column() const {
	return column_;
}

int NumberEdit::colSpan() const {
	return colSpan_;
}

void NumberEdit::setNaturalWidthInChars(int nChars) {
	naturalWidthInChars_ = nChars;
}

QSize NumberEdit::minimumSizeHint() const {
	return sizeHint();
}

QSize NumberEdit::sizeHint() const {

	const auto baseHint = QLineEdit::sizeHint();
	// taking long enough reference char to make enough room even in presence of inner shadows like in Oxygen style
	const auto charWidth       = Font::maxWidth(QFontMetrics(font()));
	const auto textMargins     = this->textMargins();
	const auto contentsMargins = this->contentsMargins();
	int customWidth            = charWidth * naturalWidthInChars_ + textMargins.left() + contentsMargins.left() + textMargins.right() + contentsMargins.right();

	return QSize(customWidth, baseHint.height()).expandedTo(QApplication::globalStrut());
}

}
