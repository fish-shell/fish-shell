.. _cmd-ghoti_prompt:

ghoti_prompt - define the appearance of the command line prompt
==============================================================

Synopsis
--------

.. synopsis::

    ghoti_prompt

::

  function ghoti_prompt
      ...
  end


Description
-----------

The ``ghoti_prompt`` function is executed when the prompt is to be shown, and the output is used as a prompt.

The exit status of commands within ``ghoti_prompt`` will not modify the value of :ref:`$status <variables-status>` outside of the ``ghoti_prompt`` function.

``ghoti`` ships with a number of example prompts that can be chosen with the ``ghoti_config`` command.


Example
-------

A simple prompt:



::

    function ghoti_prompt -d "Write out the prompt"
        # This shows up as USER@HOST /home/user/ >, with the directory colored
        # $USER and $hostname are set by ghoti, so you can just use them
        # instead of using `whoami` and `hostname`
        printf '%s@%s %s%s%s > ' $USER $hostname \
            (set_color $ghoti_color_cwd) (prompt_pwd) (set_color normal)
    end


