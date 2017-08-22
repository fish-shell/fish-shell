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
#include "muParserError.h"


namespace mu
{
  const ParserErrorMsg ParserErrorMsg::m_Instance;

  //------------------------------------------------------------------------------
  const ParserErrorMsg& ParserErrorMsg::Instance()
  {
    return m_Instance;
  }

  //------------------------------------------------------------------------------
  string_type ParserErrorMsg::operator[](unsigned a_iIdx) const
  {
    return (a_iIdx<m_vErrMsg.size()) ? m_vErrMsg[a_iIdx] : string_type();
  }

  //---------------------------------------------------------------------------
  ParserErrorMsg::~ParserErrorMsg()
  {}

  //---------------------------------------------------------------------------
  /** \brief Assignement operator is deactivated.
  */
  ParserErrorMsg& ParserErrorMsg::operator=(const ParserErrorMsg& )
  {
    assert(false);
    return *this;
  }

  //---------------------------------------------------------------------------
  ParserErrorMsg::ParserErrorMsg(const ParserErrorMsg&)
  {}

  //---------------------------------------------------------------------------
  ParserErrorMsg::ParserErrorMsg()
    :m_vErrMsg(0)
  {
    m_vErrMsg.resize(ecCOUNT);

    m_vErrMsg[ecUNASSIGNABLE_TOKEN]     = _T("Unexpected token \"$TOK$\" found at position $POS$.");
    m_vErrMsg[ecINTERNAL_ERROR]         = _T("Internal error");
    m_vErrMsg[ecINVALID_NAME]           = _T("Invalid function-, variable- or constant name: \"$TOK$\".");
    m_vErrMsg[ecINVALID_BINOP_IDENT]    = _T("Invalid binary operator identifier: \"$TOK$\".");
    m_vErrMsg[ecINVALID_INFIX_IDENT]    = _T("Invalid infix operator identifier: \"$TOK$\".");
    m_vErrMsg[ecINVALID_POSTFIX_IDENT]  = _T("Invalid postfix operator identifier: \"$TOK$\".");
    m_vErrMsg[ecINVALID_FUN_PTR]        = _T("Invalid pointer to callback function.");
    m_vErrMsg[ecEMPTY_EXPRESSION]       = _T("Expression is empty.");
    m_vErrMsg[ecINVALID_VAR_PTR]        = _T("Invalid pointer to variable.");
    m_vErrMsg[ecUNEXPECTED_OPERATOR]    = _T("Unexpected operator \"$TOK$\" found at position $POS$");
    m_vErrMsg[ecUNEXPECTED_EOF]         = _T("Unexpected end of expression at position $POS$");
    m_vErrMsg[ecUNEXPECTED_ARG_SEP]     = _T("Unexpected argument separator at position $POS$");
    m_vErrMsg[ecUNEXPECTED_PARENS]      = _T("Unexpected parenthesis \"$TOK$\" at position $POS$");
    m_vErrMsg[ecUNEXPECTED_FUN]         = _T("Unexpected function \"$TOK$\" at position $POS$");
    m_vErrMsg[ecUNEXPECTED_VAL]         = _T("Unexpected value \"$TOK$\" found at position $POS$");
    m_vErrMsg[ecUNEXPECTED_VAR]         = _T("Unexpected variable \"$TOK$\" found at position $POS$");
    m_vErrMsg[ecUNEXPECTED_ARG]         = _T("Function arguments used without a function (position: $POS$)");
    m_vErrMsg[ecMISSING_PARENS]         = _T("Missing parenthesis");
    m_vErrMsg[ecTOO_MANY_PARAMS]        = _T("Too many parameters for function \"$TOK$\" at expression position $POS$");
    m_vErrMsg[ecTOO_FEW_PARAMS]         = _T("Too few parameters for function \"$TOK$\" at expression position $POS$");
    m_vErrMsg[ecDIV_BY_ZERO]            = _T("Divide by zero");
    m_vErrMsg[ecDOMAIN_ERROR]           = _T("Domain error");
    m_vErrMsg[ecNAME_CONFLICT]          = _T("Name conflict");
    m_vErrMsg[ecOPT_PRI]                = _T("Invalid value for operator priority (must be greater or equal to zero).");
    m_vErrMsg[ecBUILTIN_OVERLOAD]       = _T("user defined binary operator \"$TOK$\" conflicts with a built in operator.");
    m_vErrMsg[ecUNEXPECTED_STR]         = _T("Unexpected string token found at position $POS$.");
    m_vErrMsg[ecUNTERMINATED_STRING]    = _T("Unterminated string starting at position $POS$.");
    m_vErrMsg[ecSTRING_EXPECTED]        = _T("String function called with a non string type of argument.");
    m_vErrMsg[ecVAL_EXPECTED]           = _T("String value used where a numerical argument is expected.");
    m_vErrMsg[ecOPRT_TYPE_CONFLICT]     = _T("No suitable overload for operator \"$TOK$\" at position $POS$.");
    m_vErrMsg[ecSTR_RESULT]             = _T("Function result is a string.");
    m_vErrMsg[ecGENERIC]                = _T("Parser error.");
    m_vErrMsg[ecLOCALE]                 = _T("Decimal separator is identic to function argument separator.");
    m_vErrMsg[ecUNEXPECTED_CONDITIONAL] = _T("The \"$TOK$\" operator must be preceeded by a closing bracket.");
    m_vErrMsg[ecMISSING_ELSE_CLAUSE]    = _T("If-then-else operator is missing an else clause");
    m_vErrMsg[ecMISPLACED_COLON]        = _T("Misplaced colon at position $POS$");
    m_vErrMsg[ecUNREASONABLE_NUMBER_OF_COMPUTATIONS] = _T("Number of computations to small for bulk mode. (Vectorisation overhead too costly)");
    
    #if defined(_DEBUG)
      for (int i=0; i<ecCOUNT; ++i)
        if (!m_vErrMsg[i].length())
          assert(false);
    #endif
  }

  //---------------------------------------------------------------------------
  //
  //  ParserError class
  //
  //---------------------------------------------------------------------------

  /** \brief Default constructor. */
  ParserError::ParserError()
    :m_strMsg()
    ,m_strFormula()
    ,m_strTok()
    ,m_iPos(-1)
    ,m_iErrc(ecUNDEFINED)
    ,m_ErrMsg(ParserErrorMsg::Instance())
  {
  }

  //------------------------------------------------------------------------------
  /** \brief This Constructor is used for internal exceptions only. 
      
    It does not contain any information but the error code.
  */
  ParserError::ParserError(EErrorCodes a_iErrc) 
    :m_strMsg()
    ,m_strFormula()
    ,m_strTok()
    ,m_iPos(-1)
    ,m_iErrc(a_iErrc)
    ,m_ErrMsg(ParserErrorMsg::Instance())
  {
    m_strMsg = m_ErrMsg[m_iErrc];
    stringstream_type stream;
    stream << (int)m_iPos;
    ReplaceSubString(m_strMsg, _T("$POS$"), stream.str());
    ReplaceSubString(m_strMsg, _T("$TOK$"), m_strTok);
  }

  //------------------------------------------------------------------------------
  /** \brief Construct an error from a message text. */
  ParserError::ParserError(const string_type &sMsg) 
    :m_ErrMsg(ParserErrorMsg::Instance())
  {
    Reset();
    m_strMsg = sMsg;
  }

  //------------------------------------------------------------------------------
  /** \brief Construct an error object. 
      \param [in] a_iErrc the error code.
      \param [in] sTok The token string related to this error.
      \param [in] sExpr The expression related to the error.
      \param [in] a_iPos the position in the expression where the error occurred. 
  */
  ParserError::ParserError( EErrorCodes iErrc,
                            const string_type &sTok,
                            const string_type &sExpr,
                            int iPos )
    :m_strMsg()
    ,m_strFormula(sExpr)
    ,m_strTok(sTok)
    ,m_iPos(iPos)
    ,m_iErrc(iErrc)
    ,m_ErrMsg(ParserErrorMsg::Instance())
  {
    m_strMsg = m_ErrMsg[m_iErrc];
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
    :m_strMsg()
    ,m_strFormula()
    ,m_strTok(sTok)
    ,m_iPos(iPos)
    ,m_iErrc(iErrc)
    ,m_ErrMsg(ParserErrorMsg::Instance())
  {
    m_strMsg = m_ErrMsg[m_iErrc];
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
    :m_strMsg(szMsg)
    ,m_strFormula()
    ,m_strTok(sTok)
    ,m_iPos(iPos)
    ,m_iErrc(ecGENERIC)
    ,m_ErrMsg(ParserErrorMsg::Instance())
  {
    stringstream_type stream;
    stream << (int)m_iPos;
    ReplaceSubString(m_strMsg, _T("$POS$"), stream.str());
    ReplaceSubString(m_strMsg, _T("$TOK$"), m_strTok);
  }

  //------------------------------------------------------------------------------
  /** \brief Copy constructor. */
  ParserError::ParserError(const ParserError &a_Obj)
    :m_strMsg(a_Obj.m_strMsg)
    ,m_strFormula(a_Obj.m_strFormula)
    ,m_strTok(a_Obj.m_strTok)
    ,m_iPos(a_Obj.m_iPos)
    ,m_iErrc(a_Obj.m_iErrc)
    ,m_ErrMsg(ParserErrorMsg::Instance())
  {
  }

  //------------------------------------------------------------------------------
  /** \brief Assignment operator. */
  ParserError& ParserError::operator=(const ParserError &a_Obj)
  {
    if (this==&a_Obj)
      return *this;

    m_strMsg = a_Obj.m_strMsg;
    m_strFormula = a_Obj.m_strFormula;
    m_strTok = a_Obj.m_strTok;
    m_iPos = a_Obj.m_iPos;
    m_iErrc = a_Obj.m_iErrc;
    return *this;
  }

  //------------------------------------------------------------------------------
  ParserError::~ParserError()
  {}

  //------------------------------------------------------------------------------
  /** \brief Replace all occurrences of a substring with another string. 
      \param strFind The string that shall be replaced.
      \param strReplaceWith The string that should be inserted instead of strFind
  */
  void ParserError::ReplaceSubString( string_type &strSource,
                                      const string_type &strFind,
                                      const string_type &strReplaceWith)
  {
    string_type strResult;
    string_type::size_type iPos(0), iNext(0);

    for(;;)
    {
      iNext = strSource.find(strFind, iPos);
      strResult.append(strSource, iPos, iNext-iPos);

      if( iNext==string_type::npos )
        break;

      strResult.append(strReplaceWith);
      iPos = iNext + strFind.length();
    } 

    strSource.swap(strResult);
  }

  //------------------------------------------------------------------------------
  /** \brief Reset the erro object. */
  void ParserError::Reset()
  {
    m_strMsg = _T("");
    m_strFormula = _T("");
    m_strTok = _T("");
    m_iPos = -1;
    m_iErrc = ecUNDEFINED;
  }
      
  //------------------------------------------------------------------------------
  /** \brief Set the expression related to this error. */
  void ParserError::SetFormula(const string_type &a_strFormula)
  {
    m_strFormula = a_strFormula;
  }

  //------------------------------------------------------------------------------
  /** \brief gets the expression related tp this error.*/
  const string_type& ParserError::GetExpr() const 
  {
    return m_strFormula;
  }

  //------------------------------------------------------------------------------
  /** \brief Returns the message string for this error. */
  const string_type& ParserError::GetMsg() const
  {
    return m_strMsg;
  }

  //------------------------------------------------------------------------------
  /** \brief Return the formula position related to the error. 

    If the error is not related to a distinct position this will return -1
  */
  int ParserError::GetPos() const
  {
    return m_iPos;
  }

  //------------------------------------------------------------------------------
  /** \brief Return string related with this token (if available). */
  const string_type& ParserError::GetToken() const
  {
    return m_strTok;
  }

  //------------------------------------------------------------------------------
  /** \brief Return the error code. */
  EErrorCodes ParserError::GetCode() const
  {
    return m_iErrc;
  }
} // namespace mu
