/*
Copyright (C) 2006 - 2015 Evan Teran
                          evan.teran@gmail.com
Copyright (C) 2017 - 2017 Ruslan Kabatsayev
                          b7.10110111@gmail.com

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

#include "ArchProcessor.h"
#include "Configuration.h"
#include "RegisterViewModel.h"
#include "State.h"
#include "Util.h"
#include "edb.h"

#include <QMenu>

namespace {
static constexpr size_t GPR_COUNT=16;
}

RegisterViewModel& getModel() {
	return static_cast<RegisterViewModel&>(edb::v1::arch_processor().get_register_view_model());
}

ArchProcessor::ArchProcessor() {
	if(edb::v1::debugger_core) {
		connect(edb::v1::debugger_ui, SIGNAL(attachEvent()), this, SLOT(justAttached()));
	}
}

QStringList ArchProcessor::update_instruction_info(edb::address_t address) {
	Q_UNUSED(address);
	QStringList ret;
	return ret;
}

bool ArchProcessor::can_step_over(const edb::Instruction &inst) const {
	return inst && (is_call(inst) || is_interrupt(inst) || !modifies_pc(inst));
}

bool ArchProcessor::is_filling(const edb::Instruction &inst) const {
	Q_UNUSED(inst);
	return false;
}

void ArchProcessor::reset() {
}

void ArchProcessor::about_to_resume() {
	getModel().saveValues();
}

void ArchProcessor::setup_register_view() {

	if(edb::v1::debugger_core) {

		update_register_view(QString(), State());
	}
}

QString pcComment(Register const& reg, QString const& default_region_name) {
	const auto symname=edb::v1::find_function_symbol(reg.valueAsAddress(), default_region_name);
	return symname.isEmpty() ? symname : '<'+symname+'>';
}

QString gprComment(Register const& reg) {

	QString regString;
	int stringLength;
	QString comment;
	if(edb::v1::get_ascii_string_at_address(reg.valueAsAddress(), regString, edb::v1::config().min_string_length, 256, stringLength))
		comment=QString("ASCII \"%1\"").arg(regString);
	else if(edb::v1::get_utf16_string_at_address(reg.valueAsAddress(), regString, edb::v1::config().min_string_length, 256, stringLength))
		comment=QString("UTF16 \"%1\"").arg(regString);
	return comment;
}

void updateGPRs(RegisterViewModel& model, State const& state, QString const& default_region_name) {
	for(std::size_t i=0;i<GPR_COUNT;++i) {
		const auto reg=state.gp_register(i);
		Q_ASSERT(!!reg); Q_ASSERT(reg.bitSize()==32);
		QString comment;
		if(i!=15)
			comment=gprComment(reg);
		else
			comment=pcComment(reg,default_region_name);
		model.updateGPR(i,reg.value<edb::value32>(),comment);
	}
}

bool is_jcc_taken(const edb::reg_t cpsr, edb::Instruction::ConditionCode cond) {
	const bool N = (cpsr & 0x80000000) != 0;
	const bool Z = (cpsr & 0x40000000) != 0;
	const bool C = (cpsr & 0x20000000) != 0;
	const bool V = (cpsr & 0x10000000) != 0;

	bool taken = false;
	switch(cond & 0xe) {
	case 0x0:
		taken = Z;
		break;
	case 0x2:
		taken = C;
		break;
	case 0x4:
		taken = N;
		break;
	case 0x6:
		taken = V;
		break;
	case 0x8:
		taken = C && !Z;
		break;
	case 0xa:
		taken = N == V;
		break;
	case 0xc:
		taken = !Z && (N==V);
		break;
	case 0xe:
		taken = true;
		break;
	}

	if(cond & 1)
		taken = !taken;

	return taken;
}

static QLatin1String jumpConditionMnemonics[] = {
	QLatin1String("EQ"),  QLatin1String("NE"),
	QLatin1String("HS"),  QLatin1String("LO"),
	QLatin1String("MI"),  QLatin1String("PL"),
	QLatin1String("VS"),  QLatin1String("VC"),
	QLatin1String("HI"),  QLatin1String("LS"),
	QLatin1String("GE"),  QLatin1String("LT"),
	QLatin1String("GT"),  QLatin1String("LE"),
	QLatin1String("AL"),  QLatin1String("??"),
};

QString cpsrComment(edb::reg_t flags) {
	QString comment="(";
	for(int cond=0;cond<0x10-2;++cond) // we're not interested in AL or UNDEFINED conditions
		if(is_jcc_taken(flags,static_cast<edb::Instruction::ConditionCode>(cond)))
			comment+=jumpConditionMnemonics[cond]+',';
	comment[comment.size()-1]=')';
	return comment;
}

void updateCPSR(RegisterViewModel& model, State const& state) {
	const auto flags=state.flags_register();
	Q_ASSERT(!!flags);
	const auto comment=cpsrComment(flags.valueAsInteger());
	model.updateCPSR(flags.value<edb::value32>(),comment);
}

void ArchProcessor::update_register_view(const QString &default_region_name, const State &state) {

	auto& model = getModel();
	updateGPRs(model,state,default_region_name);
	updateCPSR(model,state);

	if(just_attached_) {
		model.saveValues();
		just_attached_=false;
	}
	model.dataUpdateFinished();
}

std::unique_ptr<QMenu> ArchProcessor::register_item_context_menu(const Register& reg) {
	Q_UNUSED(reg);
	return util::make_unique<QMenu>(nullptr);
}

RegisterViewModelBase::Model& ArchProcessor::get_register_view_model() const {
	static RegisterViewModel model(0);
	return model;
}

void ArchProcessor::justAttached() {
	just_attached_=true;
}
