.. SPDX-FileCopyrightText: © 2012 fish-shell contributors
..
.. SPDX-License-Identifier: GPL-2.0-only

.. _cmd-fish_right_prompt:

fish_right_prompt - define the appearance of the right-side command line prompt
===============================================================================

Synopsis
--------

::

  function fish_right_prompt
      ...
  end


Description
-----------

``fish_right_prompt`` is similar to ``fish_prompt``, except that it appears on the right side of the terminal window.

Multiple lines are not supported in ``fish_right_prompt``.


Example
-------

A simple right prompt:



::

    function fish_right_prompt -d "Write out the right prompt"
        date '+%m/%d/%y'
    end


