\section cdh cdh - change to a recently visited directory

\subsection cdh-synopsis Synopsis
\fish{synopsis}
cdh [ directory ]
\endfish

\subsection cdh-description Description

`cdh` with no arguments presents a list of recently visited directories. You can then select one of the entries by letter or number. You can also press @key{tab} to use the completion pager to select an item from the list. If you give it a single argument it is equivalent to `cd directory`.

Note that the `cd` command limits directory history to the 25 most recently visited directories. The history is stored in the `$dirprev` and `$dirnext` variables which this command manipulates. If you make those universal variables your `cd` history is shared among all fish instances.

\subsection cdh-see-also See Also

See also the <a href="commands.html#prevd">`prevd`</a> and <a href="commands.html#pushd">`pushd`</a> commands which also work with the recent `cd` history and are provided for compatibility with other shells.
