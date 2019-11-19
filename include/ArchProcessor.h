/*
Copyright (C) 2006 - 2015 Evan Teran
                          evan.teran@gmail.com

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

#ifndef ARCH_PROCESSOR_H_20070312_
#define ARCH_PROCESSOR_H_20070312_

#include "API.h"
#include "RegisterViewModelBase.h"
#include "Status.h"
#include "Types.h"
#include <QObject>

class QByteArray;
class QMenu;
class QString;
class QStringList;
class State;

class EDB_EXPORT ArchProcessor : public QObject {
	Q_OBJECT

public:
	ArchProcessor();
	ArchProcessor(const ArchProcessor &) = delete;
	ArchProcessor &operator=(const ArchProcessor &) = delete;
	~ArchProcessor() override                       = default;

public:
	QStringList updateInstructionInfo(edb::address_t address);
	bool canStepOver(const edb::Instruction &inst) const;
	bool isFilling(const edb::Instruction &inst) const;
	//! Checks whether potentially conditional instruction's condition is satisfied
	bool isExecuted(const edb::Instruction &inst, const State &state) const;
	Result<edb::address_t, QString> getEffectiveAddress(const edb::Instruction &inst, const edb::Operand &op, const State &state) const;
	edb::address_t getEffectiveAddress(const edb::Instruction &inst, const edb::Operand &op, const State &state, bool &ok) const;
	void reset();
	void aboutToResume();
	void setupRegisterView();
	void updateRegisterView(const QString &default_region_name, const State &state);
	RegisterViewModelBase::Model &registerViewModel() const;

private:
	bool justAttached_ = true;
	bool hasMmx_;
	bool hasXmm_;
	bool hasYmm_;

private Q_SLOTS:
	void justAttached();
};

#endif
