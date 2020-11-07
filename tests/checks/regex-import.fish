#RUN: %fish %s
# Tests for importing named regex groups as fish variables

# Capture first match
echo "hello world" | string match --regex -q --import -- '(?<words>[^ ]+) ?'
printf "%s\n" $words
# CHECK: hello

# Capture multiple matches
echo "hello world" | string match --regex -q --import --all -- '(?<words>[^ ]+) ?'
printf "%s\n" $words
# CHECK: hello
# CHECK: world

# Capture multiple variables
echo "hello world" | string match -rqI -- '^(?<word1>[^ ]+) (?<word2>.*)$'
printf "%s\n" $word1 $word2
# CHECK: hello
# CHECK: world

# Clear variables on no match
set foo foo
echo "foo" | string match -rqI -- '^(?<foo>bar)$'
echo $foo
# CHECK:

# Named group may be empty in some of the matches
set word
set punctuation
echo "hello world, boy!" | string match -a -qrI -- '(?<word>[^ .,!;]+)(?<punctuation>[.,!;])?'
echo $word
# CHECK: hello world boy
printf "%s\n" $punctuation
# CHECK:
# CHECK: ,
# CHECK: !
