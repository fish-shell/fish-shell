.. _cmd-bind:

bind - handle fish key bindings
===============================
Synopsis
--------

.. synopsis::

    bind [(-M | --mode) MODE] [(-m | --sets-mode) NEW_MODE] [--preset | --user] [-s | --silent] KEYS COMMAND ...
    bind [(-M | --mode) MODE] [--preset] [--user] [KEYS]
    bind [-a | --all] [--preset] [--user]
    bind (-f | --function-names)
    bind (-L | --list-modes)
    bind (-e | --erase) [(-M | --mode) MODE] [--preset] [--user] [-a | --all] | KEYS ...

Description
-----------

``bind`` manages key bindings.

If both ``KEYS`` and ``COMMAND`` are given, ``bind`` adds (or replaces) a binding in ``MODE``.
If only ``KEYS`` is given, any existing binding in the given ``MODE`` will be printed.

``KEYS`` is a comma-separated list of key names.
Modifier keys can be specified by prefixing a key name with a combination of ``ctrl-``/``c-``, ``alt-``/``a-`` and ``shift-``.
For example, pressing :kbd:`w` while holding the Alt modifier is written as ``alt-w``.
Key names are case-sensitive; for example ``alt-W`` is the same as ``alt-shift-w``.
``ctrl-x,ctrl-e`` would mean pressing :kbd:`ctrl-x` followed by :kbd:`ctrl-e`.

Some keys have names, usually because they don't have an obvious printable character representation.
They are:

- the arrow keys ``up``, ``down``, ``left`` and ``right``,
- ``backspace``,
- ``comma`` (``,``),
- ``delete``,
- ``end``,
- ``enter``,
- ``escape``,
- ``f1`` through ``f12``.
- ``home``,
- ``insert``,
- ``minus`` (``-``),
- ``pageup``,
- ``pagedown``,
- ``space`` and
- ``tab``,

These names are case-sensitive.

An empty value (``''``) for ``KEYS`` designates the generic binding that will be used if nothing else matches. For most bind modes, it makes sense to bind this to the ``self-insert`` function (i.e. ``bind '' self-insert``). This will insert any keystrokes that have no bindings otherwise. Non-printable characters are ignored by the editor, so this will not result in control sequences being inserted.

To find the name of a key combination you can use :doc:`fish_key_reader <fish_key_reader>`.

``COMMAND`` can be any fish command, but it can also be one of a set of special input functions. These include functions for moving the cursor, operating on the kill-ring, performing tab completion, etc. Use ``bind --function-names`` or :ref:`see below <special-input-functions>` for a list of these input functions.

.. note::
    If a script changes the commandline, it should finish by calling the ``repaint`` special input function.

If no ``KEYS`` argument is provided, all bindings (in the given ``MODE``) are printed. If ``KEYS`` is provided but no ``COMMAND``, just the binding matching that sequence is printed.

Key bindings may use "modes", which mimics vi's modal input behavior. The default mode is "default". Every key binding applies to a single mode; you can specify which one with ``-M MODE``. If the key binding should change the mode, you can specify the new mode with ``-m NEW_MODE``. The mode can be viewed and changed via the ``$fish_bind_mode`` variable. If you want to change the mode from inside a fish function, use ``set fish_bind_mode MODE``.

To save custom key bindings, put the ``bind`` statements into :ref:`config.fish <configuration>`. Alternatively, fish also automatically executes a function called ``fish_user_key_bindings`` if it exists.

Options
-------
The following options are available:

**-f** or **--function-names**
    Display a list of available input functions

**-L** or **--list-modes**
    Display a list of defined bind modes

**-M MODE** or **--mode** *MODE*
    Specify a bind mode that the bind is used in. Defaults to "default"

**-m NEW_MODE** or **--sets-mode** *NEW_MODE*
    Change the current mode to *NEW_MODE* after this binding is executed

**-e** or **--erase**
    Erase the binding with the given sequence and mode instead of defining a new one.
    Multiple sequences can be specified with this flag.
    Specifying **-a** or **--all** with **-M** or **--mode** erases all binds in the given mode regardless of sequence.
    Specifying **-a** or **--all** without **-M** or **--mode** erases all binds in all modes regardless of sequence.

**-a** or **--all**
    See **--erase**

**--preset** and **--user**
    Specify if bind should operate on user or preset bindings.
    User bindings take precedence over preset bindings when fish looks up mappings.
    By default, all ``bind`` invocations work on the "user" level except for listing, which will show both levels.
    All invocations except for inserting new bindings can operate on both levels at the same time (if both **--preset** and **--user** are given).
    **--preset** should only be used in full binding sets (like when working on ``fish_vi_key_bindings``).

**-s** or **--silent**
    Silences some of the error messages, including for unknown key names and unbound sequences.

**-h** or **--help**
    Displays help about using this command.

.. _special-input-functions:
   
Special input functions
-----------------------
The following special input functions are available:

``and``
    only execute the next function if the previous succeeded (note: only some functions report success)

``accept-autosuggestion``
    accept the current autosuggestion

``backward-char``
    move one character to the left.
    If the completion pager is active, select the previous completion instead.

``backward-char-passive``
    move one character to the left, but do not trigger any non-movement-related operations. If the cursor is at the start of
    the commandline, does nothing. Does not change the selected item in the completion pager UI when shown.

``backward-bigword``
    move one whitespace-delimited word to the left

``backward-delete-char``
    deletes one character of input to the left of the cursor

``backward-kill-bigword``
    move the whitespace-delimited word to the left of the cursor to the killring

``backward-kill-line``
    move everything from the beginning of the line to the cursor to the killring

``backward-kill-path-component``
    move one path component to the left of the cursor to the killring. A path component is everything likely to belong to a path component, i.e. not any of the following: `/={,}'\":@ |;<>&`, plus newlines and tabs.

``backward-kill-word``
    move the word to the left of the cursor to the killring. The "word" here is everything up to punctuation or whitespace.

``backward-word``
    move one word to the left

``beginning-of-buffer``
    moves to the beginning of the buffer, i.e. the start of the first line

``beginning-of-history``
    move to the beginning of the history

``beginning-of-line``
    move to the beginning of the line

``begin-selection``
    start selecting text

``cancel``
    cancel the current commandline and replace it with a new empty one

``cancel-commandline``
    cancel the current commandline and replace it with a new empty one, leaving the old one in place with a marker to show that it was cancelled

``capitalize-word``
    make the current word begin with a capital letter

``clear-screen``
    clears the screen and redraws the prompt. if the terminal doesn't support clearing the screen it is the same as ``repaint``.

``complete``
    guess the remainder of the current token

``complete-and-search``
    invoke the searchable pager on completion options (for convenience, this also moves backwards in the completion pager)

``delete-char``
    delete one character to the right of the cursor

``delete-or-exit``
    delete one character to the right of the cursor, or exit the shell if the commandline is empty

``down-line``
    move down one line

``downcase-word``
    make the current word lowercase

``end-of-buffer``
    moves to the end of the buffer, i.e. the end of the first line

``end-of-history``
    move to the end of the history

``end-of-line``
    move to the end of the line

``end-selection``
    end selecting text

``expand-abbr``
    expands any abbreviation currently under the cursor

``execute``
    run the current commandline

``exit``
    exit the shell

``forward-bigword``
    move one whitespace-delimited word to the right

``forward-char``
    move one character to the right; or if at the end of the commandline, accept the current autosuggestion.
    If the completion pager is active, select the next completion instead.

``forward-char-passive``
    move one character to the right, but do not trigger any non-movement-related operations. If the cursor is at the end of the
    commandline, does not accept the current autosuggestion (if any). Does not change the selected item in the completion pager,
    if shown.

``forward-single-char``
    move one character to the right; or if at the end of the commandline, accept a single char from the current autosuggestion.

``forward-word``
    move one word to the right; or if at the end of the commandline, accept one word
    from the current autosuggestion.

``history-pager``
    invoke the searchable pager on history (incremental search); or if the history pager is already active, search further backwards in time.

``history-pager-delete``
    permanently delete the current history item, either from the history pager or from an active up-arrow history search

``history-search-backward``
    search the history for the previous match

``history-search-forward``
    search the history for the next match

``history-prefix-search-backward``
    search the history for the previous prefix match

``history-prefix-search-forward``
    search the history for the next prefix match

``history-token-search-backward``
    search the history for the previous matching argument

``history-token-search-forward``
    search the history for the next matching argument

``forward-jump`` and ``backward-jump``
    read another character and jump to its next occurence after/before the cursor

``forward-jump-till`` and ``backward-jump-till``
    jump to right *before* the next occurence

``repeat-jump`` and ``repeat-jump-reverse``
    redo the last jump in the same/opposite direction

``jump-to-matching-bracket``
    jump to matching bracket if the character under the cursor is bracket;
    otherwise, jump to the next occurence of *any right* bracket after the cursor.
    The following brackets are considered: ``([{}])``

``jump-till-matching-bracket``
    the same as ``jump-to-matching-bracket`` but offset cursor to the right for left bracket, and offset cursor to the left for right bracket.
    The offset is applied for both the position we jump from and position we jump to.
    In other words, the cursor will continuously jump inside the brackets but won't reach them by 1 character.
    The input function is useful to emulate ``ib`` vi text object.
    The following brackets are considered: ``([{}])``

``kill-bigword``
    move the next whitespace-delimited word to the killring

``kill-line``
    move everything from the cursor to the end of the line to the killring

``kill-selection``
    move the selected text to the killring

``kill-whole-line``
    move the line (including the following newline) to the killring. If the line is the last line, its preceeding newline is also removed

``kill-inner-line``
    move the line (without the following newline) to the killring

``kill-word``
    move the next word to the killring

``nextd-or-forward-word``
    if the commandline is empty, then move forward in the directory history, otherwise move one word to the right;
    or if at the end of the commandline, accept one word from the current autosuggestion.

``or``
    only execute the next function if the previous did not succeed (note: only some functions report failure)

``pager-toggle-search``
    toggles the search field if the completions pager is visible; or if used after ``history-pager``, search forwards in time.

``prevd-or-backward-word``
    if the commandline is empty, then move backward in the directory history, otherwise move one word to the left

``repaint``
    reexecutes the prompt functions and redraws the prompt (also ``force-repaint`` for backwards-compatibility)

``repaint-mode``
    reexecutes the :doc:`fish_mode_prompt <fish_mode_prompt>` and redraws the prompt. This is useful for vi mode. If no ``fish_mode_prompt`` exists or it prints nothing, it acts like a normal repaint.

``self-insert``
    inserts the matching sequence into the command line

``self-insert-notfirst``
    inserts the matching sequence into the command line, unless the cursor is at the beginning

``suppress-autosuggestion``
    remove the current autosuggestion. Returns true if there was a suggestion to remove.

``swap-selection-start-stop``
    go to the other end of the highlighted text without changing the selection

``transpose-chars``
    transpose two characters to the left of the cursor

``transpose-words``
    transpose two words to the left of the cursor

``togglecase-char``
    toggle the capitalisation (case) of the character under the cursor

``togglecase-selection``
    toggle the capitalisation (case) of the selection

``insert-line-under``
    add a new line under the current line

``insert-line-over``
    add a new line over the current line

``up-line``
    move up one line

``undo`` and ``redo``
    revert or redo the most recent edits on the command line

``upcase-word``
    make the current word uppercase

``yank``
    insert the latest entry of the killring into the buffer

``yank-pop``
    rotate to the previous entry of the killring

Additional functions
--------------------
The following functions are included as normal functions, but are particularly useful for input editing:

``up-or-search`` and ``down-or-search``
     move the cursor or search the history depending on the cursor position and current mode

``edit_command_buffer``
    open the visual editor (controlled by the :envvar:`VISUAL` or :envvar:`EDITOR` environment variables) with the current command-line contents

``fish_clipboard_copy``
    copy the current selection to the system clipboard

``fish_clipboard_paste``
    paste the current selection from the system clipboard before the cursor

``fish_commandline_append``
    append the argument to the command-line. If the command-line already ends with the argument, this removes the suffix instead. Starts with the last command from history if the command-line is empty.

``fish_commandline_prepend``
    prepend the argument to the command-line. If the command-line already starts with the argument, this removes the prefix instead. Starts with the last command from history if the command-line is empty.

Examples
--------

Exit the shell when :kbd:`ctrl-d` is pressed::

    bind ctrl-d 'exit'

Perform a history search when :kbd:`pageup` is pressed::

    bind pageup history-search-backward

Turn on :ref:`vi key bindings <vi-mode>` and rebind :kbd:`ctrl-c` to clear the input line::

    set -g fish_key_bindings fish_vi_key_bindings
    bind -M insert ctrl-c kill-whole-line repaint

Launch ``git diff`` and repaint the commandline afterwards when :kbd:`ctrl-g` is pressed::

   bind ctrl-g 'git diff' repaint

.. _cmd-bind-termlimits:

Terminal Limitations
--------------------

Unix terminals, like the ones fish operates in, are at heart 70s technology. They have some limitations that applications running inside them can't workaround.

For instance, historically the control key modifies a character by setting the top three bits to 0. This means:

- Many characters + control are indistinguishable from other keys: :kbd:`ctrl-i` *is* :kbd:`tab`, :kbd:`ctrl-j` *is* newline (``\n``).
- Control and shift don't work simultaneously - :kbd:`ctrl-X` is the same as :kbd:`ctrl-x`.

Other keys don't have a direct encoding, and are sent as escape sequences. For example :kbd:`right` (``→``) usually sends ``\e\[C``.

Some modern terminals support newer encodings for keys, that allow distinguishing more characters and modifiers, and fish enables as many of these as it can, automatically.

When in doubt, run :doc:`fish_key_reader`. If that tells you that pressing :kbd:`ctrl-i` sends tab, your terminal does not support these better encodings, and so fish is limited to what it sends.

.. _cmd-bind-escape:

Key timeout
-----------

When you've bound a sequence of multiple characters, there is always the possibility that fish has only seen a part of it, and then it needs to disambiguate between the full sequence and part of it.

For example::

  bind j,k 'commandline -i foo'
  # or `bind jk`

will bind the sequence ``jk`` to insert "foo" into the commandline. When you've only pressed "j", fish doesn't know if it should insert the "j" (because of the default self-insert), or wait for the "k".

You can enable a timeout for this, by setting the :envvar:`fish_sequence_key_delay_ms` variable to the timeout in milliseconds. If the timeout elapses, fish will no longer wait for the sequence to be completed, and do what it can with the characters it already has.

The escape key is a special case, because it can be used standalone as a real key or as part of a longer escape sequence, like function or arrow keys. Holding alt and something else also typically sends escape, for example holding alt+a will send an escape character and then an "a". So the escape character has its own timeout configured with :envvar:`fish_escape_delay_ms`.

See also :ref:`Key sequences <interactive-key-sequences>`.
