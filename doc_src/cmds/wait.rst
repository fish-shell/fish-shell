.. _cmd-wait:

wait - wait for jobs to complete
================================

Synopsis
--------

::

    wait [-n | --any] [PID | PROCESS_NAME] ...

Description
-----------

``wait`` waits for child jobs to complete.

- If a pid is specified, the command waits for the job that the process with the pid belongs to.
- If a process name is specified, the command waits for the jobs that the matched processes belong to.
- If neither a pid nor a process name is specified, the command waits for all background jobs.
- If the ``-n`` / ``--any`` flag is provided, the command returns as soon as the first job completes. If it is not provided, it returns after all jobs complete.

Example
-------



::

    sleep 10 &
    wait $last_pid

spawns ``sleep`` in the background, and then waits until it finishes.


::

    for i in (seq 1 5); sleep 10 &; end
    wait

spawns five jobs in the background, and then waits until all of them finishes.


::

    for i in (seq 1 5); sleep 10 &; end
    hoge &
    wait sleep

spawns five jobs and ``hoge`` in the background, and then waits until all ``sleep``\s finish, and doesn't wait for ``hoge`` finishing.
