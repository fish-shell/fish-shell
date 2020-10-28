.. highlight:: fish-docs-samples
.. _intro:

Introduction
============

This is the documentation for *fish*, the **f**\ riendly **i**\ nteractive **sh**\ ell.

A shell is a program which helps you operate your computer by starting other programs. fish offers a command-line interface focused on usability and interactive use.

Unlike other shells, fish does not follow the POSIX standard, but still roughly belongs to the same family.

Some of the special features of fish are:

- **Extensive UI**: `syntax highlighting`_, autosuggestions_, `tab completion`_ and selection lists that can be navigated and filtered.

- **No configuration needed**: fish is designed to be ready to use immediately, without requiring extensive configuration.

- **Easy scripting**: new functions_ can be added on the fly. The syntax is easy to learn and use.

This bit of the documentation is a quick guide on how to get going. If you are new to this, see the :ref:`tutorial <tutorial>`.

Installation and Start
======================

This section is on how to install, uninstall, start and exit a fish shell and on how to make fish the default shell:

- `Installation`_: How to install fish
- `Starting and Exiting`_ How to start and exit a fish shell
- `Executing Bash`_: How to execute bash commands in fish
- `Default Shell`_: How to switch to fish as the default shell
- `Uninstalling`_: How to uninstall fish


Installation
------------

Up-to-date instructions for installing the latest version of fish are on the `fish homepage <https://fishshell.com/>`_.

To install the development version of fish see the instructions at the `project's GitHub page <https://github.com/fish-shell/fish-shell>`_.


Starting and Exiting
--------------------

Once fish has been installed, open a terminal. If fish is not the default shell:

- Enter ``fish`` to start a fish shell::

    > fish


- Enter ``exit`` to exit a fish shell::

    > exit


Executing Bash
--------------

If fish is your default shell and you want to copy commands from the internet that are written in a different shell language, bash for example, you can proceed in the following way:

Consider that ``bash`` is also a command. With ``man bash`` you can see that there are two ways to do this:

- ``bash`` has a switch ``-c`` to read from a string::

    > bash -c 'some bash command'


- ``bash`` without a switch opens a bash shell that you can use and ``exit`` afterwards::

    > bash
    $ some bash command
    $ exit
    > _


Default Shell
-------------

You can make fish your default shell by adding fish's  executable in two places:

- add ``/usr/local/bin/fish`` to ``/etc/shells``
- change your default shell with ``chsh -s`` to ``/usr/local/bin/fish``

For detailed instructions see :ref:`Switching to fish <switching-to-fish>`.

Uninstalling
------------

For uninstalling fish: see :ref:`FAQ: Uninstalling fish <faq-uninstalling>`.

Shebang Line
------------

Since scripts for shell commands can be written in many different languages, they need to carry information about what interpreter is needed to execute them. For this they are expected to have a first line, the shebang line, which names an executable for this purpose.

A script written in ``bash`` would need a first line like this::

    #!/bin/bash


This line tells the shell to execute the file with the bash interpreter, that is located at the path ``/bin/bash``.

For a script written in another language, just replace the interpreter ``/bin/bash`` with the language interpreter of that other language (for example: ``/bin/python`` for a python script, or ``/usr/local/bin/fish`` for a fish script).

This line is only needed when scripts are executed without specifying the interpreter. For functions inside fish or when executing a script with ``fish /path/to/script`` they aren't required (but don't hurt either!).

.. _syntax:

Syntax overview
===============

Shells like fish are used by giving them commands. Every fish command follows the same basic syntax.

A command is executed by writing the name of the command followed by any arguments.

Example::

   echo hello world

This calls the :ref:`echo <cmd-echo>` command. ``echo`` is a command which will write its arguments to the screen. In the example above, the output will be 'hello world'. Everything in fish is done with commands. There are commands for performing a set of commands multiple times, commands for assigning variables, commands for treating a group of commands as a single command, etc.. And every single command follows the same basic syntax.

If you want to find out more about the echo command used above, read the manual page for the echo command by writing: ``man echo``

``man`` is a command for displaying a manual page on a given topic. The man command takes the name of the manual page to display as an argument. There are manual pages for almost every command on most computers. There are also manual pages for many other things, such as system libraries and important files.

Every program on your computer can be used as a command in fish. If the program file is located in one of the directories in the PATH_, you can just use the name of the program to use it. Otherwise the whole filename, including the directory (like ``/home/me/code/checkers/checkers`` or ``../checkers``) has to be used.

Here is a list of some useful commands:

- :ref:`cd <cmd-cd>`, change the current directory
- ``ls``, list files and directories
- ``man``, display a manual page on the screen
- ``mv``, move (rename) files
- ``cp``, copy files
- :ref:`open <cmd-open>`, open files with the default application associated with each filetype
- ``less``, list the contents of files

Commands and parameters are separated by the space character ``' '``. Every command ends with either a newline (i.e. by pressing the return key) or a semicolon ``;``. More than one command can be written on the same line by separating them with semicolons.

A switch is a very common special type of argument. Switches almost always start with one or more hyphens ``-`` and alter the way a command operates. For example, the ``ls`` command usually lists all the files and directories in the current working directory, but by using the ``-l`` switch, the behavior of ``ls`` is changed to not only display the filename, but also the size, permissions, owner and modification time of each file.

Switches differ between commands and are documented in the manual page for each command. Some switches are common to most command though, for example ``--help`` will usually display a help text, ``-i`` will often turn on interactive prompting before taking action, while ``-f`` will turn it off.

.. _syntax-words:

Some common words
-----------------

This is a short explanation of some of the commonly used words in fish.

- **argument** a parameter given to a command

- **builtin** a command that is implemented in the shell. Builtins are commands that are so closely tied to the shell that it is impossible to implement them as external commands.

- **command** a program that the shell can run. In another sense also specifically an external command (i.e. neither a function or builtin).

- **function** a block of commands that can be called as if they were a single command. By using functions, it is possible to string together multiple smaller commands into one more advanced command.

- **job** a running pipeline or command

- **pipeline** a set of commands stringed together so that the output of one command is the input of the next command

- **redirection** an operation that changes one of the input/output streams associated with a job

- **switch** a special flag sent as an argument to a command that will alter the behavior of the command. A switch almost always begins with one or two hyphens.

.. _quotes:

Quotes
------

Sometimes features like `parameter expansion <#expand>`_ and `character escapes <#escapes>`_ get in the way. When that happens, you can use quotes, either ``'`` (single quote) or ``"`` (double quote). Between single quotes, fish will perform no expansions, in double quotes it will only do :ref:`variable expansion <expand-variable>`. No other kind of expansion (including :ref:`brace expansion <expand-brace>` and parameter expansion) will take place, the parameter can contain spaces, and escape sequences are ignored.

The only backslash escapes that mean anything in single quotes are ``\'``, which escapes a single quote and ``\\``, which escapes the backslash symbol. The only backslash escapes in double quotes are ``\"``, which escapes a double quote, ``\$``, which escapes a dollar character, ``\`` followed by a newline, which deletes the backslash and the newline, and ``\\``, which escapes the backslash symbol.

Single quotes have no special meaning within double quotes and vice versa.

Example::

  rm "cumbersome filename.txt"

removes the file ``cumbersome filename.txt``, while

::

  rm cumbersome filename.txt


removes two files, ``cumbersome`` and ``filename.txt``.

::

   grep 'enabled)$' foo.txt

searches for lines ending in ``enabled)`` in ``foo.txt`` (the ``$`` is special to ``grep``: it matches the end of the line).


.. _escapes:

Escaping characters
-------------------

Some characters can not be written directly on the command line. For these characters, so called escape sequences are provided. These are:

- ``\a`` represents the alert character
- ``\b`` represents the backspace character
- ``\e`` represents the escape character
- ``\f`` represents the form feed character
- ``\n`` represents a newline character
- ``\r`` represents the carriage return character
- ``\t`` represents the tab character
- ``\v`` represents the vertical tab character
- :code:`\ `  escapes the space character
- ``\$`` escapes the dollar character
- ``\\`` escapes the backslash character
- ``\*`` escapes the star character
- ``\?`` escapes the question mark character (this is not necessary if the ``qmark-noglob`` :ref:`feature flag<featureflags>` is enabled)
- ``\~`` escapes the tilde character
- ``\#`` escapes the hash character
- ``\(`` escapes the left parenthesis character
- ``\)`` escapes the right parenthesis character
- ``\{`` escapes the left curly bracket character
- ``\}`` escapes the right curly bracket character
- ``\[`` escapes the left bracket character
- ``\]`` escapes the right bracket character
- ``\<`` escapes the less than character
- ``\>`` escapes the more than character
- ``\^`` escapes the circumflex character
- ``\&`` escapes the ampersand character
- ``\|`` escapes the vertical bar character
- ``\;`` escapes the semicolon character
- ``\"`` escapes the quote character
- ``\'`` escapes the apostrophe character

- ``\xHH``, where *HH* is a hexadecimal number, represents the ascii character with the specified value. For example, ``\x9`` is the tab character.

- ``\XHH``, where *HH* is a hexadecimal number, represents a byte of data with the specified value. If you are using a multibyte encoding, this can be used to enter invalid strings. Only use this if you know what you are doing.

- ``\ooo``, where *ooo* is an octal number, represents the ascii character with the specified value. For example, ``\011`` is the tab character.

- ``\uXXXX``, where *XXXX* is a hexadecimal number, represents the 16-bit Unicode character with the specified value. For example, ``\u9`` is the tab character.

- ``\UXXXXXXXX``, where *XXXXXXXX* is a hexadecimal number, represents the 32-bit Unicode character with the specified value. For example, ``\U9`` is the tab character.

- ``\cX``, where *X* is a letter of the alphabet, represents the control sequence generated by pressing the control key and the specified letter. For example, ``\ci`` is the tab character


.. _redirects:

Input/Output Redirection
-----------------------------

Most programs use three input/output [#]_ streams:

- Standard input ("stdin"), FD 0, for reading, defaults to reading from the keyboard.

- Standard output ("stdout"), FD 1, for writing, defaults to writing to the screen.

- Standard error ("stderr"), FD 2, for writing errors and warnings, defaults to writing to the screen.

They are also represented by numbers: 0 for input, 1 for output, 2 for error. This number is called "file descriptor" or "FD".

The destination of a stream can be changed by something known as *redirection*.

An example of a redirection is ``echo hello > output.txt``, which directs the output of the echo command to the file output.txt.

- To read standard input from a file, write ``<SOURCE_FILE``
- To write standard output to a file, write ``>DESTINATION``
- To write standard error to a file, write ``2>DESTINATION`` [#]_
- To append standard output to a file, write ``>>DESTINATION_FILE``
- To append standard error to a file, write ``2>>DESTINATION_FILE``
- To not overwrite ("clobber") an existing file, write ``>?DESTINATION`` or ``2>?DESTINATION`` (this is also known as the "noclobber" redirection)

``DESTINATION`` can be one of the following:

- A filename. The output will be written to the specified file.

- An ampersand (``&``) followed by the number of another file descriptor. The output will be written to that file descriptor instead.

- An ampersand followed by a minus sign (``&-``). The file descriptor will be closed.

As a convenience, the redirection ``&>`` can be used to direct both stdout and stderr to the same file.

Example:

To redirect both standard output and standard error to the file 'all_output.txt', you can write ``echo Hello &> all_output.txt``, which is a convenience for ``echo Hello > all_output.txt 2>&1``.

Any file descriptor can be redirected in an arbitrary way by prefixing the redirection with the file descriptor.

- To redirect input of FD N, write ``N<DESTINATION``
- To redirect output of FD N, write ``N>DESTINATION``
- To append the output of FD N to a file, write ``N>>DESTINATION_FILE``

Example: ``echo Hello 2>output.stderr`` writes the standard error (file descriptor 2) of the target program to ``output.stderr``.

.. [#] Also shortened as "I/O" or "IO".
.. [#] Previous versions of fish also allowed spelling this as ``^DESTINATION``, but that made another character special so it was deprecated and will be removed in future. See :ref:`feature flags<featureflags>`.

.. _pipes:

Piping
------

Another way to redirect streams is a *pipe*. This connects streams with each other, usually the standard output of one command with the standard input of another.

This is done by separating the commands by the pipe character ``|``. For example

::

  cat foo.txt | head

will call the ``cat`` program with the parameter 'foo.txt', which will print the contents of the file 'foo.txt'. The contents of foo.txt will then be sent to the 'head' program, which will write the first few lines it reads to its output - the screen.

It is possible to use a different output file descriptor by prepending its FD number and then output redirect symbol to the pipe. For example::

  make fish 2>| less


will attempt to build ``fish``, and any errors will be shown using the ``less`` pager. [#]_

As a convenience, the pipe ``&|`` redirects both stdout and stderr to the same process. (Note this is different from bash, which uses ``|&``).

.. [#] A "pager" here is a program that takes output and "paginates" it. ``less`` doesn't just do pages, it allows arbitrary scrolling (even back!).

.. _syntax-background:

Background jobs
---------------

When you start a job in fish, fish itself will pause, and give control of the terminal to the program just started. Sometimes, you want to continue using the commandline, and have the job run in the background. To create a background job, append an \& (ampersand) to your command. This will tell fish to run the job in the background. Background jobs are very useful when running programs that have a graphical user interface.

Example::

  emacs &


will start the emacs text editor in the background. :ref:`fg <cmd-fg>` can be used to bring it into the foreground again when needed.

See also :ref:`Running multiple programs <job-control>`.

.. _syntax-job-control:

Job control
-----------

Most programs allow you to suspend the program's execution and return control to fish by pressing :kbd:`Control`\ +\ :kbd:`Z` (also referred to as ``^Z``). Once back at the fish commandline, you can start other programs and do anything you want. If you then want you can go back to the suspended command by using the :ref:`fg <cmd-fg>` (foreground) command.

If you instead want to put a suspended job into the background, use the :ref:`bg <cmd-bg>` command.

To get a listing of all currently started jobs, use the :ref:`jobs <cmd-jobs>` command.
These listed jobs can be removed with the :ref:`disown <cmd-disown>` command.

.. _syntax-function:

Functions
---------

Functions are programs written in the fish syntax. They group together various commands and their arguments using a single name.

For example, here's a simple function to list directories::

  function ll
      ls -l $argv
  end

The first line tells fish to define a function by the name of ``ll``, so it can be used by simply writing ``ll`` on the commandline. The second line tells fish that the command ``ls -l $argv`` should be called when ``ll`` is invoked. :ref:`$argv <variables-argv>` is a :ref:`list variable <variables-lists>`, which always contains all arguments sent to the function. In the example above, these are simply passed on to the ``ls`` command. The ``end`` on the third line ends the definition.

Calling this as ``ll /tmp/`` will end up running ``ls -l /tmp/``, which will list the contents of /tmp.

This is a kind of function known as a :ref:`wrapper <syntax-function-wrappers>` or "alias".

Fish's prompt is also defined in a function, called :ref:`fish_prompt <cmd-fish_prompt>`. It is run when the prompt is about to be displayed and its output forms the prompt::

  function fish_prompt
      # A simple prompt. Displays the current directory (which fish stores in the $PWD variable)
      # and then a user symbol - a '►' for a normal user and a '#' for root.
      set -l user_char '►'
      if fish_is_root_user
          set user_char '#'
      end

      echo (set_color yellow)$PWD (set_color purple)$user_char
  end

To edit a function, you can use :ref:`funced <cmd-funced>`, and to save a function :ref:`funcsave <cmd-funcsave>`. This will store it in a function file that fish will :ref:`autoload <syntax-function-autoloading>` when needed.

The :ref:`functions <cmd-functions>` builtin can show a function's current definition (and :ref:`type <cmd-type>` will also do if given a function).

For more information on functions, see the documentation for the :ref:`function <cmd-function>` builtin.

.. _syntax-function-wrappers:

Defining aliases
----------------

One of the most common uses for functions is to slightly alter the behavior of an already existing command. For example, one might want to redefine the ``ls`` command to display colors. The switch for turning on colors on GNU systems is ``--color=auto``. An alias, or wrapper, around ``ls`` might look like this::

  function ls
      command ls --color=auto $argv
  end

There are a few important things that need to be noted about aliases:

- Always take care to add the :ref:`$argv <variables-argv>` variable to the list of parameters to the wrapped command. This makes sure that if the user specifies any additional parameters to the function, they are passed on to the underlying command.

- If the alias has the same name as the aliased command, you need to prefix the call to the program with ``command`` to tell fish that the function should not call itself, but rather a command with the same name. If you forget to do so, the function would call itself until the end of time. Usually fish is smart enough to figure this out and will refrain from doing so (which is hopefully in your interest).

- Autoloading isn't applicable to aliases. Since, by definition, the function is created at the time the alias command is executed. You cannot autoload aliases.

To easily create a function of this form, you can use the :ref:`alias <cmd-alias>` command.

Comments
--------

Anything after a ``#`` until the end of the line is a comment. That means it's purely for the reader's benefit, fish ignores it.

This is useful to explain what and why you are doing something::

  function ls
      # The function is called ls, so we have to explicitly call `command ls` to avoid calling ourselves.
      command ls --color=auto $argv
  end

There are no multiline comments. If you want to make a comment span multiple lines, simply start each line with a ``#``.

Comments can also appear after a line like so::

  set -gx EDITOR emacs # I don't like vim.

.. _syntax-function-autoloading:

Autoloading functions
---------------------

Functions can be defined on the commandline or in a configuration file, but they can also be automatically loaded. This has some advantages:

- An autoloaded function becomes available automatically to all running shells.
- If the function definition is changed, all running shells will automatically reload the altered version, after a while.
- Startup time and memory usage is improved, etc.

When fish needs to load a function, it searches through any directories in the :ref:`list variable <variables-lists>` ``$fish_function_path`` for a file with a name consisting of the name of the function plus the suffix ``.fish`` and loads the first it finds.

By default ``$fish_function_path`` contains the following:

- A directory for end-users to keep their own functions, usually ``~/.config/fish/functions`` (controlled by the ``XDG_CONFIG_HOME`` environment variable).
- A directory for systems administrators to install functions for all users on the system, usually ``/etc/fish/functions`` (really ``$__fish_sysconfdir/functions``).
- Directories for other software to put their own functions. These are in the directories in the ``XDG_DATA_DIRS`` environment variable, in a subdirectory called ``fish/vendor_functions.d``. The default is usually ``/usr/share/fish/vendor_functions.d`` and ``/usr/local/share/fish/vendor_functions.d``.
- The functions shipped with fish, usually installed in ``/usr/share/fish/functions`` (really ``$__fish_data_dir/functions``).

If you are unsure, your functions probably belong in ``~/.config/fish/functions``.

Autoloading also won't work for `event handlers <#event>`_, since fish cannot know that a function is supposed to be executed when an event occurs when it hasn't yet loaded the function. See the `event handlers <#event>`_ section for more information.

If you are developing another program and want to install fish functions for it, install them to the "vendor" functions directory. As this path varies from system to system, you can use ``pkgconfig`` to discover it with the output of ``pkg-config --variable functionsdir fish``. Your installation system should support a custom path to override the pkgconfig path, as other distributors may need to alter it easily.

.. _abbreviations:

Abbreviations
-------------

To avoid needless typing, a frequently-run command like ``git checkout`` can be abbreviated to ``gco`` using the :ref:`abbr <cmd-abbr>` command.

::

  abbr -a gco git checkout

After entering ``gco`` and pressing :kbd:`Space` or :kbd:`Enter`, the full text ``git checkout`` will appear in the command line.

.. _syntax-conditional:

Conditional execution of code and flow control
----------------------------------------------

Fish has some builtins that let you execute commands only if a specific criterion is met: :ref:`if <cmd-if>`, :ref:`switch <cmd-switch>`, :ref:`and <cmd-and>` and :ref:`or <cmd-or>`, and also the familiar :ref:`&&/|| <tut-combiners>` syntax.

The :ref:`switch <cmd-switch>` command is used to execute one of possibly many blocks of commands depending on the value of a string. See the documentation for :ref:`switch <cmd-switch>` for more information.

The other conditionals use the :ref:`exit status <variables-status>` of a command to decide if a command or a block of commands should be executed.

Some examples::

  # Just see if the file contains the string "fish" anywhere.
  if grep -q fish myanimals
      echo You have fish!
  else
      echo You don't have fish!
  end

  # $XDG_CONFIG_HOME is a standard place to store configuration. If it's not set applications should use ~/.config.
  set -q XDG_CONFIG_HOME; and set -l configdir $XDG_CONFIG_HOME
  or set -l configdir ~/.config

For more, see the documentation for the builtins or the :ref:`Conditionals <tut-conditionals>` section of the tutorial.

.. _expand:

Parameter expansion
-------------------

When fish is given a commandline, it expands the parameters before sending them to the command. There are multiple different kinds of expansions:

- :ref:`Wildcards <expand-wildcard>`, to create filenames from patterns
- :ref:`Variable expansion <expand-variable>`, to use the value of a variable
- :ref:`Command substitution <expand-command-substitution>`, to use the output of another command
- :ref:`Brace expansion <expand-brace>`, to write lists with common pre- or suffixes in a shorter way
- :ref:`Tilde expansion <expand-home>`, to turn the ``~`` at the beginning of paths into the path to the home directory


.. _expand-wildcard:

Wildcards ("Globbing")
----------------------

When a parameter includes an :ref:`unquoted <quotes>` ``*`` star (or "asterisk") or a ``?`` question mark, fish uses it as a wildcard to match files.

- ``*`` can match any string of characters not containing ``/``. This includes matching an empty string.

- ``**`` matches any string of characters. This includes matching an empty string. The matched string can include the ``/`` character; that is, it goes into subdirectories. If a wildcard string with ``**`` contains a ``/``, that ``/`` still needs to be matched. For example, ``**\/*.fish`` won't match ``.fish`` files directly in the PWD, only in subdirectories. In fish you should type ``**.fish`` to match files in the PWD as well as subdirectories. [#]_

- ``?`` can match any single character except ``/``. This is deprecated and can be disabled via the ``qmark-noglob`` :ref:`feature flag<featureflags>`, so ``?`` will just be an ordinary character.

Other shells, such as zsh, have a much richer glob syntax, like ``**(.)`` to only match regular files. Fish does not. Instead of reinventing the whell, use programs like ``find`` to look for files. For example::

    function ff --description 'Like ** but only returns plain files.'
        # This also ignores .git directories.
        find . \( -name .git -type d -prune \) -o -type f | \
            sed -n -e '/^\.\/\.git$/n' -e 's/^\.\///p'
    end

You would then use it in place of ``**`` like this, ``my_prog (ff)``, to pass only regular files in or below $PWD to ``my_prog``. [#]_

Wildcard matches are sorted case insensitively. When sorting matches containing numbers, they are naturally sorted, so that the strings '1' '5' and '12' would be sorted like 1, 5, 12.

Hidden files (where the name begins with a dot) are not considered when wildcarding unless the wildcard string has a dot in that place.

Examples:

- ``a*`` matches any files beginning with an 'a' in the current directory.

- ``???`` matches any file in the current directory whose name is exactly three characters long.

- ``**`` matches any files and directories in the current directory and all of its subdirectories.

- ``~/.*`` matches all hidden files (also known as "dotfiles") and directories in your home directory.

For most commands, if any wildcard fails to expand, the command is not executed, :ref:`$status <variables-status>` is set to nonzero, and a warning is printed. This behavior is like what bash does with ``shopt -s failglob``. There are exactly 4 exceptions, namely :ref:`set <cmd-set>`, overriding variables in :ref:`overrides <variables-override>`, :ref:`count <cmd-count>` and :ref:`for <cmd-for>`. Their globs will instead expand to zero arguments (so the command won't see them at all), like with ``shopt -s nullglob`` in bash.

Examples::

    ls *.foo
    # Lists the .foo files, or warns if there aren't any.

    set foos *.foo
    if count $foos >/dev/null
        ls $foos
    end
    # Lists the .foo files, if any.

.. [#] Unlike other shells, notably zsh.
.. [#] Technically, unix allows filenames with newlines, and this splits the ``find`` output on newlines. If you want to avoid that, use find's ``-print0`` option and :ref:`string split0<cmd-string-split0>`.

.. _expand-command-substitution:

Command substitution
--------------------

The output of a command (or an entire :ref:`pipeline <pipes>`) can be used as the arguments to another command.

When you write a command in parenthesis like ``outercommand (innercommand)``, the ``innercommand`` will be executed first. Its output will be taken and each line given as a separate argument to ``outercommand``, which will then be executed. [#]_

If the output is piped to :ref:`string split or string split0 <cmd-string-split>` as the last step, those splits are used as they appear instead of splitting lines.

The exit status of the last run command substitution is available in the `status <#variables-status>`_ variable if the substitution happens in the context of a :ref:`set <cmd-set>` command (so ``if set -l (something)`` checks if ``something`` returned true).

Only part of the output can be used, see :ref:`index range expansion <expand-index-range>` for details.

Fish has a default limit of 100 MiB on the data it will read in a command sustitution. If that limit is reached the command (all of it, not just the command substitution - the outer command won't be executed at all) fails and ``$status`` is set to 122. This is so command substitutions can't cause the system to go out of memory, because typically your operating system has a much lower limit, so reading more than that would be useless and harmful. This limit can be adjusted with the ``fish_read_limit`` variable (`0` meaning no limit). This limit also affects the :ref:`read <cmd-read>` command.

Examples::

    echo (basename image.jpg .jpg).png
    # Outputs 'image.png'.

    for i in *.jpg; convert $i (basename $i .jpg).png; end
    # Convert all JPEG files in the current directory to the
    # PNG format using the 'convert' program.

    begin; set -l IFS; set data (cat data.txt); end
    # Set the ``data`` variable to the contents of 'data.txt'
    # without splitting it into a list.

    set data (cat data | string split0)
    # Set ``$data`` to the contents of data, splitting on NUL-bytes.


Sometimes you want to pass the output of a command to another command that only accepts files. If it's just one file, you can usually just pass it via a pipe, like::

    grep fish myanimallist1 | wc -l

but if you need multiple or the command doesn't read from standard input, "process substitution" is useful. Other shells [#]_ allow this via ``foo <(bar) <(baz)``, and fish uses the :ref:`psub <cmd-psub>` command::

    # Compare just the lines containing "fish" in two files:
    diff -u (grep fish myanimallist1 | psub) (grep fish myanimallist2 | psub)

This creates a temporary file, stores the output of the command in that file and prints the filename, so it is given to the outer command.

.. [#] Setting ``$IFS`` to empty will disable line splitting. This is deprecated, use :ref:`string split <cmd-string-split>` instead.
.. [#] Bash and Zsh at least, though it is a POSIX extension

.. _expand-brace:

Brace expansion
---------------

Curly braces can be used to write comma-separated lists. They will be expanded with each element becoming a new parameter, with the surrounding string attached. This is useful to save on typing, and to separate a variable name from surrounding text.

Examples::

  > echo input.{c,h,txt}
  input.c input.h input.txt

  > mv *.{c,h} src/
  # Moves all files with the suffix '.c' or '.h' to the subdirectory src.

  > cp file{,.bak}
  # Make a copy of `file` at `file.bak`.

  > set -l dogs hot cool cute
  > echo {$dogs}dog
  hotdog cooldog cutedog

If there is no "," or variable expansion between the curly braces, they will not be expanded::

    > echo foo-{}
    foo-{}
    > git reset --hard HEAD@{2}
    # passes "HEAD@{2}" to git
    > echo {{a,b}}
    {a} {b} # because the inner brace pair is expanded, but the outer isn't.

If after expansion there is nothing between the braces, the argument will be removed (see :ref:`the cartesian product section <cartesian-product>`)::

    > echo foo-{$undefinedvar}
    # Output is an empty line, just like a bare `echo`.

If there is nothing between a brace and a comma or two commas, it's interpreted as an empty element::

    > echo {,,/usr}/bin
    /bin /bin /usr/bin

To use a "," as an element, :ref:`quote <quotes>` or :ref:`escape <escapes>` it.

.. _expand-variable:

Variable expansion
------------------

One of the most important expansions in fish is the "variable expansion". This is the replacing of a dollar sign (`$`) followed by a variable name with the _value_ of that variable. For more on shell variables, read the `Shell variables <#variables>`_ section.

In the simplest case, this is just something like::

    echo $HOME

which will replace ``$HOME`` with the home directory of the current user, and pass it to :ref:`echo <cmd-echo>`, which will then print it.

Sometimes a variable has no value because it is undefined or empty, and it expands to nothing::


    echo $nonexistentvariable
    # Prints no output.

To separate a variable name from text you can encase the variable within double-quotes or braces::

    echo The plural of $WORD is "$WORD"s
    # Prints "The plural of cat is cats" when $WORD is set to cat.
    echo The plural of $WORD is {$WORD}s
    # ditto

Note that without the quotes or braces, fish will try to expand a variable called ``$WORDs``, which may not exist.

The latter syntax ``{$WORD}`` works by exploiting `brace expansion <#expand-brace>`_.


In these cases, the expansion eliminates the string, as a result of the implicit :ref:`cartesian product <cartesian-product>`.

If $WORD here is undefined or an empty list, the "s" is not printed. However, it is printed if $WORD is the empty string (like after ``set WORD ""``).

Unlike all the other expansions, variable expansion also happens in double quoted strings. Inside double quotes (``"these"``), variables will always expand to exactly one argument. If they are empty or undefined, it will result in an empty string. If they have one element, they'll expand to that element. If they have more than that, the elements will be joined with spaces, unless the variable is a :ref:`path variable <variables-path>` - in that case it will use a colon (`:`) instead [#]_.

Outside of double quotes, variables will expand to as many arguments as they have elements. That means an empty list will expand to nothing, a variable with one element will expand to that element, and a variable with multiple elements will expand to each of those elements separately.

If a variable expands to nothing, it will cancel out any other strings attached to it. See the :ref:`cartesian product <cartesian-product>` section for more information.

The ``$`` symbol can also be used multiple times, as a kind of "dereference" operator (the ``*`` in C or C++), like in the following code::

    set foo a b c
    set a 10; set b 20; set c 30
    for i in (seq (count $$foo))
        echo $$foo[$i]
    end

    # Output is:
    # 10
    # 20
    # 30

When using this feature together with list brackets, the brackets will be used from the inside out. ``$$foo[5]`` will use the fifth element of the ``$foo`` variable as a variable name, instead of giving the fifth element of all the variables $foo refers to. That would instead be expressed as ``$$foo[1][5]`` (take the first element of ``$foo``, use it as a variable name, then give the fifth element of that).


.. [#] Unlike bash or zsh, which will join with the first character of $IFS (which usually is space).

.. _cartesian-product:

Combining lists (Cartesian Product)
-----------------------------------

When lists are expanded with other parts attached, they are expanded with these parts still attached. Even if two lists are attached to each other, they are expanded in all combinations. This is referred to as the `cartesian product` (like in mathematics), and works basically like :ref:`brace expansion <expand-brace>`.

Examples::

    # Brace expansion is the most familiar - all elements in the brace combine with the parts outside of the braces
    >_ echo {good,bad}" apples"
    good apples bad apples

    # The same thing happens with variable expansion.
    >_ set -l a x y z
    >_ set -l b 1 2 3

    # $a is {x,y,z}, $b is {1,2,3},
    # so this is `echo {x,y,z}{1,2,3}`
    >_ echo $a$b
    x1 y1 z1 x2 y2 z2 x3 y3 z3

    # Same thing if something is between the lists
    >_ echo $a"-"$b
    x-1 y-1 z-1 x-2 y-2 z-2 x-3 y-3 z-3

    # Or a brace expansion and a variable
    >_ echo {x,y,z}$b
    x1 y1 z1 x2 y2 z2 x3 y3 z3

    # A combined brace-variable expansion
    >_ echo {$b}word
    1word 2word 3word

    # Special case: If $c has no elements, this expands to nothing
    >_ echo {$c}word
    # Output is an empty line

Sometimes this may be unwanted, especially that tokens can disappear after expansion. In those cases, you should double-quote variables - ``echo "$c"word``.

This also happens after `command substitution <#expand-command-substitution>`_. To avoid tokens disappearing there, make the inner command return a trailing newline, or store the output in a variable and double-quote it.

E.g.

::

    >_ set b 1 2 3
    >_ echo (echo x)$b
    x1 x2 x3
    >_ echo (printf '%s' '')banana # the printf prints nothing, so this is nothing times "banana", which is nothing.
    >_ echo (printf '%s\n' '')banana # the printf prints a newline, so the command substitution expands to an empty string, so this is `''banana`
    banana

This can also be useful. For example, if you want to go through all the files in all the directories in $PATH, use::

    for file in $PATH/*


.. _expand-index-range:

Index range expansion
---------------------

Sometimes it's necessary to access only some of the elements of a list, or some of the lines a command substitution outputs. Both are possible in fish by writing a set of indices in brackets, like::

  $var[2]
  # or
  $var[1..3]

In index brackets, fish understands ranges written like ``a..b`` ('a' and 'b' being indices). They are expanded into a sequence of indices from a to b (so ``a a+1 a+2 ... b``), going up if b is larger and going down if a is larger. Negative indices can also be used - they are taken from the end of the list, so ``-1`` is the last element, and ``-2`` the one before it. If an index doesn't exist the range is clamped to the next possible index.

If a list has 5 elements the indices go from 1 to 5, so a range of ``2..16`` will only go from element 2 to element 5.

If the end is negative the range always goes up, so ``2..-2`` will go from element 2 to 4, and ``2..-16`` won't go anywhere because there is no way to go from the second element to one that doesn't exist, while going up.
If the start is negative the range always goes down, so ``-2..1`` will go from element 4 to 1, and ``-16..2`` won't go anywhere because there is no way to go from the second element to one that doesn't exist, while going down.

A missing starting index in a range defaults to 1. This is allowed if the range is the first index expression of the sequence. Similarly, a missing ending index, defaulting to -1 is allowed for the last index range in the sequence.

Multiple ranges are also possible, separated with a space.

Some examples::


    echo (seq 10)[1 2 3]
    # Prints: 1 2 3

    # Limit the command substitution output
    echo (seq 10)[2..5]
    # Uses elements from 2 to 5
    # Output is: 2 3 4 5

    echo (seq 10)[7..]
    # Prints: 7 8 9 10

    # Use overlapping ranges:
    echo (seq 10)[2..5 1..3]
    # Takes elements from 2 to 5 and then elements from 1 to 3
    # Output is: 2 3 4 5 1 2 3

    # Reverse output
    echo (seq 10)[-1..1]
    # Uses elements from the last output line to
    # the first one in reverse direction
    # Output is: 10 9 8 7 6 5 4 3 2 1

    # The command substitution has only one line,
    # so these will result in empty output:
    echo (echo one)[2..-1]
    echo (echo one)[-3..1]

The same works when setting or expanding variables::


    # Reverse path variable
    set PATH $PATH[-1..1]
    # or
    set PATH[-1..1] $PATH

    # Use only n last items of the PATH
    set n -3
    echo $PATH[$n..-1]

Variables can be used as indices for expansion of variables, like so::

    set index 2
    set letters a b c d
    echo $letters[$index] # returns 'b'

However using variables as indices for command substitution is currently not supported, so::

    echo (seq 5)[$index] # This won't work

    set sequence (seq 5) # It needs to be written on two lines like this.
    echo $sequence[$index] # returns '2'

When using indirect variable expansion with multiple ``$`` (``$$name``), you have to give all indices up to the variable you want to slice::

    > set -l list 1 2 3 4 5
    > set -l name list
    > echo $$name[1]
    1 2 3 4 5
    > echo $$name[1..-1][1..3] # or $$name[1][1..3], since $name only has one element.
    1 2 3

.. _expand-home:

Home directory expansion
------------------------

The ``~`` (tilde) character at the beginning of a parameter, followed by a username, is expanded into the home directory of the specified user. A lone ``~``, or a ``~`` followed by a slash, is expanded into the home directory of the process owner.


.. _combine:

Combining different expansions
------------------------------

All of the above expansions can be combined. If several expansions result in more than one parameter, all possible combinations are created.

When combining multiple parameter expansions, expansions are performed in the following order:

- Command substitutions
- Variable expansions
- Bracket expansion
- Wildcard expansion

Expansions are performed from right to left, nested bracket expansions are performed from the inside and out.

Example:

If the current directory contains the files 'foo' and 'bar', the command ``echo a(ls){1,2,3}`` will output ``abar1 abar2 abar3 afoo1 afoo2 afoo3``.

.. _variables:

Shell variables
---------------

Variables are a way to save data and pass it around. They can be used by the shell, or they can be "exported", so that a copy of the variable is available to any external command the shell starts. An exported variable is referred to as an "environment variable".

To set a variable value, use the :ref:`set <cmd-set>` command. A variable name can not be empty and can contain only letters, digits, and underscores. It may begin and end with any of those characters.

Example:

To set the variable ``smurf_color`` to the value ``blue``, use the command ``set smurf_color blue``.

After a variable has been set, you can use the value of a variable in the shell through `variable expansion <#expand-variable>`_.

Example::

    set smurf_color blue
    echo Smurfs are usually $smurf_color
    set pants_color red
    echo Papa smurf, who is $smurf_color, wears $pants_color pants

So you set a variable with ``set``, and use it with a ``$`` and the name.

.. _variables-scope:

Variable scope
--------------

There are three kinds of variables in fish: universal, global and local variables.

- Universal variables are shared between all fish sessions a user is running on one computer.
- Global variables are specific to the current fish session, and will never be erased unless explicitly requested by using ``set -e``.
- Local variables are specific to the current fish session, and associated with a specific block of commands, and automatically erased when a specific block goes out of scope. A block of commands is a series of commands that begins with one of the commands ``for``, ``while`` , ``if``, ``function``, ``begin`` or ``switch``, and ends with the command ``end``.

Variables can be explicitly set to be universal with the ``-U`` or ``--universal`` switch, global with the ``-g`` or ``--global`` switch, or local with the ``-l`` or ``--local`` switch.  The scoping rules when creating or updating a variable are:

- When a scope is explicitly given, it will be used. If a variable of the same name exists in a different scope, that variable will not be changed.

- When no scope is given, but a variable of that name exists, the variable of the smallest scope will be modified. The scope will not be changed.

- As a special case, when no scope is given and no variable has been defined the variable will belong to the scope of the currently executing *function*. Note that this is different from the ``--local`` flag, which would make the variable local to the current *block*.

There can be many variables with the same name, but different scopes. When you :ref:`use a variable <expand-variable>`, the smallest scoped variable of that name will be used. If a local variable exists, it will be used instead of the global or universal variable of the same name.


Example:

There are a few possible uses for different scopes.

Typically inside funcions you should use local scope::

    function something
        set -l file /path/to/my/file
        if not test -e "$file"
            set file /path/to/my/otherfile
        end
    end

If you want to set something in config.fish, or set something in a function and have it available for the rest of the session, global scope is a good choice::

    # Don't shorten the working directory in the prompt
    set -g fish_prompt_pwd_dir_length 0

    # Set my preferred cursor style:
    function setcursors
       set -g fish_cursor_default block
       set -g fish_cursor_insert line
       set -g fish_cursor_visual underscore
    end

    # Set my language (also :ref:`exported <variables-export>`):
    set -gx LANG de_DE.UTF-8

If you want to set some personal customization, universal variables are nice::

     # Typically you'd run this interactively, fish takes care of keeping it.
     set -U fish_color_autosuggestion 555

The following code will not output anything::

    begin
        # This is a nice local scope where all variables will die
        set -l pirate 'There be treasure in them thar hills'
        set captain Space, the final frontier
    end

    echo $pirate
    # This will not output anything, since the pirate was local
    echo $captain
    # This will output the good Captain's speech since $captain had function-scope.

.. _variables-override:

Overriding variables for a single command
-----------------------------------------

If you want to override a variable for a single command, you can use "var=val" statements before the command::

  # Call git status on another directory (can also be done via `git -C somerepo status`)
  GIT_DIR=somerepo git status

Note that, unlike other shells, fish will first set the variable and then perform other expansions on the line, so::

  set foo banana
  foo=gagaga echo $foo # prints gagaga, while in other shells it might print "banana"

Multiple elements can be given in a :ref:`brace expansion<expand-brace>`::

  # Call bash with a reasonable default path.
  PATH={/usr,}/{s,}bin bash

This syntax is supported since fish 3.1.

.. _variables-universal:

More on universal variables
---------------------------

Universal variables are variables that are shared between all the user's fish sessions on the computer. Fish stores many of its configuration options as universal variables. This means that in order to change fish settings, all you have to do is change the variable value once, and it will be automatically updated for all sessions, and preserved across computer reboots and login/logout.

To see universal variables in action, start two fish sessions side by side, and issue the following command in one of them ``set fish_color_cwd blue``. Since ``fish_color_cwd`` is a universal variable, the color of the current working directory listing in the prompt will instantly change to blue on both terminals.

`Universal variables <#variables-universal>`_ are stored in the file ``.config/fish/fish_variables``. Do not edit this file directly, as your edits may be overwritten. Edit the variables through fish scripts or by using fish interactively instead.

Do not append to universal variables in :ref:`config.fish <initialization>`, because these variables will then get longer with each new shell instance. Instead, simply set them once at the command line.


.. _variables-functions:

Variable scope for functions
-----------------------------

When calling a function, all current local variables temporarily disappear. This shadowing of the local scope is needed since the variable namespace would become cluttered, making it very easy to accidentally overwrite variables from another function.

For example::

    function shiver
        set phrase 'Shiver me timbers'
    end

    function avast
        set --local phrase 'Avast, mateys'
        # Calling the shiver function here can not
        # change any variables in the local scope
        shiver
        echo $phrase
    end
    avast

    # Outputs "Avast, mateys"



.. _variables-export:

Exporting variables
-------------------

Variables in fish can be "exported", so they will be inherited by any commands started by fish. In particular, this is necessary for variables used to configure external commands like $LESS or $GOPATH, but also for variables that contain general system settings like $PATH or $LANGUAGE. If an external command needs to know a variable, it needs to be exported.

Variables can be explicitly set to be exported with the ``-x`` or ``--export`` switch, or not exported with the ``-u`` or ``--unexport`` switch.  The exporting rules when setting a variable are identical to the scoping rules for variables:

- If a variable is explicitly set to either be exported or not exported, that setting will be honored.

- If a variable is not explicitly set to be exported or not exported, but has been previously defined, the previous exporting rule for the variable is kept.

- Otherwise, by default, the variable will not be exported.

- If a variable has local scope and is exported, any function called receives a *copy* of it, so any changes it makes to the variable disappear once the function returns.

- Global variables are accessible to functions whether they are exported or not.

As a naming convention, exported variables are in uppercase and unexported variables are in lowercase.

For example::

    set -gx ANDROID_HOME ~/.android # /opt/android-sdk
    set -gx ASPROOT ~/packages/asp
    set -gx CDPATH . ~ (test -e ~/Videos; and echo ~/Videos)
    set -gx EDITOR emacs -nw
    set -gx GCC_COLORS 'error=01;31:warning=01;35:note=01;36:caret=01;32:locus=01:quote=01'
    set -gx GOPATH ~/dev/go
    set -gx GTK2_RC_FILES "$XDG_CONFIG_HOME/gtk-2.0/gtkrc"
    set -gx LESSHISTFILE "-"

It typically makes sense to make exported variables global as well, but local-exported variables can be useful if you need something more specific than :ref:`Overrides <variables-override>`. They are *copied* to functions so the function can't alter them outside, and still available to commands.

.. _variables-lists:

Lists
-------

Fish can store a list (or an "array" if you wish) of multiple strings inside of a variable::

   > set mylist first second third
   > printf '%s\n' $mylist # prints each element on its own line
   first
   second
   third

To access one element of a list, use the index of the element inside of square brackets, like this::

   echo $PATH[3]

Note that list indices start at 1 in fish, not 0 like in other languages. This is because it requires less subtracting of 1 and many common Unix tools like ``seq`` work better with it (``seq 5`` prints 1 to 5, not 0 to 5). An invalid index is silently ignored resulting in no value (not even an empty string, just no argument at all).

If you don't use any brackets, all the elements of the list will be passed to the command as separate items. This means you can iterate over a list with ``for``::

    for i in $PATH
        echo $i is in the path
    end

This goes over every directory in $PATH separately and prints a line saying it is in the path.

To create a variable ``smurf``, containing the items ``blue`` and ``small``, simply write::

    set smurf blue small

It is also possible to set or erase individual elements of a list::

    # Set smurf to be a list with the elements 'blue' and 'small'
    set smurf blue small

    # Change the second element of smurf to 'evil'
    set smurf[2] evil

    # Erase the first element
    set -e smurf[1]

    # Output 'evil'
    echo $smurf


If you specify a negative index when expanding or assigning to a list variable, the index will be taken from the *end* of the list. For example, the index -1 is the last element of the list::

    > set fruit apple orange banana
    > echo $fruit[-1]
    banana

    > echo $fruit[-2..-1]
    orange
    banana

    > echo $fruit[-1..1] # reverses the list
    banana
    orange
    apple

As you see, you can use a range of indices, see :ref:`index range expansion <expand-index-range>` for details.

All lists are one-dimensional and can't contain other lists, although it is possible to fake nested lists using dereferencing - see :ref:`variable expansion <expand-variable>`.

When a list is exported as an environment variable, it is either space or colon delimited, depending on whether it is a :ref:`path variable <variables-path>`::

    > set -x smurf blue small
    > set -x smurf_PATH forest mushroom
    > env | grep smurf
    smurf=blue small
    smurf_PATH=forest:mushroom

Fish automatically creates lists from all environment variables whose name ends in PATH (like $PATH, $CDPATH or $MANPATH), by splitting them on colons. Other variables are not automatically split.

Lists can be inspected with the :ref:`count <cmd-count>` or the :ref:`contains <cmd-contains>` commands::

    count $smurf
    # 2

    contains blue $smurf
    # key found, exits with status 0

    > contains -i blue $smurf
    1

A nice thing about lists is that they are passed to commands one element as one argument, so once you've set your list, you can just pass it::

  set -l grep_args -r "my string"
  grep $grep_args . # will run the same as `grep -r "my string"` .

Unlike other shells, fish does not do "word splitting" - elements in a list stay as they are, even if they contain spaces or tabs.

.. _variables-argv:

Argument Handling
-----------------

An important list is ``$argv``, which contains the arguments to a function or script. For example::

  function myfunction
      echo $argv[1]
      echo $argv[3]
  end

This function takes whatever arguments it gets and prints the first and third::

  > myfunction first second third
  first
  third

  > myfunction apple cucumber banana
  apple
  banana

Commandline tools often get various options and flags and positional arguments, and $argv would contain all of these.

A more robust approach to argument handling is :ref:`argparse <cmd-argparse>`, which checks the defined options and puts them into various variables, leaving only the positional arguments in $argv. Here's a simple example::

  function mybetterfunction
      argparse h/help s/second -- $argv
      or return # exit if argparse failed because it found an option it didn't recognize - it will print an error

      # If -h or --help is given, we print a little help text and return
      if set -q _flag_help
          echo "mybetterfunction [-h|--help] [-s|--second] [ARGUMENTS...]"
          return 0
      end

      # If -s or --second is given, we print the second argument, not the first and third.
      if set -q _flag_second
          echo $argv[2]
      else
          echo $argv[1]
          echo $argv[3]
      end
  end

The options will be *removed* from $argv, so $argv[2] is the second *positional* argument now::

  > mybetterfunction first -s second third
  second

.. _variables-path:

PATH variables
--------------

Path variables are a special kind of variable used to support colon-delimited path lists including PATH, CDPATH, MANPATH, PYTHONPATH, etc. All variables that end in "PATH" (case-sensitive) become PATH variables.

PATH variables act as normal lists, except they are are implicitly joined and split on colons.

::

    set MYPATH 1 2 3
    echo "$MYPATH"
    # 1:2:3
    set MYPATH "$MYPATH:4:5"
    echo $MYPATH
    # 1 2 3 4 5
    echo "$MYPATH"
    # 1:2:3:4:5

Variables can be marked or unmarked as PATH variables via the ``--path`` and ``--unpath`` options to ``set``.

.. _variables-special:
.. _PATH:

Special variables
-----------------

You can change the settings of fish by changing the values of certain variables.

- ``PATH``, a list of directories in which to search for commands

- ``CDPATH``, a list of directories in which the :ref:`cd <cmd-cd>` builtin looks for a new directory.

- ``LANG``, ``LC_ALL``, ``LC_COLLATE``, ``LC_CTYPE``, ``LC_MESSAGES``, ``LC_MONETARY``, ``LC_NUMERIC`` and ``LC_TIME`` set the language option for the shell and subprograms. See the section `Locale variables <#variables-locale>`_ for more information.

- A number of variable starting with the prefixes ``fish_color`` and ``fish_pager_color``. See `Variables for changing highlighting colors <#variables-color>`__ for more information.

- ``fish_ambiguous_width`` controls the computed width of ambiguous-width characters. This should be set to 1 if your terminal renders these characters as single-width (typical), or 2 if double-width.

- ``fish_emoji_width`` controls whether fish assumes emoji render as 2 cells or 1 cell wide. This is necessary because the correct value changed from 1 to 2 in Unicode 9, and some terminals may not be aware. Set this if you see graphical glitching related to emoji (or other "special" characters). It should usually be auto-detected.

- ``FISH_DEBUG`` and ``FISH_DEBUG_OUTPUT`` control what debug output fish generates and where it puts it, analogous to the ``--debug`` and ``--debug-output`` options. These have to be set on startup, via e.g. ``FISH_DEBUG='reader*' FISH_DEBUG_OUTPUT=/tmp/fishlog fish``.

- ``fish_escape_delay_ms`` sets how long fish waits for another key after seeing an escape, to distinguish pressing the escape key from the start of an escape sequence. The default is 30ms. Increasing it increases the latency but allows pressing escape instead of alt for alt+character bindings. For more information, see :ref:`the chapter in the bind documentation <cmd-bind-escape>`.

- ``fish_greeting``, the greeting message printed on startup. This is printed by a function of the same name that can be overridden for more complicated changes (see :ref:`funced <cmd-funced>`

- ``fish_history``, the current history session name. If set, all subsequent commands within an
  interactive fish session will be logged to a separate file identified by the value of the
  variable. If unset, or set to ``default``, the default session name "fish" is used. If set to an
  empty string, history is not saved to disk (but is still available within the interactive
  session).

- ``fish_trace``, if set and not empty, will cause fish to print commands before they execute, similar to ``set -x`` in bash. The trace is printed to the path given by the :ref:`--debug-output <cmd-fish>` option to fish (stderr by default).

- ``fish_user_paths``, a list of directories that are prepended to ``PATH``. This can be a universal variable.

- ``umask``, the current file creation mask. The preferred way to change the umask variable is through the :ref:`umask <cmd-umask>` function. An attempt to set umask to an invalid value will always fail.

- ``BROWSER``, your preferred web browser. If this variable is set, fish will use the specified browser instead of the system default browser to display the fish documentation.

Fish also provides additional information through the values of certain environment variables. Most of these variables are read-only and their value can't be changed with ``set``.

- ``_``, the name of the currently running command (though this is deprecated, and the use of ``status current-command`` is preferred).

- ``argv``, a list of arguments to the shell or function. ``argv`` is only defined when inside a function call, or if fish was invoked with a list of arguments, like ``fish myscript.fish foo bar``. This variable can be changed.

- ``CMD_DURATION``, the runtime of the last command in milliseconds.

- ``COLUMNS`` and ``LINES``, the current size of the terminal in height and width. These values are only used by fish if the operating system does not report the size of the terminal. Both variables must be set in that case otherwise a default of 80x24 will be used. They are updated when the window size changes.

- ``fish_kill_signal``, the signal that terminated the last foreground job, or 0 if the job exited normally.

- ``fish_pid``, the process ID (PID) of the shell.

- ``history``, a list containing the last commands that were entered.

- ``HOME``, the user's home directory. This variable can be changed.

- ``hostname``, the machine's hostname.

- ``IFS``, the internal field separator that is used for word splitting with the :ref:`read <cmd-read>` builtin. Setting this to the empty string will also disable line splitting in `command substitution <#expand-command-substitution>`_. This variable can be changed.

- ``last_pid``, the process ID (PID) of the last background process.

- ``PWD``, the current working directory.

- ``pipestatus``, a list of exit statuses of all processes that made up the last executed pipe.

- ``SHLVL``, the level of nesting of shells.

- ``status``, the `exit status <#variables-status>`_ of the last foreground job to exit. If the job was terminated through a signal, the exit status will be 128 plus the signal number.

- ``status_generation``, the "generation" count of ``$status``. This will be incremented only when the previous command produced an explicit status. (For example, background jobs will not increment this).

- ``USER``, the current username. This variable can be changed.

- ``version``, the version of the currently running fish (also available as ``FISH_VERSION`` for backward compatibility).

As a convention, an uppercase name is usually used for exported variables, while lowercase variables are not exported. (``CMD_DURATION`` is an exception for historical reasons). This rule is not enforced by fish, but it is good coding practice to use casing to distinguish between exported and unexported variables.

Fish also uses some variables internally, their name usually starting with ``__fish``. These are internal and should not typically be modified directly.

.. _variables-status:

The status variable
-------------------

Whenever a process exits, an exit status is returned to the program that started it (usually the shell). This exit status is an integer number, which tells the calling application how the execution of the command went. In general, a zero exit status means that the command executed without problem, but a non-zero exit status means there was some form of problem.

Fish stores the exit status of the last process in the last job to exit in the ``status`` variable.

If fish encounters a problem while executing a command, the status variable may also be set to a specific value:

- 0 is generally the exit status of fish commands if they successfully performed the requested operation.

- 1 is generally the exit status of fish commands if they failed to perform the requested operation.

- 121 is generally the exit status of fish commands if they were supplied with invalid arguments.

- 123 means that the command was not executed because the command name contained invalid characters.

- 124 means that the command was not executed because none of the wildcards in the command produced any matches.

- 125 means that while an executable with the specified name was located, the operating system could not actually execute the command.

- 126 means that while a file with the specified name was located, it was not executable.

- 127 means that no function, builtin or command with the given name could be located.

If a process exits through a signal, the exit status will be 128 plus the number of the signal.


.. _variables-color:

Syntax highlighting variables
------------------------------------------

The colors used by fish for syntax highlighting can be configured by changing the values of a various variables. The value of these variables can be one of the colors accepted by the :ref:`set_color <cmd-set_color>` command. The ``--bold`` or ``-b`` switches accepted by ``set_color`` are also accepted.


Example: to make errors highlighted and red, use::

    set fish_color_error red --bold


The following variables are available to change the highlighting colors in fish:

==========================================                 =====================================================================
Variable                                                   Meaning
==========================================                 =====================================================================
``fish_color_normal``                                      default color
``fish_color_command``                                     commands like echo
``fish_color_quote``                                       quoted text like "abc"
``fish_color_redirection``                                 IO redirections like >/dev/null
``fish_color_end``                                         process separators like ';' and '&'
``fish_color_error``                                       syntax errors
``fish_color_param``                                       ordinary command parameters
``fish_color_comment``                                     comments like '# important'
``fish_color_selection``                                   selected text in vi visual mode
``fish_color_operator``                                    parameter expansion operators like '*' and '~'
``fish_color_escape``                                      character escapes like '\n' and '\x70'
``fish_color_autosuggestion``                              autosuggestions (the proposed rest of a command)
``fish_color_cwd``                                         the current working directory in the default prompt
``fish_color_user``                                        the username in the default prompt
``fish_color_host``                                        the hostname in the default prompt
``fish_color_host_remote``                                 the hostname in the default prompt for remote sessions (like ssh)
``fish_color_cancel``                                      the '^C' indicator on a canceled command
``fish_color_search_match``                                history search matches and selected pager items (background only)
==========================================                 =====================================================================

.. _variables-color-pager:

Pager color variables
------------------------------------------

fish will sometimes present a list of choices in a table, called the pager.

Example: to set the background of each pager row, use::

    set fish_pager_color_background --background=white

To have black text on alternating white and gray backgrounds::

    set fish_pager_color_prefix black
    set fish_pager_color_completion black
    set fish_pager_color_description black
    set fish_pager_color_background --background=white
    set fish_pager_color_secondary_background --background=brwhite

Variables affecting the pager colors:

==========================================                 ===========================================================
Variable                                                   Meaning
==========================================                 ===========================================================
``fish_pager_color_progress``                              the progress bar at the bottom left corner
``fish_pager_color_background``                            the background color of a line
``fish_pager_color_prefix``                                the prefix string, i.e. the string that is to be completed
``fish_pager_color_completion``                            the completion itself, i.e. the proposed rest of the string
``fish_pager_color_description``                           the completion description
``fish_pager_color_selected_background``                   background of the selected completion
``fish_pager_color_selected_prefix``                       prefix of the selected completion
``fish_pager_color_selected_completion``                   suffix of the selected completion
``fish_pager_color_selected_description``                  description of the selected completion
``fish_pager_color_secondary_background``                  background of every second unselected completion
``fish_pager_color_secondary_prefix``                      prefix of every second unselected completion
``fish_pager_color_secondary_completion``                  suffix of every second unselected completion
``fish_pager_color_secondary_description``                 description of every second unselected completion
==========================================                 ===========================================================

.. _variables-locale:

Locale variables
----------------

The most common way to set the locale to use a command like 'set -x LANG en_GB.utf8', which sets the current locale to be the English language, as used in Great Britain, using the UTF-8 character set. For a list of available locales, use 'locale -a'.

``LANG``, ``LC_ALL``, ``LC_COLLATE``, ``LC_CTYPE``, ``LC_MESSAGES``,  ``LC_MONETARY``, ``LC_NUMERIC`` and ``LC_TIME`` set the language option for the shell and subprograms. These variables work as follows: ``LC_ALL`` forces all the aspects of the locale to the specified value. If ``LC_ALL`` is set, all other locale variables will be ignored. The other ``LC_`` variables set the specified aspect of the locale information. ``LANG`` is a fallback value, it will be used if none of the ``LC_`` variables are specified.

.. _builtin-overview:

Builtin commands
----------------

Fish includes a number of commands in the shell directly. We call these "builtins". These include:

- Builtins that manipulate the shell state - :ref:`cd <cmd-cd>` changes directory, :ref:`set <cmd-set>` sets variables
- Builtins for dealing with data, like :ref:`string <cmd-string>` for strings and :ref:`math <cmd-math>` for numbers, :ref:`count <cmd-count>` for counting lines or arguments
- :ref:`status <cmd-status>` for asking about the shell's status
- :ref:`printf <cmd-printf>` and :ref:`echo <cmd-echo>` for creating output
- :ref:`test <cmd-test>` for checking conditions
- :ref:`argparse <cmd-argparse>` for parsing function arguments
- :ref:`source <cmd-source>` to read a script in the current shell (so changes to variables stay) and :ref:`eval <cmd-eval>` to execute a string as script
- :ref:`random <cmd-random>` to get random numbers or pick a random element from a list

For a list of all builtins, use ``builtin -n``.

For a list of all builtins, functions and commands shipped with fish, see the :ref:`list of commands <Commands>`. The documentation is also available by using the ``--help`` switch.

.. _identifiers:

Shell variable and function names
---------------------------------

The names given to variables and functions (so called "identifiers") have to follow certain rules:

- A variable name cannot be empty. It can contain only letters, digits, and underscores. It may begin and end with any of those characters.

- A function name cannot be empty. It may not begin with a hyphen ("-") and may not contain a slash ("/"). All other characters, including a space, are valid.

- A bind mode name (e.g., ``bind -m abc ...``) must be a valid variable name.

Other things have other restrictions. For instance what is allowed for file names depends on your system, but at the very least they cannot contain a "/" (because that is the path separator) or NULL byte (because that is how UNIX ends strings).

Interactive use
===============

Fish prides itself on being really nice to use interactively. That's down to a few features we'll explain in the next few sections.


Help
----

Fish has an extensive help system. Use the :ref:`help <cmd-help>` command to obtain help on a specific subject or command. For instance, writing ``help syntax`` displays the `syntax section <#syntax>`_ of this documentation.

Fish also has man pages for its commands. For example, ``man set`` will show the documentation for ``set`` as a man page.

Help on a specific builtin can also be obtained with the ``-h`` parameter. For instance, to obtain help on the :ref:`fg <cmd-fg>` builtin, either type ``fg -h`` or ``help fg``.

Autosuggestions
---------------

fish suggests commands as you type, based on `command history <#history-search>`_, completions, and valid file paths. As you type commands, you will see a suggestion offered after the cursor, in a muted gray color (which can be changed with the ``fish_color_autosuggestion`` variable).

To accept the autosuggestion (replacing the command line contents), press :kbd:`→` or :kbd:`Control`\ +\ :kbd:`F`. To accept the first suggested word, press :kbd:`Alt`\ +\ :kbd:`→` or :kbd:`Alt`\ +\ :kbd:`F`. If the autosuggestion is not what you want, just ignore it: it won't execute unless you accept it.

Autosuggestions are a powerful way to quickly summon frequently entered commands, by typing the first few characters. They are also an efficient technique for navigating through directory hierarchies.


Tab Completion
--------------

Tab completion is a time saving feature of any modern shell. When you type :kbd:`Tab`, fish tries to guess the rest of the word under the cursor. If it finds just one possibility, it inserts it. If it finds more, it inserts the longest unambiguous part and then opens a menu (the "pager") that you can navigate to find what you're looking for.

The pager can be navigated with the arrow keys, :kbd:`Page Up` / :kbd:`Page Down`, :kbd:`Tab` or :kbd:`Shift`\ +\ :kbd:`Tab`. Pressing :kbd:`Control`\ +\ :kbd:`S` (the ``pager-toggle-search`` binding - :kbd:`/` in vi-mode) opens up a search menu that you can use to filter the list.

Fish provides some general purpose completions:

- Commands (builtins, functions and regular programs).

- Shell variable names.

- Usernames for tilde expansion.

- Filenames, even on strings with wildcards such as ``*`` and ``**``.

It also provides a large number of program specific scripted completions. Most of these completions are simple options like the ``-l`` option for ``ls``, but some are more advanced. For example:

- The programs ``man`` and ``whatis`` show all installed manual pages as completions.

- The ``make`` program uses all targets in the Makefile in the current directory as completions.

- The ``mount`` command uses all mount points specified in fstab as completions.

- The ``ssh`` command uses all hosts that are stored in the known_hosts file as completions. (See the ssh documentation for more information)

- The ``su`` command shows the users on the system

- The ``apt-get``, ``rpm`` and ``yum`` commands show installed or installable packages

You can also write your own completions or install some you got from someone else. For that, see :ref:`Writing your own completions <completion-own>`.

.. _editor:

Command line editor
===================

The fish editor features copy and paste, a `searchable history <#history-search>`_ and many editor functions that can be bound to special keyboard shortcuts.

Similar to bash, fish has Emacs and Vi editing modes. The default editing mode is Emacs. You can switch to Vi mode with ``fish_vi_key_bindings`` and switch back with ``fish_default_key_bindings``. You can also make your own key bindings by creating a function and setting $fish_key_bindings to its name. For example::


    function fish_hybrid_key_bindings --description "Vi-style bindings that inherit emacs-style bindings in all modes"
        for mode in default insert visual
            fish_default_key_bindings -M $mode
        end
        fish_vi_key_bindings --no-erase
    end
    set -g fish_key_bindings fish_hybrid_key_bindings


.. _shared-binds:

Shared bindings
---------------

Some bindings are shared between emacs- and vi-mode because they aren't text editing bindings or because what Vi/Vim does for a particular key doesn't make sense for a shell.

- :kbd:`Tab` `completes <#tab-completion>`_ the current token. :kbd:`Shift`\ +\ :kbd:`Tab` completes the current token and starts the pager's search mode.

- :kbd:`←` (Left) and :kbd:`→` (Right) move the cursor left or right by one character. If the cursor is already at the end of the line, and an autosuggestion is available, :kbd:`→` accepts the autosuggestion.

- :kbd:`Alt`\ +\ :kbd:`←` and :kbd:`Alt`\ +\ :kbd:`→` move the cursor one word left or right (to the next space or punctuation mark), or moves forward/backward in the directory history if the command line is empty. If the cursor is already at the end of the line, and an autosuggestion is available, :kbd:`Alt`\ +\ :kbd:`→` (or :kbd:`Alt`\ +\ :kbd:`F`) accepts the first word in the suggestion.

- :kbd:`Control`\ +\ :kbd:`←` and :kbd:`Control`\ +\ :kbd:`→` move the cursor one word left or right. These accept one word of the autosuggestion - the part they'd move over.

- :kbd:`Shift`\ +\ :kbd:`←` and :kbd:`Shift`\ +\ :kbd:`→` move the cursor one word left or right, without stopping on punctuation. These accept one big word of the autosuggestion.

- :kbd:`↑` (Up) and :kbd:`↓` (Down) (or :kbd:`Control`\ +\ :kbd:`P` and :kbd:`Control`\ +\ :kbd:`N` for emacs aficionados) search the command history for the previous/next command containing the string that was specified on the commandline before the search was started. If the commandline was empty when the search started, all commands match. See the `history <#history-search>`_ section for more information on history searching.

- :kbd:`Alt`\ +\ :kbd:`↑` and :kbd:`Alt`\ +\ :kbd:`↓` search the command history for the previous/next token containing the token under the cursor before the search was started. If the commandline was not on a token when the search started, all tokens match. See the `history <#history-search>`_ section for more information on history searching.

- :kbd:`Control`\ +\ :kbd:`C` cancels the entire line.

- :kbd:`Control`\ +\ :kbd:`D` delete one character to the right of the cursor. If the command line is empty, :kbd:`Control`\ +\ :kbd:`D` will exit fish.

- :kbd:`Control`\ +\ :kbd:`U` moves contents from the beginning of line to the cursor to the `killring <#killring>`__.

- :kbd:`Control`\ +\ :kbd:`L` clears and repaints the screen.

- :kbd:`Control`\ +\ :kbd:`R` searches the history if there is something in the commandline. This is mainly to ease the transition from other shells, where ctrl+r initiates the history search.

- :kbd:`Control`\ +\ :kbd:`W` moves the previous path component (everything up to the previous "/", ":" or "@") to the `killring <#killring>`__.

- :kbd:`Control`\ +\ :kbd:`X` copies the current buffer to the system's clipboard, :kbd:`Control`\ +\ :kbd:`V` inserts the clipboard contents.

- :kbd:`Alt`\ +\ :kbd:`D` moves the next word to the `killring <#killring>`__.

- :kbd:`Alt`\ +\ :kbd:`H` (or :kbd:`F1`) shows the manual page for the current command, if one exists.

- :kbd:`Alt`\ +\ :kbd:`L` lists the contents of the current directory, unless the cursor is over a directory argument, in which case the contents of that directory will be listed.

- :kbd:`Alt`\ +\ :kbd:`O` opens the file at the cursor in a pager.

- :kbd:`Alt`\ +\ :kbd:`P` adds the string ``&| less;`` to the end of the job under the cursor. The result is that the output of the command will be paged.

- :kbd:`Alt`\ +\ :kbd:`W` prints a short description of the command under the cursor.

- :kbd:`Alt`\ +\ :kbd:`E` edit the current command line in an external editor. The editor is chosen from the first available of the ``$VISUAL`` or ``$EDITOR`` variables.

- :kbd:`Alt`\ +\ :kbd:`V` Same as :kbd:`Alt`\ +\ :kbd:`E`.

- :kbd:`Alt`\ +\ :kbd:`S` Prepends ``sudo`` to the current commandline. If the commandline is empty, prepend ``sudo`` to the last commandline.

- :kbd:`Control`\ +\ :kbd:`Space` Inserts a space without expanding an :ref:`abbreviation <abbreviations>`. For vi-mode this only applies to insert-mode.

.. _emacs-mode:

Emacs mode commands
-------------------

- :kbd:`Home` or :kbd:`Control`\ +\ :kbd:`A` moves the cursor to the beginning of the line.

- :kbd:`End` or :kbd:`Control`\ +\ :kbd:`E` moves to the end of line. If the cursor is already at the end of the line, and an autosuggestion is available, :kbd:`End` or :kbd:`Control`\ +\ :kbd:`E` accepts the autosuggestion.

- :kbd:`Control`\ +\ :kbd:`B`, :kbd:`Control`\ +\ :kbd:`F` move the cursor one character left or right or accept the autosuggestion just like the :kbd:`←` (Left) and :kbd:`→` (Right) shared bindings (which are available as well).

- :kbd:`Control`\ +\ :kbd:`N`, :kbd:`Control`\ +\ :kbd:`P` move the cursor up/down or through history, like the up and down arrow shared bindings.

- :kbd:`Delete` or :kbd:`Backspace` removes one character forwards or backwards respectively.

- :kbd:`Control`\ +\ :kbd:`K` moves contents from the cursor to the end of line to the `killring <#killring>`__.

- :kbd:`Alt`\ +\ :kbd:`C` capitalizes the current word.

- :kbd:`Alt`\ +\ :kbd:`U` makes the current word uppercase.

- :kbd:`Control`\ +\ :kbd:`T` transposes the last two characters.

- :kbd:`Alt`\ +\ :kbd:`T` transposes the last two words.

- :kbd:`Control`\ +\ :kbd:`Z`, :kbd:`Control`\ +\ :kbd:`_` (:kbd:`Control`\ +\ :kbd:`/` on some terminals) undo the most recent edit of the line.

- :kbd:`Alt`\ +\ :kbd:`/` reverts the most recent undo.


You can change these key bindings using the :ref:`bind <cmd-bind>` builtin.


.. _vi-mode:

Vi mode commands
----------------

Vi mode allows for the use of Vi-like commands at the prompt. Initially, `insert mode <#vi-mode-insert>`_ is active. :kbd:`Escape` enters `command mode <#vi-mode-command>`_. The commands available in command, insert and visual mode are described below. Vi mode shares `some bindings <#shared-binds>`_ with `Emacs mode <#emacs-mode>`_.

It is also possible to add all emacs-mode bindings to vi-mode by using something like::


    function fish_user_key_bindings
        # Execute this once per mode that emacs bindings should be used in
        fish_default_key_bindings -M insert

        # Then execute the vi-bindings so they take precedence when there's a conflict.
        # Without --no-erase fish_vi_key_bindings will default to
        # resetting all bindings.
        # The argument specifies the initial mode (insert, "default" or visual).
        fish_vi_key_bindings --no-erase insert
    end


When in vi-mode, the :ref:`fish_mode_prompt <cmd-fish_mode_prompt>` function will display a mode indicator to the left of the prompt. To disable this feature, override it with an empty function. To display the mode elsewhere (like in your right prompt), use the output of the ``fish_default_mode_prompt`` function.

When a binding switches the mode, it will repaint the mode-prompt if it exists, and the rest of the prompt only if it doesn't. So if you want a mode-indicator in your ``fish_prompt``, you need to erase ``fish_mode_prompt`` e.g. by adding an empty file at ``~/.config/fish/functions/fish_mode_prompt.fish``. (Bindings that change the mode are supposed to call the `repaint-mode` bind function, see :ref:`bind <cmd-bind>`)

The ``fish_vi_cursor`` function will be used to change the cursor's shape depending on the mode in supported terminals. The following snippet can be used to manually configure cursors after enabling vi-mode::

   # Emulates vim's cursor shape behavior
   # Set the normal and visual mode cursors to a block
   set fish_cursor_default block
   # Set the insert mode cursor to a line
   set fish_cursor_insert line
   # Set the replace mode cursor to an underscore
   set fish_cursor_replace_one underscore
   # The following variable can be used to configure cursor shape in
   # visual mode, but due to fish_cursor_default, is redundant here
   set fish_cursor_visual block

Additionally, ``blink`` can be added after each of the cursor shape parameters to set a blinking cursor in the specified shape.

If the cursor shape does not appear to be changing after setting the above variables, it's likely your terminal emulator does not support the capabilities necessary to do this. It may also be the case, however, that ``fish_vi_cursor`` has not detected your terminal's features correctly (for example, if you are using ``tmux``). If this is the case, you can force ``fish_vi_cursor`` to set the cursor shape by setting ``$fish_vi_force_cursor`` in ``config.fish``. You'll have to restart fish for any changes to take effect. If cursor shape setting remains broken after this, it's almost certainly an issue with your terminal emulator, and not fish.

.. _vi-mode-command:

Command mode
------------

Command mode is also known as normal mode.

- :kbd:`H` moves the cursor left.

- :kbd:`L` moves the cursor right.

- :kbd:`I` enters `insert mode <#vi-mode-insert>`_ at the current cursor position.

- :kbd:`V` enters `visual mode <#vi-mode-visual>`_ at the current cursor position.

- :kbd:`A` enters `insert mode <#vi-mode-insert>`_ after the current cursor position.

- :kbd:`Shift`\ +\ :kbd:`A` enters `insert mode <#vi-mode-insert>`_ at the end of the line.

- :kbd:`0` (zero) moves the cursor to beginning of line (remaining in command mode).

- :kbd:`D`\ +\ :kbd:`D` deletes the current line and moves it to the `killring <#killring>`__.

- :kbd:`Shift`\ +\ :kbd:`D` deletes text after the current cursor position and moves it to the `killring <#killring>`__.

- :kbd:`P` pastes text from the `killring <#killring>`__.

- :kbd:`U` search history backwards.

- :kbd:`[` and :kbd:`]` search the command history for the previous/next token containing the token under the cursor before the search was started. See the `history <#history-search>`_ section for more information on history searching.

- :kbd:`Backspace` moves the cursor left.

.. _vi-mode-insert:

Insert mode
-----------

- :kbd:`Escape` enters `command mode <#vi-mode-command>`_.

- :kbd:`Backspace` removes one character to the left.

.. _vi-mode-visual:

Visual mode
-----------

- :kbd:`←` (Left) and :kbd:`→` (Right) extend the selection backward/forward by one character.

- :kbd:`B` and :kbd:`W` extend the selection backward/forward by one word.

- :kbd:`D` and :kbd:`X` move the selection to the `killring <#killring>`__ and enter `command mode <#vi-mode-command>`__.

- :kbd:`Escape` and :kbd:`Control`\ +\ :kbd:`C` enter `command mode <#vi-mode-command>`_.

.. _custom-binds:

Custom bindings
---------------

In addition to the standard bindings listed here, you can also define your own with :ref:`bind <cmd-bind>`::

  # Just clear the commandline on control-c
  bind \cc 'commandline -r ""'

Put ``bind`` statements into :ref:`config.fish <initialization>` or a function called ``fish_user_key_bindings``.

The key sequence (the ``\cc``) here depends on your setup, in particular the terminal. To find out what the terminal sends use :ref:`fish_key_reader <cmd-fish_key_reader>`::

  > fish_key_reader # pressing control-c
  Press a key:
              hex:    3  char: \cC
  Press [ctrl-C] again to exit
  bind \cC 'do something'

  > fish_key_reader # pressing the right-arrow
  Press a key:
              hex:   1B  char: \c[  (or \e)
  (  0.077 ms)  hex:   5B  char: [
  (  0.037 ms)  hex:   43  char: C
  bind \e\[C 'do something'

Note that some key combinations are indistinguishable or unbindable. For instance control-i *is the same* as the tab key. This is a terminal limitation that fish can't do anything about.

Also, :kbd:`Escape` is the same thing as :kbd:`Alt` in a terminal. To distinguish between pressing :kbd:`Escape` and then another key, and pressing :kbd:`Alt` and that key (or an escape sequence the key sends), fish waits for a certain time after seeing an escape character. This is configurable via the ``fish_escape_delay_ms`` variable.

If you want to be able to press :kbd:`Escape` and then a character and have it count as :kbd:`Alt`\ +\ that character, set it to a higher value, e.g.::

  set -g fish_escape_delay_ms 100

.. _killring:

Copy and paste (Kill Ring)
--------------------------

Fish uses an Emacs-style kill ring for copy and paste functionality. For example, use :kbd:`Control`\ +\ :kbd:`K` (`kill-line`) to cut from the current cursor position to the end of the line. The string that is cut (a.k.a. killed in emacs-ese) is inserted into a list of kills, called the kill ring. To paste the latest value from the kill ring (emacs calls this "yanking") use :kbd:`Control`\ +\ :kbd:`Y` (the ``yank`` input function). After pasting, use :kbd:`Alt`\ +\ :kbd:`Y` (``yank-pop``) to rotate to the previous kill.

Copy and paste from outside are also supported, both via the :kbd:`Control`\ +\ :kbd:`X` / :kbd:`Control`\ +\ :kbd:`V` bindings (the ``fish_clipboard_copy`` and ``fish_clipboard_paste`` functions [#]_) and via the terminal's paste function, for which fish enables "Bracketed Paste Mode", so it can tell a paste from manually entered text.
In addition, when pasting inside single quotes, pasted single quotes and backslashes are automatically escaped so that the result can be used as a single token simply by closing the quote after.

.. [#] These rely on external tools. Currently xsel, xclip, wl-copy/wl-paste and pbcopy/pbpaste are supported.

.. _multiline:

Multiline editing
-----------------

The fish commandline editor can be used to work on commands that are several lines long. There are three ways to make a command span more than a single line:

- Pressing the :kbd:`Enter` key while a block of commands is unclosed, such as when one or more block commands such as ``for``, ``begin`` or ``if`` do not have a corresponding :ref:`end <cmd-end>` command.

- Pressing :kbd:`Alt`\ +\ :kbd:`Enter` instead of pressing the :kbd:`Enter` key.

- By inserting a backslash (``\``) character before pressing the :kbd:`Enter` key, escaping the newline.

The fish commandline editor works exactly the same in single line mode and in multiline mode. To move between lines use the left and right arrow keys and other such keyboard shortcuts.

.. _history-search:

Searchable command history
--------------------------

After a command has been executed, it is remembered in the history list. Any duplicate history items are automatically removed. By pressing the up and down keys, you can search forwards and backwards in the history. If the current command line is not empty when starting a history search, only the commands containing the string entered into the command line are shown.

By pressing :kbd:`Alt`\ +\ :kbd:`↑` and :kbd:`Alt`\ +\ :kbd:`↓`, a history search is also performed, but instead of searching for a complete commandline, each commandline is broken into separate elements just like it would be before execution, and the history is searched for an element matching that under the cursor.

History searches are case-insensitive unless the search string contains an uppercase character, and they can be aborted by pressing the escape key.

Prefixing the commandline with a space will prevent the entire line from being stored in the history.

The command history is stored in the file ``~/.local/share/fish/fish_history`` (or
``$XDG_DATA_HOME/fish/fish_history`` if that variable is set) by default. However, you can set the
``fish_history`` environment variable to change the name of the history session (resulting in a
``<session>_history`` file); both before starting the shell and while the shell is running.

See the :ref:`history <cmd-history>` command for other manipulations.

Examples:

To search for previous entries containing the word 'make', type ``make`` in the console and press the up key.

If the commandline reads ``cd m``, place the cursor over the ``m`` character and press :kbd:`Alt`\ +\ :kbd:`↑` to search for previously typed words containing 'm'.

Navigating directories
======================

.. _directory-history:

The current working directory can be displayed with the :ref:`pwd <cmd-pwd>` command, or the ``$PWD`` :ref:`special variable <variables-special>`.

Directory history
-----------------

Fish automatically keeps a trail of the recent visited directories with :ref:`cd <cmd-cd>` by storing this history in the ``dirprev`` and ``dirnext`` variables.

Several commands are provided to interact with this directory history:

- :ref:`dirh <cmd-dirh>` prints the history
- :ref:`cdh <cmd-cdh>` displays a prompt to quickly navigate the history
- :ref:`prevd <cmd-prevd>` moves backward through the history. It is bound to :kbd:`Alt`\ +\ :kbd:`←`
- :ref:`nextd <cmd-nextd>` moves forward through the history. It is bound to :kbd:`Alt`\ +\ :kbd:`→`

.. _directory-stack:

Directory stack
---------------

Another set of commands, usually also available in other shells like bash, deal with the directory stack. Stack handling is not automatic and needs explicit calls of the following commands:

- :ref:`dirs <cmd-dirs>` prints the stack
- :ref:`pushd <cmd-pushd>` adds a directory on top of the stack and makes it the current working directory
- :ref:`popd <cmd-popd>` removes the directory on top of the stack and changes the current working directory

.. _job-control:

Running multiple programs
=========================

Normally when fish starts a program, this program will be put in the foreground, meaning it will take control of the terminal and fish will be stopped until the program finishes. Sometimes this is not desirable. For example, you may wish to start an application with a graphical user interface from the terminal, and then be able to continue using the shell. There are several ways to do this:

- Fish puts commands ending with the ``&`` (ampersand) symbol into the background. A background process will be run simultaneous with fish. Fish will keep control of the terminal, so the program will not be able to read from the keyboard.

- By pressing :kbd:`Control`\ +\ :kbd:`Z`, you can stop a currently running foreground  program and returns control to fish. Some programs do not support this feature, or remap it to another key. GNU Emacs uses :kbd:`Control`\ +\ :kbd:`X` :kbd:`Z` to stop running.

- By using the :ref:`bg <cmd-bg>` and :ref:`fg <cmd-fg>` builtin commands, you can send any currently running job into the foreground or background.

Note that functions cannot be started in the background. Functions that are stopped and then restarted in the background using the :ref:`bg <cmd-bg>` command will not execute correctly.


.. _initialization:

Initialization files
====================

On startup, Fish evaluates a number of configuration files, which can be used to control the behavior of the shell. The location of these is controlled by a number of environment variables, and their default or usual location is given below.

Configuration files are evaluated in the following order:

- Configuration shipped with fish, which should not be edited, in ``$__fish_data_dir/config.fish`` (usually ``/usr/share/fish/config.fish``).

- Configuration snippets in files ending in ``.fish``, in the directories:

  - ``$__fish_config_dir/conf.d`` (by default, ``~/.config/fish/conf.d/``)
  - ``$__fish_sysconf_dir/conf.d`` (by default, ``/etc/fish/conf.d/``)
  - Directories for third-party software vendors to ship their own configuration snippets for their software. Fish searches the directories in the ``XDG_DATA_DIRS`` environment variable for a ``fish/vendor_conf.d`` directory; if this variable is not defined, the default is usually to search ``/usr/share/fish/vendor_conf.d`` and ``/usr/local/share/fish/vendor_conf.d``

  If there are multiple files with the same name in these directories, only the first will be executed.
  They are executed in order of their filename, sorted (like globs) in a natural order (i.e. "01" sorts before "2").

- System-wide configuration files, where administrators can include initialization that should be run for all users on the system - similar to ``/etc/profile`` for POSIX-style shells - in ``$__fish_sysconf_dir`` (usually ``/etc/fish/config.fish``).
- User initialization, usually in ``~/.config/fish/config.fish`` (controlled by the ``XDG_CONFIG_HOME`` environment variable, and accessible as ``$__fish_config_dir``).

These paths are controlled by parameters set at build, install, or run time, and may vary from the defaults listed above.

This wide search may be confusing. If you are unsure where to put your own customisations, use ``~/.config/fish/config.fish``.

Note that ``~/.config/fish/config.fish`` is sourced `after` the snippets. This is so users can copy snippets and override some of their behavior.

These files are all executed on the startup of every shell. If you want to run a command only on starting an interactive shell, use the exit status of the command ``status --is-interactive`` to determine if the shell is interactive. If you want to run a command only when using a login shell, use ``status --is-login`` instead. This will speed up the starting of non-interactive or non-login shells.

If you are developing another program, you may wish to install configuration which is run for all users of the fish shell on a system. This is discouraged; if not carefully written, they may have side-effects or slow the startup of the shell. Additionally, users of other shells will not benefit from the Fish-specific configuration. However, if they are absolutely required, you may install them to the "vendor" configuration directory. As this path may vary from system to system, the ``pkgconfig`` framework should be used to discover this path with the output of ``pkg-config --variable confdir fish``.

Examples:

If you want to add the directory ``~/linux/bin`` to your PATH variable when using a login shell, add the following to your ``~/.config/fish/config.fish`` file::

    if status --is-login
        set -x PATH $PATH ~/linux/bin
    end


If you want to run a set of commands when fish exits, use an `event handler <#event>`_ that is triggered by the exit of the shell::


    function on_exit --on-event fish_exit
        echo fish is now exiting
    end

.. _featureflags:

Future feature flags
====================

Feature flags are how fish stages changes that might break scripts. Breaking changes are introduced as opt-in, in a few releases they become opt-out, and eventually the old behavior is removed.

You can see the current list of features via ``status features``::

    > status features
    stderr-nocaret  on     3.0      ^ no longer redirects stderr
    qmark-noglob    off    3.0      ? no longer globs
    regex-easyesc   off    3.1      string replace -r needs fewer \\'s

There are two breaking changes in fish 3.0: caret ``^`` no longer redirects stderr, and question mark ``?`` is no longer a glob.

There is one breaking change in fish 3.1: ``string replace -r`` does a superfluous round of escaping for the replacement, so escaping backslashes would look like ``string replace -ra '([ab])' '\\\\\\\$1' a``. This flag removes that if turned on, so ``'\\\\$1'`` is enough.


These changes are off by default. They can be enabled on a per session basis::

    > fish --features qmark-noglob,stderr-nocaret


or opted into globally for a user::


    > set -U fish_features stderr-nocaret qmark-noglob

Features will only be set on startup, so this variable will only take effect if it is universal or exported.

You can also use the version as a group, so ``3.0`` is equivalent to "stderr-nocaret" and "qmark-noglob".

Prefixing a feature with ``no-`` turns it off instead.

.. _other:

Other features
==============

.. _color:

Syntax highlighting
-------------------

Fish interprets the command line as it is typed and uses syntax highlighting to provide feedback. The most important feedback is the detection of potential errors. By default, errors are marked red.

Detected errors include:

- Non existing commands.
- Reading from or appending to a non existing file.
- Incorrect use of output redirects
- Mismatched parenthesis


When the cursor is over a parenthesis or a quote, fish also highlights its matching quote or parenthesis.

To customize the syntax highlighting, you can set the environment variables listed in the `Variables for changing highlighting colors <#variables-color>`__ section.

.. _title:

Programmable title
------------------

When using most virtual terminals, it is possible to set the message displayed in the titlebar of the terminal window. This can be done automatically in fish by defining the :ref:`fish_title <cmd-fish_title>` function. The :ref:`fish_title <cmd-fish_title>` function is executed before and after a new command is executed or put into the foreground and the output is used as a titlebar message. The :ref:`status current-command <cmd-status>` builtin will always return the name of the job to be put into the foreground (or ``fish`` if control is returning to the shell) when the `fish_prompt <cmd-fish_prompt>` function is called. The first argument to fish_title will contain the most recently executed foreground command as a string, starting with fish 2.2.

Examples:
The default fish title is::


    function fish_title
        echo (status current-command) ' '
        pwd
    end

To show the last command in the title::

    function fish_title
        echo $argv[1]
    end

.. _prompt:

Programmable prompt
-------------------

When fish waits for input, it will display a prompt by evaluating the :ref:`fish_prompt <cmd-fish_prompt>` and :ref:`fish_right_prompt <cmd-fish_right_prompt>` functions. The output of the former is displayed on the left and the latter's output on the right side of the terminal. The output of :ref:`fish_mode_prompt <cmd-fish_mode_prompt>` will be prepended on the left, though the default function only does this when in `vi-mode <#vi-mode>`__.

.. _greeting:

Configurable greeting
---------------------

If a function named :ref:`fish_greeting <cmd-fish_greeting>` exists, it will be run when entering interactive mode. Otherwise, if an environment variable named :ref:`fish_greeting <cmd-fish_greeting>` exists, it will be printed.

.. _private-mode:

Private mode
-------------

fish supports launching in private mode via ``fish --private`` (or ``fish -P`` for short). In private mode, old history is not available and any interactive commands you execute will not be appended to the global history file, making it useful both for avoiding inadvertently leaking personal information (e.g. for screencasts) and when dealing with sensitive information to prevent it being persisted to disk. You can query the global variable ``fish_private_mode`` (``if set -q fish_private_mode ...``) if you would like to respect the user's wish for privacy and alter the behavior of your own fish scripts.

.. _event:

Event handlers
---------------

When defining a new function in fish, it is possible to make it into an event handler, i.e. a function that is automatically run when a specific event takes place. Events that can trigger a handler currently are:

- When a signal is delivered
- When a job exits
- When the value of a variable is updated
- When the prompt is about to be shown

Example:

To specify a signal handler for the WINCH signal, write::

    function my_signal_handler --on-signal WINCH
        echo Got WINCH signal!
    end

Please note that event handlers only become active when a function is loaded, which means you might need to otherwise :ref:`source <cmd-source>` or execute a function instead of relying on :ref:`autoloading <syntax-function-autoloading>`. One approach is to put it into your :ref:`initialization file <initialization>`.

For more information on how to define new event handlers, see the documentation for the :ref:`function <cmd-function>` command.


.. _debugging:

Debugging fish scripts
-----------------------

Fish includes a built in debugging facility. The debugger allows you to stop execution of a script at an arbitrary point. When this happens you are presented with an interactive prompt. At this prompt you can execute any fish command (there are no debug commands as such). For example, you can check or change the value of any variables using :ref:`printf <cmd-printf>` and :ref:`set <cmd-set>`. As another example, you can run :ref:`status print-stack-trace <cmd-status>` to see how this breakpoint was reached. To resume normal execution of the script, simply type :ref:`exit <cmd-exit>` or :kbd:`Control`\ +\ :kbd:`D`.

To start a debug session simply run the builtin command :ref:`breakpoint <cmd-breakpoint>` at the point in a function or script where you wish to gain control. Also, the default action of the TRAP signal is to call this builtin. So a running script can be debugged by sending it the TRAP signal with the ``kill`` command. Once in the debugger, it is easy to insert new breakpoints by using the funced function to edit the definition of a function.

.. _more-help:

Further help and development
============================

If you have a question not answered by this documentation, there are several avenues for help:

- The `GitHub page <https://github.com/fish-shell/fish-shell/>`_

- The official `Gitter channel <https://gitter.im/fish-shell/fish-shell>`_

- The official mailing list at `fish-users@lists.sourceforge.net <https://lists.sourceforge.net/lists/listinfo/fish-users>`_

- The IRC channel, \#fish on ``irc.oftc.net``

If you have an improvement for fish, you can submit it via the GitHub page.

.. _other_pages:

Other help pages
================
.. toctree::
   :maxdepth: 1

   self
   commands
   design
   tutorial
   completions
   faq
   license
   CHANGELOG
   fish_for_bash_users
