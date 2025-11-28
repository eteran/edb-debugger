/*
 * Copyright (C) 2014 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SESSION_ERROR_H_20170929_
#define SESSION_ERROR_H_20170929_

#include <QString>

class SessionError {
public:
	enum ErrorCode {
		NoError            = 0,
		UnknownError       = 1,
		NotAnObject        = 2,
		InvalidSessionFile = 3,
	};

public:
	ErrorCode err = NoError;
	QString message;
};

#endif
