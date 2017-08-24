#ifndef MU_PARSER_TEMPLATE_MAGIC_H
#define MU_PARSER_TEMPLATE_MAGIC_H

#include <cmath>
#include "muParserError.h"


namespace mu
{
  //-----------------------------------------------------------------------------------------------
  //
  // Compile time type detection
  //
  //-----------------------------------------------------------------------------------------------

  /** \brief A class singling out integer types at compile time using 
             template meta programming.
  */
  template<typename T>
  struct TypeInfo
  {
    static bool IsInteger() { return false; }
  };

  template<>
  struct TypeInfo<char>
  {
    static bool IsInteger() { return true;  }
  };

  template<>
  struct TypeInfo<short>
  {
    static bool IsInteger() { return true;  }
  };

  template<>
  struct TypeInfo<int>
  {
    static bool IsInteger() { return true;  }
  };

  template<>
  struct TypeInfo<long>
  {
    static bool IsInteger() { return true;  }
  };

  template<>
  struct TypeInfo<unsigned char>
  {
    static bool IsInteger() { return true;  }
  };

  template<>
  struct TypeInfo<unsigned short>
  {
    static bool IsInteger() { return true;  }
  };

  template<>
  struct TypeInfo<unsigned int>
  {
    static bool IsInteger() { return true;  }
  };

  template<>
  struct TypeInfo<unsigned long>
  {
    static bool IsInteger() { return true;  }
  };


  //-----------------------------------------------------------------------------------------------
  //
  // Standard math functions with dummy overload for integer types
  //
  //-----------------------------------------------------------------------------------------------

  /** \brief A template class for providing wrappers for essential math functions.

    This template is spezialized for several types in order to provide a unified interface
    for parser internal math function calls regardless of the data type.
  */
  template<typename T>
  struct MathImpl
  {
    static T Sin(T v)   { return sin(v);  }
    static T Cos(T v)   { return cos(v);  }
    static T Tan(T v)   { return tan(v);  }
    static T ASin(T v)  { return asin(v); }
    static T ACos(T v)  { return acos(v); }
    static T ATan(T v)  { return atan(v); }
    static T ATan2(T v1, T v2) { return atan2(v1, v2); }
    static T Sinh(T v)  { return sinh(v); }
    static T Cosh(T v)  { return cosh(v); }
    static T Tanh(T v)  { return tanh(v); }
    static T ASinh(T v) { return log(v + sqrt(v * v + 1)); }
    static T ACosh(T v) { return log(v + sqrt(v * v - 1)); }
    static T ATanh(T v) { return ((T)0.5 * log((1 + v) / (1 - v))); }
    static T Log(T v)   { return log(v); } 
    static T Log2(T v)  { return log(v)/log((T)2); } // Logarithm base 2
    static T Log10(T v) { return log10(v); }         // Logarithm base 10
    static T Exp(T v)   { return exp(v);   }
    static T Abs(T v)   { return (v>=0) ? v : -v; }
    static T Sqrt(T v)  { return sqrt(v); }
    static T Rint(T v)  { return floor(v + (T)0.5); }
    static T Sign(T v)  { return (T)((v<0) ? -1 : (v>0) ? 1 : 0); }
    static T Pow(T v1, T v2) { return std::pow(v1, v2); }
  };
}

#endif
