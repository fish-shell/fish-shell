.. _cmd-ghoti_right_prompt:

ghoti_right_prompt - define the appearance of the right-side command line prompt
===============================================================================

Synopsis
--------

::

  function ghoti_right_prompt
      ...
  end


Description
-----------

``ghoti_right_prompt`` is similar to ``ghoti_prompt``, except that it appears on the right side of the terminal window.

Multiple lines are not supported in ``ghoti_right_prompt``.


Example
-------

A simple right prompt:



::

    function ghoti_right_prompt -d "Write out the right prompt"
        date '+%m/%d/%y'
    end


