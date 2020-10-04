#RUN: %fish %s
# Test basic add and list of __abbr1
abbr __abbr1 alpha beta gamma
abbr | grep __abbr1
# CHECK: abbr -a -U -- __abbr1 'alpha beta gamma'

# Erasing one that doesn\'t exist should do nothing
abbr --erase NOT_AN_ABBR
abbr | grep __abbr1
# CHECK: abbr -a -U -- __abbr1 'alpha beta gamma'

# Adding existing __abbr1 should be idempotent
abbr __abbr1 alpha beta gamma
abbr | grep __abbr1
# CHECK: abbr -a -U -- __abbr1 'alpha beta gamma'

# Replacing __abbr1 definition
abbr __abbr1 delta
abbr | grep __abbr1
# CHECK: abbr -a -U -- __abbr1 delta

# __abbr1 -s and --show tests
abbr -s | grep __abbr1
abbr --show | grep __abbr1
# CHECK: abbr -a -U -- __abbr1 delta
# CHECK: abbr -a -U -- __abbr1 delta

# Test erasing __abbr1
abbr -e __abbr1
abbr | grep __abbr1

# Ensure we escape special characters on output
abbr '~__abbr2' '$xyz'
abbr | grep __abbr2
# CHECK: abbr -a -U -- '~__abbr2' '$xyz'
abbr -e '~__abbr2'

# Ensure we handle leading dashes in abbreviation names properly
abbr -- --__abbr3 xyz
abbr | grep __abbr3
# CHECK: abbr -a -U -- --__abbr3 xyz
abbr -e -- --__abbr3

# Test that an abbr word containing spaces is rejected
abbr "a b c" "d e f"
abbr | grep 'a b c'
# CHECKERR: abbr --add: Abbreviation 'a b c' cannot have spaces in the word

# Test renaming
abbr __abbr4 omega
abbr | grep __abbr5
abbr -r __abbr4 __abbr5
abbr | grep __abbr5
# CHECK: abbr -a -U -- __abbr5 omega
abbr -e __abbr5
abbr | grep __abbr4

# Test renaming a nonexistent abbreviation
abbr -r __abbr6 __abbr
# CHECKERR: abbr --rename: No abbreviation named __abbr6

# Test renaming to a abbreviation with spaces
abbr __abbr4 omega
abbr -r __abbr4 "g h i"
# CHECKERR: abbr --rename: Abbreviation 'g h i' cannot have spaces in the word
abbr -e __abbr4

# Test renaming without arguments
abbr __abbr7 omega
abbr -r __abbr7
# CHECKERR: abbr --rename: Requires exactly two arguments

# Test renaming with too many arguments
abbr __abbr8 omega
abbr -r __abbr8 __abbr9 __abbr10
# CHECKERR: abbr --rename: Requires exactly two arguments
abbr | grep __abbr8
abbr | grep __abbr9
abbr | grep __abbr10
# CHECK: abbr -a -U -- __abbr8 omega

# Test renaming to existing abbreviation
abbr __abbr11 omega11
abbr __abbr12 omega12
abbr -r __abbr11 __abbr12
# CHECKERR: abbr --rename: Abbreviation __abbr12 already exists, cannot rename __abbr11

abbr __abbr-with-dashes omega
complete -C__abbr-with
# CHECK: __abbr-with-dashes	Abbreviation: omega

abbr --add --global __abbr13 aaaaaaaaaaaaa
abbr --add --global __abbr14 bbbbbbbbbbbbb
abbr --erase __abbr13 __abbr14
abbr | grep __abbr13
abbr | grep __abbr14
