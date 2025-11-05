fish_is_root_user - check if the current user is root
=====================================================

Synopsis
--------

.. synopsis::

    fish_is_root_user

Description
-----------

``fish_is_root_user`` will check if the current user is root. It can be useful
for the prompt to display something different if the user is root, for example.


Example
-------

A simple example:

::

    function example --description 'Just an example'
        if fish_is_root_user
            do_something_different
        end
    end
