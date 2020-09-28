#RUN: %fish %s
# Tests for string builtin. Mostly taken from man page examples.

string match -r -v "c.*" dog can cat diz; and echo "exit 0"
# CHECK: dog
# CHECK: diz
# CHECK: exit 0

string match -q -r -v "c.*" dog can cat diz; and echo "exit 0"
# CHECK: exit 0

string match -v "c*" dog can cat diz; and echo "exit 0"
# CHECK: dog
# CHECK: diz
# CHECK: exit 0

string match -q -v "c*" dog can cat diz; and echo "exit 0"
# CHECK: exit 0

string match -v "d*" dog dan dat diz; or echo "exit 1"
# CHECK: exit 1

string match -q -v "d*" dog dan dat diz; or echo "exit 1"
# CHECK: exit 1

string match -r -v x y; and echo "exit 0"
# CHECK: y
# CHECK: exit 0

string match -r -v x x; or echo "exit 1"
# CHECK: exit 1

string match -q -r -v x y; and echo "exit 0"
# CHECK: exit 0

string match -q -r -v x x; or echo "exit 1"
# CHECK: exit 1

string length "hello, world"
# CHECK: 12

string length -q ""; and echo not zero length; or echo zero length
# CHECK: zero length

string pad foo
# CHECK: foo

string pad -r -w 7 -c - foo
# CHECK: foo----

string pad --width 7 -c '=' foo
# CHECK: ====foo

echo \|(string pad --width 10 --right foo)\|
# CHECK: |foo       |

begin
    set -l fish_emoji_width 2
    # Pad string with multi-width emoji.
    string pad -w 4 -c . 🐟
    # CHECK: ..🐟

    # Pad with multi-width character.
    string pad -w 3 -c 🐟 .
    # CHECK: 🐟.

    # Multi-width pad with remainder, complemented with a space.
    string pad -w 4 -c 🐟 . ..
    # CHECK: 🐟 .
    # CHECK: 🐟..
end

# Pad to the maximum length.
string pad -c . long longer longest
# CHECK: ...long
# CHECK: .longer
# CHECK: longest

# This tests current behavior where the max width of an argument overrules
# the width parameter. This could be changed if needed.
string pad -c_ --width 5 longer-than-width-param x
# CHECK: longer-than-width-param
# CHECK: ______________________x

# Current behavior is that only a single padding character is supported.
# We can support longer strings in future without breaking compatibilty.
string pad -c ab -w4 .
# CHECKERR: string pad: Padding should be a character 'ab'

string sub --length 2 abcde
# CHECK: ab

string sub -s 2 -l 2 abcde
# CHECK: bc

string sub --start=-2 abcde
# CHECK: de

string sub --end=3 abcde
# CHECK: abc

string sub --end=-4 abcde
# CHECK: a

string sub --start=2 --end=-2 abcde
# CHECK: bc

string sub -s -5 -e -2 abcdefgh
# CHECK: def

string sub -s -100 -e -2 abcde
# CHECK: abc

string sub -s -5 -e 2 abcde
# CHECK: ab

string sub -s -50 -e -100 abcde
# CHECK:

string sub -s 2 -e -5 abcde
# CHECK:

string split . example.com
# CHECK: example
# CHECK: com

string split -r -m1 / /usr/local/bin/fish
# CHECK: /usr/local/bin
# CHECK: fish

string split "" abc
# CHECK: a
# CHECK: b
# CHECK: c

string split --fields=2 "" abc
# CHECK: b

string split --fields=3,2 "" abc
# CHECK: c
# CHECK: b

string split --fields=2,9 "" abc; or echo "exit 1"
# CHECK: exit 1

string split --fields=1-3,5,9-7 "" 123456789
# CHECK: 1
# CHECK: 2
# CHECK: 3
# CHECK: 5
# CHECK: 9
# CHECK: 8
# CHECK: 7

string split -f1 ' ' 'a b' 'c d'
# CHECK: a
# CHECK: c

string split --allow-empty --fields=2,9 "" abc
# CHECK: b

seq 3 | string join ...
# CHECK: 1...2...3

string trim " abc  "
# CHECK: abc

string trim --right --chars=yz xyzzy zany
# CHECK: x
# CHECK: zan

echo \x07 | string escape
# CHECK: \cg

string escape --style=script 'a b#c"\'d'
# CHECK: a\ b\#c\"\'d

string escape --style=url 'a b#c"\'d'
# CHECK: a%20b%23c%22%27d

string escape --style=url \na\nb%c~d\n
# CHECK: %0Aa%0Ab%25c~d%0A

string escape --style=var 'a b#c"\'d'
# CHECK: a_20_b_23_c_22_27_d

string escape --style=var a\nghi_
# CHECK: a_0A_ghi__

string escape --style=var abc
# CHECK: abc

string escape --style=var _a_b_c_
# CHECK: __a__b__c__

string escape --style=var -- -
# CHECK: _2D_

# string escape with multibyte chars
string escape --style=url aöb
string escape --style=url 中
string escape --style=url aöb | string unescape --style=url
string escape --style=url 中 | string unescape --style=url

string escape --style=var aöb
string escape --style=var 中
string escape --style=var aöb | string unescape --style=var
string escape --style=var 中 | string unescape --style=var
# CHECK: a%C3%B6b
# CHECK: %E4%B8%AD
# CHECK: aöb
# CHECK: 中
# CHECK: a_C3_B6_b
# CHECK: _E4_B8_AD_
# CHECK: aöb
# CHECK: 中

# test regex escaping
string escape --style=regex ".ext"
string escape --style=regex "bonjour, amigo"
string escape --style=regex "^this is a literal string"
# CHECK: \.ext
# CHECK: bonjour, amigo
# CHECK: \^this is a literal string

### Verify that we can correctly unescape the same strings
#   we tested escaping above.
set x (string unescape (echo \x07 | string escape))
test $x = \x07
and echo success
# CHECK: success

string unescape --style=script (string escape --style=script 'a b#c"\'d')
# CHECK: a b#c"'d

string unescape --style=url (string escape --style=url 'a b#c"\'d')
# CHECK: a b#c"'d

string unescape --style=url (string escape --style=url \na\nb%c~d\n)
# CHECK:
# CHECK: a
# CHECK: b%c~d

string unescape --style=var (string escape --style=var 'a b#c"\'d')
# CHECK: a b#c"'d

string unescape --style=var (string escape --style=var a\nghi_)
# CHECK: a
# CHECK: ghi_

string unescape --style=var (string escape --style=var 'abc')
# CHECK: abc

string unescape --style=var (string escape --style=var '_a_b_c_')
# CHECK: _a_b_c_

string unescape --style=var -- (string escape --style=var -- -)
# CHECK: -

### Verify that we can correctly match strings.
string match "*" a
# CHECK: a

string match "a*b" axxb
# CHECK: axxb

string match -i "a**B" Axxb
# CHECK: Axxb

echo "ok?" | string match "*?"
# CHECK: ok?

string match -r "cat|dog|fish" "nice dog"
# CHECK: dog

string match -r "(\d\d?):(\d\d):(\d\d)" 2:34:56
# CHECK: 2:34:56
# CHECK: 2
# CHECK: 34
# CHECK: 56

string match -r "^(\w{2,4})\g1\$" papa mud murmur
# CHECK: papa
# CHECK: pa
# CHECK: murmur
# CHECK: mur

string match -r -a -n at ratatat
# CHECK: 2 2
# CHECK: 4 2
# CHECK: 6 2

string match -r -i "0x[0-9a-f]{1,8}" "int magic = 0xBadC0de;"
# CHECK: 0xBadC0de

string replace is was "blue is my favorite"
# CHECK: blue was my favorite

string replace 3rd last 1st 2nd 3rd
# CHECK: 1st
# CHECK: 2nd
# CHECK: last

string replace -a " " _ "spaces to underscores"
# CHECK: spaces_to_underscores

string replace -r -a "[^\d.]+" " " "0 one two 3.14 four 5x"
# CHECK: 0 3.14 5

string replace -r "(\w+)\s+(\w+)" "\$2 \$1 \$\$" "left right"
# CHECK: right left $

string replace -r "\s*newline\s*" "\n" "put a newline here"
# CHECK: put a
# CHECK: here

string replace -r -a "(\w)" "\$1\$1" ab
# CHECK: aabb

string replace --filter x X abc axc x def jkx
or echo Unexpected exit status at line (status --current-line-number)
# CHECK: aXc
# CHECK: X
# CHECK: jkX

string replace --filter y Y abc axc x def jkx
and echo Unexpected exit status at line (status --current-line-number)

string replace --regex -f "\d" X 1bc axc 2 d3f jk4 xyz
or echo Unexpected exit status at line (status --current-line-number)
# CHECK: Xbc
# CHECK: X
# CHECK: dXf
# CHECK: jkX

string replace --regex -f Z X 1bc axc 2 d3f jk4 xyz
and echo Unexpected exit status at line (status --current-line-number)

# From https://github.com/fish-shell/fish-shell/issues/5201
# 'string match -r with empty capture groups'
string match -r '^([ugoa]*)([=+-]?)([rwx]*)$' '=r'
#CHECK: =r
#CHECK:
#CHECK: =
#CHECK: r

### Test some failure cases
string match -r "[" "a[sd"; and echo "unexpected exit 0"
# CHECKERR: string match: Regular expression compile error: missing terminating ] for character class
# CHECKERR: string match: [
# CHECKERR: string match: ^

# FIXME: This prints usage summary?
#string invalidarg; and echo "unexpected exit 0"
# DONTCHECKERR: string: Subcommand 'invalidarg' is not valid

string length; or echo "missing argument returns 1"
# CHECK: missing argument returns 1

string match -r -v "[dcantg].*" dog can cat diz; or echo "no regexp invert match"
# CHECK: no regexp invert match

string match -v "*" dog can cat diz; or echo "no glob invert match"
# CHECK: no glob invert match

string match -rvn a bbb; or echo "exit 1"
# CHECK: 1 3

### Test repeat subcommand
string repeat -n 2 foo
# CHECK: foofoo

string repeat --count 2 foo
# CHECK: foofoo

echo foo | string repeat -n 2
# CHECK: foofoo

string repeat -n2 -q foo; and echo "exit 0"
# CHECK: exit 0

string repeat -n2 --quiet foo; and echo "exit 0"
# CHECK: exit 0

string repeat -n0 foo; or echo "exit 1"
# CHECK: exit 1

string repeat -n0; or echo "exit 1"
# CHECK: exit 1

string repeat -m0; or echo "exit 1"
# CHECK: exit 1

string repeat -n1 -N "there is "
echo "no newline"
# CHECK: there is no newline

string repeat -n1 --no-newline "there is "
echo "no newline"
# CHECK: there is no newline

string repeat -n10 -m4 foo
# CHECK: foof

string repeat -n10 --max 5 foo
# CHECK: foofo

string repeat -n3 -m20 foo
# CHECK: foofoofoo

string repeat -m4 foo
# CHECK: foof

string repeat -n-1 foo; and echo "exit 0"
# CHECKERR: string repeat: Invalid count value '-1'

string repeat -m-1 foo; and echo "exit 0"
# CHECKERR: string repeat: Invalid max value '-1'

string repeat -n notanumber foo; and echo "exit 0"
# CHECKERR: string repeat: Argument 'notanumber' is not a valid integer

string repeat -m notanumber foo; and echo "exit 0"
# CHECKERR: string repeat: Argument 'notanumber' is not a valid integer

echo stdin | string repeat -n1 "and arg"; and echo "exit 0"
# CHECKERR: string repeat: Too many arguments

string repeat -n; and echo "exit 0"
# CHECKERR: string repeat: Expected argument for option n

# FIXME: Also triggers usage
# string repeat -l fakearg
# DONTCHECKERR: string repeat: Unknown option '-l'

string repeat ""
or echo string repeat empty string failed
# CHECK: string repeat empty string failed

string repeat -n3 ""
or echo string repeat empty string failed
# CHECK: string repeat empty string failed

# Test equivalent matches with/without the --entire, --regex, and --invert flags.
string match -e x abc dxf xyz jkx x z
or echo exit 1
# CHECK: dxf
# CHECK: xyz
# CHECK: jkx
# CHECK: x

string match x abc dxf xyz jkx x z
# CHECK: x

string match --entire -r "a*b[xy]+" abc abxc bye aaabyz kaabxz abbxy abcx caabxyxz
or echo exit 1
# CHECK: abxc
# CHECK: bye
# CHECK: aaabyz
# CHECK: kaabxz
# CHECK: abbxy
# CHECK: caabxyxz

# 'string match --entire "" -- banana'
string match --entire "" -- banana
or echo exit 1
# CHECK: banana

# 'string match -r "a*b[xy]+" abc abxc bye aaabyz kaabxz abbxy abcx caabxyxz'
string match -r "a*b[xy]+" abc abxc bye aaabyz kaabxz abbxy abcx caabxyxz
or echo exit 1
# CHECK: abx
# CHECK: by
# CHECK: aaaby
# CHECK: aabx
# CHECK: bxy
# CHECK: aabxyx

# Make sure that groups are handled correct with/without --entire.
# 'string match --entire -r "a*b([xy]+)" abc abxc bye aaabyz kaabxz abbxy abcx caabxyxz'
string match --entire -r "a*b([xy]+)" abc abxc bye aaabyz kaabxz abbxy abcx caabxyxz
or echo exit 1
# CHECK: abxc
# CHECK: x
# CHECK: bye
# CHECK: y
# CHECK: aaabyz
# CHECK: y
# CHECK: kaabxz
# CHECK: x
# CHECK: abbxy
# CHECK: xy
# CHECK: caabxyxz
# CHECK: xyx

# 'string match -r "a*b([xy]+)" abc abxc bye aaabyz kaabxz abbxy abcx caabxyxz'
string match -r "a*b([xy]+)" abc abxc bye aaabyz kaabxz abbxy abcx caabxyxz
or echo exit 1
# CHECK: abx
# CHECK: x
# CHECK: by
# CHECK: y
# CHECK: aaaby
# CHECK: y
# CHECK: aabx
# CHECK: x
# CHECK: bxy
# CHECK: xy
# CHECK: aabxyx
# CHECK: xyx

# Test `string lower` and `string upper`.
set x (string lower abc DEF gHi)
or echo string lower exit 1
test $x[1] = abc -a $x[2] = def -a $x[3] = ghi
or echo strings not converted to lowercase

set x (echo abc DEF gHi | string lower)
or echo string lower exit 1
test $x[1] = 'abc def ghi'
or echo strings not converted to lowercase

string lower -q abc
and echo lowercasing a lowercase string did not fail as expected

set x (string upper abc DEF gHi)
or echo string upper exit 1
test $x[1] = ABC -a $x[2] = DEF -a $x[3] = GHI
or echo strings not converted to uppercase

set x (echo abc DEF gHi | string upper)
or echo string upper exit 1
test $x[1] = 'ABC DEF GHI'
or echo strings not converted to uppercase

string upper -q ABC DEF
and echo uppercasing a uppercase string did not fail as expected

# 'Check NUL'
# Note: We do `string escape` at the end to make a `\0` literal visible.
printf 'a\0b' | string escape
printf 'a\0c' | string match -e a | string escape
printf 'a\0d' | string split '' | string escape
printf 'a\0b' | string match -r '.*b$' | string escape
printf 'a\0b' | string replace b g | string escape
printf 'a\0b' | string replace -r b g | string escape
# TODO: These do not yet work!
# printf 'a\0b' | string match '*b' | string escape
# CHECK: a\x00b
# CHECK: a\x00c
# CHECK: a
# CHECK: \x00
# CHECK: d
# CHECK: a\x00b
# CHECK: a\x00g
# CHECK: a\x00g

# string split0
count (echo -ne 'abcdefghi' | string split0)
# CHECK: 1
count (echo -ne 'abc\x00def\x00ghi\x00' | string split0)
# CHECK: 3
count (echo -ne 'abc\x00def\x00ghi\x00\x00' | string split0)
# CHECK: 4
count (echo -ne 'abc\x00def\x00ghi' | string split0)
# CHECK: 3
count (echo -ne 'abc\ndef\x00ghi\x00' | string split0)
# CHECK: 2
count (echo -ne 'abc\ndef\nghi' | string split0)
# CHECK: 1
# #5701 - split0 always returned 1
echo -ne 'a\x00b' | string split0
and echo Split something
# CHECK: a
# CHECK: b
# CHECK: Split something

# string join0
set tmp beta alpha\ngamma
count (string join \n $tmp)
# CHECK: 3
count (string join0 $tmp)
# CHECK: 2
count (string join0 $tmp | string split0)
# CHECK: 2

# string split0 in functions
# This function outputs some newline-separated content, and some
# explicitly separated content.
function dualsplit
    echo alpha
    echo beta
    echo -ne 'gamma\x00delta' | string split0
end
count (dualsplit)
# CHECK: 4

# Ensure we handle empty outputs correctly (#5987)
count (string split / /)
# CHECK: 2
count (echo -ne '\x00\x00\x00' | string split0)
# CHECK: 3

# string collect
count (echo one\ntwo\nthree\nfour | string collect)
count (echo one | string collect)
# CHECK: 1
# CHECK: 1
echo [(echo one\ntwo\nthree | string collect)]
# CHECK: [one
# CHECK: two
# CHECK: three]
echo [(echo one\ntwo\nthree | string collect -N)]
# CHECK: [one
# CHECK: two
# CHECK: three
# CHECK: ]
printf '[%s]\n' (string collect one\n\n two\n)
# CHECK: [one]
# CHECK: [two]
printf '[%s]\n' (string collect -N one\n\n two\n)
# CHECK: [one
# CHECK:
# CHECK: ]
# CHECK: [two
# CHECK: ]
printf '[%s]\n' (string collect --no-trim-newlines one\n\n two\n)
# CHECK: [one
# CHECK:
# CHECK: ]
# CHECK: [two
# CHECK: ]
# string collect returns 0 when it has any output, otherwise 1
string collect >/dev/null; and echo unexpected success; or echo expected failure
# CHECK: expected failure
echo -n | string collect >/dev/null; and echo unexpected success; or echo expected failure
# CHECK: expected failure
echo | string collect -N >/dev/null; and echo expected success; or echo unexpected failure
# CHECK: expected success
echo | string collect >/dev/null; and echo unexpected success; or echo expected failure
# CHECK: expected failure
string collect a >/dev/null; and echo expected success; or echo unexpected failure
# CHECK: expected success
string collect -N '' >/dev/null; and echo unexpected success; or echo expected failure
# CHECK: expected failure
string collect \n\n >/dev/null; and echo unexpected success; or echo expected failure
# CHECK: expected failure

# string collect in functions
# This function outputs some newline-separated content, and some
# explicitly un-separated content.
function dualcollect
    echo alpha
    echo beta
    echo gamma\ndelta\nomega | string collect
end
count (dualcollect)
# CHECK: 3

string match -qer asd asd
echo $status
# CHECK: 0

string match -eq asd asd
echo $status
# CHECK: 0

# Unmatched capturing groups are treated as empty
echo az | string replace -r -- 'a(b.+)?z' 'a:$1z'
# CHECK: a:z
