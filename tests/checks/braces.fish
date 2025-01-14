#RUN: %fish %s

echo x-{1}
#CHECK: x-{1}

echo x-{1,2}
#CHECK: x-1 x-2

echo foo-{1,2{3,4}}
#CHECK: foo-1 foo-23 foo-24

echo foo-{} # literal "{}" expands to itself
#CHECK: foo-{}

echo foo-{{},{}} # the inner "{}" expand to themselves, the outer pair expands normally.
#CHECK: foo-{} foo-{}

echo foo-{{a},{}} # also works with something in the braces.
#CHECK: foo-{a} foo-{}

echo foo-{""} # still expands to foo-{}
#CHECK: foo-{}

echo foo-{$undefinedvar} # still expands to nothing
#CHECK:

echo foo-{,,,} # four empty items in the braces.
#CHECK: foo- foo- foo- foo-

echo foo-{,\,,} # an empty item, a "," and an empty item.
#CHECK: foo- foo-, foo-

echo .{  foo  bar  }. # see 6564
#CHECK: .{  foo  bar  }.

# whitespace within entries is retained
for foo in {a, hello
wo  rld }
    echo \'$foo\'
end
# CHECK: 'a'
# CHECK: 'hello
# CHECK: wo  rld'

for foo in {hello
world}
    echo \'$foo\'
end
#CHECK: '{hello
#CHECK: world}'

echo {a(echo ,)b}
#CHECK: {a,b}

e{cho,cho,cho}
# CHECK: echo echo

## Compound commands

{ echo compound; echo command; }
# CHECK: compound
# CHECK: command

{;echo -n start with\ ; echo semi; }
# CHECK: start with semi

{ echo no semi }
# CHECK: no semi

{echo no space}
# CHECK: no space

# Ambiguous case
{ echo ,comma;}
# CHECK: ,comma

# Ambiguous case with no trailing space
{echo comma, no space;}
# CHECK: comma, no space

# Ambiguous case with no space
PATH= {echo,hello}
# CHECKERR: fish: Unknown command: echo,hello
# CHECKERR: {{.*}}/braces.fish (line {{\d+}}):
# CHECKERR: PATH= {echo,hello}
# CHECKERR:        ^~~~~~~~~^

function foo,
    echo foo,
end
function bar
    echo bar
end
{foo,;bar}
# CHECK: foo,
# CHECK: bar

# Trailing tokens
set -l fish (status fish-path)
$fish -c '{ :; } true'
# CHECKERR: fish: '}' does not take arguments. Did you forget a ';'?
# CHECKERR: { :; } true
# CHECKERR:        ^~~^

; { echo semi; }
# CHECK: semi

a=b { echo $a; }
# CHECK: b

time { :; }
# CHECKERR:
# CHECKERR: {{_+}}
# CHECKERR: Executed in {{.*}}
# CHECKERR:  usr time {{.*}}
# CHECKERR:  sys time {{.*}}

true & { echo background; }
# CHECK: background

true && { echo conjunction; }
# CHECK: conjunction

true; and { echo and; }
# CHECK: and

true | { echo pipe; }
# CHECK: pipe

true 2>| { echo stderrpipe; }
# CHECK: stderrpipe

false || { echo disjunction; }
# CHECK: disjunction

false; or { echo or; }
# CHECK: or

begin { echo begin }
end
# CHECK: begin

not { false; true }
echo $status
# CHECK: 1

! { false }
echo $status
# CHECK: 0

if { set -l a true; $a && true }
    echo if-true
end
# CHECK: if-true

{
    set -l condition true
    while $condition
        {
            echo while
            set condition false
        }
    end
}
# CHECK: while

{{echo inner}
    echo outer}
# CHECK: inner
# CHECK: outer

{

    echo leading blank lines
}
# CHECK: leading blank lines

complete foo -a '123 456'
complete -C 'foo {' | sed 1q
# CHECK: {{\{.*}}

complete -C '{' | grep ^if\t
# CHECK: if{{\t}}Evaluate block if condition is true
complete -C '{ ' | grep ^if\t
# CHECK: if{{\t}}Evaluate block if condition is true

$fish -c '{'
# CHECKERR: fish: Expected a '}', but found end of the input

PATH= "{"
# CHECKERR: fish: Unknown command: '{'
# CHECKERR: {{.*}}/braces.fish (line {{\d+}}):
# CHECKERR: PATH= "{"
# CHECKERR:       ^~^

$fish -c 'builtin {'
# CHECKERR: fish: Expected end of the statement, but found a '{'
# CHECKERR: builtin {
# CHECKERR:      ^

$fish -c 'command {'
# CHECKERR: fish: Expected end of the statement, but found a '{'
# CHECKERR: command {
# CHECKERR:         ^

$fish -c 'exec {'
# CHECKERR: fish: Expected end of the statement, but found a '{'
# CHECKERR: exec {
# CHECKERR:      ^

$fish -c 'begin; }'
# CHECKERR: fish: Unexpected '}' for unopened brace
# CHECKERR: begin; }
# CHECKERR:        ^
