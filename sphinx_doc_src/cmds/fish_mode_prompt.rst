.. _cmd-fish_mode_prompt:

fish_mode_prompt - define the appearance of the mode indicator
==============================================================

Synopsis
--------

::

     function fish_mode_prompt
          echo -n "$fish_bind_mode "
     end


Description
-----------

The ``fish_mode_prompt`` function outputs the mode indicator for use in vi-mode.

The default ``fish_mode_prompt`` function will output indicators about the current Vi editor mode displayed to the left of the regular prompt. Define your own function to customize the appearance of the mode indicator. You can also define an empty ``fish_mode_prompt`` function to remove the Vi mode indicators. The ``$fish_bind_mode variable`` can be used to determine the current mode. It
will be one of ``default``, ``insert``, ``replace_one``, or ``visual``.

fish_mode_prompt will be executed when the vi mode changes. If it produces any output, it is displayed and used. If it does not, the other prompt functions (:ref:`fish_prompt <cmd-fish_prompt>` and :ref:`fish_right_prompt <cmd-fish_right_prompt>`) will be executed as well in case they contain a mode display.

Example
-------



::

    function fish_mode_prompt
      switch $fish_bind_mode
        case default
          set_color --bold red
          echo 'N'
        case insert
          set_color --bold green
          echo 'I'
        case replace_one
          set_color --bold green
          echo 'R'
        case visual
          set_color --bold brmagenta
          echo 'V'
        case '*'
          set_color --bold red
          echo '?'
      end
      set_color normal
    end


Outputting multiple lines is not supported in ``fish_mode_prompt``.
