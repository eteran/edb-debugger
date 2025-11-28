/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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
	ArchProcessor(const ArchProcessor &)            = delete;
	ArchProcessor &operator=(const ArchProcessor &) = delete;
	~ArchProcessor() override                       = default;

public:
	QStringList updateInstructionInfo(edb::address_t address);
	[[nodiscard]] bool canStepOver(const edb::Instruction &inst) const;
	[[nodiscard]] bool isFilling(const edb::Instruction &inst) const;
	//! Checks whether potentially conditional instruction's condition is satisfied
	[[nodiscard]] bool isExecuted(const edb::Instruction &inst, const State &state) const;
	[[nodiscard]] Result<edb::address_t, QString> getEffectiveAddress(const edb::Instruction &inst, const edb::Operand &op, const State &state) const;
	[[nodiscard]] edb::address_t getEffectiveAddress(const edb::Instruction &inst, const edb::Operand &op, const State &state, bool &ok) const;
	void reset();
	void aboutToResume();
	void setupRegisterView();
	void updateRegisterView(const QString &default_region_name, const State &state);
	[[nodiscard]] RegisterViewModelBase::Model &registerViewModel() const;

private:
	bool justAttached_ = true;
	bool hasMmx_;
	bool hasXmm_;
	bool hasYmm_;

private Q_SLOTS:
	void justAttached();
};

#endif
