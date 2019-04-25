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

#ifndef EXPRESSION_20070402_H_
#define EXPRESSION_20070402_H_

#include <QString>
#include <functional>
#include "Status.h"

struct ExpressionError {
public:
	enum ERROR_MSG {
		NONE,
		SYNTAX,
		UNBALANCED_PARENS,
		UNBALANCED_BRACES,
		DIVIDE_BY_ZERO,
		INVALID_NUMBER,
		UNKNOWN_VARIABLE,
		CANNOT_READ_MEMORY,
		UNEXPECTED_OPERATOR,
		UNEXPECTED_NUMBER,
		VARIABLE_LARGER_THAN_ADDRESS
	};

public:
	ExpressionError() = default;

	explicit ExpressionError(ERROR_MSG type) : error_(type) {
	}

	const char *what() const noexcept {
		switch(error_) {
		case SYNTAX:
			return "Syntax Error";
		case UNBALANCED_PARENS:
			return "Unbalanced Parenthesis";
		case DIVIDE_BY_ZERO:
			return "Divide By Zero";
		case INVALID_NUMBER:
			return "Invalid Numerical Constant";
		case UNKNOWN_VARIABLE:
			return "Unknown Variable";
		case UNBALANCED_BRACES:
			return "Unbalanced Braces";
		case CANNOT_READ_MEMORY:
			return "Cannot Read Memory At the Effective Address";
		case UNEXPECTED_OPERATOR:
			return "Unexpected Operator";
		case UNEXPECTED_NUMBER:
			return "Unexpected Numerical Constant";
		case VARIABLE_LARGER_THAN_ADDRESS:
			return "Variable Does Not Fit Into An Address. Is it an FPU Register?";
		default:
			return "Unknown Error";
		}
	}
private:
	ERROR_MSG error_ = NONE;
};


template <class T>
class Expression {
public:
	using variable_getter_t = std::function<T(const QString&, bool*, ExpressionError*)>;
	using memory_reader_t   = std::function<T(T, bool*, ExpressionError*)>;

public:
	Expression(const QString &s, variable_getter_t vg, memory_reader_t mr);
	~Expression() = default;

private:
	struct Token {
		Token() = default;
		Token(const Token& other) = default;

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
	T eval_internal() {
		T result;

		get_token();
		eval_exp(result);

		return result;
	}

public:
	Result<T, ExpressionError> evaluate_expression() noexcept {
		try {
			return eval_internal();
		} catch(const ExpressionError &e) {
			return make_unexpected(e);
		}
	}
private:
	void eval_exp(T &result);
	void eval_exp0(T &result);
	void eval_exp1(T &result);
	void eval_exp2(T &result);
	void eval_exp3(T &result);
	void eval_exp4(T &result);
	void eval_exp5(T &result);
	void eval_exp6(T &result);
	void eval_exp7(T &result);
	void eval_atom(T &result);
	void get_token();

	static bool is_delim(QChar ch) {
		return QString("[]!()=+-*/%&|^~<>\t\n\r ").contains(ch);
	}

private:
	QString                 expression_;
	QString::const_iterator expression_ptr_;
	Token                   token_;
	variable_getter_t       variable_reader_;
	memory_reader_t         memory_reader_;
};

#include "Expression.tcc"

#endif

