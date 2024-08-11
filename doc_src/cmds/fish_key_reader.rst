.. _cmd-fish_key_reader:

fish_key_reader - explore what characters keyboard keys send
============================================================

Synopsis
--------

.. synopsis::

    fish_key_reader [OPTIONS]

Description
-----------

:program:`fish_key_reader` is used to explain how you would bind a certain key sequence. By default, it prints the :doc:`bind <bind>` command for one key sequence read interactively over standard input.

The following options are available:

**-c** or **--continuous**
    Begins a session where multiple key sequences can be inspected. By default the program exits after capturing a single key sequence.

**-h** or **--help**
    Displays help about using this command.

**-V** or **--verbose**
    Explain what sequence was received in addition to the decoded key.

**-v** or **--version**
    Displays the current :program:`fish` version and then exits.

Usage Notes
-----------

``fish_key_reader`` intentionally disables handling of many signals. To terminate ``fish_key_reader`` in ``--continuous`` mode do:

- press :kbd:`ctrl-c` twice, or
- press :kbd:`ctrl-d` twice, or
- type ``exit``, or
- type ``quit``

Example
-------

::

   > fish_key_reader
   Press a key:
   # press up-arrow
   bind up 'do something'
