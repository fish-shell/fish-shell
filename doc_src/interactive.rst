.. _interactive:

Interactive use
===============

Fish prides itself on being really nice to use interactively. That's down to a few features we'll explain in the next few sections.

Fish is used by giving commands in the fish language, see :ref:`The Fish Language <language>` for information on that.

Help
----

Fish has an extensive help system. Use the :ref:`help <cmd-help>` command to obtain help on a specific subject or command. For instance, writing ``help syntax`` displays the :ref:`syntax section <syntax>` of this documentation.

Fish also has man pages for its commands, and translates the help pages to man pages. For example, ``man set`` will show the documentation for ``set`` as a man page.

Help on a specific builtin can also be obtained with the ``-h`` parameter. For instance, to obtain help on the :ref:`fg <cmd-fg>` builtin, either type ``fg -h`` or ``help fg``.

The main page can be viewed via ``help index`` (or just ``help``) or ``man fish-doc``. The tutorial can be viewed with ``help tutorial`` or ``man fish-tutorial``.

.. _autosuggestions:

Autosuggestions
---------------

fish suggests commands as you type, based on :ref:`command history <history-search>`, completions, and valid file paths. As you type commands, you will see a suggestion offered after the cursor, in a muted gray color (which can be changed with the ``fish_color_autosuggestion`` variable).

To accept the autosuggestion (replacing the command line contents), press :kbd:`→` or :kbd:`Control`\ +\ :kbd:`F`. To accept the first suggested word, press :kbd:`Alt`\ +\ :kbd:`→` or :kbd:`Alt`\ +\ :kbd:`F`. If the autosuggestion is not what you want, just ignore it: it won't execute unless you accept it.

Autosuggestions are a powerful way to quickly summon frequently entered commands, by typing the first few characters. They are also an efficient technique for navigating through directory hierarchies.

If you don't like autosuggestions, you can disable them by setting ``$fish_autosuggestion_enabled`` to 0::

  set -g fish_autosuggestion_enabled 0

.. _tab-completion:

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

.. _color:

Syntax highlighting
-------------------

Fish interprets the command line as it is typed and uses syntax highlighting to provide feedback. The most important feedback is the detection of potential errors. By default, errors are marked red.

Detected errors include:

- Non existing commands.
- Reading from or appending to a non existing file.
- Incorrect use of output redirects
- Mismatched parenthesis

To customize the syntax highlighting, you can set the environment variables listed in the :ref:`Variables for changing highlighting colors <variables-color>` section.

Fish also provides pre-made color themes you can pick with :ref:`fish_config <cmd-fish_config>`. Running just ``fish_config`` opens a browser interface, or you can use ``fish_config theme`` in the terminal.

For example, to disable nearly all coloring::

  fish_config theme choose none

Or, to see all themes, right in your terminal::

  fish_config theme show

.. _variables-color:

Syntax highlighting variables
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The colors used by fish for syntax highlighting can be configured by changing the values of a various variables. The value of these variables can be one of the colors accepted by the :ref:`set_color <cmd-set_color>` command. The modifier switches accepted by ``set_color`` like ``--bold``, ``--dim``, ``--italics``, ``--reverse`` and ``--underline`` are also accepted.


Example: to make errors highlighted and red, use::

    set fish_color_error red --bold


The following variables are available to change the highlighting colors in fish:

==========================================                 =====================================================================
Variable                                                   Meaning
==========================================                 =====================================================================
``fish_color_normal``                                      default color
``fish_color_command``                                     commands like echo
``fish_color_keyword``                                     keywords like if - this falls back on the command color if unset
``fish_color_quote``                                       quoted text like ``"abc"``
``fish_color_redirection``                                 IO redirections like >/dev/null
``fish_color_end``                                         process separators like ``;`` and ``&``
``fish_color_error``                                       syntax errors
``fish_color_param``                                       ordinary command parameters
``fish_color_option``                                      options starting with "-", up to the first "--" parameter
``fish_color_comment``                                     comments like '# important'
``fish_color_selection``                                   selected text in vi visual mode
``fish_color_operator``                                    parameter expansion operators like ``*`` and ``~``
``fish_color_escape``                                      character escapes like ``\n`` and ``\x70``
``fish_color_autosuggestion``                              autosuggestions (the proposed rest of a command)
``fish_color_cwd``                                         the current working directory in the default prompt
``fish_color_user``                                        the username in the default prompt
``fish_color_host``                                        the hostname in the default prompt
``fish_color_host_remote``                                 the hostname in the default prompt for remote sessions (like ssh)
``fish_color_cancel``                                      the '^C' indicator on a canceled command
``fish_color_search_match``                                history search matches and selected pager items (background only)
==========================================                 =====================================================================

If a variable isn't set, fish usually tries ``$fish_color_normal``, except for:

- ``$fish_color_keyword``, where it tries ``$fish_color_command`` first.
- ``$fish_color_option``, where it tries ``$fish_color_param`` first.

.. _variables-color-pager:

Pager color variables
^^^^^^^^^^^^^^^^^^^^^^^

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

When the secondary or selected variables aren't set, the normal variables are used, except for ``$fish_pager_color_selected_background``, where the background of ``$fish_color_search_match`` is tried first.

.. _abbreviations:

Abbreviations
-------------

To avoid needless typing, a frequently-run command like ``git checkout`` can be abbreviated to ``gco`` using the :ref:`abbr <cmd-abbr>` command.

::

  abbr -a gco git checkout

After entering ``gco`` and pressing :kbd:`Space` or :kbd:`Enter`, the full text ``git checkout`` will appear in the command line.

This is an alternative to aliases, and has the advantage that you see the actual command before using it, and the actual command will be stored in history.

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

When fish waits for input, it will display a prompt by evaluating the :ref:`fish_prompt <cmd-fish_prompt>` and :ref:`fish_right_prompt <cmd-fish_right_prompt>` functions. The output of the former is displayed on the left and the latter's output on the right side of the terminal. The output of :ref:`fish_mode_prompt <cmd-fish_mode_prompt>` will be prepended on the left, though the default function only does this when in :ref:`vi-mode <vi-mode>`.

.. _greeting:

Configurable greeting
---------------------

If a function named :ref:`fish_greeting <cmd-fish_greeting>` exists, it will be run when entering interactive mode. Otherwise, if an environment variable named :ref:`fish_greeting <cmd-fish_greeting>` exists, it will be printed.

.. _private-mode:

Private mode
-------------

If ``$fish_private_mode`` is set to a non-empty value, commands will not be written to the history file on disk.

You can also launch with ``fish --private`` (or ``fish -P`` for short). This both hides old history and prevents writing history to disk. This is useful to avoid leaking personal information (e.g. for screencasts) or when dealing with sensitive information.

You can query the variable ``fish_private_mode`` (``if test -n "$fish_private_mode" ...``) if you would like to respect the user's wish for privacy and alter the behavior of your own fish scripts.

.. _editor:

Command line editor
-------------------

The fish editor features copy and paste, a :ref:`searchable history <history-search>` and many editor functions that can be bound to special keyboard shortcuts.

Like bash and other shells, fish includes two sets of keyboard shortcuts (or key bindings): one inspired by the Emacs text editor, and one by the Vi text editor. The default editing mode is Emacs. You can switch to Vi mode by running ``fish_vi_key_bindings`` and switch back with ``fish_default_key_bindings``. You can also make your own key bindings by creating a function and setting the ``fish_key_bindings`` variable to its name. For example::


    function fish_hybrid_key_bindings --description \
    "Vi-style bindings that inherit emacs-style bindings in all modes"
        for mode in default insert visual
            fish_default_key_bindings -M $mode
        end
        fish_vi_key_bindings --no-erase
    end
    set -g fish_key_bindings fish_hybrid_key_bindings

While the key bindings included with fish include many of the shortcuts popular from the respective text editors, they are not a complete implementation. They include a shortcut to open the current command line in your preferred editor (:kbd:`Alt`\ +\ :kbd:`E` by default) if you need the full power of your editor.

.. _shared-binds:

Shared bindings
---------------

Some bindings are common across Emacs and Vi mode, because they aren't text editing bindings, or because what Vi/Vim does for a particular key doesn't make sense for a shell.

- :kbd:`Tab` :ref:`completes <tab-completion>` the current token. :kbd:`Shift`\ +\ :kbd:`Tab` completes the current token and starts the pager's search mode.

- :kbd:`←` (Left) and :kbd:`→` (Right) move the cursor left or right by one character. If the cursor is already at the end of the line, and an autosuggestion is available, :kbd:`→` accepts the autosuggestion.

- :kbd:`Enter` executes the current commandline or inserts a newline if it's not complete yet (e.g. a ``)`` or ``end`` is missing).

- :kbd:`Alt`\ +\ :kbd:`Enter` inserts a newline at the cursor position.

- :kbd:`Alt`\ +\ :kbd:`←` and :kbd:`Alt`\ +\ :kbd:`→` move the cursor one word left or right (to the next space or punctuation mark), or moves forward/backward in the directory history if the command line is empty. If the cursor is already at the end of the line, and an autosuggestion is available, :kbd:`Alt`\ +\ :kbd:`→` (or :kbd:`Alt`\ +\ :kbd:`F`) accepts the first word in the suggestion.

- :kbd:`Control`\ +\ :kbd:`←` and :kbd:`Control`\ +\ :kbd:`→` move the cursor one word left or right. These accept one word of the autosuggestion - the part they'd move over.

- :kbd:`Shift`\ +\ :kbd:`←` and :kbd:`Shift`\ +\ :kbd:`→` move the cursor one word left or right, without stopping on punctuation. These accept one big word of the autosuggestion.

- :kbd:`↑` (Up) and :kbd:`↓` (Down) (or :kbd:`Control`\ +\ :kbd:`P` and :kbd:`Control`\ +\ :kbd:`N` for emacs aficionados) search the command history for the previous/next command containing the string that was specified on the commandline before the search was started. If the commandline was empty when the search started, all commands match. See the :ref:`history <history-search>` section for more information on history searching.

- :kbd:`Alt`\ +\ :kbd:`↑` and :kbd:`Alt`\ +\ :kbd:`↓` search the command history for the previous/next token containing the token under the cursor before the search was started. If the commandline was not on a token when the search started, all tokens match. See the :ref:`history <history-search>` section for more information on history searching.

- :kbd:`Control`\ +\ :kbd:`C` cancels the entire line.

- :kbd:`Control`\ +\ :kbd:`D` delete one character to the right of the cursor. If the command line is empty, :kbd:`Control`\ +\ :kbd:`D` will exit fish.

- :kbd:`Control`\ +\ :kbd:`U` moves contents from the beginning of line to the cursor to the :ref:`killring <killring>`.

- :kbd:`Control`\ +\ :kbd:`L` clears and repaints the screen.

- :kbd:`Control`\ +\ :kbd:`R` searches the history if there is something in the commandline. This is mainly to ease the transition from other shells, where ctrl+r initiates the history search.

- :kbd:`Control`\ +\ :kbd:`W` moves the previous path component (everything up to the previous "/", ":" or "@") to the :ref:`killring`.

- :kbd:`Control`\ +\ :kbd:`X` copies the current buffer to the system's clipboard, :kbd:`Control`\ +\ :kbd:`V` inserts the clipboard contents.

- :kbd:`Alt`\ +\ :kbd:`D` moves the next word to the :ref:`killring`.

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

To enable emacs mode, use ``fish_default_key_bindings``. This is also the default.

- :kbd:`Home` or :kbd:`Control`\ +\ :kbd:`A` moves the cursor to the beginning of the line.

- :kbd:`End` or :kbd:`Control`\ +\ :kbd:`E` moves to the end of line. If the cursor is already at the end of the line, and an autosuggestion is available, :kbd:`End` or :kbd:`Control`\ +\ :kbd:`E` accepts the autosuggestion.

- :kbd:`Control`\ +\ :kbd:`B`, :kbd:`Control`\ +\ :kbd:`F` move the cursor one character left or right or accept the autosuggestion just like the :kbd:`←` (Left) and :kbd:`→` (Right) shared bindings (which are available as well).

- :kbd:`Control`\ +\ :kbd:`N`, :kbd:`Control`\ +\ :kbd:`P` move the cursor up/down or through history, like the up and down arrow shared bindings.

- :kbd:`Delete` or :kbd:`Backspace` removes one character forwards or backwards respectively.

- :kbd:`Control`\ +\ :kbd:`K` moves contents from the cursor to the end of line to the :ref:`killring`.

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

Vi mode allows for the use of Vi-like commands at the prompt. Initially, :ref:`insert mode <vi-mode-insert>` is active. :kbd:`Escape` enters :ref:`command mode <vi-mode-command>`. The commands available in command, insert and visual mode are described below. Vi mode shares :ref:`some bindings <shared-binds>` with :ref:`Emacs mode <emacs-mode>`.

To enable vi mode, use ``fish_vi_key_bindings``.

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
^^^^^^^^^^^^

Command mode is also known as normal mode.

- :kbd:`H` moves the cursor left.

- :kbd:`L` moves the cursor right.

- :kbd:`I` enters :ref:`insert mode <vi-mode-insert>` at the current cursor position.

- :kbd:`V` enters :ref:`visual mode <vi-mode-visual>` at the current cursor position.

- :kbd:`A` enters :ref:`insert mode <vi-mode-insert>` after the current cursor position.

- :kbd:`Shift`\ +\ :kbd:`A` enters :ref:`insert mode <vi-mode-insert>` at the end of the line.

- :kbd:`0` (zero) moves the cursor to beginning of line (remaining in command mode).

- :kbd:`D`\ +\ :kbd:`D` deletes the current line and moves it to the :ref:`killring`.

- :kbd:`Shift`\ +\ :kbd:`D` deletes text after the current cursor position and moves it to the :ref:`killring`.

- :kbd:`P` pastes text from the :ref:`killring`.

- :kbd:`U` search history backwards.

- :kbd:`[` and :kbd:`]` search the command history for the previous/next token containing the token under the cursor before the search was started. See the :ref:`history <history-search>` section for more information on history searching.

- :kbd:`Backspace` moves the cursor left.

.. _vi-mode-insert:

Insert mode
^^^^^^^^^^^

- :kbd:`Escape` enters :ref:`command mode <vi-mode-command>`.

- :kbd:`Backspace` removes one character to the left.

.. _vi-mode-visual:

Visual mode
^^^^^^^^^^^

- :kbd:`←` (Left) and :kbd:`→` (Right) extend the selection backward/forward by one character.

- :kbd:`B` and :kbd:`W` extend the selection backward/forward by one word.

- :kbd:`D` and :kbd:`X` move the selection to the :ref:`killring` and enter :ref:`command mode <vi-mode-command>`.

- :kbd:`Escape` and :kbd:`Control`\ +\ :kbd:`C` enter :ref:`command mode <vi-mode-command>`.

- :kbd:`c` and :kbd:`s` remove the selection and switch to insert mode

- :kbd:`d` and :kbd:`x` remove the selection and switch to normal mode

- :kbd:`X` removes the entire line and switches to normal mode

- :kbd:`y` copies the selection and switches to normal mode

- :kbd:`~` toggles the case (upper/lower) on the selection and switches to normal mode

- :kbd:`"*y` copies the selection to the clipboard and switches to normal mode

.. _custom-binds:

Custom bindings
---------------

In addition to the standard bindings listed here, you can also define your own with :ref:`bind <cmd-bind>`::

  # Just clear the commandline on control-c
  bind \cc 'commandline -r ""'

Put ``bind`` statements into :ref:`config.fish <configuration>` or a function called ``fish_user_key_bindings``.

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
Kill ring entries are stored in ``fish_killring`` variable.

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

History searches are case-insensitive unless the search string contains an uppercase character. You can stop a search to edit your search string by pressing :kbd:`Esc` or :kbd:`Page Down`.

Prefixing the commandline with a space will prevent the entire line from being stored in the history. It will still be available for recall until the next command is executed, but will not be stored on disk. This is to allow you to fix misspellings and such.

The command history is stored in the file ``~/.local/share/fish/fish_history`` (or
``$XDG_DATA_HOME/fish/fish_history`` if that variable is set) by default. However, you can set the
``fish_history`` environment variable to change the name of the history session (resulting in a
``<session>_history`` file); both before starting the shell and while the shell is running.

See the :ref:`history <cmd-history>` command for other manipulations.

Examples:

To search for previous entries containing the word 'make', type ``make`` in the console and press the up key.

If the commandline reads ``cd m``, place the cursor over the ``m`` character and press :kbd:`Alt`\ +\ :kbd:`↑` to search for previously typed words containing 'm'.

Navigating directories
----------------------

.. _directory-history:

Navigating directories is usually done with the :ref:`cd <cmd-cd>` command, but fish offers some advanced features as well.

The current working directory can be displayed with the :ref:`pwd <cmd-pwd>` command, or the ``$PWD`` :ref:`special variable <variables-special>`. Usually your prompt already does this.

Directory history
^^^^^^^^^^^^^^^^^

Fish automatically keeps a trail of the recent visited directories with :ref:`cd <cmd-cd>` by storing this history in the ``dirprev`` and ``dirnext`` variables.

Several commands are provided to interact with this directory history:

- :ref:`dirh <cmd-dirh>` prints the history
- :ref:`cdh <cmd-cdh>` displays a prompt to quickly navigate the history
- :ref:`prevd <cmd-prevd>` moves backward through the history. It is bound to :kbd:`Alt`\ +\ :kbd:`←`
- :ref:`nextd <cmd-nextd>` moves forward through the history. It is bound to :kbd:`Alt`\ +\ :kbd:`→`

.. _directory-stack:

Directory stack
^^^^^^^^^^^^^^^

Another set of commands, usually also available in other shells like bash, deal with the directory stack. Stack handling is not automatic and needs explicit calls of the following commands:

- :ref:`dirs <cmd-dirs>` prints the stack
- :ref:`pushd <cmd-pushd>` adds a directory on top of the stack and makes it the current working directory
- :ref:`popd <cmd-popd>` removes the directory on top of the stack and changes the current working directory
