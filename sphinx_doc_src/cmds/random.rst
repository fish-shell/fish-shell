.. _cmd-random:

random - generate random number
===============================

Synopsis
--------

::

    random
    random SEED
    random START END
    random START STEP END
    random choice [ITEMS...]

Description
-----------

``RANDOM`` generates a pseudo-random integer from a uniform distribution. The
range (inclusive) is dependent on the arguments passed.
No arguments indicate a range of [0; 32767].
If one argument is specified, the internal engine will be seeded with the
argument for future invocations of ``RANDOM`` and no output will be produced.
Two arguments indicate a range of [START; END].
Three arguments indicate a range of [START; END] with a spacing of STEP
between possible outputs.
``RANDOM choice`` will select one random item from the succeeding arguments.

Note that seeding the engine will NOT give the same result across different
systems.

You should not consider ``RANDOM`` cryptographically secure, or even
statistically accurate.

Example
-------

The following code will count down from a random even number between 10 and 20 to 1:



::

    for i in (seq (random 10 2 20) -1 1)
        echo $i
    end


And this will open a random picture from any of the subdirectories:



::

    open (random choice **jpg)

