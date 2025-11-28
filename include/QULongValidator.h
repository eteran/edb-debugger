/*
 * Copyright (C) 2006 - 2025 Evan Teran <evan.teran@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef QULONG_VALIDATOR_H_20071128_
#define QULONG_VALIDATOR_H_20071128_

#include "API.h"
#include <QValidator>
#include <cstdint>

class EDB_EXPORT QULongValidator : public QValidator {
	Q_OBJECT
public:
	using value_type = std::uint64_t;

public:
	explicit QULongValidator(QObject *parent = nullptr);
	QULongValidator(value_type minimum, value_type maximum, QObject *parent = nullptr);
	~QULongValidator() override = default;

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
