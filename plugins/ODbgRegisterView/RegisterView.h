/*
Copyright (C) 2015 Ruslan Kabatsayev <b7.10110111@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef ODBG_REGISTER_VIEW_H_20151230_
#define ODBG_REGISTER_VIEW_H_20151230_

#include "RegisterViewModelBase.h"

#include <QHBoxLayout>
#include <QPersistentModelIndex>
#include <QScrollArea>
#include <QString>

#include <memory>

namespace ODbgRegisterView {

class DialogEditSimdRegister;
class DialogEditGPR;
class DialogEditFPU;
class RegisterGroup;
class ValueField;
class FieldWidget;

class ODBRegView : public QScrollArea {
	Q_OBJECT

public:
	enum RegisterGroupType : int {
#if defined(EDB_X86) || defined(EDB_X86_64)
		GPR,
		rIP,
		ExpandedEFL,
		Segment,
		EFL,
		FPUData,
		FPUWords,
		FPULastOp,
		Debug,
		MMX,
		SSEData,
		AVXData,
		MXCSR,
#elif defined(EDB_ARM32)
		GPR,
		CPSR,
		ExpandedCPSR,
		FPSCR,
#else
#error "Not implemented"
#endif
		NUM_GROUPS
	};

public:
	explicit ODBRegView(const QString &settings, QWidget *parent = nullptr);
	void setModel(RegisterViewModelBase::Model *model);
	QList<ValueField *> valueFields() const;
	QList<FieldWidget *> fields() const;
	void showMenu(const QPoint &position, const QList<QAction *> &additionalItems = {}) const;
	void saveState(const QString &settings) const;
	void groupHidden(RegisterGroup *group);
	DialogEditGPR *gprEditDialog() const;
	DialogEditSimdRegister *simdEditDialog() const;
	DialogEditFPU *fpuEditDialog() const;
	void selectAField();

private:
	RegisterGroup *makeGroup(RegisterGroupType type);

private:
	ValueField *selectedField() const;
	void updateFieldsPalette();
	void keyPressEvent(QKeyEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void updateFont();

public Q_SLOTS:
	void fieldSelected();

private Q_SLOTS:
	void modelReset();
	void modelUpdated();
	void copyAllRegisters();
	void copyRegisterToClipboard() const;
	void settingsUpdated();

private:
	RegisterViewModelBase::Model *model_ = nullptr;
	QList<RegisterGroup *> groups_;
	std::unique_ptr<QHBoxLayout> flagsAndSegments_;
	std::vector<RegisterGroupType> visibleGroupTypes_;
	QList<QAction *> menuItems_;
	DialogEditGPR *dialogEditGpr_;
	DialogEditSimdRegister *dialogEditSIMDReg_;
	DialogEditFPU *dialogEditFpu_;
};

}

#endif
