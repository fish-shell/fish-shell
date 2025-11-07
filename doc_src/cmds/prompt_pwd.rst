prompt_pwd - print pwd suitable for prompt
==========================================

Synopsis
--------

.. synopsis::

    prompt_pwd

Description
-----------

``prompt_pwd`` is a function to print the current working directory in a way suitable for prompts. It will replace the home directory with "~" and shorten every path component but the last to a default of one character.

To change the number of characters per path component, pass ``--dir-length=`` or set :envvar:`fish_prompt_pwd_dir_length` to the number of characters. Setting it to 0 or an invalid value will disable shortening entirely. This defaults to 1.

To keep some components unshortened, pass ``--full-length-dirs=`` or set :envvar:`fish_prompt_pwd_full_dirs` to the number of components. This defaults to 1, keeping the last component.

If any positional arguments are given, ``prompt_pwd`` shortens them instead of :envvar:`PWD`.

Options
-------

**-d** or **--dir-length** *MAX*
    Causes the components to be shortened to *MAX* characters each. This overrides :envvar:`fish_prompt_pwd_dir_length`.

**-D** or **--full-length-dirs** *NUM*
    Keeps *NUM* components (counted from the right) as full length without shortening. This overrides :envvar:`fish_prompt_pwd_full_dirs`.

**-h** or **--help**
    Displays help about using this command.

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

    >_ echo (prompt_pwd | string split /)[-1]
    mustard

    >_ echo (string join / (prompt_pwd | string split /)[-3..-1])
    s/with/mustard
