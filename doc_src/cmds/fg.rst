fg - bring job to foreground
============================

Synopsis
--------

.. synopsis::

    fg [PID]

Description
-----------

The **fg** builtin brings the specified :ref:`job <syntax-job-control>` to the foreground, resuming it if it is stopped.
While a foreground job is executed, fish is suspended.
If no job is specified, the last job to be used is put in the foreground.
If ``PID`` is specified, the job containing a process with the specified process ID is put in the foreground.

For compatibility with other shells, job expansion syntax is supported for ``fg``. A *PID* of the format **%1** will foreground job 1.
Job numbers can be seen in the output of :doc:`jobs <jobs>`.

The **--help** or **-h** option displays help about using this command.

Example
-------

``fg`` will put the last job in the foreground.

``fg %3`` will put job 3 into the foreground.
