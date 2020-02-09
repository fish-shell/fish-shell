.. _cmd-vared:

vared - interactively edit the value of an environment variable
===============================================================

Synopsis
--------

::

    vared VARIABLE_NAME

Description
-----------

``vared`` is used to interactively edit the value of an environment variable. Array variables as a whole can not be edited using ``vared``, but individual list elements can.


Example
-------

``vared PATH[3]`` edits the third element of the PATH list
