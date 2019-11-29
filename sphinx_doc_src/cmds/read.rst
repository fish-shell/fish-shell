.. _cmd-read:

read - read line of input into variables
========================================

Synopsis
--------

::

    read [OPTIONS] [VARIABLE ...]

Description
-----------

``read`` reads from standard input and either writes the result back to standard output (for use in command substitution), or stores the result in one or more shell variables. By default, ``read`` reads a single line and splits it into variables on spaces or tabs. Alternatively, a null character or a maximum number of characters can be used to terminate the input, and other delimiters can be given. Unlike other shells, there is no default variable (such as ``REPLY``) for storing the result - instead, it is printed on standard output.

The following options are available:

- ``-c CMD`` or ``--command=CMD`` sets the initial string in the interactive mode command buffer to ``CMD``.

- ``-d DELIMITER`` or ``--delimiter=DELIMITER`` splits on DELIMITER. DELIMITER will be used as an entire string to split on, not a set of characters.

- ``-g`` or ``--global`` makes the variables global.

- ``-s`` or ``--silent`` masks characters written to the terminal, replacing them with asterisks. This is useful for reading things like passwords or other sensitive information.

- ``-l`` or ``--local`` makes the variables local.

- ``-n NCHARS`` or ``--nchars=NCHARS`` makes ``read`` return after reading NCHARS characters or the end of
  the line, whichever comes first.

- ``-p PROMPT_CMD`` or ``--prompt=PROMPT_CMD`` uses the output of the shell command ``PROMPT_CMD`` as the prompt for the interactive mode. The default prompt command is `set_color green; echo read; set_color normal; echo "> "`

- ``-P PROMPT_STR`` or ``--prompt-str=PROMPT_STR`` uses the string as the prompt for the interactive mode. It is equivalent to `echo PROMPT_STR` and is provided solely to avoid the need to frame the prompt as a command. All special characters in the string are automatically escaped before being passed to the `echo` command.

- ``-R RIGHT_PROMPT_CMD`` or ``--right-prompt=RIGHT_PROMPT_CMD`` uses the output of the shell command ``RIGHT_PROMPT_CMD`` as the right prompt for the interactive mode. There is no default right prompt command.

- ``-S`` or ``--shell`` enables syntax highlighting, tab completions and command termination suitable for entering shellscript code in the interactive mode. NOTE: Prior to fish 3.0, the short opt for ``--shell`` was ``-s``, but it has been changed for compatibility with bash's ``-s`` short opt for ``--silent``.

- ``-t`` -or ``--tokenize`` causes read to split the input into variables by the shell's tokenization rules. This means it will honor quotes and escaping. This option is of course incompatible with other options to control splitting like ``--delimiter`` and does not honor $IFS (like fish's tokenizer). It saves the tokens in the manner they'd be passed to commands on the commandline, so e.g. ``a\ b`` is stored as ``a b``. Note that currently it leaves command substitutions intact along with the parentheses.

- ``-u`` or ``--unexport`` prevents the variables from being exported to child processes (default behaviour).

- ``-U`` or ``--universal`` causes the specified shell variable to be made universal.

- ``-x`` or ``--export`` exports the variables to child processes.

- ``-a`` or ``--list`` stores the result as a list in a single variable. This option is also available as ``--array`` for backwards compatibility.

- ``-z`` or ``--null`` marks the end of the line with the NUL character, instead of newline. This also
  disables interactive mode.

- ``-L`` or ``--line`` reads each line into successive variables, and stops after each variable has been filled. This cannot be combined with the ``--delimiter`` option.

Without the ``--line`` option, ``read`` reads a single line of input from standard input, breaks it into tokens, and then assigns one token to each variable specified in ``VARIABLES``. If there are more tokens than variables, the complete remainder is assigned to the last variable.

If no option to determine how to split like ``--delimiter``, ``--line`` or ``--tokenize`` is given, the variable ``IFS`` is used as a list of characters to split on. Relying on the use of ``IFS`` is deprecated and this behaviour will be removed in future versions. The default value of ``IFS`` contains space, tab and newline characters. As a special case, if ``IFS`` is set to the empty string, each character of the input is considered a separate token.

With the ``--line`` option, ``read`` reads a line of input from standard input into each provided variable, stopping when each variable has been filled. The line is not tokenized.

If no variable names are provided, ``read`` enters a special case that simply provides redirection from standard input to standard output, useful for command substitution. For instance, the fish shell command below can be used to read data that should be provided via a command line argument from the console instead of hardcoding it in the command itself, allowing the command to both be reused as-is in various contexts with different input values and preventing possibly sensitive text from being included in the shell history:

``mysql -uuser -p(read)``

When running in this mode, ``read`` does not split the input in any way and text is redirected to standard output without any further processing or manipulation.

If ``-a`` or ``--array`` is provided, only one variable name is allowed and the tokens are stored as a list in this variable.

See the documentation for ``set`` for more details on the scoping rules for variables.

When ``read`` reaches the end-of-file (EOF) instead of the terminator, the exit status is set to 1.
Otherwise, it is set to 0.

In order to protect the shell from consuming too many system resources, ``read`` will only consume a
maximum of 100 MiB (104857600 bytes); if the terminator is not reached before this limit then VARIABLE
is set to empty and the exit status is set to 122. This limit can be altered with the
``fish_read_limit`` variable. If set to 0 (zero), the limit is removed.

Using another read history file
-------------------------------

The ``read`` command supported the ``-m`` and ``--mode-name`` flags in fish versions prior to 2.7.0 to specify an alternative read history file. Those flags are now deprecated and ignored. Instead, set the ``fish_history`` variable to specify a history session ID. That will affect both the ``read`` history file and the fish command history file. You can set it to an empty string to specify that no history should be read or written. This is useful for presentations where you do not want possibly private or sensitive history to be exposed to the audience but do want history relevant to the presentation to be available.

Example
-------

The following code stores the value 'hello' in the shell variable ``$foo``.



::

    echo hello|read foo

    # This is a neat way to handle command output by-line:
    printf '%s\n' line1 line2 line3 line4 | while read -l foo
                      echo "This is another line: $foo"
                  end

    # Delimiters given via "-d" are taken as one string
    echo a==b==c | read -d == -l a b c
    echo $a # a
    echo $b # b
    echo $c # c

    # --tokenize honors quotes and escaping like the shell's argument passing:
    echo 'a\ b' | read -t first second
    echo $first # outputs "a b", $second is empty

    echo 'a"foo bar"b (command echo wurst)*" "{a,b}' | read -lt -l a b c
    echo $a # outputs 'afoo bar' (without the quotes)
    echo $b # outputs '(command echo wurst)* {a,b}' (without the quotes)
    echo $c # nothing
