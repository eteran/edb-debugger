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

#ifndef EXPRESSION_20070402_TCC_
#define EXPRESSION_20070402_TCC_

namespace detail {

inline bool is_delim(QChar ch) {
	return QString("[]!()=+-*/%&|^~<>\t\n\r ").contains(ch);
}

}

//------------------------------------------------------------------------------
// Name: Expression
// Desc:
//------------------------------------------------------------------------------
template <class T>
Expression<T>::Expression(const QString &s, variable_getter_t vg, memoryReader_t mr)
	: expression_(s), expressionPtr_(expression_.begin()), variableReader_(vg), memoryReader_(mr) {
}

//------------------------------------------------------------------------------
// Name: evalExp
// Desc: private entry point with sanity check
//------------------------------------------------------------------------------
template <class T>
void Expression<T>::evalExp(T &result) {
	if (token_.type_ == Token::UNKNOWN) {
		throw ExpressionError(ExpressionError::Syntax);
	}

	evalExp0(result);

	switch (token_.type_) {
	case Token::OPERATOR:
		switch (token_.operator_) {
		case Token::LPAREN:
		case Token::RPAREN:
			throw ExpressionError(ExpressionError::UnbalancedParens);
		case Token::LBRACE:
		case Token::RBRACE:
			throw ExpressionError(ExpressionError::UnbalancedBraces);
		default:
			throw ExpressionError(ExpressionError::UnexpectedOperator);
		}
		break;
	case Token::NUMBER:
		throw ExpressionError(ExpressionError::UnexpectedNumber);
		break;
	default:
		break;
	}
}

//------------------------------------------------------------------------------
// Name: evalExp0
// Desc: logic
//------------------------------------------------------------------------------
template <class T>
void Expression<T>::evalExp0(T &result) {
	evalExp1(result);

	for (Token op = token_; op.operator_ == Token::LOGICAL_AND || op.operator_ == Token::LOGICAL_OR; op = token_) {
		T partial_value;

		getToken();
		evalExp1(partial_value);

		// add or subtract
		switch (op.operator_) {
		case Token::LOGICAL_AND:
			result = result && partial_value;
			break;
		case Token::LOGICAL_OR:
			result = result || partial_value;
			break;
		default:
			break;
		}
	}
}

//------------------------------------------------------------------------------
// Name: evalExp1
// Desc: binary logic
//------------------------------------------------------------------------------
template <class T>
void Expression<T>::evalExp1(T &result) {
	evalExp2(result);

	for (Token op = token_; op.operator_ == Token::AND || op.operator_ == Token::OR || op.operator_ == Token::XOR; op = token_) {
		T partial_value;

		getToken();
		evalExp2(partial_value);

		// add or subtract
		switch (op.operator_) {
		case Token::AND:
			result &= partial_value;
			break;
		case Token::OR:
			result |= partial_value;
			break;
		case Token::XOR:
			result ^= partial_value;
			break;
		default:
			break;
		}
	}
}

//------------------------------------------------------------------------------
// Name: evalExp2
// Desc: comparisons
//------------------------------------------------------------------------------
template <class T>
void Expression<T>::evalExp2(T &result) {
	evalExp3(result);

	for (Token op = token_; op.operator_ == Token::LT || op.operator_ == Token::LE || op.operator_ == Token::GT || op.operator_ == Token::GE || op.operator_ == Token::EQ || op.operator_ == Token::NE; op = token_) {
		T partial_value;

		getToken();
		evalExp3(partial_value);

		// perform the relational operation
		switch (op.operator_) {
		case Token::LT:
			result = result < partial_value;
			break;
		case Token::LE:
			result = result <= partial_value;
			break;
		case Token::GT:
			result = result > partial_value;
			break;
		case Token::GE:
			result = result >= partial_value;
			break;
		case Token::EQ:
			result = result == partial_value;
			break;
		case Token::NE:
			result = result != partial_value;
			break;
		default:
			break;
		}
	}
}

//------------------------------------------------------------------------------
// Name: evalExp3
// Desc: shifts
//------------------------------------------------------------------------------
template <class T>
void Expression<T>::evalExp3(T &result) {
	evalExp4(result);

	for (Token op = token_; op.operator_ == Token::RSHFT || op.operator_ == Token::LSHFT; op = token_) {
		T partial_value;

		getToken();
		evalExp4(partial_value);

		// perform the shift operation
		switch (op.operator_) {
		case Token::LSHFT:
			result <<= partial_value;
			break;
		case Token::RSHFT:
			result >>= partial_value;
			break;
		default:
			break;
		}
	}
}

//------------------------------------------------------------------------------
// Name: evalExp4
// Desc: addition/subtraction
//------------------------------------------------------------------------------
template <class T>
void Expression<T>::evalExp4(T &result) {
	evalExp5(result);

	for (Token op = token_; op.operator_ == Token::PLUS || op.operator_ == Token::MINUS; op = token_) {
		T partial_value;

		getToken();
		evalExp5(partial_value);

		// add or subtract
		switch (op.operator_) {
		case Token::PLUS:
			result += partial_value;
			break;
		case Token::MINUS:
#ifdef _MSC_VER
#pragma warning(push)
/* disable warning about applying unary - to an unsigned type */
#pragma warning(disable : 4146)
#endif
			result -= partial_value;
#ifdef _MSC_VER
#pragma warning(pop)
#endif
			break;
		default:
			break;
		}
	}
}

//------------------------------------------------------------------------------
// Name: evalExp5
// Desc: multiplication/division
//------------------------------------------------------------------------------
template <class T>
void Expression<T>::evalExp5(T &result) {
	evalExp6(result);

	for (Token op = token_; op.operator_ == Token::MUL || op.operator_ == Token::DIV || op.operator_ == Token::MOD; op = token_) {
		T partial_value;

		getToken();
		evalExp6(partial_value);

		// mul, div, or modulus
		switch (op.operator_) {
		case Token::MUL:
			result *= partial_value;
			break;
		case Token::DIV:
			if (partial_value == 0) {
				throw ExpressionError(ExpressionError::DivideByZero);
			}
			result /= partial_value;
			break;
		case Token::MOD:
			if (partial_value == 0) {
				throw ExpressionError(ExpressionError::DivideByZero);
			}
			result %= partial_value;
			break;
		default:
			break;
		}
	}
}

//------------------------------------------------------------------------------
// Name: evalExp6
// Desc: unary expressions
//------------------------------------------------------------------------------
template <class T>
void Expression<T>::evalExp6(T &result) {

	Token op = token_;
	if (op.operator_ == Token::PLUS || op.operator_ == Token::MINUS || op.operator_ == Token::CMP || op.operator_ == Token::NOT) {
		getToken();
	}

	evalExp7(result);

	switch (op.operator_) {
	case Token::PLUS:
		// this may seems like a waste, but unary + can be overloaded for a type
		// to have a non-nop effect!
		result = +result;
		break;
	case Token::MINUS:
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4146)
#endif
		result = -result;
#ifdef _MSC_VER
#pragma warning(pop)
#endif
		break;
	case Token::CMP:
		result = ~result;
		break;
	case Token::NOT:
		result = !result;
		break;
	default:
		break;
	}
}

//------------------------------------------------------------------------------
// Name: evalExp7
// Desc: sub-expressions
//------------------------------------------------------------------------------
template <class T>
void Expression<T>::evalExp7(T &result) {

	switch (token_.operator_) {
	case Token::LPAREN:
		getToken();

		// get sub-expression
		evalExp0(result);

		if (token_.operator_ != Token::RPAREN) {
			throw ExpressionError(ExpressionError::UnbalancedParens);
		}

		getToken();
		break;
	case Token::RPAREN:
		throw ExpressionError(ExpressionError::UnbalancedParens);
		break;
	case Token::LBRACE:
		do {
			getToken();

			// get sub-expression
			T effective_address;
			evalExp0(effective_address);

			if (memoryReader_) {
				bool ok;
				ExpressionError error;

				result = memoryReader_(effective_address, &ok, &error);
				if (!ok) {
					throw error;
				}
			} else {
				throw ExpressionError(ExpressionError::CannotReadMemory);
			}

			if (token_.operator_ != Token::RBRACE) {
				throw ExpressionError(ExpressionError::UnbalancedBraces);
			}

			getToken();
		} while (0);
		break;
	case Token::RBRACE:
		throw ExpressionError(ExpressionError::UnbalancedBraces);
		break;
	default:
		evalAtom(result);
		break;
	}
}

//------------------------------------------------------------------------------
// Name: evalAtom
// Desc: atoms (variables/constants)
//------------------------------------------------------------------------------
template <class T>
void Expression<T>::evalAtom(T &result) {

	switch (token_.type_) {
	case Token::VARIABLE:
		if (variableReader_) {
			bool ok;
			ExpressionError error;
			result = variableReader_(token_.data_, &ok, &error);
			if (!ok) {
				throw error;
			}
		} else {
			throw ExpressionError(ExpressionError::UnknownVariable);
		}
		getToken();
		break;
	case Token::NUMBER:
		bool ok;
		result = token_.data_.toULongLong(&ok, 0);
		if (!ok) {
			throw ExpressionError(ExpressionError::InvalidNumber);
		}
		getToken();
		break;
	default:
		throw ExpressionError(ExpressionError::Syntax);
		break;
	}
}

//------------------------------------------------------------------------------
// Name: getToken
// Desc:
//------------------------------------------------------------------------------
template <class T>
void Expression<T>::getToken() {

	// clear previous token
	token_ = Token();

	// eat up white space
	while (expressionPtr_ != expression_.end() && expressionPtr_->isSpace()) {
		++expressionPtr_;
	}

	if (expressionPtr_ != expression_.end()) {

		// get the token
		switch (expressionPtr_->toLatin1()) {
		case '(':
			++expressionPtr_;
			token_.set("(", Token::LPAREN, Token::OPERATOR);
			break;
		case ')':
			++expressionPtr_;
			token_.set(")", Token::RPAREN, Token::OPERATOR);
			break;
		case '[':
			++expressionPtr_;
			token_.set("[", Token::LBRACE, Token::OPERATOR);
			break;
		case ']':
			++expressionPtr_;
			token_.set("]", Token::RBRACE, Token::OPERATOR);
			break;
		case '!':
			++expressionPtr_;
			if (expressionPtr_ != expression_.end() && *expressionPtr_ == '=') {
				++expressionPtr_;
				token_.set("!=", Token::NE, Token::OPERATOR);
			} else {
				token_.set("!", Token::NOT, Token::OPERATOR);
			}
			break;
		case '+':
			++expressionPtr_;
			token_.set("+", Token::PLUS, Token::OPERATOR);
			break;
		case '-':
			++expressionPtr_;
			token_.set("-", Token::MINUS, Token::OPERATOR);
			break;
		case '*':
			++expressionPtr_;
			token_.set("*", Token::MUL, Token::OPERATOR);
			break;
		case '/':
			++expressionPtr_;
			token_.set("/", Token::DIV, Token::OPERATOR);
			break;
		case '%':
			++expressionPtr_;
			token_.set("%", Token::MOD, Token::OPERATOR);
			break;
		case '&':
			++expressionPtr_;
			if (expressionPtr_ != expression_.end() && *expressionPtr_ == '&') {
				++expressionPtr_;
				token_.set("&&", Token::LOGICAL_AND, Token::OPERATOR);
			} else {
				token_.set("&", Token::AND, Token::OPERATOR);
			}
			break;
		case '|':
			++expressionPtr_;
			if (expressionPtr_ != expression_.end() && *expressionPtr_ == '|') {
				++expressionPtr_;
				token_.set("||", Token::LOGICAL_OR, Token::OPERATOR);
			} else {
				token_.set("|", Token::OR, Token::OPERATOR);
			}
			break;
		case '^':
			++expressionPtr_;
			token_.set("^", Token::XOR, Token::OPERATOR);
			break;
		case '~':
			++expressionPtr_;
			token_.set("~", Token::CMP, Token::OPERATOR);
			break;
		case '=':
			++expressionPtr_;
			if (expressionPtr_ != expression_.end() && *expressionPtr_ == '=') {
				++expressionPtr_;
				token_.set("==", Token::EQ, Token::OPERATOR);
			} else {
				throw ExpressionError(ExpressionError::Syntax);
			}
			break;
		case '<':
			++expressionPtr_;
			if (expressionPtr_ != expression_.end() && *expressionPtr_ == '<') {
				++expressionPtr_;
				token_.set("<<", Token::LSHFT, Token::OPERATOR);
			} else if (expressionPtr_ != expression_.end() && *expressionPtr_ == '=') {
				++expressionPtr_;
				token_.set("<=", Token::LE, Token::OPERATOR);
			} else {
				token_.set("<", Token::LT, Token::OPERATOR);
			}
			break;
		case '>':
			++expressionPtr_;
			if (expressionPtr_ != expression_.end() && *expressionPtr_ == '>') {
				++expressionPtr_;
				token_.set(">>", Token::RSHFT, Token::OPERATOR);
			} else if (expressionPtr_ != expression_.end() && *expressionPtr_ == '=') {
				++expressionPtr_;
				token_.set(">=", Token::GE, Token::OPERATOR);
			} else {
				token_.set(">", Token::GT, Token::OPERATOR);
			}
			break;
		case '"':
			++expressionPtr_;
			// Begin a quoted string
			{
				QString temp_string;

				while (expressionPtr_ != expression_.end() && *expressionPtr_ != '"') {
					temp_string += *expressionPtr_++;
				}
				if (expressionPtr_ == expression_.end()) {
					token_.set("\"" + temp_string, Token::NONE, Token::VARIABLE);
				} else {
					token_.set(temp_string, Token::NONE, Token::VARIABLE);
				}
			}
			break;
		default:
			// is it a numerical constant?
			if (expressionPtr_->isDigit()) {
				QString temp_string;

				while (expressionPtr_ != expression_.end() && !detail::is_delim(*expressionPtr_)) {
					temp_string += *expressionPtr_++;
				}

				token_.set(temp_string, Token::NONE, Token::NUMBER);
			} else {
				// it must be a variable, get its name
				QString temp_string;

				while (expressionPtr_ != expression_.end()) {
					// NOTE(eteran): the expression: "VAR !" ... is kinda
					// nonsense AND we want to allow a name to have a "!" in
					// the middle of it since we want to support symbols with
					// module notation
					if (detail::is_delim(*expressionPtr_) && *expressionPtr_ != '!') {
						break;
					}

					temp_string += *expressionPtr_++;
				}

				token_.set(temp_string, Token::NONE, Token::VARIABLE);
			}
			break;
		}
	}
}

#endif
