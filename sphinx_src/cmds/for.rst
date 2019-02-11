\section for for - perform a set of commands multiple times.

\subsection for-synopsis Synopsis
\fish{synopsis}
for VARNAME in [VALUES...]; COMMANDS...; end
\endfish

\subsection for-description Description

`for` is a loop construct. It will perform the commands specified by `COMMANDS` multiple times. On each iteration, the local variable specified by `VARNAME` is assigned a new value from `VALUES`. If `VALUES` is empty, `COMMANDS` will not be executed at all. The `VARNAME` is visible when the loop terminates and will contain the last value assigned to it. If `VARNAME` does not already exist it will be set in the local scope. For our purposes if the `for` block is inside a function there must be a local variable with the same name. If the `for` block is not nested inside a function then global and universal variables of the same name will be used if they exist.

\subsection for-example Example

\fish
for i in foo bar baz; echo $i; end

# would output:
foo
bar
baz
\endfish

\subsection for-notes Notes

The `VARNAME` was local to the for block in releases prior to 3.0.0. This means that if you did something like this:

\fish
for var in a b c
    if break_from_loop
        break
    end
end
echo $var
\endfish

The last value assigned to `var` when the loop terminated would not be available outside the loop. What `echo $var` would write depended on what it was set to before the loop was run. Likely nothing.
