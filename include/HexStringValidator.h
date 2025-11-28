/*
 * Copyright (C) 2013 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HEX_STRING_VALIDATOR_H_20130625_
#define HEX_STRING_VALIDATOR_H_20130625_

#include <QValidator>

class QString;
class QObject;

class HexStringValidator final : public QValidator {
	Q_OBJECT
public:
	HexStringValidator(QObject *parent);

public:
	void fixup(QString &input) const override;
	State validate(QString &input, int &pos) const override;
};

#endif
