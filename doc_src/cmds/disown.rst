.. _cmd-disown:

disown - remove a process from the list of jobs
===============================================

Synopsis
--------

.. synopsis::

    disown [PID ...]

Description
-----------

``disown`` removes the specified :ref:`job <syntax-job-control>` from the list of jobs. The job itself continues to exist, but fish does not keep track of it any longer.
This will make fish lose all knowledge of the job, so functions defined with ``--on-process-exit`` or ``--on-job-exit`` will no longer fire.

Jobs in the list of jobs are sent a hang-up signal when fish terminates, which usually causes the job to terminate; ``disown`` allows these processes to continue regardless.

If no process is specified, the most recently-used job is removed (like :doc:`bg <bg>` and :doc:`fg <fg>`).  If one or more PIDs are specified, jobs with the specified process IDs are removed from the job list. Invalid jobs are ignored and a warning is printed.

If a job is stopped, it is sent a signal to continue running, and a warning is printed. It is not possible to use the :doc:`bg <bg>` builtin to continue a job once it has been disowned.

``disown`` returns 0 if all specified jobs were disowned successfully, and 1 if any problems were encountered.

The **--help** or **-h** option displays help about using this command.

Example
-------

``firefox &; disown`` will start the Firefox web browser in the background and remove it from the job list, meaning it will not be closed when the fish process is closed.

``disown (jobs -p)`` removes all :doc:`jobs <jobs>` from the job list without terminating them.
