fish_config - start the web-based configuration interface
=========================================================

Synopsis
--------

.. synopsis::

    fish_config [browse]
    fish_config prompt (choose | list | save | show)
    fish_config theme
    fish_config theme choose THEME [ --color-theme=(dark | light) ]
    fish_config theme demo
    fish_config theme dump
    fish_config theme list
    fish_config theme show [THEME...]

Description
-----------

``fish_config`` is used to configure fish.

Without arguments or with the ``browse`` command it starts the web-based configuration interface. The web interface allows you to view your functions, variables and history, and to make changes to your prompt and color configuration. It starts a local web server and opens a browser window. When you are finished, close the browser window and press the Enter key to terminate the configuration session.

If the ``BROWSER`` environment variable is set, it will be used as the name of the web browser to open instead of the system default.

With the ``prompt`` command ``fish_config`` can be used to view and choose a prompt from fish's sample prompts inside the terminal directly.

Available subcommands for the ``prompt`` command:

- ``choose`` loads a sample prompt in the current session.
- ``list`` lists the names of the available sample prompts.
- ``save`` saves the current prompt to a file (via :doc:`funcsave <funcsave>`).
- ``show`` shows what the given sample prompts (or all) would look like.

With the ``theme`` command ``fish_config`` can be used to view and choose a theme (meaning a color scheme) inside the terminal.

Available subcommands for the ``theme`` command:

- ``choose`` loads a theme in the current session.
  If the theme has light and dark variants (see below), the one matching :envvar:`fish_terminal_color_theme` will be applied (also whenever that variable changes).
  To override :envvar:`fish_terminal_color_theme`, pass the ``--color-theme`` argument.
- ``demo`` displays some sample text in the current theme.
- ``dump`` prints the current theme in a loadable format.
- ``list`` lists the names of the available themes.
- ``show`` shows what the given themes (or all) would look like.
- *(not recommended)* ``save`` saves the given theme to :ref:`universal variables <variables-universal>`.
  A theme set this way will not update as :envvar:`fish_terminal_color_theme` changes.

The **-h** or **--help** option displays help about using this command.

.. _fish-config-theme-files:

Theme Files
-----------

``fish_config theme`` and the theme selector in the web config tool load their themes from theme files. These are stored in the fish configuration directory, typically ``~/.config/fish/themes``, with a .theme ending.

You can add your own theme by adding a file in that directory.

To get started quickly::

  fish_config theme dump > ~/.config/fish/themes/my.theme

which will save your current theme in .theme format.

The format looks like this:

.. highlight:: none

::

    # name: 'My Theme'

    [light]
    # preferred_background: ffffff
    fish_color_normal 000000
    fish_color_autosuggestion 7f7f7f
    fish_color_command 0000ee

    [dark]
    # preferred_background: 000000
    fish_color_normal ffffff
    fish_color_autosuggestion 7f7f7f
    fish_color_command 5c5cff

    [unknown]
    fish_color_normal normal
    fish_color_autosuggestion brblack
    fish_color_cancel -r
    fish_color_command normal

The comments provide name and background color to the web config tool.

Themes can have three variants,
one for light mode,
one for dark mode,
and one for terminals that don't :ref:`report colors <term-compat-query-background-color>` (where :envvar:`fish_terminal_color_theme` is set to ``unknown``).

The other lines are just like ``set variable value``, except that no expansions are allowed. Quotes are, but aren't necessary.

.. _fish_config-color-variables:

Other than that, .theme files can contain any variable with a name that matches the regular expression ``'^fish_(?:pager_)?color_.*$'`` - starts with ``fish_``, an optional ``pager_``, then ``color_`` and then anything.

Example
-------

``fish_config`` or ``fish_config browse`` opens a new web browser window and allows you to configure certain fish settings.

``fish_config prompt show`` demos the available sample prompts.

``fish_config prompt choose disco`` makes the disco prompt the prompt for the current session. This can also be used in :ref:`config.fish <configuration>` to set the prompt.

``fish_config prompt save`` saves the current prompt to an :ref:`autoloaded <syntax-function-autoloading>` file.

``fish_config prompt save default`` chooses the default prompt and saves it.
