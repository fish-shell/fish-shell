#RUN: %ghoti %s
# Tests for importing named regex groups as ghoti variables

# Invalid variable name?
string match --regex -q '(?<FISH_VERSION>.*)' -- derp
# CHECKERR: Modification of read-only variable "FISH_VERSION" is not allowed

# Capture first match
echo "hello world" | string match --regex -q -- '(?<words>[^ ]+) ?'
printf "%s\n" $words
# CHECK: hello

# Capture multiple matches
echo "hello world" | string match --regex -q --all -- '(?<words>[^ ]+) ?'
printf "%s\n" $words
# CHECK: hello
# CHECK: world

# Capture multiple variables
echo "hello world"\n"snello snorld" | string match -rq -- '^(?<word1>[^ ]+) (?<word2>.*)$'
printf "%s\n" $word1 $word2
# CHECK: hello
# CHECK: world

# Clear variables on no match
set foo foo
echo foo | string match -rq -- '^(?<foo>bar)$'
echo $foo
# CHECK:

# Named group may be empty in some of the matches
set word
set punctuation
echo "hello world, boy!" | string match -a -qr -- '(?<word>[^ .,!;]+)(?<punctuation>[.,!;])?'
echo $word
# CHECK: hello world boy
printf "%s\n" $punctuation
# CHECK:
# CHECK: ,
# CHECK: !

# Same thing with multiple arguments
set word
set punctuation
printf '%s\n' "hello world, boy!" "shello shorld, shoy!" | string match -a -qr -- '(?<word>[^ .,!;]+)(?<punctuation>[.,!;])?'
echo $word
# CHECK: hello world boy
printf "%s\n" $punctuation
# CHECK:
# CHECK: ,
# CHECK: !

# Verify read-only variables may not be imported
echo hello | string match -rq "(?<version>.*)"
# CHECKERR: Modification of read-only variable "version" is not allowed

# Verify that the *first matching argument* is used.
string match -rq '(?<bee>b.*)' -- aaa ba ccc be
echo $bee
# CHECK: ba

# Verify the following regarding capture groups which are not matched:
#    1. Set no values if --all is not provided
#    2. Set an empty string value if --all is provided
set -e nums
set -e text

string match -r '(?<nums>\d+)|(?<text>[a-z]+)' -- xyz
# CHECK: xyz
# CHECK: xyz
set --show text
# CHECK: $text: set in global scope, unexported, with 1 elements
# CHECK: $text[1]: |xyz|
set --show nums
# CHECK: $nums: set in global scope, unexported, with 0 elements

string match -r --all '(?<nums>\d+)|(?<text>[a-z]+)' -- '111 aaa 222 bbb'
# CHECK: 111
# CHECK: 111
# CHECK: aaa
# CHECK: aaa
# CHECK: 222
# CHECK: 222
# CHECK: bbb
# CHECK: bbb

set --show nums
# CHECK: $nums: set in global scope, unexported, with 4 elements
# CHECK: $nums[1]: |111|
# CHECK: $nums[2]: ||
# CHECK: $nums[3]: |222|
# CHECK: $nums[4]: ||

set --show text
# CHECK: $text: set in global scope, unexported, with 4 elements
# CHECK: $text[1]: ||
# CHECK: $text[2]: |aaa|
# CHECK: $text[3]: ||
# CHECK: $text[4]: |bbb|

# Regression test for #7938.
set -e text
echo one\ntwo | string match -ra '(?<text>[a-z]+)'
# CHECK: one
# CHECK: one
# CHECK: two
# CHECK: two
set --show text
# CHECK: $text: set in global scope, unexported, with 1 elements
# CHECK: $text[1]: |one|

set -e text
echo three\nfour | string match -qra '(?<text>[a-z]+)'
set --show text
# CHECK: $text: set in global scope, unexported, with 1 elements
# CHECK: $text[1]: |three|

set -e text
echo 555\nsix | string match -qra '(?<text>[a-z]+)'
set --show text
# CHECK: $text: set in global scope, unexported, with 1 elements
# CHECK: $text[1]: |six|
