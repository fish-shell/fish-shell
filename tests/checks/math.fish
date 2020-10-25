#RUN: %fish %s
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

math 3^0.5^2
math -2^2
# CHECK: 1.316074
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
not math 'max()'
# CHECKERR: math: Error: Unexpected token
# CHECKERR: 'max()'
# CHECKERR:    ^
not math 'sin()'
# CHECKERR: math: Error: Too few arguments
# CHECKERR: 'sin()'
# CHECKERR:      ^
not math '2 + 2 4'
# CHECKERR: math: Error: Too many arguments
# CHECKERR: '2 + 2 4'
# CHECKERR:        ^
not math
# CHECKERR: math: Expected at least 1 args, got 0
not math -s 12
# CHECKERR: math: Expected at least 1 args, got 0
not math 2^999999
# CHECKERR: math: Error: Result is infinite
# CHECKERR: '2^999999'
not math 1 / 0
# CHECKERR: math: Error: Result is infinite
# CHECKERR: '1 / 0'

# Validate "x" as multiplier
math 0x2 # Hex
math 5 x 4
math 2x 4
math 2 x4 # ERROR
# CHECKERR: math: Error: Unknown function
# CHECKERR: '2 x4'
# CHECKERR:     ^
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
# CHECKERR: math: Error: Missing opening parenthesis
# CHECKERR: 'log 16'
# CHECKERR:       ^

math 'log(16'
# CHECKERR: math: Error: Missing closing parenthesis
# CHECKERR: 'log(16'
# CHECKERR:       ^
