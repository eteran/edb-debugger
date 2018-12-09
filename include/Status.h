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
#include <boost/variant.hpp>


class EDB_EXPORT Status {
public:
	enum OkType { Ok };
public:
	Status(OkType) {
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
    explicit operator bool() const { return success(); }
	QString toString() const       { return errorMessage_; }

private:
	QString errorMessage_;
};

#if 0
template <class T>
class EDB_EXPORT Result {
public:
	Result() : status_("Failure") {
	}

	Result(const QString &message, const T &value) : status_(message), value_(value) {
	}

	explicit Result(const T &value) : status_(Status::Ok), value_(value) {
	}

	Result(const Result &)            = default;
	Result& operator=(const Result &) = default;
	Result(Result &&)                 = default;
	Result& operator=(Result &&)      = default;

public:
	T operator*() const            { Q_ASSERT(succeeded()); return value_; }
	bool succeeded() const         { return status_.success(); }
	bool failed() const            { return !succeeded(); }
    explicit operator bool() const { return succeeded(); }
	QString errorMessage() const   { return status_.toString(); }
	T value() const                { Q_ASSERT(succeeded()); return value_; }

private:
	Status status_;
	T      value_;
};
#endif

template <class E>
class EDB_EXPORT Unexpected {
	template<class U, class Y>
	friend class Expected;

	template <class U>
	friend Unexpected<U> make_unexpected(U &&);

public:
	Unexpected(const Unexpected &)            = default;
	Unexpected& operator=(const Unexpected &) = default;
	Unexpected(Unexpected &&)                 = default;
	Unexpected& operator=(Unexpected &&)      = default;

private:
	template <class U>
	Unexpected(U &&error) : error_(std::forward<U>(error)) {
	}

private:
	E error_;
};

template <class T, class E>
class EDB_EXPORT Result {
public:
	template <class U>
	Result(U &&value) : value_(std::forward<U>(value)) {
	}

	Result(const Unexpected<E> &value) : value_(value) {
	}

	Result(Unexpected<E> &&value) : value_(std::move(value)) {
	}

	Result(const Result &)            = default;
	Result& operator=(const Result &) = default;
	Result(Result &&)                 = default;
	Result& operator=(Result &&)      = default;

public:
	T operator*() const            { return value(); }
	bool succeeded() const         { return value_.which() == 0; }
	bool failed() const            { return value_.which() == 1; }
	explicit operator bool() const { return succeeded(); }
	bool operator!() const         { return failed(); }
	E error() const                { Q_ASSERT(failed());    return boost::get<Unexpected<E>>(value_).error_; }
	T value() const                { Q_ASSERT(succeeded()); return boost::get<T>(value_); }

private:
	boost::variant<T, Unexpected<E>> value_;
};

template <class E>
Unexpected<E> make_unexpected(E &&error) {
	return Unexpected<E>(std::forward<E>(error));
}


#endif
