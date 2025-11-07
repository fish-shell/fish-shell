fish_config - start the web-based configuration interface
=========================================================

Synopsis
--------

.. synopsis::

    fish_config [browse]
    fish_config prompt (choose | list | save | show)
    fish_config theme (choose | demo | dump | list | save | show)

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

- ``choose`` loads a sample theme in the current session.
- ``demo`` displays some sample text in the current theme.
- ``dump`` prints the current theme in a loadable format.
- ``list`` lists the names of the available sample themes.
- ``save`` saves the given theme to :ref:`universal variables <variables-universal>`.
- ``show`` shows what the given sample theme (or all) would look like.

The **-h** or **--help** option displays help about using this command.

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

   # name: 'Cool Beans'
   # preferred_background: black

   fish_color_autosuggestion 666
   fish_color_cancel -r
   fish_color_command normal
   fish_color_comment '888'  '--italics'
   fish_color_cwd 0A0
   fish_color_cwd_root A00
   fish_color_end 009900

The two comments at the beginning are the name and background that the web config tool shows.

The other lines are just like ``set variable value``, except that no expansions are allowed. Quotes are, but aren't necessary.

Any color variable fish knows about that the theme doesn't set will be set to empty when it is loaded, so the old theme is completely overwritten.

Other than that, .theme files can contain any variable with a name that matches the regular expression ``'^fish_(?:pager_)?color.*$'`` - starts with ``fish_``, an optional ``pager_``, then ``color`` and then anything.

Example
-------

``fish_config`` or ``fish_config browse`` opens a new web browser window and allows you to configure certain fish settings.

``fish_config prompt show`` demos the available sample prompts.

``fish_config prompt choose disco`` makes the disco prompt the prompt for the current session. This can also be used in :ref:`config.fish <configuration>` to set the prompt.

``fish_config prompt save`` saves the current prompt to an :ref:`autoloaded <syntax-function-autoloading>` file.

``fish_config prompt save default`` chooses the default prompt and saves it.
