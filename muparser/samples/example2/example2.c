#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#include "muParserDLL.h"

#define PARSER_CONST_PI  3.141592653589793238462643
#define PARSER_CONST_E   2.718281828459045235360287
#define PARSER_MAXVARS		10

#ifndef _UNICODE
    #define _T(x) x
    #define myprintf printf
    #define mystrlen strlen
    #define myfgets fgets
    #define mystrcmp strcmp
#else
    #define _T(x) L ##x
    #define myprintf wprintf
    #define mystrlen wcslen
    #define myfgets fgetws
    #define mystrcmp wcscmp
#endif

extern void CalcBulk();

//---------------------------------------------------------------------------
// Callbacks for postfix operators
muFloat_t Mega(muFloat_t a_fVal)
{
    return a_fVal * 1.0e6;
}

muFloat_t Milli(muFloat_t a_fVal)
{
    return a_fVal / 1.0e3;
}

muFloat_t ZeroArg()
{
    myprintf(_T("i'm a function without arguments.\n"));
    return 123;
}

muFloat_t BulkTest(int nBulkIdx, int nThreadIdx, muFloat_t v1)
{
    (void*)&nThreadIdx;  // STFU compiler warning...
    myprintf(_T("%d,%2.2f\n"), nBulkIdx, v1);
    return v1 / (nBulkIdx + 1);
}

//---------------------------------------------------------------------------
// Callbacks for infix operators
muFloat_t Not(muFloat_t v) { return v == 0; }

//---------------------------------------------------------------------------
// Function callbacks
muFloat_t Rnd(muFloat_t v) { return v * rand() / (muFloat_t)(RAND_MAX + 1.0); }

muFloat_t SampleQuery(const muChar_t *szMsg)
{
    if (szMsg)
    {
        myprintf(_T("%s\n"), szMsg);
    }

    return 999;
}

muFloat_t Sum(const muFloat_t *a_afArg, int a_iArgc)
{
    muFloat_t fRes = 0;
    int i = 0;

    for (i = 0; i < a_iArgc; ++i)
        fRes += a_afArg[i];

    return fRes;
}

//---------------------------------------------------------------------------
// Binarty operator callbacks
muFloat_t Add(muFloat_t v1, muFloat_t v2)
{
    return v1 + v2;
}

muFloat_t Mul(muFloat_t v1, muFloat_t v2)
{
    return v1*v2;
}

//---------------------------------------------------------------------------
// Factory function for creating new parser variables
// This could as well be a function performing database queries.
muFloat_t* AddVariable(const muChar_t* a_szName, void *pUserData)
{
    static muFloat_t afValBuf[PARSER_MAXVARS];  // I don't want dynamic allocation here
    static int iVal = 0;                     // so i used this buffer

    myprintf(_T("Generating new variable \"%s\" (slots left: %d; context pointer: 0x%x)\n"), a_szName, PARSER_MAXVARS - iVal, (int)pUserData);

    afValBuf[iVal] = 0;
    if (iVal >= PARSER_MAXVARS - 1)
    {
        myprintf(_T("Variable buffer overflow."));
        return NULL;
    }

    return &afValBuf[iVal++];
}

//---------------------------------------------------------------------------
void Intro(muParserHandle_t hParser)
{
    myprintf(_T("                 __________                                       \n"));
    myprintf(_T("    _____   __ __\\______   \\_____  _______  ______  ____ _______\n"));
    myprintf(_T("   /     \\ |  |  \\|     ___/\\__  \\ \\_  __ \\/  ___/_/ __ \\\\_  __ \\ \n"));
    myprintf(_T("  |  Y Y  \\|  |  /|    |     / __ \\_|  | \\/\\___ \\ \\  ___/ |  | \\/ \n"));
    myprintf(_T("  |__|_|  /|____/ |____|    (____  /|__|  /____  > \\___  >|__|    \n"));
    myprintf(_T("        \\/                       \\/            \\/      \\/         \n"));
    myprintf(_T("  Version %s (DLL)\n"), mupGetVersion(hParser));
#ifdef _UNICODE
    myprintf(_T("  Sample build with UNICODE support\n"));
#else
    myprintf(_T("  Sample build with ASCII support\n"));
#endif
    myprintf(_T("  (C) 2015 Ingo Berg\n"));
    myprintf(_T("---------------------------------------\n"));
    myprintf(_T("Commands:\n"));
    myprintf(_T("  list var     - list parser variables\n"));
    myprintf(_T("  list exprvar - list expression variables\n"));
    myprintf(_T("  list const   - list all numeric parser constants\n"));
    myprintf(_T("  locale de    - switch to german locale\n"));
    myprintf(_T("  locale en    - switch to english locale\n"));
    myprintf(_T("  locale reset - reset locale\n"));
    myprintf(_T("  test bulk    - test bulk mode\n"));
    myprintf(_T("  quit         - exits the parser\n\n"));
    myprintf(_T("---------------------------------------\n"));
    myprintf(_T("Constants:\n"));
    myprintf(_T("  \"_e\"   2.718281828459045235360287\n"));
    myprintf(_T("  \"_pi\"  3.141592653589793238462643\n"));
    myprintf(_T("---------------------------------------\n"));
    myprintf(_T("Please enter an expression:\n"));
}

//---------------------------------------------------------------------------
// Callback function for parser errors
void OnError(muParserHandle_t hParser)
{
    myprintf(_T("\nError:\n"));
    myprintf(_T("------\n"));
    myprintf(_T("Message:  \"%s\"\n"), mupGetErrorMsg(hParser));
    myprintf(_T("Token:    \"%s\"\n"), mupGetErrorToken(hParser));
    myprintf(_T("Position: %d\n"), mupGetErrorPos(hParser));
    myprintf(_T("Errc:     %d\n"), mupGetErrorCode(hParser));
}

//---------------------------------------------------------------------------
void ListVar(muParserHandle_t a_hParser)
{
    int iNumVar = mupGetVarNum(a_hParser);
    int i = 0;

    if (iNumVar == 0)
    {
        myprintf(_T("No variables defined\n"));
        return;
    }

    myprintf(_T("\nExpression variables:\n"));
    myprintf(_T("---------------------\n"));
    myprintf(_T("Number: %d\n"), iNumVar);

    for (i = 0; i < iNumVar; ++i)
    {
        const muChar_t* szName = 0;
        muFloat_t* pVar = 0;

        mupGetVar(a_hParser, i, &szName, &pVar);
        myprintf(_T("Name: %s    Address: [0x%x]\n"), szName, (long long)pVar);
    }
}

//---------------------------------------------------------------------------
void ListExprVar(muParserHandle_t a_hParser)
{
    muInt_t iNumVar = mupGetExprVarNum(a_hParser),
        i = 0;

    if (iNumVar == 0)
    {
        myprintf(_T("Expression dos not contain variables\n"));
        return;
    }

    myprintf(_T("\nExpression variables:\n"));
    myprintf(_T("---------------------\n"));
    myprintf(_T("Expression: %s\n"), mupGetExpr(a_hParser));
    myprintf(_T("Number: %d\n"), iNumVar);

    for (i = 0; i < iNumVar; ++i)
    {
        const muChar_t* szName = 0;
        muFloat_t* pVar = 0;

        mupGetExprVar(a_hParser, i, &szName, &pVar);
        myprintf(_T("Name: %s   Address: [0x%x]\n"), szName, (long long)pVar);
    }
}

//---------------------------------------------------------------------------
void ListConst(muParserHandle_t a_hParser)
{
    muInt_t iNumVar = mupGetConstNum(a_hParser),
        i = 0;

    if (iNumVar == 0)
    {
        myprintf(_T("No constants defined\n"));
        return;
    }

    myprintf(_T("\nParser constants:\n"));
    myprintf(_T("---------------------\n"));
    myprintf(_T("Number: %d"), iNumVar);

    for (i = 0; i < iNumVar; ++i)
    {
        const muChar_t* szName = 0;
        muFloat_t fVal = 0;

        mupGetConst(a_hParser, i, &szName, &fVal);
        myprintf(_T("  %s = %f\n"), szName, fVal);
    }
}

//---------------------------------------------------------------------------
/** \brief Check for external keywords.
*/
int CheckKeywords(const muChar_t *a_szLine, muParserHandle_t a_hParser)
{
    if (!mystrcmp(a_szLine, _T("quit")))
    {
        return -1;
    }
    else if (!mystrcmp(a_szLine, _T("list var")))
    {
        ListVar(a_hParser);
        return 1;
    }
    else if (!mystrcmp(a_szLine, _T("list exprvar")))
    {
        ListExprVar(a_hParser);
        return 1;
    }
    else if (!mystrcmp(a_szLine, _T("list const")))
    {
        ListConst(a_hParser);
        return 1;
    }
    else if (!mystrcmp(a_szLine, _T("locale de")))
    {
        myprintf(_T("Setting german locale: ArgSep=';' DecSep=',' ThousandsSep='.'\n"));
        mupSetArgSep(a_hParser, ';');
        mupSetDecSep(a_hParser, ',');
        mupSetThousandsSep(a_hParser, '.');
        return 1;
    }
    else if (!mystrcmp(a_szLine, _T("locale en")))
    {
        myprintf(_T("Setting english locale: ArgSep=',' DecSep='.' ThousandsSep=''\n"));
        mupSetArgSep(a_hParser, ',');
        mupSetDecSep(a_hParser, '.');
        mupSetThousandsSep(a_hParser, 0);
        return 1;
    }
    else if (!mystrcmp(a_szLine, _T("locale reset")))
    {
        myprintf(_T("Resetting locale\n"));
        mupResetLocale(a_hParser);
        return 1;
    }
    else if (!mystrcmp(a_szLine, _T("test bulk")))
    {
        myprintf(_T("Testing bulk mode\n"));
        CalcBulk();
        return 1;
    }

    return 0;
}

//---------------------------------------------------------------------------
void CalcBulk()
{
    int nBulkSize = 200, i;
    muFloat_t *x = (muFloat_t*)malloc(nBulkSize * sizeof(muFloat_t));
    muFloat_t *y = (muFloat_t*)malloc(nBulkSize * sizeof(muFloat_t));
    muFloat_t *r = (muFloat_t*)malloc(nBulkSize * sizeof(muFloat_t));

    muParserHandle_t hParser = mupCreate(muBASETYPE_FLOAT);              // initialize the parser

    for (i = 0; i < nBulkSize; ++i)
    {
        x[i] = i;
        y[i] = i;
        r[i] = 0;
    }

    mupDefineVar(hParser, _T("x"), x);
    mupDefineVar(hParser, _T("y"), y);
    mupDefineBulkFun1(hParser, _T("bulktest"), BulkTest);
    mupSetExpr(hParser, _T("bulktest(x+y)"));
    mupEvalBulk(hParser, r, nBulkSize);
    if (mupError(hParser))
    {
        myprintf(_T("\nError:\n"));
        myprintf(_T("------\n"));
        myprintf(_T("Message:  %s\n"), mupGetErrorMsg(hParser));
        myprintf(_T("Token:    %s\n"), mupGetErrorToken(hParser));
        myprintf(_T("Position: %d\n"), mupGetErrorPos(hParser));
        myprintf(_T("Errc:     %d\n"), mupGetErrorCode(hParser));
        return;
    }

    for (i = 0; i < nBulkSize; ++i)
    {
        myprintf(_T("%d: bulkfun(%2.2f + %2.2f) = %2.2f\n"), i, x[i], y[i], r[i]);
        x[i] = i;
        y[i] = (muFloat_t)i / 10;
    }

    free(x);
    free(y);
    free(r);
}

//---------------------------------------------------------------------------
void Calc()
{
    muChar_t szLine[100];
    muFloat_t fVal = 0,
        afVarVal[] = { 1, 2 }; // Values of the parser variables
    muParserHandle_t hParser;

    hParser = mupCreate(muBASETYPE_FLOAT);              // initialize the parser
    Intro(hParser);

    // Set an error handler [optional]
    // the only function that does not take a parser instance handle
    mupSetErrorHandler(hParser, OnError);

    //#define GERMAN_LOCALS
#ifdef GERMAN_LOCALS
    mupSetArgSep(hParser, ';');
    mupSetDecSep(hParser, ',');
    mupSetThousandsSep(hParser, '.');
#else
    mupSetArgSep(hParser, ',');
    mupSetDecSep(hParser, '.');
#endif

    // Set a variable factory
    mupSetVarFactory(hParser, AddVariable, NULL);

    // Define parser variables and bind them to C++ variables [optional]
    mupDefineConst(hParser, _T("const1"), 1);
    mupDefineConst(hParser, _T("const2"), 2);
    mupDefineStrConst(hParser, _T("strBuf"), _T("Hallo welt"));

    // Define parser variables and bind them to C++ variables [optional]
    mupDefineVar(hParser, _T("a"), &afVarVal[0]);
    mupDefineVar(hParser, _T("b"), &afVarVal[1]);

    // Define postfix operators [optional]
    mupDefinePostfixOprt(hParser, _T("M"), Mega, 0);
    mupDefinePostfixOprt(hParser, _T("m"), Milli, 0);

    // Define infix operator [optional]
    mupDefineInfixOprt(hParser, _T("!"), Not, 0);

    // Define functions [optional]
    //  mupDefineStrFun(hParser, "query", SampleQuery, 0); // Add an unoptimizeable function 
    mupDefineFun0(hParser, _T("zero"), ZeroArg, 0);
    mupDefineFun1(hParser, _T("rnd"), Rnd, 0);             // Add an unoptimizeable function
    mupDefineFun1(hParser, _T("rnd2"), Rnd, 1);
    mupDefineMultFun(hParser, _T("_sum"), Sum, 0);  // "sum" is already a default function

    // Define binary operators [optional]
    mupDefineOprt(hParser, _T("add"), Add, 0, muOPRT_ASCT_LEFT, 0);
    mupDefineOprt(hParser, _T("mul"), Mul, 1, muOPRT_ASCT_LEFT, 0);

    while (myfgets(szLine, 99, stdin))
    {
        szLine[mystrlen(szLine) - 1] = 0; // overwrite the newline

        switch (CheckKeywords(szLine, hParser))
        {
        case  0:  break;       // no keyword found; parse the line
        case  1:  continue;    // A Keyword was found do not parse the line
        case -1:  return;      // abort the application
        }

        mupSetExpr(hParser, szLine);

        fVal = mupEval(hParser);


        // Without an Error handler function 
        // you must use this for error treatment:
        //if (mupError(hParser))
        //{
        //  printf("\nError:\n");
        //  printf("------\n");
        //  printf("Message:  %s\n", mupGetErrorMsg(hParser) );
        //  printf("Token:    %s\n", mupGetErrorToken(hParser) );
        //  printf("Position: %s\n", mupGetErrorPos(hParser) );
        //  printf("Errc:     %d\n", mupGetErrorCode(hParser) );
        //  continue;
        //}

        if (!mupError(hParser))
            myprintf(_T("%f\n"), fVal);

    } // while 

    // finalle free the parser ressources
    mupRelease(hParser);
}

//---------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    // The next line is just for shutting up the compiler warning
    // about unused variables without getting another warning about not
    // beeing able to use type lists in function declarations.
    myprintf(_T("Executing \"%s\" (argc=%d)\n"), argv[0], argc);
    Calc();
    printf(_T("done..."));
}
