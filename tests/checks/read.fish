# RUN: fish=%fish %fish %s
# Read with no vars is not an error
read

# Read with -a and anything other than exactly on var name is an error
read -a
#CHECKERR: read: expected 1 arguments; got 0
read --array v1 v2
#CHECKERR: read: expected 1 arguments; got 2
read --list v1

# Verify correct behavior of subcommands and splitting of input.
begin
    count (echo one\ntwo)
    #CHECK: 2
    set -l IFS \t
    count (echo one\ntwo)
    #CHECK: 2
    set -l IFS
    count (echo one\ntwo)
    #CHECK: 1
    echo [(echo -n one\ntwo)]
    #CHECK: [one
    #CHECK: two]
    count (echo one\ntwo\n)
    #CHECK: 1
    echo [(echo -n one\ntwo\n)]
    #CHECK: [one
    #CHECK: two]
    count (echo one\ntwo\n\n)
    #CHECK: 1
    echo [(echo -n one\ntwo\n\n)]
    #CHECK: [one
    #CHECK: two
    #CHECK: ]
end

function print_vars --no-scope-shadowing
    set -l space
    set -l IFS \n # ensure our command substitution works right
    for var in $argv
        echo -n $space (count $$var) \'$$var\'
        set space ''
    end
    echo
end

# Test splitting input
echo 'hello there' | read -l one two
print_vars one two
#CHECK: 1 'hello' 1 'there'
echo 'hello there' | read -l one
print_vars one
#CHECK: 1 'hello there'
echo '' | read -l one
print_vars one
#CHECK: 1 ''
echo '' | read -l one two
print_vars one two
#CHECK: 1 '' 1 ''
echo test | read -l one two three
print_vars one two three
#CHECK: 1 'test' 1 '' 1 ''
echo 'foo   bar   baz' | read -l one two three
print_vars one two three
#CHECK: 1 'foo' 1 'bar' 1 '  baz'
echo -n a | read -l one
echo "$status $one"
#CHECK: 0 a

# Test splitting input with IFS empty
set -l IFS
echo hello | read -l one
print_vars one
#CHECK: 1 'hello'
echo hello | read -l one two
print_vars one two
#CHECK: 1 'h' 1 'ello'
echo hello | read -l one two three
print_vars one two three
#CHECK: 1 'h' 1 'e' 1 'llo'
echo '' | read -l one
print_vars one
#CHECK: 0
echo t | read -l one two
print_vars one two
#CHECK: 1 't' 0
echo t | read -l one two three
print_vars one two three
#CHECK: 1 't' 0 0
echo ' t' | read -l one two
print_vars one two
#CHECK: 1 ' ' 1 't'
set -le IFS

echo 'hello there' | read -la ary
print_vars ary
#CHECK: 2 'hello' 'there'
echo hello | read -la ary
print_vars ary
#CHECK: 1 'hello'
echo 'this is a bunch of words' | read -la ary
print_vars ary
#CHECK: 6 'this' 'is' 'a' 'bunch' 'of' 'words'
echo '   one   two     three' | read -la ary
print_vars ary
#CHECK: 3 'one' 'two' 'three'
echo '' | read -la ary
print_vars ary
#CHECK: 0

set -l IFS
echo hello | read -la ary
print_vars ary
#CHECK: 5 'h' 'e' 'l' 'l' 'o'
echo h | read -la ary
print_vars ary
#CHECK: 1 'h'
echo '' | read -la ary
print_vars ary
#CHECK: 0
set -le IFS

# read -n tests
echo testing | read -n 3 foo
echo $foo
#CHECK: tes
echo test | read -n 10 foo
echo $foo
#CHECK: test
echo test | read -n 0 foo
echo $foo
#CHECK: test
echo testing | begin
    read -n 3 foo
    read -n 3 bar
end
echo $foo
#CHECK: tes
echo $bar
#CHECK: tin
echo test | read -n 1 foo
echo $foo
#CHECK: t

# read -z tests
echo -n testing | read -lz foo
echo $foo
#CHECK: testing
echo -n 'test ing' | read -lz foo
echo $foo
#CHECK: test ing
echo newline | read -lz foo
echo $foo
#CHECK: newline
#CHECK:
echo -n 'test ing' | read -lz foo bar
print_vars foo bar
#CHECK: 1 'test' 1 'ing'
echo -ne 'test\0ing' | read -lz foo bar
print_vars foo bar
#CHECK: 1 'test' 1 ''
echo -ne 'foo\nbar' | read -lz foo bar
print_vars foo bar
#CHECK: 1 'foo' 1 'bar'
echo -ne 'foo\nbar\0baz\nquux' | while read -lza foo
    print_vars foo
end
#CHECK: 2 'foo' 'bar'
#CHECK: 2 'baz' 'quux'

# Chunked read tests
set -l path /tmp/fish_chunked_read_test.txt
set -l longstr (seq 1024 | string join ',')
echo -n $longstr >$path
read -l longstr2 <$path
test "$longstr" = "$longstr2"
and echo "Chunked reads test pass"
or echo "Chunked reads test failure: long strings don't match!"
rm $path
#CHECK: Chunked reads test pass

# ==========
# The following tests verify that `read` correctly handles the limit on the
# number of bytes consumed.
#
set fish_read_limit 8192
set line abcdefghijklmnopqrstuvwxyz

# Ensure the `read` command terminates if asked to read too much data. The var
# should be empty. We throw away any data we read if it exceeds the limit on
# what we consider reasonable.
yes $line | head -c (math "1 + $fish_read_limit") | read --null x
if test $status -ne 122
    echo reading too much data did not terminate with failure status
end
# The read var should be defined but not have any elements when the read
# aborts due to too much data.
set -q x
or echo reading too much data did not define the var
set -q x[1]
and echo reading too much data resulted in a var with unexpected data

# Ensure the `read` command terminates if asked to read too much data even if
# given an explicit limit. The var should be empty. We throw away any data we
# read if it exceeds the limit on what we consider reasonable.
yes $line | read --null --nchars=(math "$fish_read_limit + 1") x
if test $status -ne 122
    echo reading too much data did not terminate with failure status
end
set -q x
or echo reading too much data with --nchars did not define the var
set -q x[1]
and echo reading too much data with --nchars resulted in a var with unexpected data

# Now do the opposite of the previous test and confirm we can read reasonable
# amounts of data.
echo $line | read x
if test $status -ne 0
    echo the read of a reasonable amount of data failed unexpectedly
end
set exp_length (string length $x)
set act_length (string length $line)
if test $exp_length -ne $act_length
    echo reading a reasonable amount of data failed the length test
    echo expected length $exp_length, actual length $act_length
end

# Confirm we can read exactly up to the limit.
yes $line | read --null --nchars $fish_read_limit x
if test $status -ne 0
    echo the read of the max amount of data with --nchars failed unexpectedly
end
if test (string length "$x") -ne $fish_read_limit
    echo reading the max amount of data with --nchars failed the length test
end

# Same as previous test but limit the amount of data fed to `read` rather than
# using the `--nchars` flag.
yes $line | head -c $fish_read_limit | read --null x
if test $status -ne 0
    echo the read of the max amount of data failed unexpectedly
end
if test (string length "$x") -ne $fish_read_limit
    echo reading with a limited amount of input data failed the length test
end

# Confirm reading non-interactively works -- #4206 regression
echo abc\ndef | $fish -i -c 'read a; read b; set --show a; set --show b'
#CHECK: $a: set in global scope, unexported, with 1 elements
#CHECK: $a[1]: |abc|
#CHECK: $b: set in global scope, unexported, with 1 elements
#CHECK: $b[1]: |def|

# Test --delimiter (and $IFS, for now)
echo a=b | read -l foo bar
echo $foo
echo $bar
#CHECK: a=b

# Delimiter =
echo a=b | read -l -d = foo bar
echo $foo
#CHECK: a
echo $bar
#CHECK: b

# Delimiter empty
echo a=b | read -l -d '' foo bar baz
echo $foo
#CHECK: a
echo $bar
#CHECK: =
echo $baz
#CHECK: b

# IFS empty string
set -l IFS ''
echo a=b | read -l foo bar baz
echo $foo
#CHECK: a
echo $bar
#CHECK: =
echo $baz
#CHECK: b

# IFS unset
set -e IFS
echo a=b | read -l foo bar baz
echo $foo
#CHECK: a=b
echo $bar
#CHECK:
echo $baz
#CHECK:

# Delimiter =
echo a=b | read -l -d = foo bar baz
echo $foo
#CHECK: a
echo $bar
#CHECK: b
echo $baz
#CHECK:

# Multi-char delimiters with -d
echo a...b...c | read -l -d "..." a b c
echo $a
#CHECK: a
echo $b
#CHECK: b
echo $c
#CHECK: c
# Multi-char delimiters with IFS
begin
    set -l IFS "..."
    echo a...b...c | read -l a b c
    echo $a
    echo $b
    echo $c
end
#CHECK: a
#CHECK: b
#CHECK: ..c

# At one point, whatever was read was printed _before_ banana
echo banana (echo sausage | read)
echo 'a | b' | read -lt a b c
#CHECK: banana sausage

echo a $a
echo b $b
echo c $c
# CHECK: a a
# CHECK: b |
# CHECK: c b

echo 'a"foo bar"b' | read -lt a b c

echo a \'$a\'
echo b $b
echo c $c
# CHECK: a 'afoo barb'
# CHECK: b
# CHECK: c

function function-scoped-read
    echo foo | read --function skamtebord
    set -S skamtebord
    begin
        echo bar | read skamtebord
        echo baz | read -f craaab
    end
    set -S skamtebord
    set -S craaab
end

function-scoped-read
# CHECK: $skamtebord: set in local scope, unexported, with 1 elements
# CHECK: $skamtebord[1]: |foo|
# CHECK: $skamtebord: set in local scope, unexported, with 1 elements
# CHECK: $skamtebord[1]: |bar|
# CHECK: $craaab: set in local scope, unexported, with 1 elements
# CHECK: $craaab[1]: |baz|

echo foo\nbar\nbaz | begin
    read -l foo
    read -l bar
    cat
    # CHECK: baz
    echo $bar
    # CHECK: bar
end

begin
    echo 1
    echo 2
end | read -l --line foo bar
echo $foo $bar
# CHECK: 1 2

echo foo | read status
# CHECKERR: read: status: cannot overwrite read-only variable
# CHECKERR: {{.*}}read.fish (line {{\d+}}):
# CHECKERR: echo foo | read status
# CHECKERR: ^
# CHECKERR: (Type 'help read' for related documentation)
echo read $status
# CHECK: read 2

echo ' foo' | read -n 1 -la var
set -S var
#CHECK: $var: set in local scope, unexported, with 0 elements

echo foo | read -n -1
# CHECKERR: read: -1: invalid integer
# CHECKERR: {{.*}}read.fish (line {{\d+}}):
# CHECKERR: echo foo | read -n -1
# CHECKERR: ^
# CHECKERR: (Type 'help read' for related documentation)

echo '1 ( (' | read -lat var
set -S var
# CHECK: $var: set in local scope, unexported, with 2 elements
# CHECK: $var[1]: |1|
# CHECK: $var[2]: |( (|

echo '1 ) )' | read -lat var
set -S var
# CHECK: $var: set in local scope, unexported, with 2 elements
# CHECK: $var[1]: |1|
# CHECK: $var[2]: |)|

echo '1 { {' | read -lat var
set -S var
# CHECK: $var: set in local scope, unexported, with 2 elements
# CHECK: $var[1]: |1|
# CHECK: $var[2]: |{ {|

echo '1 } }' | read -lat var
set -S var
# CHECK: $var: set in local scope, unexported, with 2 elements
# CHECK: $var[1]: |1|
# CHECK: $var[2]: |}|

# Raw tokens into named variables
echo 'echo "&" a\ b &
second line (dropped)' | read -l --tokenize-raw head tail
set -S head tail
# CHECK: $head: set in local scope, unexported, with 1 elements
# CHECK: $head[1]: |echo|
# CHECK: $tail: set in local scope, unexported, with 1 elements
# CHECK: $tail[1]: |"&" a\\ b &|

# Raw tokens into list
echo 'echo "&" & a\ b
second line (dropped)' | read -l --tokenize-raw -a rawlist
set -S rawlist
# CHECK: $rawlist: set in local scope, unexported, with 4 elements
# CHECK: $rawlist[1]: |echo|
# CHECK: $rawlist[2]: |"&"|
# CHECK: $rawlist[3]: |&|
# CHECK: $rawlist[4]: |a\\ b|

echo 'echo "&" & a\ b
second line' | read -l --tokenize-raw -a rawlist_null -z
set -S rawlist_null
# CHECK: $rawlist_null: set in local scope, unexported, with 8 elements
# CHECK: $rawlist_null[1]: |echo|
# CHECK: $rawlist_null[2]: |"&"|
# CHECK: $rawlist_null[3]: |&|
# CHECK: $rawlist_null[4]: |a\\ b|
# CHECK: $rawlist_null[5]: |\n|
# CHECK: $rawlist_null[6]: |second|
# CHECK: $rawlist_null[7]: |line|
# CHECK: $rawlist_null[8]: |\n|

echo '1  {} "{}"' | read -lat var
echo $var
# CHECK: 1 {} {}

printf \xf9\x98\xb1\x83\x8b | read -z out_of_range_codepoint
set -S out_of_range_codepoint
# CHECK: $out_of_range_codepoint: set in global scope, unexported, with 1 elements
# CHECK: $out_of_range_codepoint[1]: |\Xf9\X98\Xb1\X83\X8b|

printf \xff | { read invalid_utf8; set -S invalid_utf8 }
# CHECK: $invalid_utf8: set in global scope, unexported, with 1 elements
# CHECK: $invalid_utf8[1]: |\Xff|
