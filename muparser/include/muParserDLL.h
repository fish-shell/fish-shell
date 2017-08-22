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
#ifndef MU_PARSER_DLL_H
#define MU_PARSER_DLL_H

#if defined(WIN32) || defined(_WIN32)
    #ifdef MUPARSERLIB_EXPORTS
        #define API_EXPORT(TYPE) __declspec(dllexport) TYPE __cdecl
    #else
        #define API_EXPORT(TYPE) __declspec(dllimport) TYPE __cdecl
    #endif
#else
    #define API_EXPORT(TYPE) TYPE
#endif


#ifdef __cplusplus
extern "C"
{
#endif

/** \file 
    \brief This file contains the DLL interface of muparser.
*/

// Basic types
typedef void*  muParserHandle_t;    // parser handle

#ifndef _UNICODE
    typedef char   muChar_t;            // character type
#else
    typedef wchar_t   muChar_t;            // character type
#endif

typedef int    muBool_t;            // boolean type
typedef int    muInt_t;             // integer type 
typedef double muFloat_t;           // floating point type

// function types for calculation
typedef muFloat_t (*muFun0_t )(); 
typedef muFloat_t (*muFun1_t )(muFloat_t); 
typedef muFloat_t (*muFun2_t )(muFloat_t, muFloat_t); 
typedef muFloat_t (*muFun3_t )(muFloat_t, muFloat_t, muFloat_t); 
typedef muFloat_t (*muFun4_t )(muFloat_t, muFloat_t, muFloat_t, muFloat_t); 
typedef muFloat_t (*muFun5_t )(muFloat_t, muFloat_t, muFloat_t, muFloat_t, muFloat_t); 
typedef muFloat_t (*muFun6_t )(muFloat_t, muFloat_t, muFloat_t, muFloat_t, muFloat_t, muFloat_t); 
typedef muFloat_t (*muFun7_t )(muFloat_t, muFloat_t, muFloat_t, muFloat_t, muFloat_t, muFloat_t, muFloat_t); 
typedef muFloat_t (*muFun8_t )(muFloat_t, muFloat_t, muFloat_t, muFloat_t, muFloat_t, muFloat_t, muFloat_t, muFloat_t); 
typedef muFloat_t (*muFun9_t )(muFloat_t, muFloat_t, muFloat_t, muFloat_t, muFloat_t, muFloat_t, muFloat_t, muFloat_t, muFloat_t); 
typedef muFloat_t (*muFun10_t)(muFloat_t, muFloat_t, muFloat_t, muFloat_t, muFloat_t, muFloat_t, muFloat_t, muFloat_t, muFloat_t, muFloat_t); 

// Function prototypes for bulkmode functions
typedef muFloat_t (*muBulkFun0_t )(int, int); 
typedef muFloat_t (*muBulkFun1_t )(int, int, muFloat_t); 
typedef muFloat_t (*muBulkFun2_t )(int, int, muFloat_t, muFloat_t); 
typedef muFloat_t (*muBulkFun3_t )(int, int, muFloat_t, muFloat_t, muFloat_t); 
typedef muFloat_t (*muBulkFun4_t )(int, int, muFloat_t, muFloat_t, muFloat_t, muFloat_t); 
typedef muFloat_t (*muBulkFun5_t )(int, int, muFloat_t, muFloat_t, muFloat_t, muFloat_t, muFloat_t); 
typedef muFloat_t (*muBulkFun6_t )(int, int, muFloat_t, muFloat_t, muFloat_t, muFloat_t, muFloat_t, muFloat_t); 
typedef muFloat_t (*muBulkFun7_t )(int, int, muFloat_t, muFloat_t, muFloat_t, muFloat_t, muFloat_t, muFloat_t, muFloat_t); 
typedef muFloat_t (*muBulkFun8_t )(int, int, muFloat_t, muFloat_t, muFloat_t, muFloat_t, muFloat_t, muFloat_t, muFloat_t, muFloat_t); 
typedef muFloat_t (*muBulkFun9_t )(int, int, muFloat_t, muFloat_t, muFloat_t, muFloat_t, muFloat_t, muFloat_t, muFloat_t, muFloat_t, muFloat_t); 
typedef muFloat_t (*muBulkFun10_t)(int, int, muFloat_t, muFloat_t, muFloat_t, muFloat_t, muFloat_t, muFloat_t, muFloat_t, muFloat_t, muFloat_t, muFloat_t); 

typedef muFloat_t (*muMultFun_t)(const muFloat_t*, muInt_t);
typedef muFloat_t (*muStrFun1_t)(const muChar_t*);
typedef muFloat_t (*muStrFun2_t)(const muChar_t*, muFloat_t);
typedef muFloat_t (*muStrFun3_t)(const muChar_t*, muFloat_t, muFloat_t);

// Functions for parser management
typedef void (*muErrorHandler_t)(muParserHandle_t a_hParser);           // [optional] callback to an error handler
typedef muFloat_t* (*muFacFun_t)(const muChar_t*, void*);               // [optional] callback for creating new variables
typedef muInt_t (*muIdentFun_t)(const muChar_t*, muInt_t*, muFloat_t*); // [optional] value identification callbacks

//-----------------------------------------------------------------------------------------------------
// Constants
static const int muOPRT_ASCT_LEFT  = 0;
static const int muOPRT_ASCT_RIGHT = 1;

static const int muBASETYPE_FLOAT  = 0;
static const int muBASETYPE_INT    = 1;

//-----------------------------------------------------------------------------------------------------
//
//
// muParser C compatible bindings
//
//
//-----------------------------------------------------------------------------------------------------


// Basic operations / initialization  
API_EXPORT(muParserHandle_t) mupCreate(int nBaseType);
API_EXPORT(void) mupRelease(muParserHandle_t a_hParser);
API_EXPORT(const muChar_t*) mupGetExpr(muParserHandle_t a_hParser);
API_EXPORT(void) mupSetExpr(muParserHandle_t a_hParser, const muChar_t *a_szExpr);
API_EXPORT(void) mupSetVarFactory(muParserHandle_t a_hParser, muFacFun_t a_pFactory, void* pUserData);
API_EXPORT(const muChar_t*) mupGetVersion(muParserHandle_t a_hParser);
API_EXPORT(muFloat_t) mupEval(muParserHandle_t a_hParser);
API_EXPORT(muFloat_t*) mupEvalMulti(muParserHandle_t a_hParser, int *nNum);
API_EXPORT(void) mupEvalBulk(muParserHandle_t a_hParser, muFloat_t *a_fResult, int nSize);

// Defining callbacks / variables / constants
API_EXPORT(void) mupDefineFun0(muParserHandle_t a_hParser, const muChar_t *a_szName, muFun0_t a_pFun, muBool_t a_bOptimize);
API_EXPORT(void) mupDefineFun1(muParserHandle_t a_hParser, const muChar_t *a_szName, muFun1_t a_pFun, muBool_t a_bOptimize);
API_EXPORT(void) mupDefineFun2(muParserHandle_t a_hParser, const muChar_t *a_szName, muFun2_t a_pFun, muBool_t a_bOptimize);
API_EXPORT(void) mupDefineFun3(muParserHandle_t a_hParser, const muChar_t *a_szName, muFun3_t a_pFun, muBool_t a_bOptimize);
API_EXPORT(void) mupDefineFun4(muParserHandle_t a_hParser, const muChar_t *a_szName, muFun4_t a_pFun, muBool_t a_bOptimize);
API_EXPORT(void) mupDefineFun5(muParserHandle_t a_hParser, const muChar_t *a_szName, muFun5_t a_pFun, muBool_t a_bOptimize);
API_EXPORT(void) mupDefineFun6(muParserHandle_t a_hParser, const muChar_t *a_szName, muFun6_t a_pFun, muBool_t a_bOptimize);
API_EXPORT(void) mupDefineFun7(muParserHandle_t a_hParser, const muChar_t *a_szName, muFun7_t a_pFun, muBool_t a_bOptimize);
API_EXPORT(void) mupDefineFun8(muParserHandle_t a_hParser, const muChar_t *a_szName, muFun8_t a_pFun, muBool_t a_bOptimize);
API_EXPORT(void) mupDefineFun9(muParserHandle_t a_hParser, const muChar_t *a_szName, muFun9_t a_pFun, muBool_t a_bOptimize);
API_EXPORT(void) mupDefineFun10(muParserHandle_t a_hParser, const muChar_t *a_szName, muFun10_t a_pFun, muBool_t a_bOptimize);

// Defining bulkmode functions
API_EXPORT(void) mupDefineBulkFun0(muParserHandle_t a_hParser, const muChar_t *a_szName, muBulkFun0_t a_pFun);
API_EXPORT(void) mupDefineBulkFun1(muParserHandle_t a_hParser, const muChar_t *a_szName, muBulkFun1_t a_pFun);
API_EXPORT(void) mupDefineBulkFun2(muParserHandle_t a_hParser, const muChar_t *a_szName, muBulkFun2_t a_pFun);
API_EXPORT(void) mupDefineBulkFun3(muParserHandle_t a_hParser, const muChar_t *a_szName, muBulkFun3_t a_pFun);
API_EXPORT(void) mupDefineBulkFun4(muParserHandle_t a_hParser, const muChar_t *a_szName, muBulkFun4_t a_pFun);
API_EXPORT(void) mupDefineBulkFun5(muParserHandle_t a_hParser, const muChar_t *a_szName, muBulkFun5_t a_pFun);
API_EXPORT(void) mupDefineBulkFun6(muParserHandle_t a_hParser, const muChar_t *a_szName, muBulkFun6_t a_pFun);
API_EXPORT(void) mupDefineBulkFun7(muParserHandle_t a_hParser, const muChar_t *a_szName, muBulkFun7_t a_pFun);
API_EXPORT(void) mupDefineBulkFun8(muParserHandle_t a_hParser, const muChar_t *a_szName, muBulkFun8_t a_pFun);
API_EXPORT(void) mupDefineBulkFun9(muParserHandle_t a_hParser, const muChar_t *a_szName, muBulkFun9_t a_pFun);
API_EXPORT(void) mupDefineBulkFun10(muParserHandle_t a_hParser, const muChar_t *a_szName, muBulkFun10_t a_pFun);

// string functions
API_EXPORT(void) mupDefineStrFun1(muParserHandle_t a_hParser, const muChar_t *a_szName, muStrFun1_t a_pFun);
API_EXPORT(void) mupDefineStrFun2(muParserHandle_t a_hParser, const muChar_t *a_szName, muStrFun2_t a_pFun);
API_EXPORT(void) mupDefineStrFun3(muParserHandle_t a_hParser, const muChar_t *a_szName, muStrFun3_t a_pFun);

API_EXPORT(void) mupDefineMultFun( muParserHandle_t a_hParser, 
                                   const muChar_t* a_szName, 
                                   muMultFun_t a_pFun, 
                                   muBool_t a_bOptimize);

API_EXPORT(void) mupDefineOprt( muParserHandle_t a_hParser, 
                                const muChar_t* a_szName, 
                                muFun2_t a_pFun, 
                                muInt_t a_nPrec, 
                                muInt_t a_nOprtAsct,
                                muBool_t a_bOptimize);

API_EXPORT(void) mupDefineConst( muParserHandle_t a_hParser, 
                                 const muChar_t* a_szName, 
                                 muFloat_t a_fVal );

API_EXPORT(void) mupDefineStrConst( muParserHandle_t a_hParser, 
                                    const muChar_t* a_szName, 
                                    const muChar_t *a_sVal );

API_EXPORT(void) mupDefineVar( muParserHandle_t a_hParser, 
                               const muChar_t* a_szName, 
                               muFloat_t *a_fVar);

API_EXPORT(void) mupDefineBulkVar( muParserHandle_t a_hParser, 
                               const muChar_t* a_szName, 
                               muFloat_t *a_fVar);

API_EXPORT(void) mupDefinePostfixOprt( muParserHandle_t a_hParser, 
                                       const muChar_t* a_szName, 
                                       muFun1_t a_pOprt, 
                                       muBool_t a_bOptimize);


API_EXPORT(void) mupDefineInfixOprt( muParserHandle_t a_hParser, 
                                     const muChar_t* a_szName, 
                                     muFun1_t a_pOprt, 
                                     muBool_t a_bOptimize);

// Define character sets for identifiers
API_EXPORT(void) mupDefineNameChars(muParserHandle_t a_hParser, const muChar_t* a_szCharset);
API_EXPORT(void) mupDefineOprtChars(muParserHandle_t a_hParser, const muChar_t* a_szCharset);
API_EXPORT(void) mupDefineInfixOprtChars(muParserHandle_t a_hParser, const muChar_t* a_szCharset);

// Remove all / single variables
API_EXPORT(void) mupRemoveVar(muParserHandle_t a_hParser, const muChar_t* a_szName);
API_EXPORT(void) mupClearVar(muParserHandle_t a_hParser);
API_EXPORT(void) mupClearConst(muParserHandle_t a_hParser);
API_EXPORT(void) mupClearOprt(muParserHandle_t a_hParser);
API_EXPORT(void) mupClearFun(muParserHandle_t a_hParser);

// Querying variables / expression variables / constants
API_EXPORT(int) mupGetExprVarNum(muParserHandle_t a_hParser);
API_EXPORT(int) mupGetVarNum(muParserHandle_t a_hParser);
API_EXPORT(int) mupGetConstNum(muParserHandle_t a_hParser);
API_EXPORT(void) mupGetExprVar(muParserHandle_t a_hParser, unsigned a_iVar, const muChar_t** a_pszName, muFloat_t** a_pVar);
API_EXPORT(void) mupGetVar(muParserHandle_t a_hParser, unsigned a_iVar, const muChar_t** a_pszName, muFloat_t** a_pVar);
API_EXPORT(void) mupGetConst(muParserHandle_t a_hParser, unsigned a_iVar, const muChar_t** a_pszName, muFloat_t* a_pVar);
API_EXPORT(void) mupSetArgSep(muParserHandle_t a_hParser, const muChar_t cArgSep);
API_EXPORT(void) mupSetDecSep(muParserHandle_t a_hParser, const muChar_t cArgSep);
API_EXPORT(void) mupSetThousandsSep(muParserHandle_t a_hParser, const muChar_t cArgSep);
API_EXPORT(void) mupResetLocale(muParserHandle_t a_hParser);

// Add value recognition callbacks
API_EXPORT(void) mupAddValIdent(muParserHandle_t a_hParser, muIdentFun_t);

// Error handling
API_EXPORT(muBool_t) mupError(muParserHandle_t a_hParser);
API_EXPORT(void) mupErrorReset(muParserHandle_t a_hParser);
API_EXPORT(void) mupSetErrorHandler(muParserHandle_t a_hParser, muErrorHandler_t a_pErrHandler);
API_EXPORT(const muChar_t*) mupGetErrorMsg(muParserHandle_t a_hParser);
API_EXPORT(muInt_t) mupGetErrorCode(muParserHandle_t a_hParser);
API_EXPORT(muInt_t) mupGetErrorPos(muParserHandle_t a_hParser);
API_EXPORT(const muChar_t*) mupGetErrorToken(muParserHandle_t a_hParser);
//API_EXPORT(const muChar_t*) mupGetErrorExpr(muParserHandle_t a_hParser);

// This is used for .NET only. It creates a new variable allowing the dll to
// manage the variable rather than the .NET garbage collector.
API_EXPORT(muFloat_t*) mupCreateVar();
API_EXPORT(void) mupReleaseVar(muFloat_t*);

#ifdef __cplusplus
}
#endif

#endif // include guard
