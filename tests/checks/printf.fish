# RUN: %fish %s

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
# CHECKERR: 2,34: value not completely converted

# Try to use one of several locales that use a comma as the decimal mark
# rather than the period used in english speaking locales. If we don't find
# one installed we simply don't run this test.
#
# musl currently does not have a `locale` command, so we also skip it then.
set -l locales (command -sq locale; and locale -a)
set -l acceptable_locales bg_BG de_DE es_ES fr_FR ru_RU
set -l numeric_locale
for locale in {$acceptable_locales}.{UTF-8,UTF8}
    if string match -i -q $locale $locales
        set numeric_locale $locale
        break
    end
end

# OpenBSD's wcstod does not honor LC_NUMERIC, meaning this feature is broken there.
if set -q numeric_locale[1]; and test (uname) != OpenBSD
    set -x LC_NUMERIC $numeric_locale
    printf '%e\n' "3,45" # should succeed, output should be 3,450000e+00
    printf '%e\n' "4.56" # should succeed, output should be 4,560000e+00
else
    echo '3,450000e+00'
    echo '4,560000e+00'
end
# CHECK: 3,450000e+00
# CHECK: 4,560000e+00

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
# CHECKERR: 15.1: value not completely converted
echo $status
# CHECK: 1
