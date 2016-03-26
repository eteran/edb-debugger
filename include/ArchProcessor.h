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

#ifndef ARCHPROCESSOR_20070312_H_
#define ARCHPROCESSOR_20070312_H_

#include "State.h"
#include "Types.h"
#include <QCoreApplication>
#include <QStringList>
#include <QObject>
#include <vector>
#include "RegisterViewModelBase.h"

class QMenu;
class QByteArray;
class QString;
class RegisterListWidget;
class State;

class ArchProcessor : public QObject {
	Q_OBJECT
	Q_DISABLE_COPY(ArchProcessor)
public:
	ArchProcessor();
	virtual ~ArchProcessor() {}

public:
	QStringList update_instruction_info(edb::address_t address);
	bool can_step_over(const edb::Instruction &inst) const;
	bool is_filling(const edb::Instruction &inst) const;
	void reset();
	void about_to_resume();
	void setup_register_view();
	void update_register_view(const QString &default_region_name, const State &state);
	std::unique_ptr<QMenu> register_item_context_menu(const Register& reg);
	RegisterViewModelBase::Model& get_register_view_model() const;

private:
	enum class FPUOrderMode {
		Stack, // ST(i)
		Independent // Ri
	};
private:
	void setupFPURegisterMenu(QMenu& menu);
	void setupMMXRegisterMenu(QMenu& menu);
	void setupSSEAVXRegisterMenu(QMenu& menu, const QString& extType);
	void update_fpu_view(int& itemNumber, const State &state, const QPalette& palette) const;
	QString getRoundingMode(unsigned modeBits) const;

private:
	bool just_attached_=true;

	bool has_mmx_;
	bool has_xmm_;
	bool has_ymm_;

private Q_SLOTS:
	void just_attached();
};

#endif

