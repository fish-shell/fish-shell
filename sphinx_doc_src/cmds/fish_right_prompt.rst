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

If ``fish_right_prompt`` returns a line containing only ``TRANSIENT_RPROMPT``, right prompt will be removed from display when accepting a command line.
This may be useful with terminals with other cut/paste methods.
Corresponds to setting ``TRANSIENT_RPROMPT`` in zsh.

Example
-------

A simple right prompt:



::

    function fish_right_prompt -d "Write out the right prompt"
        date '+%m/%d/%y'
    end

A transient right prompt:



::

    function fish_right_prompt -d "Write out the transient right prompt"
        echo TRANSIENT_RPROMPT
        date '+%m/%d/%y'
    end
