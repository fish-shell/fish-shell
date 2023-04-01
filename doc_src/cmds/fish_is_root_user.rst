.. _cmd-ghoti_is_root_user:

ghoti_is_root_user - check if the current user is root
=====================================================

Synopsis
--------

.. synopsis::

    ghoti_is_root_user

Description
-----------

``ghoti_is_root_user`` will check if the current user is root. It can be useful
for the prompt to display something different if the user is root, for example.


Example
-------

A simple example:

::

    function example --description 'Just an example'
        if ghoti_is_root_user
            do_something_different
        end
    end
