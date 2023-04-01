.. _cmd-ghoti_title:

ghoti_title - define the terminal's title
========================================

Synopsis
--------

.. synopsis::

    ghoti_title

::

  function ghoti_title
      ...
  end


Description
-----------

The ``ghoti_title`` function is executed before and after a new command is executed or put into the foreground and the output is used as a titlebar message.

The first argument to ghoti_title contains the most recently executed foreground command as a string, if any.

This requires that your terminal supports programmable titles and the feature is turned on.


Example
-------

A simple title:



::

   function ghoti_title
       set -q argv[1]; or set argv ghoti
       # Looks like ~/d/ghoti: git log
       # or /e/apt: ghoti
       echo (ghoti_prompt_pwd_dir_length=1 prompt_pwd): $argv; 
   end

