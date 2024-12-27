#RUN: %fish %s
# Test the `functions` builtin

function f1
end

# ==========
# Verify that `functions --details` works as expected when given too many args.
functions    --details f1 f2
#CHECKERR: functions: --details: expected 1 arguments; got 2

# Verify that it still mentions "--details" even if it isn't the last option.
functions    --details --verbose f1 f2
#CHECKERR: functions: --details: expected 1 arguments; got 2

# ==========
# Verify that `functions --details` works as expected when given the name of a
# known function.
functions --details f1
#CHECK: {{.*}}checks/functions.fish

# ==========
# Verify that `functions --details` works as expected when given the name of an
# unknown function.
functions -D f2
#CHECK: n/a

# ==========
# Verify that `functions --details` works as expected when given the name of a
# function that could be autoloaded but isn't currently loaded.
set x (functions -D vared)
if test (count $x) -ne 1
    or not string match -rq '.*/share(/fish)?/functions/vared\.fish' "$x"
    echo "Unexpected output for 'functions -D vared': $x" >&2
end

# ==========
# Verify that `functions --verbose --details` works as expected when given the name of a
# function that was autoloaded.
set x (functions -v -D vared)
if test (count $x) -ne 5
    or not string match -rq '.*/share(/fish)?/functions/vared\.fish' $x[1]
    or test $x[2] != autoloaded
    or test $x[3] != 6
    or test $x[4] != scope-shadowing
    or test $x[5] != 'Edit variable value'
    echo "Unexpected output for 'functions -v -D vared': $x" >&2
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
# Verify that `functions --details` works as expected when given the name of a
# function that is copied. (Prints the filename where it was copied.)
functions -c f1 f1a
functions -D f1a
#CHECK: {{.*}}checks/functions.fish
functions -Dv f1a
#CHECK: {{.*}}checks/functions.fish
#CHECK: {{.*}}checks/functions.fish
#CHECK: {{\d+}}
#CHECK: scope-shadowing
#CHECK:
echo "functions -c f1 f1b" | source
functions -D f1b
#CHECK: -
functions -Dv f1b
#CHECK: -
#CHECK: {{.*}}checks/functions.fish
#CHECK: {{\d+}}
#CHECK: scope-shadowing
#CHECK:

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

# ==========
# Verify that the functions are printed in order.
functions f1 test_func_desc
# CHECK: # Defined in {{.*}}
# CHECK: function f1
# CHECK: end
# CHECK: # Defined in {{.*}}
# CHECK: function test_func_desc --description description2
# CHECK: end

# Note: This test isn't ideal - if ls was loaded before,
# or doesn't exist, it'll succeed anyway.
#
# But we can't *confirm* that an ls function exists,
# so this is the best we can do.
functions --erase ls
type -t ls
#CHECK: file

# ==========
# Verify that `functions --query` does not return 0 if there are 256 missing functions
functions --query a(seq 1 256)
echo $status
#CHECK: 255

echo "function t; echo tttt; end" | source
functions t
# CHECK: # Defined via `source`
# CHECK: function t
# CHECK: echo tttt;
# CHECK: end

functions --no-details t
# CHECK: function t
# CHECK: echo tttt;
# CHECK: end

functions -c t t2
functions t2
# CHECK: # Defined via `source`, copied in {{.*}}checks/functions.fish @ line {{\d+}}
# CHECK: function t2
# CHECK: echo tttt;
# CHECK: end
functions -D t2
#CHECK: {{.*}}checks/functions.fish
functions -Dv t2
#CHECK: {{.*}}checks/functions.fish
#CHECK: -
#CHECK: {{\d+}}
#CHECK: scope-shadowing
#CHECK:

echo "functions -c t t3" | source
functions t3
# CHECK: # Defined via `source`, copied via `source`
# CHECK: function t3
# CHECK: echo tttt;
# CHECK: end
functions -D t3
#CHECK: -
functions -Dv t3
#CHECK: -
#CHECK: -
#CHECK: {{\d+}}
#CHECK: scope-shadowing
#CHECK:

functions --no-details t2
# CHECK: function t2
# CHECK: echo tttt;
# CHECK: end

functions --no-details --details t
# CHECKERR: functions: invalid option combination
# CHECKERR:
# CHECKERR: {{.*}}checks/functions.fish (line {{\d+}}):
# CHECKERR: functions --no-details --details t
# CHECKERR: ^
# CHECKERR: (Type 'help functions' for related documentation)
# XXX FIXME ^ caret should point at --no-details --details

function term1 --on-signal TERM
end
function term2 --on-signal TERM
end
function term3 --on-signal TERM
end

functions --handlers-type signal
# CHECK: Event signal
# CHECK: SIGTRAP fish_sigtrap_handler
# CHECK: SIGTERM term1
# CHECK: SIGTERM term2
# CHECK: SIGTERM term3

# See how --names and --all work.
# We don't want to list all of our functions here,
# so we just match a few that we know are there.
functions -n | string match cd
# CHECK: cd

functions --names | string match __fish_config_interactive
echo $status
# CHECK: 1

functions --names -a | string match __fish_config_interactive
# CHECK: __fish_config_interactive

functions --description ""
# CHECKERR: functions: Expected exactly one function name
# CHECKERR: {{.*}}checks/functions.fish (line {{\d+}}):
# CHECKERR: functions --description ""
# CHECKERR: ^
# CHECKERR: (Type 'help functions' for related documentation)

function foo --on-variable foo; end
# This should print *everything*
functions --handlers-type "" | string match 'Event *'
# CHECK: Event signal
# CHECK: Event variable
# CHECK: Event generic
functions -e foo

functions --details --verbose thisfunctiondoesnotexist
# CHECK: n/a
# CHECK: n/a
# CHECK: 0
# CHECK: n/a
# CHECK: n/a

functions --banana
# CHECKERR: functions: --banana: unknown option
echo $status
# CHECK: 2
