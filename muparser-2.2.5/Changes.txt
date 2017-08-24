#######################################################################
#                                                                     #
#                                                                     #
#                 __________                                          #
#    _____   __ __\______   \_____  _______  ______  ____ _______     #
#   /     \ |  |  \|     ___/\__  \ \_  __ \/  ___/_/ __ \\_  __ \    #
#  |  Y Y  \|  |  /|    |     / __ \_|  | \/\___ \ \  ___/ |  | \/    #
#  |__|_|  /|____/ |____|    (____  /|__|  /____  > \___  >|__|       #
#        \/                       \/            \/      \/            #
#					  Fast math parser Library                        #
#                                                                     #
#  Copyright (C) 2015 Ingo Berg                                       #
#                                                                     #
#  Web:     muparser.beltoforion.de                                   #
#  e-mail:  muparser@beltoforion.de                                   #
#                                                                     #
#                                                                     #
#######################################################################
 

History:
--------

Rev 2.2.5: 27.04.2015
---------------------
  Changes:
   * example2 extended to work with UNICODE character set
   * Applied patch from Issue 9
   
  Bugfixes:
   * muChar_t in muParserDLL.h was not set properly when UNICODE was used
   * muparser.dll did not build on UNICODE systems
   
Rev 2.2.4: 02.10.2014
---------------------
  Changes:
   * explicit positive sign allowed

  Bugfixes:
   * Fix for Issue 6 (https://code.google.com/p/muparser/issues/detail?id=6)
   * String constants did not work properly. Using more than a single one 
     was impossible.
   * Project Files for VS2008 and VS2010 removed from the repository 
   * Fix for Issue 4 (https://code.google.com/p/muparser/issues/detail?id=4)
   * Fix for VS2013 64 bit build option
   * return type of ParserError::GetPos changed to int
   * OpenMP support enabled in the VS2013 project files and precompiled windows DLL's
   * Bulkmode did not evaluate properly if "=" and "," operator was used in the expression
	
Rev 2.2.3: 22.12.2012
---------------------

  Removed features:
   * build files for msvc2005, borland and watcom compiler were removed

  Bugfixes:
   * Bugfix for Intel Compilers added: The power operator did not work properly 
     with Intel C++ composer XE 2011.
     (see https://sourceforge.net/projects/muparser/forums/forum/462843/topic/5117983/index/page/1)
   * Issue 3509860: Callbacks of functions with string parameters called twice
     (see http://sourceforge.net/tracker/?func=detail&aid=3509860&group_id=137191&atid=737979)
   * Issue 3570423: example1 shows slot number in hexadecimal
     (see https://sourceforge.net/tracker/?func=detail&aid=3570423&group_id=137191&atid=737979)
   * Fixes for compiling with the "MUP_MATH_EXCEPTIONS" macro definition:
 	- division by zero in constant expressions was reported with the code "ec_GENERIC" 
          instead of "ecDIV_BY_ZERO"
        - added throwing of "ecDOMAIN_ERROR" to sqrt and log functions


Rev 2.2.2: 18.02.2012
---------------------
  Bugfixes:
   * Optimizer did'nt work properly for division:
     (see https://sourceforge.net/projects/muparser/forums/forum/462843/topic/5037825)

Rev 2.2.1: 22.01.2012
---------------------
  Bugfixes:
   * Optimizer bug in 64 bit systems fixed
     (see https://sourceforge.net/projects/muparser/forums/forum/462843/topic/4977977/index/page/1)
	 
Rev 2.2.0: 22.01.2012
---------------------
  Improvements:
   * Optimizer rewritten and improved. In general: more optimizations are 
     now applied to the bytecode. The downside is that callback Functions 
     can no longer be flagged as non-optimizable. (The flag is still present 
     but ignored) This is necessary since the optimizer had to call the 
     functions in order to precalculate the result (see Bugfixes). These calls 
     posed a problems for callback functions with side  effects and if-then-else 
     clauses in general since they undermined the shortcut evaluation prinziple.

  Bugfixes:
   * Infix operators where not properly detected in the presence of a constant 
     name starting with an underscore which is a valid character for infix 
     operators too (i.e. "-_pi").
   * Issue 3463353: Callback functions are called twice during the first call to eval.
   * Issue 3447007: GetUsedVar unnecessaryly executes callback functions.


Rev 2.1.0: 19.11.2011
---------------------
  New feature:
   * Function atan2 added

  Bugfixes:
   * Issue 3438380:  Changed behaviour of tellg with GCC >4.6 led to failures  
                     in value detection callbacks.
   * Issue 3438715:  only "double" is a valid MUP_BASETYPE 
                     MUP_BASETYPE can now be any of:
                       float, 
                       double, 
                       long double, 
                       short, 
                       unsigned short, 
                       unsigned int, 
                       long,
                       unsigned long. 
                     Previousely only floating point types were allowed. 
                     Using "int" is still not allowed!
   * Compiler issues with GCC 4.6 fixed
   * Custom value recognition callbacks added with AddValIdent had lower
     priority than built in functions. This was causing problems with 
     hex value recognition since detection of non hex values had priority
     over the detection of hex values. The "0" in the hex prefix "0x" would 
     be read as a separate non-hex number leaving the rest of the expression
     unparseable.

Rev 2.0.0: 04.09.2011
---------------------
This release introduces a new version numbering scheme in order to make
future changes in the ABI apparent to users of the library. The number is
now based on the SONAME property as used by GNU/Linux. 

  Changes:
   * Beginning with this version all version numbers will be SONAME compliant
   * Project files for MSVC2010 added
   * Project files for MSVC2003 removed
   * Bytecode parsing engine cleaned up and rewritten
   * Retrieving all results of expressions made up of comma separate 
     subexpressions is now possible with a new Eval overload.
   * Callback functions with fixed number of arguments can now have up to 10 
     Parameters (previous limit was 5)

  New features:
   * ternary if-then-else operator added (C++ like; "(...) ? ... : ..." )
   * new intrinsic binary operators: "&&", "||" (logical and, or)
   * A new bulkmode allows submitting large arrays as variables to compute large 
     numbers of expressions with a single call. This can drastically improve 
     parsing performance when interfacing the library from managed languages like 
     C#. (It doesn't bring any performance benefit for C++ users though...)

  Removed features:
   * intrinsic "and", "or" and "xor" operators have been removed. I'd like to let 
     users the freedom of defining them on their own versions (either as logical or bitwise 
     operators).
   * Implementation for complex numbers removed. This was merely a hack. If you 
     need complex numbers try muParserX which provides native support for them.
     (see: http://beltoforion.de/muparserx/math_expression_parser_en.html)

  Bugfixes:
   * User defined operators could collide with built in operators that entirely 
     contained their identifier. i.e. user defined "&" would not work with the built 
     in "&&" operator since the user defined operator was detected with a higher 
     priority resulting in a syntax error.
   * Detection of unknown variables did not work properly in case a postfix operator
     was defined which was part of the undefined variable.
     i.e. If a postfix operator "m" was defined expressions like "multi*1.0" did
     not detect "multi" as an undefined variable.
     (Reference:  http://sourceforge.net/tracker/index.php?func=detail&aid=3343891&group_id=137191&atid=737979)
   * Postfix operators sharing the first few characters were causing bogus parsing exception.
     (Reference: https://sourceforge.net/tracker/?func=detail&aid=3358571&group_id=137191&atid=737979)

Rev 1.34: 04.09.2010
--------------------
  Changes:
   * The prefix needed for parsing hex values is now "0x" and no longer "$". 
   * AddValIdent reintroduced into the DLL interface

  New features:
   * The associativity of binary operators can now be changed. The pow operator 
     is now right associative. (This is what Mathematica is using) 
   * Seperator can now be used outside of functions. This allows compound 
     expressions like:
     "a=10,b=20,c=a*b" The last "argument" will be taken as the return value

  Bugfixes:	
   * The copy constructor did not copy binary operator definitions. Those were lost
     in the copied parser instance.
   * Mixing special characters and alphabetic characters in binary operator names 
     led to inconsistent parsing behaviour when parsing expressions like "a ++ b" 
     and "a++b" when "++" is defined as a binary operator. Binary operators must 
     now consist entirely of special characters or of alphabetic ones.
     (original bug report: https://sourceforge.net/projects/muparser/forums/forum/462843/topic/3696881/index/page/1)
   * User defined operators were not exactly handled like built in operators. This 
     led to inconsistencies in expression evaluation when using them. The results 
     differed due to slightly different precedence rules.
   * Using empty string arguments ("") would cause a crash of muParser


Rev 1.32: 29.01.2010 
--------------------

  Changes:
   * "example3" renamed to "example2"
   * Project/Makefiles files are now provided for:
           - msvc2003 
           - msvc2005 
           - msvc2008
           - watcom   (makefile)
           - mingw    (makefile)
           - bcc      (makefile)
   * Project files for borland cpp builder were removed


  New features:
   * Added function returning muparsers version number
   * Added function for resetting the locale


  Bugfixes:	
   * Changes example1 in order to fix issues with irritating memory leak reports. 
     Added conditional code for memory leak detection with MSVC in example1.
     (see: http://www.codeproject.com/KB/recipes/FastMathParser.aspx?msg=3286367#xx3286367xx)
   * Fixed some warnings for gcc



Rev 1.31cp: 15.01.2010  (Maintainance release for CodeProject)
----------------------

  Changes:
   * Archive structure changed
   * C# wrapper added
   * Fixed issued that prevented compiling with VS2010 Beta2
    

Rev 1.30: 09.06.2008
--------------------

  Changes:
   * Epsilon of the numerical differentiation algorithm changed to allow greater accuracy.

  New features:
   * Setting thousands separator and decimal separator is now possible

  Bugfixes:
   * The dll interface did not provide a callback for functions without any arguments.	 
	

Rev 1.29: Januar 2008
---------------------

  Unrelease Version available only via SVN.


Rev 1.28: 02. July, 2007
---------------------------
 
  Library changes:
  * Interface for the dynamic library changed and extended to create an interface 
    using pure C functions only. 
  * mupInit() removed

  Build system:
   * MSVC7 Project files removed in favor of MSVC8

  Bugfixes:
   * The dynamic library did not build on other systems than linux due to a misplaced 
     preprocessor definition. This is fixed now.


Rev 1.27:
---------------------------

  Build system:
   * Modified build\ directory layout introducing some subfolders 
     for the various IDE supported
   * Project files for BCB and MSVC7 added
   * Switched to use bakefile 0.2.1 which now correctly creates the 
     "make uninstall" target for autoconf's Makefile.in
   * Now the library debug builds are named "muparserd" instead of "muparser"
     to allow multiple mixed release/debug builds to coexist; so e.g. on Windows
     when building with DEBUG=1, you'll get "muparserd.lib" instead of "muparser.lib"

  New Features:
   * Factory functions can now take a user defined pointer
   * String functions can now be used with up to two additional 
     double parameters
   * Support for UNICODE character types added
   * Infix operator priority can now be changed by the user

  Bugfixes:
   * An internal error was raised when evaluating an empty 
     expressions
   * The error message raised in case of name collisions between 
     implicitely defined variables and postfix operators did contain
     misleading data.


Rev 1.26: (unofficial release)
------------------------------

  New Features:
   * Unary operator precedence can now be changed by the user.


Rev 1.25: 5. February, 2006
---------------------------

  Build system: (special thanks to Francesco Montorsi for implementing it!)
    * created a bakefile-based build system which adds support for the following win32 compilers:
      -> MS visual C++ (6 and .NET)
      -> BorlandC++ (5 or greater)
      -> Mingw32 (tested with gcc 3.2)
      -> Watcom (not tested)
      and for GCC on Unix (using a standard autoconf's configure script).

  Compatibility improvements:
    * fixed some small warnings when using -Wall with GCC on Unix
    * added inclusion guards for win32-specific portions of code
    * added fixes that remove compiler warnings on Intel C++ and the Solaris C++ compiler.


Rev 1.24: 29. October, 2005
---------------------------

Changes:

  Compatibility improvements:
    * parser now works on 64 bit compilers
    * (bytecode base datatype can now be changed freely)


Rev 1.23: 19. October, 2005
---------------------------

Changes:

  Bugfixes:
    * Variable factory examples in Example1.cpp and Example3.cpp contained a subtle bug.

  New features:
    * Added a MSVC6 project file and introduced muParserFixes.h in order to make it compile with MSVC6


Rev 1.22: October, 2005
-----------------------

Release notes:

All features of Version 1.22 are similar to Version 1.21. Version 1.22 fixes a compilation issue with
gcc 4.0. In order to fix this issue I rewrote part of the library to remove some unnecessary templates. 
This should make the code cleaner. The Borland Project files were removed. If you want to use it 
with Borland either use the dll version or create your own project files. I can't support it since I don't 
have this compiler at hand.

Changes:

  Project Changes:
    * Borland project files removed
      (The code should still compile with BCB but I cant provide you with project files)

  Internal Changes:
    * unnecessary template files have been removed:
        - new files: muParserError.cpp, muParserTokenReader.cpp, muParserCallback.cpp
        - removed Files: muIParserTypes.h


Rev 1.2 / 1.21: April, 2005
---------------------------

Release Notes:
First of all the interface has changed so this version is not backwards compatible.
After receiving a couple of questions about it, this version features support for
user defined binary operators. Consequently the built in operators can now be 
turned off, thus you can deactivate them and write complete customized parser
subclasses that only contain the functionality you want. Another new feature is 
the introduction of callback functions taking string arguments, implicit 
generation of variables and the Assignement operator.

  Functionality
    * New built in operator: xor; Logical xor.
    * New built in operator: Assignement operator; Defining variables in terms of 
                             other variables/constants
    * New feature: Strings as arguments for callback functions
    * New feature: User defined binary operators
    * New feature: ParserInt a class with a sample implementation for
                     integer numbers.
    * New feature: Callbacks to value regognition functions.

    * Removed:  all predefined postfix operators have been removed.
    * New project file:  Now comes with a ready to use windows DLL.
    * New project file:  Makefile for cygwin now included.
    * New example:  Example3 shows usage of the DLL.

  Interface changes
    * New member function: DefineOprt For adding user defined binary operators.
    * New member function: EnableBuiltInOprt(bool) Enables/Disables built in 
                           binary operators.
    * New member function: AddValIdent(...) to add callbacks for custom value 
                           recognition functions.
    * Removed: SetVar(), SetConst().
    * Renamed: Most interface functions have been renamed
    * Changed: The type for multiargument callbacks multfun_type has changed. 
               It no longer takes a std::vector as input.

  Internal changes
    * new class muParserTokenReader.h encapsulates the token identification 
      and token assignement.
    * Internal handling of function callbacks unified as a result the performance 
      of the bytecode evaluation increased.


Rev 1.10 : December 30, 2004
----------------------------

Release Notes:
This version does not contain major new feature compared to V1.07 but its internal structure has
changed significantly. The String parsing routine is slower than the one of V1.07 but bytecode
parsing is equally fast. On the other hand the error messages of V1.09 are more flexible and you
can change its value datatype. It should work on 64-bit systems. For this reason I supply both
versions for download. If you use V1.07 and are happy with it there is no need for updating
your version.

    * New example program: Archive now contains two demo programs: One for standard C++ and one for
                           managed C++.
    * New member function: RemoveVar(...) can be used for removing a single variable from the internal 
                           storage.
    * New member function: GetVar() can be used for querying the variable names and pointers of all
                           variables defined in the parser.
    * New member function: GetConst() can be used for querying all defined constants and their values.
    * New member function: GetFunDef() can be used for querying all defined functions and the number of
                           arguments they expect.
    * Internal structure changed; hanging base datatype at compile time is now possible.
    * Bugfix: Postfix operator parsing could fail in certain cases; This has been fixed now.
    * Bugfix: Variable names must will now be tested if they conflict with constant or function names.
    * Internal change:  Removed most dependencies from the C-string libraries.
    * Internal change:  Bytecode is now stored in a separate class: ParserByteCode.h
    * Internal change:  GetUsedVar() does no longer require that variables are defined at time of call.
    * Internal change:  Error treatment changed. ParserException is no longer derived from
                        std::runtime_error; Internal treatment of Error messages changed.
    * New functions in Parser interface: ValidNameChars(), ValidOprtChars() and ValidPrefixOprtChars()
                        they are used for defining the charset allowed for variable-, operator- and
                        function names.


Rev 1.09 : November 20, 2004
----------------------------

    * New member function:  RemoveVar(...) can be used for removing a single variable from the internal 
                            storage.
    * Internal structure changed; changing base datatype at compile time is now possible.
    * Bug fix: Postfix operator parsing could fail in certain cases; This has been fixed now.
    * Internal change:  Removed most dependencies from the C-string libraries.
    * Internal change:  Bytecode is now stored in a seperate class: ParserByteCode.h.
    * Internal change:  GetUsedVar() does no longer require that variables are defined at time of call.
    * Internal change:  Error treatment changed. ParserException is no longer derived from 
                        std::runtime_error; Internal treatment of Error messages changed.
    * New functions in Parser interface; ValidNameChars(), ValidOprtChars() and ValidPrefixOprtChars()
      they are used for defining the charset allowed for variable-, operator- and function names.


Rev 1.08 : November, 2004 
-------------------------

    * unpublished; experimental template version with respect to data type and underlying string
      type (string <-> widestring). The idea was dropped...


Rev 1.07 : September 4 2004
---------------------------

    * Improved portability; Changes to make life for MSVC 6 user easier, there are probably still some
      issues left.
    * Improved portability; Changes in order to allow compiling on BCB.
    * New function; value_type Diff(value_type *a_Var, value_type a_fPos) 4th order Differentiation with
      respect to a certain variable; added in muParser.h.


Rev 1.06 : August 20 2004
-------------------------

    * Volatile functions added; All overloaded AddFun(...) functions can now take a third parameter
      indicating that the function can not be optimized.
    * Internal changes: muParserStack.h simplified; refactorings
    * Parser is now distributed under the MIT License; all comments changed accordingly.


Rev 1.05 : August 20 2004
-------------------------

    * Variable/constant names will now be checked for invalid characters.
    * Querying the names of all variables used in an expression is now possible; new function: GetUsedVar().
    * Disabling bytecode parsing is now possible; new function: EnableByteCode(bool bStat).
    * Predefined functions with variable number of arguments added: sum, avg, min, max.
    * Unary prefix operators added; new functions: AddPrefixOp(...), ClearPrefixOp().
    * Postfix operator interface names changed; new function names: AddPostfixOp(...), ClearPostfixOp().
    * Hardcoded sign operators removed in favor of prefix operators; bytecode format changed accordingly.
    * Internal changes: static array removed in Command code calculation routine; misc. changes.


Rev 1.04 : August 16 2004
-------------------------

    * Support for functions with variable number of arguments added.
    * Internal structure changed; new: ParserBase.h, ParserBase.cpp; removed: ParserException.h;
      changed: Parser.h, Parser.cpp.
    * Bug in the bytecode calculation function fixed (affected the unary minus operator).
    * Optimizer can be deactivated; new function: EnableOptimizer(bool bStat).


Rev 1.03 : August 10 2004
-------------------------

    * Support for user-defined unary postfix operators added; new functions: AddPostOp(), InitPostOp(),
      ClearPostOp().
    * Minor changes to the bytecode parsing routine.
    * User defined functions can now have up to four parameters.
    * Performance optimized: simple formula optimization added; (precalculation of constant parts of the
      expression).
    * Bug fixes: Multi-arg function parameters, constant name lookup and unary minus did not work properly.


Rev 1.02 : July 30 2004
-----------------------

    * Support for user defined constants added; new functions: InitConst(), AddConst(), SetConst(),
      ClearConst().
    * Single variables can now be added using AddVar(); you have now the choice of adding them either
      one by one or all at the same time using SetVar(const varmap_type &a_vVar).
    * Internal handling of variables changed, is now similar to function handling.
    * Virtual destructor added; InitFun(), InitConst() are now virtual too thus making it possible to
      derive new parsers with a modified set of default functions and constants.
    * Support for user defined functions with 2 or 3 parameters added; bytecode format changed to hold
      function parameter count.


Rev 1.01 : July 23 2004
-----------------------

    * Support for user defined functions has been added; new functions: AddFun(), ClearFun(),
      InitFunctions().
    * Built in constants have been removed; the parser contained undocumented built in
      constants pi, e.
      There was the possibility of name conflicts with user defined variables.
    * Setting multiple variables with SetVar can now be done with a map of names and pointers as the only
      argument. For this reason, a new type Parser::varmap_type was added. The old version that took 3
      arguments (array of names, array of pointers, and array length) is now marked as deprecated.
    * The names of logarithm functions have changed. The new names are: log2 for base 2, log10 or log for
      base 10, and ln for base e.


Rev 1.00 : July 21 2004
-----------------------

    * Initial release
