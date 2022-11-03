# SPDX-FileCopyrightText: © 2020 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

#RUN: %fish %s
# Verify that specifying unexpected options or arguments results in an error.

# First using the legacy, now deprecated, long options to specify a
# subcommand.

# First with the history function.
history --search --merge
#CHECKERR: history: merge search: options cannot be used together
history --clear --contains
#CHECKERR: history: clear: subcommand takes no options
history --merge -t
#CHECKERR: history: merge: subcommand takes no options
history --save xyz
#CHECKERR: history: save: expected 0 arguments; got 1

# Now with the history builtin.
builtin history --save --prefix
#CHECKERR: history: save: subcommand takes no options
builtin history --clear --show-time
#CHECKERR: history: clear: subcommand takes no options
builtin history --merge xyz
#CHECKERR: history: merge: expected 0 arguments; got 1
builtin history --clear abc def
#CHECKERR: history: clear: expected 0 arguments; got 2

# Now using the preferred subcommand form. Note that we support flags before
# or after the subcommand name so test both variants.

# First with the history function.
history clear --contains
#CHECKERR: history: clear: subcommand takes no options
history clear-session --contains
#CHECKERR: history: clear-session: subcommand takes no options
history merge -t
#CHECKERR: history: merge: subcommand takes no options
history save xyz
#CHECKERR: history: save: expected 0 arguments; got 1
history --prefix clear
#CHECKERR: history: clear: subcommand takes no options
history --prefix clear-session
#CHECKERR: history: clear-session: subcommand takes no options
history --show-time merge
#CHECKERR: history: merge: subcommand takes no options

# Now with the history builtin.
builtin history --search --merge
#CHECKERR: history: search merge: options cannot be used together
builtin history save --prefix
#CHECKERR: history: save: subcommand takes no options
builtin history clear --show-time
#CHECKERR: history: clear: subcommand takes no options
builtin history clear-session --show-time
#CHECKERR: history: clear-session: subcommand takes no options
builtin history merge xyz
#CHECKERR: history: merge: expected 0 arguments; got 1
builtin history clear abc def
#CHECKERR: history: clear: expected 0 arguments; got 2
builtin history clear-session abc def
#CHECKERR: history: clear-session: expected 0 arguments; got 2
builtin history --contains save
#CHECKERR: history: save: subcommand takes no options
builtin history -t merge
#CHECKERR: history: merge: subcommand takes no options

# Now do a history command that should succeed so we exit with a zero,
# success, status.
builtin history save

set -g fish_private_mode 1
builtin history merge
#CHECKERR: history: can't merge history in private mode
