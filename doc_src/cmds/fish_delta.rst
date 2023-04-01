ghoti_delta - compare functions and completions to the default
==============================================================

Synopsis
--------

.. synopsis::

    ghoti_delta name ...
    ghoti_delta [-f | --no-functions] [-c | --no-completions] [-C | --no-config] [-d | --no-diff] [-n | --new] [-V | --vendor=]
    ghoti_delta [-h | --help]


Description
-----------

The ``ghoti_delta`` function tells you, at a glance, which of your functions and completions differ from the set that ghoti ships.

It does this by going through the relevant variables (:envvar:`ghoti_function_path` for functions and :envvar:`ghoti_complete_path` for completions) and comparing the files against ghoti's default directories.

If any names are given, it will only compare files by those names (plus a ".ghoti" extension).

By default, it will also use ``diff`` to display the difference between the files. If ``diff`` is unavailable, it will skip it, but in that case it also cannot figure out if the files really differ.

The exit status is 1 if there was a difference and 2 for other errors, otherwise 0.

Options
-------

The following options are available:

**-f** or **--no-functions**
    Stops checking functions

**-c** or **--no-completions**
    Stops checking completions

**-C** or **--no-config**
    Stops checking configuration files like config.ghoti or snippets in the conf.d directories.

**-d** or **--no-diff**
    Removes the diff display (this happens automatically if ``diff`` can't be found)

**-n** or **--new**
    Also prints new files (i.e. those that can't be found in ghoti's default directories).

**-Vvalue** or **--vendor=value**
   Determines how the vendor directories are counted. Valid values are:

   - "default" - counts vendor files as belonging to the defaults. Any changes in other directories will be counted as changes over them. This is the default.
   - "user" - counts vendor files as belonging to the user files. Any changes in them will be counted as new or changed files.
   - "ignore" - ignores vendor directories. Files of the same name will be counted as "new" if no file of the same name in ghoti's default directories exists.

**-h** or **--help**
    Prints ``ghoti_delta``'s help (this).

Example
-------

Running just::

  ghoti_delta

will give you a list of all your changed functions and completions, including diffs (if you have the ``diff`` command).

It might look like this::

  > ghoti_delta
  New: /home/alfa/.config/ghoti/functions/battery.ghoti
  Changed: /home/alfa/.config/ghoti/test/completions/cargo.ghoti
  --- /home/alfa/.config/ghoti/test/completions/cargo.ghoti 2022-09-02 12:57:55.579229959 +0200
  +++ /usr/share/ghoti/completions/cargo.ghoti      2022-09-25 17:51:53.000000000 +0200
  # the output of `diff` follows

The options are there to select which parts of the output you want. With ``--no-completions`` you can compare just functions, and with ``--no-diff`` you can turn off the ``diff`` display.

To only compare your ``ghoti_git_prompt``, you might use::

  ghoti_delta --no-completions ghoti_git_prompt

which will only compare files called "ghoti_git_prompt.ghoti".
