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

#ifndef STATUS_H_20160803_
#define STATUS_H_20160803_

#include "API.h"
#include <QString>
#include <boost/variant.hpp>
#include <type_traits>

class EDB_EXPORT Status {
public:
	enum OkType { Ok };

public:
	Status(OkType) {
	}

	explicit Status(const QString &message)
		: error_(message) {
	}

	Status(const Status &) = default;
	Status &operator=(const Status &) = default;
	Status(Status &&)                 = default;
	Status &operator=(Status &&) = default;

public:
	bool success() const { return error_.isEmpty(); }
	bool failure() const { return !success(); }
	explicit operator bool() const { return success(); }
	QString error() const { return error_; }

private:
	QString error_;
};

template <class E>
class Unexpected {
	template <class U, class Y>
	friend class Expected;

	template <class U, class Y>
	friend class Result;

	template <class U>
	friend Unexpected<typename std::decay<U>::type> make_unexpected(U &&);

public:
	Unexpected(const Unexpected &) = default;
	Unexpected &operator=(const Unexpected &) = default;
	Unexpected(Unexpected &&)                 = default;
	Unexpected &operator=(Unexpected &&) = default;

private:
	template <class U>
	Unexpected(U &&error)
		: error_(std::forward<U>(error)) {
	}

private:
	E error_;
};

template <class T, class E>
class Result {
public:
	template <class U, class = typename std::enable_if<std::is_convertible<U, T>::value>::type>
	Result(U &&value)
		: value_(std::forward<U>(value)) {
	}

	Result(const Unexpected<E> &value)
		: value_(value) {
	}

	Result(Unexpected<E> &&value)
		: value_(std::move(value)) {
	}

	Result(const Result &) = default;
	Result &operator=(const Result &) = default;
	Result(Result &&)                 = default;
	Result &operator=(Result &&) = default;

public:
	const T *operator->() const {
		Q_ASSERT(succeeded());
		return &boost::get<T>(value_);
	}

	const T &operator*() const { return value(); }
	bool succeeded() const { return value_.which() == 0; }
	bool failed() const { return value_.which() == 1; }
	explicit operator bool() const { return succeeded(); }
	bool operator!() const { return failed(); }

	const E &error() const {
		Q_ASSERT(failed());
		return boost::get<Unexpected<E>>(value_).error_;
	}

	const T &value() const {
		Q_ASSERT(succeeded());
		return boost::get<T>(value_);
	}

private:
	boost::variant<T, Unexpected<E>> value_;
};

template <class E>
class Result<void, E> {
public:
	Result() {
	}

	Result(const Unexpected<E> &value)
		: value_(value) {
	}

	Result(Unexpected<E> &&value)
		: value_(std::move(value)) {
	}

	Result(const Result &) = default;
	Result &operator=(const Result &) = default;
	Result(Result &&)                 = default;
	Result &operator=(Result &&) = default;

public:
	bool succeeded() const { return value_.which() == 0; }
	bool failed() const { return value_.which() == 1; }
	explicit operator bool() const { return succeeded(); }
	bool operator!() const { return failed(); }

	const E &error() const {
		Q_ASSERT(failed());
		return boost::get<Unexpected<E>>(value_).error_;
	}

private:
	boost::variant<boost::blank, Unexpected<E>> value_;
};

template <class E>
Unexpected<typename std::decay<E>::type> make_unexpected(E &&e) {
	return Unexpected<typename std::decay<E>::type>(std::forward<E>(e));
}

#endif
