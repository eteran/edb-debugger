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
	value_type bottom() const;
	value_type top() const;
	QValidator::State validate(QString &input, int &pos) const override;
	void setRange(value_type bottom, value_type top);
	void setBottom(value_type bottom);
	void setTop(value_type top);

private:
	value_type minimum_ = 0;
	value_type maximum_ = 0;
};

#endif
