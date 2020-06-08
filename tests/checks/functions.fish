#RUN: %fish %s
# Test the `functions` builtin

function f1
end

# ==========
# Verify that `functions --details` works as expected when given too many args.
set x (functions --details f1 f2 2>&1)
if test "$x" != "functions --details: Expected 1 args, got 2"
    echo "Unexpected output for 'functions --details f1 f2': $x" >&2
end

# ==========
# Verify that `functions --details` works as expected when given the name of a
# known function.
functions --details f1
#CHECK: {{.*}}checks/functions.fish

# ==========
# Verify that `functions --details` works as expected when given the name of an
# unknown function.
set x (functions -D f2)
if test "$x" != n/a
    echo "Unexpected output for 'functions --details f2': $x" >&2
end

# ==========
# Verify that `functions --details` works as expected when given the name of a
# function that could be autoloaded but isn't currently loaded.
set x (functions -D abbr)
if test (count $x) -ne 1
    or not string match -q '*/share/functions/abbr.fish' "$x"
    echo "Unexpected output for 'functions -D abbr': $x" >&2
end

# ==========
# Verify that `functions --verbose --details` works as expected when given the name of a
# function that was autoloaded.
set x (functions -v -D abbr)
if test (count $x) -ne 5
    or not string match -q '*/share/functions/abbr.fish' $x[1]
    or test $x[2] != autoloaded
    or test $x[3] != 1
    or test $x[4] != scope-shadowing
    or test $x[5] != 'Manage abbreviations'
    echo "Unexpected output for 'functions -v -D abbr': $x" >&2
end

# ==========
# Verify that `functions --verbose --details` properly escapes a function
# with a multiline description.
function multiline_descr -d 'line 1\n
line 2 & more; way more'
end
set x (functions -v -D multiline_descr)
if test $x[5] != 'line 1\\\\n\\nline 2 & more; way more'
    echo "Unexpected output for 'functions -v -D multiline_descr': $x" >&2
end

# ==========
# Verify function description setting
function test_func_desc
end
functions test_func_desc | string match --quiet '*description*'
and echo "Unexpected description" >&2

functions --description description1 test_func_desc
functions test_func_desc | string match --quiet '*description1*'
or echo "Failed to find description 1" >&2

functions -d description2 test_func_desc
functions test_func_desc | string match --quiet '*description2*'
or echo "Failed to find description 2" >&2

# Note: This test isn't ideal - if ls was loaded before,
# or doesn't exist, it'll succeed anyway.
#
# But we can't *confirm* that an ls function exists,
# so this is the best we can do.
functions --erase ls
type -t ls
#CHECK: file
