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
#include "muParserDef.h"

#include <assert.h>

namespace mu {
//---------------------------------------------------------------------------
string_type parser_error_for_code(EErrorCodes code) {
    switch (code) {
        case ecUNASSIGNABLE_TOKEN:
            return _T("Unexpected token \"$TOK$\" found at position $POS$.");
        case ecINVALID_NAME:
            return _T("Invalid function-, variable- or constant name: \"$TOK$\".");
        case ecINVALID_BINOP_IDENT:
            return _T("Invalid binary operator identifier: \"$TOK$\".");
        case ecINVALID_INFIX_IDENT:
            return _T("Invalid infix operator identifier: \"$TOK$\".");
        case ecINVALID_POSTFIX_IDENT:
            return _T("Invalid postfix operator identifier: \"$TOK$\".");
        case ecINVALID_FUN_PTR:
            return _T("Invalid pointer to callback function.");
        case ecEMPTY_EXPRESSION:
            return _T("Expression is empty.");
        case ecINVALID_VAR_PTR:
            return _T("Invalid pointer to variable.");
        case ecUNEXPECTED_OPERATOR:
            return _T("Unexpected operator \"$TOK$\" found at position $POS$");
        case ecUNEXPECTED_EOF:
            return _T("Unexpected end of expression at position $POS$");
        case ecUNEXPECTED_ARG_SEP:
            return _T("Unexpected argument separator at position $POS$");
        case ecUNEXPECTED_PARENS:
            return _T("Unexpected parenthesis \"$TOK$\" at position $POS$");
        case ecUNEXPECTED_FUN:
            return _T("Unexpected function \"$TOK$\" at position $POS$");
        case ecUNEXPECTED_VAL:
            return _T("Unexpected value \"$TOK$\" found at position $POS$");
        case ecUNEXPECTED_VAR:
            return _T("Unexpected variable \"$TOK$\" found at position $POS$");
        case ecUNEXPECTED_ARG:
            return _T("Function arguments used without a function (position: $POS$)");
        case ecMISSING_PARENS:
            return _T("Missing parenthesis");
        case ecTOO_MANY_PARAMS:
            return _T("Too many parameters for function \"$TOK$\" at expression position $POS$");
        case ecTOO_FEW_PARAMS:
            return _T("Too few parameters for function \"$TOK$\" at expression position $POS$");
        case ecDIV_BY_ZERO:
            return _T("Divide by zero");
        case ecDOMAIN_ERROR:
            return _T("Domain error");
        case ecNAME_CONFLICT:
            return _T("Name conflict");
        case ecOPT_PRI:
            return _T("Invalid value for operator priority (must be greater or equal to zero).");
        case ecBUILTIN_OVERLOAD:
            return _T("user defined binary operator \"$TOK$\" conflicts with a built in operator.");
        case ecUNEXPECTED_STR:
            return _T("Unexpected string token found at position $POS$.");
        case ecUNTERMINATED_STRING:
            return _T("Unterminated string starting at position $POS$.");
        case ecSTRING_EXPECTED:
            return _T("String function called with a non string type of argument.");
        case ecVAL_EXPECTED:
            return _T("String value used where a numerical argument is expected.");
        case ecOPRT_TYPE_CONFLICT:
            return _T("No suitable overload for operator \"$TOK$\" at position $POS$.");
        case ecSTR_RESULT:
            return _T("Function result is a string.");
        case ecGENERIC:
            return _T("Parser error.");
        case ecLOCALE:
            return _T("Decimal separator is identic to function argument separator.");
        case ecUNEXPECTED_CONDITIONAL:
            return _T("The \"$TOK$\" operator must be preceeded by a closing bracket.");
        case ecMISSING_ELSE_CLAUSE:
            return _T("If-then-else operator is missing an else clause");
        case ecMISPLACED_COLON:
            return _T("Misplaced colon at position $POS$");
        case ecUNREASONABLE_NUMBER_OF_COMPUTATIONS:
            return _T("Number of computations to small for bulk mode. (Vectorisation overhead too ")
                   _T("costly)");
        default:
            assert(0 && "Invalid error code");
            return string_type();
    }
}

//---------------------------------------------------------------------------
//
//  ParserError class
//
//---------------------------------------------------------------------------

/** \brief Default constructor. */
ParserError::ParserError() {}

//------------------------------------------------------------------------------
/** \brief This Constructor is used for internal exceptions only.

  It does not contain any information but the error code.
*/
ParserError::ParserError(EErrorCodes a_iErrc) : m_iErrc(a_iErrc) {
    m_strMsg = parser_error_for_code(m_iErrc);
    stringstream_type stream;
    stream << (int)m_iPos;
    ReplaceSubString(m_strMsg, _T("$POS$"), stream.str());
    ReplaceSubString(m_strMsg, _T("$TOK$"), m_strTok);
}

//------------------------------------------------------------------------------
/** \brief Construct an error from a message text. */
ParserError::ParserError(const string_type &sMsg) {
    Reset();
    m_strMsg = sMsg;
}

//------------------------------------------------------------------------------
/** \brief Construct an error object.
    \param [in] a_iErrc the error code.
    \param [in] sTok The token string related to this error.
    \param [in] a_iPos the position in the expression where the error occurred.
*/
ParserError::ParserError(EErrorCodes iErrc, const string_type &sTok, int iPos)
    : m_strTok(sTok), m_iPos(iPos), m_iErrc(iErrc) {
    m_strMsg = parser_error_for_code(m_iErrc);
    stringstream_type stream;
    stream << (int)m_iPos;
    ReplaceSubString(m_strMsg, _T("$POS$"), stream.str());
    ReplaceSubString(m_strMsg, _T("$TOK$"), m_strTok);
}

//------------------------------------------------------------------------------
/** \brief Construct an error object.
    \param [in] iErrc the error code.
    \param [in] iPos the position in the expression where the error occurred.
    \param [in] sTok The token string related to this error.
*/
ParserError::ParserError(EErrorCodes iErrc, int iPos, const string_type &sTok)
    : m_strMsg(), m_strTok(sTok), m_iPos(iPos), m_iErrc(iErrc) {
    m_strMsg = parser_error_for_code(m_iErrc);
    stringstream_type stream;
    stream << (int)m_iPos;
    ReplaceSubString(m_strMsg, _T("$POS$"), stream.str());
    ReplaceSubString(m_strMsg, _T("$TOK$"), m_strTok);
}

//------------------------------------------------------------------------------
/** \brief Construct an error object.
    \param [in] szMsg The error message text.
    \param [in] iPos the position related to the error.
    \param [in] sTok The token string related to this error.
*/
ParserError::ParserError(const char_type *szMsg, int iPos, const string_type &sTok)
    : m_strMsg(szMsg), m_strTok(sTok), m_iPos(iPos), m_iErrc(ecGENERIC) {
    stringstream_type stream;
    stream << (int)m_iPos;
    ReplaceSubString(m_strMsg, _T("$POS$"), stream.str());
    ReplaceSubString(m_strMsg, _T("$TOK$"), m_strTok);
}

//------------------------------------------------------------------------------
ParserError::~ParserError() {}

//------------------------------------------------------------------------------
/** \brief Replace all occurrences of a substring with another string.
    \param strFind The string that shall be replaced.
    \param strReplaceWith The string that should be inserted instead of strFind
*/
void ParserError::ReplaceSubString(string_type &strSource, const string_type &strFind,
                                   const string_type &strReplaceWith) {
    string_type strResult;
    string_type::size_type iPos(0), iNext(0);

    for (;;) {
        iNext = strSource.find(strFind, iPos);
        strResult.append(strSource, iPos, iNext - iPos);

        if (iNext == string_type::npos) break;

        strResult.append(strReplaceWith);
        iPos = iNext + strFind.length();
    }

    strSource.swap(strResult);
}

//------------------------------------------------------------------------------
/** \brief Reset the erro object. */
void ParserError::Reset() {
    m_strMsg = _T("");
    m_strTok = _T("");
    m_iPos = -1;
    m_iErrc = ecUNDEFINED;
}

//------------------------------------------------------------------------------
/** \brief Returns the message string for this error. */
const string_type &ParserError::GetMsg() const { return m_strMsg; }

//------------------------------------------------------------------------------
/** \brief Return the formula position related to the error.

  If the error is not related to a distinct position this will return -1
*/
int ParserError::GetPos() const { return m_iPos; }

//------------------------------------------------------------------------------
/** \brief Return string related with this token (if available). */
const string_type &ParserError::GetToken() const { return m_strTok; }

//------------------------------------------------------------------------------
/** \brief Return the error code. */
EErrorCodes ParserError::GetCode() const { return m_iErrc; }
}  // namespace mu
