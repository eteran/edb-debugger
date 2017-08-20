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

#ifndef ODBG_REGISTER_VIEW_X86_GROUPS_H_20170817
#define ODBG_REGISTER_VIEW_X86_GROUPS_H_20170817

#include "RegisterView.h"

namespace ODbgRegisterView {

RegisterGroup *createEFL(RegisterViewModelBase::Model *model, QWidget *parent);
RegisterGroup *createExpandedEFL(RegisterViewModelBase::Model *model, QWidget *parent);
RegisterGroup *createFPUData(RegisterViewModelBase::Model *model, QWidget *parent);
RegisterGroup *createFPUWords(RegisterViewModelBase::Model *model, QWidget *parent);
RegisterGroup *createFPULastOp(RegisterViewModelBase::Model *model, QWidget *parent);
RegisterGroup *createDebugGroup(RegisterViewModelBase::Model *model, QWidget *parent);
RegisterGroup *createMXCSR(RegisterViewModelBase::Model *model, QWidget *parent);

}

#endif
