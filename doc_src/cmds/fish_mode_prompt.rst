.. _cmd-ghoti_mode_prompt:

ghoti_mode_prompt - define the appearance of the mode indicator
==============================================================

Synopsis
--------

.. synopsis::

    ghoti_mode_prompt

::

     function ghoti_mode_prompt
          echo -n "$ghoti_bind_mode "
     end


Description
-----------

The ``ghoti_mode_prompt`` function outputs the mode indicator for use in vi-mode.

The default ``ghoti_mode_prompt`` function will output indicators about the current Vi editor mode displayed to the left of the regular prompt. Define your own function to customize the appearance of the mode indicator. The ``$ghoti_bind_mode variable`` can be used to determine the current mode. It will be one of ``default``, ``insert``, ``replace_one``, or ``visual``.

You can also define an empty ``ghoti_mode_prompt`` function to remove the Vi mode indicators::

    function ghoti_mode_prompt; end
    funcsave ghoti_mode_prompt

``ghoti_mode_prompt`` will be executed when the vi mode changes. If it produces any output, it is displayed and used. If it does not, the other prompt functions (:doc:`ghoti_prompt <ghoti_prompt>` and :doc:`ghoti_right_prompt <ghoti_right_prompt>`) will be executed as well in case they contain a mode display.

Example
-------



::

    function ghoti_mode_prompt
      switch $ghoti_bind_mode
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


Outputting multiple lines is not supported in ``ghoti_mode_prompt``.
