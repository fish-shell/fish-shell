fish_prompt - define the appearance of the command line prompt
==============================================================

Synopsis
--------

.. synopsis::

    fish_prompt

::

  function fish_prompt
      ...
  end


Description
-----------

The ``fish_prompt`` function is executed when the prompt is to be shown, and the output is used as a prompt.

The exit status of commands within ``fish_prompt`` will not modify the value of :ref:`$status <variables-status>` outside of the ``fish_prompt`` function.

If :envvar:`fish_transient_prompt` is set to 1, ``fish_prompt --final-rendering`` is run before executing the commandline.

``fish`` ships with a number of example prompts that can be chosen with the ``fish_config`` command.


Example
-------

A simple prompt:



::

    function fish_prompt -d "Write out the prompt"
        # This shows up as USER@HOST /home/user/ >, with the directory colored
        # $USER and $hostname are set by fish, so you can just use them
        # instead of using `whoami` and `hostname`
        printf '%s@%s %s%s%s > ' $USER $hostname \
            (set_color $fish_color_cwd) (prompt_pwd) (set_color normal)
    end


