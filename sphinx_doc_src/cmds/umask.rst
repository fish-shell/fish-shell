.. _cmd-umask:

umask - set or get the file creation mode mask
==============================================

Synopsis
--------

::

    umask [OPTIONS] [MASK]


Description
-----------

``umask`` displays and manipulates the "umask", or file creation mode mask, which is used to restrict the default access to files.

The umask may be expressed either as an octal number, which represents the rights that will be removed by default, or symbolically, which represents the only rights that will be granted by default.

Access rights are explained in the manual page for the ``chmod(1)`` program.

With no parameters, the current file creation mode mask is printed as an octal number.

- ``-h`` or ``--help`` prints this message.

- ``-S`` or ``--symbolic`` prints the umask in symbolic form instead of octal form.

- ``-p`` or ``--as-command`` outputs the umask in a form that may be reused as input

If a numeric mask is specified as a parameter, the current shell's umask will be set to that value, and the rights specified by that mask will be removed from new files and directories by default.

If a symbolic mask is specified, the desired permission bits, and not the inverse, should be specified. A symbolic mask is a comma separated list of rights. Each right consists of three parts:

- The first part specifies to whom this set of right applies, and can be one of ``u``, ``g``, ``o`` or ``a``, where ``u`` specifies the user who owns the file, ``g`` specifies the group owner of the file, ``o`` specific other users rights and ``a`` specifies all three should be changed.

- The second part of a right specifies the mode, and can be one of ``=``, ``+`` or ``-``, where ``=`` specifies that the rights should be set to the new value, ``+`` specifies that the specified right should be added to those previously specified and ``-`` specifies that the specified rights should be removed from those previously specified.

- The third part of a right specifies what rights should be changed and can be any combination of ``r``, ``w`` and ``x``, representing read, write and execute rights.

If the first and second parts are skipped, they are assumed to be ``a`` and ``=``, respectively. As an example, ``r,u+w`` means all users should have read access and the file owner should also have write access.

Note that symbolic masks currently do not work as intended.


Example
-------

``umask 177`` or ``umask u=rw`` sets the file creation mask to read and write for the owner and no permissions at all for any other users.
