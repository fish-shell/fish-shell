.. _cmd-fish_title:

fish_title - define the terminal's title
========================================

Synopsis
--------

.. synopsis::

    fish_title

::

  function fish_title
      ...
  end


Description
-----------

The ``fish_title`` function is executed before and after a new command is executed or put into the foreground and the output is used as a titlebar message.

The first argument to fish_title contains the most recently executed foreground command as a string, if any.

This requires that your terminal supports programmable titles and the feature is turned on.

To disable setting the title, use an empty function (see below).

Example
-------

A simple title::

   function fish_title
       set -q argv[1]; or set argv fish
       # Looks like ~/d/fish: git log
       # or /e/apt: fish
       echo (fish_prompt_pwd_dir_length=1 prompt_pwd): $argv; 
   end

Do not change the title::

   function fish_title
   end
