# RUN: fish=%fish %fish %s

# caret position (#5812)
printf '<%s>\n' ($fish -c ' $f[a]' 2>&1)

# CHECK: <fish: Invalid index value>
# CHECK: < $f[a]>
# CHECK: <    ^>

printf '<%s>\n' ($fish -c 'if $f[a]; end' 2>&1)
# CHECK: <fish: Invalid index value>
# CHECK: <if $f[a]; end>
# CHECK: <      ^>

set a A
set aa AA
set aaa AAA
echo {$aa}a{1,2,3}(for a in 1 2 3; echo $a; end)
#CHECK: AAa11 AAa21 AAa31 AAa12 AAa22 AAa32 AAa13 AAa23 AAa33

# basic expansion test
echo {}
echo {apple}
echo {apple,orange}
#CHECK: {}
#CHECK: {apple}
#CHECK: apple orange

# expansion tests with spaces
echo {apple, orange}
echo { apple, orange, banana }
#CHECK: apple orange
#CHECK: apple orange banana

# expansion with spaces and cartesian products
echo \'{ hello , world }\'
#CHECK: 'hello' 'world'

# expansion with escapes
for phrase in {good\,,   beautiful ,morning}
    echo -n "$phrase "
end | string trim
echo
for phrase in {goodbye\,,\ cruel\ ,world\n}
    echo -n $phrase
end
#CHECK: good, beautiful morning
#CHECK: goodbye, cruel world

# dual expansion cartesian product
echo { alpha, beta }\ {lambda, gamma }, | string replace -r ',$' ''
#CHECK: alpha lambda, beta lambda, alpha gamma, beta gamma

# expansion with subshells
for name in { (echo Meg), (echo Jo) }
    echo $name
end
#CHECK: Meg
#CHECK: Jo

# subshells with expansion
for name in (for name in {Beth, Amy}; printf "$name\n"; end)
    printf "$name\n"
end
#CHECK: Beth
#CHECK: Amy

echo {{a,b}}
#CHECK: {a} {b}

# Test expansion of variables

# We don't use the test utility function of the same name because we want
# different behavior. Specifically, that the expansion of any variables or
# other strings before we are invoked produce the expected expansion.
function expansion --description 'Prints argument count followed by arguments'
    echo (count $argv) $argv
end

set -l foo
expansion "$foo"
expansion $foo
expansion "prefix$foo"
expansion prefix$foo
#CHECK: 1
#CHECK: 0
#CHECK: 1 prefix
#CHECK: 0

expansion "$$foo"
expansion $$foo
expansion "prefix$$foo"
expansion prefix$$foo
#CHECK: 1
#CHECK: 0
#CHECK: 1 prefix
#CHECK: 0

set -l foo ''
expansion "$foo"
expansion $foo
expansion "prefix$foo"
expansion prefix$foo
#CHECK: 1
#CHECK: 1
#CHECK: 1 prefix
#CHECK: 1 prefix

expansion "$$foo"
expansion $$foo
expansion "prefix$$foo"
expansion prefix$$foo
#CHECK: 1
#CHECK: 0
#CHECK: 1 prefix
#CHECK: 0

set -l foo bar
set -l bar
expansion "$$foo"
expansion $$foo
expansion "prefix$$foo"
expansion prefix$$foo
#CHECK: 1
#CHECK: 0
#CHECK: 1 prefix
#CHECK: 0

set -l bar baz
expansion "$$foo"
expansion $$foo
expansion "prefix$$foo"
expansion prefix$$foo
#CHECK: 1 baz
#CHECK: 1 baz
#CHECK: 1 prefixbaz
#CHECK: 1 prefixbaz

set -l bar baz quux
expansion "$$foo"
expansion $$foo
expansion "prefix$$foo"
expansion prefix$$foo
#CHECK: 1 baz quux
#CHECK: 2 baz quux
#CHECK: 1 prefixbaz quux
#CHECK: 2 prefixbaz prefixquux

set -l foo bar fooer fooest
set -l fooer
set -l fooest
expansion "$$foo"
expansion $$foo
expansion "prefix$$foo"
expansion prefix$$foo
#CHECK: 1 baz quux fooer fooest
#CHECK: 2 baz quux
#CHECK: 1 prefixbaz quux fooer fooest
#CHECK: 2 prefixbaz prefixquux

set -l fooer ''
expansion $$foo
expansion prefix$$foo
#CHECK: 3 baz quux
#CHECK: 3 prefixbaz prefixquux prefix

# Slices

set -l foo bar '' fooest
expansion "$$foo"
expansion $$foo
expansion "prefix$$foo"
expansion prefix$$foo
expansion $foo[-5..2] # No result, because the starting index is invalid and we force-reverse.
expansion $foo[-2..-1]
expansion $foo[-10..-5]
expansion (printf '%s\n' $foo)[-5..2]
expansion (printf '%s\n' $foo)[-2..-1]
expansion (printf '%s\n' $foo)[-10..-5]
expansion (echo one)[2..-1]
#CHECK: 1 baz quux  fooest
#CHECK: 2 baz quux
#CHECK: 1 prefixbaz quux  fooest
#CHECK: 2 prefixbaz prefixquux
#CHECK: 0
#CHECK: 2  fooest
#CHECK: 0
#CHECK: 0
#CHECK: 2  fooest
#CHECK: 0
#CHECK: 0

set -l foo
expansion "$foo[1]"
expansion $foo[1]
expansion "$foo[-1]"
expansion $foo[-1]
expansion "$foo[2]"
expansion $foo[2]
expansion "$foo[1 2]"
expansion $foo[1 2]
expansion "$foo[2 1]"
expansion $foo[2 1]
#CHECK: 1
#CHECK: 0
#CHECK: 1
#CHECK: 0
#CHECK: 1
#CHECK: 0
#CHECK: 1
#CHECK: 0
#CHECK: 1
#CHECK: 0
set -l foo a b c
expansion $foo[17]
expansion $foo[-17]
expansion $foo[17..18]
expansion $foo[4..-2]
#CHECK: 0
#CHECK: 0
#CHECK: 0
#CHECK: 0
set -l foo a
expansion $foo[2..-1]
#CHECK: 0
expansion $foo[0]
#CHECKERR: {{.*}}expansion.fish (line {{\d+}}): array indices start at 1, not 0.
#CHECKERR: expansion $foo[0]
#CHECKERR: ^
# see https://github.com/fish-shell/fish-shell/issues/8213
expansion $foo[1..0]
#CHECKERR: {{.*}}expansion.fish (line {{\d+}}): array indices start at 1, not 0.
#CHECKERR: expansion $foo[1..0]
#CHECKERR: ^
expansion $foo[-0]
#CHECKERR: {{.*}}expansion.fish (line {{\d+}}): array indices start at 1, not 0.
#CHECKERR: expansion $foo[-0]
#CHECKERR: ^

echo "$foo[d]"
#CHECKERR: {{.*}}expansion.fish (line {{\d+}}): Invalid index value
#CHECKERR: echo "$foo[d]"
#CHECKERR: ^
echo $foo[d]
#CHECKERR: {{.*}}expansion.fish (line {{\d+}}): Invalid index value
#CHECKERR: echo $foo[d]
#CHECKERR: ^

echo ()[1]
# No output
echo ()[d]
#CHECKERR: {{.*}}expansion.fish (line {{\d+}}): Invalid index value
#CHECKERR: echo ()[d]
#CHECKERR: ^

set -l outer out1 out2
set -l inner 1 2
echo $outer[$inner[2]]
#CHECK: out2
echo $outer[$inner[2..1]]
#CHECK: out2 out1

# Percent self
echo %selfNOT NOT%self \%self "%self" '%self'
echo %self | string match -qr '^\\d+$'
#CHECK: %selfNOT NOT%self %self %self %self
echo "All digits: $status"
#CHECK: All digits: 0

set paren ')'
echo $$paren
#CHECKERR: {{.*}}expansion.fish (line {{\d+}}): $) is not a valid variable in fish.
#CHECKERR: echo $$paren
#CHECKERR: ^

# Test tilde expansion
# On OS X, /tmp is symlinked to /private/tmp
# $PWD is our best bet for resolving it
set -l saved $PWD
cd (mktemp -d)
set tmpdir $PWD
cd $saved
mkdir $tmpdir/realhome
ln -s $tmpdir/realhome $tmpdir/linkhome
set expandedtilde (env HOME=$tmpdir/linkhome $fish -c 'echo ~')
if test $expandedtilde != $tmpdir/linkhome
	echo '~ expands to' $expandedtilde ' - expected ' $tmpdir/linkhome
end
rm $tmpdir/linkhome
rmdir $tmpdir/realhome
rmdir $tmpdir

# Test path variables
set TEST_DELIMITER one two three
set TEST_DELIMITER_PATH one two three
echo TEST_DELIMITER: $TEST_DELIMITER "$TEST_DELIMITER"
echo TEST_DELIMITER_PATH: $TEST_DELIMITER_PATH "$TEST_DELIMITER_PATH"
#CHECK: TEST_DELIMITER: one two three one two three
#CHECK: TEST_DELIMITER_PATH: one two three one:two:three

set testvar ONE:TWO:THREE
echo "Not a path: $testvar" (count $testvar)
#CHECK: Not a path: ONE:TWO:THREE 1
set --path testvar $testvar
echo "As a path: $testvar" (count $testvar)
#CHECK: As a path: ONE:TWO:THREE 3
set testvar "$testvar:FOUR"
echo "Appended path: $testvar" (count $testvar)
#CHECK: Appended path: ONE:TWO:THREE:FOUR 4
set --unpath testvar $testvar
echo "Back to normal variable: $testvar" (count $testvar)
#CHECK: Back to normal variable: ONE TWO THREE FOUR 4

# Test fatal syntax errors
$fish -c 'echo $,foo'
#CHECKERR: fish: Expected a variable name after this $.
#CHECKERR: echo $,foo
#CHECKERR: ^
$fish -c 'echo {'
#CHECKERR: fish: Unexpected end of string, incomplete parameter expansion
#CHECKERR: echo {
#CHECKERR: ^
$fish -c 'echo {}}'
#CHECKERR: fish: Unexpected '}' for unopened brace
#CHECKERR: echo {}}
#CHECKERR: ^
printf '<%s>\n' ($fish -c 'command (asd)' 2>&1)
#CHECK: <fish: command substitutions not allowed in command position. Try var=(your-cmd) $var ...>
#CHECK: <command (asd)>
#CHECK: <        ^~~~^>
true

printf '<%s>\n' ($fish -c 'echo "$abc["' 2>&1)
#CHECK: <fish: Invalid index value>
#CHECK: <echo "$abc[">
#CHECK: <           ^>

set -l pager command less
echo foo | $pager
#CHECKERR: {{.*}}checks/expansion.fish (line 339): The expanded command is a keyword.
#CHECKERR: echo foo | $pager
#CHECKERR:            ^~~~~^

"command" -h
#CHECKERR: Documentation for command

echo {~,asdf}
# CHECK: /{{.*}} asdf
echo {asdf,~}
# CHECK: asdf /{{.*}}
echo {~}
# CHECK: {~}

function compare
    test $argv[1] = $argv[2]
    or begin
        echo unexpected expansion result:
        echo expected: $argv[1]
        echo actual: $argv[2]
    end
end
if string match -rq -- '^[\w.-]+$' $USER
    set -l user_home "$(eval "echo ~$USER")"
    compare $user_home "$(echo ~$USER)"
    compare $user_home "$(echo ~(printf %s $USER))"
end
