.. _interactive:

Interactive use
===============

Fish prides itself on being really nice to use interactively. That's down to a few features we'll explain in the next few sections.

Fish is used by giving commands in the fish language, see :ref:`The Fish Language <language>` for information on that.

Help
----

Fish has an extensive help system. Use the :doc:`help <cmds/help>` command to obtain help on a specific subject or command. For instance, writing ``help syntax`` displays the :ref:`syntax section <syntax>` of this documentation.

Fish also has man pages for its commands, and translates the help pages to man pages. For example, ``man set`` will show the documentation for ``set`` as a man page.

Help on a specific builtin can also be obtained with the ``-h`` parameter. For instance, to obtain help on the :doc:`fg <cmds/fg>` builtin, either type ``fg -h`` or ``help fg``.

The main page can be viewed via ``help index`` (or just ``help``) or ``man fish-doc``. The tutorial can be viewed with ``help tutorial`` or ``man fish-tutorial``.

.. _autosuggestions:

Autosuggestions
---------------

fish suggests commands as you type, based on :ref:`command history <history-search>`, completions, and valid file paths. As you type commands, you will see a suggestion offered after the cursor, in a muted gray color (which can be changed with the ``fish_color_autosuggestion`` variable).

To accept the autosuggestion (replacing the command line contents), press :kbd:`right` (``→``) or :kbd:`ctrl-f`. To accept the first suggested word, press :kbd:`alt-right` (``→``) or :kbd:`alt-f`. If the autosuggestion is not what you want, ignore it: it won't execute unless you accept it.

Autosuggestions are a powerful way to quickly summon frequently entered commands, by typing the first few characters. They are also an efficient technique for navigating through directory hierarchies.

If you don't like autosuggestions, you can disable them by setting ``$fish_autosuggestion_enabled`` to 0::

  set -g fish_autosuggestion_enabled 0

.. _tab-completion:

Tab Completion
--------------

Tab completion is a time saving feature of any modern shell. When you type :kbd:`tab`, fish tries to guess the rest of the word under the cursor. If it finds exactly one possibility, it inserts it. If it finds more, it inserts the longest unambiguous part and then opens a menu (the "pager") that you can navigate to find what you're looking for.

The pager can be navigated with the arrow keys, :kbd:`pageup` / :kbd:`pagedown`, :kbd:`tab` or :kbd:`shift-tab`. Pressing :kbd:`ctrl-s` (the ``pager-toggle-search`` binding - :kbd:`/` in vi mode) opens up a search menu that you can use to filter the list.

Fish provides some general purpose completions, like for commands, variable names, usernames or files.

It also provides a large number of program specific scripted completions. Most of these completions are simple options like the ``-l`` option for ``ls``, but a lot are more advanced. For example:

- ``man`` and ``whatis`` show the installed manual pages as completions.

- ``make`` uses targets in the Makefile in the current directory as completions.

- ``mount`` uses mount points specified in fstab as completions.

- ``apt``, ``rpm`` and ``yum`` show installed or installable packages

You can also write your own completions or install some you got from someone else. For that, see :ref:`Writing your own completions <completion-own>`.

Completion scripts are loaded on demand, like :ref:`functions are <syntax-function-autoloading>`. The difference is the ``$fish_complete_path`` :ref:`list <variables-lists>` is used instead of ``$fish_function_path``. Typically you can drop new completions in ~/.config/fish/completions/name-of-command.fish and fish will find them automatically.

.. _color:

Syntax highlighting
-------------------

Fish interprets the command line as it is typed and uses syntax highlighting to provide feedback. The most important feedback is the detection of potential errors. By default, errors are marked red.

Detected errors include:

- Non-existing commands.
- Reading from or appending to a non-existing file.
- Incorrect use of output redirects
- Mismatched parenthesis

To customize the syntax highlighting, you can set the environment variables listed in the :ref:`Variables for changing highlighting colors <variables-color>` section.

Fish also provides pre-made color themes you can pick with :doc:`fish_config <cmds/fish_config>`. Running just ``fish_config`` opens a browser interface, or you can use ``fish_config theme`` in the terminal.

For example, to disable nearly all coloring::

  fish_config theme choose None

Or, to see all themes, right in your terminal::

  fish_config theme show

.. _variables-color:

Syntax highlighting variables
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The colors used by fish for syntax highlighting can be configured by changing the values of various variables. The value of these variables can be one of the colors accepted by the :doc:`set_color <cmds/set_color>` command. The modifier switches accepted by ``set_color`` like ``--bold``, ``--dim``, ``--italics``, ``--reverse`` and ``--underline`` are also accepted.


Example: to make errors highlighted and red, use::

    set fish_color_error red --bold


The following variables are available to change the highlighting colors in fish:

==========================================        =====================================================================
Variable                                          Meaning
==========================================        =====================================================================
.. envvar:: fish_color_normal                     default color
.. envvar:: fish_color_command                    commands like echo
.. envvar:: fish_color_keyword                    keywords like if - this falls back on the command color if unset
.. envvar:: fish_color_quote                      quoted text like ``"abc"``
.. envvar:: fish_color_redirection                IO redirections like >/dev/null
.. envvar:: fish_color_end                        process separators like ``;`` and ``&``
.. envvar:: fish_color_error                      syntax errors
.. envvar:: fish_color_param                      ordinary command parameters
.. envvar:: fish_color_valid_path                 parameters that are filenames (if the file exists)
.. envvar:: fish_color_option                     options starting with "-", up to the first "--" parameter
.. envvar:: fish_color_comment                    comments like '# important'
.. envvar:: fish_color_selection                  selected text in vi visual mode
.. envvar:: fish_color_operator                   parameter expansion operators like ``*`` and ``~``
.. envvar:: fish_color_escape                     character escapes like ``\n`` and ``\x70``
.. envvar:: fish_color_autosuggestion             autosuggestions (the proposed rest of a command)
.. envvar:: fish_color_cwd                        the current working directory in the default prompt
.. envvar:: fish_color_cwd_root                   the current working directory in the default prompt for the root user
.. envvar:: fish_color_user                       the username in the default prompt
.. envvar:: fish_color_host                       the hostname in the default prompt
.. envvar:: fish_color_host_remote                the hostname in the default prompt for remote sessions (like ssh)
.. envvar:: fish_color_status                     the last command's nonzero exit code in the default prompt
.. envvar:: fish_color_cancel                     the '^C' indicator on a canceled command
.. envvar:: fish_color_search_match               history search matches and selected pager items (background only)
.. envvar:: fish_color_history_current            the current position in the history for commands like ``dirh`` and ``cdh``

==========================================        =====================================================================

If a variable isn't set or is empty, fish usually tries ``$fish_color_normal``, except for:

- ``$fish_color_keyword``, where it tries ``$fish_color_command`` first.
- ``$fish_color_option``, where it tries ``$fish_color_param`` first.
- For ``$fish_color_valid_path``, if that doesn't have a color, but only modifiers, it adds those to the color that would otherwise be used,
  like ``$fish_color_param``. But if valid paths have a color, it uses that and adds in modifiers from the other color.

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

===================================================        ===========================================================
Variable                                                   Meaning
===================================================        ===========================================================
.. envvar:: fish_pager_color_progress                      the progress bar at the bottom left corner
.. envvar:: fish_pager_color_background                    the background color of a line
.. envvar:: fish_pager_color_prefix                        the prefix string, i.e. the string that is to be completed
.. envvar:: fish_pager_color_completion                    the completion itself, i.e. the proposed rest of the string
.. envvar:: fish_pager_color_description                   the completion description
.. envvar:: fish_pager_color_selected_background           background of the selected completion
.. envvar:: fish_pager_color_selected_prefix               prefix of the selected completion
.. envvar:: fish_pager_color_selected_completion           suffix of the selected completion
.. envvar:: fish_pager_color_selected_description          description of the selected completion
.. envvar:: fish_pager_color_secondary_background          background of every second unselected completion
.. envvar:: fish_pager_color_secondary_prefix              prefix of every second unselected completion
.. envvar:: fish_pager_color_secondary_completion          suffix of every second unselected completion
.. envvar:: fish_pager_color_secondary_description         description of every second unselected completion
===================================================        ===========================================================

When the secondary or selected variables aren't set or are empty, the normal variables are used, except for ``$fish_pager_color_selected_background``, where the background of ``$fish_color_search_match`` is tried first.

.. _abbreviations:

Abbreviations
-------------

To avoid needless typing, a frequently-run command like ``git checkout`` can be abbreviated to ``gco`` using the :doc:`abbr <cmds/abbr>` command.

::

  abbr -a gco git checkout

After entering ``gco`` and pressing :kbd:`space` or :kbd:`enter`, a ``gco`` in command position will turn into ``git checkout`` in the command line. If you want to use a literal ``gco`` sometimes, use :kbd:`ctrl-space` [#]_.

Abbreviations are a lot more powerful than just replacing literal strings. For example you can make going up a number of directories easier with this::

  function multicd
      echo cd (string repeat -n (math (string length -- $argv[1]) - 1) ../)
  end
  abbr --add dotdot --regex '^\.\.+$' --function multicd

Now, ``..`` transforms to ``cd ../``, while ``...`` turns into ``cd ../../`` and ``....`` expands to ``cd ../../../``.

The advantage over aliases is that you can see the actual command before using it, add to it or change it, and the actual command will be stored in history.

.. [#] Any binding that executes the ``expand-abbr`` or ``execute`` :doc:`bind function <cmds/bind>` will expand abbreviations. By default :kbd:`ctrl-space` is bound to just inserting a space.

.. _prompt:

Programmable prompt
-------------------

When it is fish's turn to ask for input (like after it started or the command ended), it will show a prompt. Often this looks something like::

    you@hostname ~>

This prompt is determined by running the :doc:`fish_prompt <cmds/fish_prompt>` and :doc:`fish_right_prompt <cmds/fish_right_prompt>` functions.

The output of the former is displayed on the left and the latter's output on the right side of the terminal.
For :ref:`vi mode <vi-mode>`, the output of :doc:`fish_mode_prompt <cmds/fish_mode_prompt>` will be prepended on the left.

If :envvar:`fish_transient_prompt` is set to 1, fish will redraw the prompt with a ``--final-rendering`` argument before running a commandline, allowing you to change it before pushing it to the scrollback.

Fish ships with a few prompts which you can see with :doc:`fish_config <cmds/fish_config>`. If you run just ``fish_config`` it will open a web interface [#]_ where you'll be shown the prompts and can pick which one you want. ``fish_config prompt show`` will show you the prompts right in your terminal.

For example ``fish_config prompt choose disco`` will temporarily select the "disco" prompt. If you like it and decide to keep it, run ``fish_config prompt save``.

You can also change these functions yourself by running ``funced fish_prompt`` and ``funcsave fish_prompt`` once you are happy with the result (or ``fish_right_prompt`` if you want to change that).

.. [#] The web interface runs purely locally on your computer and requires python to be installed.

.. _greeting:

Configurable greeting
---------------------

When it is started interactively, fish tries to run the :doc:`fish_greeting <cmds/fish_greeting>` function. The default fish_greeting prints a simple message. You can change its text by changing the ``$fish_greeting`` variable, for instance using a :ref:`universal variable <variables-universal>`::

  set -U fish_greeting

or you can set it :ref:`globally <variables-scope>` in :ref:`config.fish <configuration>`::

  set -g fish_greeting 'Hey, stranger!'

or you can script it by changing the function::

  function fish_greeting
      random choice "Hello!" "Hi" "G'day" "Howdy"
  end

save this in config.fish or :ref:`a function file <syntax-function-autoloading>`. You can also use :doc:`funced <cmds/funced>` and :doc:`funcsave <cmds/funcsave>` to edit it easily.

.. _title:

Programmable title
------------------

When using most terminals, it is possible to set the text displayed in the titlebar of the terminal window. Fish does this by running the :doc:`fish_title <cmds/fish_title>` function. It is executed before and after a command and the output is used as a titlebar message.

The :doc:`status current-command <cmds/status>` builtin will always return the name of the job to be put into the foreground (or ``fish`` if control is returning to the shell) when the :doc:`fish_title <cmds/fish_title>` function is called. The first argument will contain the most recently executed foreground command as a string.

The default title shows the hostname if connected via ssh, the currently running command (unless it is fish) and the current working directory. All of this is shortened to not make the tab too wide.

Examples:

To show the last command and working directory in the title::

    function fish_title
        # `prompt_pwd` shortens the title. This helps prevent tabs from becoming very wide.
        echo $argv[1] (prompt_pwd)
        pwd
    end

.. _editor:

Command line editor
-------------------

The fish editor features copy and paste, a :ref:`searchable history <history-search>` and many editor functions that can be bound to special keyboard shortcuts.

Like bash and other shells, fish includes two sets of keyboard shortcuts (or key bindings): one inspired by the Emacs text editor, and one by the vi text editor. The default editing mode is Emacs. You can switch to vi mode by running :doc:`fish_vi_key_bindings <cmds/fish_vi_key_bindings>` and switch back with :doc:`fish_default_key_bindings <cmds/fish_default_key_bindings>`. You can also make your own key bindings by creating a function and setting the ``fish_key_bindings`` variable to its name. For example::


    function fish_hybrid_key_bindings --description \
    "Vi-style bindings that inherit emacs-style bindings in all modes"
        for mode in default insert visual
            fish_default_key_bindings -M $mode
        end
        fish_vi_key_bindings --no-erase
    end
    set -g fish_key_bindings fish_hybrid_key_bindings

While the key bindings included with fish include many of the shortcuts popular from the respective text editors, they are not a complete implementation. They include a shortcut to open the current command line in your preferred editor (:kbd:`alt-e` by default) if you need the full power of your editor.

.. _shared-binds:

Shared bindings
^^^^^^^^^^^^^^^

Some bindings are common across Emacs and vi mode, because they aren't text editing bindings, or because what vi/Vim does for a particular key doesn't make sense for a shell.

- :kbd:`tab` :ref:`completes <tab-completion>` the current token. :kbd:`shift-tab` completes the current token and starts the pager's search mode. :kbd:`tab` is the same as :kbd:`ctrl-i`.

- :kbd:`left` (``←``) and :kbd:`right` (``→``) move the cursor left or right by one character. If the cursor is already at the end of the line, and an autosuggestion is available, :kbd:`right` (``→``) accepts the autosuggestion.

- :kbd:`enter` executes the current commandline or inserts a newline if it's not complete yet (e.g. a ``)`` or ``end`` is missing).

- :kbd:`alt-enter` inserts a newline at the cursor position. This is useful to add a line to a commandline that's already complete.

- :kbd:`alt-left` (``←``) and :kbd:`alt-right` (``→``) move the cursor left or right by one argument (or one word on macOS).
  If the command line is empty, they move forward/backward in the directory history.
  If the cursor is already at the end of the line, and an autosuggestion is available, :kbd:`alt-right` (``→``) (or :kbd:`alt-f`) accepts the first argument (or word on macOS) in the suggestion.

- :kbd:`ctrl-left` (``←``) and :kbd:`ctrl-right` (``→``) move the cursor left or right by one word. These accept one word of the autosuggestion - the part they'd move over.

- :kbd:`shift-left` (``←``) and :kbd:`shift-right` (``→``) move the cursor one word left or right, without stopping on punctuation. These accept one big word of the autosuggestion.

- :kbd:`up` (``↑``) and :kbd:`down` (``↓``) (or :kbd:`ctrl-p` and :kbd:`ctrl-n` for emacs aficionados) search the command history for the previous/next command containing the string that was specified on the commandline before the search was started. If the commandline was empty when the search started, all commands match. See the :ref:`history <history-search>` section for more information on history searching.

- :kbd:`alt-up` (``↑``) and :kbd:`alt-down` (``↓``) search the command history for the previous/next token containing the token under the cursor before the search was started. If the commandline was not on a token when the search started, all tokens match. See the :ref:`history <history-search>` section for more information on history searching.

- :kbd:`ctrl-c` interrupts/kills whatever is running (SIGINT).

- :kbd:`ctrl-d` deletes one character to the right of the cursor. If the command line is empty, :kbd:`ctrl-d` will exit fish.

- :kbd:`ctrl-u` removes contents from the beginning of line to the cursor (moving it to the :ref:`killring <killring>`).

- :kbd:`ctrl-l` clears and repaints the screen.

- :kbd:`ctrl-w` removes the previous path component (everything up to the previous "/", ":" or "@") (moving it to the :ref:`killring`).

- :kbd:`ctrl-x` copies the current buffer to the system's clipboard, :kbd:`ctrl-v` inserts the clipboard contents. (see :doc:`fish_clipboard_copy <cmds/fish_clipboard_copy>` and :doc:`fish_clipboard_paste <cmds/fish_clipboard_paste>`)

- :kbd:`alt-d` moves the next word to the :ref:`killring`.

- :kbd:`ctrl-delete` moves the next word (or next argument on macOS) to the :ref:`killring`.

- :kbd:`alt-d` lists the directory history if the command line is empty.

- :kbd:`alt-delete` moves the next argument (or word on macOS) to the :ref:`killring`.

- :kbd:`shift-delete` removes the current history item or autosuggestion from the command history.

- :kbd:`alt-h` (or :kbd:`f1`) shows the manual page for the current command, if one exists.

- :kbd:`alt-l` lists the contents of the current directory, unless the cursor is over a directory argument, in which case the contents of that directory will be listed.

- :kbd:`alt-o` opens the file at the cursor in a pager. If the cursor is in command position and the command is a script, it will instead open that script in your editor. The editor is chosen from the first available of the ``$VISUAL`` or ``$EDITOR`` variables.

- :kbd:`alt-p` adds the string ``&| less;`` to the end of the job under the cursor. The result is that the output of the command will be paged.

- :kbd:`alt-w` prints a short description of the command under the cursor.

- :kbd:`alt-e` edits the current command line in an external editor. The editor is chosen from the first available of the ``$VISUAL`` or ``$EDITOR`` variables.

- :kbd:`alt-v` Same as :kbd:`alt-e`.

- :kbd:`alt-s` Prepends ``sudo`` to the current commandline. If the commandline is empty, prepend ``sudo`` to the last commandline.

- :kbd:`ctrl-space` Inserts a space without expanding an :ref:`abbreviation <abbreviations>`. For vi mode, this only applies to insert-mode.

.. _emacs-mode:

Emacs mode commands
^^^^^^^^^^^^^^^^^^^

To enable emacs mode, use :doc:`fish_default_key_bindings <cmds/fish_default_key_bindings>`. This is also the default.

- :kbd:`home` or :kbd:`ctrl-a` moves the cursor to the beginning of the line.

- :kbd:`end` or :kbd:`ctrl-e` moves to the end of line. If the cursor is already at the end of the line, and an autosuggestion is available, :kbd:`end` or :kbd:`ctrl-e` accepts the autosuggestion.

- :kbd:`ctrl-b`, :kbd:`ctrl-f` move the cursor one character left or right or accept the autosuggestion just like the :kbd:`left` (``←``) and :kbd:`right` (``→``) shared bindings (which are available as well).

- :kbd:`alt-b`, :kbd:`alt-f` move the cursor one word left or right, or accept one word of the autosuggestion. If the command line is empty, moves forward/backward in the directory history instead.

- :kbd:`ctrl-n`, :kbd:`ctrl-p` move the cursor up/down or through history, like the up and down arrow shared bindings.

- :kbd:`delete` or :kbd:`backspace` or :kbd:`ctrl-h` removes one character forwards or backwards respectively.

- :kbd:`ctrl-backspace` removes one word backwards and :kbd:`alt-backspace` removes one argument backwards.
  On macOS, it's the other way round.

- :kbd:`alt-<` moves to the beginning of the commandline, :kbd:`alt->` moves to the end.

- :kbd:`ctrl-k` deletes from the cursor to the end of line (moving it to the :ref:`killring`).

- :kbd:`escape` and :kbd:`ctrl-g` cancel the current operation. Immediately after an unambiguous completion this undoes it.

- :kbd:`alt-c` capitalizes the current word.

- :kbd:`alt-u` makes the current word uppercase.

- :kbd:`ctrl-t` transposes the last two characters.

- :kbd:`alt-t` transposes the last two words.

- :kbd:`ctrl-z`, :kbd:`ctrl-_` (:kbd:`ctrl-/` on some terminals) undo the most recent edit of the line.

- :kbd:`alt-/` or :kbd:`ctrl-shift-z` reverts the most recent undo.

- :kbd:`ctrl-r` opens the history in a pager. This will show history entries matching the search, a few at a time. Pressing :kbd:`ctrl-r` again will search older entries, pressing :kbd:`ctrl-s` (that otherwise toggles pager search) will go to newer entries. The search bar will always be selected.


You can change these key bindings using the :doc:`bind <cmds/bind>` builtin.


.. _vi-mode:

Vi mode commands
^^^^^^^^^^^^^^^^

Vi mode allows for the use of vi-like commands at the prompt. Initially, :ref:`insert mode <vi-mode-insert>` is active. :kbd:`escape` enters :ref:`command mode <vi-mode-command>`. The commands available in command, insert and visual mode are described below. Vi mode shares :ref:`some bindings <shared-binds>` with :ref:`Emacs mode <emacs-mode>`.

To enable vi mode, use :doc:`fish_vi_key_bindings <cmds/fish_vi_key_bindings>`.
It is also possible to add all Emacs mode bindings to vi mode by using something like::


    function fish_user_key_bindings
        # Execute this once per mode that emacs bindings should be used in
        fish_default_key_bindings -M insert

        # Then execute the vi-bindings so they take precedence when there's a conflict.
        # Without --no-erase fish_vi_key_bindings will default to
        # resetting all bindings.
        # The argument specifies the initial mode (insert, "default" or visual).
        fish_vi_key_bindings --no-erase insert
    end


When in vi mode, the :doc:`fish_mode_prompt <cmds/fish_mode_prompt>` function will display a mode indicator to the left of the prompt. To disable this feature, override it with an empty function. To display the mode elsewhere (like in your right prompt), use the output of the ``fish_default_mode_prompt`` function.

When a binding switches the mode, it will repaint the mode-prompt if it exists, and the rest of the prompt only if it doesn't. So if you want a mode-indicator in your ``fish_prompt``, you need to erase ``fish_mode_prompt`` e.g. by adding an empty file at ``~/.config/fish/functions/fish_mode_prompt.fish``. (Bindings that change the mode are supposed to call the `repaint-mode` bind function, see :doc:`bind <cmds/bind>`)

The ``fish_vi_cursor`` function will be used to change the cursor's shape depending on the mode in supported terminals. The following snippet can be used to manually configure cursors after enabling vi mode::

   # Emulates vim's cursor shape behavior
   # Set the normal and visual mode cursors to a block
   set fish_cursor_default block
   # Set the insert mode cursor to a line
   set fish_cursor_insert line
   # Set the replace mode cursors to an underscore
   set fish_cursor_replace_one underscore
   set fish_cursor_replace underscore
   # Set the external cursor to a line. The external cursor appears when a command is started. 
   # The cursor shape takes the value of fish_cursor_default when fish_cursor_external is not specified.
   set fish_cursor_external line
   # The following variable can be used to configure cursor shape in
   # visual mode, but due to fish_cursor_default, is redundant here
   set fish_cursor_visual block

Additionally, ``blink`` can be added after each of the cursor shape parameters to set a blinking cursor in the specified shape.

Fish knows the shapes "block", "line" and "underscore", other values will be ignored.

If the cursor shape does not appear to be changing after setting the above variables, it's likely your terminal emulator does not support the capabilities necessary to do this.

.. _vi-mode-command:

Command mode
""""""""""""

Command mode is also known as normal mode.

- :kbd:`h` moves the cursor left.

- :kbd:`l` moves the cursor right.

- :kbd:`k` and :kbd:`j` search the command history for the previous/next command containing the string that was specified on the commandline before the search was started. If the commandline was empty when the search started, all commands match. See the :ref:`history <history-search>` section for more information on history searching. In multi-line commands, they move the cursor up and down respectively.

- :kbd:`i` enters :ref:`insert mode <vi-mode-insert>` at the current cursor position.

- :kbd:`I` enters :ref:`insert mode <vi-mode-insert>` at the beginning of the line.

- :kbd:`v` enters :ref:`visual mode <vi-mode-visual>` at the current cursor position.

- :kbd:`a` enters :ref:`insert mode <vi-mode-insert>` after the current cursor position.

- :kbd:`A` enters :ref:`insert mode <vi-mode-insert>` at the end of the line.

- :kbd:`o` inserts a new line under the current one and enters :ref:`insert mode <vi-mode-insert>`

- :kbd:`O` (capital-"o") inserts a new line above the current one and enters :ref:`insert mode <vi-mode-insert>`

- :kbd:`0` (zero) moves the cursor to beginning of line (remaining in command mode).

- :kbd:`d,d` deletes the current line and moves it to the :ref:`killring`.

- :kbd:`D` deletes text after the current cursor position and moves it to the :ref:`killring`.

- :kbd:`p` pastes text from the :ref:`killring`.

- :kbd:`u` undoes the most recent edit of the command line.

- :kbd:`ctrl-r` redoes the most recent edit.

- :kbd:`[` and :kbd:`]` search the command history for the previous/next token containing the token under the cursor before the search was started. See the :ref:`history <history-search>` section for more information on history searching.

- :kbd:`/` opens the history in a pager. This will show history entries matching the search, a few at a time. Pressing it again will search older entries, pressing :kbd:`ctrl-s` (that otherwise toggles pager search) will go to newer entries. The search bar will always be selected.

- :kbd:`backspace` moves the cursor left.

- :kbd:`g` / :kbd:`G` moves the cursor to the beginning/end of the commandline, respectively.

- :kbd:`:,q` exits fish.

.. _vi-mode-insert:

Insert mode
"""""""""""

- :kbd:`escape` enters :ref:`command mode <vi-mode-command>`.

- :kbd:`backspace` removes one character to the left.

- :kbd:`ctrl-n` accepts the autosuggestion.

.. _vi-mode-visual:

Visual mode
"""""""""""

- :kbd:`left (``←``) and :kbd:`right`(``→``) extend the selection backward/forward by one character.

- :kbd:`h` moves the cursor left.

- :kbd:`l` moves the cursor right.

- :kbd:`k` moves the cursor up.

- :kbd:`j` moves the cursor down.

- :kbd:`b` and :kbd:`w` extend the selection backward/forward by one word.

- :kbd:`d` and :kbd:`x` move the selection to the :ref:`killring` and enter :ref:`command mode <vi-mode-command>`.

- :kbd:`escape` and :kbd:`ctrl-c` enter :ref:`command mode <vi-mode-command>`.

- :kbd:`c` and :kbd:`s` remove the selection and switch to insert mode.

- :kbd:`X` moves the entire line to the :ref:`killring`, and enters :ref:`command mode <vi-mode-command>`.

- :kbd:`y` copies the selection to the :ref:`killring`, and enters :ref:`command mode <vi-mode-command>`.

- :kbd:`~` toggles the case (upper/lower) on the selection, and enters :ref:`command mode <vi-mode-command>`.

- :kbd:`",*,y` copies the selection to the clipboard, and enters :ref:`command mode <vi-mode-command>`.

.. _custom-binds:

Custom bindings
^^^^^^^^^^^^^^^

In addition to the standard bindings listed here, you can also define your own with :doc:`bind <cmds/bind>`::

  # Prints ``^C`` and a new prompt
  bind ctrl-c cancel-commandline

Put ``bind`` statements into :ref:`config.fish <configuration>` or a function called ``fish_user_key_bindings``.

If you change your mind on a binding and want to go back to fish's default, you can erase it again::

  bind --erase ctrl-c

Fish remembers its preset bindings and so it will take effect again. This saves you from having to remember what it was before and add it again yourself.

If you use :ref:`vi bindings <vi-mode>`, note that ``bind`` will by default bind keys in :ref:`command mode <vi-mode-command>`. To bind something in :ref:`insert mode <vi-mode-insert>`::

  bind --mode insert ctrl-c 'commandline -r ""'

.. _interactive-key-sequences:

Key sequences
"""""""""""""

To find out the name of a key, you can use :doc:`fish_key_reader <cmds/fish_key_reader>`.

::

  > fish_key_reader # Press Alt + right-arrow
  Press a key:
  bind alt-right 'do something'
  
Note that the historical way the terminal encodes keys and sends them to the application (fish, in this case) makes a lot of combinations indistinguishable or unbindable. In the usual encoding, :kbd:`ctrl-i` *is the same* as the tab key, and shift cannot be detected when ctrl is also pressed.

There are more powerful encoding schemes, and fish tries to tell the terminal to turn them on, but there are still many terminals that do not support them. When ``fish_key_reader`` prints the same sequence for two different keys, then that is because your terminal sends the same sequence for them, and there isn't anything fish can do about it. It is our hope that these schemes will become more widespread, making input more flexible.

In the historical scheme, :kbd:`escape` is the same thing as :kbd:`alt` in a terminal. To distinguish between pressing :kbd:`escape` and then another key, and pressing :kbd:`alt` and that key (or an escape sequence the key sends), fish waits for a certain time after seeing an escape character. This is configurable via the :envvar:`fish_escape_delay_ms` variable.

If you want to be able to press :kbd:`escape` and then a character and have it count as :kbd:`alt`\ +\ that character, set it to a higher value, e.g.::

  set -g fish_escape_delay_ms 100

Similarly, to disambiguate *other* keypresses where you've bound a subsequence and a longer sequence, fish has :envvar:`fish_sequence_key_delay_ms`::

  # This binds the sequence j,k to switch to normal mode in vi mode.
  # If you kept it like that, every time you press "j",
  # fish would wait for a "k" or other key to disambiguate
  bind -M insert -m default j,k cancel repaint-mode

  # After setting this, fish only waits 200ms for the "k",
  # or decides to treat the "j" as a separate sequence, inserting it.
  set -g fish_sequence_key_delay_ms 200

.. _killring:

Copy and paste (Kill Ring)
^^^^^^^^^^^^^^^^^^^^^^^^^^

Fish uses an Emacs-style kill ring for copy and paste functionality. For example, use :kbd:`ctrl-k` (`kill-line`) to cut from the current cursor position to the end of the line. The string that is cut (a.k.a. killed in emacs-ese) is inserted into a list of kills, called the kill ring. To paste the latest value from the kill ring (emacs calls this "yanking") use :kbd:`ctrl-y` (the ``yank`` input function). After pasting, use :kbd:`alt-y` (``yank-pop``) to rotate to the previous kill.

Copy and paste from outside are also supported, both via the :kbd:`ctrl-x` / :kbd:`ctrl-v` bindings (the ``fish_clipboard_copy`` and ``fish_clipboard_paste`` functions [#]_) and via the terminal's paste function, for which fish enables "Bracketed Paste Mode", so it can tell a paste from manually entered text.
In addition, when pasting inside single quotes, pasted single quotes and backslashes are automatically escaped so that the result can be used as a single token by closing the quote after.
Kill ring entries are stored in ``fish_killring`` variable.

The commands ``begin-selection`` and ``end-selection`` (unbound by default; used for selection in vi visual mode) control text selection together with cursor movement commands that extend the current selection.
The variable :envvar:`fish_cursor_selection_mode` can be used to configure if that selection should include the character under the cursor (``inclusive``) or not (``exclusive``). The default is ``exclusive``, which works well with any cursor shape. For vi mode, and particularly for the ``block`` or ``underscore`` cursor shapes you may prefer ``inclusive``.

.. [#] These rely on external tools. Currently xsel, xclip, wl-copy/wl-paste and pbcopy/pbpaste are supported.

.. _multiline:

Multiline editing
^^^^^^^^^^^^^^^^^

The fish commandline editor can be used to work on commands that are several lines long. There are three ways to make a command span more than a single line:

- Pressing the :kbd:`enter` key while a block of commands is unclosed, such as when one or more block commands such as ``for``, ``begin`` or ``if`` do not have a corresponding :doc:`end <cmds/end>` command.

- Pressing :kbd:`alt-enter` instead of pressing the :kbd:`enter` key.

- By inserting a backslash (``\``) character before pressing the :kbd:`enter` key, escaping the newline.

The fish commandline editor works exactly the same in single line mode and in multiline mode. To move between lines use the left and right arrow keys and other such keyboard shortcuts.

.. _history-search:

Searchable command history
^^^^^^^^^^^^^^^^^^^^^^^^^^

After a command has been executed, it is remembered in the history list. Any duplicate history items are automatically removed. By pressing the up and down keys, you can search forwards and backwards in the history. If the current command line is not empty when starting a history search, only the commands containing the string entered into the command line are shown.

By pressing :kbd:`alt-up` (``↑``) and :kbd:`alt-down` (``↓``), a history search is also performed, but instead of searching for a complete commandline, each commandline is broken into separate elements like it would be before execution, and the history is searched for an element matching that under the cursor.

For more complicated searches, you can press :kbd:`ctrl-r` to open a pager that allows you to search the history. It shows a limited number of entries in one page, press :kbd:`ctrl-r` [#]_ again to move to the next page and :kbd:`ctrl-s` [#]_ to move to the previous page. You can change the text to refine your search.

History searches are case-insensitive unless the search string contains an uppercase character. You can stop a search to edit your search string by pressing :kbd:`escape` or :kbd:`pagedown`.

Prefixing the commandline with a space will prevent the entire line from being stored in the history. It will still be available for recall until the next command is executed, but will not be stored on disk. This is to allow you to fix misspellings and such.

The command history is stored in the file ``~/.local/share/fish/fish_history`` (or
``$XDG_DATA_HOME/fish/fish_history`` if that variable is set) by default. However, you can set the
``fish_history`` environment variable to change the name of the history session (resulting in a
``<session>_history`` file); both before starting the shell and while the shell is running.

See the :doc:`history <cmds/history>` command for other manipulations.

Examples:

To search for previous entries containing the word 'make', type ``make`` in the console and press the up key.

If the commandline reads ``cd m``, place the cursor over the ``m`` character and press :kbd:`alt-up` (``↑``) to search for previously typed words containing 'm'.

.. [#] Or another binding that triggers the ``history-pager`` input function. See :doc:`bind <cmds/bind>` for a list.
.. [#] Or another binding that triggers the ``pager-toggle-search`` input function.

.. _private-mode:

Private mode
-------------

Fish has a private mode, in which command history will not be written to the history file on disk. To enable it, either set ``$fish_private_mode`` to a non-empty value, or launch with ``fish --private`` (or ``fish -P`` for short).

If you launch fish with ``-P``, it both hides old history and prevents writing history to disk. This is useful to avoid leaking personal information (e.g. for screencasts) or when dealing with sensitive information.

You can query the variable ``fish_private_mode`` (``if test -n "$fish_private_mode" ...``) if you would like to respect the user's wish for privacy and alter the behavior of your own fish scripts.

Navigating directories
----------------------

.. _directory-history:

Navigating directories is usually done with the :doc:`cd <cmds/cd>` command, but fish offers some advanced features as well.

The current working directory can be displayed with the :doc:`pwd <cmds/pwd>` command, or the ``$PWD`` :ref:`special variable <variables-special>`. Usually your prompt already does this.

Directory history
^^^^^^^^^^^^^^^^^

Fish automatically keeps a trail of the recent visited directories with :doc:`cd <cmds/cd>` by storing this history in the ``dirprev`` and ``dirnext`` variables.

Several commands are provided to interact with this directory history:

- :doc:`dirh <cmds/dirh>` prints the history
- :doc:`cdh <cmds/cdh>` displays a prompt to quickly navigate the history
- :doc:`prevd <cmds/prevd>` moves backward through the history. It is bound to :kbd:`alt-left` (``←``)
- :doc:`nextd <cmds/nextd>` moves forward through the history. It is bound to :kbd:`alt-right` (``→``)

.. _directory-stack:

Directory stack
^^^^^^^^^^^^^^^

Another set of commands, usually also available in other shells like bash, deal with the directory stack. Stack handling is not automatic and needs explicit calls of the following commands:

- :doc:`dirs <cmds/dirs>` prints the stack
- :doc:`pushd <cmds/pushd>` adds a directory on top of the stack and makes it the current working directory
- :doc:`popd <cmds/popd>` removes the directory on top of the stack and changes the current working directory
