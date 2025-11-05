wait - wait for jobs to complete
================================

Synopsis
--------

.. synopsis::

    wait [-n | --any] [PID | PROCESS_NAME] ...

Description
-----------

.. only:: builder_man

          NOTE: This page documents the fish builtin ``wait``.
          To see the documentation on any non-fish versions, use ``command man wait``.

``wait`` waits for child jobs to complete.

If a *PID* is specified, the command waits for the job that the process with that process ID belongs to.

If a *PROCESS_NAME* is specified, the command waits for the jobs that the matched processes belong to.

If neither a pid nor a process name is specified, the command waits for all background jobs.

If the **-n** or **--any** flag is provided, the command returns as soon as the first job completes. If it is not provided, it returns after all jobs complete.

The **-h** or **--help** option displays help about using this command.

Example
-------

::

    sleep 10 &
    wait $last_pid

spawns ``sleep`` in the background, and then waits until it finishes.


::

    for i in (seq 1 5); sleep 10 &; end
    wait

spawns five jobs in the background, and then waits until all of them finish.


::

    for i in (seq 1 5); sleep 10 &; end
    hoge &
    wait sleep

spawns five ``sleep`` jobs and ``hoge`` in the background, and then waits until all ``sleep``\s finish, and doesn't wait for ``hoge``.
