\section alias alias - create a function

\subsection alias-synopsis Synopsis
\fish{synopsis}
alias
alias [OPTIONS] NAME DEFINITION
alias [OPTIONS] NAME=DEFINITION
\endfish

\subsection alias-description Description

`alias` is a simple wrapper for the `function` builtin, which creates a function wrapping a command. It has similar syntax to POSIX shell `alias`. For other uses, it is recommended to define a <a href='#function'>function</a>.

`fish` marks functions that have been created by `alias` by including the command used to create them in the function description. You can list `alias`-created functions by running `alias` without arguments. They must be erased using `functions -e`.

- `NAME` is the name of the alias
- `DEFINITION` is the actual command to execute. The string `$argv` will be appended.

You cannot create an alias to a function with the same name. Note that spaces need to be escaped in the call to `alias` just like at the command line, _even inside quoted parts_.

The following options are available:

- `-h` or `--help` displays help about using this command.

- `-s` or `--save` Automatically save the function created by the alias into your fish configuration directory using <a href='#funcsave'>funcsave</a>.

\subsection alias-example Example

The following code will create `rmi`, which runs `rm` with additional arguments on every invocation.

\fish
alias rmi="rm -i"

# This is equivalent to entering the following function:
function rmi --wraps rm --description 'alias rmi=rm -i'
    rm -i $argv
end

# This needs to have the spaces escaped or "Chrome.app..." will be seen as an argument to "/Applications/Google":
alias chrome='/Applications/Google\ Chrome.app/Contents/MacOS/Google\ Chrome banana'
\endfish
