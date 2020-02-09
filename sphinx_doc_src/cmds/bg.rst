.. _cmd-bg:

bg - send jobs to background
============================

Synopsis
--------

::

    bg [PID...]

Description
-----------

``bg`` sends :ref:`jobs <syntax-job-control>` to the background, resuming them if they are stopped.

A background job is executed simultaneously with fish, and does not have access to the keyboard. If no job is specified, the last job to be used is put in the background. If PID is specified, the jobs with the specified process group IDs are put in the background.

When at least one of the arguments isn't a valid job specifier (i.e. PID),
``bg`` will print an error without backgrounding anything.

When all arguments are valid job specifiers, bg will background all matching jobs that exist.

Example
-------

``bg 123 456 789`` will background 123, 456 and 789.

If only 123 and 789 exist, it will still background them and print an error about 456.

``bg 123 banana`` or ``bg banana 123`` will complain that "banana" is not a valid job specifier.
