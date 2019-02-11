while - perform a command multiple times
==========================================

Synopsis
--------

while CONDITION; COMMANDS...; end


Description
------------

`while` repeatedly executes `CONDITION`, and if the exit status is 0, then executes `COMMANDS`.

If the exit status of `CONDITION` is non-zero on the first iteration, `COMMANDS` will not be
executed at all, and the exit status of the loop set to the exit status of `CONDITION`.

The exit status of the loop is 0 otherwise.

You can use <a href="#and">`and`</a> or <a href="#or">`or`</a> for complex conditions. Even more complex control can be achieved with `while true` containing a <a href="#break">break</a>.

Example
------------

\fish
while test -f foo.txt; or test -f bar.txt ; echo file exists; sleep 10; end
# outputs 'file exists' at 10 second intervals as long as the file foo.txt or bar.txt exists.
\endfish
