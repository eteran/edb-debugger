/*
Copyright (C) 2006 - 2023 Evan Teran
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
#include <type_traits>
#include <variant>

class EDB_EXPORT Status {
public:
	enum OkType { Ok };

public:
	Status(OkType) {
	}

	explicit Status(QString message)
		: error_(std::move(message)) {
	}

	Status(const Status &)            = default;
	Status &operator=(const Status &) = default;
	Status(Status &&)                 = default;
	Status &operator=(Status &&)      = default;

public:
	[[nodiscard]] bool failure() const { return !success(); }
	[[nodiscard]] bool success() const { return error_.isEmpty(); }
	[[nodiscard]] QString error() const { return error_; }
	[[nodiscard]] explicit operator bool() const { return success(); }

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
	Unexpected(const Unexpected &)            = default;
	Unexpected &operator=(const Unexpected &) = default;
	Unexpected(Unexpected &&)                 = default;
	Unexpected &operator=(Unexpected &&)      = default;

private:
	template <class U, class = typename std::enable_if<!std::is_same<U, Unexpected>::value && std::is_convertible<U, E>::value>::type>
	Unexpected(U &&error)
		: error_(std::forward<U>(error)) {
	}

private:
	E error_;
};

template <class T, class E>
class Result {
public:
	template <class U, class = typename std::enable_if<!std::is_same<U, Result>::value && std::is_convertible<U, T>::value>::type>
	Result(U &&value)
		: value_(std::forward<U>(value)) {
	}

	Result(const Unexpected<E> &value)
		: value_(value) {
	}

	Result(Unexpected<E> &&value)
		: value_(std::move(value)) {
	}

	Result(const Result &)            = default;
	Result &operator=(const Result &) = default;
	Result(Result &&)                 = default;
	Result &operator=(Result &&)      = default;

public:
	const T *operator->() const {
		Q_ASSERT(succeeded());
		return &std::get<T>(value_);
	}

	[[nodiscard]] bool failed() const { return value_.index() == 1; }
	[[nodiscard]] bool operator!() const { return failed(); }
	[[nodiscard]] bool succeeded() const { return value_.index() == 0; }
	[[nodiscard]] const T &operator*() const { return value(); }
	[[nodiscard]] explicit operator bool() const { return succeeded(); }

	[[nodiscard]] const E &error() const {
		Q_ASSERT(failed());
		return std::get<Unexpected<E>>(value_).error_;
	}

	[[nodiscard]] const T &value() const {
		Q_ASSERT(succeeded());
		return std::get<T>(value_);
	}

private:
	std::variant<T, Unexpected<E>> value_;
};

template <class E>
class Result<void, E> {
public:
	Result() = default;

	Result(const Unexpected<E> &value)
		: value_(value) {
	}

	Result(Unexpected<E> &&value)
		: value_(std::move(value)) {
	}

	Result(const Result &)            = default;
	Result &operator=(const Result &) = default;
	Result(Result &&)                 = default;
	Result &operator=(Result &&)      = default;

public:
	[[nodiscard]] bool failed() const { return value_.index() == 1; }
	[[nodiscard]] bool operator!() const { return failed(); }
	[[nodiscard]] bool succeeded() const { return value_.index() == 0; }
	[[nodiscard]] explicit operator bool() const { return succeeded(); }

	[[nodiscard]] const E &error() const {
		Q_ASSERT(failed());
		return std::get<Unexpected<E>>(value_).error_;
	}

private:
	std::variant<std::monostate, Unexpected<E>> value_;
};

template <class E>
Unexpected<typename std::decay<E>::type> make_unexpected(E &&e) {
	return Unexpected<typename std::decay<E>::type>(std::forward<E>(e));
}

#endif
