.. _cmd-history:

history - Show and manipulate command history
=============================================

Synopsis
--------

::

    history search [ --show-time ] [ --case-sensitive ] [ --exact | --prefix | --contains ] [ --max=n ] [ --null ] [ -R | --reverse ] [ "search string"... ]
    history delete [ --show-time ] [ --case-sensitive ] [ --exact | --prefix | --contains ] "search string"...
    history merge
    history save
    history clear
    history ( -h | --help )

Description
-----------

``history`` is used to search, delete, and otherwise manipulate the :ref:`history of interactive commands <history-search>`.

The following operations (sub-commands) are available:

- ``search`` returns history items matching the search string. If no search string is provided it returns all history items. This is the default operation if no other operation is specified. You only have to explicitly say ``history search`` if you wish to search for one of the subcommands. The ``--contains`` search option will be used if you don't specify a different search option. Entries are ordered newest to oldest unless you use the ``--reverse`` flag. If stdout is attached to a tty the output will be piped through your pager by the history function. The history builtin simply writes the results to stdout.

- ``delete`` deletes history items. The ``--contains`` search option will be used if you don't specify a different search option. If you don't specify ``--exact`` a prompt will be displayed before any items are deleted asking you which entries are to be deleted. You can enter the word "all" to delete all matching entries. You can enter a single ID (the number in square brackets) to delete just that single entry. You can enter more than one ID separated by a space to delete multiple entries. Just press [enter] to not delete anything. Note that the interactive delete behavior is a feature of the history function. The history builtin only supports ``--exact --case-sensitive`` deletion.

- ``merge`` immediately incorporates history changes from other sessions. Ordinarily ``fish`` ignores history changes from sessions started after the current one. This command applies those changes immediately.

- ``save`` immediately writes all changes to the history file. The shell automatically saves the history file; this option is provided for internal use and should not normally need to be used by the user.

- ``clear`` clears the history file. A prompt is displayed before the history is erased asking you to confirm you really want to clear all history unless ``builtin history`` is used.

The following options are available:

These flags can appear before or immediately after one of the sub-commands listed above.

- ``-C`` or ``--case-sensitive`` does a case-sensitive search. The default is case-insensitive. Note that prior to fish 2.4.0 the default was case-sensitive.

- ``-c`` or ``--contains`` searches or deletes items in the history that contain the specified text string. This is the default for the ``--search`` flag. This is not currently supported by the ``delete`` subcommand.

- ``-e`` or ``--exact`` searches or deletes items in the history that exactly match the specified text string. This is the default for the ``delete`` subcommand. Note that the match is case-insensitive by default. If you really want an exact match, including letter case, you must use the ``-C`` or ``--case-sensitive`` flag.

- ``-p`` or ``--prefix`` searches or deletes items in the history that begin with the specified text string. This is not currently supported by the ``--delete`` flag.

- ``-t`` or ``--show-time`` prepends each history entry with the date and time the entry was recorded. By default it uses the strftime format ``# %c%n``. You can specify another format; e.g., ``--show-time="%Y-%m-%d %H:%M:%S "`` or ``--show-time="%a%I%p"``. The short option, ``-t``, doesn't accept a strftime format string; it only uses the default format. Any strftime format is allowed, including ``%s`` to get the raw UNIX seconds since the epoch.

- ``-z`` or ``--null`` causes history entries written by the search operations to be terminated by a NUL character rather than a newline. This allows the output to be processed by ``read -z`` to correctly handle multiline history entries.

- ``-<number>`` ``-n <number>`` or ``--max=<number>`` limits the matched history items to the first "n" matching entries. This is only valid for ``history search``.

- ``-R`` or ``--reverse`` causes the history search results to be ordered oldest to newest. Which is the order used by most shells. The default is newest to oldest.

- ``-h`` or ``--help`` display help for this command.

Example
-------



::

    history clear
    # Deletes all history items
    
    history search --contains "foo"
    # Outputs a list of all previous commands containing the string "foo".
    
    history delete --prefix "foo"
    # Interactively deletes commands which start with "foo" from the history.
    # You can select more than one entry by entering their IDs separated by a space.


Customizing the name of the history file
----------------------------------------

By default interactive commands are logged to ``$XDG_DATA_HOME/fish/fish_history`` (typically ``~/.local/share/fish/fish_history``).

You can set the ``fish_history`` variable to another name for the current shell session. The default value (when the variable is unset) is ``fish`` which corresponds to ``$XDG_DATA_HOME/fish/fish_history``. If you set it to e.g. ``fun``, the history would be written to ``$XDG_DATA_HOME/fish/fun_history``. An empty string means history will not be stored at all. This is similar to the private session features in web browsers.

You can change ``fish_history`` at any time (by using ``set -x fish_history "session_name"``) and it will take effect right away. If you set it to ``"default"``, it will use the default session name (which is ``"fish"``).

Other shells such as bash and zsh use a variable named ``HISTFILE`` for a similar purpose. Fish uses a different name to avoid conflicts and signal that the behavior is different (session name instead of a file path). Also, if you set the var to anything other than ``fish`` or ``default`` it will inhibit importing the bash history. That's because the most common use case for this feature is to avoid leaking private or sensitive history when giving a presentation.

Notes
-----

If you specify both ``--prefix`` and ``--contains`` the last flag seen is used.

Note that for backwards compatibility each subcommand can also be specified as a long option. For example, rather than ``history search`` you can type ``history --search``. Those long options are deprecated and will be removed in a future release.
