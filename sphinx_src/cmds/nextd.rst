\section nextd nextd - move forward through directory history

\subsection nextd-synopsis Synopsis
\fish{synopsis}
nextd [ -l | --list ] [POS]
\endfish

\subsection nextd-description Description

`nextd` moves forwards `POS` positions in the history of visited directories; if the end of the history has been hit, a warning is printed.

If the `-l` or `--list` flag is specified, the current directory history is also displayed.

Note that the `cd` command limits directory history to the 25 most recently visited directories. The history is stored in the `$dirprev` and `$dirnext` variables which this command manipulates.

You may be interested in the <a href="commands.html#cdh">`cdh`</a> command which provides a more intuitive way to navigate to recently visited directories.

\subsection nextd-example Example

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
