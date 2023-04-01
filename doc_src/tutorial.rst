.. highlight:: ghoti-docs-samples

.. _tutorial:

Tutorial
========

Why ghoti?
---------

Fish is a fully-equipped command line shell (like bash or zsh) that is smart and user-friendly. Fish supports powerful features like syntax highlighting, autosuggestions, and tab completions that just work, with nothing to learn or configure.

If you want to make your command line more productive, more useful, and more fun, without learning a bunch of arcane syntax and configuration options, then ghoti might be just what you're looking for!


Getting started
---------------

Once installed, just type in ``ghoti`` into your current shell to try it out!

You will be greeted by the standard ghoti prompt,
which means you are all set up and can start using ghoti::

    > ghoti
    Welcome to ghoti, the friendly interactive shell
    Type help for instructions on how to use ghoti
    you@hostname ~>


This prompt that you see above is the ghoti default prompt: it shows your username, hostname, and working directory.
- to change this prompt see :ref:`how to change your prompt <prompt>`
- to switch to ghoti permanently see :ref:`Default Shell <default-shell>`.

From now on, we'll pretend your prompt is just a ``>`` to save space.


Learning ghoti
-------------

This tutorial assumes a basic understanding of command line shells and Unix commands, and that you have a working copy of ghoti.

If you have a strong understanding of other shells, and want to know what ghoti does differently, search for the magic phrase *unlike other shells*, which is used to call out important differences.

Or, if you want a quick overview over the differences to other shells like Bash, see :ref:`Fish For Bash Users <ghoti_for_bash_users>`.

For the full, detailed description of how to use ghoti interactively, see :ref:`Interactive Use <interactive>`.

For a comprehensive description of ghoti's scripting language, see :ref:`The Fish Language<language>`.

Running Commands
----------------

Fish runs commands like other shells: you type a command, followed by its arguments. Spaces are separators::

    > echo hello world
    hello world


This runs the command ``echo`` with the arguments ``hello`` and ``world``. In this case that's the same as one argument ``hello world``, but in many cases it's not. If you need to pass an argument that includes a space, you can :ref:`escape <escapes>` with a backslash, or :ref:`quote <quotes>` it using single or double quotes::

    > mkdir My\ Files
    # Makes a directory called "My Files", with a space in the name
    > cp ~/Some\ File 'My Files'
    # Copies a file called "Some File" in the home directory to "My Files"
    > ls "My Files"
    Some File


Getting Help
------------

Run ``help`` to open ghoti's help in a web browser, and ``man`` with the page (like ``ghoti-language``) to open it in a man page. You can also ask for help with a specific command, for example, ``help set`` to open in a web browser, or ``man set`` to see it in the terminal.

::

    > man set
    set - handle shell variables
      Synopsis...

To open this section, use ``help getting-help``.

Fish works by running commands, which are often also installed on your computer. Usually these commands also provide help in the man system, so you can get help for them there. Try ``man ls`` to get help on your computer's ``ls`` command.

Syntax Highlighting
-------------------

.. role:: red
.. role:: gray
.. role:: prompt
.. role:: command
.. role:: param
.. role:: param-valid-path

You'll quickly notice that ghoti performs syntax highlighting as you type. Invalid commands are colored red by default:

.. parsed-literal::
    :class: highlight

    :prompt:`>` :red:`/bin/mkd`

A command may be invalid because it does not exist, or refers to a file that you cannot execute. When the command becomes valid, it is shown in a different color::

    > /bin/mkdir


Valid file paths are underlined as you type them:

.. parsed-literal::
    :class: highlight

    :prompt:`>` :command:`cat` :param-valid-path:`~/somefi`


This tells you that there exists a file that starts with ``somefi``, which is useful feedback as you type.

These colors, and many more, can be changed by running ``ghoti_config``, or by modifying :ref:`color variables <variables-color>` directly.

For example, if you want to disable (almost) all coloring::

  ghoti_config theme choose none

This picks the "none" theme. To see all themes::

  ghoti_config theme show

Just running ``ghoti_config`` will open up a browser interface that allows you to pick from the available themes.

Wildcards
---------

Fish supports the familiar wildcard ``*``. To list all JPEG files::

    > ls *.jpg
    lena.jpg
    meena.jpg
    santa maria.jpg


You can include multiple wildcards::

    > ls l*.p*
    lena.png
    lesson.pdf


The recursive wildcard ``**`` searches directories recursively::

    > ls /var/**.log
    /var/log/system.log
    /var/run/sntp.log


If that directory traversal is taking a long time, you can :kbd:`Control`\ +\ :kbd:`C` out of it.

For more, see :ref:`Wildcards <expand-wildcard>`.

Pipes and Redirections
----------------------

You can pipe between commands with the usual vertical bar::

    > echo hello world | wc
          1       2      12

stdin and stdout can be redirected via the familiar ``<`` and ``>``. stderr is redirected with a ``2>``.

::

    > grep ghoti < /etc/shells > ~/output.txt 2> ~/errors.txt

To redirect stdout and stderr into one file, you can use ``&>``::

    > make &> make_output.txt

For more, see :ref:`Input and output redirections <redirects>` and :ref:`Pipes <pipes>`.

Autosuggestions
---------------

As you type ghoti will suggest commands to the right of the cursor, in gray. For example:

.. parsed-literal::
    :class: highlight

    :prompt:`>` :red:`/bin/h`:gray:`ostname`


It knows about paths and options:

.. parsed-literal::
    :class: highlight

    :prompt:`>` :command:`grep` :param:`--i`:gray:`gnore-case`


And history too. Type a command once, and you can re-summon it by just typing a few letters:

.. parsed-literal::
    :class: highlight

    :prompt:`>` :red:`r`:gray:`sync -avze ssh . myname@somelonghost.com:/some/long/path/doo/dee/doo/dee/doo`


To accept the autosuggestion, hit :kbd:`→` (right arrow) or :kbd:`Control`\ +\ :kbd:`F`. To accept a single word of the autosuggestion, :kbd:`Alt`\ +\ :kbd:`→` (right arrow). If the autosuggestion is not what you want, just ignore it.

If you don't like autosuggestions, you can disable them by setting ``$ghoti_autosuggestion_enabled`` to 0::

  set -g ghoti_autosuggestion_enabled 0

Tab Completions
---------------

A rich set of tab completions work "out of the box".

Press :kbd:`Tab` and ghoti will attempt to complete the command, argument, or path:

.. parsed-literal::
    :class: highlight

    :prompt:`>` :red:`/pri`:kbd:`Tab` => :command:`/private/`


If there's more than one possibility, it will list them:

.. parsed-literal::
    :class: highlight

    :prompt:`>` :red:`~/stuff/s`:kbd:`Tab`
    ~/stuff/script.sh  :gray:`(command)`  ~/stuff/sources/  :gray:`(directory)`


Hit tab again to cycle through the possibilities. The part in parentheses there (that "command" and "directory") is the completion description. It's just a short hint to explain what kind of argument it is.

ghoti can also complete many commands, like git branches:

.. parsed-literal::
    :class: highlight

    :prompt:`>` :command:`git` :param:`merge pr`:kbd:`Tab` => :command:`git` :param:`merge prompt_designer`
    :prompt:`>` :command:`git` :param:`checkout b`:kbd:`Tab`
    builtin_list_io_merge :gray:`(Branch)`  builtin_set_color :gray:`(Branch)` busted_events :gray:`(Tag)`

Try hitting tab and see what ghoti can do!

Variables
---------

Like other shells, a dollar sign followed by a variable name is replaced with the value of that variable::

    > echo My home directory is $HOME
    My home directory is /home/tutorial

This is known as variable substitution, and it also happens in double quotes, but not single quotes::

    > echo "My current directory is $PWD"
    My current directory is /home/tutorial
    > echo 'My current directory is $PWD'
    My current directory is $PWD

Unlike other shells, ghoti has an ordinary command to set variables: ``set``, which takes a variable name, and then its value.

::

    > set name 'Mister Noodle'
    > echo $name
    Mister Noodle


(Notice the quotes: without them, ``Mister`` and ``Noodle`` would have been separate arguments, and ``$name`` would have been made into a list of two elements.)

Unlike other shells, variables are not further split after substitution::

    > mkdir $name
    > ls
    Mister Noodle

In bash, this would have created two directories "Mister" and "Noodle". In ghoti, it created only one: the variable had the value "Mister Noodle", so that is the argument that was passed to ``mkdir``, spaces and all.

You can erase (or "delete") a variable with ``-e`` or ``--erase``

::

    > set -e MyVariable
    > env | grep MyVariable
    (no output)

For more, see :ref:`Variable expansion <expand-variable>`.

.. _tut-exports:

Exports (Shell Variables)
-------------------------

Sometimes you need to have a variable available to an external command, often as a setting. For example many programs like ``git`` or ``man`` read the ``$PAGER`` variable to figure out your preferred pager (the program that lets you scroll text). Other variables used like this include ``$BROWSER``, ``$LANG`` (to configure your language) and ``$PATH``. You'll note these are written in ALLCAPS, but that's just a convention.

To give a variable to an external command, it needs to be "exported". This is done with a flag to ``set``, either ``--export`` or just ``-x``.

::

    > set -x MyVariable SomeValue
    > env | grep MyVariable
    MyVariable=SomeValue

It can also be unexported with ``--unexport`` or ``-u``.

This works the other way around as well! If ghoti is started by something else, it inherits that parents exported variables. So if your terminal emulator starts ghoti, and it exports ``$LANG`` set to ``en_US.UTF-8``, ghoti will receive that setting. And whatever started your terminal emulator also gave *it* some variables that it will then pass on unless it specifically decides not to. This is how ghoti usually receives the values for things like ``$LANG``, ``$PATH`` and ``$TERM``, without you having to specify them again.

Exported variables can be local or global or universal - "exported" is not a :ref:`scope <variables-scope>`! Usually you'd make them global via ``set -gx MyVariable SomeValue``.

For more, see :ref:`Exporting variables <variables-export>`.

.. _tut-lists:

Lists
-----

The ``set`` command above used quotes to ensure that ``Mister Noodle`` was one argument. If it had been two arguments, then ``name`` would have been a list of length 2.  In fact, all variables in ghoti are really lists, that can contain any number of values, or none at all.

Some variables, like ``$PWD``, only have one value. By convention, we talk about that variable's value, but we really mean its first (and only) value.

Other variables, like ``$PATH``, really do have multiple values. During variable expansion, the variable expands to become multiple arguments::

    > echo $PATH
    /usr/bin /bin /usr/sbin /sbin /usr/local/bin

Variables whose name ends in "PATH" are automatically split on colons to become lists. They are joined using colons when exported to subcommands. This is for compatibility with other tools, which expect $PATH to use colons. You can also explicitly add this quirk to a variable with ``set --path``, or remove it with ``set --unpath``.

Lists cannot contain other lists: there is no recursion.  A variable is a list of strings, full stop.

Get the length of a list with ``count``::

    > count $PATH
    5

You can append (or prepend) to a list by setting the list to itself, with some additional arguments. Here we append /usr/local/bin to $PATH::

    > set PATH $PATH /usr/local/bin

You can access individual elements with square brackets. Indexing starts at 1 from the beginning, and -1 from the end::

    > echo $PATH
    /usr/bin /bin /usr/sbin /sbin /usr/local/bin
    > echo $PATH[1]
    /usr/bin
    > echo $PATH[-1]
    /usr/local/bin

You can also access ranges of elements, known as "slices":

::

    > echo $PATH[1..2]
    /usr/bin /bin
    > echo $PATH[-1..2]
    /usr/local/bin /sbin /usr/sbin /bin

You can iterate over a list (or a slice) with a for loop::

    for val in $PATH
      echo "entry: $val"
    end
    # Will print:
    # entry: /usr/bin/
    # entry: /bin
    # entry: /usr/sbin
    # entry: /sbin
    # entry: /usr/local/bin

Lists adjacent to other lists or strings are expanded as :ref:`cartesian products <cartesian-product>` unless quoted (see :ref:`Variable expansion <expand-variable>`)::

    > set a 1 2 3
    > set 1 a b c
    > echo $a$1
    1a 2a 3a 1b 2b 3b 1c 2c 3c
    > echo $a" banana"
    1 banana 2 banana 3 banana
    > echo "$a banana"
    1 2 3 banana

This is similar to :ref:`Brace expansion <expand-brace>`.

For more, see :ref:`Lists <variables-lists>`.


Command Substitutions
---------------------

Command substitutions use the output of one command as an argument to another. Unlike other shells, ghoti does not use backticks `` for command substitutions. Instead, it uses parentheses with or without a dollar::

    > echo In (pwd), running $(uname)
    In /home/tutorial, running FreeBSD

A common idiom is to capture the output of a command in a variable::

    > set os (uname)
    > echo $os
    Linux

Command substitutions without a dollar are not expanded within quotes, so the version with a dollar is simpler::

    > touch "testing_$(date +%s).txt"
    > ls *.txt
    testing_1360099791.txt

Unlike other shells, ghoti does not split command substitutions on any whitespace (like spaces or tabs), only newlines. Usually this is a big help because unix commands operate on a line-by-line basis. Sometimes it can be an issue with commands like ``pkg-config`` that print what is meant to be multiple arguments on a single line. To split it on spaces too, use ``string split``.

::

    > printf '%s\n' (pkg-config --libs gio-2.0)
    -lgio-2.0 -lgobject-2.0 -lglib-2.0
    > printf '%s\n' (pkg-config --libs gio-2.0 | string split -n " ")
    -lgio-2.0
    -lgobject-2.0
    -lglib-2.0

If you need a command substitutions output as one argument, without any splits, use quoted command substitution::

    > echo "first line
    second line" > myfile
    > set myfile "$(cat myfile)"
    > printf '|%s|' $myfile
    |first line
    second line|

For more, see :ref:`Command substitution <expand-command-substitution>`.

.. _tut-semicolon:

Separating Commands (Semicolon)
-------------------------------

Like other shells, ghoti allows multiple commands either on separate lines or the same line.

To write them on the same line, use the semicolon (";"). That means the following two examples are equivalent::

    echo ghoti; echo chips
    
    # or
    echo ghoti
    echo chips

This is useful interactively to enter multiple commands. In a script it's easier to read if the commands are on separate lines.

Exit Status
-----------

When a command exits, it returns a status code as a non-negative integer (that's a whole number >= 0).

Unlike other shells, ghoti stores the exit status of the last command in ``$status`` instead of ``$?``.

::

    > false
    > echo $status
    1

This indicates how the command fared - 0 usually means success, while the others signify kinds of failure. For instance ghoti's ``set --query`` returns the number of variables it queried that weren't set - ``set --query PATH`` usually returns 0, ``set --query arglbargl boogagoogoo`` usually returns 2.

There is also a ``$pipestatus`` list variable for the exit statuses [#]_ of processes in a pipe.

For more, see :ref:`The status variable <variables-status>`.

.. [#] or "stati" if you prefer, or "statūs" if you've time-travelled from ancient Rome or work as a latin teacher

.. _tut-combiners:

Combiners (And, Or, Not)
------------------------

ghoti supports the familiar ``&&`` and ``||`` to combine commands, and ``!`` to negate them::

    > ./configure && make && sudo make install

Here, ``make`` is only executed if ``./configure`` succeeds (returns 0), and ``sudo make install`` is only executed if both ``./configure`` and ``make`` succeed.

ghoti also supports :doc:`and <cmds/and>`, :doc:`or <cmds/or>`, and :doc:`not <cmds/not>`. The first two are job modifiers and have lower precedence. Example usage::

    > cp file1 file1_bak && cp file2 file2_bak; and echo "Backup successful"; or echo "Backup failed"
    Backup failed


As mentioned in :ref:`the section on the semicolon <tut-semicolon>`, this can also be written in multiple lines, like so::

    cp file1 file1_bak && cp file2 file2_bak
    and echo "Backup successful"
    or echo "Backup failed"

.. _tut-conditionals:

Conditionals (If, Else, Switch)
-------------------------------

Use :doc:`if <cmds/if>` and :doc:`else <cmds/else>` to conditionally execute code, based on the exit status of a command.

::

    if grep ghoti /etc/shells
        echo Found ghoti
    else if grep bash /etc/shells
        echo Found bash
    else
        echo Got nothing
    end

To compare strings or numbers or check file properties (whether a file exists or is writeable and such), use :doc:`test <cmds/test>`, like

::

    if test "$ghoti" = "flounder"
        echo FLOUNDER
    end
    
    # or
    
    if test "$number" -gt 5
        echo $number is greater than five
    else
        echo $number is five or less
    end

    # or

    # This test is true if the path /etc/hosts exists
    # - it could be a file or directory or symlink (or possibly something else).
    if test -e /etc/hosts
        echo We most likely have a hosts file
    else
        echo We do not have a hosts file
    end

:ref:`Combiners <tut-combiners>` can also be used to make more complex conditions, like

::

    if command -sq ghoti; and grep ghoti /etc/shells
        echo ghoti is installed and configured
    end

For even more complex conditions, use :doc:`begin <cmds/begin>` and :doc:`end <cmds/end>` to group parts of them.

There is also a :doc:`switch <cmds/switch>` command::

    switch (uname)
    case Linux
        echo Hi Tux!
    case Darwin
        echo Hi Hexley!
    case FreeBSD NetBSD DragonFly
        echo Hi Beastie!
    case '*'
        echo Hi, stranger!
    end

As you see, :doc:`case <cmds/case>` does not fall through, and can accept multiple arguments or (quoted) wildcards.

For more, see :ref:`Conditions <syntax-conditional>`.

Functions
---------

A ghoti function is a list of commands, which may optionally take arguments. Unlike other shells, arguments are not passed in "numbered variables" like ``$1``, but instead in a single list ``$argv``. To create a function, use the :doc:`function <cmds/function>` builtin::

    function say_hello
        echo Hello $argv
    end
    say_hello
    # prints: Hello
    say_hello everybody!
    # prints: Hello everybody!

Unlike other shells, ghoti does not have aliases or special prompt syntax. Functions take their place. [#]_

You can list the names of all functions with the :doc:`functions <cmds/functions>` builtin (note the plural!). ghoti starts out with a number of functions::

    > functions
    N_, abbr, alias, bg, cd, cdh, contains_seq, dirh, dirs, disown, down-or-search, edit_command_buffer, export, fg, ghoti_add_path, ghoti_breakpoint_prompt, ghoti_clipboard_copy, ghoti_clipboard_paste, ghoti_config, ghoti_default_key_bindings, ghoti_default_mode_prompt, ghoti_git_prompt, ghoti_hg_prompt, ghoti_hybrid_key_bindings, ghoti_indent, ghoti_is_root_user, ghoti_job_summary, ghoti_key_reader, ghoti_md5, ghoti_mode_prompt, ghoti_npm_helper, ghoti_opt, ghoti_print_git_action, ghoti_print_hg_root, ghoti_prompt, ghoti_sigtrap_handler, ghoti_svn_prompt, ghoti_title, ghoti_update_completions, ghoti_vcs_prompt, ghoti_vi_cursor, ghoti_vi_key_bindings, funced, funcsave, grep, help, history, hostname, isatty, kill, la, ll, ls, man, nextd, open, popd, prevd, prompt_hostname, prompt_pwd, psub, pushd, realpath, seq, setenv, suspend, trap, type, umask, up-or-search, vared, wait

You can see the source for any function by passing its name to ``functions``::

    > functions ls
    function ls --description 'List contents of directory'
        command ls -G $argv
    end

For more, see :ref:`Functions <syntax-function>`.

.. [#] There is a function called :doc:`alias <cmds/alias>`, but it's just a shortcut to make functions. ghoti also provides :ref:`abbreviations <abbreviations>`, through the :ref:`abbr <cmd-abbr>` command.

Loops
-----

While loops::

    while true
        echo "Loop forever"
    end
    # Prints:
    # Loop forever
    # Loop forever
    # Loop forever
    # yes, this really will loop forever. Unless you abort it with ctrl-c.

For loops can be used to iterate over a list. For example, a list of files::

    for file in *.txt
        cp $file $file.bak
    end

Iterating over a list of numbers can be done with ``seq``::

    for x in (seq 5)
        touch file_$x.txt
    end

For more, see :ref:`Loops and blocks <syntax-loops-and-blocks>`.

Prompt
------

.. role:: purple

Unlike other shells, there is no prompt variable like ``PS1``. To display your prompt, ghoti executes the :doc:`ghoti_prompt <cmds/ghoti_prompt>` function and uses its output as the prompt. And if it exists, ghoti also executes the :doc:`ghoti_right_prompt <cmds/ghoti_right_prompt>` function and uses its output as the right prompt.

You can define your own prompt from the command line:

.. parsed-literal::
    :class: highlight

    > function ghoti_prompt; echo "New Prompt % "; end
    New Prompt % _


Then, if you are happy with it, you can save it to disk by typing ``funcsave ghoti_prompt``. This saves the prompt in ``~/.config/ghoti/functions/ghoti_prompt.ghoti``. (Or, if you want, you can create that file manually from the start.)

Multiple lines are OK. Colors can be set via :doc:`set_color <cmds/set_color>`, passing it named ANSI colors, or hex RGB values::

    function ghoti_prompt
        set_color purple
        date "+%m/%d/%y"
        set_color F00
        echo (pwd) '>' (set_color normal)
    end


This prompt would look like:

.. parsed-literal::
    :class: highlight

    :purple:`02/06/13`
    :red:`/home/tutorial >` _


You can choose among some sample prompts by running ``ghoti_config`` for a web UI or ``ghoti_config prompt`` for a simpler version inside your terminal.

$PATH
-----

``$PATH`` is an environment variable containing the directories that ghoti searches for commands. Unlike other shells, $PATH is a :ref:`list <tut-lists>`, not a colon-delimited string.

Fish takes care to set ``$PATH`` to a default, but typically it is just inherited from ghoti's parent process and is set to a value that makes sense for the system - see :ref:`Exports <tut-exports>`.

To prepend /usr/local/bin and /usr/sbin to ``$PATH``, you can write::

    > set PATH /usr/local/bin /usr/sbin $PATH


To remove /usr/local/bin from ``$PATH``, you can write::

    > set PATH (string match -v /usr/local/bin $PATH)

For compatibility with other shells and external commands, $PATH is a :ref:`path variable<variables-path>`, and so will be joined with colons (not spaces) when you quote it::

    > echo "$PATH"
    /usr/local/sbin:/usr/local/bin:/usr/bin

and it will be exported like that, and when ghoti starts it splits the $PATH it receives into a list on colon.

You can do so directly in ``config.ghoti``, like you might do in other shells with ``.profile``. See :ref:`this example <path_example>`.

A faster way is to use the :doc:`ghoti_add_path <cmds/ghoti_add_path>` function, which adds given directories to the path if they aren't already included. It does this by modifying the ``$ghoti_user_paths`` :ref:`universal variable <tut-universal>`, which is automatically prepended to ``$PATH``. For example, to permanently add ``/usr/local/bin`` to your ``$PATH``, you could write::

    > ghoti_add_path /usr/local/bin


The advantage is that you don't have to go mucking around in files: just run this once at the command line, and it will affect the current session and all future instances too. You can also add this line to :ref:`config.ghoti <tut-config>`, as it only adds the component if necessary.

Or you can modify $ghoti_user_paths yourself, but you should be careful *not* to append to it unconditionally in config.ghoti, or it will grow longer and longer.

.. _tut-config:

Startup (Where's .bashrc?)
--------------------------

Fish starts by executing commands in ``~/.config/ghoti/config.ghoti``. You can create it if it does not exist.

It is possible to directly create functions and variables in ``config.ghoti`` file, using the commands shown above. For example:

.. _path_example:

::

    > cat ~/.config/ghoti/config.ghoti
    
    set -x PATH $PATH /sbin/
    
    function ll
        ls -lh $argv
    end


However, it is more common and efficient to use  autoloading functions and universal variables.

If you want to organize your configuration, ghoti also reads commands in .ghoti files in ``~/.config/ghoti/conf.d/``. See :ref:`Configuration Files <configuration>` for the details.

Autoloading Functions
---------------------

When ghoti encounters a command, it attempts to autoload a function for that command, by looking for a file with the name of that command in ``~/.config/ghoti/functions/``.

For example, if you wanted to have a function ``ll``, you would add a text file ``ll.ghoti`` to ``~/.config/ghoti/functions``::

    > cat ~/.config/ghoti/functions/ll.ghoti
    function ll
        ls -lh $argv
    end


This is the preferred way to define your prompt as well::

    > cat ~/.config/ghoti/functions/ghoti_prompt.ghoti
    function ghoti_prompt
        echo (pwd) "> "
    end


See the documentation for :doc:`funced <cmds/funced>` and :doc:`funcsave <cmds/funcsave>` for ways to create these files automatically, and :ref:`$ghoti_function_path <syntax-function-autoloading>` to control their location.

.. _tut-universal:

Universal Variables
-------------------

A universal variable is a variable whose value is shared across all instances of ghoti, now and in the future – even after a reboot. You can make a variable universal with ``set -U``::

    > set -U EDITOR vim


Now in another shell::

    > echo $EDITOR
    vim


Ready for more?
---------------

If you want to learn more about ghoti, there is :ref:`lots of detailed documentation <intro>`, the `official gitter channel <https://gitter.im/ghoti-shell/ghoti-shell>`__, an `official mailing list <https://lists.sourceforge.net/lists/listinfo/ghoti-users>`__, and the `github page <https://github.com/ghoti-shell/ghoti-shell/>`__.
