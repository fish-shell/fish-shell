/*
                 __________
    _____   __ __\______   \_____  _______  ______  ____ _______
   /     \ |  |  \|     ___/\__  \ \_  __ \/  ___/_/ __ \\_  __ \
  |  Y Y  \|  |  /|    |     / __ \_|  | \/\___ \ \  ___/ |  | \/
  |__|_|  /|____/ |____|    (____  /|__|  /____  > \___  >|__|
        \/                       \/            \/      \/
  Copyright (C) 2011 Ingo Berg

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

#include "muParserInt.h"

#include <algorithm>
#include <cmath>
#include <numeric>

using namespace std;

/** \file
    \brief Implementation of a parser using integer value.
*/

/** \brief Namespace for mathematical applications. */
namespace mu {
ValueOrError ParserInt::Abs(value_type v) { return (value_type)Round(fabs((double)v)); }
ValueOrError ParserInt::Sign(value_type v) { return (Round(v) < 0) ? -1 : (Round(v) > 0) ? 1 : 0; }
ValueOrError ParserInt::Ite(value_type v1, value_type v2, value_type v3) {
    return (Round(v1) == 1) ? Round(v2) : Round(v3);
}
ValueOrError ParserInt::Add(value_type v1, value_type v2) { return Round(v1) + Round(v2); }
ValueOrError ParserInt::Sub(value_type v1, value_type v2) { return Round(v1) - Round(v2); }
ValueOrError ParserInt::Mul(value_type v1, value_type v2) { return Round(v1) * Round(v2); }
ValueOrError ParserInt::Div(value_type v1, value_type v2) { return Round(v1) / Round(v2); }
ValueOrError ParserInt::Mod(value_type v1, value_type v2) { return Round(v1) % Round(v2); }
ValueOrError ParserInt::Shr(value_type v1, value_type v2) { return Round(v1) >> Round(v2); }
ValueOrError ParserInt::Shl(value_type v1, value_type v2) { return Round(v1) << Round(v2); }
ValueOrError ParserInt::LogAnd(value_type v1, value_type v2) { return Round(v1) & Round(v2); }
ValueOrError ParserInt::LogOr(value_type v1, value_type v2) { return Round(v1) | Round(v2); }
ValueOrError ParserInt::And(value_type v1, value_type v2) { return Round(v1) && Round(v2); }
ValueOrError ParserInt::Or(value_type v1, value_type v2) { return Round(v1) || Round(v2); }
ValueOrError ParserInt::Less(value_type v1, value_type v2) { return Round(v1) < Round(v2); }
ValueOrError ParserInt::Greater(value_type v1, value_type v2) { return Round(v1) > Round(v2); }
ValueOrError ParserInt::LessEq(value_type v1, value_type v2) { return Round(v1) <= Round(v2); }
ValueOrError ParserInt::GreaterEq(value_type v1, value_type v2) { return Round(v1) >= Round(v2); }
ValueOrError ParserInt::Equal(value_type v1, value_type v2) { return Round(v1) == Round(v2); }
ValueOrError ParserInt::NotEqual(value_type v1, value_type v2) { return Round(v1) != Round(v2); }
ValueOrError ParserInt::Not(value_type v) { return !Round(v); }

ValueOrError ParserInt::Pow(value_type v1, value_type v2) {
    return std::pow((double)Round(v1), (double)Round(v2));
}

//---------------------------------------------------------------------------
// Unary operator Callbacks: Infix operators
ValueOrError ParserInt::UnaryMinus(value_type v) { return -Round(v); }

//---------------------------------------------------------------------------
ValueOrError ParserInt::Sum(const value_type *a_afArg, int a_iArgc) {
    if (!a_iArgc) return ParserError(_T("too few arguments for function sum."));

    value_type fRes = 0;
    for (int i = 0; i < a_iArgc; ++i) fRes += a_afArg[i];

    return fRes;
}

//---------------------------------------------------------------------------
ValueOrError ParserInt::Min(const value_type *a_afArg, int a_iArgc) {
    if (!a_iArgc) return ParserError(_T("too few arguments for function min."));

    value_type fRes = a_afArg[0];
    for (int i = 0; i < a_iArgc; ++i) fRes = std::min(fRes, a_afArg[i]);

    return fRes;
}

//---------------------------------------------------------------------------
ValueOrError ParserInt::Max(const value_type *a_afArg, int a_iArgc) {
    if (!a_iArgc) return ParserError(_T("too few arguments for function min."));

    value_type fRes = a_afArg[0];
    for (int i = 0; i < a_iArgc; ++i) fRes = std::max(fRes, a_afArg[i]);

    return fRes;
}

//---------------------------------------------------------------------------
// Default value recognition callback
int ParserInt::IsVal(const char_type *a_szExpr, int *a_iPos, value_type *a_fVal) {
    string_type buf(a_szExpr);
    std::size_t pos = buf.find_first_not_of(_T("0123456789"));

    if (pos == std::string::npos) return 0;

    stringstream_type stream(buf.substr(0, pos));
    int iVal(0);

    stream >> iVal;
    if (stream.fail()) return 0;

    stringstream_type::pos_type iEnd = stream.tellg();  // Position after reading
    if (stream.fail()) iEnd = stream.str().length();

    if (iEnd == (stringstream_type::pos_type)-1) return 0;

    *a_iPos += (int)iEnd;
    *a_fVal = (value_type)iVal;
    return 1;
}

//---------------------------------------------------------------------------
/** \brief Check a given position in the expression for the presence of
           a hex value.
    \param a_szExpr Pointer to the expression string
    \param [in/out] a_iPos Pointer to an integer value holding the current parsing
           position in the expression.
    \param [out] a_fVal Pointer to the position where the detected value shall be stored.

  Hey values must be prefixed with "0x" in order to be detected properly.
*/
int ParserInt::IsHexVal(const char_type *a_szExpr, int *a_iPos, value_type *a_fVal) {
    if (a_szExpr[1] == 0 || (a_szExpr[0] != '0' || a_szExpr[1] != 'x')) return 0;

    unsigned iVal(0);

    // New code based on streams for UNICODE compliance:
    stringstream_type::pos_type nPos(0);
    stringstream_type ss(a_szExpr + 2);
    ss >> std::hex >> iVal;
    nPos = ss.tellg();

    if (nPos == (stringstream_type::pos_type)0) return 1;

    *a_iPos += (int)(2 + nPos);
    *a_fVal = (value_type)iVal;
    return 1;
}

//---------------------------------------------------------------------------
int ParserInt::IsBinVal(const char_type *a_szExpr, int *a_iPos, value_type *a_fVal) {
    if (a_szExpr[0] != '#') return 0;

    unsigned iVal(0), iBits(sizeof(iVal) * 8), i(0);

    for (i = 0; (a_szExpr[i + 1] == '0' || a_szExpr[i + 1] == '1') && i < iBits; ++i)
        iVal |= (int)(a_szExpr[i + 1] == '1') << ((iBits - 1) - i);

    if (i == 0) return 0;

    // return false on overflow
    if (i == iBits) return 0;

    *a_fVal = (unsigned)(iVal >> (iBits - i));
    *a_iPos += i + 1;

    return 1;
}

//---------------------------------------------------------------------------
/** \brief Constructor.

  Call ParserBase class constructor and trigger Function, Operator and Constant initialization.
*/
ParserInt::ParserInt() : ParserBase() {
    AddValIdent(IsVal);  // lowest priority
    AddValIdent(IsBinVal);
    AddValIdent(IsHexVal);  // highest priority

    InitCharSets();
    InitFun();
    InitOprt();
}

//---------------------------------------------------------------------------
void ParserInt::InitConst() {}

//---------------------------------------------------------------------------
void ParserInt::InitCharSets() {
    DefineNameChars(_T("0123456789_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"));
    DefineOprtChars(_T("+-*^/?<>=!%&|~'_"));
    DefineInfixOprtChars(_T("/+-*^?<>=!%&|~'_"));
}

//---------------------------------------------------------------------------
/** \brief Initialize the default functions. */
void ParserInt::InitFun() {
    DefineFun(_T("sign"), Sign);
    DefineFun(_T("abs"), Abs);
    DefineFun(_T("if"), Ite);
    DefineFun(_T("sum"), Sum);
    DefineFun(_T("min"), Min);
    DefineFun(_T("max"), Max);
}

/// assert that the given optional error \p oerr is not an error.
/// This is used only during initialization, when it ought to be impossible
/// to generate an error.
static void assertNoError(OptionalError oerr) {
    assert(!oerr.has_error() && "Unexpected error during initialization");
    (void)oerr;
}

//---------------------------------------------------------------------------
/** \brief Initialize operators. */
void ParserInt::InitOprt() {
    // disable all built in operators, not all of them useful for integer numbers
    // (they don't do rounding of values)
    EnableBuiltInOprt(false);

    // Disable all built in operators, they wont work with integer numbers
    // since they are designed for floating point numbers
    assertNoError(DefineInfixOprt(_T("-"), UnaryMinus));
    assertNoError(DefineInfixOprt(_T("!"), Not));

    assertNoError(DefineOprt(_T("&"), LogAnd, prLOGIC));
    assertNoError(DefineOprt(_T("|"), LogOr, prLOGIC));
    assertNoError(DefineOprt(_T("&&"), And, prLOGIC));
    assertNoError(DefineOprt(_T("||"), Or, prLOGIC));

    assertNoError(DefineOprt(_T("<"), Less, prCMP));
    assertNoError(DefineOprt(_T(">"), Greater, prCMP));
    assertNoError(DefineOprt(_T("<="), LessEq, prCMP));
    assertNoError(DefineOprt(_T(">="), GreaterEq, prCMP));
    assertNoError(DefineOprt(_T("=="), Equal, prCMP));
    assertNoError(DefineOprt(_T("!="), NotEqual, prCMP));

    assertNoError(DefineOprt(_T("+"), Add, prADD_SUB));
    assertNoError(DefineOprt(_T("-"), Sub, prADD_SUB));

    assertNoError(DefineOprt(_T("*"), Mul, prMUL_DIV));
    assertNoError(DefineOprt(_T("/"), Div, prMUL_DIV));
    assertNoError(DefineOprt(_T("%"), Mod, prMUL_DIV));

    assertNoError(DefineOprt(_T("^"), Pow, prPOW, oaRIGHT));
    assertNoError(DefineOprt(_T(">>"), Shr, prMUL_DIV + 1));
    assertNoError(DefineOprt(_T("<<"), Shl, prMUL_DIV + 1));
}

}  // namespace mu
