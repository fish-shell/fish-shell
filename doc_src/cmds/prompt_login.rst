prompt_login - describe the login suitable for prompt
=====================================================

Synopsis
--------

.. synopsis::

    prompt_login

Description
-----------

``prompt_login`` is a function to describe the current login. It will show the user, the host and also whether the shell is running in a chroot (currently Debian's ``debian_chroot`` file is supported).

Examples
--------
::

    function fish_prompt
        echo -n (prompt_login) (prompt_pwd) '$ '
    end

::

    >_ prompt_login
    root@bananablaster
