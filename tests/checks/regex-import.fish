#RUN: %fish %s
# Tests for importing named regex groups as fish variables

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
echo "foo" | string match -rq -- '^(?<foo>bar)$'
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
