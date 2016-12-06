#include "Float80Edit.h"
#include "FloatX.h"
#include <QValidator>
#include <QApplication>

Float80Edit::Float80Edit(QWidget* parent)
	: QLineEdit(parent)
{
	setValidator(new FloatXValidator<long double>(this));
}

void Float80Edit::setValue(edb::value80 input)
{
	setText(formatFloat(input));
}
QSize Float80Edit::sizeHint() const
{
	const auto baseHint=QLineEdit::sizeHint();
	// Default size hint gives space for about 15-20 chars. We need about 30.
	return QSize(baseHint.width()*2,baseHint.height()).expandedTo(QApplication::globalStrut());
}
void Float80Edit::focusOutEvent(QFocusEvent* e)
{
	QLineEdit::focusOutEvent(e);
	Q_EMIT defocussed();
}

