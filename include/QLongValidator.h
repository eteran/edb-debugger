/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef QLONG_VALIDATOR_H_20071128_
#define QLONG_VALIDATOR_H_20071128_

#include "API.h"
#include <QValidator>
#include <cstdint>

class EDB_EXPORT QLongValidator : public QValidator {
	Q_OBJECT
public:
	using value_type = std::int64_t;

public:
	explicit QLongValidator(QObject *parent = nullptr);
	QLongValidator(value_type minimum, value_type maximum, QObject *parent = nullptr);
	~QLongValidator() override = default;

public:
	[[nodiscard]] value_type bottom() const;
	[[nodiscard]] value_type top() const;
	QValidator::State validate(QString &input, int &pos) const override;
	void setRange(value_type bottom, value_type top);
	void setBottom(value_type bottom);
	void setTop(value_type top);

private:
	value_type minimum_ = 0;
	value_type maximum_ = 0;
};

#endif
