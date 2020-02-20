.. _cmd-prompt_pwd:

prompt_pwd - Print pwd suitable for prompt
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

To change the number of characters per path component, set ``$fish_prompt_pwd_dir_length`` to the number of characters. Setting it to 0 or an invalid value will disable shortening entirely.

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
