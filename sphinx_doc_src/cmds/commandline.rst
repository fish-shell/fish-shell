.. _cmd-commandline:

commandline - set or get the current command line buffer
========================================================

Synopsis
--------

::

    commandline [OPTIONS] [CMD]

Description
-----------

``commandline`` can be used to set or get the current contents of the command line buffer.

With no parameters, ``commandline`` returns the current value of the command line.

With ``CMD`` specified, the command line buffer is erased and replaced with the contents of ``CMD``.

The following options are available:

- ``-C`` or ``--cursor`` set or get the current cursor position, not the contents of the buffer. If no argument is given, the current cursor position is printed, otherwise the argument is interpreted as the new cursor position.

- ``-f`` or ``--function`` causes any additional arguments to be interpreted as input functions, and puts them into the queue, so that they will be read before any additional actual key presses are. This option cannot be combined with any other option. See :ref:`bind <cmd-bind>` for a list of input functions.

The following options change the way ``commandline`` updates the command line buffer:

- ``-a`` or ``--append`` do not remove the current commandline, append the specified string at the end of it

- ``-i`` or ``--insert`` do not remove the current commandline, insert the specified string at the current cursor position

- ``-r`` or ``--replace`` remove the current commandline and replace it with the specified string (default)

The following options change what part of the commandline is printed or updated:

- ``-b`` or ``--current-buffer`` select the entire buffer, including any displayed autosuggestion (default)

- ``-j`` or ``--current-job`` select the current job

- ``-p`` or ``--current-process`` select the current process

- ``-s`` or ``--current-selection`` selects the current selection

- ``-t`` or ``--current-token`` select the current token

The following options change the way ``commandline`` prints the current commandline buffer:

- ``-c`` or ``--cut-at-cursor`` only print selection up until the current cursor position

- ``-o`` or ``--tokenize`` tokenize the selection and print one string-type token per line

If ``commandline`` is called during a call to complete a given string using ``complete -C STRING``, ``commandline`` will consider the specified string to be the current contents of the command line.

The following options output metadata about the commandline state:

- ``-L`` or ``--line`` print the line that the cursor is on, with the topmost line starting at 1

- ``-S`` or ``--search-mode`` evaluates to true if the commandline is performing a history search

- ``-P`` or ``--paging-mode`` evaluates to true if the commandline is showing pager contents, such as tab completions


Example
-------

``commandline -j $history[3]`` replaces the job under the cursor with the third item from the command line history.

If the commandline contains


::

    >_ echo $fl___ounder >&2 | less; and echo $catfish


(with the cursor on the "o" of "flounder")

Then the following invocations behave like this:


::

    >_ commandline -t
    $flounder
    >_ commandline -ct
    $fl
    >_ commandline -b # or just commandline
    echo $flounder >&2 | less; and echo $catfish
    >_ commandline -p
    echo $flounder >&2
    >_ commandline -j
    echo $flounder >&2 | less

