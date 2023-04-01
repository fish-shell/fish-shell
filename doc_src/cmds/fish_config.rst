.. _cmd-ghoti_config:

ghoti_config - start the web-based configuration interface
=========================================================

Synopsis
--------

.. synopsis::

    ghoti_config [browse]
    ghoti_config prompt (choose | list | save | show)
    ghoti_config theme (choose | demo | dump | list | save | show)

Description
-----------

``ghoti_config`` is used to configure ghoti.

Without arguments or with the ``browse`` command it starts the web-based configuration interface. The web interface allows you to view your functions, variables and history, and to make changes to your prompt and color configuration. It starts a local web server and opens a browser window. When you are finished, close the browser window and press the Enter key to terminate the configuration session.

If the ``BROWSER`` environment variable is set, it will be used as the name of the web browser to open instead of the system default.

With the ``prompt`` command ``ghoti_config`` can be used to view and choose a prompt from ghoti's sample prompts inside the terminal directly.

Available subcommands for the ``prompt`` command:

- ``choose`` loads a sample prompt in the current session.
- ``list`` lists the names of the available sample prompts.
- ``save`` saves the current prompt to a file (via :doc:`funcsave <funcsave>`).
- ``show`` shows what the given sample prompts (or all) would look like.

With the ``theme`` command ``ghoti_config`` can be used to view and choose a theme (meaning a color scheme) inside the terminal.

Available subcommands for the ``theme`` command:

- ``choose`` loads a sample theme in the current session.
- ``demo`` displays some sample text in the current theme.
- ``dump`` prints the current theme in a loadable format.
- ``list`` lists the names of the available sample themes.
- ``save`` saves the given theme to :ref:`universal variables <variables-universal>`.
- ``show`` shows what the given sample theme (or all) would look like.

The themes are loaded from the theme directory shipped with ghoti or a ``themes`` directory in the ghoti configuration directory (typically ``~/.config/ghoti/themes``).

The **-h** or **--help** option displays help about using this command.

Example
-------

``ghoti_config`` or ``ghoti_config browse`` opens a new web browser window and allows you to configure certain ghoti settings.

``ghoti_config prompt show`` demos the available sample prompts.

``ghoti_config prompt choose disco`` makes the disco prompt the prompt for the current session. This can also be used in :ref:`config.ghoti <configuration>` to set the prompt.

``ghoti_config prompt save`` saves the current prompt to an :ref:`autoloaded <syntax-function-autoloading>` file.

``ghoti_config prompt save default`` chooses the default prompt and saves it.
