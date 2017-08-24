//---------------------------------------------------------------------------
//
//                 __________                                      
//    _____   __ __\______   \_____  _______  ______  ____ _______ 
//   /     \ |  |  \|     ___/\__  \ \_  __ \/  ___/_/ __ \\_  __ \ 
//  |  Y Y  \|  |  /|    |     / __ \_|  | \/\___ \ \  ___/ |  | \/
//  |__|_|  /|____/ |____|    (____  /|__|  /____  > \___  >|__|   
//        \/                       \/            \/      \/        
//  (C) 2015 Ingo Berg
//
//  example1.cpp - using the parser as a static library
//
//---------------------------------------------------------------------------

#include "muParserTest.h"

#if defined(_WIN32) && defined(_DEBUG)
  #define _CRTDBG_MAP_ALLOC
  #include <stdlib.h>
  #include <crtdbg.h>
  #define CREATE_LEAKAGE_REPORT
#endif

#if defined( USINGDLL ) && defined( _WIN32 )
#error This sample can be used only with STATIC builds of muParser (on win32)
#endif

/** \brief This macro will enable mathematical constants like M_PI. */
#define _USE_MATH_DEFINES		

#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <iostream>
#include <locale>
#include <limits>
#include <ios> 
#include <iomanip>
#include <numeric>

#include "muParser.h"

using namespace std;
using namespace mu;


#if defined(CREATE_LEAKAGE_REPORT)

// Dumping memory leaks in the destructor of the static guard
// guarantees i won't get false positives from the ParserErrorMsg 
// class wich is a singleton with a static instance.
struct DumpLeaks
{
 ~DumpLeaks()
  {
    _CrtDumpMemoryLeaks();
  }
} static LeakDumper;

#endif

// Forward declarations
void CalcBulk();

// Operator callback functions
value_type Mega(value_type a_fVal) { return a_fVal * 1e6; }
value_type Milli(value_type a_fVal) { return a_fVal / (value_type)1e3; }
value_type Rnd(value_type v) { return v*std::rand()/(value_type)(RAND_MAX+1.0); }
value_type Not(value_type v) { return v==0; }
value_type Add(value_type v1, value_type v2) { return v1+v2; }
value_type Mul(value_type v1, value_type v2) { return v1*v2; }

//---------------------------------------------------------------------------
value_type ThrowAnException(value_type) 
{ 
  throw std::runtime_error("This function does throw an exception.");
}

//---------------------------------------------------------------------------
value_type BulkFun1(int nBulkIdx, int nThreadIdx, value_type v1)
{
  // Note: I'm just doing something with all three parameters to shut 
  // compiler warnings up!
  return nBulkIdx + nThreadIdx + v1;
}

//---------------------------------------------------------------------------
value_type Ping() 
{ 
  mu::console() << "ping\n"; 
  return 0; 
}

//---------------------------------------------------------------------------
value_type StrFun0(const char_type *szMsg) 
{
  if (szMsg) 
    mu::console() << szMsg << std::endl;

  return 999;
}

//---------------------------------------------------------------------------
value_type StrFun2(const char_type *v1, value_type v2,value_type v3) 
{ 
  mu::console() << v1 << std::endl;
  return v2+v3; 
}

//---------------------------------------------------------------------------
value_type Debug(mu::value_type v1, mu::value_type v2) 
{ 
  ParserBase::EnableDebugDump(v1!=0, v2!=0);
  mu::console() << _T("Bytecode dumping ") << ((v1!=0) ? _T("active") : _T("inactive")) << _T("\n");
  return 1; 
}

//---------------------------------------------------------------------------
// Factory function for creating new parser variables
// This could as well be a function performing database queries.
value_type* AddVariable(const char_type *a_szName, void *a_pUserData)
{
  // I don't want dynamic allocation here, so i used this static buffer
  // If you want dynamic allocation you must allocate all variables dynamically
  // in order to delete them later on. Or you find other ways to keep track of 
  // variables that have been created implicitely.
  static value_type afValBuf[100];  
  static int iVal = -1;

  ++iVal;

  mu::console() << _T("Generating new variable \"") 
                << a_szName << std::dec << _T("\" (slots left: ")
                << 99-iVal << _T(")")
                << _T(" User data pointer is:") 
                << std::hex << a_pUserData <<endl;
  afValBuf[iVal] = 0;

  if (iVal>=99)
    throw mu::ParserError( _T("Variable buffer overflow.") );
  else
    return &afValBuf[iVal];
}

int IsHexValue(const char_type *a_szExpr, int *a_iPos, value_type *a_fVal) 
{ 
  if (a_szExpr[1]==0 || (a_szExpr[0]!='0' || a_szExpr[1]!='x') ) 
    return 0;

  unsigned iVal(0);

  // New code based on streams for UNICODE compliance:
  stringstream_type::pos_type nPos(0);
  stringstream_type ss(a_szExpr + 2);
  ss >> std::hex >> iVal;
  nPos = ss.tellg();

  if (nPos==(stringstream_type::pos_type)0)
    return 1;

  *a_iPos += (int)(2 + nPos);
  *a_fVal = (value_type)iVal;

  return 1;
}

//---------------------------------------------------------------------------
void Splash()
{
  mu::console() << _T("                 __________                                       \n");
  mu::console() << _T("    _____   __ __\\______   \\_____  _______  ______  ____ _______\n");
  mu::console() << _T("   /     \\ |  |  \\|     ___/\\__  \\ \\_  __ \\/  ___/_/ __ \\\\_  __ \\ \n");
  mu::console() << _T("  |  Y Y  \\|  |  /|    |     / __ \\_|  | \\/\\___ \\ \\  ___/ |  | \\/ \n");
  mu::console() << _T("  |__|_|  /|____/ |____|    (____  /|__|  /____  > \\___  >|__|    \n");
  mu::console() << _T("        \\/                       \\/            \\/      \\/         \n");
  mu::console() << _T("  Version ") << Parser().GetVersion(pviFULL) << _T("\n");
  mu::console() << _T("  (C) 2015 Ingo Berg\n");
}

//---------------------------------------------------------------------------
value_type SelfTest()
{
  mu::console() << _T( "-----------------------------------------------------------\n");
  mu::console() << _T( "Running test suite:\n\n");

  // Skip the self test if the value type is set to an integer type.
  if (mu::TypeInfo<mu::value_type>::IsInteger())
  {
    mu::console() << _T( "  Test skipped: integer data type are not compatible with the unit test!\n\n");
  }
  else
  {
    mu::Test::ParserTester pt;
    pt.Run();
  }

  return 0;
}

//---------------------------------------------------------------------------
value_type Help()
{
  mu::console() << _T( "-----------------------------------------------------------\n");
  mu::console() << _T( "Commands:\n\n");
  mu::console() << _T( "  list var     - list parser variables\n");
  mu::console() << _T( "  list exprvar - list expression variables\n");
  mu::console() << _T( "  list const   - list all numeric parser constants\n");
  mu::console() << _T( "  opt on       - enable optimizer (default)\n");
  mu::console() << _T( "  opt off      - disable optimizer\n");
  mu::console() << _T( "  locale de    - switch to german locale\n");
  mu::console() << _T( "  locale en    - switch to english locale\n");
  mu::console() << _T( "  locale reset - reset locale\n");
  mu::console() << _T( "  test bulk    - test bulk mode\n");
  mu::console() << _T( "  quit         - exits the parser\n");
  mu::console() << _T( "\nConstants:\n\n");
  mu::console() << _T( "  \"_e\"   2.718281828459045235360287\n");
  mu::console() << _T( "  \"_pi\"  3.141592653589793238462643\n");
  mu::console() << _T( "-----------------------------------------------------------\n");
  return 0;
}

//---------------------------------------------------------------------------
/*
void CheckLocale()
{
  // Local names:
  // "C" - the classic C locale
  // "de_DE" - not for Windows?
  // "en_US" - not for Windows?
  // "German_germany" - For MSVC8
  try
  {
    std::locale loc("German_germany");
    console() << _T("Locale settings:\n");
    console() << _T("  Decimal point:  '") << std::use_facet<numpunct<char_type> >(loc).decimal_point() << _T("'\n"); 
    console() << _T("  Thousands sep:  '") << std::use_facet<numpunct<char_type> >(loc).thousands_sep() << _T("'\n"); 
    console() << _T("  Grouping:       '") << std::use_facet<numpunct<char_type> >(loc).grouping()  << _T("'\n"); 
    console() << _T("  True is named:  '") << std::use_facet<numpunct<char_type> >(loc).truename()  << _T("'\n"); 
    console() << _T("  False is named: '") << std::use_facet<numpunct<char_type> >(loc).falsename() << _T("'\n"); 
    console() << _T("-----------------------------------------------------------\n");
  }
  catch(...)
  {
    console() << _T("Locale settings:\n");
    console() << _T("  invalid locale name\n");
    console() << _T("-----------------------------------------------------------\n");
  }
}

//---------------------------------------------------------------------------
void CheckDiff()
{
  mu::Parser  parser;
  value_type x = 1, 
             v1,
             v2,
             v3,
             eps(pow(std::numeric_limits<value_type>::epsilon(), 0.2));
  parser.DefineVar(_T("x"), &x);
  parser.SetExpr(_T("_e^-x*sin(x)"));
  
  v1 = parser.Diff(&x, 1),
  v2 = parser.Diff(&x, 1, eps);
  v3 = cos((value_type)1.0)/exp((value_type)1) - sin((value_type)1.0)/exp((value_type)1); //-0.110793765307;
  mu::console() << parser.GetExpr() << _T("\n");
  mu::console() << _T("v1 = ") << v1 << _T("; v1-v3 = ") << v1-v3 << _T("\n");
  mu::console() << _T("v2 = ") << v2 << _T("; v2-v3 = ") << v2-v3 << _T("\n");
}
*/

//---------------------------------------------------------------------------
void ListVar(const mu::ParserBase &parser)
{
  // Query the used variables (must be done after calc)
  mu::varmap_type variables = parser.GetVar();
  if (!variables.size())
    return;

  cout << "\nParser variables:\n";
  cout <<   "-----------------\n";
  cout << "Number: " << (int)variables.size() << "\n";
  varmap_type::const_iterator item = variables.begin();
  for (; item!=variables.end(); ++item)
    mu::console() << _T("Name: ") << item->first << _T("   Address: [0x") << item->second << _T("]\n");
}

//---------------------------------------------------------------------------
void ListConst(const mu::ParserBase &parser)
{
  mu::console() << _T("\nParser constants:\n");
  mu::console() << _T("-----------------\n");

  mu::valmap_type cmap = parser.GetConst();
  if (!cmap.size())
  {
    mu::console() << _T("Expression does not contain constants\n");
  }
  else
  {
    valmap_type::const_iterator item = cmap.begin();
    for (; item!=cmap.end(); ++item)
      mu::console() << _T("  ") << item->first << _T(" =  ") << item->second << _T("\n");
  }
}

//---------------------------------------------------------------------------
void ListExprVar(const mu::ParserBase &parser)
{
  string_type sExpr = parser.GetExpr();
  if (sExpr.length()==0)
  {
    cout << _T("Expression string is empty\n");
    return;
  }

  // Query the used variables (must be done after calc)
  mu::console() << _T("\nExpression variables:\n");
  mu::console() <<   _T("---------------------\n");
  mu::console() << _T("Expression: ") << parser.GetExpr() << _T("\n");

  varmap_type variables = parser.GetUsedVar();
  if (!variables.size())
  {
    mu::console() << _T("Expression does not contain variables\n");
  }
  else
  {
    mu::console() << _T("Number: ") << (int)variables.size() << _T("\n");
    mu::varmap_type::const_iterator item = variables.begin();
    for (; item!=variables.end(); ++item)
      mu::console() << _T("Name: ") << item->first << _T("   Address: [0x") << item->second << _T("]\n");
  }
}

//---------------------------------------------------------------------------
/** \brief Check for external keywords.
*/
int CheckKeywords(const mu::char_type *a_szLine, mu::Parser &a_Parser)
{
  string_type sLine(a_szLine);

  if ( sLine == _T("quit") )
  {
    return -1;
  }
  else if ( sLine == _T("list var") )
  {
    ListVar(a_Parser);
    return 1;
  }
  else if ( sLine == _T("opt on") )
  {
    a_Parser.EnableOptimizer(true);
    mu::console() << _T("Optimizer enabled\n");
    return 1;
  }
  else if ( sLine == _T("opt off") )
  {
    a_Parser.EnableOptimizer(false);
    mu::console() << _T("Optimizer disabled\n");
    return 1;
  }
  else if ( sLine == _T("list const") )
  {
    ListConst(a_Parser);
    return 1;
  }
  else if ( sLine == _T("list exprvar") )
  {
    ListExprVar(a_Parser);
    return 1;
  }
  else if ( sLine == _T("locale de") )
  {
    mu::console() << _T("Setting german locale: ArgSep=';' DecSep=',' ThousandsSep='.'\n");
    a_Parser.SetArgSep(';');
    a_Parser.SetDecSep(',');
    a_Parser.SetThousandsSep('.');
    return 1;
  }
  else if ( sLine == _T("locale en") )
  {
    mu::console() << _T("Setting english locale: ArgSep=',' DecSep='.' ThousandsSep=''\n");
    a_Parser.SetArgSep(',');
    a_Parser.SetDecSep('.');
    a_Parser.SetThousandsSep();
    return 1;
  }
  else if ( sLine == _T("locale reset") )
  {
    mu::console() << _T("Resetting locale\n");
    a_Parser.ResetLocale();
    return 1;
  }
  else if ( sLine == _T("test bulk") )
  {
    mu::console() << _T("Testing bulk mode\n");
    CalcBulk();
    return 1;
  }

  return 0;
}

//---------------------------------------------------------------------------
void CalcBulk()
{
  const int nBulkSize = 200;
  value_type *x = new value_type[nBulkSize];
  value_type *y = new value_type[nBulkSize];
  value_type *result = new value_type[nBulkSize];

  try
  {
    for (int i=0; i<nBulkSize; ++i)
    {
      x[i] = i;
      y[i] = (value_type)i/10;
    }
    mu::Parser  parser;
    parser.DefineVar(_T("x"), x);
    parser.DefineVar(_T("y"), y);
    parser.DefineFun(_T("fun1"), BulkFun1);
    parser.SetExpr(_T("fun1(0)+x+y"));
    parser.Eval(result, nBulkSize);

    for (int i=0; i<nBulkSize; ++i)
    {
      mu::console() << _T("Eqn. ") << i << _T(": x=") << x[i] << _T("; y=") << y[i] << _T("; result=") << result[i] << _T("\n");
    }
  }
  catch(...)
  {
    delete [] x;
    delete [] y;
    delete [] result;
    throw;
  }

  delete [] x;
  delete [] y;
  delete [] result;
}

//---------------------------------------------------------------------------
void Calc()
{
  mu::Parser  parser;

  // Change locale settings if necessary
  // function argument separator:   sum(2;3;4) vs. sum(2,3,4)
  // decimal separator:             3,14       vs. 3.14
  // thousands separator:           1000000    vs 1.000.000
//#define USE_GERMAN_LOCALE
#ifdef  USE_GERMAN_LOCALE
  parser.SetArgSep(';');
  parser.SetDecSep(',');
  parser.SetThousandsSep('.');
#else
  // this is the default, so i it's commented:
  //parser.SetArgSep(',');
  //parser.SetDecSep('.');
  //parser.SetThousandsSep('');
#endif

  // Add some variables
  value_type  vVarVal[] = { 1, 2 }; // Values of the parser variables
  parser.DefineVar(_T("a"), &vVarVal[0]);  // Assign Variable names and bind them to the C++ variables
  parser.DefineVar(_T("b"), &vVarVal[1]);
  parser.DefineVar(_T("ft"), &vVarVal[1]);
  parser.DefineStrConst(_T("sVar1"), _T("Sample string 1") );
  parser.DefineStrConst(_T("sVar2"), _T("Sample string 2") );
  parser.AddValIdent(IsHexValue);

  // Add user defined unary operators
  parser.DefinePostfixOprt(_T("M"), Mega);
  parser.DefinePostfixOprt(_T("m"), Milli);
  parser.DefineInfixOprt(_T("!"), Not);
  parser.DefineFun(_T("strfun0"), StrFun0);
  parser.DefineFun(_T("strfun2"), StrFun2);
  parser.DefineFun(_T("ping"), Ping);
  parser.DefineFun(_T("rnd"), Rnd);     // Add an unoptimizeable function
  parser.DefineFun(_T("throw"), ThrowAnException);

  
  parser.DefineOprt(_T("add"), Add, 0);
  parser.DefineOprt(_T("mul"), Mul, 1);

  // These are service and debug functions
  parser.DefineFun(_T("debug"), Debug);
  parser.DefineFun(_T("selftest"), SelfTest);
  parser.DefineFun(_T("help"), Help);

  parser.DefinePostfixOprt(_T("{ft}"), Milli);
  parser.DefinePostfixOprt(_T("ft"), Milli);
#ifdef _DEBUG
//  parser.EnableDebugDump(1, 0);
#endif

  // Define the variable factory
  parser.SetVarFactory(AddVariable, &parser);

  for(;;)
  {
    try
    {
      string_type sLine;
      std::getline(mu::console_in(), sLine);

      switch (CheckKeywords(sLine.c_str(), parser))
      {
      case  0: break;
      case  1: continue;
      case -1: return;
      }

      if (!sLine.length())
        continue;

      parser.SetExpr(sLine);
      mu::console() << std::setprecision(12);

      // There are multiple ways to retrieve the result...
      // 1.) If you know there is only a single return value or in case you only need the last 
      //     result of an expression consisting of comma separated subexpressions you can 
      //     simply use: 
      mu::console() << _T("ans=") << parser.Eval() << _T("\n");

      // 2.) As an alternative you can also retrieve multiple return values using this API:
      int nNum = parser.GetNumResults();
      if (nNum>1)
      {
        mu::console() << _T("Multiple return values detected! Complete list:\n");

        // this is the hard way if you need to retrieve multiple subexpression
        // results
        value_type *v = parser.Eval(nNum);
        mu::console() << std::setprecision(12);
        for (int i=0; i<nNum; ++i)
        {
          mu::console() << v[i] << _T("\n");
        }
      }
    }
    catch(mu::Parser::exception_type &e)
    {
      mu::console() << _T("\nError:\n");
      mu::console() << _T("------\n");
      mu::console() << _T("Message:     ")   << e.GetMsg()   << _T("\n");
      mu::console() << _T("Expression:  \"") << e.GetExpr()  << _T("\"\n");
      mu::console() << _T("Token:       \"") << e.GetToken()    << _T("\"\n");
      mu::console() << _T("Position:    ")   << (int)e.GetPos() << _T("\n");
      mu::console() << _T("Errc:        ")   << std::dec << e.GetCode() << _T("\n");
    }
  } // while running
}

//---------------------------------------------------------------------------
int main(int, char**)
{
  Splash();
  SelfTest();
  Help();

//  CheckLocale();
//  CheckDiff();

  mu::console() << _T("Enter an expression or a command:\n");

  try
  {
    Calc();
  }
  catch(Parser::exception_type &e)
  {
    // Only erros raised during the initialization will end up here
    // formula related errors are treated in Calc()
    console() << _T("Initialization error:  ") << e.GetMsg() << endl;
    console() << _T("aborting...") << endl;
    string_type sBuf;
    console_in() >> sBuf;
  }
  catch(std::exception & /*exc*/)
  {
    // there is no unicode compliant way to query exc.what()
    // so i'll leave it for this example.
    console() << _T("aborting...\n");
  }

  return 0;
}
