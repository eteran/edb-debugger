/*
Copyright (C) 2006 - 2011 Evan Teran
                          eteran@alum.rit.edu

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

#ifndef FUNCTIONINFO_20070320_H_
#define FUNCTIONINFO_20070320_H_

#include "API.h"
#include <QChar>
#include <QVector>

class EDB_EXPORT FunctionInfo {
public:
	FunctionInfo(const FunctionInfo &other) : params_(other.params_) {
	}

	FunctionInfo &operator=(const FunctionInfo &rhs) {
		params_ = rhs.params_;
		return *this;
	}

	FunctionInfo() {
	}

	explicit FunctionInfo(QChar param1) {
		params_.push_back(param1);
	}

	FunctionInfo(QChar param1, QChar param2) {
		params_.push_back(param1);
		params_.push_back(param2);
	}

	FunctionInfo(QChar param1, QChar param2, QChar param3) {
		params_.push_back(param1);
		params_.push_back(param2);
		params_.push_back(param3);
	}

	FunctionInfo(QChar param1, QChar param2, QChar param3, QChar param4) {
		params_.push_back(param1);
		params_.push_back(param2);
		params_.push_back(param3);
		params_.push_back(param4);
	}

	FunctionInfo(QChar param1, QChar param2, QChar param3, QChar param4, QChar param5) {
		params_.push_back(param1);
		params_.push_back(param2);
		params_.push_back(param3);
		params_.push_back(param4);
		params_.push_back(param5);
	}

	FunctionInfo(QChar param1, QChar param2, QChar param3, QChar param4, QChar param5, QChar param6) {
		params_.push_back(param1);
		params_.push_back(param2);
		params_.push_back(param3);
		params_.push_back(param4);
		params_.push_back(param5);
		params_.push_back(param6);
	}

	FunctionInfo(QChar param1, QChar param2, QChar param3, QChar param4, QChar param5, QChar param6, QChar param7) {
		params_.push_back(param1);
		params_.push_back(param2);
		params_.push_back(param3);
		params_.push_back(param4);
		params_.push_back(param5);
		params_.push_back(param6);
		params_.push_back(param7);
	}

	~FunctionInfo() {}

public:
	const QVector<QChar> &params() const { return params_; }

private:
	QVector<QChar> params_;
};

#endif

