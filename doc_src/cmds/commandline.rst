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

**-B** or **--selection-start**
    Get current position of the selection start in the buffer.

**-E** or **--selection-end**
    Get current position of the selection end in the buffer.

**-f** or **--function**
    Causes any additional arguments to be interpreted as input functions, and puts them into the queue, so that they will be read before any additional actual key presses are.
    This option cannot be combined with any other option.
    See :doc:`bind <bind>` for a list of input functions.

**-h** or **--help**
    Displays help about using this command.

The following options change the way ``commandline`` updates the command line buffer:

**-a** or **--append**
    Do not remove the current commandline, append the specified string at the end of it.

**-i**, **--insert** or **--insert-smart**
    Do not remove the current commandline, insert the specified string at the current cursor position.
    The **--insert-smart** option turns on a Do-What-I-Mean (DWIM) mode: it strips any **$** prefix from the first command on each line.

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

**--search-field**
    Use the pager search field instead of the command line. Returns false if the search field is not shown.

**--input=INPUT**
    Operate on this string instead of the commandline. Useful for using options like **--tokens-expanded**.

The following options change the way ``commandline`` prints the current commandline buffer:

**-c** or **--cut-at-cursor**
    Only print selection up until the current cursor position.
    If combined with ``--tokens-expanded``, this will print up until the last completed token - excluding the token the cursor is in.
    This is typically what you would want for instance in completions.
    To get both, use both ``commandline --cut-at-cursor --tokens-expanded; commandline --cut-at-cursor --current-token``,
    or ``commandline -cx; commandline -ct`` for short.

**-x** or **--tokens-expanded**
    Perform argument expansion on the selection and print one argument per line.
    Command substitutions are not expanded but forwarded as-is.

**-o**, **tokenize**, **--tokens-raw**
    Deprecated; do not use.

If ``commandline`` is called during a call to complete a given string using ``complete -C STRING``, ``commandline`` will consider the specified string to be the current contents of the command line.

The following options output metadata about the commandline state:

**-L** or **--line**
    If no argument is given, print the line that the cursor is on, with the topmost line starting at 1.
    Otherwise, set the cursor to the given line.

**--column**
    If no argument is given, print the 1-based offset from the start of the line to the cursor position in Unicode code points.
    Otherwise, set the cursor to the given code point offset.

**-S** or **--search-mode**
    Evaluates to true if the commandline is performing a history search.

**-P** or **--paging-mode**
    Evaluates to true if the commandline is showing pager contents, such as tab completions.

**--paging-full-mode**
    Evaluates to true if the commandline is showing pager contents, such as tab completions and all lines are shown (no "<n> more rows" message).

**--is-valid**
    Returns true when the commandline is syntactically valid and complete.
    If it is, it would be executed when the ``execute`` bind function is called.
    If the commandline is incomplete, return 2, if erroneous, return 1.

**--showing-suggestion**
    Evaluates to true (i.e. returns 0) when the shell is currently showing an automatic history completion/suggestion, available to be consumed via one of the `forward-` bindings.
    For example, can be used to determine if moving the cursor to the right when already at the end of the line would have no effect or if it would cause a completion to be accepted (note that `forward-char-passive` does this automatically).

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

The most common use for something like completions is

::

   set -l tokens (commandline -xpc)

which gives the current *process* (what is being completed), tokenized into separate entries, up to but excluding the currently being completed token

If you are then also interested in the in-progress token, add

::

   set -l current (commandline -ct)

Note that this makes it easy to render fish's infix matching moot - if possible it's best if the completions just print all possibilities and leave the matching to the current token up to fish's logic.

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

