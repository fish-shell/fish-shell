.. _cmd-funcdel:

funcdel - remove the definition of a function from the user's autoload directory
=============================================================================

Synopsis
--------

.. synopsis::

    funcdel FUNCTION_NAME
    funcdel [-q | --quiet] [(-d | --directory) DIR] FUNCTION_NAME


Description
-----------

``funcdel`` erases a user function from the current session and removes the file from the fish configuration directory. This can be useful to remove saved functions permanently.

This is often used after renaming an existing user function to clean up the function file with the old name.
