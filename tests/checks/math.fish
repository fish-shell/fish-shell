#RUN: %fish %s
# OpenBSD doesn't do hex numbers in str/wcstod (like C99 requires).
# So let's skip this.
#REQUIRES: test "$(uname)" != OpenBSD
# Validate basic expressions
math 3 / 2
# CHECK: 1.5
math 10/6
# CHECK: 1.666667
math -s0 10 / 6
# CHECK: 1
math 'floor(10 / 6)'
# CHECK: 1
math -s3 10/6
# CHECK: 1.667
math '10 % 6'
# CHECK: 4
math -s0 '10 % 6'
# CHECK: 4
math '23 % 7'
# CHECK: 2
math --scale=6 '5 / 3 * 0.3'
# CHECK: 0.5
math --scale=max '5 / 3'
# CHECK: 1.666666666666667
math "7^2"
# CHECK: 49
math -1 + 1
# CHECK: 0
math '-2 * -2'
# CHECK: 4
math 5 \* -2
# CHECK: -10
math -- -4 / 2
# CHECK: -2
math -- '-4 * 2'
# CHECK: -8

# Validate max and min
math 'max(1,2)'
math 'min(1,2)'
# CHECK: 2
# CHECK: 1

# Validate some rounding functions
math 'round(3/2)'
math 'floor(3/2)'
math 'ceil(3/2)'
# CHECK: 2
# CHECK: 1
# CHECK: 2
math 'round(-3/2)'
math 'floor(-3/2)'
math 'ceil(-3/2)'
# CHECK: -2
# CHECK: -2
# CHECK: -1

# Validate some integral computations
math 1
math 10
math 100
math 1000
# CHECK: 1
# CHECK: 10
# CHECK: 100
# CHECK: 1000
math '10^15'
math '-10^14'
math '-10^15'
# CHECK: 1000000000000000
# CHECK: 100000000000000
# CHECK: -1000000000000000

# Floating point operations under x86 without SSE2 have reduced accuracy!
# This includes both i586 targets and i686 under Debian, where it's patched to remove SSE2.
# As a result, some floating point tests use regular expressions to loosely match against
# the shape of the expected result.

# NB: The i586 case should also pass under other platforms, but not the other way around.
math "3^0.5^2"
# CHECK: {{1\.316074|1\.316\d+}}

math -2^2
# CHECK: 4

math -s0 '1.0 / 2.0'
math -s0 '3.0 / 2.0'
math -s0 '10^15 / 2.0'
# CHECK: 0
# CHECK: 1
# CHECK: 500000000000000

# Validate how variables in an expression are handled
math $x + 1
set x 1
math $x + 1
set x 3
set y 1.5
math "-$x * $y"
math -s0 "-$x * $y"
# CHECK: 1
# CHECK: 2
# CHECK: -4.5
# CHECK: -4

# Validate math error reporting
# NOTE: The leading whitespace for the carets here is ignored
# by littlecheck.
not math '2 - '
# CHECKERR: math: Error: Too few arguments
# CHECKERR: '2 - '
# CHECKERR:     ^
not math 'ncr(1)'
# CHECKERR: math: Error: Too few arguments
# CHECKERR: 'ncr(1)'
# CHECKERR:       ^

math "min()"
# CHECKERR: math: Error: Too few arguments
# CHECKERR: 'min()'
# CHECKERR:      ^

# There is no "blah" function.
not math 'blah()'
# CHECKERR: math: Error: Unknown function
# CHECKERR: 'blah()'
# CHECKERR:  ^~~^

math n + 4
# CHECKERR: math: Error: Unknown function
# CHECKERR: 'n + 4'
# CHECKERR:  ^


not math 'sin()'
# CHECKERR: math: Error: Too few arguments
# CHECKERR: 'sin()'
# CHECKERR:      ^
not math '2 + 2 4'
# CHECKERR: math: Error: Missing operator
# CHECKERR: '2 + 2 4'
# This regex to check whitespace - the error appears between the second 2 and the 4!
# (right after the 2)
# CHECKERR: {{^}}      ^
printf '<%s>\n' (math '2 + 2      4' 2>&1)
# CHECK: <math: Error: Missing operator>
# CHECK: <'2 + 2      4'>
# CHECK: <      ^~~~~^>

not math '(1 2)'
# CHECKERR: math: Error: Missing operator
# CHECKERR: '(1 2)'
# CHECKERR:    ^
not math '(1 pi)'
# CHECKERR: math: Error: Missing operator
# CHECKERR: '(1 pi)'
# CHECKERR:    ^
not math '(1 pow 1,2)'
# CHECKERR: math: Error: Missing operator
# CHECKERR: '(1 pow 1,2)'
# CHECKERR:    ^
not math
# CHECKERR: math: expected >= 1 arguments; got 0
not math -s 12
# CHECKERR: math: expected >= 1 arguments; got 0
# XXX FIXME these two should be just "missing argument" errors, the count is not helpful
not math 2^999999
# CHECKERR: math: Error: Result is infinite
# CHECKERR: '2^999999'
not math 'sqrt(-1)'
# CHECKERR: math: Error: Result is not a number
# CHECKERR: 'sqrt(-1)'
math 'sqrt(-0)'
# CHECK: -0
not math 2^53 + 1
# CHECKERR: math: Error: Result magnitude is too large
# CHECKERR: '2^53 + 1'
not math -2^53 - 1
# CHECKERR: math: Error: Result magnitude is too large
# CHECKERR: '-2^53 - 1'
printf '<%s>\n' (not math 1 / 0 2>&1)
# CHECK: <math: Error: Division by zero>
# CHECK: <'1 / 0'>
# CHECK: <   ^>
printf '<%s>\n' (math 1 % 0 - 5 2>&1)
# CHECK: <math: Error: Division by zero>
# CHECK: <'1 % 0 - 5'>
# CHECK: <   ^>
printf '<%s>\n' (math min 1 / 0, 5 2>&1)
# CHECK: <math: Error: Division by zero>
# CHECK: <'min 1 / 0, 5'>
# CHECK: <       ^>

# Validate "x" as multiplier
math 0x2 # Hex
math 5 x 4
math 2x 4
math 2 x4 # ERROR
# CHECKERR: math: Error: Unknown function
# CHECKERR: '2 x4'
# CHECKERR:    ^^
math 0x 3
# CHECK: 2
# CHECK: 20
# CHECK: 8
# CHECK: 0

math "42 >= 1337"
# CHECKERR: math: Error: Logical operations are not supported, use `test` instead
# CHECKERR: '42 >= 1337'
# CHECKERR:     ^

math "bitand(0xFE, 1)"
# CHECK: 0
math "bitor(0xFE, 1)"
# CHECK: 255
math "bitxor(5, 1)"
# CHECK: 4
math "bitand(5.5, 2)"
# CHECK: 0
math "bitand(5.5, 1)"
# CHECK: 1

math "bitor(37 ^ 5, 255)"
# CHECK: 69343999

math 'log 16'
# CHECK: 1.20412

math 'log(16'
# CHECKERR: math: Error: Missing closing parenthesis
# CHECKERR: 'log(16'
# CHECKERR:       ^

math '(2'
# CHECKERR: math: Error: Missing closing parenthesis
# CHECKERR: '(2'
# CHECKERR:   ^

math --base=16 255 / 15
# CHECK: 0x11
math -bhex 16 x 2
# CHECK: 0x20
math --base hex 12 + 0x50
# CHECK: 0x5c
math --base hex 0
# CHECK: 0x0
math --base hex -1
# CHECK: -0x1
math --base hex -15
# CHECK: -0xf
math --base hex 'pow(2,40)'
# CHECK: 0x10000000000
math --base octal 0
# CHECK: 0
math --base octal -1
# CHECK: -01
math --base octal -15
# CHECK: -017
math --base octal 'pow(2,40)'
# CHECK: 020000000000000
math --base octal --scale=0 55
# CHECK: 067
math --base notabase
# CHECKERR: math: notabase: invalid base value
echo $status
# CHECK: 2

math 'log2(8)'
# CHECK: 3

# same as sin(cos(2 x pi))
math sin cos 2 x pi
# CHECK: 0.841471
# Inner function binds stronger, so this is interpreted as
# pow(sin(3,5))

math pow sin 3, 5
# CHECKERR: math: Error: Too many arguments
# CHECKERR: 'pow sin 3, 5'
# CHECKERR: ^

math pow sin 3, 5 + 2
# CHECKERR: math: Error: Too many arguments
# CHECKERR: 'pow sin 3, 5 + 2'
# CHECKERR:             ^~~~^

math sin pow 3, 5
# CHECK: {{-0\.890009|-0.890\d*}}

math pow 2, cos -pi
# CHECK: 0.5

# pow(2*cos(-pi), 2)
# i.e. 2^2
# i.e. 4
math pow 2 x cos'(-pi)', 2
# CHECK: 4

# This used to take ages, see #8170.
# If this test hangs, that's reintroduced!
math 'ncr(0/0, 1)'
# CHECKERR: math: Error: Division by zero
# CHECKERR: 'ncr(0/0, 1)'
# CHECKERR:       ^

# Variadic functions require at least one argument
math min
# CHECKERR: math: Error: Too few arguments
# CHECKERR: 'min'
# CHECKERR:    ^
math min 2
# CHECK: 2
math min 2, 3, 4, 5, -10, 1
# CHECK: -10

# Parentheses are required to disambiguate function call nested in argument list,
# except when the call is the last argument.
math 'min 5, 4, 3, ncr 2, 1, 5'
# CHECKERR: math: Error: Too many arguments
# CHECKERR:      'min 5, 4, 3, ncr 2, 1, 5'
# CHECKERR: {{^}}                        ^
math 'min 5, 4, 3, ncr(2, 1), 5'
# CHECK: 2
math 'min 5, 4, 3, 5, ncr 2, 1'
# CHECK: 2
# Variadic function consumes all available arguments,
# so it is always the last argument unless parenthesised.
# max(1, 2, min(3, 4, 5))
math 'max 1, 2, min 3, 4, 5'
# CHECK: 3
# max(1, 2, min(3, 4), 5)
math 'max 1, 2, min(3, 4), 5'
# CHECK: 5
math 0_1
# CHECK: 1
math 0x0_A
# CHECK: 10
math 1_000 + 2_000
# CHECK: 3000
math 1_0_0_0
# CHECK: 1000
math 0_0.5_0 + 0_1.0_0
# CHECK: 1.5
math 2e0_0_2
# CHECK: 200
math -0_0.5_0_0E0_0_3
# CHECK: -500
math 20e-0_1
# CHECK: 2
math 0x0_2.0_0_0P0_2
# CHECK: 8
math -0x8p-0_3
# CHECK: -1

echo 5 + 6 | math
# CHECK: 11

# Historical: If we have arguments on stdin and argv,
# the former takes precedence and the latter is ignored entirely.
echo 7 + 6 | math 2 + 2
# CHECK: 13

# It isn't checked at all.
echo 7 + 8 | math not an expression
# CHECK: 15

math (string repeat -n 1000 1) 2>| string shorten -m50 --char=""
# CHECK: math: Error: Number is too large
# CHECK: '1111111111111111111111111111111111111111111111111
# CHECK:  ^

math 0x0_2.0P-0x3
# CHECKERR: math: Error: Unknown function
# CHECKERR: '0x0_2.0P-0x3'
# CHECKERR:            ^^
math 0x0_2.0P-f
# CHECKERR: math: Error: Unexpected token
# CHECKERR: '0x0_2.0P-f'
# CHECKERR:           ^
math "22 / 5 - 5"
# CHECK: -0.6
math -s 0 --scale-mode=truncate "22 / 5 - 5"
# CHECK: -0
math --scale=0 -m truncate "22 / 5 - 5"
# CHECK: -0
math -s 0 --scale-mode=floor "22 / 5 - 5"
# CHECK: -1
math -s 0 --scale-mode=round "22 / 5 - 5"
# CHECK: -1
math -s 0 --scale-mode=ceiling "22 / 5 - 5"
# CHECK: -0
math "1 / 3 - 1"
# CHECK: -0.666667
math --scale-mode=truncate "1 / 3 - 1"
# CHECK: -0.666666
math --scale-mode=floor "1 / 3 - 1"
# CHECK: {{-0.666667|-0.666668}}
math --scale-mode=floor "2 / 3 - 1"
# CHECK: {{-0.333334|-0.333335}}
math --scale-mode=round "1 / 3 - 1"
# CHECK: {{-0.666667|-0.666668}}
math --scale-mode=ceiling "1 / 3 - 1"
# CHECK: -0.666666
math --scale-mode=ceiling "2 / 3 - 1"
# CHECK: -0.333333
math -s 6 --scale-mode=truncate "1 / 3 - 1"
# CHECK: -0.666666
math -s 6 --scale-mode=floor "1 / 3 - 1"
# CHECK: {{-0.666667|-0.666668}}
math -s 6 --scale-mode=floor "2 / 3 - 1"
# CHECK: {{-0.333334|-0.333335}}
math -s 6 --scale-mode=round "1 / 3 - 1"
# CHECK: {{-0.666667|-0.666668}}
math -s 6 --scale-mode=ceiling "1 / 3 - 1"
# CHECK: -0.666666
math -s 6 --scale-mode=ceiling "2 / 3 - 1"
# CHECK: -0.333333
