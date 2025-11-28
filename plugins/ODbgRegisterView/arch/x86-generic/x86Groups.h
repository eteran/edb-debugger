/*
 * Copyright (C) 2015 Ruslan Kabatsayev <b7.10110111@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef ODBG_REGISTER_VIEW_X86_GROUPS_H_20170817_
#define ODBG_REGISTER_VIEW_X86_GROUPS_H_20170817_

#include "RegisterView.h"

namespace ODbgRegisterView {

RegisterGroup *create_eflags(RegisterViewModelBase::Model *model, QWidget *parent);
RegisterGroup *create_expanded_eflags(RegisterViewModelBase::Model *model, QWidget *parent);
RegisterGroup *create_fpu_data(RegisterViewModelBase::Model *model, QWidget *parent);
RegisterGroup *create_fpu_words(RegisterViewModelBase::Model *model, QWidget *parent);
RegisterGroup *create_fpu_last_op(RegisterViewModelBase::Model *model, QWidget *parent);
RegisterGroup *create_debug_group(RegisterViewModelBase::Model *model, QWidget *parent);
RegisterGroup *create_mxcsr(RegisterViewModelBase::Model *model, QWidget *parent);

}

#endif
