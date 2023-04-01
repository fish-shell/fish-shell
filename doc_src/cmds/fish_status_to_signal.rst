.. _cmd-ghoti_status_to_signal:

ghoti_status_to_signal - convert exit codes to human-friendly signals
====================================================================

Synopsis
--------

.. synopsis::

    ghoti_status_to_signal NUM

::

    function ghoti_prompt
        echo -n (ghoti_status_to_signal $pipestatus | string join '|') (prompt_pwd) '$ '
    end

Description
-----------

``ghoti_status_to_signal`` converts exit codes to their corresponding human-friendly signals if one exists.
This is likely to be useful for prompts in conjunction with the ``$status`` and ``$pipestatus`` variables.

Example
-------

::

    >_ sleep 5
    ^C⏎
    >_ ghoti_status_to_signal $status
    SIGINT
