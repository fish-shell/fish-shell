#RUN: %fish %s
# Verify that specifying unexpected options or arguments results in an error.

# First using the legacy, now deprecated, long options to specify a
# subcommand.

# First with the history function.
history --search --merge
#CHECKERR: history: Mutually exclusive flags 'merge' and `search` seen
history --clear --contains
#CHECKERR: history: you cannot use any options with the clear command
history --merge -t
#CHECKERR: history: you cannot use any options with the merge command
history --save xyz
#CHECKERR: history: save expected 0 args, got 1

# Now with the history builtin.
builtin history --save --prefix
#CHECKERR: history: you cannot use any options with the save command
builtin history --clear --show-time
#CHECKERR: history: you cannot use any options with the clear command
builtin history --merge xyz
#CHECKERR: history merge: Expected 0 args, got 1
builtin history --clear abc def
#CHECKERR: history clear: Expected 0 args, got 2

# Now using the preferred subcommand form. Note that we support flags before
# or after the subcommand name so test both variants.

# First with the history function.
history clear --contains
#CHECKERR: history: you cannot use any options with the clear command
history merge -t
#CHECKERR: history: you cannot use any options with the merge command
history save xyz
#CHECKERR: history: save expected 0 args, got 1
history --prefix clear
#CHECKERR: history: you cannot use any options with the clear command
history --show-time merge
#CHECKERR: history: you cannot use any options with the merge command

# Now with the history builtin.
builtin history --search --merge
#CHECKERR: history: Invalid combination of options,
#CHECKERR: you cannot do both 'search' and 'merge' in the same invocation
builtin history save --prefix
#CHECKERR: history: you cannot use any options with the save command
builtin history clear --show-time
#CHECKERR: history: you cannot use any options with the clear command
builtin history merge xyz
#CHECKERR: history merge: Expected 0 args, got 1
builtin history clear abc def
#CHECKERR: history clear: Expected 0 args, got 2
builtin history --contains save
#CHECKERR: history: you cannot use any options with the save command
builtin history -t merge
#CHECKERR: history: you cannot use any options with the merge command

# Now do a history command that should succeed so we exit with a zero,
# success, status.
builtin history save
