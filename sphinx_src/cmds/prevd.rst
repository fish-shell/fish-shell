prevd - move backward through directory history
==========================================


\subsection prevd-synopsis Synopsis
\fish{synopsis}
prevd [ -l | --list ] [POS]
\endfish

\subsection prevd-description Description

`prevd` moves backwards `POS` positions in the history of visited directories; if the beginning of the history has been hit, a warning is printed.

If the `-l` or `--list` flag is specified, the current history is also displayed.

Note that the `cd` command limits directory history to the 25 most recently visited directories. The history is stored in the `$dirprev` and `$dirnext` variables which this command manipulates.

You may be interested in the <a href="commands.html#cdh">`cdh`</a> command which provides a more intuitive way to navigate to recently visited directories.

\subsection prevd-example Example

\fish
cd /usr/src
# Working directory is now /usr/src

cd /usr/src/fish-shell
# Working directory is now /usr/src/fish-shell

prevd
# Working directory is now /usr/src

nextd
# Working directory is now /usr/src/fish-shell
\endfish
