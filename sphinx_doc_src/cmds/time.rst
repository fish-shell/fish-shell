.. _cmd-time:

time - measure how long a command or block takes
================================================

Synopsis
--------

::

   time COMMAND

Description
-----------

``time`` causes fish to measure how long a command takes and print the results afterwards. The command can be a simple fish command or a block. The results can not currently be redirected.

For checking timing after a command has completed, check :ref:`$CMD_DURATION <variables-special>`.

Your system most likely also has a `time` command. To use that use something like `command time`, as in `command time sleep 10`. Because it's not inside fish, it won't have access to fish functions and won't be able to time blocks and such.

Example
-------

(for obvious reasons exact results will vary on your system)

::

   >_ time sleep 1s
   
   ________________________________________________________
   Executed in    1,01 secs   fish           external
      usr time    2,32 millis    0,00 micros    2,32 millis
      sys time    0,88 millis  877,00 micros    0,00 millis

   >_ time for i in 1 2 3; sleep 1s; end

   ________________________________________________________
   Executed in    3,01 secs   fish           external
      usr time    9,16 millis    2,94 millis    6,23 millis
      sys time    0,23 millis    0,00 millis    0,23 millis
