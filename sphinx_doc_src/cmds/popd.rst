\section popd popd - move through directory stack

\subsection popd-synopsis Synopsis
\fish{synopsis}
popd
\endfish

\subsection popd-description Description

`popd` removes the top directory from the directory stack and changes the working directory to the new top directory. Use <a href="#pushd">`pushd`</a> to add directories to the stack.

You may be interested in the <a href="commands.html#cdh">`cdh`</a> command which provides a more intuitive way to navigate to recently visited directories.

\subsection popd-example Example

\fish
pushd /usr/src
# Working directory is now /usr/src
# Directory stack contains /usr/src

pushd /usr/src/fish-shell
# Working directory is now /usr/src/fish-shell
# Directory stack contains /usr/src /usr/src/fish-shell

popd
# Working directory is now /usr/src
# Directory stack contains /usr/src
\endfish
