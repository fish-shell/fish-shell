#RUN: %fish %s
##########

set -g LANG C

# Start by verifying a bunch of error conditions.

# No args is an error
argparse
#CHECKERR: argparse: No option specs were provided

# Missing -- is an error
argparse h/help
#CHECKERR: argparse: Missing -- separator

# Flags but no option specs is an error
argparse -s -- hello
#CHECKERR: argparse: No option specs were provided

# Invalid option specs
argparse h-
argparse +help
argparse h/help:
argparse h-help::
argparse h-help=x
#CHECKERR: argparse: Invalid option spec 'h-' at char '-'
#CHECKERR: argparse: Short flag '+' invalid, must be alphanum or '#'
#CHECKERR: argparse: Invalid option spec 'h/help:' at char ':'
#CHECKERR: argparse: Invalid option spec 'h-help::' at char ':'
#CHECKERR: argparse: Invalid option spec 'h-help=x' at char 'x'

# --max-args and --min-args work
begin
    argparse --name min-max --min-args 1 h/help --
    #CHECKERR: min-max: Expected at least 1 args, got 0
    argparse --name min-max --min-args 1 --max-args 3 h/help -- arg1
    argparse --name min-max --min-args 1 --max-args 3 h/help -- arg1 arg2
    argparse --name min-max --min-args 1 --max-args 3 h/help -- --help arg1 arg2 arg3
    argparse --name min-max --min-args 1 --max-args 3 h/help -- arg1 arg2 -h arg3 arg4
    #CHECKERR: min-max: Expected at most 3 args, got 4
    argparse --name min-max --max-args 1 h/help --
    argparse --name min-max --max-args 1 h/help -- arg1
    argparse --name min-max --max-args 1 h/help -- arg1 arg2
    #CHECKERR: min-max: Expected at most 1 args, got 2
end

# Invalid \"#-val\" spec
begin
    argparse '#-val=' -- abc -x def
    # CHECKERR: argparse: Implicit int short flag '#' does not allow modifiers like '='
end

# Invalid arg in the face of a \"#-val\" spec
begin
    argparse '#-val' -- abc -x def
    # CHECKERR: argparse: Unknown option '-x'
    # CHECKERR: {{.*}}argparse.fish (line {{\d+}}):
    # CHECKERR: argparse '#-val' -- abc -x def
    # CHECKERR: {{\s*\^\s*}}
end

# Defining a short flag more than once
begin
    argparse s/short x/xray s/long -- -s -x --long
    # CHECKERR: argparse: Short flag 's' already defined
end

# Defining a long flag more than once
begin
    argparse s/short x/xray l/short -- -s -x --long
    # CHECKERR: argparse: Long flag 'short' already defined
end

# Defining an implicit int flag more than once
begin
    argparse '#-val' x/xray 'v#val' -- -s -x --long
    # CHECKERR: argparse: Implicit int flag '#' already defined
end

# Defining an implicit int flag with modifiers
begin
    argparse 'v#val=' --
    # CHECKERR: argparse: Implicit int short flag 'v' does not allow modifiers like '='
end

##########
# Now verify that validly formed invocations work as expected.

# No args
begin
    argparse h/help --
end

# One arg and no matching flags
begin
    argparse h/help -- help
    set -l
    # CHECK: argv help
end

# Five args with two matching a flag
begin
    argparse h/help -- help --help me -h 'a lot more'
    set -l
    # CHECK: _flag_h '--help'  '-h'
    # CHECK: _flag_help '--help'  '-h'
    # CHECK: argv 'help'  'me'  'a lot more'
end

# Required, optional, and multiple flags
begin
    argparse h/help 'a/abc=' 'd/def=?' 'g/ghk=+' -- help --help me --ghk=g1 --abc=ABC --ghk g2 --d -g g3
    set -l
    # CHECK: _flag_a ABC
    # CHECK: _flag_abc ABC
    # CHECK: _flag_d
    # CHECK: _flag_def
    # CHECK: _flag_g 'g1'  'g2'  'g3'
    # CHECK: _flag_ghk 'g1'  'g2'  'g3'
    # CHECK: _flag_h --help
    # CHECK: _flag_help --help
    # CHECK: argv 'help'  'me'
end

# --stop-nonopt works
begin
    argparse --stop-nonopt h/help 'a/abc=' -- -a A1 -h --abc A2 non-opt 'second non-opt' --help
    set -l
    # CHECK: _flag_a A2
    # CHECK: _flag_abc A2
    # CHECK: _flag_h -h
    # CHECK: _flag_help -h
    # CHECK: argv 'non-opt'  'second non-opt'  '--help'
end

# Implicit int flags work
begin
    argparse '#-val' -- abc -123 def
    set -l
    # CHECK: _flag_val 123
    # CHECK: argv 'abc'  'def'
end
begin
    argparse v/verbose '#-val' 't/token=' -- -123 a1 --token woohoo --234 -v a2 --verbose
    set -l
    # CHECK: _flag_t woohoo
    # CHECK: _flag_token woohoo
    # CHECK: _flag_v '-v'  '--verbose'
    # CHECK: _flag_val -234
    # CHECK: _flag_verbose '-v'  '--verbose'
    # CHECK: argv 'a1'  'a2'
end

# Should be set to 987
begin
    argparse 'm#max' -- argle -987 bargle
    set -l
    # CHECK: _flag_m 987
    # CHECK: _flag_max 987
    # CHECK: argv 'argle'  'bargle'
end

# Should be set to 765
begin
    argparse 'm#max' -- argle -987 bargle --max 765
    set -l
    # CHECK: _flag_m 765
    # CHECK: _flag_max 765
    # CHECK: argv 'argle'  'bargle'
end

# Bool short flag only
begin
    argparse C v -- -C -v arg1 -v arg2
    set -l
    # CHECK: _flag_C -C
    # CHECK: _flag_v '-v'  '-v'
    # CHECK: argv 'arg1'  'arg2'
end

# Value taking short flag only
begin
    argparse 'x=' v/verbose -- --verbose arg1 -v -x arg2
    set -l
    # CHECK: _flag_v '--verbose'  '-v'
    # CHECK: _flag_verbose '--verbose'  '-v'
    # CHECK: _flag_x arg2
    # CHECK: argv arg1
end

# Implicit int short flag only
begin
    argparse 'x#' v/verbose -- -v -v argle -v -x 321 bargle
    set -l
    # CHECK: _flag_v '-v'  '-v'  '-v'
    # CHECK: _flag_verbose '-v'  '-v'  '-v'
    # CHECK: _flag_x 321
    # CHECK: argv 'argle'  'bargle'
end

# Implicit int short flag only with custom validation passes
begin
    argparse 'x#!_validate_int --max 500' v/verbose -- -v -v -x 499 -v
    set -l
    # CHECK: _flag_v '-v'  '-v'  '-v'
    # CHECK: _flag_verbose '-v'  '-v'  '-v'
    # CHECK: _flag_x 499
    # CHECK: argv
end

# Implicit int short flag only with custom validation fails
begin
    argparse 'x#!_validate_int --min 500' v/verbose -- -v -v -x 499 -v
    set -l
    # CHECKERR: argparse: Value '499' for flag 'x' less than min allowed of '500'
end

##########
# Verify that flag value validation works.

# Implicit int flag validation fails
argparse 'm#max' -- argle --max 765x bargle
and echo unxpected argparse return status >&2
argparse 'm#max' -- argle -ma1 bargle
and echo unxpected argparse return status >&2
# CHECKERR: argparse: Value '765x' for flag 'max' is not an integer
# CHECKERR: argparse: Value 'a1' for flag 'm' is not an integer

# Check the exit status from argparse validation
argparse 'm#max!set | grep "^_flag_"; function x; return 57; end; x' -- argle --max=83 bargle 2>&1
set -l saved_status $status
test $saved_status -eq 57
and echo expected argparse return status $saved_status
# CHECK: _flag_name max
# CHECK: _flag_value 83
# CHECK: expected argparse return status 57

# Explicit int flag validation
# These should fail
argparse 'm#max!_validate_int --min 1 --max 1' -- argle -m2 bargle
and echo unexpected argparse return status $status >&2
argparse 'm#max!_validate_int --min 0 --max 1' -- argle --max=-1 bargle
and echo unexpected argparse return status $status >&2
# CHECKERR: argparse: Value '2' for flag 'm' greater than max allowed of '1'
# CHECKERR: argparse: Value '-1' for flag 'max' less than min allowed of '0'
# These should succeed
argparse 'm/max=!_validate_int --min 0 --max 1' -- argle --max=0 bargle
or echo unexpected argparse return status $status >&2
argparse 'm/max=!_validate_int --min 0 --max 1' -- argle --max=1 bargle
or echo unexpected argparse return status $status >&2

# Errors use function name by default
function notargparse
    argparse a/alpha -- --banana
end
notargparse
# CHECKERR: notargparse: Unknown option '--banana'
# CHECKERR: {{.*}}argparse.fish (line {{\d+}}):
# CHECKERR:     argparse a/alpha -- --banana
# CHECKERR:     ^
# CHECKERR: in function 'notargparse'
# CHECKERR: called on line {{\d+}} of file {{.*}}argparse.fish

true

# Ignoring unknown options
argparse -i a=+ b=+ -- -a alpha -b bravo -t tango -a aaaa --wurst
or echo unexpected argparse return status $status >&2
# The unknown options are removed _entirely_.
echo $argv
echo $_flag_a
# CHECK: -t tango --wurst
# CHECK: alpha aaaa

# Check for crash when last option is unknown
argparse -i b/break -- "-b kubectl get pods -l name=foo"

begin
    # Checking arguments after "--"
    argparse a/alpha -- a --alpha -- b -a
    printf '%s\n' $argv
    # CHECK: a
    # CHECK: b
    # CHECK: -a
end

# #5864 - short flag only with same validation function.
# Checking validation for short flags only
argparse 'i=!_validate_int' 'o=!_validate_int' -- -i 2 -o banana
argparse 'i=!_validate_int' 'o=!_validate_int' -- -i -o banana
# CHECKERR: argparse: Value 'banana' for flag 'o' is not an integer
# CHECKERR: argparse: Value '-o' for flag 'i' is not an integer
begin
    argparse 'i=!_validate_int' 'o=!_validate_int' -- -i 2 -o 3
    set -l
    # CHECK: _flag_a 'alpha'  'aaaa'
    # CHECK: _flag_b -b
    # CHECK: _flag_break -b
    # CHECK: _flag_i 2
    # CHECK: _flag_m 1
    # CHECK: _flag_max 1
    # CHECK: _flag_o 3
    # CHECK: argv
    # CHECK: saved_status 57
end

# #6483 - error messages for missing arguments
argparse -n foo q r/required= -- foo -qr
# CHECKERR: foo: Expected argument for option r

argparse r/required= -- foo --required
# CHECKERR: argparse: Expected argument for option --required

### The fish_opt wrapper:
# No args is an error
fish_opt
and echo unexpected status $status
#CHECKERR: fish_opt: The --short flag is required and must be a single character

# No short flag or an invalid short flag is an error
fish_opt -l help
and echo unexpected status $status
#CHECKERR: fish_opt: The --short flag is required and must be a single character
fish_opt -s help
and echo unexpected status $status
#CHECKERR: fish_opt: The --short flag is required and must be a single character

# A required and optional arg makes no sense
fish_opt -s h -l help -r --optional-val
and echo unexpected status $status
#CHECKERR: fish_opt: Mutually exclusive flags 'o/optional-val' and `r/required-val` seen

# A repeated and optional arg makes no sense
fish_opt -s h -l help --multiple-vals --optional-val
and echo unexpected status $status
#CHECKERR: fish_opt: Mutually exclusive flags 'multiple-vals' and `o/optional-val` seen

# An unexpected arg not associated with a flag is an error
fish_opt -s h -l help hello
and echo unexpected status $status
#CHECKERR: fish_opt: Expected at most 0 args, got 1

# Now verify that valid combinations of options produces the correct output.

# Bool, short only
fish_opt -s h
or echo unexpected status $status
#CHECK: h

# Bool, short and long
fish_opt --short h --long help
or echo unexpected status $status
#CHECK: h/help

# Bool, short and long but the short var cannot be used
fish_opt --short h --long help --long-only
#CHECK: h-help

# Required val, short and long but the short var cannot be used
fish_opt --short h --long help -r --long-only
or echo unexpected status $status
#CHECK: h-help=

# Optional val, short and long valid
fish_opt --short h -l help --optional-val
or echo unexpected status $status
#CHECK: h/help=?

# Optional val, short and long but the short var cannot be used
fish_opt --short h -l help --optional-val --long-only
or echo unexpected status $status
#CHECK: h-help=?

# Repeated val, short and long valid
fish_opt --short h -l help --multiple-vals
or echo unexpected status $status
#CHECK: h/help=+

# Repeated val, short and long but short not valid
fish_opt --short h -l help --multiple-vals --long-only
or echo unexpected status $status
#CHECK: h-help=+

# Repeated val, short only
fish_opt -s h --multiple-vals
or echo unexpected status $status
fish_opt -s h --multiple-vals --long-only
or echo unexpected status $status
#CHECK: h=+
#CHECK: h=+
