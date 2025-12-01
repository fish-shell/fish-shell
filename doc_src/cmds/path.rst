path - manipulate and check paths
=================================

Synopsis
--------

.. synopsis::

    path basename GENERAL_OPTIONS [(-E | --no-extension)] [PATH ...]
    path dirname GENERAL_OPTIONS  [PATH ...]
    path extension GENERAL_OPTIONS [PATH ...]
    path filter GENERAL_OPTIONS [-v | --invert]
        [-d] [-f] [-l] [-r] [-w] [-x]
        [(-t | --type) TYPE] [(-p | --perm) PERMISSION] [--all] [PATH ...]
    path is GENERAL_OPTIONS [(-v | --invert)] [(-t | --type) TYPE]
        [-d] [-f] [-l] [-r] [-w] [-x]
        [(-p | --perm) PERMISSION] [PATH ...]
    path mtime GENERAL_OPTIONS [(-R | --relative)] [PATH ...]
    path normalize GENERAL_OPTIONS [PATH ...]
    path resolve GENERAL_OPTIONS [PATH ...]
    path change-extension GENERAL_OPTIONS EXTENSION [PATH ...]
    path sort GENERAL_OPTIONS [-r | --reverse]
        [-u | --unique] [--key=(basename | dirname | path)] [PATH ...]

    GENERAL_OPTIONS
        [-z | --null-in] [-Z | --null-out] [-q | --quiet]

Description
-----------

``path`` performs operations on paths.

PATH arguments are taken from the command line unless standard input is connected to a pipe or a file, in which case they are read from standard input, one PATH per line. It is an error to supply PATH arguments on both the command line and on standard input.

Arguments starting with ``-`` are normally interpreted as switches; ``--`` causes the following arguments not to be treated as switches even if they begin with ``-``. Switches and required arguments are recognized only on the command line.

When a path starts with ``-``, ``path filter`` and ``path normalize`` will prepend ``./`` on output to avoid it being interpreted as an option otherwise, so it's safe to pass path's output to other commands that can handle relative paths.

All subcommands accept a ``-q`` or ``--quiet`` switch, which suppresses the usual output but exits with the documented status. In this case these commands will quit early, without reading all of the available input.

All subcommands also accept a ``-Z`` or ``--null-out`` switch, which makes them print output separated with NUL instead of newlines. This is for further processing, e.g. passing to another ``path``, or ``xargs -0``. This is not recommended when the output goes to the terminal or a command substitution.

All subcommands also accept a ``-z`` or ``--null-in`` switch, which makes them accept arguments from stdin separated with NULL-bytes. Since Unix paths can't contain NULL, that makes it possible to handle all possible paths and read input from e.g. ``find -print0``. If arguments are given on the commandline this has no effect. This should mostly be unnecessary since ``path`` automatically starts splitting on NULL if one appears in the first PATH_MAX bytes, PATH_MAX being the operating system's maximum length for a path plus a NULL byte.

Some subcommands operate on the paths as strings and so work on nonexistent paths, while others need to access the paths themselves and so filter out nonexistent paths.

The following subcommands are available.

.. _cmd-path-basename:

"basename" subcommand
---------------------

::

    path basename [-E | --no-extension] [-z | --null-in] [-Z | --null-out] [-q | --quiet] [PATH ...]

``path basename`` returns the last path component of the given path, by removing the directory prefix and removing trailing slashes. In other words, it is the part that is not the dirname. For files you might call it the "filename".

If the ``-E`` or ``---no-extension`` option is used and the base name contained a period, the path is returned with the extension (or the last extension) removed, i.e. the "filename" without an extension (akin to calling ``path change-extension "" (path basename $path)``).

It returns 0 if there was a basename, i.e. if the path wasn't empty or just slashes.

Examples
^^^^^^^^

::

   >_ path basename ./foo.mp4
   foo.mp4

   >_ path basename ../banana
   banana

   >_ path basename /usr/bin/
   bin

   >_ path basename /usr/bin/*
   # This prints all files in /usr/bin/
   # A selection:
   cp
   fish
   grep
   rm

"dirname" subcommand
--------------------

::

    path dirname [-z | --null-in] [-Z | --null-out] [-q | --quiet] [PATH ...]

``path dirname`` returns the dirname for the given path. This is the part before the last "/", discounting trailing slashes. In other words, it is the part that is not the basename (discounting superfluous slashes).

It returns 0 if there was a dirname, i.e. if the path wasn't empty or just slashes.

Examples
^^^^^^^^

::

   >_ path dirname ./foo.mp4
   .

   >_ path dirname ../banana
   ..

   >_ path dirname /usr/bin/
   /usr

"extension" subcommand
-----------------------

::

    path extension [-z | --null-in] [-Z | --null-out] [-q | --quiet] [PATH ...]

``path extension`` returns the extension of the given path. This is the part after (and including) the last ".", unless that "." followed a "/" or the basename is "." or "..", in which case there is no extension and an empty line is printed.

If the filename ends in a ".", only a "." is printed.

It returns 0 if there was an extension.

Examples
^^^^^^^^

::

   >_ path extension ./foo.mp4
   .mp4

   >_ path extension ../banana
   # an empty line, status 1

   >_ path extension ~/.config
   # an empty line, status 1

   >_ path extension ~/.config.d
   .d

   >_ path extension ~/.config.
   .

   >_ set -l path (path change-extension '' ./foo.mp4)
   >_ set -l extension (path extension ./foo.mp4)
   > echo $path$extension
   # reconstructs the original path again.
   ./foo.mp4

.. _cmd-path-filter:

"filter" subcommand
--------------------

::

    path filter [-z | --null-in] [-Z | --null-out] [-q | --quiet] \
        [-d] [-f] [-l] [-r] [-w] [-x] \
        [-v | --invert] [(-t | --type) TYPE] [(-p | --perm) PERMISSION] [--all] [PATH ...]

``path filter`` returns all of the given paths that match the given checks. In all cases, the paths need to exist, nonexistent paths are always filtered.

The available filters are:

- ``-t`` or ``--type`` with the options: "dir", "file", "link", "block", "char", "fifo" and "socket", in which case the path needs to be a directory, file, link, block device, character device, named pipe or socket, respectively.
- ``-d``, ``-f`` and ``-l`` are short for ``--type=dir``, ``--type=file`` and ``--type=link``, respectively. There are no shortcuts for the other types.

- ``-p`` or ``--perm`` with the options: "read", "write", and "exec", as well as "suid", "sgid", "user" (referring to the path owner) and "group" (referring to the path's group), in which case the path needs to have all of the given permissions for the current user.
- ``-r``, ``-w`` and ``-x`` are short for ``--perm=read``, ``--perm=write`` and ``--perm=exec``, respectively. There are no shortcuts for the other permissions.

Note that the path needs to be *any* of the given types, but have *all* of the given permissions. This is because having a path that is both writable and executable makes sense, but having a path that is both a directory and a file doesn't. Links will count as the type of the linked-to file, so links to files count as files, links to directories count as directories.

The filter options can either be given as multiple options, or comma-separated - ``path filter -t dir,file`` or ``path filter --type dir --type file`` are equivalent.

With ``--invert``, the meaning of the filtering is inverted - any path that wouldn't pass (including by not existing) passes, and any path that would pass fails.

When a path starts with ``-``, ``path filter`` will prepend ``./`` to avoid it being interpreted as an option otherwise.

It returns 0 if at least one path passed the filter.

With ``--all``, return status 0 (true) if all paths pass the filter, and status 1 (false) if any path fails. This is equivalent to ``not path filter -v``. It produces no output, only a status.

When ``--all`` combined with ``--invert``, it returns status 0 (true) if all paths fail the filter and status 1 (false) if any path passes.

``path is`` is shorthand for ``path filter -q``, i.e. just checking without producing output, see :ref:`The is subcommand <cmd-path-is>`.

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

   >_ path filter -fx $PATH/*
   # Prints all possible commands - the first entry of each name is what fish would execute!

   >_ path filter --all /usr/bin /usr/argagagji
   # This returns 1 (false) because not all paths pass the filter.

.. _cmd-path-is:

"is" subcommand
--------------------

::

    path is [-z | --null-in] [-Z | --null-out] [-q | --quiet] \
        [-d] [-f] [-l] [-r] [-w] [-x] \
        [-v | --invert] [(-t | --type) TYPE] [(-p | --perm) PERMISSION] [PATH ...]

``path is`` is short for ``path filter -q``. It returns true if any of the given files passes the filter, but does not produce any output.

``--quiet`` can still be passed for compatibility but is redundant. The options are the same as for ``path filter``.

Examples
^^^^^^^^

::

   >_ path is /usr/bin /usr/argagagji
   # /usr/bin exists, so this returns a status of 0 (true). It prints nothing.
   >_ path is /usr/argagagji
   # /usr/argagagji does not, so this returns a status of 1 (false). It also prints nothing.
   >_ path is -fx /bin/sh
   # /bin/sh is usually an executable file, so this returns true.

"mtime" subcommand
-----------------------

::

    path mtime [-z | --null-in] [-Z | --null-out] [-q | --quiet] [-R | --relative] [PATH ...]

``path mtime`` returns the last modification time ("mtime" in unix jargon) of the given paths, in seconds since the unix epoch (the beginning of the 1st of January 1970).

With ``--relative`` (or ``-R``), it prints the number of seconds since the modification time. It only reads the current time once at start, so in case multiple paths are given the times are all relative to the *start* of ``path mtime -R`` running.

If you want to know if a file is newer or older than another file, consider using ``test -nt`` instead. See :doc:`the test documentation <test>`.

It returns 0 if reading mtime for any path succeeded.

Examples
^^^^^^^^

::

    >_ date +%s
    # This prints the current time as seconds since the epoch
    1657217847

    >_ path mtime /etc/
    1657213796

    >_ path mtime -R /etc/
    4078
    # So /etc/ on this system was last modified a little over an hour ago

    # This is the same as
    >_ math (date +%s) - (path mtime /etc/)

"normalize" subcommand
-----------------------

::

    path normalize [-z | --null-in] [-Z | --null-out] [-q | --quiet] [PATH ...]

``path normalize`` returns the normalized versions of all paths. That means it squashes duplicate "/", collapses "../" with earlier components and removes "." components.

Unlike ``realpath`` or ``path resolve``, it does not make the paths absolute. It also does not resolve any symlinks. As such it can operate on non-existent paths.

Because it operates on paths as strings and doesn't resolve symlinks, it works sort of like ``pwd -L`` and ``cd``. E.g. ``path normalize link/..`` will return ``.``, just like ``cd link; cd ..`` would return to the current directory. For a physical view of the filesystem, see ``path resolve``.

Leading "./" components are usually removed. But when a path starts with ``-``, ``path normalize`` will add it instead to avoid confusion with options.

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

    >_ path normalize ./my/subdirs/../sub2
    my/sub2

    >_ path normalize -- -/foo
    ./-/foo

"resolve" subcommand
--------------------

::

    path resolve [-z | --null-in] [-Z | --null-out] [-q | --quiet] [PATH ...]

``path resolve`` returns the normalized, physical and absolute versions of all paths. That means it resolves symlinks and does what ``path normalize`` does: it squashes duplicate "/", collapses "../" with earlier components and removes "." components. Then it turns that path into the absolute path starting from the filesystem root "/".

It is similar to ``realpath``, as it creates the "real", canonical version of the path. However, for paths that can't be resolved, e.g. if they don't exist or form a symlink loop, it will resolve as far as it can and normalize the rest.

Because it resolves symlinks, it works sort of like ``pwd -P``. E.g. ``path resolve link/..`` will return the parent directory of what the link points to, just like ``cd link; cd (pwd -P)/..`` would go to it. For a logical view of the filesystem, see ``path normalize``.

It returns 0 if any normalization or resolution was done, i.e. any given path wasn't in canonical form.

Examples
^^^^^^^^

::

   >_ path resolve /bin//sh
   # The "//" is squashed, and /bin is resolved if your system links it to /usr/bin.
   # sh here is bash (this is common on linux systems)
   /usr/bin/bash

   >_ path resolve /bin/foo///bar/../baz
   # Assuming /bin exists and is a symlink to /usr/bin, but /bin/foo doesn't.
   # This resolves the /bin/ and normalizes the nonexistent rest:
   /usr/bin/foo/baz

"change-extension" subcommand
-----------------------------

::

    path change-extension [-z | --null-in] [-Z | --null-out] \
        [-q | --quiet] EXTENSION [PATH ...]

``path change-extension`` returns the given paths, with their extension changed to the given new extension. The extension is the part after (and including) the last ".", unless that "." followed a "/" or the basename is "." or "..", in which case there is no previous extension and the new one is added.

If the extension is empty, any previous extension is stripped, along with the ".". This is, of course, the inverse of ``path extension``.

One leading dot on the extension is ignored, so ".mp3" and "mp3" are treated the same.

It returns 0 if it was given any paths.

Examples
^^^^^^^^

::

   >_ path change-extension mp4 ./foo.wmv
   ./foo.mp4

   >_ path change-extension .mp4 ./foo.wmv
   ./foo.mp4

   >_ path change-extension '' ../banana
   ../banana

   >_ path change-extension '' ~/.config
   /home/alfa/.config

   >_ path change-extension '' ~/.config.d
   /home/alfa/.config

   >_ path change-extension '' ~/.config.
   /home/alfa/.config

"sort" subcommand
-----------------------------

::

    path sort [-z | --null-in] [-Z | --null-out] \
        [-q | --quiet] [-r | --reverse] \
        [--key=basename|dirname|path] [PATH ...]


``path sort`` returns the given paths in sorted order. They are sorted in the same order as globs - alphabetically, but with runs of numerical digits compared numerically.

With ``--reverse`` or ``-r`` the sort is reversed.

With ``--key=`` only the given part of the path is compared, e.g. ``--key=dirname`` causes only the dirname to be compared, ``--key=basename`` only the basename and ``--key=path`` causes the entire path to be compared (this is the default).

With ``--unique`` or ``-u`` the sort is deduplicated, meaning only the first of a run that have the same key is kept. So if you are sorting by basename, then only the first of each basename is used.

The sort used is stable, so sorting first by basename and then by dirname works and causes the files to be grouped according to directory.

It currently returns 0 if it was given any paths.

Examples
^^^^^^^^

::

   >_ path sort 10-foo 2-bar
   2-bar
   10-foo

   >_ path sort --reverse 10-foo 2-bar
   10-foo
   2-bar

   >_ path sort --unique --key=basename $fish_function_path/*.fish
   # prints a list of all function files fish would use, sorted by name.


Combining ``path``
-------------------

``path`` is meant to be easy to combine with itself, other tools and fish.

This is why

- ``path``'s output is automatically split by fish if it goes into a command substitution, so just doing ``(path ...)`` handles all paths, even those containing newlines, correctly
- ``path`` has ``--null-in`` to handle null-delimited input (typically automatically detected!), and ``--null-out`` to pass on null-delimited output

Some examples of combining ``path``::

  # Expand all paths in the current directory, leave only executable files, and print their resolved path
  path filter -zZ -xf -- * | path resolve -z

  # The same thing, but using find (note -maxdepth needs to come first or find will scream)
  # (this also depends on your particular version of find)
  # Note the `-z` is unnecessary for any sensible version of find - if `path` sees a NULL,
  # it will split on NULL automatically.
  find . -maxdepth 1 -type f -executable -print0 | path resolve -z

  set -l paths (path filter -p exec $PATH/fish -Z | path resolve)
