\section fish fish - the friendly interactive shell

\subsection fish-synopsis Synopsis
\fish{synopsis}
fish [OPTIONS] [-c command] [FILE [ARGUMENTS...]]
\endfish

\subsection fish-description Description

`fish` is a command-line shell written mainly with interactive use in mind. The full manual is available <a href='index.html'>in HTML</a> by using the <a href='#help'>help</a> command from inside fish.

The following options are available:

- `-c` or `--command=COMMANDS` evaluate the specified commands instead of reading from the commandline

- `-C` or `--init-command=COMMANDS` evaluate the specified commands after reading the configuration, before running the command specified by `-c` or reading interactive input

- `-d` or `--debug-level=DEBUG_LEVEL` specify the verbosity level of fish. A higher number means higher verbosity. The default level is 1.

- `-i` or `--interactive` specify that fish is to run in interactive mode

- `-l` or `--login` specify that fish is to run as a login shell

- `-n` or `--no-execute` do not execute any commands, only perform syntax checking

- `-p` or `--profile=PROFILE_FILE` when fish exits, output timing information on all executed commands to the specified file

- `-v` or `--version` display version and exit

- `-D` or `--debug-stack-frames=DEBUG_LEVEL` specify how many stack frames to display when debug messages are written. The default is zero. A value of 3 or 4 is usually sufficient to gain insight into how a given debug call was reached but you can specify a value up to 128.

- `-f` or `--features=FEATURES` enables one or more feature flags (separated by a comma). These are how fish stages changes that might break scripts.

The fish exit status is generally the exit status of the last foreground command. If fish is exiting because of a parse error, the exit status is 127.
