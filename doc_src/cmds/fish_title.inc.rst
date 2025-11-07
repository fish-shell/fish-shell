Synopsis
--------

.. synopsis::

    fish_title
    fish_tab_title

::

  function fish_title
      ...
  end


  function fish_tab_title
      ...
  end


Description
-----------

The ``fish_title`` function is executed before and after a new command is executed or put into the foreground and the output is used as a titlebar message.

The first argument to ``fish_title`` contains the most recently executed foreground command as a string, if any.

This requires that your terminal supports :ref:`programmable titles <term-compat-osc-0>` and the feature is turned on.

To disable setting the title, use an empty function (see below).

To set the terminal tab title to something other than the terminal window title,
define the ``fish_tab_title`` function, which works like ``fish_title`` but overrides that one.

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

Change the tab title only::

    function fish_tab_title
        echo fish $fish_pid
    end
