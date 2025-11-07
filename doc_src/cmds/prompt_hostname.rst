prompt_hostname - print the hostname, shortened for use in the prompt
=====================================================================

Synopsis
--------

.. synopsis::

    prompt_hostname

Description
-----------

``prompt_hostname`` prints a shortened version the current hostname for use in the prompt. It will print just the first component of the hostname, everything up to the first dot.

Examples
--------

::

    function fish_prompt
        echo -n (whoami)@(prompt_hostname) (prompt_pwd) '$ '
    end

::

    # The machine's full hostname is foo.bar.com
    >_ prompt_hostname
    foo
