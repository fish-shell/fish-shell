#RUN: %fish %s

# Universal abbreviations are imported.
set -U _fish_abbr_cuckoo somevalue
set fish (status fish-path)
$fish -c abbr
# CHECK: abbr -a -- cuckoo somevalue # imported from a universal variable, see `help cmds/abbr`

# Test basic add and list of __abbr1
abbr __abbr1 alpha beta gamma
abbr | grep __abbr1
# CHECK: abbr -a -- __abbr1 'alpha beta gamma'

# Erasing one that doesn\'t exist should do nothing
abbr --erase NOT_AN_ABBR
abbr | grep __abbr1
# CHECK: abbr -a -- __abbr1 'alpha beta gamma'

# Adding existing __abbr1 should be idempotent
abbr __abbr1 alpha beta gamma
abbr | grep __abbr1
# CHECK: abbr -a -- __abbr1 'alpha beta gamma'

# Replacing __abbr1 definition
abbr __abbr1 delta
abbr | grep __abbr1
# CHECK: abbr -a -- __abbr1 delta

# __abbr1 -s and --show tests
abbr -s | grep __abbr1
abbr --show | grep __abbr1
# CHECK: abbr -a -- __abbr1 delta
# CHECK: abbr -a -- __abbr1 delta

# Test erasing __abbr1
abbr -e __abbr1
abbr | grep __abbr1

# Ensure we escape special characters on output
abbr '~__abbr2' '$xyz'
abbr | grep __abbr2
# CHECK: abbr -a -- '~__abbr2' '$xyz'
abbr -e '~__abbr2'

# Ensure we handle leading dashes in abbreviation names properly
abbr -- --__abbr3 xyz
abbr | grep __abbr3
# CHECK: abbr -a -- --__abbr3 xyz
abbr -e -- --__abbr3

# Test that an abbr word containing spaces is rejected
abbr "a b c" "d e f"
abbr | grep 'a b c'
# CHECKERR: abbr --add: Abbreviation 'a b c' cannot have spaces in the word

# Test renaming
abbr __abbr4 omega
abbr | grep __abbr5
abbr --rename __abbr4 __abbr5
abbr | grep __abbr5
# CHECK: abbr -a -- __abbr5 omega
abbr -e __abbr5
abbr | grep __abbr4

# Test renaming a nonexistent abbreviation
abbr --rename __abbr6 __abbr
# CHECKERR: abbr --rename: No abbreviation named __abbr6 with the specified command restrictions

# Test renaming to a abbreviation with spaces
abbr __abbr4 omega
abbr --rename __abbr4 "g h i"
# CHECKERR: abbr --rename: Abbreviation 'g h i' cannot have spaces in the word
abbr -e __abbr4

# Test renaming without arguments
abbr __abbr7 omega
abbr --rename __abbr7
# CHECKERR: abbr --rename: Requires exactly two arguments

# Test renaming with too many arguments
abbr __abbr8 omega
abbr --rename __abbr8 __abbr9 __abbr10
# CHECKERR: abbr --rename: Requires exactly two arguments
abbr | grep __abbr8
abbr | grep __abbr9
abbr | grep __abbr10
# CHECK: abbr -a -- __abbr8 omega

# Test renaming to existing abbreviation
abbr __abbr11 omega11
abbr __abbr12 omega12
abbr --rename __abbr11 __abbr12
# CHECKERR: abbr --rename: Abbreviation __abbr12 already exists, cannot rename __abbr11

abbr __abbr-with-dashes omega
complete -C__abbr-with
# CHECK: __abbr-with-dashes	Abbreviation: omega

abbr --add --global __abbr13 aaaaaaaaaaaaa
abbr --add --global __abbr14 bbbbbbbbbbbbb
abbr --erase __abbr13 __abbr14
abbr | grep __abbr13
abbr | grep __abbr14

abbr -q banana
echo $status
# CHECK: 1

abbr -q __abbr8 banana
echo $status
# CHECK: 0

abbr -q banana __abbr8 foobar
echo $status
# CHECK: 0

abbr --add grape --position nowhere juice
echo $status
# CHECKERR: abbr: Invalid position 'nowhere'
# CHECKERR: Position must be one of: command, anywhere
# CHECK: 2

abbr --add grape --position anywhere juice
echo $status
# CHECK: 0

abbr --add grape --position command juice
echo $status
# CHECK: 0

abbr --query banana --position anywhere
echo $status
# CHECKERR: abbr: --position option requires --add
# CHECK: 2

abbr --query banana --function foo
echo $status
# CHECKERR: abbr: --function option requires --add
# CHECK: 2

abbr --query banana --function
echo $status
# CHECKERR: abbr: --function: option requires an argument
# CHECKERR: {{.*}}checks/abbr.fish (line 141):
# CHECKERR: abbr --query banana --function
# CHECKERR: ^
# CHECKERR: (Type 'help abbr' for related documentation)
# CHECK: 2

abbr --add peach --function invalid/function/name
echo $status
# CHECKERR: abbr: Invalid function name: invalid/function/name
# CHECK: 2

# Function names cannot contain spaces, to prevent confusion with fish script.
abbr --add peach --function 'no space allowed'
echo $status
# CHECKERR: abbr: Invalid function name: no space allowed
# CHECK: 2

# Erase all abbreviations
abbr --erase (abbr --list)
abbr --show
# Should be no output

abbr --add nonregex_name foo
abbr --add regex_name --regex 'A[0-9]B' bar
abbr --add !! --position anywhere --function replace_history
abbr --show
# CHECK: abbr -a -- nonregex_name foo
# CHECK: abbr -a --regex 'A[0-9]B' -- regex_name bar
# CHECK: abbr -a --position anywhere --function replace_history -- !!

# Confirm that this erases the old uvar
# (slightly cheating since we haven't imported it as an abbr,
#  but that's okay)
abbr --erase cuckoo
echo erase $status
# CHECK: erase 0
set --show _fish_abbr_cuckoo
# Nothing

abbr --add '$PAGER' less
abbr --erase (abbr --list)
abbr --list
# Nothing
abbr --add '$PAGER' less
abbr --list
# CHECK: $PAGER

abbr --add bogus --position never stuff
# CHECKERR: abbr: Invalid position 'never'
# CHECKERR: Position must be one of: command, anywhere

abbr --add bogus --position anywhere --position command stuff
# CHECKERR: abbr: Cannot specify multiple positions

abbr --add --regex foo --function foo
# CHECKERR: abbr --add: Name cannot be empty
echo foo
# CHECK: foo

abbr --add regex_name --regex '(*UTF).*' bar
# CHECKERR: abbr: Regular expression compile error: using UTF is disabled by the application
# CHECKERR: abbr: (*UTF).*
# CHECKERR: abbr:      ^

abbr --add foo --set-cursor 'foo % bar'
abbr | grep foo
# CHECK: abbr -a --set-cursor='%' -- foo 'foo % bar'

# Command-specific and general abbrs can coexist
abbr __abbr_coexist "general def"
abbr --command foo __abbr_coexist "command def"
abbr | grep __abbr_coexist
# CHECK: abbr -a -- __abbr_coexist 'general def'
# CHECK: abbr -a --position anywhere --command foo -- __abbr_coexist 'command def'

# Erase general abbreviation
abbr -e __abbr_coexist
abbr | grep __abbr_coexist
# CHECK: abbr -a --position anywhere --command foo -- __abbr_coexist 'command def'

# Abbrs with different commands coexist
abbr --command bar __abbr_coexist "bar command"
abbr | grep __abbr_coexist
# CHECK: abbr -a --position anywhere --command foo -- __abbr_coexist 'command def'
# CHECK: abbr -a --position anywhere --command bar -- __abbr_coexist 'bar command'

# Rename command-specific abbr
abbr --rename --command bar __abbr_coexist __abbr_coexist_2
abbr | grep __abbr_coexist
# CHECK: abbr -a --position anywhere --command foo -- __abbr_coexist 'command def'
# CHECK: abbr -a --position anywhere --command bar -- __abbr_coexist_2 'bar command'
abbr -e --command foo __abbr_coexist
abbr -e --command bar __abbr_coexist_2
