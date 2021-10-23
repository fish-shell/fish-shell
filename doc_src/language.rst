.. _language:

The fish language
=================

This document is a comprehensive overview of fish's scripting language.

For interactive features see :ref:`Interactive use <interactive>`.

.. _syntax:

Syntax overview
---------------

Shells like fish are used by giving them commands. A command is executed by writing the name of the command followed by any arguments. For example::

    echo hello world

This calls the :ref:`echo <cmd-echo>` command. ``echo`` writes its arguments to the screen. In this example the output is ``hello world``.

Everything in fish is done with commands. There are commands for repeating other commands, commands for assigning variables, commands for treating a group of commands as a single command, etc. All of these commands follow the same basic syntax.

To learn more about the ``echo`` command, read its manual page by typing ``man echo``. ``man`` is a command for displaying a manual page on a given topic. It takes the name of the manual page to display as an argument. There are manual pages for almost every command. There are also manual pages for many other things, such as system libraries and important files.

Every program on your computer can be used as a command in fish. If the program file is located in one of the :ref:`PATH <PATH>` directories, you can just type the name of the program to use it. Otherwise the whole filename, including the directory (like ``/home/me/code/checkers/checkers`` or ``../checkers``) is required.

Here is a list of some useful commands:

- :ref:`cd <cmd-cd>`: Change the current directory
- ``ls``: List files and directories
- ``man``: Display a manual page
- ``mv``: Move (rename) files
- ``cp``: Copy files
- :ref:`open <cmd-open>`: Open files with the default application associated with each filetype
- ``less``: Display the contents of files

Commands and arguments are separated by the space character ``' '``. Every command ends with either a newline (by pressing the return key) or a semicolon ``;``. Multiple commands can be written on the same line by separating them with semicolons.

A switch is a very common special type of argument. Switches almost always start with one or more hyphens ``-`` and alter the way a command operates. For example, the ``ls`` command usually lists the names of all files and directories in the current working directory. By using the ``-l`` switch, the behavior of ``ls`` is changed to not only display the filename, but also the size, permissions, owner, and modification time of each file.

Switches differ between commands and are usually documented on a command's manual page. There are some switches, however, that are common to most commands. For example, ``--help`` will usually display a help text, ``--version`` will usually display the command version, and ``-i`` will often turn on interactive prompting before taking action.

.. _terminology:

Terminology
-----------

Here we define some of the terms used on this page and throughout the rest of the fish documentation:

- **Argument**: A parameter given to a command.

- **Builtin**: A command that is implemented by the shell. Builtins are so closely tied to the operation of the shell that it is impossible to implement them as external commands.

- **Command**: A program that the shell can run, or more specifically an external program that the shell runs in another process.

- **Function**: A block of commands that can be called as if they were a single command. By using functions, it is possible to string together multiple simple commands into one more advanced command.

- **Job**: A running pipeline or command.

- **Pipeline**: A set of commands strung together so that the output of one command is the input of the next command.

- **Redirection**: An operation that changes one of the input or output streams associated with a job.

- **Switch** or **Option**: A special kind of argument that alters the behavior of a command. A switch almost always begins with one or two hyphens.

.. _quotes:

Quotes
------

Sometimes features like :ref:`parameter expansion <expand>` and :ref:`character escapes <escapes>` get in the way. When that happens, you can use quotes, either single (``'``) or double (``"``). Between single quotes, fish performs no expansions. Between double quotes, fish only performs :ref:`variable expansion <expand-variable>`. No other kind of expansion (including :ref:`brace expansion <expand-brace>` or parameter expansion) is performed, and escape sequences (for example, ``\n``) are ignored. Within quotes, whitespace is not used to separate arguments, allowing quoted arguments to contain spaces.

The only meaningful escape sequences in single quotes are ``\'``, which escapes a single quote and ``\\``, which escapes the backslash symbol. The only meaningful escapes in double quotes are ``\"``, which escapes a double quote, ``\$``, which escapes a dollar character, ``\`` followed by a newline, which deletes the backslash and the newline, and ``\\``, which escapes the backslash symbol.

Single quotes have no special meaning within double quotes and vice versa.

Example::

    rm "cumbersome filename.txt"

removes the file ``cumbersome filename.txt``, while

::

    rm cumbersome filename.txt

removes two files, ``cumbersome`` and ``filename.txt``.

Another example::

    grep 'enabled)$' foo.txt

searches for lines ending in ``enabled)`` in ``foo.txt`` (the ``$`` is special to ``grep``: it matches the end of the line).

.. _escapes:

Escaping Characters
-------------------

Some characters cannot be written directly on the command line. For these characters, so-called escape sequences are provided. These are:

- ``\a`` represents the alert character.
- ``\e`` represents the escape character.
- ``\f`` represents the form feed character.
- ``\n`` represents a newline character.
- ``\r`` represents the carriage return character.
- ``\t`` represents the tab character.
- ``\v`` represents the vertical tab character.
- ``\xHH``, where ``HH`` is a hexadecimal number, represents the ASCII character with the specified value. For example, ``\x9`` is the tab character.
- ``\XHH``, where ``HH`` is a hexadecimal number, represents a byte of data with the specified value. If you are using a multibyte encoding, this can be used to enter invalid strings. Only use this if you know what you are doing.
- ``\ooo``, where ``ooo`` is an octal number, represents the ASCII character with the specified value. For example, ``\011`` is the tab character.
- ``\uXXXX``, where ``XXXX`` is a hexadecimal number, represents the 16-bit Unicode character with the specified value. For example, ``\u9`` is the tab character.
- ``\UXXXXXXXX``, where ``XXXXXXXX`` is a hexadecimal number, represents the 32-bit Unicode character with the specified value. For example, ``\U9`` is the tab character.
- ``\cX``, where ``X`` is a letter of the alphabet, represents the control sequence generated by pressing the control key and the specified letter. For example, ``\ci`` is the tab character

Some characters have special meaning to the shell. For example, an apostrophe ``'`` disables expansion (see :ref:`Quotes<quotes>`). To tell the shell to treat these characters literally, escape them with a backslash. For example, the command::

    echo \'hello world\'

outputs ``'hello world'`` (including the apostrophes), while the command::

    echo 'hello world'

outputs ``hello world`` (without the apostrophes). In the former case the shell treats the apostrophes as literal ``'`` characters, while in the latter case it treats them as special expansion modifiers.

The special characters and their escape sequences are:

- :code:`\ ` (backslash space) escapes the space character. This keeps the shell from splitting arguments on the escaped space.
- ``\$`` escapes the dollar character.
- ``\\`` escapes the backslash character.
- ``\*`` escapes the star character.
- ``\?`` escapes the question mark character (this is not necessary if the ``qmark-noglob`` :ref:`feature flag<featureflags>` is enabled).
- ``\~`` escapes the tilde character.
- ``\#`` escapes the hash character.
- ``\(`` escapes the left parenthesis character.
- ``\)`` escapes the right parenthesis character.
- ``\{`` escapes the left curly bracket character.
- ``\}`` escapes the right curly bracket character.
- ``\[`` escapes the left bracket character.
- ``\]`` escapes the right bracket character.
- ``\<`` escapes the less than character.
- ``\>`` escapes the more than character.
- ``\^`` escapes the circumflex character.
- ``\&`` escapes the ampersand character.
- ``\|`` escapes the vertical bar character.
- ``\;`` escapes the semicolon character.
- ``\"`` escapes the quote character.
- ``\'`` escapes the apostrophe character.

.. _redirects:

Input/Output Redirection
-----------------------------

Most programs use three input/output (I/O) streams:

- Standard input (stdin) for reading. Defaults to reading from the keyboard.
- Standard output (stdout) for writing output. Defaults to writing to the screen.
- Standard error (stderr) for writing errors and warnings. Defaults to writing to the screen.

Each stream has a number called the file descriptor (FD): 0 for stdin, 1 for stdout, and 2 for stderr.

The destination of a stream can be changed using something called *redirection*. For example, ``echo hello > output.txt``, redirects the standard output of the ``echo`` command to a text file.

- To read standard input from a file, use ``<SOURCE_FILE``.
- To write standard output to a file, use ``>DESTINATION``.
- To write standard error to a file, use ``2>DESTINATION``. [#]_
- To append standard output to a file, use ``>>DESTINATION_FILE``.
- To append standard error to a file, use ``2>>DESTINATION_FILE``.
- To not overwrite ("clobber") an existing file, use ``>?DESTINATION`` or ``2>?DESTINATION``. This is known as the "noclobber" redirection.

``DESTINATION`` can be one of the following:

- A filename. The output will be written to the specified file. Often ``>/dev/null`` to silence output by writing it to the special "sinkhole" file.
- An ampersand (``&``) followed by the number of another file descriptor like ``&2`` for standard error. The output will be written to the destination descriptor.
- An ampersand followed by a minus sign (``&-``). The file descriptor will be closed.

As a convenience, the redirection ``&>`` can be used to direct both stdout and stderr to the same destination. For example, ``echo hello &> all_output.txt`` redirects both stdout and stderr to the file ``all_output.txt``. This is equivalent to ``echo hello > all_output.txt 2>&1``.

Any arbitrary file descriptor can used in a redirection by prefixing the redirection with the FD number.

- To redirect the input of descriptor N, use ``N<DESTINATION``.
- To redirect the output of descriptor N, use ``N>DESTINATION``.
- To append the output of descriptor N to a file, use ``N>>DESTINATION_FILE``.

For example, ``echo hello 2> output.stderr`` writes the standard error (file descriptor 2) to ``output.stderr``.

It is an error to redirect a builtin, function, or block to a file descriptor above 2. However this is supported for external commands.

.. [#] Previous versions of fish also allowed specifying this as ``^DESTINATION``, but that made another character special so it was deprecated and will be removed in the future. See :ref:`feature flags<featureflags>`.

.. _pipes:

Piping
------

Another way to redirect streams is a *pipe*. A pipe connects streams with each other. Usually the standard output of one command is connected with the standard input of another. This is done by separating commands with the pipe character ``|``. For example::

    cat foo.txt | head

The command ``cat foo.txt`` sends the contents of ``foo.txt`` to stdout. This output is provided as input for the ``head`` program, which prints the first 10 lines of its input.

It is possible to pipe a different output file descriptor by prepending its FD number and the output redirect symbol to the pipe. For example::

    make fish 2>| less

will attempt to build ``fish``, and any errors will be shown using the ``less`` pager. [#]_

As a convenience, the pipe ``&|`` redirects both stdout and stderr to the same process. This is different from bash, which uses ``|&``.

.. [#] A "pager" here is a program that takes output and "paginates" it. ``less`` doesn't just do pages, it allows arbitrary scrolling (even back!).

.. _syntax-job-control:

Job control
-----------

When you start a job in fish, fish itself will pause, and give control of the terminal to the program just started. Sometimes, you want to continue using the commandline, and have the job run in the background. To create a background job, append an \& (ampersand) to your command. This will tell fish to run the job in the background. Background jobs are very useful when running programs that have a graphical user interface.

Example::

  emacs &


will start the emacs text editor in the background. :ref:`fg <cmd-fg>` can be used to bring it into the foreground again when needed.

Most programs allow you to suspend the program's execution and return control to fish by pressing :kbd:`Control`\ +\ :kbd:`Z` (also referred to as ``^Z``). Once back at the fish commandline, you can start other programs and do anything you want. If you then want you can go back to the suspended command by using the :ref:`fg <cmd-fg>` (foreground) command.

If you instead want to put a suspended job into the background, use the :ref:`bg <cmd-bg>` command.

To get a listing of all currently started jobs, use the :ref:`jobs <cmd-jobs>` command.
These listed jobs can be removed with the :ref:`disown <cmd-disown>` command.

At the moment, functions cannot be started in the background. Functions that are stopped and then restarted in the background using the :ref:`bg <cmd-bg>` command will not execute correctly.

If the ``&`` character is followed by a non-separating character, it is not interpreted as background operator. Separating characters are whitespace and the characters ``;<>&|``.

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
      # A simple prompt. Displays the current directory
      # (which fish stores in the $PWD variable)
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
^^^^^^^^^^^^^^^^

One of the most common uses for functions is to slightly alter the behavior of an already existing command. For example, one might want to redefine the ``ls`` command to display colors. The switch for turning on colors on GNU systems is ``--color=auto``. An alias, or wrapper, around ``ls`` might look like this::

  function ls
      command ls --color=auto $argv
  end

There are a few important things that need to be noted about aliases:

- Always take care to add the :ref:`$argv <variables-argv>` variable to the list of parameters to the wrapped command. This makes sure that if the user specifies any additional parameters to the function, they are passed on to the underlying command.

- If the alias has the same name as the aliased command, you need to prefix the call to the program with ``command`` to tell fish that the function should not call itself, but rather a command with the same name. If you forget to do so, the function would call itself until the end of time. Usually fish is smart enough to figure this out and will refrain from doing so (which is hopefully in your interest).


To easily create a function of this form, you can use the :ref:`alias <cmd-alias>` command. Unlike other shells, this just makes functions - fish has no separate concept of an "alias", we just use the word for a function wrapper like this. :ref:`alias <cmd-alias>` immediately creates a function. Consider using ``alias --save`` or :ref:`funcsave <cmd-funcsave>` to save the created function into an autoload file instead of recreating the alias each time.

For an alternative, try :ref:`abbreviations <abbreviations>`. These are words that are expanded while you type, instead of being actual functions inside the shell.

.. _syntax-function-autoloading:

Autoloading functions
^^^^^^^^^^^^^^^^^^^^^

Functions can be defined on the commandline or in a configuration file, but they can also be automatically loaded. This has some advantages:

- An autoloaded function becomes available automatically to all running shells.
- If the function definition is changed, all running shells will automatically reload the altered version, after a while.
- Startup time and memory usage is improved, etc.

When fish needs to load a function, it searches through any directories in the :ref:`list variable <variables-lists>` ``$fish_function_path`` for a file with a name consisting of the name of the function plus the suffix ``.fish`` and loads the first it finds.

For example if you try to execute something called ``banana``, fish will go through all directories in $fish_function_path looking for a file called ``banana.fish`` and load the first one it finds.

By default ``$fish_function_path`` contains the following:

- A directory for users to keep their own functions, usually ``~/.config/fish/functions`` (controlled by the ``XDG_CONFIG_HOME`` environment variable).
- A directory for functions for all users on the system, usually ``/etc/fish/functions`` (really ``$__fish_sysconfdir/functions``).
- Directories for other software to put their own functions. These are in the directories in the ``XDG_DATA_DIRS`` environment variable, in a subdirectory called ``fish/vendor_functions.d``. The default is usually ``/usr/share/fish/vendor_functions.d`` and ``/usr/local/share/fish/vendor_functions.d``.
- The functions shipped with fish, usually installed in ``/usr/share/fish/functions`` (really ``$__fish_data_dir/functions``).

If you are unsure, your functions probably belong in ``~/.config/fish/functions``.

As we've explained, autoload files are loaded *by name*, so, while you can put multiple functions into one file, the file will only be loaded automatically once you try to execute the one that shares the name.

Autoloading also won't work for :ref:`event handlers <event>`, since fish cannot know that a function is supposed to be executed when an event occurs when it hasn't yet loaded the function. See the :ref:`event handlers <event>` section for more information.

If a file of the right name doesn't define the function, fish will not read other autoload files, instead it will go on to try builtins and finally commands. This allows masking a function defined later in $fish_function_path, e.g. if your administrator has put something into /etc/fish/functions that you want to skip.

If you are developing another program and want to install fish functions for it, install them to the "vendor" functions directory. As this path varies from system to system, you can use ``pkgconfig`` to discover it with the output of ``pkg-config --variable functionsdir fish``. Your installation system should support a custom path to override the pkgconfig path, as other distributors may need to alter it easily.

Comments
--------

Anything after a ``#`` until the end of the line is a comment. That means it's purely for the reader's benefit, fish ignores it.

This is useful to explain what and why you are doing something::

  function ls
      # The function is called ls,
      # so we have to explicitly call `command ls` to avoid calling ourselves.
      command ls --color=auto $argv
  end

There are no multiline comments. If you want to make a comment span multiple lines, simply start each line with a ``#``.

Comments can also appear after a line like so::

  set -gx EDITOR emacs # I don't like vim.

.. _syntax-conditional:

Conditions
----------

Fish has some builtins that let you execute commands only if a specific criterion is met: :ref:`if <cmd-if>`, :ref:`switch <cmd-switch>`, :ref:`and <cmd-and>` and :ref:`or <cmd-or>`, and also the familiar :ref:`&&/|| <tut-combiners>` syntax.

The :ref:`switch <cmd-switch>` command is used to execute one of possibly many blocks of commands depending on the value of a string. See the documentation for :ref:`switch <cmd-switch>` for more information.

The other conditionals use the :ref:`exit status <variables-status>` of a command to decide if a command or a block of commands should be executed.

Unlike programming languages you might know, :ref:`if <cmd-if>` doesn't take a *condition*, it takes a *command*. If that command returned a successful :ref:`exit status <variables-status>` (that's 0), the ``if`` branch is taken, otherwise the :ref:`else <cmd-else>` branch.

To check a condition, there is the :ref:`test <cmd-test>` command::

  if test 5 -gt 2
      echo Yes, five is greater than two
  end

Some examples::

  # Just see if the file contains the string "fish" anywhere.
  # This executes the `grep` command, which searches for a string,
  # and if it finds it returns a status of 0.
  # The `-q` switch stops it from printing any matches.
  if grep -q fish myanimals
      echo "You have fish!"
  else
      echo "You don't have fish!"
  end

  # $XDG_CONFIG_HOME is a standard place to store configuration.
  # If it's not set applications should use ~/.config.
  set -q XDG_CONFIG_HOME; and set -l configdir $XDG_CONFIG_HOME
  or set -l configdir ~/.config

For more, see the documentation for the builtins or the :ref:`Conditionals <tut-conditionals>` section of the tutorial.

.. _syntax-loops-and-blocks:

Loops and blocks
----------------

Like most programming language, fish also has the familiar :ref:`while <cmd-while>` and :ref:`for <cmd-for>` loops.

``while`` works like a repeated :ref:`if <cmd-if>`::

  while true
      echo Still running
      sleep 1
  end

will print "Still running" once a second. You can abort it with ctrl-c.

``for`` loops work like in other shells, which is more like python's for-loops than e.g. C's::

  for file in *
      echo file: $file
  end

will print each file in the current directory. The part after the ``in`` is just a list of arguments, so you can use any :ref:`expansions <expand>` there::

  set moreanimals bird fox
  for animal in {cat,}fish dog $moreanimals
     echo I like the $animal
  end

If you need a list of numbers, you can use the ``seq`` command to create one::

  for i in (seq 1 5)
      echo $i
  end

:ref:`break <cmd-break>` is available to break out of a loop, and :ref:`continue <cmd-continue>` to jump to the next iteration.

:ref:`Input and output redirections <redirects>` (including :ref:`pipes <pipes>`) can also be applied to loops::

  while read -l line
      echo line: $line
  end < file

In addition there's a :ref:`begin <cmd-begin>` block that just groups commands together so you can redirect to a block or use a new :ref:`variable scope <variables-scope>` without any repetition::

  begin
     set -l foo bar # this variable will only be available in this block!
  end

.. _expand:

Parameter expansion
-------------------

When fish is given a commandline, it expands the parameters before sending them to the command. There are multiple different kinds of expansions:

- :ref:`Wildcards <expand-wildcard>`, to create filenames from patterns
- :ref:`Variable expansion <expand-variable>`, to use the value of a variable
- :ref:`Command substitution <expand-command-substitution>`, to use the output of another command
- :ref:`Brace expansion <expand-brace>`, to write lists with common pre- or suffixes in a shorter way
- :ref:`Tilde expansion <expand-home>`, to turn the ``~`` at the beginning of paths into the path to the home directory

Parameter expansion is limited to 524288 items. There is a limit to how many arguments the operating system allows for any command, and 524288 is far above it. This is a measure to stop the shell from hanging doing useless computation.

.. _expand-wildcard:

Wildcards ("Globbing")
^^^^^^^^^^^^^^^^^^^^^^

When a parameter includes an :ref:`unquoted <quotes>` ``*`` star (or "asterisk") or a ``?`` question mark, fish uses it as a wildcard to match files.

- ``*`` matches any number of characters (including zero) in a file name, not including ``/``.

- ``**`` matches any number of characters (including zero), and also descends into subdirectories. If ``**`` is a segment by itself, that segment may match zero times, for compatibility with other shells.

- ``?`` can match any single character except ``/``. This is deprecated and can be disabled via the ``qmark-noglob`` :ref:`feature flag<featureflags>`, so ``?`` will just be an ordinary character.

Wildcard matches are sorted case insensitively. When sorting matches containing numbers, they are naturally sorted, so that the strings '1' '5' and '12' would be sorted like 1, 5, 12.

Hidden files (where the name begins with a dot) are not considered when wildcarding unless the wildcard string has a dot in that place.

Examples:

- ``a*`` matches any files beginning with an 'a' in the current directory.

- ``**`` matches any files and directories in the current directory and all of its subdirectories.

- ``~/.*`` matches all hidden files (also known as "dotfiles") and directories in your home directory.

For most commands, if any wildcard fails to expand, the command is not executed, :ref:`$status <variables-status>` is set to nonzero, and a warning is printed. This behavior is like what bash does with ``shopt -s failglob``. There are exactly 4 exceptions, namely :ref:`set <cmd-set>`, overriding variables in :ref:`overrides <variables-override>`, :ref:`count <cmd-count>` and :ref:`for <cmd-for>`. Their globs will instead expand to zero arguments (so the command won't see them at all), like with ``shopt -s nullglob`` in bash.

Examples::

    # List the .foo files, or warns if there aren't any.
    ls *.foo

    # List the .foo files, if any.
    set foos *.foo
    if count $foos >/dev/null
        ls $foos
    end

Unlike bash (by default), fish will not pass on the literal glob character if no match was found, so for a command like ``apt install`` that does the matching itself, you need to add quotes::

    apt install "ncurses-*"

.. [#] Technically, unix allows filenames with newlines, and this splits the ``find`` output on newlines. If you want to avoid that, use find's ``-print0`` option and :ref:`string split0<cmd-string-split0>`.

.. _expand-variable:

Variable expansion
^^^^^^^^^^^^^^^^^^

One of the most important expansions in fish is the "variable expansion". This is the replacing of a dollar sign (``$``) followed by a variable name with the _value_ of that variable. For more on shell variables, read the :ref:`Shell variables <variables>` section.

In the simplest case, this is just something like::

    echo $HOME

which will replace ``$HOME`` with the home directory of the current user, and pass it to :ref:`echo <cmd-echo>`, which will then print it.

Some variables like ``$HOME`` are already set because fish sets them by default or because fish's parent process passed them to fish when it started it. You can define your own variables by setting them with :ref:`set <cmd-set>`::

    set my_directory /home/cooluser/mystuff
    ls $my_directory
    # shows the contents of /home/cooluser/mystuff

For more on how setting variables works, see :ref:`Shell variables <variables>` and the following sections.

Sometimes a variable has no value because it is undefined or empty, and it expands to nothing::


    echo $nonexistentvariable
    # Prints no output.

To separate a variable name from text you can encase the variable within double-quotes or braces::

    set WORD cat
    echo The plural of $WORD is "$WORD"s
    # Prints "The plural of cat is cats" because $WORD is set to "cat".
    echo The plural of $WORD is {$WORD}s
    # ditto

Without the quotes or braces, fish will try to expand a variable called ``$WORDs``, which may not exist.

The latter syntax ``{$WORD}`` is a special case of :ref:`brace expansion <expand-brace>`.

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

``$$foo[$i]`` is "the value of the variable named by ``$foo[$i]``.

When using this feature together with list brackets, the brackets will be used from the inside out. ``$$foo[5]`` will use the fifth element of ``$foo`` as a variable name, instead of giving the fifth element of all the variables $foo refers to. That would instead be expressed as ``$$foo[1][5]`` (take the first element of ``$foo``, use it as a variable name, then give the fifth element of that).

.. [#] Unlike bash or zsh, which will join with the first character of $IFS (which usually is space).

.. _expand-command-substitution:

Command substitution
^^^^^^^^^^^^^^^^^^^^

The output of a command (or an entire :ref:`pipeline <pipes>`) can be used as the arguments to another command.

When you write a command in parenthesis like ``outercommand (innercommand)``, the ``innercommand`` will be executed first. Its output will be taken and each line given as a separate argument to ``outercommand``, which will then be executed. [#]_

A command substitution can have a dollar sign before the opening parenthesis like ``outercommand $(innercommand)``. This variant is also allowed inside double quotes. When using double quotes, the command output is not split up by lines.

If the output is piped to :ref:`string split or string split0 <cmd-string-split>` as the last step, those splits are used as they appear instead of splitting lines.

The exit status of the last run command substitution is available in the :ref:`status <variables-status>` variable if the substitution happens in the context of a :ref:`set <cmd-set>` command (so ``if set -l (something)`` checks if ``something`` returned true).

To use only part of the output, refer to :ref:`index range expansion <expand-index-range>`.

Fish has a default limit of 100 MiB on the data it will read in a command sustitution. If that limit is reached the command (all of it, not just the command substitution - the outer command won't be executed at all) fails and ``$status`` is set to 122. This is so command substitutions can't cause the system to go out of memory, because typically your operating system has a much lower limit, so reading more than that would be useless and harmful. This limit can be adjusted with the ``fish_read_limit`` variable (`0` meaning no limit). This limit also affects the :ref:`read <cmd-read>` command.

Examples::

    # Outputs 'image.png'.
    echo (basename image.jpg .jpg).png

    # Convert all JPEG files in the current directory to the
    # PNG format using the 'convert' program.
    for i in *.jpg; convert $i (basename $i .jpg).png; end

    # Set the ``data`` variable to the contents of 'data.txt'
    # without splitting it into a list.
    set data "$(cat data.txt)"

    # Set ``$data`` to the contents of data, splitting on NUL-bytes.
    set data (cat data | string split0)


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
^^^^^^^^^^^^^^^

Curly braces can be used to write comma-separated lists. They will be expanded with each element becoming a new parameter, with the surrounding string attached. This is useful to save on typing, and to separate a variable name from surrounding text.

Examples::

  > echo input.{c,h,txt}
  input.c input.h input.txt

  # Move all files with the suffix '.c' or '.h' to the subdirectory src.
  > mv *.{c,h} src/

  # Make a copy of `file` at `file.bak`.
  > cp file{,.bak}

  > set -l dogs hot cool cute "good "
  > echo {$dogs}dog
  hotdog cooldog cutedog good dog

If there is no "," or variable expansion between the curly braces, they will not be expanded::

    # This {} isn't special
    > echo foo-{}
    foo-{}
    # This passes "HEAD@{2}" to git
    > git reset --hard HEAD@{2}
    > echo {{a,b}}
    {a} {b} # because the inner brace pair is expanded, but the outer isn't.

If after expansion there is nothing between the braces, the argument will be removed (see :ref:`the cartesian product section <cartesian-product>`)::

    > echo foo-{$undefinedvar}
    # Output is an empty line, just like a bare `echo`.

If there is nothing between a brace and a comma or two commas, it's interpreted as an empty element::

    > echo {,,/usr}/bin
    /bin /bin /usr/bin

To use a "," as an element, :ref:`quote <quotes>` or :ref:`escape <escapes>` it.

.. _cartesian-product:

Combining lists (Cartesian Product)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

When lists are expanded with other parts attached, they are expanded with these parts still attached. Even if two lists are attached to each other, they are expanded in all combinations. This is referred to as the `cartesian product` (like in mathematics), and works basically like :ref:`brace expansion <expand-brace>`.

Examples::

    # Brace expansion is the most familiar:
    # All elements in the brace combine with the parts outside of the braces
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

This also happens after :ref:`command substitution <expand-command-substitution>`. To avoid tokens disappearing there, make the inner command return a trailing newline, or store the output in a variable and double-quote it.

E.g.

::

    >_ set b 1 2 3
    >_ echo (echo x)$b
    x1 x2 x3
    >_ echo (printf '%s' '')banana
    # the printf prints nothing, so this is nothing times "banana",
    # which is nothing.
    >_ echo (printf '%s\n' '')banana
    # the printf prints a newline,
    # so the command substitution expands to an empty string,
    # so this is `''banana`
    banana

This can be quite useful. For example, if you want to go through all the files in all the directories in $PATH, use::

    for file in $PATH/*

Because :ref:`$PATH <path>` is a list, this expands to all the files in all the directories in it. And if there are no directories in $PATH, the right answer here is to expand to no files.

.. _expand-index-range:

Index range expansion
^^^^^^^^^^^^^^^^^^^^^

Sometimes it's necessary to access only some of the elements of a :ref:`list <variables-lists>` (all fish variables are lists), or some of the lines a :ref:`command substitution <expand-command-substitution>` outputs. Both are possible in fish by writing a set of indices in brackets, like::

  # Make $var a list of four elements
  set var one two three four
  # Print the second:
  echo $var[2]
  # prints "two"
  # or print the first three:
  echo $var[1..3]
  # prints "one two three"

In index brackets, fish understands ranges written like ``a..b`` ('a' and 'b' being indices). They are expanded into a sequence of indices from a to b (so ``a a+1 a+2 ... b``), going up if b is larger and going down if a is larger. Negative indices can also be used - they are taken from the end of the list, so ``-1`` is the last element, and ``-2`` the one before it. If an index doesn't exist the range is clamped to the next possible index.

If a list has 5 elements the indices go from 1 to 5, so a range of ``2..16`` will only go from element 2 to element 5.

If the end is negative the range always goes up, so ``2..-2`` will go from element 2 to 4, and ``2..-16`` won't go anywhere because there is no way to go from the second element to one that doesn't exist, while going up.
If the start is negative the range always goes down, so ``-2..1`` will go from element 4 to 1, and ``-16..2`` won't go anywhere because there is no way to go from an element that doesn't exist to the second element, while going down.

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
^^^^^^^^^^^^^^^^^^^^^^^^

The ``~`` (tilde) character at the beginning of a parameter, followed by a username, is expanded into the home directory of the specified user. A lone ``~``, or a ``~`` followed by a slash, is expanded into the home directory of the process owner::

  ls ~/Music # lists my music directory

  echo ~root # prints root's home directory, probably "/root"


.. _combine:

Combining different expansions
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

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

Variables are a way to save data and pass it around. They can be used just by the shell, or they can be ":ref:`exported <variables-export>`", so that a copy of the variable is available to any external command the shell starts. An exported variable is referred to as an "environment variable".

To set a variable value, use the :ref:`set <cmd-set>` command. A variable name can not be empty and can contain only letters, digits, and underscores. It may begin and end with any of those characters.

Example:

To set the variable ``smurf_color`` to the value ``blue``, use the command ``set smurf_color blue``.

After a variable has been set, you can use the value of a variable in the shell through :ref:`variable expansion <expand-variable>`.

Example::

    set smurf_color blue
    echo Smurfs are usually $smurf_color
    set pants_color red
    echo Papa smurf, who is $smurf_color, wears $pants_color pants

So you set a variable with ``set``, and use it with a ``$`` and the name.

.. _variables-scope:

Variable scope
^^^^^^^^^^^^^^

There are four kinds of variables in fish: universal, global, function and local variables.

- Universal variables are shared between all fish sessions a user is running on one computer.
- Global variables are specific to the current fish session, and will never be erased unless explicitly requested by using ``set -e``.
- Function variables are specific to the currently executing function. They are erased ("go out of scope") when the current function ends.
- Local variables are specific to the current block of commands, and automatically erased when a specific block goes out of scope. A block of commands is a series of commands that begins with one of the commands ``for``, ``while`` , ``if``, ``function``, ``begin`` or ``switch``, and ends with the command ``end``. Outside of a block, this is the same as the function scope.

Variables can be explicitly set to be universal with the ``-U`` or ``--universal`` switch, global with ``-g`` or ``--global``, function-scoped with ``-f`` or ``--function`` and local to the current block with ``-l`` or ``--local``.  The scoping rules when creating or updating a variable are:

- When a scope is explicitly given, it will be used. If a variable of the same name exists in a different scope, that variable will not be changed.

- When no scope is given, but a variable of that name exists, the variable of the smallest scope will be modified. The scope will not be changed.

- When no scope is given and no variable of that name exists, the variable is created in function scope if inside a function, or global scope if no function is executing.

There can be many variables with the same name, but different scopes. When you :ref:`use a variable <expand-variable>`, the smallest scoped variable of that name will be used. If a local variable exists, it will be used instead of the global or universal variable of the same name.


Example:

There are a few possible uses for different scopes.

Typically inside functions you should use local scope::

    function something
        set -l file /path/to/my/file
        if not test -e "$file"
            set file /path/to/my/otherfile
        end
    end

    # or

    function something
        if test -e /path/to/my/file
            set -f file /path/to/my/file
        else
            set -f file /path/to/my/otherfile
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

    # Set my language
    set -gx LANG de_DE.UTF-8

If you want to set some personal customization, universal variables are nice::

     # Typically you'd run this interactively, fish takes care of keeping it.
     set -U fish_color_autosuggestion 555

Here is an example of local vs function-scoped variables::

  function test-scopes
      begin
          # This is a nice local scope where all variables will die
          set -l pirate 'There be treasure in them thar hills'
          set -f captain Space, the final frontier
          # If no variable of that name was defined, it is function-local.
          set gnu "In the beginning there was nothing, which exploded"
      end

      echo $pirate
      # This will not output anything, since the pirate was local
      echo $captain
      # This will output the good Captain's speech since $captain had function-scope.
      echo $gnu
      # Will output Sir Terry's wisdom.
  end

When in doubt, use function-scoped variables. When you need to make a variable accessible everywhere, make it global. When you need to persistently store configuration, make it universal. When you want to use a variable only in a short block, make it local.

.. _variables-override:

Overriding variables for a single command
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

If you want to override a variable for a single command, you can use "var=val" statements before the command::

  # Call git status on another directory
  # (can also be done via `git -C somerepo status`)
  GIT_DIR=somerepo git status

Unlike other shells, fish will first set the variable and then perform other expansions on the line, so::

  set foo banana
  foo=gagaga echo $foo # prints gagaga, while in other shells it might print "banana"

Multiple elements can be given in a :ref:`brace expansion<expand-brace>`::

  # Call bash with a reasonable default path.
  PATH={/usr,}/{s,}bin bash

Or with a :ref:`glob <expand-wildcard>`::

  # Run vlc on all mp3 files in the current directory
  # If no file exists it will still be run with no arguments
  mp3s=*.mp3 vlc $mp3s

Unlike other shells, this does *not* inhibit any lookup (aliases or similar). Calling a command after setting a variable override will result in the exact same command being run.

This syntax is supported since fish 3.1.

.. _variables-universal:

More on universal variables
^^^^^^^^^^^^^^^^^^^^^^^^^^^

Universal variables are variables that are shared between all the user's fish sessions on the computer. Fish stores many of its configuration options as universal variables. This means that in order to change fish settings, all you have to do is change the variable value once, and it will be automatically updated for all sessions, and preserved across computer reboots and login/logout.

To see universal variables in action, start two fish sessions side by side, and issue the following command in one of them ``set fish_color_cwd blue``. Since ``fish_color_cwd`` is a universal variable, the color of the current working directory listing in the prompt will instantly change to blue on both terminals.

:ref:`Universal variables <variables-universal>` are stored in the file ``.config/fish/fish_variables``. Do not edit this file directly, as your edits may be overwritten. Edit the variables through fish scripts or by using fish interactively instead.

Do not append to universal variables in :ref:`config.fish <configuration>`, because these variables will then get longer with each new shell instance. Instead, simply set them once at the command line.


.. _variables-functions:

Variable scope for functions
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

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
^^^^^^^^^^^^^^^^^^^

Variables in fish can be "exported", so they will be inherited by any commands started by fish. In particular, this is necessary for variables used to configure external commands like $LESS or $GOPATH, but also for variables that contain general system settings like $PATH or $LANGUAGE. If an external command needs to know a variable, it needs to be exported.

This also applies to fish - when it starts up, it receives environment variables from its parent (usually the terminal). These typically include system configuration like :ref:`$PATH <PATH>` and :ref:`locale variables <variables-locale>`.

Variables can be explicitly set to be exported with the ``-x`` or ``--export`` switch, or not exported with the ``-u`` or ``--unexport`` switch.  The exporting rules when setting a variable are identical to the scoping rules for variables:

- If a variable is explicitly set to either be exported or not exported, that setting will be honored.

- If a variable is not explicitly set to be exported or not exported, but has been previously defined, the previous exporting rule for the variable is kept.

- Otherwise, by default, the variable will not be exported.

- If a variable has function or local scope and is exported, any function called receives a *copy* of it, so any changes it makes to the variable disappear once the function returns.

- Global variables are accessible to functions whether they are exported or not.

As a naming convention, exported variables are in uppercase and unexported variables are in lowercase.

For example::

    set -gx ANDROID_HOME ~/.android # /opt/android-sdk
    set -gx CDPATH . ~ (test -e ~/Videos; and echo ~/Videos)
    set -gx EDITOR emacs -nw
    set -gx GOPATH ~/dev/go
    set -gx GTK2_RC_FILES "$XDG_CONFIG_HOME/gtk-2.0/gtkrc"
    set -gx LESSHISTFILE "-"

Note: Exporting is not a :ref:`scope <variables-scope>`, but an additional state. It typically makes sense to make exported variables global as well, but local-exported variables can be useful if you need something more specific than :ref:`Overrides <variables-override>`. They are *copied* to functions so the function can't alter them outside, and still available to commands.

.. _variables-lists:

Lists
^^^^^

Fish can store a list (or an "array" if you wish) of multiple strings inside of a variable::

   > set mylist first second third
   > printf '%s\n' $mylist # prints each element on its own line
   first
   second
   third

To access one element of a list, use the index of the element inside of square brackets, like this::

   echo $PATH[3]

List indices start at 1 in fish, not 0 like in other languages. This is because it requires less subtracting of 1 and many common Unix tools like ``seq`` work better with it (``seq 5`` prints 1 to 5, not 0 to 5). An invalid index is silently ignored resulting in no value (not even an empty string, just no argument at all).

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
^^^^^^^^^^^^^^^^^

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

      # If -s or --second is given, we print the second argument,
      # not the first and third.
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
^^^^^^^^^^^^^^

Path variables are a special kind of variable used to support colon-delimited path lists including PATH, CDPATH, MANPATH, PYTHONPATH, etc. All variables that end in "PATH" (case-sensitive) become PATH variables.

PATH variables act as normal lists, except they are implicitly joined and split on colons.

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

Special variables
^^^^^^^^^^^^^^^^^

You can change the settings of fish by changing the values of certain variables.

.. _PATH:

- ``PATH``, a list of directories in which to search for commands

- ``CDPATH``, a list of directories in which the :ref:`cd <cmd-cd>` builtin looks for a new directory.

- The locale variables ``LANG``, ``LC_ALL``, ``LC_COLLATE``, ``LC_CTYPE``, ``LC_MESSAGES``, ``LC_MONETARY``, ``LC_NUMERIC`` and ``LC_TIME`` set the language option for the shell and subprograms. See the section :ref:`Locale variables <variables-locale>` for more information.

- A number of variable starting with the prefixes ``fish_color`` and ``fish_pager_color``. See :ref:`Variables for changing highlighting colors <variables-color>` for more information.

- ``fish_ambiguous_width`` controls the computed width of ambiguous-width characters. This should be set to 1 if your terminal renders these characters as single-width (typical), or 2 if double-width.

- ``fish_emoji_width`` controls whether fish assumes emoji render as 2 cells or 1 cell wide. This is necessary because the correct value changed from 1 to 2 in Unicode 9, and some terminals may not be aware. Set this if you see graphical glitching related to emoji (or other "special" characters). It should usually be auto-detected.

- ``fish_handle_reflow``, determines whether fish should try to repaint the commandline when the terminal resizes. In terminals that reflow text this should be disabled. Set it to 1 to enable, anything else to disable.

- ``fish_key_bindings``, the name of the function that sets up the keyboard shortcuts for the :ref:`command-line editor <editor>`.

- ``fish_escape_delay_ms`` sets how long fish waits for another key after seeing an escape, to distinguish pressing the escape key from the start of an escape sequence. The default is 30ms. Increasing it increases the latency but allows pressing escape instead of alt for alt+character bindings. For more information, see :ref:`the chapter in the bind documentation <cmd-bind-escape>`.

- ``fish_greeting``, the greeting message printed on startup. This is printed by a function of the same name that can be overridden for more complicated changes (see :ref:`funced <cmd-funced>`)

- ``fish_history``, the current history session name. If set, all subsequent commands within an
  interactive fish session will be logged to a separate file identified by the value of the
  variable. If unset, the default session name "fish" is used. If set to an
  empty string, history is not saved to disk (but is still available within the interactive
  session).

- ``fish_trace``, if set and not empty, will cause fish to print commands before they execute, similar to ``set -x`` in bash. The trace is printed to the path given by the :ref:`--debug-output <cmd-fish>` option to fish or the ``FISH_DEBUG_OUTPUT`` variable. It goes to stderr by default.

- ``FISH_DEBUG`` and ``FISH_DEBUG_OUTPUT`` control what debug output fish generates and where it puts it, analogous to the ``--debug`` and ``--debug-output`` options. These have to be set on startup, via e.g. ``FISH_DEBUG='reader*' FISH_DEBUG_OUTPUT=/tmp/fishlog fish``.

- ``fish_user_paths``, a list of directories that are prepended to ``PATH``. This can be a universal variable.

- ``umask``, the current file creation mask. The preferred way to change the umask variable is through the :ref:`umask <cmd-umask>` function. An attempt to set umask to an invalid value will always fail.

- ``BROWSER``, your preferred web browser. If this variable is set, fish will use the specified browser instead of the system default browser to display the fish documentation.

Fish also provides additional information through the values of certain environment variables. Most of these variables are read-only and their value can't be changed with ``set``.

- ``_``, the name of the currently running command (though this is deprecated, and the use of ``status current-command`` is preferred).

- ``argv``, a list of arguments to the shell or function. ``argv`` is only defined when inside a function call, or if fish was invoked with a list of arguments, like ``fish myscript.fish foo bar``. This variable can be changed.

- ``CMD_DURATION``, the runtime of the last command in milliseconds.

- ``COLUMNS`` and ``LINES``, the current size of the terminal in height and width. These values are only used by fish if the operating system does not report the size of the terminal. Both variables must be set in that case otherwise a default of 80x24 will be used. They are updated when the window size changes.

- ``fish_kill_signal``, the signal that terminated the last foreground job, or 0 if the job exited normally.

- ``fish_killring``, a list of entries in fish's :ref:`kill ring <killring>` of cut text.

- ``fish_pid``, the process ID (PID) of the shell.

- ``history``, a list containing the last commands that were entered.

- ``HOME``, the user's home directory. This variable can be changed.

- ``hostname``, the machine's hostname.

- ``IFS``, the internal field separator that is used for word splitting with the :ref:`read <cmd-read>` builtin. Setting this to the empty string will also disable line splitting in :ref:`command substitution <expand-command-substitution>`. This variable can be changed.

- ``last_pid``, the process ID (PID) of the last background process.

- ``PWD``, the current working directory.

- ``pipestatus``, a list of exit statuses of all processes that made up the last executed pipe. See :ref:`exit status <variables-status>`.

- ``SHLVL``, the level of nesting of shells. Fish increments this in interactive shells, otherwise it simply passes it along.

- ``status``, the :ref:`exit status <variables-status>` of the last foreground job to exit. If the job was terminated through a signal, the exit status will be 128 plus the signal number.

- ``status_generation``, the "generation" count of ``$status``. This will be incremented only when the previous command produced an explicit status. (For example, background jobs will not increment this).

- ``USER``, the current username. This variable can be changed.

- ``version``, the version of the currently running fish (also available as ``FISH_VERSION`` for backward compatibility).

As a convention, an uppercase name is usually used for exported variables, while lowercase variables are not exported. (``CMD_DURATION`` is an exception for historical reasons). This rule is not enforced by fish, but it is good coding practice to use casing to distinguish between exported and unexported variables.

Fish also uses some variables internally, their name usually starting with ``__fish``. These are internal and should not typically be modified directly.

.. _variables-status:

The status variable
^^^^^^^^^^^^^^^^^^^

Whenever a process exits, an exit status is returned to the program that started it (usually the shell). This exit status is an integer number, which tells the calling application how the execution of the command went. In general, a zero exit status means that the command executed without problem, but a non-zero exit status means there was some form of problem.

Fish stores the exit status of the last process in the last job to exit in the ``status`` variable.

If fish encounters a problem while executing a command, the status variable may also be set to a specific value:

- 0 is generally the exit status of commands if they successfully performed the requested operation.

- 1 is generally the exit status of commands if they failed to perform the requested operation.

- 121 is generally the exit status of commands if they were supplied with invalid arguments.

- 123 means that the command was not executed because the command name contained invalid characters.

- 124 means that the command was not executed because none of the wildcards in the command produced any matches.

- 125 means that while an executable with the specified name was located, the operating system could not actually execute the command.

- 126 means that while a file with the specified name was located, it was not executable.

- 127 means that no function, builtin or command with the given name could be located.

If a process exits through a signal, the exit status will be 128 plus the number of the signal.

The status can be negated with :ref:`not <cmd-not>` (or ``!``), which is useful in a :ref:`condition <syntax-conditional>`. This turns a status of 0 into 1 and any non-zero status into 0.

There is also ``$pipestatus``, which is a list of all ``status`` values of processes in a pipe. One difference is that :ref:`not <cmd-not>` applies to ``$status``, but not ``$pipestatus``, because it loses information.

For example::

  not cat file | grep -q fish
  echo status is: $status pipestatus is $pipestatus

Here ``$status`` reflects the status of ``grep``, which returns 0 if it found something, negated with ``not`` (so 1 if it found something, 0 otherwise). ``$pipestatus`` reflects the status of ``cat`` (which returns non-zero for example when it couldn't find the file) and ``grep``, without the negation.

So if both ``cat`` and ``grep`` succeeded, ``$status`` would be 1 because of the ``not``, and ``$pipestatus`` would be 0 and 0.

.. _variables-locale:

Locale variables
^^^^^^^^^^^^^^^^

The "locale" of a program is its set of language and regional settings. In UNIX, there are a few separate variables to control separate things - ``LC_CTYPE`` defines the text encoding while ``LC_TIME`` defines the time format.

The locale variables are: ``LANG``, ``LC_ALL``, ``LC_COLLATE``, ``LC_CTYPE``, ``LC_MESSAGES``,  ``LC_MONETARY``, ``LC_NUMERIC`` and ``LC_TIME``. These variables work as follows: ``LC_ALL`` forces all the aspects of the locale to the specified value. If ``LC_ALL`` is set, all other locale variables will be ignored (this is typically not recommended!). The other ``LC_`` variables set the specified aspect of the locale information. ``LANG`` is a fallback value, it will be used if none of the ``LC_`` variables are specified.

The most common way to set the locale to use a command like ``set -gx LANG en_GB.utf8``, which sets the current locale to be the English language, as used in Great Britain, using the UTF-8 character set. That way any program that requires one setting differently can easily override just that and doesn't have to resort to LC_ALL. For a list of available locales on your system, try ``locale -a``.

Because it needs to handle output that might include multibyte characters (like e.g. emojis), fish will try to set its own internal LC_CTYPE to one that is UTF8-capable even if given an effective LC_CTYPE of "C" (the default). This prevents issues with e.g. filenames given in autosuggestions even if the user started fish with LC_ALL=C. To turn this handling off, set ``fish_allow_singlebyte_locale`` to "1".

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

.. _configuration:

Configuration files
====================

When fish is started, it reads and runs its configuration files. Where these are depends on build configuration and environment variables.

The main file is ``~/.config/fish/config.fish`` (or more precisely ``$XDG_CONFIG_HOME/fish/config.fish``).

Configuration files are run in the following order:

- Configuration snippets (named ``*.fish``) in the directories:

  - ``$__fish_config_dir/conf.d`` (by default, ``~/.config/fish/conf.d/``)
  - ``$__fish_sysconf_dir/conf.d`` (by default, ``/etc/fish/conf.d/``)
  - Directories for others to ship configuration snippets for their software. Fish searches the directories in the ``XDG_DATA_DIRS`` environment variable for a ``fish/vendor_conf.d`` directory; if that is not defined, the default is ``/usr/share/fish/vendor_conf.d`` and ``/usr/local/share/fish/vendor_conf.d``, unless your distribution customized this.

  If there are multiple files with the same name in these directories, only the first will be executed.
  They are executed in order of their filename, sorted (like globs) in a natural order (i.e. "01" sorts before "2").

- System-wide configuration files, where administrators can include initialization for all users on the system - similar to ``/etc/profile`` for POSIX-style shells - in ``$__fish_sysconf_dir`` (usually ``/etc/fish/config.fish``).
- User configuration, usually in ``~/.config/fish/config.fish`` (controlled by the ``XDG_CONFIG_HOME`` environment variable, and accessible as ``$__fish_config_dir``).

``~/.config/fish/config.fish`` is sourced *after* the snippets. This is so you can copy snippets and override some of their behavior.

These files are all executed on the startup of every shell. If you want to run a command only on starting an interactive shell, use the exit status of the command ``status --is-interactive`` to determine if the shell is interactive. If you want to run a command only when using a login shell, use ``status --is-login`` instead. This will speed up the starting of non-interactive or non-login shells.

If you are developing another program, you may want to add configuration for all users of fish on a system. This is discouraged; if not carefully written, they may have side-effects or slow the startup of the shell. Additionally, users of other shells won't benefit from the fish-specific configuration. However, if they are required, you can install them to the "vendor" configuration directory. As this path may vary from system to system, ``pkg-config`` should be used to discover it: ``pkg-config --variable confdir fish``.

.. _featureflags:

Future feature flags
--------------------

Feature flags are how fish stages changes that might break scripts. Breaking changes are introduced as opt-in, in a few releases they become opt-out, and eventually the old behavior is removed.

You can see the current list of features via ``status features``::

    > status features
    stderr-nocaret          on  3.0 ^ no longer redirects stderr
    qmark-noglob            off 3.0 ? no longer globs
    regex-easyesc           off 3.1 string replace -r needs fewer \\'s
    ampersand-nobg-in-token off 3.4 & only backgrounds if followed by a separating character

Here is what they mean:

- ``stderr-nocaret`` was introduced in fish 3.0 (and made the default in 3.3). It makes ``^`` an ordinary character instead of denoting an stderr redirection, to make dealing with quoting and such easier. Use ``2>`` instead.
- ``qmark-noglob`` was also introduced in fish 3.0. It makes ``?`` an ordinary character instead of a single-character glob. Use a ``*`` instead (which will match multiple characters) or find other ways to match files like ``find``.
- ``regex-easyesy`` was introduced in 3.1. It makes it so the replacement expression in ``string replace -r`` does one fewer round of escaping. Before, to escape a backslash you would have to use ``string replace -ra '([ab])' '\\\\\\\\$1'``. After, just ``'\\\\$1'`` is enough. Check your ``string replace`` cals if you use this anywhere.
- ``ampersand-nobg-in-token`` was introduced in fish 3.4. It makes it so a ``&`` i no longer interpreted as the backgrounding operator in the middle of a token, so dealing with URLs becomes easier. Either put spaces or a semicolon after the ``&``. This is recommended formatting anyway, and ``fish_indent`` will have done it for you already.


These changes are introduced off by default. They can be enabled on a per session basis::

    > fish --features qmark-noglob,stderr-nocaret


or opted into globally for a user::


    > set -U fish_features stderr-nocaret qmark-noglob

Features will only be set on startup, so this variable will only take effect if it is universal or exported.

You can also use the version as a group, so ``3.0`` is equivalent to "stderr-nocaret" and "qmark-noglob". Instead of a version, the special group ``all`` enables all features.

Prefixing a feature with ``no-`` turns it off instead. E.g. to reenable the ``^`` redirection::

  set -Ua fish_features no-stderr-nocaret

Currently, the following features are enabled by default:

- stderr-nocaret - ``^`` no longer redirects stderr, use ``2>``. Enabled by default in fish 3.3.0.

.. _event:

Event handlers
--------------

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

Please note that event handlers only become active when a function is loaded, which means you need to otherwise :ref:`source <cmd-source>` or execute a function instead of relying on :ref:`autoloading <syntax-function-autoloading>`. One approach is to put it into your :ref:`configuration file <configuration>`.

For more information on how to define new event handlers, see the documentation for the :ref:`function <cmd-function>` command.


.. _debugging:

Debugging fish scripts
----------------------

Fish includes a built in debugging facility. The debugger allows you to stop execution of a script at an arbitrary point. When this happens you are presented with an interactive prompt. At this prompt you can execute any fish command (there are no debug commands as such). For example, you can check or change the value of any variables using :ref:`printf <cmd-printf>` and :ref:`set <cmd-set>`. As another example, you can run :ref:`status print-stack-trace <cmd-status>` to see how this breakpoint was reached. To resume normal execution of the script, simply type :ref:`exit <cmd-exit>` or :kbd:`Control`\ +\ :kbd:`D`.

To start a debug session simply run the builtin command :ref:`breakpoint <cmd-breakpoint>` at the point in a function or script where you wish to gain control. Also, the default action of the TRAP signal is to call this builtin. So a running script can be debugged by sending it the TRAP signal with the ``kill`` command. Once in the debugger, it is easy to insert new breakpoints by using the funced function to edit the definition of a function.
