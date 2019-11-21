
#ifndef FPU_VALUE_FIELD_H_20191119_
#define FPU_VALUE_FIELD_H_20191119_

#include "ValueField.h"

namespace ODbgRegisterView {

#if defined(EDB_X86) || defined(EDB_X86_64)
class FpuValueField final : public ValueField {
	Q_OBJECT

private:
	int showAsRawActionIndex;
	int showAsFloatActionIndex;

	FieldWidget *commentWidget;
	int row;
	int column;

	QPersistentModelIndex tagValueIndex;

	bool groupDigits = false;

public:
	// Will add itself and commentWidget to the group and renew their positions as needed
	FpuValueField(int fieldWidth, const QModelIndex &regValueIndex, const QModelIndex &tagValueIndex, RegisterGroup *group, FieldWidget *commentWidget, int row, int column);

public Q_SLOTS:
	void showFPUAsRaw();
	void showFPUAsFloat();
	void displayFormatChanged();
	void updatePalette() override;
};
#endif

}

#endif
