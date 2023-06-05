.. _cmd-bg:

bg - send jobs to background
============================

Synopsis
--------

.. synopsis::

    bg [PID ...]

Description
-----------

``bg`` sends :ref:`jobs <syntax-job-control>` to the background, resuming them if they are stopped.

A background job is executed simultaneously with fish, and does not have access to the keyboard. If no job is specified, the last job to be used is put in the background. If ``PID`` is specified, the jobs containing the specified process IDs are put in the background.

A PID of the format ``%n``, where n is an integer, will be interpreted as the PID of job number n. Job numbers can be seen in the output of :doc:`jobs <jobs>`.

When at least one of the arguments isn't a valid job specifier, ``bg`` will print an error without backgrounding anything.

When all arguments are valid job specifiers, ``bg`` will background all matching jobs that exist.

The **-h** or **--help** option displays help about using this command.

Example
-------

The typical use is to run something, stop it with ctrl-z, and then continue it in the background with bg::

  > find / -name "*.js" >/tmp/jsfiles 2>/dev/null # oh no, this takes too long, let's press Ctrl-z!
  fish: Job 1, 'find / -name "*.js" >/tmp/jsfil…' has stopped
  > bg
  Send job 1 'find / -name "*.js" >/tmp/jsfiles 2>/dev/null' to background
  > # I can continue using this shell!
  > # Eventually:
  fish: Job 1, 'find / -name "*.js" >/tmp/jsfil…' has ended

``bg 123 456 789`` will background the jobs that contain processes 123, 456 and 789.

If only 123 and 789 exist, it will still background them and print an error about 456.

``bg 123 banana`` or ``bg banana 123`` will complain that "banana" is not a valid job specifier.

``bg %2`` will background job 2.
