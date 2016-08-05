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

#ifndef STATUS_20160803_H_
#define STATUS_20160803_H_

#include "API.h"
#include <QString>

class EDB_EXPORT Status {
public:
	Status() {
	}
	
	explicit Status(const QString &message) : errorMessage_(message) {
	}
	
	Status(const Status &)            = default;
	Status& operator=(const Status &) = default;
	Status(Status &&)                 = default;
	Status& operator=(Status &&)      = default;
	
public:
	bool success() const           { return errorMessage_.isEmpty(); }
	bool failure() const           { return !success(); }
	explicit operator bool() const { return success(); };
	QString toString() const       { return errorMessage_; }
	
private:
	QString errorMessage_;
};

template <class T>
class EDB_EXPORT Result {
public:
	Result() {
	}
	
	Result(const QString &message, const T &value) : status_(message), value_(value) {
	}

	explicit Result(const T &value) : value_(value) {
	}
	
	Result(const Result &)            = default;
	Result& operator=(const Result &) = default;
	Result(Result &&)                 = default;
	Result& operator=(Result &&)      = default;
	
public:
	T operator*() const            { Q_ASSERT(succeeded()); return value_; }
	bool succeeded() const         { return status_.success(); }
	bool failed() const            { return !succeeded(); }
	explicit operator bool() const { return succeeded(); };
	QString errorMessage() const   { return status_.toString(); }
	T value() const                { Q_ASSERT(succeeded()); return value_; }
	
private:
	Status status_;
	T      value_;
};

#endif
