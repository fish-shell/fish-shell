.. _cmd-fish_transient_right_prompt:

fish_transient_right_prompt - define the appearance of the transient right-side command line prompt
===================================================================================================

Synopsis
--------

::

  function fish_transient_right_prompt
      ...
  end


Description
-----------

``fish_transient_right_prompt`` is similar to ``fish_right_prompt``, except that it is transient.
That is removed from display when accepting a command line.
This may be useful with terminals with other cut/paste methods. 
Corresponds to `RPROMPT` with setting `TRANSIENT_RPROMPT` in zsh.

Multiple lines are not supported in ``fish_transient_right_prompt``.


Example
-------

A simple transient right prompt:



::

    function fish_transient_right_prompt -d "Write out the transient right prompt"
        date '+%m/%d/%y'
    end


