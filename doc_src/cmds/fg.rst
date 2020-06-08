.. _cmd-fg:

fg - bring job to foreground
============================

Synopsis
--------

::

    fg [PID]

Description
-----------

``fg`` brings the specified :ref:`job <syntax-job-control>` to the foreground, resuming it if it is stopped. While a foreground job is executed, fish is suspended. If no job is specified, the last job to be used is put in the foreground. If PID is specified, the job with the specified group ID is put in the foreground.


Example
-------

``fg`` will put the last job in the foreground.
