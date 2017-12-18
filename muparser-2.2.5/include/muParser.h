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
#ifndef MU_PARSER_H
#define MU_PARSER_H

//--- Standard includes ------------------------------------------------------------------------
#include <vector>

//--- Parser includes --------------------------------------------------------------------------
#include "muParserBase.h"

/** \file
    \brief Definition of the standard floating point parser.
*/

namespace mu {
/** \brief Mathematical expressions parser.

  Standard implementation of the mathematical expressions parser.
  Can be used as a reference implementation for subclassing the parser.

  <small>
  (C) 2011 Ingo Berg<br>
  muparser(at)beltoforion.de
  </small>
*/
/* final */ class Parser : public ParserBase {
   public:
    Parser();

    virtual void InitCharSets();
    virtual void InitFun();
    virtual void InitConst();
    virtual void InitOprt();
    virtual void OnDetectVar(string_type *pExpr, int &nStart, int &nEnd);

    ValueOrError Diff(value_type *a_Var, value_type a_fPos, value_type a_fEpsilon = 0) const;

   protected:
    // Trigonometric functions
    static ValueOrError Sin(value_type);
    static ValueOrError Cos(value_type);
    static ValueOrError Tan(value_type);
    static ValueOrError Tan2(value_type, value_type);
    // arcus functions
    static ValueOrError ASin(value_type);
    static ValueOrError ACos(value_type);
    static ValueOrError ATan(value_type);
    static ValueOrError ATan2(value_type, value_type);

    // hyperbolic functions
    static ValueOrError Sinh(value_type);
    static ValueOrError Cosh(value_type);
    static ValueOrError Tanh(value_type);
    // arcus hyperbolic functions
    static ValueOrError ASinh(value_type);
    static ValueOrError ACosh(value_type);
    static ValueOrError ATanh(value_type);
    // Logarithm functions
    static ValueOrError Log2(value_type);   // Logarithm Base 2
    static ValueOrError Log10(value_type);  // Logarithm Base 10
    static ValueOrError Ln(value_type);     // Logarithm Base e (natural logarithm)
    // misc
    static ValueOrError Exp(value_type);
    static ValueOrError Abs(value_type);
    static ValueOrError Sqrt(value_type);
    static ValueOrError Rint(value_type);
    static ValueOrError Sign(value_type);

    // Prefix operators
    // !!! Unary Minus is a MUST if you want to use negative signs !!!
    static ValueOrError UnaryMinus(value_type);
    static ValueOrError UnaryPlus(value_type);

    // Functions with variable number of arguments
    static ValueOrError Sum(const value_type *, int);  // sum
    static ValueOrError Avg(const value_type *, int);  // mean value
    static ValueOrError Min(const value_type *, int);  // minimum
    static ValueOrError Max(const value_type *, int);  // maximum

    static int IsVal(const char_type *a_szExpr, int *a_iPos, value_type *a_fVal);
};
}  // namespace mu

#endif
