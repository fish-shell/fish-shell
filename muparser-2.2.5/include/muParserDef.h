/*
                 __________
    _____   __ __\______   \_____  _______  ______  ____ _______
   /     \ |  |  \|     ___/\__  \ \_  __ \/  ___/_/ __ \\_  __ \
  |  Y Y  \|  |  /|    |     / __ \_|  | \/\___ \ \  ___/ |  | \/
  |__|_|  /|____/ |____|    (____  /|__|  /____  > \___  >|__|
        \/                       \/            \/      \/
  Copyright (C) 2014 Ingo Berg

  Permission is hereby granted, free of charge, to any person obtaining a copy of this
  software and associated documentation files (the "Software"), to deal in the Software
  without restriction, including without limitation the rights to use, copy, modify,
  merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
  permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or
  substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
  NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
  DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#ifndef MUP_DEF_H
#define MUP_DEF_H

#include <cassert>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "muParserFixes.h"

/** \file
    \brief This file contains standard definitions used by the parser.
*/

#define MUP_VERSION _T("2.2.5")
#define MUP_VERSION_DATE _T("20150427; GC")

#define MUP_CHARS _T("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ")

/** \brief If this macro is defined mathematical exceptions (div by zero) will be thrown as
 * exceptions. */
//#define MUP_MATH_EXCEPTIONS

/** \brief Define the base datatype for values.

  This datatype must be a built in value type. You can not use custom classes.
  It should be working with all types except "int"!
*/
#define MUP_BASETYPE double

#if defined(_UNICODE)
/** \brief Definition of the basic parser string type. */
#define MUP_STRING_TYPE std::wstring

#if !defined(_T)
#define _T(x) L##x
#endif  // not defined _T
#else
#ifndef _T
#define _T(x) x
#endif

/** \brief Definition of the basic parser string type. */
#define MUP_STRING_TYPE std::string
#endif

namespace mu {
#if defined(_UNICODE)

//------------------------------------------------------------------------------
/** \brief Encapsulate wcout. */
inline std::wostream &console() { return std::wcout; }

/** \brief Encapsulate cin. */
inline std::wistream &console_in() { return std::wcin; }

#else

/** \brief Encapsulate cout.

  Used for supporting UNICODE more easily.
*/
inline std::ostream &console() { return std::cout; }

/** \brief Encapsulate cin.

  Used for supporting UNICODE more easily.
*/
inline std::istream &console_in() { return std::cin; }

#endif

/// Our stack type.
template <typename T>
class ParserStack : public std::vector<T> {
   public:
    // Convenience to get the top value and pop it.
    T pop() {
        T val = std::move(this->back());
        this->pop_back();
        return val;
    }

    T &top() { return this->back(); }

    void push(T val) { this->push_back(std::move(val)); }
};

//------------------------------------------------------------------------------
/** \brief Bytecode values.

    \attention The order of the operator entries must match the order in ParserBase::c_DefaultOprt!
*/
enum ECmdCode {
    // The following are codes for built in binary operators
    // apart from built in operators the user has the opportunity to
    // add user defined operators.
    cmLE = 0,    ///< Operator item:  less or equal
    cmGE = 1,    ///< Operator item:  greater or equal
    cmNEQ = 2,   ///< Operator item:  not equal
    cmEQ = 3,    ///< Operator item:  equals
    cmLT = 4,    ///< Operator item:  less than
    cmGT = 5,    ///< Operator item:  greater than
    cmADD = 6,   ///< Operator item:  add
    cmSUB = 7,   ///< Operator item:  subtract
    cmMUL = 8,   ///< Operator item:  multiply
    cmDIV = 9,   ///< Operator item:  division
    cmPOW = 10,  ///< Operator item:  y to the power of ...
    cmLAND = 11,
    cmLOR = 12,
    cmASSIGN = 13,   ///< Operator item:  Assignment operator
    cmBO = 14,       ///< Operator item:  opening bracket
    cmBC = 15,       ///< Operator item:  closing bracket
    cmIF = 16,       ///< For use in the ternary if-then-else operator
    cmELSE = 17,     ///< For use in the ternary if-then-else operator
    cmENDIF = 18,    ///< For use in the ternary if-then-else operator
    cmARG_SEP = 19,  ///< function argument separator
    cmVAR = 20,      ///< variable item
    cmVAL = 21,      ///< value item

    // For optimization purposes
    cmVARPOW2,
    cmVARPOW3,
    cmVARPOW4,
    cmVARMUL,
    cmPOW2,

    // operators and functions
    cmFUNC,          ///< Code for a generic function item
    cmFUNC_STR,      ///< Code for a function with a string parameter
    cmSTRING,        ///< Code for a string token
    cmOPRT_BIN,      ///< user defined binary operator
    cmOPRT_POSTFIX,  ///< code for postfix operators
    cmOPRT_INFIX,    ///< code for infix operators
    cmEND,           ///< end of formula
    cmUNKNOWN        ///< uninitialized item
};

//------------------------------------------------------------------------------
/** \brief Types internally used by the parser.
*/
enum ETypeCode {
    tpSTR = 0,  ///< String type (Function arguments and constants only, no string variables)
    tpDBL = 1,  ///< Floating point variables
    tpVOID = 2  ///< Undefined type.
};

//------------------------------------------------------------------------------
enum EParserVersionInfo { pviBRIEF, pviFULL };

//------------------------------------------------------------------------------
/** \brief Parser operator precedence values. */
enum EOprtAssociativity { oaLEFT = 0, oaRIGHT = 1, oaNONE = 2 };

//------------------------------------------------------------------------------
/** \brief Parser operator precedence values. */
enum EOprtPrecedence {
    // binary operators
    prLOR = 1,
    prLAND = 2,
    prLOGIC = 3,    ///< logic operators
    prCMP = 4,      ///< comparsion operators
    prADD_SUB = 5,  ///< addition
    prMUL_DIV = 6,  ///< multiplication/division
    prPOW = 7,      ///< power operator priority (highest)

    // infix operators
    prINFIX = 6,   ///< Signs have a higher priority than ADD_SUB, but lower than power operator
    prPOSTFIX = 6  ///< Postfix operator priority (currently unused)
};

//------------------------------------------------------------------------------
// basic types

/** \brief The numeric datatype used by the parser.

  Normally this is a floating point type either single or double precision.
*/
typedef MUP_BASETYPE value_type;

/** \brief The stringtype used by the parser.

  Depends on wether UNICODE is used or not.
*/
typedef MUP_STRING_TYPE string_type;

/** \brief The character type used by the parser.

  Depends on wether UNICODE is used or not.
*/
typedef string_type::value_type char_type;

/** \brief Typedef for easily using stringstream that respect the parser stringtype. */
typedef std::basic_stringstream<char_type, std::char_traits<char_type>, std::allocator<char_type> >
    stringstream_type;

// Data container types

/** \brief Type used for storing variables. */
typedef std::map<string_type, value_type *> varmap_type;

/** \brief Type used for storing constants. */
typedef std::map<string_type, value_type> valmap_type;

/** \brief Type for assigning a string name to an index in the internal string table. */
typedef std::map<string_type, std::size_t> strmap_type;

/** \brief Error codes. */
enum EErrorCodes {
    // Formula syntax errors
    ecUNEXPECTED_OPERATOR = 0,  ///< Unexpected binary operator found
    ecUNASSIGNABLE_TOKEN = 1,   ///< Token cant be identified.
    ecUNEXPECTED_EOF = 2,       ///< Unexpected end of formula. (Example: "2+sin(")
    ecUNEXPECTED_ARG_SEP = 3,   ///< An unexpected comma has been found. (Example: "1,23")
    ecUNEXPECTED_ARG = 4,       ///< An unexpected argument has been found
    ecUNEXPECTED_VAL = 5,       ///< An unexpected value token has been found
    ecUNEXPECTED_VAR = 6,       ///< An unexpected variable token has been found
    ecUNEXPECTED_PARENS = 7,    ///< Unexpected Parenthesis, opening or closing
    ecUNEXPECTED_STR = 8,       ///< A string has been found at an inapropriate position
    ecSTRING_EXPECTED = 9,  ///< A string function has been called with a different type of argument
    ecVAL_EXPECTED =
        10,  ///< A numerical function has been called with a non value type of argument
    ecMISSING_PARENS = 11,       ///< Missing parens. (Example: "3*sin(3")
    ecUNEXPECTED_FUN = 12,       ///< Unexpected function found. (Example: "sin(8)cos(9)")
    ecUNTERMINATED_STRING = 13,  ///< unterminated string constant. (Example: "3*valueof("hello)")
    ecTOO_MANY_PARAMS = 14,      ///< Too many function parameters
    ecTOO_FEW_PARAMS = 15,       ///< Too few function parameters. (Example: "ite(1<2,2)")
    ecOPRT_TYPE_CONFLICT =
        16,             ///< binary operators may only be applied to value items of the same type
    ecSTR_RESULT = 17,  ///< result is a string

    // Invalid Parser input Parameters
    ecINVALID_NAME = 18,           ///< Invalid function, variable or constant name.
    ecINVALID_BINOP_IDENT = 19,    ///< Invalid binary operator identifier
    ecINVALID_INFIX_IDENT = 20,    ///< Invalid function, variable or constant name.
    ecINVALID_POSTFIX_IDENT = 21,  ///< Invalid function, variable or constant name.

    ecBUILTIN_OVERLOAD = 22,  ///< Trying to overload builtin operator
    ecINVALID_FUN_PTR = 23,   ///< Invalid callback function pointer
    ecINVALID_VAR_PTR = 24,   ///< Invalid variable pointer
    ecEMPTY_EXPRESSION = 25,  ///< The Expression is empty
    ecNAME_CONFLICT = 26,     ///< Name conflict
    ecOPT_PRI = 27,           ///< Invalid operator priority
    //
    ecDOMAIN_ERROR = 28,  ///< catch division by zero, sqrt(-1), log(0) (currently unused)
    ecDIV_BY_ZERO = 29,   ///< Division by zero (currently unused)
    ecGENERIC = 30,       ///< Generic error
    ecLOCALE = 31,        ///< Conflict with current locale

    ecUNEXPECTED_CONDITIONAL = 32,
    ecMISSING_ELSE_CLAUSE = 33,
    ecMISPLACED_COLON = 34,

    ecUNREASONABLE_NUMBER_OF_COMPUTATIONS = 35,

    // The last two are special entries
    ecCOUNT,  ///< This is no error code, It just stores just the total number of error codes
    ecUNDEFINED = -1  ///< Undefined message, placeholder to detect unassigned error messages
};

/// \return an error message for the given code.
string_type parser_error_for_code(EErrorCodes code);

//---------------------------------------------------------------------------
/** \brief Error class of the parser.
 \author Ingo Berg

 Part of the math parser package.
 */
class ParserError {
   private:
    /** \brief Replace all ocuurences of a substring with another string. */
    void ReplaceSubString(string_type &strSource, const string_type &strFind,
                          const string_type &strReplaceWith);
    void Reset();

   public:
    ParserError();
    explicit ParserError(EErrorCodes a_iErrc);
    explicit ParserError(const string_type &sMsg);
    ParserError(EErrorCodes a_iErrc, const string_type &sTok,
                const string_type &sFormula = string_type(), int a_iPos = -1);
    ParserError(EErrorCodes a_iErrc, int a_iPos, const string_type &sTok);
    ParserError(const char_type *a_szMsg, int a_iPos = -1, const string_type &sTok = string_type());
    ParserError(const ParserError &a_Obj);
    ParserError &operator=(const ParserError &a_Obj);
    ~ParserError();

    void SetFormula(const string_type &a_strFormula);
    const string_type &GetExpr() const;
    const string_type &GetMsg() const;
    int GetPos() const;
    const string_type &GetToken() const;
    EErrorCodes GetCode() const;

   private:
    string_type m_strMsg;      ///< The message string
    string_type m_strFormula;  ///< Formula string
    string_type m_strTok;      ///< Token related with the error
    int m_iPos;                ///< Formula position related to the error
    EErrorCodes m_iErrc;       ///< Error code
};

// Compatibility with non-clang compilers.
#ifndef __has_attribute
#define __has_attribute(x) 0
#endif

// Define a type-level attribute declaring that this type, when used as the return value
// of a function, should produce warnings.
#if __has_attribute(warn_unused_result)
#define MUPARSER_ATTR_WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#else
#define MUPARSER_ATTR_WARN_UNUSED_RESULT
#endif

// OptionalError is used to optionally encapsulate an error.
class OptionalError {
    std::unique_ptr<ParserError> error_{};

   public:
    /// Create an OptionalError that represents no error.
    OptionalError() {}

    /// Create an OptionalError for the given error.
    /* implicit */ OptionalError(ParserError err) : error_(new ParserError(std::move(err))) {}

    /// \return whether an error is present.
    bool has_error() const { return bool(error_); }

    /// \return the error. asserts if this is not an error.
    const ParserError &error() const {
        assert(error_ && "Value did not error");
        return *error_;
    }
} MUPARSER_ATTR_WARN_UNUSED_RESULT;

// ValueOrError is used to propagate failures to callers.
class ValueOrError {
    value_type value_{0};
    OptionalError error_{};

   public:
    /// \return true if this has a value, false if it is an error.
    bool has_value() const { return !error_.has_error(); }

    /// \return false if this has a value,true if it is an error.
    bool has_error() const { return error_.has_error(); }

    /// Construct from a value.
    /* implicit */ ValueOrError(value_type value) : value_(value) {}

    /// Construct from an error.
    /* implicit */ ValueOrError(ParserError err) : error_(std::move(err)) {}

    /// \return the error. asserts if this is not an error.
    const ParserError &error() const { return error_.error(); }

    /// \return the value. asserts if this is an error.
    value_type value() const {
        assert(has_value() && "Value is an error");
        return value_;
    }

    /// \return the value or throw the error
    value_type getOrThrow() const {
        if (has_error()) throw error();
        return value();
    }

    /// \return whether this has a value.
    explicit operator bool() const { return has_value(); }

    value_type operator*() const {
        assert(has_value() && "Cannot access value with error");
        return value_;
    }

} MUPARSER_ATTR_WARN_UNUSED_RESULT;

// Parser callbacks

/** \brief Callback type used for functions without arguments. */
typedef ValueOrError (*generic_fun_type)();

/** \brief Callback type used for functions without arguments. */
typedef ValueOrError (*fun_type0)();

/** \brief Callback type used for functions with a single arguments. */
typedef ValueOrError (*fun_type1)(value_type);

/** \brief Callback type used for functions with two arguments. */
typedef ValueOrError (*fun_type2)(value_type, value_type);

/** \brief Callback type used for functions with three arguments. */
typedef ValueOrError (*fun_type3)(value_type, value_type, value_type);

/** \brief Callback type used for functions with four arguments. */
typedef ValueOrError (*fun_type4)(value_type, value_type, value_type, value_type);

/** \brief Callback type used for functions with five arguments. */
typedef ValueOrError (*fun_type5)(value_type, value_type, value_type, value_type, value_type);

/** \brief Callback type used for functions with five arguments. */
typedef ValueOrError (*fun_type6)(value_type, value_type, value_type, value_type, value_type,
                                  value_type);

/** \brief Callback type used for functions with five arguments. */
typedef ValueOrError (*fun_type7)(value_type, value_type, value_type, value_type, value_type,
                                  value_type, value_type);

/** \brief Callback type used for functions with five arguments. */
typedef ValueOrError (*fun_type8)(value_type, value_type, value_type, value_type, value_type,
                                  value_type, value_type, value_type);

/** \brief Callback type used for functions with five arguments. */
typedef ValueOrError (*fun_type9)(value_type, value_type, value_type, value_type, value_type,
                                  value_type, value_type, value_type, value_type);

/** \brief Callback type used for functions with five arguments. */
typedef ValueOrError (*fun_type10)(value_type, value_type, value_type, value_type, value_type,
                                   value_type, value_type, value_type, value_type, value_type);

/** \brief Callback type used for functions with a variable argument list. */
typedef ValueOrError (*multfun_type)(const value_type *, int);

/** \brief Callback type used for functions taking a string as an argument. */
typedef ValueOrError (*strfun_type1)(const char_type *);

/** \brief Callback type used for functions taking a string and a value as arguments. */
typedef ValueOrError (*strfun_type2)(const char_type *, value_type);

/** \brief Callback type used for functions taking a string and two values as arguments. */
typedef ValueOrError (*strfun_type3)(const char_type *, value_type, value_type);

/** \brief Callback used for functions that identify values in a string. */
typedef int (*identfun_type)(const char_type *sExpr, int *nPos, value_type *fVal);

/** \brief Callback used for variable creation factory functions. */
typedef value_type *(*facfun_type)(const char_type *, void *);
}  // end of namespace

#endif
