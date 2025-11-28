/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef FPU_VALUE_FIELD_H_20191119_
#define FPU_VALUE_FIELD_H_20191119_

#include "ValueField.h"

namespace ODbgRegisterView {

#if defined(EDB_X86) || defined(EDB_X86_64)
class FpuValueField final : public ValueField {
	Q_OBJECT

private:
	int showAsRawActionIndex_;
	int showAsFloatActionIndex_;

	FieldWidget *commentWidget_;
	int row_;
	int column_;

	QPersistentModelIndex tagValueIndex_;

	bool groupDigits_ = false;

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
