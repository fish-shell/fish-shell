.. _cmd-disown:

disown - remove a process from the list of jobs
===============================================

Synopsis
--------

::

    disown [ PID ... ]

Description
-----------

``disown`` removes the specified :ref:`job <syntax-job-control>` from the list of jobs. The job itself continues to exist, but fish does not keep track of it any longer.

Jobs in the list of jobs are sent a hang-up signal when fish terminates, which usually causes the job to terminate; ``disown`` allows these processes to continue regardless.

If no process is specified, the most recently-used job is removed (like :ref:`bg <cmd-bg>` and :ref:`fg <cmd-fg>`).  If one or more PIDs are specified, jobs with the specified process IDs are removed from the job list. Invalid jobs are ignored and a warning is printed.

If a job is stopped, it is sent a signal to continue running, and a warning is printed. It is not possible to use the ``bg`` builtin to continue a job once it has been disowned.

``disown`` returns 0 if all specified jobs were disowned successfully, and 1 if any problems were encountered.

Example
-------

``firefox &; disown`` will start the Firefox web browser in the background and remove it from the job list, meaning it will not be closed when the fish process is closed.

``disown (jobs -p)`` removes all :ref:`jobs <cmd-jobs>` from the job list without terminating them.
