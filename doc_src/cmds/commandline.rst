.. _cmd-commandline:

commandline - set or get the current command line buffer
========================================================

Synopsis
--------

.. synopsis::

    commandline [OPTIONS] [CMD]

Description
-----------

``commandline`` can be used to set or get the current contents of the command line buffer.

With no parameters, ``commandline`` returns the current value of the command line.

With **CMD** specified, the command line buffer is erased and replaced with the contents of **CMD**.

The following options are available:

**-C** or **--cursor**
    Set or get the current cursor position, not the contents of the buffer.
    If no argument is given, the current cursor position is printed, otherwise the argument is interpreted as the new cursor position.
    If one of the options **-j**, **-p** or **-t** is given, the position is relative to the respective substring instead of the entire command line buffer.

**-f** or **--function**
    Causes any additional arguments to be interpreted as input functions, and puts them into the queue, so that they will be read before any additional actual key presses are.
    This option cannot be combined with any other option.
    See :ref:`bind <cmd-bind>` for a list of input functions.

The following options change the way ``commandline`` updates the command line buffer:

**-a** or **--append**
    Do not remove the current commandline, append the specified string at the end of it.

**-i** or **--insert**
    Do not remove the current commandline, insert the specified string at the current cursor position

**-r** or **--replace**
    Remove the current commandline and replace it with the specified string (default)

The following options change what part of the commandline is printed or updated:

**-b** or **--current-buffer**
    Select the entire commandline, not including any displayed autosuggestion (default).

**-j** or **--current-job**
    Select the current job - a **job** here is one pipeline.
    Stops at logical operators or terminators (**;**, **&**, and newlines).

**-p** or **--current-process**
    Select the current process - a **process** here is one command.
    Stops at logical operators, terminators, and pipes.

**-s** or **--current-selection**
    Selects the current selection

**-t** or **--current-token**
    Selects the current token

The following options change the way ``commandline`` prints the current commandline buffer:

**-c** or **--cut-at-cursor**
    Only print selection up until the current cursor position.

**-o** or **--tokenize**
    Tokenize the selection and print one string-type token per line.

If ``commandline`` is called during a call to complete a given string using ``complete -C STRING``, ``commandline`` will consider the specified string to be the current contents of the command line.

The following options output metadata about the commandline state:

**-L** or **--line**
    Print the line that the cursor is on, with the topmost line starting at 1.

**-S** or **--search-mode**
    Evaluates to true if the commandline is performing a history search.

**-P** or **--paging-mode**
    Evaluates to true if the commandline is showing pager contents, such as tab completions.

**--paging-full-mode**
    Evaluates to true if the commandline is showing pager contents, such as tab completions and all lines are shown (no "<n> more rows" message).

**--is-valid**
    Returns true when the commandline is syntactically valid and complete.
    If it is, it would be executed when the ``execute`` bind function is called.
    If the commandline is incomplete, return 2, if erroneus, return 1.

Example
-------

``commandline -j $history[3]`` replaces the job under the cursor with the third item from the command line history.

If the commandline contains


::

    >_ echo $flounder >&2 | less; and echo $catfish


(with the cursor on the "o" of "flounder")

The ``echo $flounder >&`` is the first process, ``less`` the second and ``and echo $catfish`` the third.

``echo $flounder >&2 | less`` is the first job, ``and echo $catfish`` the second.

**$flounder** is the current token.

More examples:


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

