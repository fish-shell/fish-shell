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

#include <iostream>
#include <string>
#include <sstream>
#include <map>

#include "muParserFixes.h"

/** \file
    \brief This file contains standard definitions used by the parser.
*/

#define MUP_VERSION _T("2.2.5")
#define MUP_VERSION_DATE _T("20150427; GC")

#define MUP_CHARS _T("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ")

/** \brief If this macro is defined mathematical exceptions (div by zero) will be thrown as exceptions. */
//#define MUP_MATH_EXCEPTIONS

/** \brief Define the base datatype for values.

  This datatype must be a built in value type. You can not use custom classes.
  It should be working with all types except "int"!
*/
#define MUP_BASETYPE double

/** \brief Activate this option in order to compile with OpenMP support. 

  OpenMP is used only in the bulk mode it may increase the performance a bit. 
*/
//#define MUP_USE_OPENMP

#if defined(_UNICODE)
  /** \brief Definition of the basic parser string type. */
  #define MUP_STRING_TYPE std::wstring

  #if !defined(_T)
    #define _T(x) L##x
  #endif // not defined _T
#else
  #ifndef _T
  #define _T(x) x
  #endif
  
  /** \brief Definition of the basic parser string type. */
  #define MUP_STRING_TYPE std::string
#endif

#if defined(_DEBUG)
  /** \brief Debug macro to force an abortion of the programm with a certain message.
  */
  #define MUP_FAIL(MSG)     \
          {                 \
            bool MSG=false; \
            assert(MSG);    \
          }

    /** \brief An assertion that does not kill the program.

        This macro is neutralised in UNICODE builds. It's
        too difficult to translate.
    */
    #define MUP_ASSERT(COND)                         \
            if (!(COND))                             \
            {                                        \
              stringstream_type ss;                  \
              ss << _T("Assertion \"") _T(#COND) _T("\" failed: ") \
                 << __FILE__ << _T(" line ")         \
                 << __LINE__ << _T(".");             \
              throw ParserError( ss.str() );         \
            }
#else
  #define MUP_FAIL(MSG)
  #define MUP_ASSERT(COND)
#endif


namespace mu
{
#if defined(_UNICODE)

  //------------------------------------------------------------------------------
  /** \brief Encapsulate wcout. */
  inline std::wostream& console()
  {
    return std::wcout;
  }

  /** \brief Encapsulate cin. */
  inline std::wistream& console_in()
  {
    return std::wcin;
  }

#else

  /** \brief Encapsulate cout. 
  
    Used for supporting UNICODE more easily.
  */
  inline std::ostream& console()
  {
    return std::cout;
  }

  /** \brief Encapsulate cin. 

    Used for supporting UNICODE more easily.
  */
  inline std::istream& console_in()
  {
    return std::cin;
  }

#endif

  //------------------------------------------------------------------------------
  /** \brief Bytecode values.

      \attention The order of the operator entries must match the order in ParserBase::c_DefaultOprt!
  */
  enum ECmdCode
  {
    // The following are codes for built in binary operators
    // apart from built in operators the user has the opportunity to
    // add user defined operators.
    cmLE            = 0,   ///< Operator item:  less or equal
    cmGE            = 1,   ///< Operator item:  greater or equal
    cmNEQ           = 2,   ///< Operator item:  not equal
    cmEQ            = 3,   ///< Operator item:  equals
    cmLT            = 4,   ///< Operator item:  less than
    cmGT            = 5,   ///< Operator item:  greater than
    cmADD           = 6,   ///< Operator item:  add
    cmSUB           = 7,   ///< Operator item:  subtract
    cmMUL           = 8,   ///< Operator item:  multiply
    cmDIV           = 9,   ///< Operator item:  division
    cmPOW           = 10,  ///< Operator item:  y to the power of ...
    cmLAND          = 11,
    cmLOR           = 12,
    cmASSIGN        = 13,  ///< Operator item:  Assignment operator
    cmBO            = 14,  ///< Operator item:  opening bracket
    cmBC            = 15,  ///< Operator item:  closing bracket
    cmIF            = 16,  ///< For use in the ternary if-then-else operator
    cmELSE          = 17,  ///< For use in the ternary if-then-else operator
    cmENDIF         = 18,  ///< For use in the ternary if-then-else operator
    cmARG_SEP       = 19,  ///< function argument separator
    cmVAR           = 20,  ///< variable item
    cmVAL           = 21,  ///< value item

    // For optimization purposes
    cmVARPOW2,
    cmVARPOW3,
    cmVARPOW4,
    cmVARMUL,
    cmPOW2,

    // operators and functions
    cmFUNC,                ///< Code for a generic function item
    cmFUNC_STR,            ///< Code for a function with a string parameter
    cmFUNC_BULK,           ///< Special callbacks for Bulk mode with an additional parameter for the bulk index 
    cmSTRING,              ///< Code for a string token
    cmOPRT_BIN,            ///< user defined binary operator
    cmOPRT_POSTFIX,        ///< code for postfix operators
    cmOPRT_INFIX,          ///< code for infix operators
    cmEND,                 ///< end of formula
    cmUNKNOWN              ///< uninitialized item
  };

  //------------------------------------------------------------------------------
  /** \brief Types internally used by the parser.
  */
  enum ETypeCode
  {
    tpSTR  = 0,     ///< String type (Function arguments and constants only, no string variables)
    tpDBL  = 1,     ///< Floating point variables
    tpVOID = 2      ///< Undefined type.
  };

  //------------------------------------------------------------------------------
  enum EParserVersionInfo
  {
    pviBRIEF,
    pviFULL
  };

  //------------------------------------------------------------------------------
  /** \brief Parser operator precedence values. */
  enum EOprtAssociativity
  {
    oaLEFT  = 0,
    oaRIGHT = 1,
    oaNONE  = 2
  };

  //------------------------------------------------------------------------------
  /** \brief Parser operator precedence values. */
  enum EOprtPrecedence
  {
    // binary operators
    prLOR     = 1,
    prLAND    = 2,
    prLOGIC   = 3,  ///< logic operators
    prCMP     = 4,  ///< comparsion operators
    prADD_SUB = 5,  ///< addition
    prMUL_DIV = 6,  ///< multiplication/division
    prPOW     = 7,  ///< power operator priority (highest)

    // infix operators
    prINFIX   = 6, ///< Signs have a higher priority than ADD_SUB, but lower than power operator
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
  typedef std::basic_stringstream<char_type,
                                  std::char_traits<char_type>,
                                  std::allocator<char_type> > stringstream_type;

  // Data container types

  /** \brief Type used for storing variables. */
  typedef std::map<string_type, value_type*> varmap_type;
  
  /** \brief Type used for storing constants. */
  typedef std::map<string_type, value_type> valmap_type;
  
  /** \brief Type for assigning a string name to an index in the internal string table. */
  typedef std::map<string_type, std::size_t> strmap_type;

  // Parser callbacks
  
  /** \brief Callback type used for functions without arguments. */
  typedef value_type (*generic_fun_type)();

  /** \brief Callback type used for functions without arguments. */
  typedef value_type (*fun_type0)();

  /** \brief Callback type used for functions with a single arguments. */
  typedef value_type (*fun_type1)(value_type);

  /** \brief Callback type used for functions with two arguments. */
  typedef value_type (*fun_type2)(value_type, value_type);

  /** \brief Callback type used for functions with three arguments. */
  typedef value_type (*fun_type3)(value_type, value_type, value_type);

  /** \brief Callback type used for functions with four arguments. */
  typedef value_type (*fun_type4)(value_type, value_type, value_type, value_type);

  /** \brief Callback type used for functions with five arguments. */
  typedef value_type (*fun_type5)(value_type, value_type, value_type, value_type, value_type);

  /** \brief Callback type used for functions with five arguments. */
  typedef value_type (*fun_type6)(value_type, value_type, value_type, value_type, value_type, value_type);

  /** \brief Callback type used for functions with five arguments. */
  typedef value_type (*fun_type7)(value_type, value_type, value_type, value_type, value_type, value_type, value_type);

  /** \brief Callback type used for functions with five arguments. */
  typedef value_type (*fun_type8)(value_type, value_type, value_type, value_type, value_type, value_type, value_type, value_type);

  /** \brief Callback type used for functions with five arguments. */
  typedef value_type (*fun_type9)(value_type, value_type, value_type, value_type, value_type, value_type, value_type, value_type, value_type);

  /** \brief Callback type used for functions with five arguments. */
  typedef value_type (*fun_type10)(value_type, value_type, value_type, value_type, value_type, value_type, value_type, value_type, value_type, value_type);

  /** \brief Callback type used for functions without arguments. */
  typedef value_type (*bulkfun_type0)(int, int);

  /** \brief Callback type used for functions with a single arguments. */
  typedef value_type (*bulkfun_type1)(int, int, value_type);

  /** \brief Callback type used for functions with two arguments. */
  typedef value_type (*bulkfun_type2)(int, int, value_type, value_type);

  /** \brief Callback type used for functions with three arguments. */
  typedef value_type (*bulkfun_type3)(int, int, value_type, value_type, value_type);

  /** \brief Callback type used for functions with four arguments. */
  typedef value_type (*bulkfun_type4)(int, int, value_type, value_type, value_type, value_type);

  /** \brief Callback type used for functions with five arguments. */
  typedef value_type (*bulkfun_type5)(int, int, value_type, value_type, value_type, value_type, value_type);

  /** \brief Callback type used for functions with five arguments. */
  typedef value_type (*bulkfun_type6)(int, int, value_type, value_type, value_type, value_type, value_type, value_type);

  /** \brief Callback type used for functions with five arguments. */
  typedef value_type (*bulkfun_type7)(int, int, value_type, value_type, value_type, value_type, value_type, value_type, value_type);

  /** \brief Callback type used for functions with five arguments. */
  typedef value_type (*bulkfun_type8)(int, int, value_type, value_type, value_type, value_type, value_type, value_type, value_type, value_type);

  /** \brief Callback type used for functions with five arguments. */
  typedef value_type (*bulkfun_type9)(int, int, value_type, value_type, value_type, value_type, value_type, value_type, value_type, value_type, value_type);

  /** \brief Callback type used for functions with five arguments. */
  typedef value_type (*bulkfun_type10)(int, int, value_type, value_type, value_type, value_type, value_type, value_type, value_type, value_type, value_type, value_type);

  /** \brief Callback type used for functions with a variable argument list. */
  typedef value_type (*multfun_type)(const value_type*, int);

  /** \brief Callback type used for functions taking a string as an argument. */
  typedef value_type (*strfun_type1)(const char_type*);

  /** \brief Callback type used for functions taking a string and a value as arguments. */
  typedef value_type (*strfun_type2)(const char_type*, value_type);

  /** \brief Callback type used for functions taking a string and two values as arguments. */
  typedef value_type (*strfun_type3)(const char_type*, value_type, value_type);

  /** \brief Callback used for functions that identify values in a string. */
  typedef int (*identfun_type)(const char_type *sExpr, int *nPos, value_type *fVal);

  /** \brief Callback used for variable creation factory functions. */
  typedef value_type* (*facfun_type)(const char_type*, void*);
} // end of namespace

#endif

