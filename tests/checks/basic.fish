# RUN: %fish -C 'set -g fish %fish' %s
#
# Test function, loops, conditionals and some basic elements
#

# Comments in odd places don't cause problems
for i in 1 2 # Comment on same line as command
# Comment inside loop
    for j in a b
		# Double loop
        echo $i$j
	end;
end
#CHECK: 1a
#CHECK: 1b
#CHECK: 2a
#CHECK: 2b

# Escaped newlines
echo foo\ bar
echo foo\
bar
echo "foo\
bar"
echo 'foo\
bar'
#CHECK: foo bar
#CHECK: foobar
#CHECK: foobar
#CHECK: foo\
#CHECK: bar

for i in \
    a b c
    echo $i
end
#CHECK: a
#CHECK: b
#CHECK: c


# Simple function tests

function foo
    echo >../test/temp/fish_foo.txt $argv
end

foo hello

cat ../test/temp/fish_foo.txt |read foo

if test $foo = hello;
  echo Test 2 pass
else
  echo Test 2 fail
end
#CHECK: Test 2 pass

function foo
    printf 'Test %s' $argv[1]; echo ' pass'
end

foo 3a
#CHECK: Test 3a pass

for i in Test for continue break and switch builtins problems;
	switch $i
		case Test
			printf "%s " $i
		case "for"
			printf "%s " 3b
		case "c*"
			echo pass
		case break
			continue
			echo fail
		case and
			break
			echo fail
		case "*"
			echo fail
	end
end
#CHECK: Test 3b pass

set -l sta
if eval true
	if eval false
		set sta fail
	else
		set sta pass
	end
else
	set sta fail
end
echo Test 4 $sta
#CHECK: Test 4 pass

# Testing builtin status

function test_builtin_status
	return 1
end
test_builtin_status
if [ $status -eq 1 ]
	set sta pass
else
	set sta fail
end
echo Test 5 $sta
#CHECK: Test 5 pass

####################
# echo tests
echo 'abc\ndef'
#CHECK: abc\ndef
echo -e 'abc\ndef'
#CHECK: abc
#CHECK: def
echo -e 'abc\zdef'
#CHECK: abc\zdef
echo -e 'abc\41def'
echo -e 'abc\041def'
#CHECK: abc!def
#CHECK: abc!def
echo -e 'abc\121def'
echo -e 'abc\1212def'
#CHECK: abcQdef
#CHECK: abcQ2def
echo -e 'abc\cdef' # won't output a newline!
#CHECK: abc
echo ''
echo -
#CHECK: -
echo -h
#CHECK: -h
echo -ne '\376' | display_bytes
#CHECK: 0000000 376
#CHECK: 0000001
echo -e 'abc\x21def'
echo -e 'abc\x211def'
#CHECK: abc!def
#CHECK: abc!1def

# Comments allowed in between lines (#1987)
echo before comment \
  # comment
  after comment
#CHECK: before comment after comment

# Backslashes are part of comments and do not join lines (#1255)
# This should execute false, not echo it
echo -n # comment\
false

function always_fails
    if true
        return 1
    end
end

# Verify $argv set correctly in sourced scripts (#139)
echo 'echo "source argv {$argv}"' | source
#CHECK: source argv {}
echo 'echo "source argv {$argv}"' | source -
#CHECK: source argv {}
echo 'echo "source argv {$argv}"' | source - abc
#CHECK: source argv {abc}
echo 'echo "source argv {$argv}"' | source - abc def
#CHECK: source argv {abc def}

always_fails
echo $status
#CHECK: 1

# Test that subsequent cases do not blow away the status from previous ones
for val in one two three four
	switch $val
	case one
		/bin/sh -c 'exit 1'
	case two
		/bin/sh -c 'exit 2'
	case three
		/bin/sh -c 'exit 3'
	end
	echo $status
end
#CHECK: 1
#CHECK: 2
#CHECK: 3
#CHECK: 0

# Test that the `switch` builtin itself does not blow away status before evaluating a case
false
switch one
case one
	echo $status
end
#CHECK: 1

#test contains -i
contains -i string a b c string d
#CHECK: 4
contains -i string a b c d; or echo nothing
#CHECK: nothing
contains -i -- string a b c string d
#CHECK: 4
contains -i -- -- a b c; or echo nothing
#CHECK: nothing
contains -i -- -- a b c -- v
#CHECK: 4

# Test if, else, and else if
if true
	echo alpha1.1
	echo alpha1.2
else if false
	echo beta1.1
	echo beta1.2
else if false
	echo gamma1.1
	echo gamma1.2
else
	echo delta1.1
	echo delta1.2
end
#CHECK: alpha1.1
#CHECK: alpha1.2

if false
	echo alpha2.1
	echo alpha2.2
else if begin ; true ; end
	echo beta2.1
	echo beta2.2
else if begin ; echo nope2.1; false ; end
	echo gamma2.1
	echo gamma2.2
else
	echo delta2.1
	echo delta2.2
end
#CHECK: beta2.1
#CHECK: beta2.2

if false
	echo alpha3.1
	echo alpha3.2
else if begin ; echo yep3.1; false ; end
	echo beta3.1
	echo beta3.2
else if begin ; echo yep3.2; true ; end
	echo gamma3.1
	echo gamma3.2
else
	echo delta3.1
	echo delta3.2
end
#CHECK: yep3.1
#CHECK: yep3.2
#CHECK: gamma3.1
#CHECK: gamma3.2

if false
	echo alpha4.1
	echo alpha4.2
else if begin ; echo yep4.1; false ; end
	echo beta4.1
	echo beta4.2
else if begin ; echo yep4.2; false ; end
	echo gamma4.1
	echo gamma4.2
else
	echo delta4.1
	echo delta4.2
end
#CHECK: yep4.1
#CHECK: yep4.2
#CHECK: delta4.1
#CHECK: delta4.2

if test ! -n "abc"
else if test -n "def"
	echo "epsilon5.2"
else if not_a_valid_command but it should be OK because a previous branch was taken
	echo "epsilon 5.3"
else if test ! -n "abc"
	echo "epsilon 5.4"	
end
#CHECK: epsilon5.2

# Ensure builtins work
# https://github.com/fish-shell/fish-shell/issues/359
if not echo skip1 > /dev/null
	echo "zeta 6.1"
else if echo skip2 > /dev/null
	echo "zeta 6.2"
end
#CHECK: zeta 6.2

echo '###'
#CHECK: ###

# Ensure 'type' works
# https://github.com/fish-shell/fish-shell/issues/513
function fish_test_type_zzz
	true
end
# Should succeed
type -q fish_test_type_zzz ; echo $status
#CHECK: 0
# Should fail
type -q -f fish_test_type_zzz ; echo $status
#CHECK: 1

# ensure that builtins that produce no output can still truncate files
# (bug PCA almost reintroduced!)
echo abc > ../test/temp/file_truncation_test.txt
cat ../test/temp/file_truncation_test.txt
echo -n > ../test/temp/file_truncation_test.txt
cat ../test/temp/file_truncation_test.txt
#CHECK: abc

# Test events.


# This pattern caused a crash; github issue #449

set -g var before

function test1 --on-event test
    set -g var $var:test1
    functions -e test2
end

function test2 --on-event test
    # this should not run, as test2 gets removed before it has a chance of running
    set -g var $var:test2a
end
emit test

echo $var
#CHECK: before:test1


function test3 --on-event test3
    echo received event test3 with args: $argv
end

emit test3 foo bar
#CHECK: received event test3 with args: foo bar

# test empty argument
emit
#CHECKERR: emit: expected event name

# Test break and continue
# This should output Ping once
for i in a b c
    if not contains $i c ; continue ; end
    echo Ping
end
#CHECK: Ping

# This should output Pong not at all
for i in a b c
    if not contains $i c ; break ; end
    echo Pong
end

# This should output Foop three times, and Boop not at all
set i a a a
while contains $i a
    set -e i[-1]
    echo Foop
    continue
    echo Boop
end
#CHECK: Foop
#CHECK: Foop
#CHECK: Foop

# This should output Doop once
set i a a a
while contains $i a
    set -e i[-1]
    echo Doop
    break
    echo Darp
end
#CHECK: Doop

# Test implicit cd. This should do nothing.
./

# Test special for loop expansion
# Here we the name of the variable is derived from another variable
set var1 var2
for $var1 in 1 2 3
    echo -n $var2
end
echo
#CHECK: 123

# Test status -n
eval 'status -n
status -n
status -n'
#CHECK: 1
#CHECK: 2
#CHECK: 3

# Test support for unbalanced blocks
function try_unbalanced_block
    $fish -c "echo $argv | source " 2>&1 | grep "Missing end" 1>&2
end
try_unbalanced_block 'begin'
#CHECKERR: - (line 1): Missing end to balance this begin
try_unbalanced_block 'while true'
#CHECKERR: - (line 1): Missing end to balance this while loop
try_unbalanced_block 'for x in 1 2 3'
#CHECKERR: - (line 1): Missing end to balance this for loop
try_unbalanced_block 'switch abc'
#CHECKERR: - (line 1): Missing end to balance this switch statement
try_unbalanced_block 'function anything'
#CHECKERR: - (line 1): Missing end to balance this function definition
try_unbalanced_block 'if false'
#CHECKERR: - (line 1): Missing end to balance this if statement

# Ensure that quoted keywords work
'while' false; end
"while" false; end
"wh"'ile' false; "e"nd

# BOM checking (see #1518). But only in UTF8 locales.
# (locale guarded because of musl)
if command -sq locale; and string match -qi '*utf-8*' -- (locale)
    echo \uFEFF"echo bom_test" | source
else
    echo "echo bom_test" | source
end
#CHECK: bom_test

# Comments abutting text (#953)
echo not#a#comment
#CHECK: not#a#comment
echo is # a # comment
#CHECK: is

# Test that our builtins can all do --query
command --query cp
echo $status
#CHECK: 0

type --query cp
echo $status
#CHECK: 0

jobs --query 0
echo $status
#CHECK: 1

abbr --query thisshouldnotbeanabbreviationohmygoshitssolongwhywouldanyoneeverusethis
echo $status
#CHECK: 1

functions --query alias
echo $status
#CHECK: 0

set --query status
echo $status
#CHECK: 0

builtin --query echo
echo $status
#CHECK: 0
