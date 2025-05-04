# RUN: %fish %s

printf "%d %d\n" 1 2 3
# CHECK: 1 2
# CHECK: 3 0

printf "Hello %d %i %f %F %g %G\n" 1 2 3 4 5 6
# CHECK: Hello 1 2 3.000000 4.000000 5 6

printf "%x %X %o %llu\n" 10 11 8 -1
# CHECK: a B 10 18446744073709551615

# %a has OS-dependent output - see #1139
#printf "%a %A\n" 14 15

printf "%c %s\n" a hello
# CHECK: a hello

printf "%c%c%c\n" hello … o
# CHECK: h…o

printf "%e %E\n" 5 6
# CHECK: 5.000000e+00 6.000000E+00

printf "%20d\n" 50
# CHECK:                   50

printf "%-20d%d\n" 5 10
# CHECK: 5                   10

printf "%*d\n" 10 100
# CHECK:       100

printf "%%\"\\\n"
printf "%s\b%s\n" x y
# CHECK: %"\nxy

printf "abc\rdef\n"
# CHECK: abc{{\r}}def

printf "Msg1\fMsg2\n"
# CHECK: Msg1{{\f}}Msg2

printf "foo\vbar\vbaz\n"
# CHECK: foobarbaz

printf "\111 \x50 \u0051 \U00000052"
echo
# CHECK: I P Q R

# \c escape means "stop printing"
printf 'a\cb'
echo
# CHECK: a

# Bogus printf specifier, should produce no stdout
printf "%5" 10 2>/dev/null

# Octal escapes produce literal bytes, not characters
# \376 is 0xFE
printf '\376' | display_bytes
# CHECK: 0000000 376
# CHECK: 0000001

# Verify that floating point conversions and output work correctly with
# different combinations of locales and floating point strings. See issue
# #3334. This starts by assuming an locale using english conventions.
printf '%e\n' "1.23" # should succeed, output should be 1.230000e+00
# CHECK: 1.230000e+00

printf '%e\n' "2,34" # should fail
# CHECK: 2.000000e+00
# CHECKERR: 2,34: value not completely converted (can't convert ',34')

# Verify long long ints are handled correctly. See issue #3352.
printf 'long hex1 %x\n' 498216206234
# CHECK: long hex1 73ffffff9a
printf 'long hex2 %X\n' 498216206234
# CHECK: long hex2 73FFFFFF9A
printf 'long hex3 %X\n' 0xABCDEF1234567890
# CHECK: long hex3 ABCDEF1234567890
printf 'long hex4 %X\n' 0xABCDEF12345678901
# CHECKERR: 0xABCDEF12345678901: Number out of range
printf 'long decimal %d\n' 498216206594
# CHECK: long hex4 long decimal 498216206594
printf 'long signed %d\n' -498216206595
# CHECK: long signed -498216206595
printf 'long signed to unsigned %u\n' -498216206596
# CHECK: long signed to unsigned 18446743575493345020

# Just check that we print no error for no arguments
printf
echo $status
# CHECK: 2

# Verify numeric conversion still happens even if it couldn't be fully converted
printf '%d\n' 15.1
# CHECK: 15
# CHECKERR: 15.1: value not completely converted (can't convert '.1')
echo $status
# CHECK: 1

printf '%d\n' 07
# CHECK: 7
echo $status
# CHECK: 0
printf '%d\n' 08
# CHECK: 0
# CHECKERR: 08: value not completely converted (can't convert '8')
# CHECKERR: Hint: a leading '0' without an 'x' indicates an octal number
echo $status
# CHECK: 1

printf '%d\n' 0f
# CHECK: 0
# CHECKERR: 0f: value not completely converted (can't convert 'f')
# CHECKERR: Hint: a leading '0' without an 'x' indicates an octal number
echo $status
# CHECK: 1

printf '%d\n' 0g
# CHECK: 0
# CHECKERR: 0g: value not completely converted (can't convert 'g')
echo $status
# CHECK: 1

printf '%f\n' 0x2
# CHECK: 2.000000

printf '%f\n' 0x2p3
# CHECK: 16.000000

printf '%.1f\n' -0X1.5P8
# CHECK: -336.0

# Test that we ignore options
printf -a
printf --foo
# CHECK: -a--foo
echo

set -l helpvar --help
printf $helpvar
echo
# CHECK: --help

printf --help
echo
# CHECK: --help

# This is how mc likes to encode the directory we should cd to.
printf '%b\n' '\0057foo\0057bar\0057'
# CHECK: /foo/bar/

printf %18446744073709551616s
# CHECKERR: Number out of range

# Test non-ASCII behavior
printf '|%3s|\n' 'ö'
# CHECK: |  ö|
printf '|%3s|\n' '🇺🇳'
#CHECK: | 🇺🇳|
printf '|%.3s|\n' '🇺🇳🇺🇳'
#CHECK: |🇺🇳|
printf '|%.3s|\n' 'a🇺🇳'
#CHECK: |a🇺🇳|
printf '|%.3s|\n' 'aa🇺🇳'
#CHECK: |aa|
printf '|%3.3s|\n' 'aa🇺🇳'
#CHECK: | aa|
printf '|%.1s|\n' '𒈙a'
#CHECK: |𒈙|
printf '|%3.3s|\n' '👨‍👨‍👧‍👧'
#CHECK: | 👨‍👨‍👧‍👧|
