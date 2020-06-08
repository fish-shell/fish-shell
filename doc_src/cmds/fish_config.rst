.. _cmd-fish_config:

fish_config - start the web-based configuration interface
=========================================================

Synopsis
--------

::

    fish_config [TAB]

Description
-----------

``fish_config`` starts the web-based configuration interface.

The web interface allows you to view your functions, variables and history, and to make changes to your prompt and color configuration.

``fish_config`` starts a local web server and then opens a web browser window; when you have finished, close the browser window and then press the Enter key to terminate the configuration session.

``fish_config`` optionally accepts name of the initial configuration tab. For e.g. ``fish_config history`` will start configuration interface with history tab.

If the ``BROWSER`` environment variable is set, it will be used as the name of the web browser to open instead of the system default.


Example
-------

``fish_config`` opens a new web browser window and allows you to configure certain fish settings.
