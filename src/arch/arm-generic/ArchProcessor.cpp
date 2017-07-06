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

#include "ArchProcessor.h"
#include "Configuration.h"
#include "Util.h"

#include <QMenu>


ArchProcessor::ArchProcessor() {
}

QStringList ArchProcessor::update_instruction_info(edb::address_t address) {
	Q_UNUSED(address);
	QStringList ret;
	return ret;
}

bool ArchProcessor::can_step_over(const edb::Instruction &inst) const {
	Q_UNUSED(inst);
	return false;
}

bool ArchProcessor::is_filling(const edb::Instruction &inst) const {
	Q_UNUSED(inst);
	return false;
}

void ArchProcessor::reset() {
}

void ArchProcessor::about_to_resume() {
}

void ArchProcessor::setup_register_view() {
}

void ArchProcessor::update_register_view(const QString &default_region_name, const State &state) {
	Q_UNUSED(default_region_name);
	Q_UNUSED(state);
}

std::unique_ptr<QMenu> ArchProcessor::register_item_context_menu(const Register& reg) {
	Q_UNUSED(reg);
	return util::make_unique<QMenu>(nullptr);
}

RegisterViewModelBase::Model& ArchProcessor::get_register_view_model() const {
	static RegisterViewModelBase::Model ret;
	return ret;
}

void ArchProcessor::justAttached() {
}
