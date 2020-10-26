.. _cmd-bind:

bind - handle fish key bindings
===============================

Synopsis
--------

::

    bind [(-M | --mode) MODE] [(-m | --sets-mode) NEW_MODE] [--preset | --user] [(-s | --silent)] [(-k | --key)] SEQUENCE COMMAND [COMMAND...]
    bind [(-M | --mode) MODE] [(-k | --key)] [--preset] [--user] SEQUENCE
    bind (-K | --key-names) [(-a | --all)] [--preset] [--user]
    bind (-f | --function-names)
    bind (-L | --list-modes)
    bind (-e | --erase) [(-M | --mode) MODE] [--preset] [--user] (-a | --all | [(-k | --key)] SEQUENCE [SEQUENCE...])

Description
-----------

``bind`` manages bindings.

It can add bindings if given a SEQUENCE of characters to bind to. These should be written as :ref:`fish escape sequences <escapes>`. The most important of these are ``\c`` for the control key, and ``\e`` for escape, and because of historical reasons also the Alt key (sometimes also called "Meta").

For example, :kbd:`Alt`\ +\ :kbd:`W` can be written as ``\ew``, and :kbd:`Control`\ +\ :kbd:`X` (^X) can be written as ``\cx``. Note that Alt-based key bindings are case sensitive and Control-based key bindings are not. This is a constraint of text-based terminals, not ``fish``.

The generic key binding that matches if no other binding does can be set by specifying a ``SEQUENCE`` of the empty string (that is, ``''`` ). For most key bindings, it makes sense to bind this to the ``self-insert`` function (i.e. ``bind '' self-insert``). This will insert any keystrokes not specifically bound to into the editor. Non-printable characters are ignored by the editor, so this will not result in control sequences being inserted.

If the ``-k`` switch is used, the name of a key (such as 'down', 'up' or 'backspace') is used instead of a sequence. The names used are the same as the corresponding curses variables, but without the 'key\_' prefix. (See ``terminfo(5)`` for more information, or use ``bind --key-names`` for a list of all available named keys). Normally this will print an error if the current ``$TERM`` entry doesn't have a given key, unless the ``-s`` switch is given.

``COMMAND`` can be any fish command, but it can also be one of a set of special input functions. These include functions for moving the cursor, operating on the kill-ring, performing tab completion, etc. Use ``bind --function-names`` for a complete list of these input functions.

When ``COMMAND`` is a shellscript command, it is a good practice to put the actual code into a `function <#function>`__ and simply bind to the function name. This way it becomes significantly easier to test the function while editing, and the result is usually more readable as well.

If a script produces output, it should finish by calling ``commandline -f repaint`` to tell fish that a repaint is in order.

Note that special input functions cannot be combined with ordinary shell script commands. The commands must be entirely a sequence of special input functions (from ``bind -f``) or all shell script commands (i.e., valid fish script).

If no ``SEQUENCE`` is provided, all bindings (or just the bindings in the given ``MODE``) are printed. If ``SEQUENCE`` is provided but no ``COMMAND``, just the binding matching that sequence is printed.

To save custom keybindings, put the ``bind`` statements into :ref:`config.fish <initialization>`. Alternatively, fish also automatically executes a function called ``fish_user_key_bindings`` if it exists.

Key bindings may use "modes", which mimics Vi's modal input behavior. The default mode is "default", and every bind applies to a single mode. The mode can be viewed/changed with the ``$fish_bind_mode`` variable.

Options
-------
The following options are available:

- ``-k`` or ``--key`` Specify a key name, such as 'left' or 'backspace' instead of a character sequence

- ``-K`` or ``--key-names`` Display a list of available key names. Specifying ``-a`` or ``--all`` includes keys that don't have a known mapping

- ``-f`` or ``--function-names`` Display a list of available input functions

- ``-L`` or ``--list-modes`` Display a list of defined bind modes

- ``-M MODE`` or ``--mode MODE`` Specify a bind mode that the bind is used in. Defaults to "default"

- ``-m NEW_MODE`` or ``--sets-mode NEW_MODE`` Change the current mode to ``NEW_MODE`` after this binding is executed

- ``-e`` or ``--erase`` Erase the binding with the given sequence and mode instead of defining a new one. Multiple sequences can be specified with this flag. Specifying ``-a`` or ``--all`` with ``-M`` or ``--mode`` erases all binds in the given mode regardless of sequence. Specifying ``-a`` or ``--all`` without ``-M`` or ``--mode`` erases all binds in all modes regardless of sequence.

- ``-a`` or ``--all`` See ``--erase`` and ``--key-names``

- ``--preset`` and ``--user`` specify if bind should operate on user or preset bindings. User bindings take precedence over preset bindings when fish looks up mappings. By default, all ``bind`` invocations work on the "user" level except for listing, which will show both levels. All invocations except for inserting new bindings can operate on both levels at the same time (if both ``--preset`` and ``--user`` are given). ``--preset`` should only be used in full binding sets (like when working on ``fish_vi_key_bindings``).

Special input functions
-----------------------
The following special input functions are available:

- ``and``, only execute the next function if the previous succeeded (note: only some functions report success)

- ``accept-autosuggestion``, accept the current autosuggestion completely

- ``backward-char``, moves one character to the left

- ``backward-bigword``, move one whitespace-delimited word to the left

- ``backward-delete-char``, deletes one character of input to the left of the cursor

- ``backward-kill-bigword``, move the whitespace-delimited word to the left of the cursor to the killring

- ``backward-kill-line``, move everything from the beginning of the line to the cursor to the killring

- ``backward-kill-path-component``, move one path component to the left of the cursor to the killring. A path component is everything likely to belong to a path component, i.e. not any of the following: `/={,}'\":@ |;<>&`, plus newlines and tabs.

- ``backward-kill-word``, move the word to the left of the cursor to the killring. The "word" here is everything up to punctuation or whitespace.

- ``backward-word``, move one word to the left

- ``beginning-of-buffer``, moves to the beginning of the buffer, i.e. the start of the first line

- ``beginning-of-history``, move to the beginning of the history

- ``beginning-of-line``, move to the beginning of the line

- ``begin-selection``, start selecting text

- ``cancel``, cancel the current commandline and replace it with a new empty one

- ``cancel-commandline``, cancel the current commandline and replace it with a new empty one, leaving the old one in place with a marker to show that it was cancelled

- ``capitalize-word``, make the current word begin with a capital letter

- ``complete``, guess the remainder of the current token

- ``complete-and-search``, invoke the searchable pager on completion options (for convenience, this also moves backwards in the completion pager)

- ``delete-char``, delete one character to the right of the cursor

- ``delete-or-exit``, deletes one character to the right of the cursor or exits the shell if the commandline is empty.

- ``down-line``, move down one line

- ``downcase-word``, make the current word lowercase

- ``end-of-buffer``, moves to the end of the buffer, i.e. the end of the first line

- ``end-of-history``, move to the end of the history

- ``end-of-line``, move to the end of the line

- ``end-selection``, end selecting text

- ``expand-abbr``, expands any abbreviation currently under the cursor

- ``execute``, run the current commandline

- ``forward-bigword``, move one whitespace-delimited word to the right

- ``forward-char``, move one character to the right

- ``forward-single-char``, move one character to the right; if an autosuggestion is available, only take a single char from it

- ``forward-word``, move one word to the right

- ``history-search-backward``, search the history for the previous match

- ``history-search-forward``, search the history for the next match

- ``history-prefix-search-backward``, search the history for the previous prefix match

- ``history-prefix-search-forward``, search the history for the next prefix match

- ``history-token-search-backward``, search the history for the previous matching argument

- ``history-token-search-forward``, search the history for the next matching argument

- ``forward-jump`` and ``backward-jump``, read another character and jump to its next occurence after/before the cursor

- ``forward-jump-till`` and ``backward-jump-till``, jump to right *before* the next occurence

- ``repeat-jump`` and ``repeat-jump-reverse``, redo the last jump in the same/opposite direction

- ``kill-bigword``, move the next whitespace-delimited word to the killring

- ``kill-line``, move everything from the cursor to the end of the line to the killring

- ``kill-selection``, move the selected text to the killring

- ``kill-whole-line``, move the line to the killring

- ``kill-word``, move the next word to the killring

- ``or``, only execute the next function if the previous succeeded (note: only some functions report success)

- ``pager-toggle-search``, toggles the search field if the completions pager is visible.

- ``repaint``, reexecutes the prompt functions and redraws the prompt (also ``force-repaint`` for backwards-compatibility)

- ``repaint-mode``, reexecutes the :ref:`fish_mode_prompt <cmd-fish_mode_prompt>` and redraws the prompt. This is useful for vi-mode. If no ``fish_mode_prompt`` exists or it prints nothing, it acts like a normal repaint.

- ``self-insert``, inserts the matching sequence into the command line

- ``self-insert-notfirst``, inserts the matching sequence into the command line, unless the cursor is at the beginning

- ``suppress-autosuggestion``, remove the current autosuggestion. Returns true if there was a suggestion to remove.

- ``swap-selection-start-stop``, go to the other end of the highlighted text without changing the selection

- ``transpose-chars``,  transpose two characters to the left of the cursor

- ``transpose-words``, transpose two words to the left of the cursor

- ``up-line``, move up one line

- ``undo`` and ``redo``, revert or redo the most recent edits on the command line

- ``upcase-word``, make the current word uppercase

- ``yank``, insert the latest entry of the killring into the buffer

- ``yank-pop``, rotate to the previous entry of the killring

Examples
--------

Exit the shell when :kbd:`Control`\ +\ :kbd:`D` is pressed::

    bind \cd 'exit'

Perform a history search when :kbd:`Page Up` is pressed::

    bind -k ppage history-search-backward

Turn on Vi key bindings and rebind :kbd:`Control`\ +\ :kbd:`C` to clear the input line::

    set -g fish_key_bindings fish_vi_key_bindings
    bind -M insert \cc kill-whole-line repaint

Launch ``git diff`` and repaint the commandline afterwards when :kbd:`Control`\ +\ :kbd:`G` is pressed::

   bind \cg 'git diff; commandline -f repaint'

.. _cmd-bind-termlimits:

Terminal Limitations
--------------------

Unix terminals, like the ones fish operates in, are at heart 70s technology. They have some limitations that applications running inside them can't workaround.

For instance, the control key modifies a character by setting the top three bits to 0. This means:

- Many characters + control are indistinguishable from other keys. :kbd:`Control`\ +\ :kbd:`I` *is* tab, :kbd:`Control`\ +\ :kbd:`J` *is* newline (`\n`).
- Control and shift don't work simultaneously

Other keys don't have a direct encoding, and are sent as escape sequences. For example :kbd:`→` (Right) often sends ``\e\[C``. These can differ from terminal to terminal, and the mapping is typically available in `terminfo(5)`. Sometimes however a terminal identifies as e.g. ``xterm-256color`` for compatibility, but then implements xterm's sequences incorrectly.

.. _cmd-bind-escape:

Special Case: The Escape Character
----------------------------------

The escape key can be used standalone, for example, to switch from insertion mode to normal mode when using Vi keybindings. Escape can also be used as a "meta" key, to indicate the start of an escape sequence, like for function or arrow keys. Custom bindings can also be defined that begin with an escape character.

Holding alt and something else also typically sends escape, for example holding alt+a will send an escape character and then an "a".

fish waits for a period after receiving the escape character, to determine whether it is standalone or part of an escape sequence. While waiting, additional key presses make the escape key behave as a meta key. If no other key presses come in, it is handled as a standalone escape. The waiting period is set to 30 milliseconds (0.03 seconds). It can be configured by setting the ``fish_escape_delay_ms`` variable to a value between 10 and 5000 ms. This can be a universal variable that you set once from an interactive session.
