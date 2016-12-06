#ifndef DIALOG_EDIT_FPU_H_20151031
#define DIALOG_EDIT_FPU_H_20151031

#include <QDialog>
#include <QLineEdit>
#include "Register.h"

class Float80Edit;

class DialogEditFPU : public QDialog
{
	Q_OBJECT

	static_assert(sizeof(long double)>=10,"This class will only work with true 80-bit long double");
	Register reg;

	edb::value80 value_;
	Float80Edit* const floatEntry;
	QLineEdit* const hexEntry;
public:
	DialogEditFPU(QWidget* parent=nullptr);
	Register value() const;
	void set_value(const Register& reg);
private Q_SLOTS:
    void onHexEdited(const QString&);
    void onFloatEdited(const QString&);
	void updateFloatEntry();
	void updateHexEntry();
};

#endif
