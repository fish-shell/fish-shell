.. _cmd-prompt_pwd:

prompt_pwd - print pwd suitable for prompt
==========================================

Synopsis
--------

::

    function fish_prompt
        echo -n (prompt_pwd) '$ '
    end

Description
-----------

``prompt_pwd`` is a function to print the current working directory in a way suitable for prompts. It will replace the home directory with "~" and shorten every path component but the last to a default of one character.

To change the number of characters per path component, pass ``--dir-length=`` or set ``$fish_prompt_pwd_dir_length`` to the number of characters. Setting it to 0 or an invalid value will disable shortening entirely. This defaults to 1.

To keep some components unshortened, pass ``--full-length-dirs=`` or set ``$fish_prompt_pwd_full_dirs`` to the number of components. This defaults to 1, keeping the last component.

If any positional arguments are given, prompt_pwd shortens them instead of $PWD.

Options
-------

- ``-h`` or ``--help`` displays the help and exits
- ``-d`` or ``--dir-length=MAX`` causes the components to be shortened to MAX characters each. This overrides $fish_prompt_pwd_dir_length.
- ``-D`` or ``--full-length-dirs=NUM`` keeps NUM components (counted from the right) as full length without shortening. This overrides $fish_prompt_pwd_full_dirs.

Examples
--------

::

    >_ cd ~/
    >_ echo $PWD
    /home/alfa

    >_ prompt_pwd
    ~

    >_ cd /tmp/banana/sausage/with/mustard
    >_ prompt_pwd
    /t/b/s/w/mustard

    >_ set -g fish_prompt_pwd_dir_length 3
    >_ prompt_pwd
    /tmp/ban/sau/wit/mustard

    >_ prompt_pwd --full-length-dirs=2 --dir-length=1
    /t/b/s/with/mustard
