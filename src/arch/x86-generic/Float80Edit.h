#ifndef FLOAT_80_EDIT_H_20151031
#define FLOAT_80_EDIT_H_20151031

#include <QLineEdit>
#include "Types.h"

class Float80Edit : public QLineEdit
{
	Q_OBJECT
public:
	Float80Edit(QWidget* parent=0);
	void setValue(edb::value80 input);
	QSize sizeHint() const override;
protected:
	void focusOutEvent(QFocusEvent* e) override;
Q_SIGNALS:
	void defocussed();
};

#endif
