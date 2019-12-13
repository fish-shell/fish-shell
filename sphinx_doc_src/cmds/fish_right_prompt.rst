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

``fish`` passes ``before_input`` or ``after_input`` as a parameter to ``fish_right_prompt`` depending on the timing of calling.

Multiple lines are not supported in ``fish_right_prompt``.


Examples
--------

A simple right prompt:



::

    function fish_right_prompt -d "Write out the right prompt"
        date '+%m/%d/%y'
    end



A simple transient right prompt:



::

    function fish_right_prompt -a input_phase -d "Write out the transient right prompt"
        test $input_phase = "before_input"
            and date '+%m/%d/%y'
    end
