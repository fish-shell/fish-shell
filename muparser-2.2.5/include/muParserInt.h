/*
                 __________                                      
    _____   __ __\______   \_____  _______  ______  ____ _______ 
   /     \ |  |  \|     ___/\__  \ \_  __ \/  ___/_/ __ \\_  __ \
  |  Y Y  \|  |  /|    |     / __ \_|  | \/\___ \ \  ___/ |  | \/
  |__|_|  /|____/ |____|    (____  /|__|  /____  > \___  >|__|   
        \/                       \/            \/      \/        
  Copyright (C) 2004-2013 Ingo Berg

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

#ifndef MU_PARSER_INT_H
#define MU_PARSER_INT_H

#include "muParserBase.h"
#include <vector>


/** \file
    \brief Definition of a parser using integer value.
*/


namespace mu
{

/** \brief Mathematical expressions parser.
  
  This version of the parser handles only integer numbers. It disables the built in operators thus it is 
  slower than muParser. Integer values are stored in the double value_type and converted if needed.
*/
class ParserInt : public ParserBase
{
private:
    static int  Round(value_type v) { return (int)(v + ((v>=0) ? 0.5 : -0.5) ); };
  
    static value_type  Abs(value_type);
    static value_type  Sign(value_type);
    static value_type  Ite(value_type, value_type, value_type);
    // !! The unary Minus is a MUST, otherwise you cant use negative signs !!
    static value_type  UnaryMinus(value_type);
    // Functions with variable number of arguments
    static value_type  Sum(const value_type* a_afArg, int a_iArgc);  // sum
    static value_type  Min(const value_type* a_afArg, int a_iArgc);  // minimum
    static value_type  Max(const value_type* a_afArg, int a_iArgc);  // maximum
    // binary operator callbacks
    static value_type  Add(value_type v1, value_type v2);
    static value_type  Sub(value_type v1, value_type v2);
    static value_type  Mul(value_type v1, value_type v2);
    static value_type  Div(value_type v1, value_type v2);
    static value_type  Mod(value_type v1, value_type v2);
    static value_type  Pow(value_type v1, value_type v2);
    static value_type  Shr(value_type v1, value_type v2);
    static value_type  Shl(value_type v1, value_type v2);
    static value_type  LogAnd(value_type v1, value_type v2);
    static value_type  LogOr(value_type v1, value_type v2);
    static value_type  And(value_type v1, value_type v2);
    static value_type  Or(value_type v1, value_type v2);
    static value_type  Xor(value_type v1, value_type v2);
    static value_type  Less(value_type v1, value_type v2);
    static value_type  Greater(value_type v1, value_type v2);
    static value_type  LessEq(value_type v1, value_type v2);
    static value_type  GreaterEq(value_type v1, value_type v2);
    static value_type  Equal(value_type v1, value_type v2);
    static value_type  NotEqual(value_type v1, value_type v2);
    static value_type  Not(value_type v1);

    static int IsHexVal(const char_type* a_szExpr, int *a_iPos, value_type *a_iVal);
    static int IsBinVal(const char_type* a_szExpr, int *a_iPos, value_type *a_iVal);
    static int IsVal   (const char_type* a_szExpr, int *a_iPos, value_type *a_iVal);

    /** \brief A facet class used to change decimal and thousands separator. */
    template<class TChar>
    class change_dec_sep : public std::numpunct<TChar>
    {
    public:
      
      explicit change_dec_sep(char_type cDecSep, char_type cThousandsSep = 0, int nGroup = 3)
        :std::numpunct<TChar>()
        ,m_cDecPoint(cDecSep)
        ,m_cThousandsSep(cThousandsSep)
        ,m_nGroup(nGroup)
      {}
      
    protected:
      
      virtual char_type do_decimal_point() const
      {
        return m_cDecPoint;
      }

      virtual char_type do_thousands_sep() const
      {
        return m_cThousandsSep;
      }

      virtual std::string do_grouping() const 
      { 
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

public:
    ParserInt();

    virtual void InitFun();
    virtual void InitOprt();
    virtual void InitConst();
    virtual void InitCharSets();
};

} // namespace mu

#endif

