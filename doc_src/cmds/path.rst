.. _cmd-path:

path - manipulate and check paths
=================================

Synopsis
--------

::

    path base [(-z | --null-in)] [(-Z | --null-out)] [(-q | --quiet)] [PATH...]
    path dir  [(-z | --null-in)] [(-Z | --null-out)] [(-q | --quiet)] [PATH...]
    path extension [(-z | --null-in)] [(-Z | --null-out)] [(-q | --quiet)] [PATH...]
    path filter [(-z | --null-in)] [(-Z | --null-out)] [(-v | --invert)] [(-q | --quiet)] [(-t | --type) TYPE] [(-p | --perm) PERMISSION] [PATH...]
    path is [(-z | --null-in)] [(-Z | --null-out)] [(-v | --invert)] [(-q | --quiet)] [(-t | --type) TYPE] [(-p | --perm) PERMISSION] [PATH...]
    path normalize [(-z | --null-in)] [(-Z | --null-out)] [(-q | --quiet)] [PATH...]
    path real [(-z | --null-in)] [(-Z | --null-out)] [(-q | --quiet)] [PATH...]
    path strip-extension [(-z | --null-in)] [(-Z | --null-out)] [(-q | --quiet)] [PATH...]

Description
-----------

``path`` performs operations on paths.

PATH arguments are taken from the command line unless standard input is connected to a pipe or a file, in which case they are read from standard input, one PATH per line. It is an error to supply PATH arguments on both the command line and on standard input.

Arguments starting with ``-`` are normally interpreted as switches; ``--`` causes the following arguments not to be treated as switches even if they begin with ``-``. Switches and required arguments are recognized only on the command line.

All subcommands accept a ``-q`` or ``--quiet`` switch, which suppresses the usual output but exits with the documented status. In this case these commands will quit early, without reading all of the available input.

All subcommands also accept a ``-Z`` or ``--null-out`` switch, which makes them print output separated with NULL instead of newlines. This is for further processing, e.g. passing to another ``path``, or ``xargs -0``. This is not recommended when the output goes to the terminal or a command substitution.

All subcommands also accept a ``-z`` or ``--null-in`` switch, which makes them accept arguments from stdin separated with NULL-bytes. Since Unix paths can't contain NULL, that makes it possible to handle all possible paths and read input from e.g. ``find -print0``. If arguments are given on the commandline this has no effect. This should mostly be unnecessary since ``path`` automatically starts splitting on NULL if one appears in the first PATH_MAX bytes, PATH_MAX being the operating system's maximum length for a path plus a NULL byte.

Some subcommands operate on the paths as strings and so work on nonexistent paths, while others need to access the paths themselves and so filter out nonexistent paths.

The following subcommands are available.

.. _cmd-path-base:

"base" subcommand
--------------------

::

    path base [(-z | --null-in)] [(-Z | --null-out)] [(-q | --quiet)] [PATH...]

``path base`` returns the last path component of the given path, by removing the directory prefix and removing trailing slashes. In other words, it is the part that is not the dirname. For files you might call it the "filename".

It returns 0 if there was a basename, i.e. if the path wasn't empty or just slashes.

Examples
^^^^^^^^

::

   >_ path base ./foo.mp4
   foo.mp4

   >_ path base ../banana
   banana

   >_ path base /usr/bin/
   bin

   >_ path base /usr/bin/*
   # This prints all files in /usr/bin/
   # A selection:
   cp
   fish
   grep
   rm

"dir" subcommand
--------------------

::

    path dir [(-z | --null-in)] [(-Z | --null-out)] [(-q | --quiet)] [PATH...]

``path dir`` returns the dirname for the given path. This is the part before the last "/", discounting trailing slashes. In other words, it is the part that is not the basename (discounting superfluous slashes).

It returns 0 if there was a dirname, i.e. if the path wasn't empty or just slashes.

Examples
^^^^^^^^

::

   >_ path dir ./foo.mp4
   .

   >_ path base ../banana
   banana

   >_ path base /usr/bin/
   bin

"extension" subcommand
-----------------------

::

    path extension [(-z | --null-in)] [(-Z | --null-out)] [(-q | --quiet)] [PATH...]

``path extension`` returns the extension of the given path. This is the part after (and excluding) the last ".", unless that "." followed a "/" or the basename is "." or "..", in which case there is no extension and nothing is printed.

If the filename ends in a ".", the extension is empty, so an empty line will be printed.

It returns 0 if there was an extension.

Examples
^^^^^^^^

::

   >_ path extension ./foo.mp4
   mp4

   >_ path extension ../banana
   # nothing, status 1

   >_ path extension ~/.config
   # nothing, status 1

   >_ path extension ~/.config.d
   d

   >_ path extension ~/.config.
   # one empty line, status 0
   
"filter" subcommand
--------------------

::

    path filter [(-z | --null-in)] [(-Z | --null-out)] [(-q | --quiet)] [(-t | --type) TYPE] [(-p | --perm) PERMISSION] [PATH...]

``path filter`` returns all of the given paths that match the given checks. In all cases, the paths need to exist, nonexistent paths are always filtered.

The available filters are:

- ``-t`` or ``--type`` with the options: "dir", "file", "link", "block", "char", "fifo" and "socket", in which case the path needs to be a directory, file, link, block device, character device, named pipe or socket, respectively.
- ``-d``, ``-f`` and ``-l`` are short for ``--type=dir``, ``--type=file`` and ``--type=link``, respectively.

- ``-p`` or ``--perm`` with the options: "read", "write", and "exec", as well as "suid", "sgid", "sticky", "user" (referring to the path owner) and "group" (referring to the path's group), in which case the path needs to have all of the given permissions for the current user.
- ``-r``, ``-w`` and ``-x`` are short for ``--perm=read``, ``--perm=write`` and ``--perm=exec``, respectively.

Note that the path needs to be *any* of the given types, but have *all* of the given permissions. The filter options can either be given as multiple options, or comma-separated - ``path filter -t dir,file`` or ``path filter --type dir --type file`` are equivalent.

If your operating system doesn't support a "sticky" bit, that check will always be false, so no path will pass.

With ``--invert``, the meaning of the filtering is inverted - any path that wouldn't pass (including by not existing) passes, and any path that would pass fails.

It returns 0 if at least one path passed the filter.

Examples
^^^^^^^^

::

   >_ path filter /usr/bin /usr/argagagji
   # The (hopefully) nonexistent argagagji is filtered implicitly:
   /usr/bin

   >_ path filter --type file /usr/bin /usr/bin/fish
   # Only fish is a file
   /usr/bin/fish

   >_ path filter --type file,dir --perm exec,write /usr/bin/fish /home/me
   # fish is a file, which passes, and executable, which passes,
   # but probably not writable, which fails.
   #
   # $HOME is a directory and both writable and executable, typically.
   # So it passes.
   /home/me

   >_ path filter -fdxw /usr/bin/fish /home/me
   # This is the same as above: "-f" is "--type=file", "-d" is "--type=dir",
   # "-x" is short for "--perm=exec" and "-w" short for "--perm=write"!
   /home/me
   
"is" subcommand
--------------------

::

    path is [(-z | --null-in)] [(-Z | --null-out)] [(-q | --quiet)] [(-t | --type) TYPE] [(-p | --perm) PERMISSION] [PATH...]

``path is`` is short for ``path filter -q``. It returns true if any of the given files passes the filter.

Examples
^^^^^^^^

::

   >_ path is /usr/bin /usr/argagagji
   # /usr/bin exists, so this returns a status of 0 (true).
   >_ path is /usr/argagagji
   # /usr/argagagji does not, so this returns a status of 1 (false).
   >_ path is -fx /bin/sh
   # /bin/sh is usually an executable file, so this returns true.

"normalize" subcommand
-----------------------

::

    path normalize [(-z | --null-in)] [(-Z | --null-out)] [(-q | --quiet)] [PATH...]

``path normalize`` returns the normalized versions of all paths. That means it squashes duplicate "/" (except for two leading "//"), collapses "../" with earlier components and removes "." components.

It is the same as ``realpath --no-symlinks``, as it creates the "real", canonical version of the path but doesn't resolve any symlinks. As such it can operate on nonexistent paths.

It returns 0 if any normalization was done, i.e. any given path wasn't in canonical form.

Examples
^^^^^^^^

::

    >_ path normalize /usr/bin//../../etc/fish
    # The "//" is squashed and the ".." components neutralize the components before
    /etc/fish

    >_ path normalize /bin//bash
    # The "//" is squashed, but /bin isn't resolved even if your system links it to /usr/bin.
    /bin/bash
    
"real" subcommand
--------------------

::

    path real [(-z | --null-in)] [(-Z | --null-out)] [(-q | --quiet)] [PATH...]

``path real`` returns the normalized, physical versions of all paths. That means it resolves symlinks and does what ``path normalize`` does: it squashes duplicate "/" (except for two leading "//"), collapses "../" with earlier components and removes "." components.

It is the same as ``realpath``, as it creates the "real", canonical version of the path. As such it can't operate on nonexistent paths.

It returns 0 if any normalization or resolution was done, i.e. any given path wasn't in canonical form.

Examples
^^^^^^^^

::
   >_ path real /bin//sh
   # The "//" is squashed, and /bin is resolved if your system links it to /usr/bin.
   # sh here is bash (on an Archlinux system)
   /usr/bin/bash
    
"strip-extension" subcommand
----------------------------

::
    path strip-extension [(-z | --null-in)] [(-Z | --null-out)] [(-q | --quiet)] [PATH...]

``path strip-extension`` returns the given paths without the extension. This is the part after (and excluding) the last ".", unless that "." followed a "/" or the basename is "." or "..", in which case there is no extension and the full path is printed.

This is, of course, the inverse of ``path extension``.

It returns 0 if there was an extension.

Examples
^^^^^^^^

::

   >_ path strip-extension ./foo.mp4
   ./foo

   >_ path strip-extension ../banana
   ../banana
   # but status 1, because there was no extension.

   >_ path strip-extension ~/.config
   /home/alfa/.config
   # status 1

   >_ path strip-extension ~/.config.d
   /home/alfa/.config
   # status 0

   >_ path strip-extension ~/.config.
   /home/alfa/.config
   # status 0
   
Combining ``path``
-------------------

``path`` is meant to be easy to combine with itself, other tools and fish.

This is why

- ``path``'s output is automatically split by fish if it goes into a command substitution, so just doing ``(path ...)`` handles all paths, even those containing newlines, correctly
- ``path`` has ``--null-in`` to handle null-delimited input (typically automatically detected!), and ``--null-out`` to pass on null-delimited output

Some examples of combining ``path``::

  # Expand all paths in the current directory, leave only executable files, and print their real path
  path expand '*' -Z | path filter -zZ --perm=exec --type=file | path real -z

  # The same thing, but using find (note -maxdepth needs to come first or find will scream)
  # (this also depends on your particular version of find)
  # Note the `-z` is unnecessary for any sensible version of find - if `path` sees a NULL,
  # it will split on NULL automatically.
  find . -maxdepth 1 -type f -executable -print0 | path real -z

  set -l paths (path filter -p exec $PATH/fish -Z | path real)
