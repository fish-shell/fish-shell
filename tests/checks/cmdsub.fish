#RUN: %fish %s

# Command substitution inside double quotes strips trailing newline.
echo "a$(echo b)c"
# CHECK: abc

# Nesting
echo "$(echo "$(echo a)")"
# CHECK: a
echo "$(echo $(echo b))"
# CHECK: b

echo "$(echo multiple).$(echo command).$(echo substitutions)"
# CHECK: multiple.command.substitutions

test -n "$()" || echo "empty list is interpolated to empty string"
# CHECK: empty list is interpolated to empty string

# Variables in command substitution output are not expanded.
echo "$(echo \~ \$HOME)"
# CHECK: ~ $HOME

echo "$(printf %s 'quoted command substitution multiline output
line 2
line 3
')"
# CHECK: quoted command substitution multiline output
# CHECK: line 2
# CHECK: line 3

echo trim any newlines "$(echo \n\n\n)" after cmdsub
#CHECK: trim any newlines  after cmdsub

echo i{1, (echo 2), "$(echo 3)"}
# CHECK: i1 i2 i3

echo "$(echo index\nrange\nexpansion)[2]"
#CHECK: range

echo "$(echo '"')"
#CHECK: "

echo "$(echo $(echo 1) ())"
#CHECK: 1
