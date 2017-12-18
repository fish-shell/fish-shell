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

#include "muParserBase.h"

//--- Standard includes ------------------------------------------------------------------------
#include <algorithm>
#include <cassert>
#include <cmath>
#include <deque>
#include <locale>
#include <memory>
#include <sstream>
#include <vector>

using namespace std;

/** \file
    \brief This file contains the basic implementation of the muparser engine.
*/

namespace mu {
std::locale ParserBase::s_locale =
    std::locale(std::locale::classic(), new change_dec_sep<char_type>('.'));

bool ParserBase::g_DbgDumpCmdCode = false;
bool ParserBase::g_DbgDumpStack = false;

//------------------------------------------------------------------------------
/** \brief Identifiers for built in binary operators.

    When defining custom binary operators with #AddOprt(...) make sure not to choose
    names conflicting with these definitions.
*/
const char_type *ParserBase::c_DefaultOprt[] = {
    _T("<="), _T(">="), _T("!="), _T("=="), _T("<"), _T(">"), _T("+"), _T("-"), _T("*"), _T("/"),
    _T("^"),  _T("&&"), _T("||"), _T("="),  _T("("), _T(")"), _T("?"), _T(":"), 0};

//------------------------------------------------------------------------------
/** \brief Constructor.
    \param a_szFormula the formula to interpret.
    \throw ParserException if a_szFormula is null.
*/
ParserBase::ParserBase() : m_pTokenReader(new ParserTokenReader(this)) {}

//---------------------------------------------------------------------------
ParserBase::~ParserBase() = default;

//---------------------------------------------------------------------------
/** \brief Set the decimal separator.
    \param cDecSep Decimal separator as a character value.
    \sa SetThousandsSep

    By default muparser uses the "C" locale. The decimal separator of this
    locale is overwritten by the one provided here.
*/
void ParserBase::SetDecSep(char_type cDecSep) {
    char_type cThousandsSep = std::use_facet<change_dec_sep<char_type> >(s_locale).thousands_sep();
    s_locale = std::locale(std::locale("C"), new change_dec_sep<char_type>(cDecSep, cThousandsSep));
}

//---------------------------------------------------------------------------
/** \brief Sets the thousands operator.
    \param cThousandsSep The thousands separator as a character
    \sa SetDecSep

    By default muparser uses the "C" locale. The thousands separator of this
    locale is overwritten by the one provided here.
*/
void ParserBase::SetThousandsSep(char_type cThousandsSep) {
    char_type cDecSep = std::use_facet<change_dec_sep<char_type> >(s_locale).decimal_point();
    s_locale = std::locale(std::locale("C"), new change_dec_sep<char_type>(cDecSep, cThousandsSep));
}

//---------------------------------------------------------------------------
/** \brief Resets the locale.

  The default locale used "." as decimal separator, no thousands separator and
  "," as function argument separator.
*/
void ParserBase::ResetLocale() {
    s_locale = std::locale(std::locale("C"), new change_dec_sep<char_type>('.'));
    SetArgSep(',');
}

//---------------------------------------------------------------------------
/** \brief Reset parser to string parsing mode and clear internal buffers.

    Clear bytecode, reset the token reader.
*/
void ParserBase::ReInit() const {
    m_vStringBuf.clear();
    m_vRPN.clear();
    m_pTokenReader->ReInit();
}

//---------------------------------------------------------------------------
void ParserBase::OnDetectVar(string_type * /*pExpr*/, int & /*nStart*/, int & /*nEnd*/) {}

//---------------------------------------------------------------------------
/** \brief Add a value parsing function.

    When parsing an expression muParser tries to detect values in the expression
    string using different valident callbacks. Thus it's possible to parse
    for hex values, binary values and floating point values.
*/
void ParserBase::AddValIdent(identfun_type a_pCallback) {
    m_pTokenReader->AddValIdent(a_pCallback);
}

//---------------------------------------------------------------------------
/** \brief Set a function that can create variable pointer for unknown expression variables.
    \param a_pFactory A pointer to the variable factory.
    \param pUserData A user defined context pointer.
*/
void ParserBase::SetVarFactory(facfun_type a_pFactory, void *pUserData) {
    m_pTokenReader->SetVarCreator(a_pFactory, pUserData);
}

//---------------------------------------------------------------------------
/** \brief Add a function or operator callback to the parser. */
OptionalError ParserBase::AddCallback(const string_type &a_strName,
                                      const ParserCallback &a_Callback, funmap_type &a_Storage,
                                      const char_type *a_szCharSet) {
    assert(a_Callback.GetAddr() && "Null callback");

    const funmap_type *pFunMap = &a_Storage;

    // Check for conflicting operator or function names
    if (pFunMap != &m_FunDef && m_FunDef.find(a_strName) != m_FunDef.end())
        return Error(ecNAME_CONFLICT, -1, a_strName);

    if (pFunMap != &m_PostOprtDef && m_PostOprtDef.find(a_strName) != m_PostOprtDef.end())
        return Error(ecNAME_CONFLICT, -1, a_strName);

    if (pFunMap != &m_InfixOprtDef && pFunMap != &m_OprtDef &&
        m_InfixOprtDef.find(a_strName) != m_InfixOprtDef.end())
        return Error(ecNAME_CONFLICT, -1, a_strName);

    if (pFunMap != &m_InfixOprtDef && pFunMap != &m_OprtDef &&
        m_OprtDef.find(a_strName) != m_OprtDef.end())
        return Error(ecNAME_CONFLICT, -1, a_strName);

    OptionalError oerr = CheckOprt(a_strName, a_Callback, a_szCharSet);
    if (oerr.has_error()) return oerr;
    a_Storage[a_strName] = a_Callback;
    ReInit();
    return {};
}

//---------------------------------------------------------------------------
/** \brief Check if a name contains invalid characters.

    \throw ParserException if the name contains invalid characters.
*/
OptionalError ParserBase::CheckOprt(const string_type &a_sName, const ParserCallback &a_Callback,
                                    const string_type &a_szCharSet) const {
    if (!a_sName.length() || (a_sName.find_first_not_of(a_szCharSet) != string_type::npos) ||
        (a_sName[0] >= '0' && a_sName[0] <= '9')) {
        switch (a_Callback.GetCode()) {
            case cmOPRT_POSTFIX:
                return Error(ecINVALID_POSTFIX_IDENT, -1, a_sName);
            case cmOPRT_INFIX:
                return Error(ecINVALID_INFIX_IDENT, -1, a_sName);
            default:
                return Error(ecINVALID_NAME, -1, a_sName);
        }
    }
    return {};
}

//---------------------------------------------------------------------------
/** \brief Check if a name contains invalid characters.

    \throw ParserException if the name contains invalid characters.
*/
OptionalError ParserBase::CheckName(const string_type &a_sName,
                                    const string_type &a_szCharSet) const {
    if (!a_sName.length() || (a_sName.find_first_not_of(a_szCharSet) != string_type::npos) ||
        (a_sName[0] >= '0' && a_sName[0] <= '9')) {
        return Error(ecINVALID_NAME);
    }
    return {};
}

//---------------------------------------------------------------------------
/** \brief Set the formula.
    \param a_strFormula Formula as string_type
    \throw ParserException in case of syntax errors.

    Triggers first time calculation thus the creation of the bytecode and
    scanning of used variables.
*/
OptionalError ParserBase::SetExpr(const string_type &a_sExpr) {
    // Check locale compatibility
    std::locale loc;
    if (m_pTokenReader->GetArgSep() == std::use_facet<numpunct<char_type> >(loc).decimal_point())
        return Error(ecLOCALE);

    // <ibg> 20060222: Bugfix for Borland-Kylix:
    // adding a space to the expression will keep Borlands KYLIX from going wild
    // when calling tellg on a stringstream created from the expression after
    // reading a value at the end of an expression. (mu::Parser::IsVal function)
    // (tellg returns -1 otherwise causing the parser to ignore the value)
    string_type sBuf(a_sExpr + _T(" "));
    m_pTokenReader->SetFormula(sBuf);
    ReInit();
    return {};
}

//---------------------------------------------------------------------------
/** \brief Get the default symbols used for the built in operators.
    \sa c_DefaultOprt
*/
const char_type **ParserBase::GetOprtDef() const { return (const char_type **)(&c_DefaultOprt[0]); }

//---------------------------------------------------------------------------
/** \brief Define the set of valid characters to be used in names of
           functions, variables, constants.
*/
void ParserBase::DefineNameChars(const char_type *a_szCharset) { m_sNameChars = a_szCharset; }

//---------------------------------------------------------------------------
/** \brief Define the set of valid characters to be used in names of
           binary operators and postfix operators.
*/
void ParserBase::DefineOprtChars(const char_type *a_szCharset) { m_sOprtChars = a_szCharset; }

//---------------------------------------------------------------------------
/** \brief Define the set of valid characters to be used in names of
           infix operators.
*/
void ParserBase::DefineInfixOprtChars(const char_type *a_szCharset) {
    m_sInfixOprtChars = a_szCharset;
}

//---------------------------------------------------------------------------
/** \brief Virtual function that defines the characters allowed in name identifiers.
    \sa #ValidOprtChars, #ValidPrefixOprtChars
*/
const char_type *ParserBase::ValidNameChars() const {
    assert(m_sNameChars.size());
    return m_sNameChars.c_str();
}

//---------------------------------------------------------------------------
/** \brief Virtual function that defines the characters allowed in operator definitions.
    \sa #ValidNameChars, #ValidPrefixOprtChars
*/
const char_type *ParserBase::ValidOprtChars() const {
    assert(m_sOprtChars.size());
    return m_sOprtChars.c_str();
}

//---------------------------------------------------------------------------
/** \brief Virtual function that defines the characters allowed in infix operator definitions.
    \sa #ValidNameChars, #ValidOprtChars
*/
const char_type *ParserBase::ValidInfixOprtChars() const {
    assert(m_sInfixOprtChars.size());
    return m_sInfixOprtChars.c_str();
}

//---------------------------------------------------------------------------
/** \brief Add a user defined operator.
    \post Will reset the Parser to string parsing mode.
*/
OptionalError ParserBase::DefinePostfixOprt(const string_type &a_sName, fun_type1 a_pFun) {
    return AddCallback(a_sName, ParserCallback(a_pFun, prPOSTFIX, cmOPRT_POSTFIX), m_PostOprtDef,
                       ValidOprtChars());
}

//---------------------------------------------------------------------------
/** \brief Initialize user defined functions.

  Calls the virtual functions InitFun(), InitConst() and InitOprt().
*/
void ParserBase::Init() {
    InitCharSets();
    InitFun();
    InitConst();
    InitOprt();
}

//---------------------------------------------------------------------------
/** \brief Add a user defined operator.
    \post Will reset the Parser to string parsing mode.
    \param [in] a_sName  operator Identifier
    \param [in] a_pFun  Operator callback function
    \param [in] a_iPrec  Operator Precedence (default=prSIGN)
    \sa EPrec
*/
OptionalError ParserBase::DefineInfixOprt(const string_type &a_sName, fun_type1 a_pFun,
                                          int a_iPrec) {
    return AddCallback(a_sName, ParserCallback(a_pFun, a_iPrec, cmOPRT_INFIX), m_InfixOprtDef,
                       ValidInfixOprtChars());
}

//---------------------------------------------------------------------------
/** \brief Define a binary operator.
    \param [in] a_sName The identifier of the operator.
    \param [in] a_pFun Pointer to the callback function.
    \param [in] a_iPrec Precedence of the operator.
    \param [in] a_eAssociativity The associativity of the operator.

    Adds a new Binary operator the the parser instance.
*/
OptionalError ParserBase::DefineOprt(const string_type &a_sName, fun_type2 a_pFun, unsigned a_iPrec,
                                     EOprtAssociativity a_eAssociativity) {
    // Check for conflicts with built in operator names
    for (int i = 0; m_bBuiltInOp && i < cmENDIF; ++i)
        if (a_sName == string_type(c_DefaultOprt[i])) return Error(ecBUILTIN_OVERLOAD, -1, a_sName);

    return AddCallback(a_sName, ParserCallback(a_pFun, a_iPrec, a_eAssociativity), m_OprtDef,
                       ValidOprtChars());
}

//---------------------------------------------------------------------------
/** \brief Define a new string constant.
    \param [in] a_strName The name of the constant.
    \param [in] a_strVal the value of the constant.
*/
OptionalError ParserBase::DefineStrConst(const string_type &a_strName,
                                         const string_type &a_strVal) {
    // Test if a constant with that names already exists
    if (m_StrVarDef.find(a_strName) != m_StrVarDef.end()) return Error(ecNAME_CONFLICT);

    OptionalError oerr = CheckName(a_strName, ValidNameChars());
    if (oerr.has_error()) return oerr;

    m_vStringVarBuf.push_back(a_strVal);  // Store variable string in internal buffer
    m_StrVarDef[a_strName] = m_vStringVarBuf.size() - 1;  // bind buffer index to variable name

    ReInit();
    return {};
}

//---------------------------------------------------------------------------
/** \brief Add a user defined variable.
    \param [in] a_sName the variable name
    \param [in] a_pVar A pointer to the variable value.
    \post Will reset the Parser to string parsing mode.
    \throw ParserException in case the name contains invalid signs or a_pVar is NULL.
*/
OptionalError ParserBase::DefineVar(const string_type &a_sName, value_type *a_pVar) {
    assert(a_pVar != nullptr && "Null variable pointer");

    // Test if a constant with that names already exists
    if (m_ConstDef.find(a_sName) != m_ConstDef.end()) return Error(ecNAME_CONFLICT);

    OptionalError oerr = CheckName(a_sName, ValidNameChars());
    if (oerr.has_error()) return oerr;

    m_VarDef[a_sName] = a_pVar;
    ReInit();
    return {};
}

//---------------------------------------------------------------------------
/** \brief Add a user defined constant.
    \param [in] a_sName The name of the constant.
    \param [in] a_fVal the value of the constant.
    \post Will reset the Parser to string parsing mode.
    \throw ParserException in case the name contains invalid signs.
*/
OptionalError ParserBase::DefineConst(const string_type &a_sName, value_type a_fVal) {
    OptionalError oerr = CheckName(a_sName, ValidNameChars());
    if (oerr.has_error()) return oerr;
    m_ConstDef[a_sName] = a_fVal;
    ReInit();
    return {};
}

//---------------------------------------------------------------------------
/** \brief Get operator priority.
    \throw ParserException if a_Oprt is no operator code
*/
int ParserBase::GetOprtPrecedence(const token_type &a_Tok) const {
    switch (a_Tok.GetCode()) {
        // built in operators
        case cmEND:
            return -5;
        case cmARG_SEP:
            return -4;
        case cmASSIGN:
            return -1;
        case cmELSE:
        case cmIF:
            return 0;
        case cmLAND:
            return prLAND;
        case cmLOR:
            return prLOR;
        case cmLT:
        case cmGT:
        case cmLE:
        case cmGE:
        case cmNEQ:
        case cmEQ:
            return prCMP;
        case cmADD:
        case cmSUB:
            return prADD_SUB;
        case cmMUL:
        case cmDIV:
            return prMUL_DIV;
        case cmPOW:
            return prPOW;

        // user defined binary operators
        case cmOPRT_INFIX:
        case cmOPRT_BIN:
            return a_Tok.GetPri();
        default:
            assert(0 && "Unexpected operator in muParser");
            return 999;
    }
}

//---------------------------------------------------------------------------
/** \brief Get operator priority.
    \throw ParserException if a_Oprt is no operator code
*/
EOprtAssociativity ParserBase::GetOprtAssociativity(const token_type &a_Tok) const {
    switch (a_Tok.GetCode()) {
        case cmASSIGN:
        case cmLAND:
        case cmLOR:
        case cmLT:
        case cmGT:
        case cmLE:
        case cmGE:
        case cmNEQ:
        case cmEQ:
        case cmADD:
        case cmSUB:
        case cmMUL:
        case cmDIV:
            return oaLEFT;
        case cmPOW:
            return oaRIGHT;
        case cmOPRT_BIN:
            return a_Tok.GetAssociativity();
        default:
            return oaNONE;
    }
}

//---------------------------------------------------------------------------
/** \brief Retrieve the formula. */
const string_type &ParserBase::GetExpr() const { return m_pTokenReader->GetExpr(); }

//---------------------------------------------------------------------------
/** \brief Execute a function that takes a single string argument.
    \param a_FunTok Function token.
    \throw exception_type If the function token is not a string function
*/
OptionalError ParserBase::ApplyStrFunc(const token_type &a_FunTok,
                                       const std::vector<token_type> &a_vArg) const {
    if (a_vArg.back().GetCode() != cmSTRING)
        return Error(ecSTRING_EXPECTED, m_pTokenReader->GetPos(), a_FunTok.GetAsString());

    token_type valTok;
    generic_fun_type pFunc = a_FunTok.GetFuncAddr();
    assert(pFunc);
    bool errored = false;
    // Check function arguments; write dummy value into valtok to represent the result
    switch (a_FunTok.GetArgCount()) {
        case 0:
            valTok.SetVal(1);
            a_vArg[0].GetAsString();
            break;
        case 1:
            valTok.SetVal(1);
            a_vArg[1].GetAsString();
            errored |= a_vArg[0].GetVal().has_error();
            break;
        case 2:
            valTok.SetVal(1);
            a_vArg[2].GetAsString();
            errored |= a_vArg[1].GetVal().has_error();
            errored |= a_vArg[0].GetVal().has_error();
            break;
        default:
            assert(0 && "Unexpected arg count");
    }
    if (errored) {
        return Error(ecVAL_EXPECTED, m_pTokenReader->GetPos(), a_FunTok.GetAsString());
    }

    m_vRPN.AddStrFun(pFunc, a_FunTok.GetArgCount(), a_vArg.back().GetIdx());
    return {};
}

//---------------------------------------------------------------------------
/** \brief Apply a function token.
    \param iArgCount Number of Arguments actually gathered used only for multiarg functions.
    \post The result is pushed to the value stack
    \post The function token is removed from the stack
    \return ParserError if Argument count does not match function requirements.
*/
OptionalError ParserBase::ApplyFunc(ParserStack<token_type> &a_stOpt,
                                    ParserStack<token_type> &a_stVal, int a_iArgCount) const {
    assert(m_pTokenReader.get());

    // Operator stack empty or does not contain tokens with callback functions
    if (a_stOpt.empty() || a_stOpt.top().GetFuncAddr() == 0) return {};

    token_type funTok = a_stOpt.pop();
    assert(funTok.GetFuncAddr());

    // Binary operators must rely on their internal operator number
    // since counting of operators relies on commas for function arguments
    // binary operators do not have commas in their expression
    int iArgCount = (funTok.GetCode() == cmOPRT_BIN) ? funTok.GetArgCount() : a_iArgCount;

    // determine how many parameters the function needs. To remember iArgCount includes the
    // string parameter whilst GetArgCount() counts only numeric parameters.
    int iArgRequired = funTok.GetArgCount() + ((funTok.GetType() == tpSTR) ? 1 : 0);

    // Thats the number of numerical parameters
    int iArgNumerical = iArgCount - ((funTok.GetType() == tpSTR) ? 1 : 0);

    if (funTok.GetCode() == cmFUNC_STR && iArgCount - iArgNumerical > 1)
        assert(0 && "muParser internal error");

    if (funTok.GetArgCount() >= 0 && iArgCount > iArgRequired)
        return Error(ecTOO_MANY_PARAMS, m_pTokenReader->GetPos() - 1, funTok.GetAsString());

    if (funTok.GetCode() != cmOPRT_BIN && iArgCount < iArgRequired)
        return Error(ecTOO_FEW_PARAMS, m_pTokenReader->GetPos() - 1, funTok.GetAsString());

    if (funTok.GetCode() == cmFUNC_STR && iArgCount > iArgRequired)
        return Error(ecTOO_MANY_PARAMS, m_pTokenReader->GetPos() - 1, funTok.GetAsString());

    // Collect the numeric function arguments from the value stack and store them
    // in a vector
    std::vector<token_type> stArg;
    for (int i = 0; i < iArgNumerical; ++i) {
        stArg.push_back(a_stVal.pop());
        if (stArg.back().GetType() == tpSTR && funTok.GetType() != tpSTR)
            return Error(ecVAL_EXPECTED, m_pTokenReader->GetPos(), funTok.GetAsString());
    }

    switch (funTok.GetCode()) {
        case cmFUNC_STR: {
            stArg.push_back(a_stVal.pop());

            if (stArg.back().GetType() == tpSTR && funTok.GetType() != tpSTR)
                return Error(ecVAL_EXPECTED, m_pTokenReader->GetPos(), funTok.GetAsString());

            OptionalError err = ApplyStrFunc(funTok, stArg);
            if (err.has_error()) return err;
            break;
        }

        case cmOPRT_BIN:
        case cmOPRT_POSTFIX:
        case cmOPRT_INFIX:
        case cmFUNC:
            if (funTok.GetArgCount() == -1 && iArgCount == 0)
                return Error(ecTOO_FEW_PARAMS, m_pTokenReader->GetPos(), funTok.GetAsString());

            m_vRPN.AddFun(funTok.GetFuncAddr(),
                          (funTok.GetArgCount() == -1) ? -iArgNumerical : iArgNumerical);
            break;
        default:
            assert(0 && "Unexpected function token");
            break;
    }

    // Push dummy value representing the function result to the stack
    token_type token;
    token.SetVal(1);
    a_stVal.push(token);
    return {};
}

//---------------------------------------------------------------------------
OptionalError ParserBase::ApplyIfElse(ParserStack<token_type> &a_stOpt,
                                      ParserStack<token_type> &a_stVal) const {
    // Check if there is an if Else clause to be calculated
    while (a_stOpt.size() && a_stOpt.top().GetCode() == cmELSE) {
        token_type opElse = a_stOpt.pop();
        assert(a_stOpt.size() > 0 && "Invalid if/else clause");

        // Take the value associated with the else branch from the value stack
        token_type vVal2 = a_stVal.pop();

        assert(a_stOpt.size() > 0 && "Invalid if/else clause");
        assert(a_stVal.size() >= 2 && "Invalid if/else clause");

        // it then else is a ternary operator Pop all three values from the value s
        // tack and just return the right value
        token_type vVal1 = a_stVal.pop();
        token_type vExpr = a_stVal.pop();

        ValueOrError vExprValue = vExpr.GetVal();
        if (vExprValue.has_error()) return vExprValue.error();
        a_stVal.push((vExprValue.value() != 0) ? vVal1 : vVal2);

        token_type opIf = a_stOpt.pop();
        assert(opElse.GetCode() == cmELSE && "Invalid if/else clause");
        assert(opIf.GetCode() == cmIF && "Invalid if/else clause");

        m_vRPN.AddIfElse(cmENDIF);
    }  // while pending if-else-clause found
    return {};
}

//---------------------------------------------------------------------------
/** \brief Performs the necessary steps to write code for
           the execution of binary operators into the bytecode.
*/
OptionalError ParserBase::ApplyBinOprt(ParserStack<token_type> &a_stOpt,
                                       ParserStack<token_type> &a_stVal) const {
    // is it a user defined binary operator?
    if (a_stOpt.top().GetCode() == cmOPRT_BIN) {
        return ApplyFunc(a_stOpt, a_stVal, 2);
    } else {
        assert(a_stVal.size() >= 2 && "Too few arguments for binary operator");
        token_type valTok1 = a_stVal.pop(), valTok2 = a_stVal.pop(), optTok = a_stOpt.pop(), resTok;

        if (valTok1.GetType() != valTok2.GetType() ||
            (valTok1.GetType() == tpSTR && valTok2.GetType() == tpSTR))
            return Error(ecOPRT_TYPE_CONFLICT, m_pTokenReader->GetPos(), optTok.GetAsString());

        if (optTok.GetCode() == cmASSIGN) {
            if (valTok2.GetCode() != cmVAR) return Error(ecUNEXPECTED_OPERATOR, -1, _T("="));

            m_vRPN.AddAssignOp(valTok2.GetVar());
        } else
            m_vRPN.AddOp(optTok.GetCode());

        resTok.SetVal(1);
        a_stVal.push(resTok);
    }
    return {};
}

//---------------------------------------------------------------------------
/** \brief Apply a binary operator.
    \param a_stOpt The operator stack
    \param a_stVal The value stack
*/
OptionalError ParserBase::ApplyRemainingOprt(ParserStack<token_type> &stOpt,
                                             ParserStack<token_type> &stVal) const {
    while (stOpt.size() && stOpt.top().GetCode() != cmBO && stOpt.top().GetCode() != cmIF) {
        OptionalError oerr;
        token_type tok = stOpt.top();
        switch (tok.GetCode()) {
            case cmOPRT_INFIX:
            case cmOPRT_BIN:
            case cmLE:
            case cmGE:
            case cmNEQ:
            case cmEQ:
            case cmLT:
            case cmGT:
            case cmADD:
            case cmSUB:
            case cmMUL:
            case cmDIV:
            case cmPOW:
            case cmLAND:
            case cmLOR:
            case cmASSIGN:
                if (stOpt.top().GetCode() == cmOPRT_INFIX)
                    oerr = ApplyFunc(stOpt, stVal, 1);
                else
                    oerr = ApplyBinOprt(stOpt, stVal);
                break;

            case cmELSE:
                oerr = ApplyIfElse(stOpt, stVal);
                break;

            default:
                assert(0 && "muParser internal error");
        }
        if (oerr.has_error()) return oerr.error();
    }
    return {};
}

/// Invoke the function \p func as the fun_typeN given argCount, passing in \p argCount arguments
/// starting at \p args.
ValueOrError ParserBase::InvokeFunction(generic_fun_type func, const value_type *args,
                                        int argCount) const {
    assert(0 <= argCount && argCount <= 10 && "Invalid arg count");
    switch (argCount) {
        case 0:
            return ((fun_type0)func)();
        case 1:
            return ((fun_type1)func)(args[0]);
        case 2:
            return ((fun_type2)func)(args[0], args[1]);
        case 3:
            return ((fun_type3)func)(args[0], args[1], args[2]);
        default:
            // Unreachable.
            assert(0 && "Internal error");
            return 0;
    }
}

//---------------------------------------------------------------------------
/** \brief Execute the RPN.

    Command code contains precalculated stack positions of the values and the
    associated operators. The Stack is filled beginning from index one the
    value at index zero is not used at all.
*/
ValueOrError ParserBase::ExecuteRPN() const {
    assert(! m_vRPN.empty() && "Missing RPN");
    value_type *Stack = &m_vStackBuffer[0];
    int sidx(0);
    for (const SToken *pTok = m_vRPN.GetBase(); pTok->Cmd != cmEND; ++pTok) {
        switch (pTok->Cmd) {
            // built in binary operators
            case cmLE:
                --sidx;
                Stack[sidx] = Stack[sidx] <= Stack[sidx + 1];
                continue;
            case cmGE:
                --sidx;
                Stack[sidx] = Stack[sidx] >= Stack[sidx + 1];
                continue;
            case cmNEQ:
                --sidx;
                Stack[sidx] = Stack[sidx] != Stack[sidx + 1];
                continue;
            case cmEQ:
                --sidx;
                Stack[sidx] = Stack[sidx] == Stack[sidx + 1];
                continue;
            case cmLT:
                --sidx;
                Stack[sidx] = Stack[sidx] < Stack[sidx + 1];
                continue;
            case cmGT:
                --sidx;
                Stack[sidx] = Stack[sidx] > Stack[sidx + 1];
                continue;
            case cmADD:
                --sidx;
                Stack[sidx] += Stack[1 + sidx];
                continue;
            case cmSUB:
                --sidx;
                Stack[sidx] -= Stack[1 + sidx];
                continue;
            case cmMUL:
                --sidx;
                Stack[sidx] *= Stack[1 + sidx];
                continue;
            case cmDIV:
                --sidx;

#if defined(MUP_MATH_EXCEPTIONS)
                if (Stack[1 + sidx] == 0) Error(ecDIV_BY_ZERO);
#endif
                Stack[sidx] /= Stack[1 + sidx];
                continue;

            case cmPOW:
                --sidx;
                Stack[sidx] = std::pow(Stack[sidx], Stack[1 + sidx]);
                continue;

            case cmLAND:
                --sidx;
                Stack[sidx] = Stack[sidx] && Stack[sidx + 1];
                continue;
            case cmLOR:
                --sidx;
                Stack[sidx] = Stack[sidx] || Stack[sidx + 1];
                continue;

            case cmASSIGN:
                // Bugfix for Bulkmode:
                // for details see:
                //    https://groups.google.com/forum/embed/?place=forum/muparser-dev&showsearch=true&showpopout=true&showtabs=false&parenturl=http://muparser.beltoforion.de/mup_forum.html&afterlogin&pli=1#!topic/muparser-dev/szgatgoHTws
                --sidx;
                Stack[sidx] = *(pTok->Oprt.ptr) = Stack[sidx + 1];
                continue;
            // original code:
            //--sidx; Stack[sidx] = *pTok->Oprt.ptr = Stack[sidx+1]; continue;

            // case  cmBO:  // unused, listed for compiler optimization purposes
            // case  cmBC:
            //      MUP_FAIL(INVALID_CODE_IN_BYTECODE);
            //      continue;

            case cmIF:
                if (Stack[sidx--] == 0) pTok += pTok->Oprt.offset;
                continue;

            case cmELSE:
                pTok += pTok->Oprt.offset;
                continue;

            case cmENDIF:
                continue;

            // case  cmARG_SEP:
            //      MUP_FAIL(INVALID_CODE_IN_BYTECODE);
            //      continue;

            // value and variable tokens
            case cmVAR:
                Stack[++sidx] = *(pTok->Val.ptr);
                continue;
            case cmVAL:
                Stack[++sidx] = pTok->Val.data;
                continue;

            // Next is treatment of numeric functions
            case cmFUNC: {
                int iArgCount = pTok->Fun.argc;
                ValueOrError funcResult{0.0};
                if (iArgCount >= 0) {
                    sidx -= iArgCount - 1;
                    funcResult = InvokeFunction(pTok->Fun.ptr, &Stack[sidx], iArgCount);
                } else {
                    // function with variable arguments store the number as a negative value
                    sidx -= -iArgCount - 1;
                    funcResult = ((multfun_type)pTok->Fun.ptr)(&Stack[sidx], -iArgCount);
                }
                if (!funcResult) return funcResult;
                Stack[sidx] = *funcResult;
                continue;
            }

            // Next is treatment of string functions
            case cmFUNC_STR: {
                sidx -= pTok->Fun.argc - 1;

                // The index of the string argument in the string table
                int iIdxStack = pTok->Fun.idx;
                assert(iIdxStack >= 0 && iIdxStack < (int)m_vStringBuf.size() &&
                       "Invalid string index");
                ValueOrError funcResult{0.0};
                switch (pTok->Fun.argc)  // switch according to argument count
                {
                    case 0:
                        funcResult = ((strfun_type1)pTok->Fun.ptr)(m_vStringBuf[iIdxStack].c_str());
                        break;
                    case 1:
                        funcResult = ((strfun_type2)pTok->Fun.ptr)(m_vStringBuf[iIdxStack].c_str(),
                                                                   Stack[sidx]);
                        break;
                    case 2:
                        funcResult = ((strfun_type3)pTok->Fun.ptr)(m_vStringBuf[iIdxStack].c_str(),
                                                                   Stack[sidx], Stack[sidx + 1]);
                        break;
                }
                if (!funcResult) return funcResult;
                Stack[sidx] = *funcResult;
                continue;
            }

            default:
                assert(0 && "muParser internal error");
                return 0;
        }  // switch CmdCode
    }      // for all bytecode tokens

    return Stack[m_nFinalResultIdx];
}

//---------------------------------------------------------------------------
OptionalError ParserBase::CreateRPN() const {
    if (!m_pTokenReader->GetExpr().length()) return ParserError(ecUNEXPECTED_EOF, 0);
    int ifElseCounter = 0;

    ParserStack<token_type> stOpt, stVal;
    ParserStack<int> stArgCount;
    token_type opta, opt;  // for storing operators
    token_type val, tval;  // for storing value

    ReInit();

    // The outermost counter counts the number of separated items
    // such as in "a=10,b=20,c=c+a"
    stArgCount.push(1);

    for (;;) {
        opt = m_pTokenReader->ReadNextToken();
        OptionalError oerr = m_pTokenReader->acquireFirstError();
        if (oerr.has_error()) return oerr;

        switch (opt.GetCode()) {
            //
            // Next three are different kind of value entries
            //
            case cmSTRING:
                opt.SetIdx((int)m_vStringBuf.size());  // Assign buffer index to token
                stVal.push(opt);
                m_vStringBuf.push_back(opt.GetAsString());  // Store string in internal buffer
                break;

            case cmVAR:
                stVal.push(opt);
                m_vRPN.AddVar(static_cast<value_type *>(opt.GetVar()));
                break;

            case cmVAL: {
                stVal.push(opt);
                ValueOrError optVal = opt.GetVal();
                if (optVal.has_error()) return optVal.error();
                m_vRPN.AddVal(optVal.value());
                break;
            }

            case cmELSE:
                ifElseCounter--;
                if (ifElseCounter < 0) return Error(ecMISPLACED_COLON, m_pTokenReader->GetPos());

                oerr = ApplyRemainingOprt(stOpt, stVal);
                if (oerr.has_error()) return oerr.error();
                m_vRPN.AddIfElse(cmELSE);
                stOpt.push(opt);
                break;

            case cmARG_SEP:
                if (stArgCount.empty())
                    return Error(ecUNEXPECTED_ARG_SEP, m_pTokenReader->GetPos());
                ++stArgCount.top();
            // fallthrough intentional (no break!)

            case cmEND:
                oerr = ApplyRemainingOprt(stOpt, stVal);
                if (oerr.has_error()) return oerr.error();
                break;

            case cmBC: {
                // The argument count for parameterless functions is zero
                // by default an opening bracket sets parameter count to 1
                // in preparation of arguments to come. If the last token
                // was an opening bracket we know better...
                if (opta.GetCode() == cmBO) --stArgCount.top();

                oerr = ApplyRemainingOprt(stOpt, stVal);
                if (oerr.has_error()) return oerr.error();

                // Check if the bracket content has been evaluated completely
                if (stOpt.size() && stOpt.top().GetCode() == cmBO) {
                    // if opt is ")" and opta is "(" the bracket has been evaluated, now its time to
                    // check
                    // if there is either a function or a sign pending
                    // neither the opening nor the closing bracket will be pushed back to
                    // the operator stack
                    // Check if a function is standing in front of the opening bracket,
                    // if yes evaluate it afterwards check for infix operators
                    assert(stArgCount.size());
                    int iArgCount = stArgCount.pop();

                    stOpt.pop();  // Take opening bracket from stack

                    if (iArgCount > 1 &&
                        (stOpt.size() == 0 ||
                         (stOpt.top().GetCode() != cmFUNC && stOpt.top().GetCode() != cmFUNC_STR)))
                        return Error(ecUNEXPECTED_ARG, m_pTokenReader->GetPos());

                    // The opening bracket was popped from the stack now check if there
                    // was a function before this bracket
                    if (stOpt.size() && stOpt.top().GetCode() != cmOPRT_INFIX &&
                        stOpt.top().GetCode() != cmOPRT_BIN && stOpt.top().GetFuncAddr() != 0) {
                        OptionalError err = ApplyFunc(stOpt, stVal, iArgCount);
                        if (err.has_error()) return err.error();
                    }
                }
            }  // if bracket content is evaluated
            break;

            //
            // Next are the binary operator entries
            //
            // case cmAND:   // built in binary operators
            // case cmOR:
            // case cmXOR:
            case cmIF:
                ifElseCounter++;
            // fallthrough intentional (no break!)

            case cmLAND:
            case cmLOR:
            case cmLT:
            case cmGT:
            case cmLE:
            case cmGE:
            case cmNEQ:
            case cmEQ:
            case cmADD:
            case cmSUB:
            case cmMUL:
            case cmDIV:
            case cmPOW:
            case cmASSIGN:
            case cmOPRT_BIN:

                // A binary operator (user defined or built in) has been found.
                while (stOpt.size() && stOpt.top().GetCode() != cmBO &&
                       stOpt.top().GetCode() != cmELSE && stOpt.top().GetCode() != cmIF) {
                    int nPrec1 = GetOprtPrecedence(stOpt.top()), nPrec2 = GetOprtPrecedence(opt);

                    if (stOpt.top().GetCode() == opt.GetCode()) {
                        // Deal with operator associativity
                        EOprtAssociativity eOprtAsct = GetOprtAssociativity(opt);
                        if ((eOprtAsct == oaRIGHT && (nPrec1 <= nPrec2)) ||
                            (eOprtAsct == oaLEFT && (nPrec1 < nPrec2))) {
                            break;
                        }
                    } else if (nPrec1 < nPrec2) {
                        // In case the operators are not equal the precedence decides alone...
                        break;
                    }

                    OptionalError oerr;
                    if (stOpt.top().GetCode() == cmOPRT_INFIX)
                        oerr = ApplyFunc(stOpt, stVal, 1);
                    else
                        oerr = ApplyBinOprt(stOpt, stVal);
                    if (oerr.has_error()) {
                        return oerr.error();
                    }
                }  // while ( ... )

                if (opt.GetCode() == cmIF) m_vRPN.AddIfElse(opt.GetCode());

                // The operator can't be evaluated right now, push back to the operator stack
                stOpt.push(opt);
                break;

            //
            // Last section contains functions and operators implicitly mapped to functions
            //
            case cmBO:
                stArgCount.push(1);
                stOpt.push(opt);
                break;

            case cmOPRT_INFIX:
            case cmFUNC:
            case cmFUNC_STR:
                stOpt.push(opt);
                break;

            case cmOPRT_POSTFIX: {
                stOpt.push(opt);
                OptionalError oerr = ApplyFunc(stOpt, stVal, 1);  // this is the postfix operator
                if (oerr.has_error()) return oerr.error();
                break;
            }

            default:
                assert(0 && "muParser internal error");
        }  // end of switch operator-token

        opta = opt;

        if (opt.GetCode() == cmEND) {
            m_vRPN.Finalize();
            break;
        }

        if (ParserBase::g_DbgDumpStack) {
            StackDump(stVal, stOpt);
            m_vRPN.AsciiDump();
        }
    }  // while (true)

    if (ParserBase::g_DbgDumpCmdCode) m_vRPN.AsciiDump();

    if (ifElseCounter > 0) return Error(ecMISSING_ELSE_CLAUSE);

    // get the last value (= final result) from the stack
    assert(stArgCount.size() == 1 && "Expected arg count of 1");
    m_nFinalResultIdx = stArgCount.top();
    if (m_nFinalResultIdx == 0) assert(0 && "muParser internal error");

    if (stVal.size() == 0) return Error(ecEMPTY_EXPRESSION);

    if (stVal.top().GetType() != tpDBL) return Error(ecSTR_RESULT);

    m_vStackBuffer.resize(m_vRPN.GetMaxStackSize());
    return {};
}

ValueOrError ParserBase::BuildAndExecuteRPN() const {
  if (m_vRPN.empty()) {
    OptionalError oerr = CreateRPN();
    if (oerr.has_error()) return oerr.error();
    assert(! m_vRPN.empty() && "RPN should no longer be empty");
  }
  return ExecuteRPN();
}

//---------------------------------------------------------------------------
/** \brief Create an error containing the parse error position.

  This function will create an Parser Exception object containing the error text and
  its position.

  \param a_iErrc [in] The error code of type #EErrorCodes.
  \param a_iPos [in] The position where the error was detected.
  \param a_strTok [in] The token string representation associated with the error.
*/
ParserError ParserBase::Error(EErrorCodes a_iErrc, int a_iPos, const string_type &a_sTok) const {
    return ParserError(a_iErrc, a_sTok, a_iPos);
}

//------------------------------------------------------------------------------
/** \brief Remove a variable from internal storage.

    Removes a variable if it exists. If the Variable does not exist nothing will be done.
*/
void ParserBase::RemoveVar(const string_type &a_strVarName) {
    varmap_type::iterator item = m_VarDef.find(a_strVarName);
    if (item != m_VarDef.end()) {
        m_VarDef.erase(item);
        ReInit();
    }
}

//------------------------------------------------------------------------------
/** \brief Clear all user defined constants.

    Both numeric and string constants will be removed from the internal storage.
    \post Resets the parser to string parsing mode.
*/
void ParserBase::ClearConst() {
    m_ConstDef.clear();
    m_StrVarDef.clear();
    ReInit();
}

//------------------------------------------------------------------------------
/** \brief Clear all user defined postfix operators.
    \post Resets the parser to string parsing mode.
*/
void ParserBase::ClearPostfixOprt() {
    m_PostOprtDef.clear();
    ReInit();
}

//---------------------------------------------------------------------------
/** \brief Enable the dumping of bytecode and stack content on the console.
    \param bDumpCmd Flag to enable dumping of the current bytecode to the console.
    \param bDumpStack Flag to enable dumping of the stack content is written to the console.

   This function is for debug purposes only!
*/
void ParserBase::EnableDebugDump(bool bDumpCmd, bool bDumpStack) {
    ParserBase::g_DbgDumpCmdCode = bDumpCmd;
    ParserBase::g_DbgDumpStack = bDumpStack;
}

//------------------------------------------------------------------------------
/** \brief Enable or disable the built in binary operators.
    \sa m_bBuiltInOp, ReInit()

  If you disable the built in binary operators there will be no binary operators
  defined. Thus you must add them manually one by one. It is not possible to
  disable built in operators selectively. This function will Reinitialize the
  parser by calling ReInit().
*/
void ParserBase::EnableBuiltInOprt(bool a_bIsOn) {
    m_bBuiltInOp = a_bIsOn;
    ReInit();
}

//------------------------------------------------------------------------------
/** \brief Query status of built in variables.
    \return #m_bBuiltInOp; true if built in operators are enabled.
*/
bool ParserBase::HasBuiltInOprt() const { return m_bBuiltInOp; }

//------------------------------------------------------------------------------
/** \brief Get the argument separator character.
*/
char_type ParserBase::GetArgSep() const { return m_pTokenReader->GetArgSep(); }

//------------------------------------------------------------------------------
/** \brief Set argument separator.
    \param cArgSep the argument separator character.
*/
void ParserBase::SetArgSep(char_type cArgSep) { m_pTokenReader->SetArgSep(cArgSep); }

//------------------------------------------------------------------------------
/** \brief Dump stack content.

    This function is used for debugging only.
*/
void ParserBase::StackDump(const ParserStack<token_type> &a_stVal,
                           const ParserStack<token_type> &a_stOprt) const {
    ParserStack<token_type> stOprt(a_stOprt), stVal(a_stVal);

    mu::console() << _T("\nValue stack:\n");
    while (!stVal.empty()) {
        token_type val = stVal.pop();
        if (val.GetType() == tpSTR)
            mu::console() << _T(" \"") << val.GetAsString() << _T("\" ");
        else
            mu::console() << _T(" ") << val.GetVal().value() << _T(" ");
    }
    mu::console() << "\nOperator stack:\n";

    while (!stOprt.empty()) {
        if (stOprt.top().GetCode() <= cmASSIGN) {
            mu::console() << _T("OPRT_INTRNL \"")
                          << ParserBase::c_DefaultOprt[stOprt.top().GetCode()] << _T("\" \n");
        } else {
            switch (stOprt.top().GetCode()) {
                case cmVAR:
                    mu::console() << _T("VAR\n");
                    break;
                case cmVAL:
                    mu::console() << _T("VAL\n");
                    break;
                case cmFUNC:
                    mu::console() << _T("FUNC \"") << stOprt.top().GetAsString() << _T("\"\n");
                    break;
                case cmOPRT_INFIX:
                    mu::console() << _T("OPRT_INFIX \"") << stOprt.top().GetAsString()
                                  << _T("\"\n");
                    break;
                case cmOPRT_BIN:
                    mu::console() << _T("OPRT_BIN \"") << stOprt.top().GetAsString() << _T("\"\n");
                    break;
                case cmFUNC_STR:
                    mu::console() << _T("FUNC_STR\n");
                    break;
                case cmEND:
                    mu::console() << _T("END\n");
                    break;
                case cmUNKNOWN:
                    mu::console() << _T("UNKNOWN\n");
                    break;
                case cmBO:
                    mu::console() << _T("BRACKET \"(\"\n");
                    break;
                case cmBC:
                    mu::console() << _T("BRACKET \")\"\n");
                    break;
                case cmIF:
                    mu::console() << _T("IF\n");
                    break;
                case cmELSE:
                    mu::console() << _T("ELSE\n");
                    break;
                case cmENDIF:
                    mu::console() << _T("ENDIF\n");
                    break;
                default:
                    mu::console() << stOprt.top().GetCode() << _T(" ");
                    break;
            }
        }
        stOprt.pop();
    }

    mu::console() << dec << endl;
}

//------------------------------------------------------------------------------
/** \brief Evaluate an expression containing comma separated subexpressions
    \param [out] nStackSize The total number of results available
    \return Pointer to the array containing all expression results

    This member function can be used to retrieve all results of an expression
    made up of multiple comma separated subexpressions (i.e. "x+y,sin(x),cos(y)")
*/
void ParserBase::Eval(std::vector<ValueOrError> *outResult) const {
    ValueOrError v = BuildAndExecuteRPN();
    if (v.has_error()) {
        outResult->push_back(std::move(v));
        return;
    }
    int stackSize = m_nFinalResultIdx;

    // (for historic reasons the stack starts at position 1)
    outResult->assign(&m_vStackBuffer[1], &m_vStackBuffer[1 + stackSize]);
}

//---------------------------------------------------------------------------
/** \brief Return the number of results on the calculation stack.

  If the expression contains comma separated subexpressions (i.e. "sin(y), x+y").
  There may be more than one return value. This function returns the number of
  available results.
*/
int ParserBase::GetNumResults() const { return m_nFinalResultIdx; }

//---------------------------------------------------------------------------
/** \brief Calculate the result.

  A note on const correctness:
  I consider it important that Calc is a const function.
  Due to caching operations Calc changes only the state of internal variables with one exception
  m_UsedVar this is reset during string parsing and accessible from the outside.

  \pre A formula must be set.
  \pre Variables must have been set (if needed)

  \sa #m_pParseFormula
  \return The evaluation result
  \throw ParseException if no Formula is set or in case of any other error related to the formula.
*/
ValueOrError ParserBase::Eval() const { return BuildAndExecuteRPN(); }

}  // namespace mu
