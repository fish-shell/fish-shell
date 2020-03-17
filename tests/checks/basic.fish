#RUN: %fish %s
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
