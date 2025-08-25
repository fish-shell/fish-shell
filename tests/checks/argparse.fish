#RUN: %fish %s
##########

# NOTE: This uses argparse, which touches the local variables.
# Any call that isn't an error should be enclosed in a begin/end block!

# Start by verifying a bunch of error conditions.
# These are *argparse* errors, and therefore bugs in the script,
# so they print a stack trace.

# No args (not even --) is an error
argparse
#CHECKERR: argparse: Missing -- separator
#CHECKERR: {{.*}}checks/argparse.fish (line {{\d+}}):
#CHECKERR: argparse
#CHECKERR: ^
#CHECKERR: (Type 'help argparse' for related documentation)

# Missing -- is an error
argparse h/help
#CHECKERR: argparse: Missing -- separator
#CHECKERR: {{.*}}checks/argparse.fish (line {{\d+}}):
#CHECKERR: argparse h/help
#CHECKERR: ^
#CHECKERR: (Type 'help argparse' for related documentation)

# Flags but no option specs is not an error
begin
    argparse -s -- hello
    echo $status
    # CHECK: 0
    set -l
    # CHECK: argv hello
    # CHECK: argv_opts
end

# Invalid option specs
argparse h-
argparse /
argparse +help
argparse h/help:
argparse h-help::
argparse h-help=x
#CHECKERR: argparse: Invalid option spec 'h-' at char '-'
#CHECKERR: {{.*}}checks/argparse.fish (line {{\d+}}):
#CHECKERR: argparse h-
#CHECKERR: ^
#CHECKERR: (Type 'help argparse' for related documentation)
#CHECKERR: argparse: Short flag '/' invalid, must be alphanum or '#'
#CHECKERR: {{.*}}checks/argparse.fish (line {{\d+}}):
#CHECKERR: argparse /
#CHECKERR: ^
#CHECKERR: (Type 'help argparse' for related documentation)
#CHECKERR: argparse: Short flag '+' invalid, must be alphanum or '#'
#CHECKERR: {{.*}}checks/argparse.fish (line {{\d+}}):
#CHECKERR: argparse +help
#CHECKERR: ^
#CHECKERR: (Type 'help argparse' for related documentation)
#CHECKERR: argparse: Invalid option spec 'h/help:' at char ':'
#CHECKERR: {{.*}}checks/argparse.fish (line {{\d+}}):
#CHECKERR: argparse h/help:
#CHECKERR: ^
#CHECKERR: (Type 'help argparse' for related documentation)
#CHECKERR: argparse: Invalid option spec 'h-help::' at char ':'
#CHECKERR: {{.*}}checks/argparse.fish (line {{\d+}}):
#CHECKERR: argparse h-help::
#CHECKERR: ^
#CHECKERR: (Type 'help argparse' for related documentation)
#CHECKERR: argparse: Invalid option spec 'h-help=x' at char 'x'
#CHECKERR: {{.*}}checks/argparse.fish (line {{\d+}}):
#CHECKERR: argparse h-help=x
#CHECKERR: ^
#CHECKERR: (Type 'help argparse' for related documentation)

# --max-args and --min-args work
begin
    argparse --name min-max --min-args 1 h/help --
    #CHECKERR: min-max: expected >= 1 arguments; got 0
    argparse --name min-max --min-args 1 --
    #CHECKERR: min-max: expected >= 1 arguments; got 0
    argparse --name min-max --min-args 1 --max-args 3 h/help -- arg1
    argparse --name min-max --min-args 1 --max-args 3 -- arg1
    argparse --name min-max --min-args 1 --max-args 3 h/help -- arg1 arg2
    argparse --name min-max --min-args 1 --max-args 3 h/help -- --help arg1 arg2 arg3
    argparse --name min-max --min-args 1 --max-args 3 h/help -- arg1 arg2 -h arg3 arg4
    #CHECKERR: min-max: expected <= 3 arguments; got 4
    argparse --name min-max --max-args 1 h/help --
    argparse --name min-max --max-args 1 h/help -- arg1
    argparse --name min-max --max-args 1 h/help -- arg1 arg2
    #CHECKERR: min-max: expected <= 1 arguments; got 2
    argparse --name min-max --max-args 1 -- arg1 arg2
    #CHECKERR: min-max: expected <= 1 arguments; got 2
end

# Invalid \"#-val\" spec
begin
    argparse '#-val=' -- abc -x def
    # CHECKERR: argparse: Implicit int short flag '#' does not allow modifiers like '='
    #CHECKERR: {{.*}}checks/argparse.fish (line {{\d+}}):
    #CHECKERR: argparse '#-val=' -- abc -x def
    #CHECKERR: ^
    #CHECKERR: (Type 'help argparse' for related documentation)
end

# Invalid arg in the face of a \"#-val\" spec
begin
    argparse '#-val' -- abc -x def
    # CHECKERR: argparse: -x: unknown option
end

# Defining a short flag more than once
begin
    argparse s/short x/xray s/long -- -s -x --long
    # CHECKERR: argparse: Short flag 's' already defined
    #CHECKERR: {{.*}}checks/argparse.fish (line {{\d+}}):
    #CHECKERR: argparse s/short x/xray s/long -- -s -x --long
    #CHECKERR: ^
    #CHECKERR: (Type 'help argparse' for related documentation)
end

# Defining a long flag more than once
begin
    argparse s/short x/xray l/short -- -s -x --long
    # CHECKERR: argparse: Long flag 'short' already defined
    #CHECKERR: {{.*}}checks/argparse.fish (line {{\d+}}):
    #CHECKERR: argparse s/short x/xray l/short -- -s -x --long
    #CHECKERR: ^
    #CHECKERR: (Type 'help argparse' for related documentation)
end

# Defining an implicit int flag more than once
begin
    argparse '#-val' x/xray 'v#val' -- -s -x --long
    # CHECKERR: argparse: Implicit int flag '#' already defined
    #CHECKERR: {{.*}}checks/argparse.fish (line {{\d+}}):
    #CHECKERR: argparse '#-val' x/xray 'v#val' -- -s -x --long
    #CHECKERR: ^
    #CHECKERR: (Type 'help argparse' for related documentation)
end

# Defining an implicit int flag with modifiers
begin
    argparse 'v#val=' --
    # CHECKERR: argparse: Implicit int short flag 'v' does not allow modifiers like '='
    #CHECKERR: {{.*}}checks/argparse.fish (line {{\d+}}):
    #CHECKERR: argparse 'v#val=' --
    #CHECKERR: ^
    #CHECKERR: (Type 'help argparse' for related documentation)
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
    # CHECK: argv_opts
end

# Five args with two matching a flag
begin
    argparse h/help -- help --help me -h 'a lot more'
    set -l
    # CHECK: _flag_h '--help'  '-h'
    # CHECK: _flag_help '--help'  '-h'
    # CHECK: argv 'help'  'me'  'a lot more'
    # CHECK: argv_opts '--help'  '-h'
end

# Required, optional, and multiple flags
begin
    argparse h/help 'a/abc=' 'd/def=?' 'g/ghk=+' -- help --help me --ghk=g1 --abc=ABC --ghk g2 -d -g g3
    set -lL
    # CHECK: _flag_a ABC
    # CHECK: _flag_abc ABC
    # CHECK: _flag_d
    # CHECK: _flag_def
    # CHECK: _flag_g 'g1'  'g2'  'g3'
    # CHECK: _flag_ghk 'g1'  'g2'  'g3'
    # CHECK: _flag_h --help
    # CHECK: _flag_help --help
    # CHECK: argv 'help'  'me'
    # CHECK: argv_opts '--help'  '--ghk=g1'  '--abc=ABC'  '--ghk'  'g2'  '-d'  '-g'  'g3'
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
    # CHECK: argv_opts '-a'  'A1'  '-h'  '--abc'  'A2'
end

# Implicit int flags work
begin
    argparse '#-val' -- abc -123 def
    set -l
    # CHECK: _flag_val 123
    # CHECK: argv 'abc'  'def'
    # CHECK: argv_opts -123
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
    # CHECK: argv_opts '-123'  '--token'  'woohoo'  '--234'  '-v'  '--verbose'
end

# Should be set to 987
begin
    argparse 'm#max' -- argle -987 bargle
    set -l
    # CHECK: _flag_m 987
    # CHECK: _flag_max 987
    # CHECK: argv 'argle'  'bargle'
    # CHECK: argv_opts -987
end

# Should be set to 765
begin
    argparse 'm#max' -- argle -987 bargle --max 765
    set -l
    # CHECK: _flag_m 765
    # CHECK: _flag_max 765
    # CHECK: argv 'argle'  'bargle'
    # CHECK: argv_opts '-987'  '--max'  '765'
end

# Bool short flag only
begin
    argparse C v -- -C -v arg1 -v arg2
    set -l
    # CHECK: _flag_C -C
    # CHECK: _flag_v '-v'  '-v'
    # CHECK: argv 'arg1'  'arg2'
    # CHECK: argv_opts '-C'  '-v'  '-v'
end

# Value taking short flag only
begin
    argparse 'x=' v/verbose -- --verbose arg1 -v -x arg2
    set -l
    # CHECK: _flag_v '--verbose'  '-v'
    # CHECK: _flag_verbose '--verbose'  '-v'
    # CHECK: _flag_x arg2
    # CHECK: argv arg1
    # CHECK: argv_opts '--verbose'  '-v'  '-x'  'arg2'
end

# Implicit int short flag only
begin
    argparse 'x#' v/verbose -- -v -v argle -v -x 321 bargle
    set -l
    # CHECK: _flag_v '-v'  '-v'  '-v'
    # CHECK: _flag_verbose '-v'  '-v'  '-v'
    # CHECK: _flag_x 321
    # CHECK: argv 'argle'  'bargle'
    # CHECK: argv_opts '-v'  '-v'  '-v'  '-x'  '321'
end

# Implicit int short flag only with custom validation passes
begin
    argparse 'x#!_validate_int --max 500' v/verbose -- -v -v -x 499 -v
    set -l
    # CHECK: _flag_v '-v'  '-v'  '-v'
    # CHECK: _flag_verbose '-v'  '-v'  '-v'
    # CHECK: _flag_x 499
    # CHECK: argv
    # CHECK: argv_opts '-v'  '-v'  '-x'  '499'  '-v'
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
and echo unexpected argparse return status >&2
argparse 'm#max' -- argle -ma1 bargle
and echo unexpected argparse return status >&2
# CHECKERR: argparse: Value '765x' for flag 'max' is not an integer
# CHECKERR: argparse: Value 'a1' for flag 'm' is not an integer

begin
# Check the exit status from argparse validation
    argparse 'm#max!set -l | grep "^_flag_"; function x; return 57; end; x' -- argle --max=83 bargle 2>&1
    set -l saved_status $status
    test $saved_status -eq 57
    and echo expected argparse return status $saved_status
    # CHECK: _flag_name max
    # CHECK: _flag_value 83
    # CHECK: expected argparse return status 57
end

begin
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
end

# Errors use function name by default
function notargparse
    argparse a/alpha -- --banana
end
notargparse
# CHECKERR: notargparse: --banana: unknown option

true

begin
    # Ignoring unknown options
    argparse -i a=+ b=+ -- -a alpha -b bravo -t tango -a aaaa --wurst
    or echo unexpected argparse return status $status >&2
    # The unknown options are removed _entirely_.
    echo $argv
    echo $argv_opts
    echo $_flag_a
    # CHECK: -t tango --wurst
    # CHECK: -a alpha -b bravo -a aaaa
    # CHECK: alpha aaaa
end

begin
    # Ignoring unknown long options that share a character with a known short options
    argparse -i o -- --long=value --long
    or echo unexpected argparse return status $status >&2
    set -l
    # CHECK: argv '--long=value'  '--long'
    # CHECK: argv_opts
end

begin
    # Check for crash when last option is unknown
    # Note that because of the quotation marks, there is a single argument
    # starting with the known short option '-b', and continuing with an unknown short option
    # that starts with a space
    argparse -i b/break -- "-b kubectl get pods -l name=foo"
    set -l
    # CHECK: _flag_b -b
    # CHECK: _flag_break -b
    # CHECK: argv '- kubectl get pods -l name=foo'
    # CHECK: argv_opts -b
end

begin
    # Ignore unknown and move unknown are mutually exclusive
    argparse -i --move-unknown --
    #CHECKERR: argparse: --ignore-unknown --move-unknown: options cannot be used together
    #CHECKERR: {{.*}}checks/argparse.fish (line {{\d+}}):
    #CHECKERR: argparse -i --move-unknown --
    #CHECKERR: ^
    #CHECKERR: (Type 'help argparse' for related documentation)
end

begin
    # Moving unknown options
    argparse --move-unknown --unknown-arguments=optional h i -- -hoa -oia
    echo -- $argv
    #CHECK:
    echo -- $argv_opts
    #CHECK: -hoa -oia
    echo $_flag_h
    #CHECK: -h
    set -q _flag_i
    or echo No flag I
    #CHECK: No flag I
end

begin
    # Moving unknown options
    argparse -u a=+ b=+ -- -a alpha -b bravo -t tango -a aaaa --wurst
    or echo unexpected argparse return status $status >&2
    # The unknown options are removed _entirely_.
    echo $argv
    echo $argv_opts
    echo $_flag_a
    # CHECK: tango
    # CHECK: -a alpha -b bravo -t -a aaaa --wurst
    # CHECK: alpha aaaa
end

begin
    # Move unknown long options that share a character with a known short options
    argparse -u o -- --long=value --long
    or echo unexpected argparse return status $status >&2
    set -l
    # CHECK: argv
    # CHECK: argv_opts '--long=value'  '--long'
end


begin
    argparse -u b/break -- "-b kubectl get pods -l name=foo"
    set -l
    # CHECK: _flag_b -b
    # CHECK: _flag_break -b
    # CHECK: argv
    # CHECK: argv_opts '-b kubectl get pods -l name=foo'
end

begin
    # Checking unknown-arguments, with errors
    argparse -i -U none -- --long=value
    # CHECKERR: argparse: --long=value: option does not take an argument
    argparse -i -U required -- -u
    # CHECKERR: argparse: -u: option requires an argument
    argparse -i -U required -- --long
    # CHECKERR: argparse: --long: option requires an argument
end

begin
    argparse -U none -i b= -- -abv=val in --long between -u
    set -l
    # CHECK: _flag_b v=val
    # CHECK: argv '-a'  'in'  '--long'  'between'  '-u'
    # CHECK: argv_opts -bv=val
end

begin
    argparse -U none b= -- -abv in --long between -u
    set -l
    # CHECK: _flag_b v
    # CHECK: argv 'in'  'between'
    # CHECK: argv_opts '-abv'  '--long'  '-u'
end

begin
    argparse -iU required b= -- -abv -b -b --long -b -u -b
    set -l
    # CHECK: _flag_b -b
    # CHECK: argv '-abv'  '--long'  '-b'  '-u'  '-b'
    # CHECK: argv_opts '-b'  '-b'
end

begin
    argparse -uU required b= -- -abv -b -b --long -b -u -b
    set -l
    # CHECK: _flag_b -b
    # CHECK: argv
    # CHECK: argv_opts '-abv'  '-b'  '-b'  '--long'  '-b'  '-u'  '-b'
end

begin
    # Checking arguments after "--"
    argparse a/alpha -- a --alpha -- b -a
    printf '%s\n' $argv
    # CHECK: a
    # CHECK: b
    printf '%s\n' $argv_opts
    # CHECK: -a
    # CHECK: --alpha
end

begin
    # #5864 - short flag only with same validation function.
    # Checking validation for short flags only
    argparse 'i=!_validate_int' 'o=!_validate_int' -- -i 2 -o banana
    argparse 'i=!_validate_int' 'o=!_validate_int' -- -i -o banana
    # CHECKERR: argparse: Value 'banana' for flag 'o' is not an integer
    # CHECKERR: argparse: Value '-o' for flag 'i' is not an integer
    argparse 'i=!_validate_int' 'o=!_validate_int' -- -i 2 -o 3
    set -l
    # CHECK: _flag_i 2
    # CHECK: _flag_o 3
    # CHECK: argv
    # CHECK: argv_opts '-i'  '2'  '-o'  '3'
end

# long-only flags
begin
    argparse installed= foo -- --installed=no --foo
    set -l
    # CHECK: _flag_foo --foo
    # CHECK: _flag_installed no
    # CHECK: argv
    # CHECK: argv_opts '--installed=no'  '--foo'
end

# long-only flags with one letter
begin
    argparse i-i= /f -- --i=no --f
    set -l
    # CHECK: _flag_f --f
    # CHECK: _flag_i no
    # CHECK: argv
    # CHECK: argv_opts '--i=no'  '--f'
end

begin
    argparse installed='!_validate_int --max 12' foo -- --installed=5 --foo
    set -l
    # CHECK: _flag_foo --foo
    # CHECK: _flag_installed 5
    # CHECK: argv
    # CHECK: argv_opts '--installed=5'  '--foo'
end

begin
    argparse '#num' installed= -- --installed=5 -5
    set -l
    # CHECK: _flag_installed 5
    # CHECK: _flag_num 5
    # CHECK: argv
    # CHECK: argv_opts '--installed=5'  '-5'
end

begin
    argparse installed='!_validate_int --max 12' foo -- --foo --installed=error --foo
    # CHECKERR: argparse: Value 'error' for flag 'installed' is not an integer
end

# #6483 - error messages for missing arguments
argparse -n foo q r/required= -- foo -qr
# CHECKERR: foo: -r: option requires an argument

argparse r/required= -- foo --required
# CHECKERR: argparse: --required: option requires an argument

### The fish_opt wrapper:
# No args is an error
fish_opt
and echo unexpected status $status
#CHECKERR: fish_opt: Either the --short or --long flag must be provided

# Long-only with no long makes no sense
fish_opt -s h --long-only
and echo unexpected status $status
#CHECKERR: fish_opt: The --long-only flag requires the --long flag

fish_opt -s help
and echo unexpected status $status
#CHECKERR: fish_opt: The --short flag must be a single character

# A required and optional arg makes no sense
fish_opt -s h -l help -r --optional-val
and echo unexpected status $status
#CHECKERR: fish_opt: o/optional-val r/required-val: options cannot be used together
# XXX FIXME the error should output -r and --optional-val: what the user used

# An unexpected arg not associated with a flag is an error
fish_opt -s h -l help hello
and echo unexpected status $status
#CHECKERR: fish_opt: Extra non-option arguments were provided

# A -v / --validate without any arguments is an error
fish_opt -s h -l help -rv
and echo unexpected status $status
#CHECKERR: fish_opt: The --validate flag requires subsequent arguments

# A -v / --validate for boolean options is an error
fish_opt -s h -l help -v echo hello
and echo unexpected status $status
#CHECKERR: fish_opt: The --validate flag requires the --required-val, --optional-value, or --multiple-vals flag

# Now verify that valid combinations of options produces the correct output.

# Bool, short only
fish_opt -s h
or echo unexpected status $status
#CHECK: h

# Long flag only
fish_opt -l help
or echo unexpected status $status
#CHECK: /help

# One character long flag
fish_opt -l h
or echo unexpected status $status
#CHECK: /h

# Bool, short and long
fish_opt --short h --long help
or echo unexpected status $status
#CHECK: h/help

# Bool, short and long but the short var cannot be used
fish_opt --short h --long help --long-only
#CHECK: h-help

# Required val and long
fish_opt --long help -r
or echo unexpected status $status
#CHECK: /help=

# Optional val, short and long valid, and delete
fish_opt --short h -l help --optional-val --delete
or echo unexpected status $status
#CHECK: h/help=?&

# Optional val, short and long but the short var cannot be used
fish_opt --short h -l help --optional-val --long-only
or echo unexpected status $status
#CHECK: h-help=?

# Repeated val, short and long valid, with validate
fish_opt --short h -l help --multiple-vals --validate _validate_int --max 500
or echo unexpected status $status
#CHECK: h/help=+!_validate_int --max 500

# Repeated and optional val, short and long valid
fish_opt --short h -l help --optional-val -m
or echo unexpected status $status
#CHECK: h/help=*

# Repeated val and short, with validate
fish_opt -ml help --long-only -v test \$_flag_value != "' '"
or echo unexpected status $status
#CHECK: /help=+!test $_flag_value != ' '

# Repeated and optional val, short and long but short not valid
fish_opt --short h -l help --long-only -mo
or echo unexpected status $status
#CHECK: h-help=*

# Repeated val, short only, and deleted
fish_opt -ms h -md
or echo unexpected status $status
#CHECK: h=+&

# Repeated and optional val, short only
fish_opt -s h -om
or echo unexpected status $status
#CHECK: h=*

function wrongargparse
    argparse -foo -- banana
    argparse a-b
    argparse
end

begin
    argparse ''
    #CHECKERR: argparse: An option spec must have at least a short or a long flag
    #CHECKERR: {{.*}}checks/argparse.fish (line {{\d+}}):
    #CHECKERR: argparse ''
    #CHECKERR: ^
    #CHECKERR: (Type 'help argparse' for related documentation)
end

begin
    argparse -U
    #CHECKERR: argparse: -U: option requires an argument
    #CHECKERR: {{.*}}checks/argparse.fish (line {{\d+}}):
    #CHECKERR: argparse -U
    #CHECKERR: ^
    #CHECKERR: (Type 'help argparse' for related documentation)
end

begin
    argparse --unknown-arguments what --
    #CHECKERR: argparse: Invalid --unknown-arguments value 'what'
    #CHECKERR: {{.*}}checks/argparse.fish (line {{\d+}}):
    #CHECKERR: argparse --unknown-arguments what --
    #CHECKERR: ^
    #CHECKERR: (Type 'help argparse' for related documentation)
end

begin
    argparse f!echo hello -- -f
    #CHECKERR: argparse: Invalid option spec 'f!echo' at char '!'
end

begin
    argparse --ignore-unknown h i -- -hoa -oia
    echo -- $argv
    #CHECK: -oa -oia
    echo -- $argv_opts
    #CHECK: -h
    echo $_flag_h
    #CHECK: -h
    set -q _flag_i
    or echo No flag I
    #CHECK: No flag I
end

# Tests for --delete
begin
    argparse 'a/long&' -- before -a inbetween --long after
    set -l
    # CHECK: _flag_a '-a'  '--long'
    # CHECK: _flag_long '-a'  '--long'
    # CHECK: argv 'before'  'inbetween'  'after'
    # CHECK: argv_opts
end

begin
    argparse 'a/long=+&' '#int&' -- -123 before -a -a inbetween -astuck ater -long=long
    set -l
    # CHECK: _flag_a '-a'  'stuck'  'long'
    # CHECK: _flag_int 123
    # CHECK: _flag_long '-a'  'stuck'  'long'
    # CHECK: argv 'before'  'inbetween'  'ater'
    # CHECK: argv_opts
end


begin
    argparse 'd=?&' a b  -- -d -d3 -ad -bd345
    set -l
    # CHECK: _flag_a -a
    # CHECK: _flag_b -b
    # CHECK: _flag_d 345
    # CHECK: argv
    # CHECK: argv_opts '-a'  '-b'
end

begin
    argparse 'd&' a b 'v='  -- 0 -adbv124 1 -abdv125 2 -dabv124 3 -vd3
    set -l
    # CHECK: _flag_a '-a'  '-a'  '-a'
    # CHECK: _flag_b '-b'  '-b'  '-b'
    # CHECK: _flag_d '-d'  '-d'  '-d'
    # CHECK: _flag_v d3
    # CHECK: argv '0'  '1'  '2'  '3'
    # CHECK: argv_opts '-abv124'  '-abv125'  '-abv124'  '-vd3'
end

argparse a/alpha -- --banna=value
# CHECKERR: argparse: --banna=value: unknown option
# But this gives a better message
argparse a/alpha -- --alpha=value --banna=value
# CHECKERR: argparse: --alpha=value: option does not take an argument

# Check behaviour without -S/--strict-longopts option
begin
    argparse long valu=+ -- --lon -long -lon -valu=3 -valu 4 -val=3 -val 4
    set -lL
    # CHECK: _flag_long '--long'  '--long'  '--long'
    # CHECK: _flag_valu '3'  '4'  '3'  '4'
    # CHECK: argv
    # CHECK: argv_opts '--lon'  '-long'  '-lon'  '-valu=3'  '-valu'  '4'  '-val=3'  '-val'  '4'
    argparse amb ambig -- -am
    # CHECKERR: argparse: -am: unknown option
    argparse a ambig -- -ambig
    # CHECKERR: argparse: -ambig: unknown option
    argparse long -- -long3
    # CHECKERR: argparse: -long3: unknown option
end

# Check behaviour with -S/--strict-longopts option
begin
    argparse -S long -- --lon
    # CHECKERR: argparse: --lon: unknown option
    argparse -S long -- -long
    # CHECKERR: argparse: -long: unknown option
    argparse --strict-longopts long -- -lon
    # CHECKERR: argparse: -lon: unknown option
    argparse -S value=+ -- -valu=3
    # CHECKERR: argparse: -valu=3: unknown option
    argparse --strict-longopts value=+ -- -valu 4
    # CHECKERR: argparse: -valu: unknown option
end

# Check =* options
begin
    set -l _flag_o previous value
    argparse 'o/opt=*' -- -o non-value -oval --opt arg --opt=456
    set -l
    # CHECK: _flag_o ''  'val'  ''  '456'
    # CHECK: _flag_opt ''  'val'  ''  '456'
    # CHECK: argv 'non-value'  'arg'
    # CHECK: argv_opts '-o'  '-oval'  '--opt'  '--opt=456'
end

# Check that the argparse's are properly wrapped in begin blocks
set -l
