/*
                 __________
    _____   __ __\______   \_____  _______  ______  ____ _______
   /     \ |  |  \|     ___/\__  \ \_  __ \/  ___/_/ __ \\_  __ \
  |  Y Y  \|  |  /|    |     / __ \_|  | \/\___ \ \  ___/ |  | \/
  |__|_|  /|____/ |____|    (____  /|__|  /____  > \___  >|__|
        \/                       \/            \/      \/
  Copyright (C) 2013 Ingo Berg

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
#ifndef MU_PARSER_BASE_H
#define MU_PARSER_BASE_H

//--- Standard includes ------------------------------------------------------------------------
#include <limits.h>
#include <cmath>
#include <iostream>
#include <locale>
#include <map>
#include <memory>
#include <string>

//--- Parser includes --------------------------------------------------------------------------
#include "muParserBytecode.h"
#include "muParserDef.h"
#include "muParserTokenReader.h"

namespace mu {
/** \file
    \brief This file contains the class definition of the muparser engine.
*/

//--------------------------------------------------------------------------------------------------
/** \brief Mathematical expressions parser (base parser engine).
    \author (C) 2013 Ingo Berg

  This is the implementation of a bytecode based mathematical expressions parser.
  The formula will be parsed from string and converted into a bytecode.
  Future calculations will be done with the bytecode instead the formula string
  resulting in a significant performance increase.
  Complementary to a set of internally implemented functions the parser is able to handle
  user defined functions and variables.
*/
class ParserBase {
    friend class ParserTokenReader;

   private:
    /** \brief Typedef for the parse functions.

      The parse function do the actual work. The parser exchanges
      the function pointer to the parser function depending on
      which state it is in. (i.e. bytecode parser vs. string parser)
    */
    typedef ValueOrError (ParserBase::*ParseFunction)() const;

    /** \brief Type for a vector of strings. */
    typedef std::vector<string_type> stringbuf_type;

    /** \brief Type used for parser tokens. */
    typedef ParserToken<value_type, string_type> token_type;

   public:
    /** \brief Type of the error class.

      Included for backwards compatibility.
    */
    typedef ParserError exception_type;

    static void EnableDebugDump(bool bDumpCmd, bool bDumpStack);

    ParserBase();
    ParserBase(const ParserBase &a_Parser) = delete;
    ParserBase &operator=(const ParserBase &a_Parser) = delete;

    virtual ~ParserBase();

    ValueOrError Eval() const;
    void Eval(std::vector<ValueOrError> *results) const;

    int GetNumResults() const;

    OptionalError SetExpr(const string_type &a_sExpr);
    void SetVarFactory(facfun_type a_pFactory, void *pUserData = NULL);

    void SetDecSep(char_type cDecSep);
    void SetThousandsSep(char_type cThousandsSep = 0);
    void ResetLocale();

    void EnableBuiltInOprt(bool a_bIsOn = true);

    bool HasBuiltInOprt() const;
    void AddValIdent(identfun_type a_pCallback);

    /** \fn void mu::ParserBase::DefineFun(const string_type &a_strName, fun_type0 a_pFun)
        \brief Define a parser function without arguments.
        \param a_strName Name of the function
        \param a_pFun Pointer to the callback function
    */
    template <typename T>
    OptionalError DefineFun(const string_type &a_strName, T a_pFun) {
        return AddCallback(a_strName, ParserCallback(a_pFun), m_FunDef, ValidNameChars());
    }

    OptionalError DefineOprt(const string_type &a_strName, fun_type2 a_pFun, unsigned a_iPri = 0,
                             EOprtAssociativity a_eAssociativity = oaLEFT);
    OptionalError DefineConst(const string_type &a_sName, value_type a_fVal);
    OptionalError DefineStrConst(const string_type &a_sName, const string_type &a_strVal);
    OptionalError DefineVar(const string_type &a_sName, value_type *a_fVar);
    OptionalError DefinePostfixOprt(const string_type &a_strFun, fun_type1 a_pOprt);
    OptionalError DefineInfixOprt(const string_type &a_strName, fun_type1 a_pOprt,
                                  int a_iPrec = prINFIX);

    // Clear user defined constants or operators.
    void ClearConst();
    void ClearPostfixOprt();

    void RemoveVar(const string_type &a_strVarName);
    const varmap_type &GetVar() const;
    const string_type &GetExpr() const;

    const char_type **GetOprtDef() const;
    void DefineNameChars(const char_type *a_szCharset);
    void DefineOprtChars(const char_type *a_szCharset);
    void DefineInfixOprtChars(const char_type *a_szCharset);

    const char_type *ValidNameChars() const;
    const char_type *ValidOprtChars() const;
    const char_type *ValidInfixOprtChars() const;

    void SetArgSep(char_type cArgSep);
    char_type GetArgSep() const;

    ParserError Error(EErrorCodes a_iErrc, int a_iPos = (int)mu::string_type::npos,
                      const string_type &a_strTok = string_type()) const;

   protected:
    void Init();

    virtual void InitCharSets() = 0;
    virtual void InitFun() = 0;
    virtual void InitConst() = 0;
    virtual void InitOprt() = 0;

    virtual void OnDetectVar(string_type *pExpr, int &nStart, int &nEnd);

    static const char_type *c_DefaultOprt[];
    static std::locale s_locale;  ///< The locale used by the parser
    static bool g_DbgDumpCmdCode;
    static bool g_DbgDumpStack;

    /** \brief A facet class used to change decimal and thousands separator. */
    template <class TChar>
    class change_dec_sep : public std::numpunct<TChar> {
       public:
        explicit change_dec_sep(char_type cDecSep, char_type cThousandsSep = 0, int nGroup = 3)
            : std::numpunct<TChar>(),
              m_nGroup(nGroup),
              m_cDecPoint(cDecSep),
              m_cThousandsSep(cThousandsSep) {}

       protected:
        virtual char_type do_decimal_point() const { return m_cDecPoint; }

        virtual char_type do_thousands_sep() const { return m_cThousandsSep; }

        virtual std::string do_grouping() const {
            // fix for issue 4: https://code.google.com/p/muparser/issues/detail?id=4
            // courtesy of Jens Bartsch
            // original code:
            //        return std::string(1, (char)m_nGroup);
            // new code:
            return std::string(1, (char)(m_cThousandsSep > 0 ? m_nGroup : CHAR_MAX));
        }

       private:
        int m_nGroup;
        char_type m_cDecPoint;
        char_type m_cThousandsSep;
    };

   private:
    void ReInit() const;

    OptionalError AddCallback(const string_type &a_strName, const ParserCallback &a_Callback,
                              funmap_type &a_Storage, const char_type *a_szCharSet);

    OptionalError ApplyRemainingOprt(ParserStack<token_type> &a_stOpt,
                                     ParserStack<token_type> &a_stVal) const;
    OptionalError ApplyBinOprt(ParserStack<token_type> &a_stOpt,
                               ParserStack<token_type> &a_stVal) const;

    OptionalError ApplyIfElse(ParserStack<token_type> &a_stOpt,
                              ParserStack<token_type> &a_stVal) const;

    OptionalError ApplyFunc(ParserStack<token_type> &a_stOpt, ParserStack<token_type> &a_stVal,
                            int iArgCount) const;

    OptionalError ApplyStrFunc(const token_type &a_FunTok,
                               const std::vector<token_type> &a_vArg) const;

    int GetOprtPrecedence(const token_type &a_Tok) const;
    EOprtAssociativity GetOprtAssociativity(const token_type &a_Tok) const;

    OptionalError CreateRPN() const;

    ValueOrError ExecuteRPN() const;
    ValueOrError InvokeFunction(generic_fun_type func, const value_type *args, int argCount) const;

    /// Build the RPN if necessary, and then execute it.
    /// \return the result, or an error.
    ValueOrError BuildAndExecuteRPN() const;

    OptionalError CheckName(const string_type &a_strName, const string_type &a_CharSet) const;
    OptionalError CheckOprt(const string_type &a_sName, const ParserCallback &a_Callback,
                            const string_type &a_szCharSet) const;

    void StackDump(const ParserStack<token_type> &a_stVal,
                   const ParserStack<token_type> &a_stOprt) const;

    /** \brief Pointer to the parser function.

      Eval() calls the function whose address is stored there.
    */
    mutable ParserByteCode m_vRPN;  ///< The Bytecode class.
    mutable stringbuf_type
        m_vStringBuf;  ///< String buffer, used for storing string function arguments
    stringbuf_type m_vStringVarBuf;

    std::unique_ptr<ParserTokenReader> m_pTokenReader;

    funmap_type m_FunDef;        ///< Map of function names and pointers.
    funmap_type m_PostOprtDef;   ///< Postfix operator callbacks
    funmap_type m_InfixOprtDef;  ///< unary infix operator.
    funmap_type m_OprtDef;       ///< Binary operator callbacks
    valmap_type m_ConstDef;      ///< user constants.
    strmap_type m_StrVarDef;     ///< user defined string constants
    varmap_type m_VarDef;        ///< user defind variables.

    bool m_bBuiltInOp =
        true;  ///< Flag that can be used for switching built in operators on and off

    string_type m_sNameChars;       ///< Charset for names
    string_type m_sOprtChars;       ///< Charset for postfix/ binary operator tokens
    string_type m_sInfixOprtChars;  ///< Charset for infix operator tokens

    // items merely used for caching state information
    /// This is merely a buffer used for the stack in the cmd parsing routine
    mutable std::vector<value_type> m_vStackBuffer;
    mutable int m_nFinalResultIdx = 0;
};
}  // namespace mu

#endif
