.. _cmd-fish_prompt:

fish_prompt - define the appearance of the command line prompt
==============================================================

Synopsis
--------

::

  function fish_prompt
      ...
  end


Description
-----------

By defining the ``fish_prompt`` function, the user can choose a custom prompt. The ``fish_prompt`` function is executed when the prompt is to be shown, and the output is used as a prompt.

The exit status of commands within ``fish_prompt`` will not modify the value of :ref:`$status <variables-status>` outside of the ``fish_prompt`` function.

``fish`` ships with a number of example prompts that can be chosen with the ``fish_config`` command.


Example
-------

A simple prompt:



::

    function fish_prompt -d "Write out the prompt"
        printf '%s@%s%s%s%s> ' (whoami) (hostname | cut -d . -f 1) \
        		(set_color $fish_color_cwd) (prompt_pwd) (set_color normal)
    end


