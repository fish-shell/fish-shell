help - display fish documentation
=================================

Synopsis
--------

.. synopsis::

    help [SECTION]

Description
-----------

``help`` displays the fish help documentation.

If a *SECTION* is specified, the help for that command is shown.

The **-h** or **--help** option displays help about using this command.

If the :envvar:`BROWSER` environment variable is set, it will be used to display the documentation.
Otherwise, fish will search for a suitable browser.
To use a different browser than as described above, you can set ``$fish_help_browser``
This variable may be set as a list, where the first element is the browser command and the rest are browser options.

Example
-------

``help fg`` shows the documentation for the :doc:`fg <fg>` builtin.

Notes
-----

Most builtin commands, including this one, display their help in the terminal when given the **--help** option.
