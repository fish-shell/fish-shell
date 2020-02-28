.. _cmd-ulimit:

ulimit - set or get resource usage limits
=========================================

Synopsis
--------

::

    ulimit [OPTIONS] [LIMIT]


Description
-----------

``ulimit`` builtin sets or outputs the resource usage limits of the shell and any processes spawned by it. If a new limit value is omitted, the current value of the limit of the resource is printed; otherwise, the specified limit is set to the new value.

Use one of the following switches to specify which resource limit to set or report:

- ``-c`` or ``--core-size``: the maximum size of core files created. By setting this limit to zero, core dumps can be disabled.

- ``-d`` or ``--data-size``: the maximum size of a process' data segment.

- ``-f`` or ``--file-size``: the maximum size of files created by the shell.

- ``-l`` or ``--lock-size``: the maximum size that may be locked into memory.

- ``-m`` or ``--resident-set-size``: the maximum resident set size.

- ``-n`` or ``--file-descriptor-count``: the maximum number of open file descriptors (most systems do not allow this value to be set).

- ``-s`` or ``--stack-size``: the maximum stack size.

- ``-t`` or ``--cpu-time``: the maximum amount of CPU time in seconds.

- ``-u`` or ``--process-count``: the maximum number of processes available to a single user.

- ``-v`` or ``--virtual-memory-size`` The maximum amount of virtual memory available to the shell.

Note that not all these limits are available in all operating systems.

The value of limit can be a number in the unit specified for the resource or one of the special values ``hard``, ``soft``, or ``unlimited``, which stand for the current hard limit, the current soft limit, and no limit, respectively.

If limit is given, it is the new value of the specified resource. If no option is given, then ``-f`` is assumed. Values are in kilobytes, except for ``-t``, which is in seconds and ``-n`` and ``-u``, which are unscaled values. The exit status is 0 unless an invalid option or argument is supplied, or an error occurs while setting a new limit.

``ulimit`` also accepts the following switches that determine what type of limit to set:

- ``-H`` or ``--hard`` sets hard resource limit

- ``-S`` or ``--soft`` sets soft resource limit

A hard limit can only be decreased. Once it is set it cannot be increased; a soft limit may be increased up to the value of the hard limit. If neither ``-H`` nor ``-S`` is specified, both the soft and hard limits are updated when assigning a new limit value, and the soft limit is used when reporting the current value.

The following additional options are also understood by ``ulimit``:

- ``-a`` or ``--all`` prints all current limits

The ``fish`` implementation of ``ulimit`` should behave identically to the implementation in bash, except for these differences:

- Fish ``ulimit`` supports GNU-style long options for all switches

- Fish ``ulimit`` does not support the ``-p`` option for getting the pipe size. The bash implementation consists of a compile-time check that empirically guesses this number by writing to a pipe and waiting for SIGPIPE. Fish does not do this because it this method of determining pipe size is unreliable. Depending on bash version, there may also be further additional limits to set in bash that do not exist in fish.

- Fish ``ulimit`` does not support getting or setting multiple limits in one command, except reporting all values using the ``-a`` switch


Example
-------

``ulimit -Hs 64`` sets the hard stack size limit to 64 kB.

