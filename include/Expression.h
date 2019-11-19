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

#ifndef EXPRESSION_H_20070402_
#define EXPRESSION_H_20070402_

#include "Status.h"
#include <QString>
#include <functional>

struct ExpressionError {
public:
	enum ErrorMessage {
		None,
		Syntax,
		UnbalancedParens,
		UnbalancedBraces,
		DivideByZero,
		InvalidNumber,
		UnknownVariable,
		CannotReadMemory,
		UnexpectedOperator,
		UnexpectedNumber,
		VariableLargerThanAddress
	};

public:
	ExpressionError() = default;

	explicit ExpressionError(ErrorMessage type)
		: error_(type) {
	}

	const char *what() const noexcept {
		switch (error_) {
		case Syntax:
			return "Syntax Error";
		case UnbalancedParens:
			return "Unbalanced Parenthesis";
		case DivideByZero:
			return "Divide By Zero";
		case InvalidNumber:
			return "Invalid Numerical Constant";
		case UnknownVariable:
			return "Unknown Variable";
		case UnbalancedBraces:
			return "Unbalanced Braces";
		case CannotReadMemory:
			return "Cannot Read Memory At the Effective Address";
		case UnexpectedOperator:
			return "Unexpected Operator";
		case UnexpectedNumber:
			return "Unexpected Numerical Constant";
		case VariableLargerThanAddress:
			return "Variable Does Not Fit Into An Address. Is it an FPU Register?";
		default:
			return "Unknown Error";
		}
	}

private:
	ErrorMessage error_ = None;
};

template <class T>
class Expression {
public:
	using variable_getter_t = std::function<T(const QString &, bool *, ExpressionError *)>;
	using memoryReader_t    = std::function<T(T, bool *, ExpressionError *)>;

public:
	Expression(const QString &s, variable_getter_t vg, memoryReader_t mr);
	~Expression() = default;

private:
	struct Token {
		Token()                   = default;
		Token(const Token &other) = default;

		enum Operator {
			NONE,
			AND,
			OR,
			XOR,
			LSHFT,
			RSHFT,
			PLUS,
			MINUS,
			MUL,
			DIV,
			MOD,
			CMP,
			LPAREN,
			RPAREN,
			LBRACE,
			RBRACE,
			NOT,
			LT,
			LE,
			GT,
			GE,
			EQ,
			NE,
			LOGICAL_AND,
			LOGICAL_OR
		};

		enum Type {
			UNKNOWN,
			OPERATOR,
			NUMBER,
			VARIABLE
		};

		void set(const QString &data, Operator oper, Type type) {
			data_     = data;
			operator_ = oper;
			type_     = type;
		}

		QString data_;
		Operator operator_ = NONE;
		Type type_         = UNKNOWN;
	};

private:
	T evalInternal() {
		T result;

		getToken();
		evalExp(result);

		return result;
	}

public:
	Result<T, ExpressionError> evaluate() noexcept {
		try {
			return evalInternal();
		} catch (const ExpressionError &e) {
			return make_unexpected(e);
		}
	}

private:
	void evalExp(T &result);
	void evalExp0(T &result);
	void evalExp1(T &result);
	void evalExp2(T &result);
	void evalExp3(T &result);
	void evalExp4(T &result);
	void evalExp5(T &result);
	void evalExp6(T &result);
	void evalExp7(T &result);
	void evalAtom(T &result);
	void getToken();

private:
	QString expression_;
	QString::const_iterator expressionPtr_;
	Token token_;
	variable_getter_t variableReader_;
	memoryReader_t memoryReader_;
};

#include "Expression.tcc"

#endif
