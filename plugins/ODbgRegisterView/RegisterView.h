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

class QMenu;
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

public:
	[[nodiscard]] DialogEditFPU *fpuEditDialog() const;
	[[nodiscard]] DialogEditGPR *gprEditDialog() const;
	[[nodiscard]] DialogEditSimdRegister *simdEditDialog() const;
	[[nodiscard]] QList<FieldWidget *> fields() const;
	[[nodiscard]] QList<ValueField *> valueFields() const;
	void groupHidden(RegisterGroup *group);
	void saveState(const QString &settings) const;
	void selectAField() const;
	void setModel(RegisterViewModelBase::Model *model);
	void showMenu(const QPoint &position, const QList<QAction *> &additionalItems = {}) const;

private:
	[[nodiscard]] RegisterGroup *makeGroup(RegisterGroupType type);
	void restoreHiddenGroup(RegisterGroupType type);

private:
	[[nodiscard]] ValueField *selectedField() const;
	void updateFieldsPalette() const;
	void keyPressEvent(QKeyEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void updateFont();

public Q_SLOTS:
	void fieldSelected();

private Q_SLOTS:
	void modelReset();
	void modelUpdated();
	void copyAllRegisters() const;
	void copyRegisterToClipboard() const;
	void settingsUpdated();

private:
	RegisterViewModelBase::Model *model_ = nullptr;
	QList<RegisterGroup *> groups_;
	std::unique_ptr<QHBoxLayout> flagsAndSegments_;
	std::vector<RegisterGroupType> visibleGroupTypes_;
	QList<QAction *> menuItems_;
	QMenu *hiddenGroupsMenu_;
	QAction *hiddenGroupsAction_;
	DialogEditGPR *dialogEditGpr_;
	DialogEditSimdRegister *dialogEditSIMDReg_;
	DialogEditFPU *dialogEditFpu_;
};

}

#endif
