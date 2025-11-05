fish_status_to_signal - convert exit codes to human-friendly signals
====================================================================

Synopsis
--------

.. synopsis::

    fish_status_to_signal NUM

::

    function fish_prompt
        echo -n (fish_status_to_signal $pipestatus | string join '|') (prompt_pwd) '$ '
    end

Description
-----------

``fish_status_to_signal`` converts exit codes to their corresponding human-friendly signals if one exists.
This is likely to be useful for prompts in conjunction with the ``$status`` and ``$pipestatus`` variables.

Example
-------

::

    >_ sleep 5
    ^C⏎
    >_ fish_status_to_signal $status
    SIGINT
