.. _cmd-jobs:

jobs - print currently running jobs
===================================

Synopsis
--------

::

    jobs [OPTIONS] [PID]


Description
-----------

``jobs`` prints a list of the currently running :ref:`jobs <syntax-job-control>` and their status.

jobs accepts the following switches:

- ``-c`` or ``--command`` prints the command name for each process in jobs.

- ``-g`` or ``--group`` only prints the group ID of each job.

- ``-l`` or ``--last`` prints only the last job to be started.

- ``-p`` or ``--pid`` prints the process ID for each process in all jobs.

- ``-q`` or ``--query`` prints no output for evaluation of jobs by exit status only. For compatibility with old fish versions this is also ``--quiet`` (but this is deprecated).

On systems that supports this feature, jobs will print the CPU usage of each job since the last command was executed. The CPU usage is expressed as a percentage of full CPU activity. Note that on multiprocessor systems, the total activity may be more than 100\%.

If the output of ``jobs`` is redirected or if it is part of a command substitution, the column header that is usually printed is omitted, making it easier to parse.

The exit status of ``jobs`` is ``0`` if there are running background jobs and ``1`` otherwise.

Example
-------

``jobs`` outputs a summary of the current jobs, such as two long-running tasks in this example:

.. code-block:: none

   Job Group   State   Command
   2   26012   running nc -l 55232 < /dev/random &
   1   26011   running python tests/test_11.py &
