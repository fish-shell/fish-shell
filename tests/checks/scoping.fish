#RUN: %fish %s
# Test scoping rules for functions and status

set -e smurf

function setter
    set smurf green
end

function unsetter
    set -e smurf
end

function call1
    set smurf blue
    setter
    if test $smurf = blue
        echo Test 1 pass
    else
        echo Test 1 fail
    end
end

function call2
    set smurf blue
    unsetter
    if test $smurf = blue
        echo Test 2 pass
    else
        echo Test 2 fail
    end
end

call1
#CHECK: Test 1 pass
call2
#CHECK: Test 2 pass

function call3
    setter
    if test $smurf = green
        echo Test 3 pass
    else
        echo Test 3 fail
    end
end

function call4
    unsetter
    if not set -q smurf
        echo Test 4 pass
    else
        echo Test 4 fail
    end
end

set -g smurf yellow
call3
#CHECK: Test 3 pass
call4
#CHECK: Test 4 pass

set -l foo 1
set -g bar 2
set -U baz 3

set -l -q foo

if test $status -ne 0
    echo Test 5 fail
else
    echo Test 5 pass
end
#CHECK: Test 5 pass

if not set -g -q bar
    echo Test 6 fail
else
    echo Test 6 pass
end
#CHECK: Test 6 pass

if not set -U -q baz
    echo Test 7 fail
else
    echo Test 7 pass
end
#CHECK: Test 7 pass

set -u -l -q foo
if test $status -ne 0
    echo Test 8 fail
else
    echo Test 8 pass

end
#CHECK: Test 8 pass

if not set -u -g -q bar
    echo Test 9 fail
else
    echo Test 9 pass
end
#CHECK: Test 9 pass

if not set -u -U -q baz
    echo Test 10 fail
else
    echo Test 10 pass
end
#CHECK: Test 10 pass

set -x -l -q foo
if test $status -eq 0
    echo Test 11 fail
else
    echo Test 11 pass
end
#CHECK: Test 11 pass

if set -x -g -q bar
    echo Test 12 fail
else
    echo Test 12 pass
end
#CHECK: Test 12 pass

if set -x -U -q baz
    echo Test 13 fail
else
    echo Test 13 pass
end
#CHECK: Test 13 pass

set -x -l foo 1
set -x -g bar 2
set -x -U baz 3

set -l -q foo
if test $status -ne 0
    echo Test 14 fail
else
    echo Test 14 pass
end
#CHECK: Test 14 pass

if not set -g -q bar
    echo Test 15 fail
else
    echo Test 15 pass
end
#CHECK: Test 15 pass

if not set -U -q baz
    echo Test 16 fail
else
    echo Test 16 pass

end
#CHECK: Test 16 pass

set -u -l -q foo
if test $status -ne 1
    echo Test 17 fail
else
    echo Test 17 pass
end
#CHECK: Test 17 pass

if set -u -g -q bar
    echo Test 18 fail
else
    echo Test 18 pass
end
#CHECK: Test 18 pass

if set -u -U -q baz
    echo Test 19 fail
else
    echo Test 19 pass

end
#CHECK: Test 19 pass

set -x -l -q foo
if test $status -ne 0
    echo Test 20 fail
else
    echo Test 20 pass
end
#CHECK: Test 20 pass

if not set -x -g -q bar
    echo Test 21 fail
else
    echo Test 21 pass
end
#CHECK: Test 21 pass

if not set -x -U -q baz
    echo Test 22 fail
else
    echo Test 22 pass
end
#CHECK: Test 22 pass

set -U -e baz

# Verify subcommand statuses
echo (false) $status (true) $status (false) $status
#CHECK: 1 0 1

# Verify that set passes through exit status, except when passed -n or -q or -e
false
set foo bar
echo 1 $status # passthrough
#CHECK: 1 1
true
set foo bar
echo 2 $status # passthrough
#CHECK: 2 0
false
set -q foo
echo 3 $status # no passthrough
#CHECK: 3 0
true
set -q foo
echo 4 $status # no passthrough
#CHECK: 4 0
false
set -n >/dev/null
echo 5 $status # no passthrough
#CHECK: 5 0
false
set -e foo
echo 6 $status # no passthrough
#CHECK: 6 0
true
set -e foo
echo 7 $status # no passthrough
#CHECK: 7 4
false
set -h >/dev/null
# CHECKERR: Documentation for set

echo 8 $status # no passthrough
#CHECK: 8 0
true
set -NOT_AN_OPTION 2>/dev/null
echo 9 $status # no passthrough
#CHECK: 9 2
false
set foo (echo A; true)
echo 10 $status $foo
#CHECK: 10 0 A
true
set foo (echo B; false)
echo 11 $status $foo
#CHECK: 11 1 B
true

function setql_check
    set -l setql_foo val
    if set -ql setql_foo
        echo Pass
    else
        echo Fail
    end
end
setql_check
#CHECK: Pass
