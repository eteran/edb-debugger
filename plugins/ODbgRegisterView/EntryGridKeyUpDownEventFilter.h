/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef ENTRY_GRID_KEY_UP_DOWN_EVENT_FILTER_H_20170705_
#define ENTRY_GRID_KEY_UP_DOWN_EVENT_FILTER_H_20170705_

class QWidget;
class QObject;
class QEvent;

namespace ODbgRegisterView {

bool entry_grid_key_event_filter(QWidget *parent, QObject *obj, QEvent *event);

}

#endif
