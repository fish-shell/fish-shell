fish_mode_prompt - define the appearance of the mode indicator
==============================================================



The fish_mode_prompt function will output the mode indicator for use in vi-mode.

Description
-----------

The default ``fish_mode_prompt`` function will output indicators about the current Vi editor mode displayed to the left of the regular prompt. Define your own function to customize the appearance of the mode indicator. You can also define an empty ``fish_mode_prompt`` function to remove the Vi mode indicators. The ``$fish_bind_mode variable`` can be used to determine the current mode. It
will be one of ``default``, ``insert``, ``replace_one``, or ``visual``.

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
