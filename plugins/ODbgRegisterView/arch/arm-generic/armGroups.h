/*
 * Copyright (C) 2016 Ruslan Kabatsayev <b7.10110111@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef ODBG_REGISTER_VIEW_ARM_GROUPS_H_20170819_
#define ODBG_REGISTER_VIEW_ARM_GROUPS_H_20170819_

#include "RegisterView.h"

namespace ODbgRegisterView {

RegisterGroup *createCPSR(RegisterViewModelBase::Model *model, QWidget *parent);
RegisterGroup *createExpandedCPSR(RegisterViewModelBase::Model *model, QWidget *parent);
RegisterGroup *createFPSCR(RegisterViewModelBase::Model *model, QWidget *parent);

}

#endif
