.. SPDX-FileCopyrightText: © 2005 Axel Liljencrantz
..
.. SPDX-License-Identifier: GPL-2.0-only

.. _cmd-vared:

vared - interactively edit the value of an environment variable
===============================================================

Synopsis
--------

.. synopsis::

    vared VARIABLE_NAME

Description
-----------

``vared`` is used to interactively edit the value of an environment variable. Array variables as a whole can not be edited using ``vared``, but individual list elements can.

The **-h** or **--help** option displays help about using this command.

Example
-------

``vared PATH[3]`` edits the third element of the PATH list
