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

- ``-q`` or ``--quiet`` prints no output for evaluation of jobs by exit status only.

On systems that supports this feature, jobs will print the CPU usage of each job since the last command was executed. The CPU usage is expressed as a percentage of full CPU activity. Note that on multiprocessor systems, the total activity may be more than 100\%.

The exit status of the ``jobs`` builtin is ``0`` if there are running background jobs and ``1`` otherwise.

no output.
----------


Example
-------

``jobs`` outputs a summary of the current jobs.
