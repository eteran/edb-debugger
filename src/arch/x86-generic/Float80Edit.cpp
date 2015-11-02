#include "Float80Edit.h"
#include <QValidator>
#include <QApplication>

class Float80Validator : public QValidator
{
	static_assert(sizeof(long double)>=10,"This class will only work with true 80-bit long double");
public:
	explicit Float80Validator(QObject* parent=0)
		: QValidator(parent)
	{
	}
	virtual QValidator::State validate(QString& input, int&) const override
	{
		if(input.isEmpty()) return QValidator::Intermediate;
		std::istringstream str(input.toStdString());
		long double value;
		str >> std::noskipws >> value;
		if(str)
		{
			// Check that no trailing chars are left
			char c;
			str >> c;
			if(!str) return QValidator::Acceptable;
		}
		// OK, so we failed to read it or it is unfinished. Let's check whether it's intermediate or invalid.
		QRegExp basicFormat("[+-]?[0-9]*\\.?[0-9]*(e([+-]?[0-9]*)?)?");
		QRegExp specialFormat("[+-]?[sq]?nan|[+-]?inf",Qt::CaseInsensitive);
		QRegExp specialFormatUnfinished("[+-]?[sq]?(n(an?)?)?|[+-]?(i(nf?)?)?",Qt::CaseInsensitive);

		if(basicFormat.exactMatch(input))
			return QValidator::Intermediate;
		if(specialFormat.exactMatch(input))
			return QValidator::Acceptable;
		if(specialFormatUnfinished.exactMatch(input))
			return QValidator::Intermediate;

		// All possible options are exhausted, so consider the input invalid
		return QValidator::Invalid;
	}
};

Float80Edit::Float80Edit(QWidget* parent)
	: QLineEdit(parent)
{
	setValidator(new Float80Validator(this));
}
void Float80Edit::setValue(edb::value80 input)
{
	const auto value=input.toFloatValue();
	if(!std::isnan(value) && !std::isinf(value))
		setText(input.toString());
	else
		setText(input.floatTypeString(input.floatType()));
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

