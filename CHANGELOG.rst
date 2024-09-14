fish 3.8.0 (released ???)
===================================

.. ignore: 9439 9440 9442 9452 9469 9480 9482 9520 9536 9541 9544 9559 9561 9576 9575
   9568 9588 9556 9563 9567 9585 9591 9593 9594 9603 9600 9599 9592 9612 9613 9619 9630 9638 9625 9654 9637 9673 9666 9626 9688 9725 9636 9735
   9542 9332 9566 9573 9579 9586 9589 9607 9608 9615 9616 9621 9641 9642 9643 9653 9658 9661
   9671 9726 9729 9739 9745 9746 9754 9765 9767 9768 9771 9777 9778 9786 9816 9818 9821 9839
   9845 9864 9916 9923 9962 9963 9990 9991 10121 10179 9856 9859 9861 9863 9867 9869 9874 9879
   9881 9894 9896 9902 9873 10228 9925 9928 10145 10146 10161 10173 10195 10220 10222 10288
   10342 10358 9927 9930 9947 9948 9950 9952 9966 9968 9980 9981 9984 10040 10061 10101 10090
   10102 10108 10114 10115 10128 10129 10143 10174 10175 10180 10182 10184 10185 10186 10188
   10198 10200 10201 10204 10210 10214 10219 10223 10227 10232 10235 10237 10243 10244 10245
   10246 10251 10260 10267 10281 10347 10366 10368 10370 10371 10263 10270 10272 10276 10277
   10278 10279 10291 10293 10305 10306 10309 10316 10317 10327 10328 10329 10330 10336 10340
   10345 10346 10353 10354 10356 10372 10373 3299 10360 10359
   2037 2037 3017 3018 3162 4865 5284 5991 6981 6996 7172 9751 9893 10241 10254 10268 10290 10307
   10308 10321 10338 10348 10349 10355 10357 10379 10381 10388 10389 10390 10395 10398 10400 10403
   10404 10407 10408 10409 10411 10412 10417 10418 10427 10429 10438 10439 10440 10441 10442 10443
   10445 10448 10450 10451 10456 10457 10462 10463 10464 10466 10467 10474 10481 10490 10492 10494
   10499 10503 10505 10508 10509 10510 10511 10512 10513 10518 10547 10719

The entirety of fish's C++ code has been ported to Rust (:issue:`9512`).
This means a large change in dependencies and how to build fish.
Packagers should see the :ref:`For Distributors <rust-packaging>` section at the end.

Notable backwards-incompatible changes
--------------------------------------

- As part of a larger binding rework, ``bind`` gained a new key notation.
  In most cases the old notation should keep working, but in rare cases you may have to change a ``bind`` invocation to use the new notation.
  See :ref:`below <changelog-new-bindings>` for details.
- Fish no longer supports terminals that fail to ignore OSC or CSI sequences they don't recognize.
  The typical problem is that terminals echo the raw sequences sent by fish instead of silently ignoring them.
- ``random`` now uses a different random number generator and so the values you get even with the same seed have changed.
  Notably, it will now work much more sensibly with very small seeds.
  The seed was never guaranteed to give the same result across systems,
  so we do not expect this to have a large impact (:issue:`9593`).
- ``functions --handlers`` will now list handlers in a different order.
  Now it is definition order, first to last, where before it was last to first.
  This was never specifically defined, and we recommend not relying on a specific order (:issue:`9944`).
- The ``qmark-noglob`` feature flag, introduced in fish 3.0, is now turned on by default. That means ``?`` will no longer act as a single-character glob.
  You can, for the time being, turn it back on by adding ``no-qmark-noglob`` to :envvar:`fish_features` and restarting fish::

    set -Ua fish_features no-qmark-noglob

  The flag will eventually be made read-only, making it impossible to turn off.
- Fish no longer searches directories from the Windows system/user ``$PATH`` environment variable for Linux executables. To execute Linux binaries by name (i.e. not with a relative or absolute path) from a Windows folder, make sure the ``/mnt/c/...`` path is explicitly added to ``$fish_user_paths`` and not just automatically appended to ``$PATH`` by ``wsl.exe`` (:issue:`10506`).
- Under WSLv1, backgrounded jobs that have not been disowned and do not terminate on their own after a ``SIGHUP`` + ``SIGCONT`` sequence will be explicitly killed by fish on exit/exec (after the usual prompt to close or disown them) to work around a WSL deficiency that sees backgrounded processes that run into ``SIGTTOU`` remain in a suspended state indefinitely (:issue:`5263`). The workaround is to explicitly ``disown`` processes you wish to outlive the shell session.
- :kbd:`ctrl-c` no longer cancels builtin ``read``.


Notable improvements and fixes
------------------------------
.. _changelog-new-bindings:

-  fish asks terminals to speak keyboard protocols CSI u, XTerm's ``modifyOtherKeys`` and some progressive enhancements from the `kitty keyboard protocol <https://sw.kovidgoyal.net/kitty/keyboard-protocol/>`_.
   Depending on terminal support, this allows to bind a lot more key combinations, including arbitrary combinations of modifiers :kbd:`ctrl`, :kbd:`alt` and :kbd:`shift`,
   and to distinguish e.g. :kbd:`ctrl-i` from :kbd:`tab`.

   Additionally, builtin ``bind`` no longer requires specifying keys as byte sequences but learned a human-readable syntax.
   This includes modifier names, and names for keys like :kbd:`enter` and :kbd:`backspace`.
   For example

   - ``bind up 'do something'`` binds the up-arrow key instead of a two-key sequence ("u" and then "p")
   - ``bind ctrl-x,alt-c 'do something'`` binds a sequence of two keys.

   Any key argument that starts with an ASCII control character (like ``\e`` or ``\cX``) or is up to 3 characters long, not a named key, and does not contain ``,`` or ``-`` will be interpreted in the old syntax to keep compatibility for the majority of bindings.
- A new function ``fish_should_add_to_history`` can be overridden to decide whether a command should be added to the history (:issue:`10302`).
- :kbd:`ctrl-c` during command input no longer prints ``^C`` and a new prompt but merely clears the command line. This restores the behavior from version 2.2. To revert to the old behavior use ``bind ctrl-c __fish_cancel_commandline`` (:issue:`10213`).
- Bindings can now mix special input functions and shell commands, so ``bind ctrl-g expand-abbr "commandline -i \n"`` works as expected (:issue:`8186`).
- Special input functions run from bindings via ``commandline -f`` are now applied immediately instead of after the currently executing binding.
  For example, ``commandline -i foo; commandline | grep foo`` succeeds now.
- Undo history is no longer truncated after every command but kept for the lifetime of the shell process.
- The :kbd:`ctrl-r` history search now uses glob syntax (:issue:`10131`).
- The :kbd:`ctrl-r` history search now operates only on the line or command substitution at cursor, making it easier to combine commands from history.
- Abbreviations can now be restricted to specific commands. For instance::

    abbr --add --command git back 'reset --hard HEAD^'

  will expand "back" to ``reset --hard HEAD^``, but only when the command is ``git`` (:issue:`9411`, :issue:`10452`).

Deprecations and removed features
---------------------------------

- ``commandline --tokenize`` (short option ``-o``) has been deprecated in favor of ``commandline --tokens-expanded`` (short option ``-x``) which expands variables and other shell expressions, removing the need to use "eval" in completion scripts (:issue:`10212`).
- Two new feature flags:

  - ``remove-percent-self`` (see ``status features``) disables PID expansion of ``%self`` which has been supplanted by ``$fish_pid`` (:issue:`10262`).
  - ``test-require-arg``, will disable ``test``'s one-argument mode. That means ``test -n`` without an additional argument will return false, ``test -z`` will keep returning true. Any other option without an argument, anything that is not an option and no argument will be an error. This also goes for ``[``, test's alternate name.
    This is a frequent source of confusion and so we are breaking with POSIX explicitly in this regard.
    In addition to the feature flag, there is a debug category "deprecated-test". Running fish with ``fish -d deprecated-test`` will show warnings whenever a ``test`` invocation that would change is used. (:issue:`10365`).

  as always these can be enabled with::

    set -Ua fish_features remove-percent-self test-require-arg

  They are available as a preview now, it is our intention to enable them by default in future, and after that eventually make them read-only.
- Specifying key names as terminfo name (``bind -k``) is deprecated and may be removed in a future version.
- Flow control -- which, if enabled by ``stty ixon ixoff``, allows to pause terminal input with :kbd:`ctrl-s` and resume it with :kbd:`ctrl-q` -- now works only while fish is executing an external command.
- When a terminal pastes text into fish using bracketed paste, fish used to switch to a special ``paste`` bind mode.
  This bind mode has been removed. The behavior on paste is currently not meant to be configurable.
- When an interactive fish is stopped or terminated by a signal that cannot be caught (SIGSTOP or SIGKILL), it may leave the terminal in a state where keypresses with modifiers are sent as CSI u sequences instead of traditional control characters or escape sequecnes (that are recognized by bash/readline).
  If this happens, you can use the ``reset`` command from ``ncurses`` to restore the terminal state.
- ``fish_key_reader --verbose`` no longer shows timing information.
  Raw byte values should no longer be necessary because fish now decodes them to the new human-readable key names for builtin bind.
- Instant propagation of universal variables now only works on Linux and macOS. On other platforms, changes to universal variables may only become visible on the next prompt.

Scripting improvements
----------------------
- for-loops will no longer remember local variables from the previous iteration (:issue:`10525`).
- Add ``history append`` subcommand to append a command to the history without executing it (:issue:`4506`).
- A new redirection: ``<? /path/to/file`` will try opening the file as input, and if it doesn't succeed silently use /dev/null instead.
  This can help with checks like ``test -f /path/to/file; and string replace foo bar < /path/to/file``. (:issue:`10387`)
- New option ``commandline --tokens-raw`` prints a list of tokens without any unescaping (:issue:`10212`).
- ``functions`` and ``type`` now show where a function was copied and where it originally was instead of saying ``Defined interactively`` (:issue:`6575`).
- Stack trace now shows line numbers for copied functions.
- ``foo & && bar`` is now a syntax error, like in other shells (:issue:`9911`).
- ``if -e foo; end`` now prints a more accurate error (:issue:`10000`).
- Variables in command position that expand to a subcommand keyword are now forbidden to fix a likely user error.
  For example ``set editor command emacs; $editor`` is no longer allowed (:issue:`10249`).
- ``cd`` into a directory that is not readable but accessible (permissions ``--x``) is now possible (:issue:`10432`).
- An integer overflow in ``string repeat`` leading to a near-infinite loop has been fixed (:issue:`9899`).
- ``string shorten`` behaves better in the presence of non-printable characters, including fixing an integer overflow that shortened strings more than intended. (:issue:`9854`)
- ``string pad`` no longer allows non-printable characters as padding. (:issue:`9854`)
- ``string repeat`` now allows omission of ``-n`` when the first argument is an integer. (:issue:`10282`)
- ``functions --handlers-type caller-exit`` once again lists functions defined as ``function --on-job-exit caller``, rather than them being listed by ``functions --handlers-type process-exit``.
- ``set`` has a new ``--no-event`` flag, to set or erase variables without triggering a variable event. This is useful e.g. to change a variable in an event handler. (:issue:`10480`)
- Commas in command substitution output are no longer used as separators in brace expansion, preventing a surprising expansion in rare cases (:issue:`5048`).
- Universal variables can now store strings containing invalid Unicode codepoints (:issue:`10313`).
- ``path basename`` now takes a ``-E`` option that causes it to return the basename (i.e. "filename" with the directory prefix removed) with the final extension (if any) also removed. This takes the place of ``path change-extension "" (path basename $foo)`` (:issue:`10521`).
- ``math`` now adds ``--scale-mode`` parameter. You can choose between ``truncate``, ``round``, ``floor``, ``ceiling`` as you wish (default value is ``truncate``). (:issue:`9117`).

Interactive improvements
------------------------
- When using :kbd:`ctrl-x` on Wayland in the VSCode terminal, the clipboard is no longer cleared on :kbd:`ctrl-c`.
- Command-specific tab completions may now offer results whose first character is a period. For example, it is now possible to tab-complete ``git add`` for files with leading periods. The default file completions hide these files, unless the token itself has a leading period (:issue:`3707`).
- Option completion now uses fuzzy subsequence filtering, just like non-option completion (:issue:`830`).
  This means that ``--fb`` may be completed to ``--foobar`` if there is no better match.
- Completions that insert an entire token now use quotes instead of backslashes to escape special characters (:issue:`5433`).
- Historically, file name completions are provided after the last ``:``  or ``=`` within a token.
  This helps commands like ``rsync --files-from=``.
  If the ``=`` or ``:`` is actually part of the filename, it will be escaped as ``\:`` and ``\=``,
  and no longer get this special treatment.
  This matches Bash's behavior.
- Autosuggestions were sometimes not shown after recalling a line from history, which has been fixed (:issue:`10287`).
- Up-arrow search matches -- which are highlighted in reverse video -- are no longer syntax-highlighted, to fix bad contrast with the search match highlighting.
- Command abbreviations (those with ``--position command`` or without a ``--position``) now also expand after decorators like ``command`` (:issue:`10396`).
- Abbreviations now expand after process separators like ``;`` and ``|``. This fixes a regression in version 3.6 (:issue:`9730`).
- When exporting interactively defined functions (using ``type``, ``functions`` or ``funcsave``) the function body is now indented, same as in the interactive command line editor (:issue:`8603`).
- :kbd:`ctrl-x` (``fish_clipboard_copy``) on multiline commands now includes indentation (:issue:`10437`).
- :kbd:`ctrl-v` (``fish_clipboard_paste``) now strips ASCII control characters from the pasted text.
  This is consistent with normal keyboard input (:issue:`5274`).
- When a command like ``fg %2`` fails to find the given job, it no longer behaves as if no job spec was given (:issue:`9835`).
- Redirection in command position like ``>echo`` is now highlighted as error (:issue:`8877`).
- ``fish_vi_cursor`` now works properly inside the prompt created by builtin ``read`` (:issue:`10088`).
- fish no longer fails to open a fifo if interrupted by a terminal resize signal (:issue:`10250`).
- ``read --help`` and friends no longer ignore redirections. This fixes a regression in version 3.1 (:issue:`10274`).
- Measuring a command with ``time`` now considers the time taken for command substitution (:issue:`9100`).
- ``fish_add_path`` now automatically enables verbose mode when used interactively (in the commandline), in an effort to be clearer about what it does (:issue:`10532`).
- fish no longer adopts TTY modes of failed commands (:issue:`10603`).
- `complete -e cmd` now prevents autoloading completions for `cmd` (:issue:`6716`).

New or improved bindings
^^^^^^^^^^^^^^^^^^^^^^^^
- When the cursor is on a command that resolves to an executable script, :kbd:`alt-o` will now open that script in your editor (:issue:`10266`).
- During up-arrow history search, :kbd:`shift-delete` will delete the current search item and move to the next older item. Previously this was only supported in the history pager.
  Same for autosuggestions.
- :kbd:`ctrl-Z` (also known as :kbd:`ctrl-shift-z`) is now bound to redo.
- Some improvements to the :kbd:`alt-e` binding which edits the commandline in an external editor:
  - The editor's cursor position is copied back to fish. This is currently supported for Vim and Kakoune.
  - Cursor position synchronization is only supported for a set of known editors. This has been extended by also resolving aliases. For example use ``complete --wraps my-vim vim`` to synchronize cursors when ``EDITOR=my-vim``.
  - Multiline commands are indented before being sent to the editor, which matches how they are displayed in fish.
- The ``*-path-component`` bindings like ``backward-kill-path-component`` now treat ``#`` as part of a path component (:issue:`10271`).
- Bindings like :kbd:`alt-l` that print output in between prompts now work correctly with multiline commandlines.
- :kbd:`alt-d` on an empty command line lists the directory history again. This restores the behavior of version 2.1.
- ``history-prefix-search-{backward,forward}`` now maintain the cursor position instead of moving the cursor to the end of the command line (:issue:`10430`).
- The :kbd:`E` binding in vi mode now correctly handles the last character of the word, by jumping to the next word (:issue:`9700`).
- If the terminal supports shifted key codes from the `kitty keyboard protocol <https://sw.kovidgoyal.net/kitty/keyboard-protocol/>`_, :kbd:`shift-enter` now inserts a newline instead of executing the command line.
- New special input functions ``forward-char-passive`` and ``backward-char-passive`` are like their non-passive variants but do not accept autosuggestions or move focus in the completion pager (:issue:`10398`).
- Vi mode has seen some improvements but continues to suffer from the lack of people working on it.
  - Insert-mode :kbd:`ctrl-n` accepts autosuggestions (:issue:`10339`).
  - Outside insert mode, the cursor will no longer be placed beyond the last character on the commandline.
  - When the cursor is at the end of the commandline, a single :kbd:`l` will accept an autosuggestion (:issue:`10286`).
  - The cursor position after pasting (:kbd:`p`) has been corrected.
  - When the cursor is at the start of a line, escaping from insert mode no longer moves the cursor to the previous line.
  - Added bindings for clipboard interaction, like :kbd:`",+,p` and :kbd:`",+,y,y`.
  - Deleting in visual mode now moves the cursor back, matching vi (:issue:`10394`).
  - Support :kbd:`%` motion.
  - Support `ab` and `ib` vi text objects. New input functions are introduced ``jump-{to,till}-matching-bracket`` (:issue:`1842`).

Completions
^^^^^^^^^^^
- Various new completion scripts and numerous updates to existing ones.
- Generated completions are now stored in ``$XDG_CACHE_HOME/fish`` or ``~/.cache/fish`` by default (:issue:`10369`)

Improved terminal support
^^^^^^^^^^^^^^^^^^^^^^^^^
- Fish now marks the prompt and command-output regions (via OSC 133) to enable terminal shell integration (:issue:`10352`).
  Shell integration shortcuts can scroll to the next/previous prompt or show the last command output in a pager.
- Fish now reports the working directory (via OSC 7) unconditionally instead of only for some terminals (:issue:`9955`).
- Fish now sets the terminal window title (via OSC 0) unconditionally instead of only for some terminals (:issue:`10037`).
- Focus reporting in tmux is no longer disabled on the first prompt.
- Focus reporting is now disabled during execution of bind commands (:issue:`6942`).
- ``fish_vi_cursor`` no longer attempts to detect if the terminal is capable, as we can no longer find terminals that aren't and the allowlist was hard to keep up-to-date. (:issue:`10693`)

Other improvements
------------------
- ``fish_indent`` will now collapse multiple successive empty lines into one (:issue:`10325`).
- ``fish_indent`` now preserves the modification time of files if there were no changes (:issue:`10624`).
- The HTML-based configuration UI (``fish_config``) now uses Alpine.js instead of AngularJS (:issue:`9554`).
- ``fish_config`` now also works in a Windows MSYS environment (:issue:`10111`).
- Performance and interactivity under WSLv1 and WSLv2 has been improved with a workaround for Windows-specific locations being appended to ``$PATH`` by default (:issue:`10506`).

.. _rust-packaging:

For distributors
----------------

Fish has been ported to Rust. That means the dependencies have changed.

It now requires Rust 1.70 at least.

CMake remains for now because cargo is unable to install the many asset files that fish needs. The minimum required CMake version has increased to 3.19.

Some smaller changes:

- The default build configuration has changed to "Debug".
  Please pass ``-DCMAKE_BUILD_TYPE=Release`` if you want to build a package.
- Xcode support has been removed (:issue:`9924`).
- fish no longer links against the (n)curses library, opting to read the terminfo database via the terminfo crate.
  This means hashed terminfo databases are no longer supported (from our research, they are basically unmaintained and unused).
  When packaging fish, please add a dependency on the package containing your terminfo database instead of curses,
  if such a package is required.
  If it cannot find a terminfo database, fish will now fall back on an included ``xterm-256color`` definition (:issue:`10269`).

--------------

fish 3.7.1 (released March 19, 2024)
====================================

This release of fish fixes the following problems identified in fish 3.7.0:

- Deleting the last history entry via ``history delete`` works again (:issue:`10190`).
- Wildcards (``*``) will no longer sometimes generate paths that did not exist (:issue:`10205`).

This release also contains some improvements:

- A crash when trying to run an ELF program with a missing interpreter has been fixed. This crashed in the process after fork, so did not affect the fish process that tried to start the program (:issue:`10199`).
- ``funced`` will now always ``source`` the file after it has written it, even if the contents did not change. This prevents issues if the file was otherwise modified (:issue:`10318`).
- The warning for when a builtin returns a negative exit code was improved, now mentioning the original status (:issue:`10187`).
- Added completions for

  - ``cobra-cli`` (:issue:`10293`)
  - ``dmidecode`` (:issue:`10368`)
  - ``mycli`` (:issue:`10309`)
  - ``ollama`` (:issue:`10327`)
  - ``pstree`` (:issue:`10317`)

- Some improvements to documentation and completions.

--------------

fish 3.7.0 (released January 1, 2024)
=====================================

This release of fish includes a number of improvements over fish 3.6.4, detailed below. Although work continues on the porting of fish internals to the Rust programming language, that work is not included in this release. fish 3.7.0 and any future releases in the 3.7 series remain C++ programs.

Notable improvements and fixes
------------------------------
- Improvements to the history pager, including:

  - The history pager will now also attempt subsequence matches (:issue:`9476`), so you can find a command line like ``git log 3.6.1..Integration_3.7.0`` by searching for ``gitInt``.
  - Opening the history pager will now fill the search field with a search string if you're already in a search (:issue:`10005`). This makes it nicer to search something with :kbd:`up` and then later decide to switch to the full pager.
  - Closing the history pager with enter will now copy the search text to the commandline if there was no match, so you can continue editing the command you tried to find right away (:issue:`9934`).

- Performance improvements for command completions and globbing, where supported by the operating system, especially on slow filesystems such as NFS (:issue:`9891`, :issue:`9931`, :issue:`10032`, :issue:`10052`).
- fish can now be configured to wait a specified amount of time for a multi-key sequence to be completed,  instead of waiting indefinitely. For example, this makes binding ``kj`` to switching modes in vi mode possible.
  The timeout can be set via the new :envvar:`fish_sequence_key_delay_ms` variable (:issue:`7401`), and may be set by default in future versions.

Deprecations and removed features
---------------------------------
- ``LS_COLORS`` is no longer set automatically by the ``ls`` function (:issue:`10080`). Users
  that set ``.dircolors`` should manually import it using other means. Typically this would be ``set -gx LS_COLORS (dircolors -c .dircolors | string split ' ')[3]``

Scripting improvements
----------------------
- Running ``exit`` with a negative number no longer crashes fish (:issue:`9659`).
- ``fish --command`` will now return a non-zero status if parsing failed (:issue:`9888`).
- The ``jobs`` builtin will now escape the commands it prints (:issue:`9808`).
- ``string repeat`` no longer overflows if the count is a multiple of the chunk size (:issue:`9900`).
- The ``builtin`` builtin will now properly error out with invalid arguments instead of doing nothing and returning true (:issue:`9942`).
- ``command time`` in a pipeline is allowed again, as is ``command and`` and ``command or`` (:issue:`9985`).
- ``exec`` will now also apply variable overrides, so ``FOO=bar exec`` will now set ``$FOO`` correctly (:issue:`9995`).
- ``umask`` will now handle empty symbolic modes correctly, like ``umask u=,g=rwx,o=`` (:issue:`10177`).
- Improved error messages for errors occurring in command substitutions (:issue:`10054`).

Interactive improvements
------------------------
- ``read`` no longer enables bracketed paste so it doesn't stay enabled in combined commandlines like ``mysql -p(read --silent)`` (:issue:`8285`).
- Vi mode now uses :envvar:`fish_cursor_external` to set the cursor shape for external commands (:issue:`4656`).
- Opening the history search in vi mode switches to insert mode correctly (:issue:`10141`).
- Vi mode cursor shaping is now enabled in iTerm2 (:issue:`9698`).
- Completing commands as root includes commands not owned by root, fixing a regression introduced in fish 3.2.0 (:issue:`9699`).
- Selection uses ``fish_color_selection`` for the foreground and background colors, as intended, rather than just the background (:issue:`9717`).
- The completion pager will no longer sometimes skip the last entry when moving through a long list (:issue:`9833`).
- The interactive ``history delete`` interface now allows specifying index ranges like "1..5" (:issue:`9736`), and ``history delete --exact`` now properly saves the history (:issue:`10066`).
- Command completion will now call the stock ``manpath`` on macOS, instead of a potential Homebrew version. This prevents awkward error messages (:issue:`9817`).
- the ``redo`` special input function restores the pre-undo cursor position.
- A new bind function ``history-pager-delete``, bound to :kbd:`shift-delete` by default, will delete the currently-selected history pager item from history (:issue:`9454`).
- ``fish_key_reader`` will now use printable characters as-is, so pressing "ö" no longer leads to it telling you to bind ``\u00F6`` (:issue:`9986`).
- ``open`` can be used to launch terminal programs again, as an ``xdg-open`` bug has been fixed and a workaround has been removed  (:issue:`10045`).
- The ``repaint-mode`` binding will now only move the cursor if there is repainting to be done. This fixes :kbd:`alt` combination bindings in vi mode (:issue:`7910`).
- A new ``clear-screen`` bind function is used for :kbd:`ctrl-l` by default. This clears the screen and repaints the existing prompt at first,
  so it eliminates visible flicker unless the terminal is very slow (:issue:`10044`).
- The ``alias`` convenience function has better support for commands with unusual characters, like ``+`` (:issue:`8720`).
- A longstanding issue where items in the pager would sometimes display without proper formatting has been fixed (:issue:`9617`).
- The :kbd:`alt-l` binding, which lists the directory of the token under the cursor, correctly expands tilde (``~``) to the home directory (:issue:`9954`).
- Various fish utilities that use an external pager will now try a selection of common pagers if the :envvar:`PAGER` environment variable is not set, or write the output to the screen without a pager if there is not one available (:issue:`10074`).
- Command-specific tab completions may now offer results whose first character is a period. For example, it is now possible to tab-complete ``git add`` for files with leading periods. The default file completions hide these files, unless the token itself has a leading period (:issue:`3707`).

Improved prompts
^^^^^^^^^^^^^^^^
- The default theme now only uses named colors, so it will track the terminal's palette (:issue:`9913`).
- The Dracula theme has now been synced with upstream (:issue:`9807`); use ``fish_config`` to re-apply it to pick up the changes.
- ``fish_vcs_prompt`` now also supports fossil (:issue:`9497`).
- Prompts which display the working directory using the ``prompt_pwd`` function correctly display directories beginning with dashes (:issue:`10169`).

Completions
^^^^^^^^^^^
- Added completions for:

  - ``age`` and ``age-keygen`` (:issue:`9813`)
  - ``airmon-ng`` (:issue:`10116`)
  - ``ar`` (:issue:`9720`)
  - ``blender`` (:issue:`9905`)
  - ``bws`` (:issue:`10165`)
  - ``calendar`` (:issue:`10138`)
  - ``checkinstall`` (:issue:`10106`)
  - ``crc`` (:issue:`10034`)
  - ``doctl``
  - ``gimp`` (:issue:`9904`)
  - ``gojq`` (:issue:`9740`)
  - ``horcrux`` (:issue:`9922`)
  - ``ibmcloud`` (:issue:`10004`)
  - ``iwctl`` (:issue:`6884`)
  - ``java_home`` (:issue:`9998`)
  - ``krita`` (:issue:`9903`)
  - ``oc`` (:issue:`10034`)
  - ``qjs`` (:issue:`9723`)
  - ``qjsc`` (:issue:`9731`)
  - ``rename`` (:issue:`10136`)
  - ``rpm-ostool`` (:issue:`9669`)
  - ``smerge`` (:issue:`10135`)
  - ``userdel`` (:issue:`10056`)
  - ``watchexec`` (:issue:`10027`)
  - ``wpctl`` (:issue:`10043`)
  - ``xxd`` (:issue:`10137`)
  - ``zabbix`` (:issue:`9647`)

- The ``zfs`` completions no longer print errors about setting a read-only variable (:issue:`9705`).
- The ``kitty`` completions have been removed in favor of keeping them upstream (:issue:`9750`).
- ``git`` completions now support aliases that reference other aliases (:issue:`9992`).
- The ``gw`` and ``gradlew`` completions are loaded properly (:issue:`10127`).
- Improvements to many other completions.
- Improvements to the manual page completion generator (:issue:`9787`, :issue:`9814`, :issue:`9961`).

Other improvements
------------------
- Improvements and corrections to the documentation.
- The Web-based configuration now uses a more readable style when printed, such as for a keybinding reference (:issue:`9828`).
- Updates to the German translations (:issue:`9824`).
- The colors of the Nord theme better match their official style (:issue:`10168`).

For distributors
----------------
- The licensing information for some of the derived code distributed with fish was incomplete. Though the license information was present in the source distribution, it was not present in the documentation. This has been corrected (:issue:`10162`).
- The CMake configure step will now also look for libterminfo as an alternative name for libtinfo, as used in NetBSD curses (:issue:`9794`).

----

fish 3.6.4 (released December 5, 2023)
======================================

This release contains a complete fix for the test suite failure in fish 3.6.2 and 3.6.3.

--------------

fish 3.6.3 (released December 4, 2023)
======================================

This release contains a fix for a test suite failure in fish 3.6.2.

--------------

fish 3.6.2 (released December 4, 2023)
======================================

This release of fish contains a security fix for CVE-2023-49284, a minor security problem identified
in fish 3.6.1 and previous versions (thought to affect all released versions of fish).

fish uses certain Unicode non-characters internally for marking wildcards and expansions. It
incorrectly allowed these markers to be read on command substitution output, rather than
transforming them into a safe internal representation.

For example, ``echo \UFDD2HOME`` has the same output as ``echo $HOME``.

While this may cause unexpected behavior with direct input, this may become a minor security problem
if the output is being fed from an external program into a command substitution where this output
may not be expected.

--------------

fish 3.6.1 (released March 25, 2023)
====================================

This release of fish contains a number of fixes for problems identified in fish 3.6.1, as well as some enhancements.

Notable improvements and fixes
------------------------------
- ``abbr --erase`` now also erases the universal variables used by the old abbr function. That means::

    abbr --erase (abbr --list)

  can now be used to clean out all old abbreviations (:issue:`9468`).
- ``abbr --add --universal`` now warns about ``--universal`` being non-functional, to make it easier to detect old-style ``abbr`` calls (:issue:`9475`).

Deprecations and removed features
---------------------------------
- The Web-based configuration for abbreviations has been removed, as it was not functional with the changes abbreviations introduced in 3.6.0 (:issue:`9460`).

Scripting improvements
----------------------
- ``abbr --list`` no longer escapes the abbr name, which is necessary to be able to pass it to ``abbr --erase`` (:issue:`9470`).
- ``read`` will now print an error if told to set a read-only variable, instead of silently doing nothing (:issue:`9346`).
- ``set_color -v`` no longer crashes fish (:issue:`9640`).

Interactive improvements
------------------------
- Using ``fish_vi_key_bindings`` in combination with fish's ``--no-config`` mode works without locking up the shell (:issue:`9443`).
- The history pager now uses more screen space, usually half the screen (:issue:`9458`)
- Variables that were set while the locale was C (the default ASCII-only locale) will now properly be encoded if the locale is switched (:issue:`2613`, :issue:`9473`).
- Escape during history search restores the original command line again (fixing a regression in 3.6.0).
- Using ``--help`` on builtins now respects the ``$MANPAGER`` variable, in preference to ``$PAGER`` (:issue:`9488`).
- :kbd:`ctrl-g` closes the history pager, like other shells (:issue:`9484`).
- The documentation for the ``:``, ``[`` and ``.`` builtin commands can now be looked up with ``man`` (:issue:`9552`).
- fish no longer crashes when searching history for non-ASCII codepoints case-insensitively (:issue:`9628`).
- The :kbd:`alt-s` binding will now also use ``please`` if available (:issue:`9635`).
- Themes that don't specify every color option can be installed correctly in the Web-based configuration (:issue:`9590`).
- Compatibility with Midnight Commander's prompt integration has been improved (:issue:`9540`).
- A spurious error, noted when using fish in Google Drive directories under WSL 2, has been silenced (:issue:`9550`).
- Using ``read`` in ``fish_greeting`` or similar functions will not trigger an infinite loop (:issue:`9564`).
- Compatibility when upgrading from old versions of fish (before 3.4.0) has been improved (:issue:`9569`).

Improved prompts
^^^^^^^^^^^^^^^^
- The git prompt will compute the stash count to be used independently of the informative status (:issue:`9572`).

Completions
^^^^^^^^^^^
- Added completions for:

  - ``apkanalyzer`` (:issue:`9558`)
  - ``neovim`` (:issue:`9543`)
  - ``otool``
  - ``pre-commit`` (:issue:`9521`)
  - ``proxychains`` (:issue:`9486`)
  - ``scrypt`` (:issue:`9583`)
  - ``stow`` (:issue:`9571`)
  - ``trash`` and helper utilities ``trash-empty``, ``trash-list``, ``trash-put``, ``trash-restore`` (:issue:`9560`)
  - ``ssh-copy-id`` (:issue:`9675`)

- Improvements to many completions, including the speed of completing directories in WSL 2 (:issue:`9574`).
- Completions using ``__fish_complete_suffix`` are now offered in the correct order, fixing a regression in 3.6.0 (:issue:`8924`).
- ``git`` completions for ``git-foo``-style commands was restored, fixing a regression in 3.6.0 (:issue:`9457`).
- File completion now offers ``../`` and ``./`` again, fixing a regression in 3.6.0 (:issue:`9477`).
- The behaviour of completions using ``__fish_complete_path`` matches standard path completions (:issue:`9285`).

Other improvements
------------------
- Improvements and corrections to the documentation.

For distributors
----------------
- fish 3.6.1 builds correctly on Cygwin (:issue:`9502`).

--------------

fish 3.6.0 (released January 7, 2023)
=====================================

Notable improvements and fixes
------------------------------
- By default, :kbd:`ctrl-r` now opens the command history in the pager (:issue:`602`). This is fully searchable and syntax-highlighted, as an alternative to the incremental search seen in other shells. The new special input function ``history-pager`` has been added for custom bindings.
- Abbrevations are more flexible (:issue:`9313`, :issue:`5003`, :issue:`2287`):

  - They may optionally replace tokens anywhere on the command line, instead of only commands
  - Matching tokens may be described using a regular expression instead of a literal word
  - The replacement text may be produced by a fish function, instead of a literal word
  - They may position the cursor anywhere in the expansion, instead of at the end

  For example::

    function multicd
        echo cd (string repeat -n (math (string length -- $argv[1]) - 1) ../)
    end

    abbr --add dotdot --regex '^\.\.+$' --function multicd

  This expands ``..`` to ``cd ../``, ``...`` to ``cd ../../`` and ``....`` to ``cd ../../../`` and so on.

  Or::

    function last_history_item; echo $history[1]; end
    abbr -a !! --position anywhere --function last_history_item

  which expands ``!!`` to the last history item, anywhere on the command line, mimicking other shells' history expansion.

  See :ref:`the documentation <cmd-abbr>` for more.
- ``path`` gained a new ``mtime`` subcommand to print the modification time stamp for files. For example, this can be used to handle cache file ages (:issue:`9057`)::

    > touch foo
    > sleep 10
    > path mtime --relative foo
    10

- ``string`` gained a new ``shorten`` subcommand to shorten strings to a given visible width (:issue:`9156`)::

    > string shorten --max 10 "Hello this is a long string"
    Hello thi…

- ``test`` (aka ``[``) gained ``-ot`` (older than) and ``-nt`` (newer than) operators to compare file modification times, and ``-ef`` to compare whether the arguments are the same file (:issue:`3589`).
- fish will now mark the extent of many errors with a squiggly line, instead of just a caret (``^``) at the beginning (:issue:`9130`). For example::

    checks/set.fish (line 471): for: a,b: invalid variable name. See `help identifiers`
    for a,b in y 1 z 3
        ^~^
- A new function, ``fish_delta``, shows changes that have been made in fish's configuration from the defaults (:issue:`9255`).
- ``set --erase`` can now be used with multiple scopes at once, like ``set -efglU foo`` (:issue:`7711`, :issue:`9280`).
- ``status`` gained a new subcommand, ``current-commandline``, which retrieves the entirety of the currently-executing command line when called from a function during execution. This allows easier job introspection (:issue:`8905`, :issue:`9296`).

Deprecations and removed features
---------------------------------
- The ``\x`` and ``\X`` escape syntax is now equivalent. ``\xAB`` previously behaved the same as ``\XAB``, except that it would error if the value "AB" was larger than "7f" (127 in decimal, the highest ASCII value) (:issue:`9247`, :issue:`9245`, :issue:`1352`).
- The ``fish_git_prompt`` will now only turn on features if the appropriate variable has been set to a true value (of "1", "yes" or "true") instead of just checking if it is defined. This allows specifically turning features *off* without having to erase variables, such as via universal variables. If you have defined a variable to a different value and expect it to count as true, you need to change it (:issue:`9274`).
  For example, ``set -g __fish_git_prompt_show_informative_status 0`` previously would have enabled informative status (because any value would have done so), but now it turns it off.
- Abbreviations are no longer stored in universal variables. Existing universal abbreviations are still imported, but new abbreviations should be added to ``config.fish``.
- The short option ``-r`` for abbreviations has changed from ``rename`` to ``regex``, for consistency with ``string``.

Scripting improvements
----------------------
- ``argparse`` can now be used without option specifications, to allow using ``--min-args``, ``--max-args`` or for commands that take no options (but might in future) (:issue:`9006`)::

    function my_copy
        argparse --min-args 2 -- $argv
        or return

        cp $argv
    end

- ``set --show`` now shows when a variable was inherited from fish's parent process, which should help with debugging (:issue:`9029`)::

    > set --show XDG_DATA_DIRS
    $XDG_DATA_DIRS: set in global scope, exported, a path variable with 4 elements
    $XDG_DATA_DIRS[1]: |/home/alfa/.local/share/flatpak/exports/share|
    $XDG_DATA_DIRS[2]: |/var/lib/flatpak/exports/share|
    $XDG_DATA_DIRS[3]: |/usr/local/share|
    $XDG_DATA_DIRS[4]: |/usr/share|
    $XDG_DATA_DIRS: originally inherited as |/home/alfa/.local/share/flatpak/exports/share:/var/lib/flatpak/exports/share:/usr/local/share/:/usr/share/|

- The read limit is now restored to the default when :envvar:`fish_read_limit` is unset (:issue:`9129`).
- ``math`` produces an error for division-by-zero, as well as augmenting some errors with their extent (:issue:`9190`). This changes behavior in some limited cases, such as::

    math min 1 / 0, 5

  which would previously print "5" (because in floating point division "1 / 0" yields infinite, and 5 is smaller than infinite) but will now return an error.
- ``fish_clipboard_copy`` and ``fish_clipboard_paste`` can now be used in pipes (:issue:`9271`)::

    git rev-list 3.5.1 | fish_clipboard_copy

    fish_clipboard_paste | string join + | math

- ``status fish-path`` returns a fully-normalised path, particularly noticeable on NetBSD (:issue:`9085`).

Interactive improvements
------------------------
- If the terminal definition for :envvar:`TERM` can't be found, fish now tries using the "xterm-256color" and "xterm" definitions before "ansi" and "dumb". As the majority of terminal emulators in common use are now more or less xterm-compatible (often even explicitly claiming the xterm-256color entry), this should often result in a fully or almost fully usable terminal (:issue:`9026`).
- A new variable, :envvar:`fish_cursor_selection_mode`, can be used to configure whether the command line selection includes the character under the cursor (``inclusive``) or not (``exclusive``). The new default is ``exclusive``; use ``set fish_cursor_selection_mode inclusive`` to get the previous behavior back (:issue:`7762`).
- fish's completion pager now fills half the terminal on first tab press instead of only 4 rows, which should make results visible more often and save key presses, without constantly snapping fish to the top of the terminal (:issue:`9105`, :issue:`2698`).
- The ``complete-and-search`` binding, used with :kbd:`shift-tab` by default, selects the first item in the results immediately (:issue:`9080`).
- ``bind`` output is now syntax-highlighted when used interacively.
- :kbd:`alt-h` (the default ``__fish_man_page`` binding) does a better job of showing the manual page of the command under cursor (:issue:`9020`).
- If :envvar:`fish_color_valid_path` contains an actual color instead of just modifiers, those will be used for valid paths even if the underlying color isn't "normal" (:issue:`9159`).
- The key combination for the QUIT terminal sequence, often :kbd:`ctrl-\\` (``\x1c``), can now be used as a binding (:issue:`9234`).
- fish's vi mode uses normal xterm-style sequences to signal cursor change, instead of using the iTerm's proprietary escape sequences. This allows for a blinking cursor and makes it work in complicated scenarios with nested terminals. (:issue:`3741`, :issue:`9172`)
- When running fish on a remote system (such as inside SSH or a container), :kbd:`ctrl-x` now copies to the local client system's clipboard if the terminal supports OSC 52.
- ``commandline`` gained two new options, ``--selection-start`` and ``--selection-end``, to set the start/end of the current selection (:issue:`9197`, :issue:`9215`).
- fish's builtins now handle keyboard interrupts (:kbd:`ctrl-c`) correctly (:issue:`9266`).

Completions
^^^^^^^^^^^
- Added completions for:

  - ``ark``
  - ``asciinema`` (:issue:`9257`)
  - ``clojure`` (:issue:`9272`)
  - ``csh``
  - ``direnv`` (:issue:`9268`)
  - ``dive`` (:issue:`9082`)
  - ``dolphin``
  - ``dua`` (:issue:`9277`)
  - ``efivar`` (:issue:`9318`)
  - ``eg``
  - ``es`` (:issue:`9388`)
  - ``firefox-developer-edition`` and ``firefox`` (:issue:`9090`)
  - ``fortune`` (:issue:`9177`)
  - ``kb``
  - ``kind`` (:issue:`9110`)
  - ``konsole``
  - ``ksh``
  - ``loadkeys`` (:issue:`9312`)
  - ``okular``
  - ``op`` (:issue:`9300`)
  - ``ouch`` (:issue:`9405`)
  - ``pix``
  - ``readelf`` (:issue:`8746`, :issue:`9386`)
  - ``qshell``
  - ``rc``
  - ``sad`` (:issue:`9145`)
  - ``tcsh``
  - ``toot``
  - ``tox`` (:issue:`9078`)
  - ``wish``
  - ``xed``
  - ``xonsh`` (:issue:`9389`)
  - ``xplayer``
  - ``xreader``
  - ``xviewer``
  - ``yash`` (:issue:`9391`)
  - ``zig`` (:issue:`9083`)

- Improvements to many completions, including making ``cd`` completion much faster (:issue:`9220`).
- Completion of tilde (``~``) works properly even when the file name contains an escaped character (:issue:`9073`).
- fish no longer loads completions if the command is used via a relative path and is not in :envvar:`PATH` (:issue:`9133`).
- fish no longer completes inside of comments (:issue:`9320`).

Improved terminal support
^^^^^^^^^^^^^^^^^^^^^^^^^
- Opening ``help`` on WSL now uses PowerShell to open the browser if available, removing some awkward UNC path errors (:issue:`9119`).

Other improvements
------------------
- The Web-based configuration tool now works on systems with IPv6 disabled (:issue:`3857`).
- Aliases can ignore arguments by ending them with ``#`` (:issue:`9199`).
-  ``string`` is now faster when reading large strings from stdin (:issue:`9139`).
- ``string repeat`` uses less memory and is faster. (:issue:`9124`)
- Builtins are much faster when writing to a pipe or file. (:issue:`9229`).
- Performance improvements to highlighting (:issue:`9180`) should make using fish more pleasant on slow systems.
- On 32-bit systems, globs like ``*`` will no longer fail to return some files, as large file support has been enabled.

Fixed bugs
----------
- The history search text for a token search is now highlighted correctly if the line contains multiple instances of that text (:issue:`9066`).
- ``process-exit`` and ``job-exit`` events are now generated for all background jobs, including those launched from event handlers (:issue:`9096`).
- A crash when completing a token that contained both a potential glob and a quoted variable expansion was fixed (:issue:`9137`).
- ``prompt_pwd`` no longer accidentally overwrites a global or universal ``$fish_prompt_pwd_full_dirs`` when called with the ``-d`` or ``--full-length-dirs`` option (:issue:`9123`).
- A bug which caused fish to freeze or exit after running a command which does not preserve the foreground process group was fixed (:issue:`9181`).
- The "Disco" sample prompt no longer prints an error in some working directories (:issue:`9164`). If you saved this prompt, you should run ``fish_config prompt save disco`` again.
- fish launches external commands via the given path again, rather than always using an absolute path. This behaviour was inadvertently changed in 3.5.0 and is visible, for example, when launching a bash script which checks ``$0`` (:issue:`9143`).
- ``printf`` no longer tries to interpret the first argument as an option (:issue:`9132`).
- Interactive ``read`` in scripts will now have the correct keybindings again (:issue:`9227`).
- A possible stack overflow when recursively evaluating substitutions has been fixed (:issue:`9302`).
- A crash with relative $CDPATH has been fixed (:issue:`9407`).
- ``printf`` now properly fills extra ``%d`` specifiers with 0 even on macOS and BSD (:issue:`9321`).
- ``fish_key_reader`` now correctly exits when receiving a SIGHUP (like after closing the terminal) (:issue:`9309`).
- ``fish_config theme save`` now works as documented instead of erroring out (:issue:`9088`, :issue:`9273`).
- fish no longer triggers prompts to install command line tools when first run on macOS (:issue:`9343`).
- ``fish_git_prompt`` now quietly fails on macOS if the xcrun cache is not yet populated (:issue:`6625`), working around a potential hang.

For distributors
----------------
- The vendored PCRE2 sources have been removed. It is recommended to declare PCRE2 as a dependency when packaging fish. If the CMake variable FISH_USE_SYSTEM_PCRE2 is false, fish will now download and build PCRE2 from the official repo (:issue:`8355`, :issue:`8363`). Note this variable defaults to true if PCRE2 is found installed on the system.

--------------

fish 3.5.1 (released July 20, 2022)
===================================

This release of fish introduces the following small enhancements:

- Cursor shaping for Vi mode is enabled by default in tmux, and will be used if the outer terminal is capable (:issue:`8981`).
- ``printf`` returns a better error when used with arguments interpreted as octal numbers (:issue:`9035`).
- ``history merge`` when in private mode is now an error, rather than wiping out other sessions' history (:issue:`9050`).
- The error message when launching a command that is built for the wrong architecture on macOS is more helpful (:issue:`9052`).
- Added completions for:

  - ``choose`` (:issue:`9065`)
  - ``expect`` (:issue:`9060`)
  - ``navi`` (:issue:`9064`)
  - ``qdbus`` (:issue:`9031`)
  - ``reflector`` (:issue:`9027`)

- Improvements to some completions.

This release also fixes a number of problems identified in fish 3.5.0.

- Completing ``git blame`` or ``git -C`` works correctly (:issue:`9053`).
- On terminals that emit a ``CSI u`` sequence for :kbd:`shift-space`, fish inserts a space instead of printing an error. (:issue:`9054`).
- ``status fish-path`` on Linux-based platforms could print the path with a " (deleted)" suffix (such as ``/usr/bin/fish (deleted)``), which is now removed (:issue:`9019`).
- Cancelling an initial command (from fish's ``--init-command`` option) with :kbd:`ctrl-c` no longer prevents configuration scripts from running (:issue:`9024`).
- The job summary contained extra blank lines if the prompt used multiple lines, which is now fixed (:issue:`9044`).
- Using special input functions in bindings, in combination with ``and``/``or`` conditionals, no longer crashes (:issue:`9051`).

--------------

fish 3.5.0 (released June 16, 2022)
===================================

Notable improvements and fixes
------------------------------
- A new ``path`` builtin command to filter and transform paths (:issue:`7659`, :issue:`8958`). For example, to list all the separate extensions used on files in /usr/share/man (after removing one extension, commonly a ".gz")::

    path filter -f /usr/share/man/** | path change-extension '' | path extension | path sort -u
- Tab (or any key bound to ``complete``) now expands wildcards instead of invoking completions, if there is a wildcard in the path component under the cursor (:issue:`954`, :issue:`8593`).
- Scripts can now catch and handle the SIGINT and SIGTERM signals, either via ``function --on-signal`` or with ``trap`` (:issue:`6649`).

Deprecations and removed features
---------------------------------
- The ``stderr-nocaret`` feature flag, introduced in fish 3.0 and enabled by default in fish 3.1, has been made read-only.
  That means it is no longer possible to disable it, and code supporting the ``^`` redirection has been removed (:issue:`8857`, :issue:`8865`).

  To recap: fish used to support ``^`` to redirect stderr, so you could use commands like::

    test "$foo" -gt 8 ^/dev/null

  to ignore error messages. This made the ``^`` symbol require escaping and quoting, and was a bit of a weird shortcut considering ``2>`` already worked, which is only one character longer.

  So the above can simply become::

    test "$foo" -gt 8 2>/dev/null

- The following feature flags have been enabled by default:

  - ``regex-easyesc``, which makes ``string replace -r`` not do a superfluous round of unescaping in the replacement expression.
    That means e.g. to escape any "a" or "b" in an argument you can use ``string replace -ra '([ab])' '\\\\$1' foobar`` instead of needing 8 backslashes.

    This only affects the *replacement* expression, not the *match* expression (the ``'([ab])'`` part in the example).
    A survey of plugins on GitHub did not turn up any affected code, so we do not expect this to affect many users.

    This flag was introduced in fish 3.1.
  - ``ampersand-nobg-in-token``, which means that ``&`` will not create a background job if it occurs in the middle of a word. For example, ``echo foo&bar`` will print "foo&bar" instead of running ``echo foo`` in the background and then starting ``bar`` as a second job.

    Reformatting with ``fish_indent`` would already introduce spaces, turning ``echo foo&bar`` into ``echo foo & bar``.

    This flag was introduced in fish 3.4.

  To turn off these flags, add ``no-regex-easyesc`` or ``no-ampersand-nobg-in-token`` to :envvar:`fish_features` and restart fish::

    set -Ua fish_features no-regex-easyesc

  Like ``stderr-nocaret``, they will eventually be made read-only.
- Most ``string`` subcommands no longer append a newline to their input if the input didn't have one (:issue:`8473`, :issue:`3847`)
- Fish's escape sequence removal (like for ``string length --visible`` or to figure out how wide the prompt is) no longer has special support for non-standard color sequences like from Data General terminals, e.g. the Data General Dasher D220 from 1984. This removes a bunch of work in the common case, allowing ``string length --visible`` to be much faster with unknown escape sequences. We don't expect anyone to have ever used fish with such a terminal (:issue:`8769`).
- Code to upgrade universal variables from fish before 3.0 has been removed. Users who upgrade directly from fish versions 2.7.1 or before will have to set their universal variables & abbreviations again. (:issue:`8781`)
- The meaning of an empty color variable has changed (:issue:`8793`). Previously, when a variable was set but empty, it would be interpreted as the "normal" color. Now, empty color variables cause the same effect as unset variables - the general highlighting variable for that type is used instead. For example::

    set -g fish_color_command blue
    set -g fish_color_keyword

  would previously make keywords "normal" (usually white in a dark terminal). Now it'll make them blue. To achieve the previous behavior, use the normal color explicitly: ``set -g fish_color_keyword normal``.

  This makes it easier to make self-contained color schemes that don't accidentally use color that was set before.
  ``fish_config`` has been adjusted to set known color variables that a theme doesn't explicitly set to empty.
- ``eval`` is now a reserved keyword, so it can't be used as a function name. This follows ``set`` and ``read``, and is necessary because it can't be cleanly shadowed by a function - at the very least ``eval set -l argv foo`` breaks. Fish will ignore autoload files for it, so left over ``eval.fish`` from previous fish versions won't be loaded.
- The git prompt in informative mode now defaults to skipping counting untracked files, as this was extremely slow. To turn it on, set :envvar:`__fish_git_prompt_showuntrackedfiles` or set the git config value "bash.showuntrackedfiles" to ``true`` explicitly (which can be done for individual repositories). The "informative+vcs" sample prompt already skipped display of untracked files, but didn't do so in a way that skipped the computation, so it should be quite a bit faster in many cases (:issue:`8980`).
- The ``__terlar_git_prompt`` function, used by the "Terlar" sample prompt, has been rebuilt as a configuration of the normal ``fish_git_prompt`` to ease maintenance, improve performance and add features (like reading per-repo git configuration). Some slight changes remain; users who absolutely must have the same behavior are encouraged to copy the old function (:issue:`9011`, :issue:`7918`, :issue:`8979`).

Scripting improvements
----------------------
- Quoted command substitution that directly follow a variable expansion (like ``echo "$var$(echo x)"``) no longer affect the variable expansion (:issue:`8849`).
- Fish now correctly expands command substitutions that are preceded by an escaped dollar (like ``echo \$(echo)``). This regressed in version 3.4.0.
- ``math`` can now handle underscores (``_``) as visual separators in numbers (:issue:`8611`, :issue:`8496`)::

    math 5 + 2_123_252

- ``math``'s ``min`` and ``max`` functions now take a variable number of arguments instead of always requiring 2 (:issue:`8644`, :issue:`8646`)::

    > math min 8,2,4
    2

- ``read`` is now faster as the last process in a pipeline (:issue:`8552`).
- ``string join`` gained a new ``--no-empty`` flag to skip empty arguments (:issue:`8774`, :issue:`8351`).
- ``read`` now only triggers the ``fish_read`` event, not the ``fish_prompt`` event (:issue:`8797`). It was supposed to work this way in fish 3.2.0 and later, but both events were emitted.
- The TTY modes are no longer restored when non-interactive shells exit. This fixes wrong tty modes in pipelines with interactive commands. (:issue:`8705`).
- Some functions shipped with fish printed error messages to standard output, but they now they rightly go to standard error (:issue:`8855`).
- ``jobs`` now correctly reports CPU usage as a percentage, instead of as a number of clock ticks (:issue:`8919`).
- ``process-exit`` events now fire when the process exits even if the job has not yet exited, fixing a regression in 3.4.1 (:issue:`8914`).

Interactive improvements
------------------------
- Fish now reports a special error if a command wasn't found and there is a non-executable file by that name in :envvar:`PATH` (:issue:`8804`).
- ``less`` and other interactive commands would occasionally be stopped when run in a pipeline with fish functions; this has been fixed (:issue:`8699`).
- Case-changing autosuggestions generated mid-token now correctly append only the suffix, instead of duplicating the token (:issue:`8820`).
- ``ulimit`` learned a number of new options for the resource limits available on Linux, FreeBSD ande NetBSD, and returns a specific warning if the limit specified is not available on the active operating system (:issue:`8823`, :issue:`8786`).
- The ``vared`` command can now successfully edit variables named "tmp" or "prompt" (:issue:`8836`, :issue:`8837`).
- ``time`` now emits an error if used after the first command in a pipeline (:issue:`8841`).
- ``fish_add_path`` now prints a message for skipped non-existent paths when using the ``-v`` flag (:issue:`8884`).
- Since fish 3.2.0, pressing :kbd:`ctrl-d` while a command is running would end up inserting a space into the next commandline, which has been fixed (:issue:`8871`).
- A bug that caused multi-line prompts to be moved down a line when pasting or switching modes has been fixed (:issue:`3481`).
- The Web-based configuration system no longer strips too many quotes in the abbreviation display (:issue:`8917`, :issue:`8918`).
- Fish started with ``--no-config`` will now use the default keybindings (:issue:`8493`)
- When fish inherits a :envvar:`USER` environment variable value that doesn't correspond to the current effective user ID, it will now correct it in all cases (:issue:`8879`, :issue:`8583`).
- Fish sets a new :envvar:`EUID` variable containing the current effective user id (:issue:`8866`).
- ``history search`` no longer interprets the search term as an option (:issue:`8853`)
- The status message when a job terminates should no longer be erased by a multiline prompt (:issue:`8817`)

New or improved bindings
^^^^^^^^^^^^^^^^^^^^^^^^
- The :kbd:`alt-s` binding will now insert ``doas`` instead of ``sudo`` if necessary (:issue:`8942`).
- The ``kill-whole-line`` special input function now kills the newline preceeding the last line. This makes ``dd`` in vi-mode clear the last line properly.
- The new ``kill-inner-line`` special input function kills the line without any newlines, allowing ``cc`` in vi-mode to clear the line while preserving newlines (:issue:`8983`).
- On terminals that emit special sequences for these combinations, :kbd:`shift-space` is bound like :kbd:`space`, and :kbd:`ctrl-enter` is bound like :kbd:`return` (:issue:`8874`).

Improved prompts
^^^^^^^^^^^^^^^^
- A new ``Astronaut`` prompt (:issue:`8775`), a multi-line prompt using plain text reminiscent of the Starship.rs prompt.

Completions
^^^^^^^^^^^
- Added completions for:

  - ``archlinux-java`` (:issue:`8911`)
  - ``apk`` (:issue:`8951`)
  - ``brightnessctl`` (:issue:`8758`)
  - ``efibootmgr`` (:issue:`9010`)
  - ``fastboot`` (:issue:`8904`)
  - ``optimus-manager`` (:issue:`8913`)
  - ``rclone`` (:issue:`8819`)
  - ``sops`` (:issue:`8821`)
  - ``tuned-adm`` (:issue:`8760`)
  - ``wg-quick`` (:issue:`8687`)

- ``complete`` can now be given multiple ``--condition`` options. They will be attempted in the order they were given, and only if all succeed will the completion be made available (as if they were connected with ``&&``). This helps with caching - fish's complete system stores the return value of each condition as long as the commandline doesn't change, so this can reduce the number of conditions that need to be evaluated (:issue:`8536`, :issue:`8967`).

Improved terminal support
^^^^^^^^^^^^^^^^^^^^^^^^^
- Working directory reporting is enabled for kitty (:issue:`8806`).
- Changing the cursor shape is now enabled by default in iTerm2 (:issue:`3696`).

For distributors
----------------
- libatomic is now correctly detected as necessary when building on RISC-V (:issue:`8850`, :issue:`8851`).
- In some cases, the build process found the wrong libintl on macOS. This has been corrected (:issue:`5244`).
- The paths for completions, functions, and configuration snippets now include
  subdirectories ``fish/vendor_completions.d``, ``fish/vendor_functions.d``, and
  ``fish/vendor_conf.d`` (respectively) within ``XDG_DATA_HOME`` (or ``~/.local/share``
  if not defined) (:issue:`8887`, :issue:`7816`).

--------------

fish 3.4.1 (released March 25, 2022)
====================================

This release of fish fixes the following problems identified in fish 3.4.0:

- An error printed after upgrading, where old instances could pick up a newer version of the ``fish_title`` function, has been fixed (:issue:`8778`)
- fish builds correctly on NetBSD (:issue:`8788`) and OpenIndiana (:issue:`8780`).
- ``nextd-or-forward-word``, bound to :kbd:`alt-right` by default, was inadvertently changed to move like ``forward-bigword``. This has been corrected (:issue:`8790`).
- ``funcsave -q`` and ``funcsave --quiet`` now work correctly (:issue:`8830`).
- Issues with the ``csharp`` and ``nmcli`` completions were corrected.

If you are upgrading from version 3.3.1 or before, please also review the release notes for 3.4.0 (included below).

--------------

fish 3.4.0 (released March 12, 2022)
====================================

Notable improvements and fixes
------------------------------
- fish's command substitution syntax has been extended: ``$(cmd)`` now has the same meaning as ``(cmd)`` but it can be used inside double quotes, to prevent line splitting of the results (:issue:`159`)::

    foo (bar | string collect)
    # can now be written as
    foo "$(bar)"

    # and

    foo (bar)
    # can now be written as
    foo $(bar)
    # this will still split on newlines only.

- Complementing the ``prompt`` command in 3.3.0, ``fish_config`` gained a ``theme`` subcommand to show and pick from the sample themes (meaning color schemes) directly in the terminal, instead of having to open a Web browser. For example ``fish_config theme choose Nord`` loads the Nord theme in the current session (:issue:`8132`). The current theme can be saved with ``fish_config theme dump``, and custom themes can be added by saving them in ``~/.config/fish/themes/``.
- ``set`` and ``read`` learned a new option, ``--function``, to set a variable in the function's top scope. This should be a more familiar way of scoping variables and avoids issues with ``--local``, which is actually block-scoped (:issue:`565`, :issue:`8145`)::

    function demonstration
        if true
            set --function foo bar
            set --local baz banana
        end
        echo $foo # prints "bar" because $foo is still valid
        echo $baz # prints nothing because $baz went out of scope
    end

- ``string pad`` now excludes escape sequences like colors that fish knows about, and a new ``--visible`` flag to ``string length`` makes it use that kind of visible width. This is useful to get the number of terminal cells an already colored string would occupy, like in a prompt. (:issue:`8182`, :issue:`7784`, :issue:`4012`)::

    > string length --visible (set_color red)foo
    3

- Performance improvements to globbing, especially on systems using glibc. In some cases (large directories with files with many numbers in the names) this almost halves the time taken to expand the glob.
- Autosuggestions can now be turned off by setting ``$fish_autosuggestion_enabled`` to 0, and (almost) all highlighting can be turned off by choosing the new "None" theme. The exception is necessary colors, like those which distinguish autosuggestions from the actual command line. (:issue:`8376`)
- The ``fish_git_prompt`` function, which is included in the default prompts, now overrides ``git`` to avoid running  commands set by per-repository configuration. This avoids a potential security issue in some circumstances, and has been assigned CVE-2022-20001 (:issue:`8589`).

Deprecations and removed features
---------------------------------
- A new feature flag, ``ampersand-nobg-in-token`` makes ``&`` only act as background operator if followed by a separator. In combination with ``qmark-noglob``, this allows entering most URLs at the command line without quoting or escaping (:issue:`7991`). For example::

    > echo foo&bar # will print "foo&bar", instead of running "echo foo" in the background and executing "bar"
    > echo foo & bar # will still run "echo foo" in the background and then run "bar"
    # with both ampersand-nobg-in-token and qmark-noglob, this argument has no special characters anymore
    > open https://www.youtube.com/watch?v=dQw4w9WgXcQ&feature=youtu.be

  As a reminder, feature flags can be set on startup with ``fish --features ampersand-nobg-in-token,qmark-noglob`` or with a universal variable called ``fish_features``::

    > set -Ua fish_features ampersand-nobg-in-token

- ``$status`` is now forbidden as a command, to prevent a surprisingly common error among new users: Running ``if $status`` (:issue:`8171`). This applies *only* to ``$status``, other variables are still allowed.
- ``set --query`` now returns an exit status of 255 if given no variable names. This means ``if set -q $foo`` will not enter the if-block if ``$foo`` is empty or unset. To restore the previous behavior, use ``if not set -q foo; or set -q $foo`` - but this is unlikely to be desireable (:issue:`8214`).
- ``_`` is now a reserved keyword (:issue:`8342`).
- The special input functions ``delete-or-exit``, ``nextd-or-forward-word`` and ``prevd-or-backward-word`` replace fish functions of the same names (:issue:`8538`).
- Mac OS X 10.9 is no longer supported. The minimum Mac version is now 10.10 "Yosemite."

Scripting improvements
----------------------
- ``string collect`` supports a new ``--allow-empty`` option, which will output one empty argument in a command substitution that has no output (:issue:`8054`). This allows commands like ``test -n (echo -n | string collect --allow-empty)`` to work more reliably. Note this can also be written as ``test -n "$(echo -n)"`` (see above).
- ``string match`` gained a ``--groups-only`` option, which makes it only output capturing groups, excluding the full match. This allows ``string match`` to do simple transformations (:issue:`6056`)::

    > string match -r --groups-only '(.*)fish' 'catfish' 'twofish' 'blue fish' | string escape
    cat
    two
    'blue '

- ``$fish_user_paths`` is now automatically deduplicated to fix a common user error of appending to it in config.fish when it is universal (:issue:`8117`). :ref:`fish_add_path <cmd-fish_add_path>` remains the recommended way to add to $PATH.
- ``return`` can now be used outside functions. In scripts, it does the same thing as ``exit``. In interactive mode,it sets ``$status`` without exiting (:issue:`8148`).
- An oversight prevented all syntax checks from running on commands given to ``fish -c`` (:issue:`8171`). This includes checks such as ``exec`` not being allowed in a pipeline, and ``$$`` not being a valid variable. Generally, another error was generated anyway.
- ``fish_indent`` now correctly reformats tokens that end with a backslash followed by a newline (:issue:`8197`).
- ``commandline`` gained an ``--is-valid`` option to check if the command line is syntactically valid and complete. This allows basic implementation of transient prompts (:issue:`8142`).
- ``commandline`` gained a ``--paging-full-mode`` option to check if the pager is showing all the possible lines (no "7 more rows" message) (:issue:`8485`).
- List expansion correctly reports an error when used with all zero indexes (:issue:`8213`).
- Running ``fish`` with a directory instead of a script as argument (eg ``fish .``) no longer leads to an infinite loop. Instead it errors out immediately (:issue:`8258`)
- Some error messages occuring after fork, like "text file busy" have been replaced by bespoke error messages for fish (like "File is currently open for writing"). This also restores error messages with current glibc versions that removed sys_errlist (:issue:`8234`, :issue:`4183`).
- The ``realpath`` builtin now also squashes leading slashes with the ``--no-symlinks`` option (:issue:`8281`).
- When trying to ``cd`` to a dangling (broken) symbolic link, fish will print an error noting that the target is a broken link (:issue:`8264`).
- On MacOS terminals that are not granted permissions to access a folder, ``cd`` would print a spurious "rotten symlink" error, which has been corrected to "permission denied" (:issue:`8264`).
- Since fish 3.0, ``for`` loops would trigger a variable handler function before the loop was entered. As the variable had not actually changed or been set, this was a spurious event and has been removed (:issue:`8384`).
- ``math`` now correctly prints negative values and values larger than ``2**31`` when in hex or octal bases (:issue:`8417`).
- ``dirs`` always produces an exit status of 0, instead of sometimes returning 1 (:issue:`8211`).
- ``cd ""`` no longer crashes fish (:issue:`8147`).
- ``set --query`` can now query whether a variable is a path variable via ``--path`` or ``--unpath`` (:issue:`8494`).
- Tilde characters (``~``) produced by custom completions are no longer escaped when applied to the command line, making it easier to use the output of a recursive ``complete -C`` in completion scripts (:issue:`4570`).
- ``set --show`` reports when a variable is read-only (:issue:`8179`).
- Erasing ``$fish_emoji_width`` will reset fish to the default guessed emoji width (:issue:`8274`).
- The ``la`` function no longer lists entries for "." and "..", matching other systems defaults (:issue:`8519`).
- ``abbr -q`` returns the correct exit status when given multiple abbreviation names as arguments (:issue:`8431`).
- ``command -v`` returns an exit status of 127 instead of 1 if no command was found (:issue:`8547`).
- ``argparse`` with ``--ignore-unknown`` no longer breaks with multiple unknown options in a short option group (:issue:`8637`).
- Comments inside command substitutions or brackets now correctly ignore parentheses, quotes, and brackets (:issue:`7866`, :issue:`8022`, :issue:`8695`).
- ``complete -C`` supports a new ``--escape`` option, which turns on escaping in returned completion strings (:issue:`3469`).
- Invalid byte or unicode escapes like ``\Utest`` or ``\xNotHex`` are now a tokenizer error instead of causing the token to be truncated (:issue:`8545`).

Interactive improvements
------------------------
- Vi mode cursors are now set properly after :kbd:`ctrl-c` (:issue:`8125`).
- ``funced`` will try to edit the whole file containing a function definition, if there is one (:issue:`391`).
- Running a command line consisting of just spaces now deletes an ephemeral (starting with space) history item again (:issue:`8232`).
- Command substitutions no longer respect job control, instead running inside fish's own process group (:issue:`8172`). This more closely matches other shells, and improves :kbd:`ctrl-c` reliability inside a command substitution.
- ``history`` and ``__fish_print_help`` now properly support ``less`` before version 530, including the version that ships with macOS. (:issue:`8157`).
- ``help`` now knows which section is in which document again (:issue:`8245`).
- fish's highlighter will now color options (starting with ``-`` or ``--``) with the color given in the new $fish_color_option, up to the first ``--``. It falls back on $fish_color_param, so nothing changes for existing setups (:issue:`8292`).
- When executing a command, abbreviations are no longer expanded when the cursor is separated from the command by spaces, making it easier to suppress abbreviation expansion of commands without arguments. (:issue:`8423`).
- ``fish_key_reader``'s output was simplified. By default, it now only prints a bind statement. The previous per-character timing information can be seen with a new ``--verbose`` switch (:issue:`8467`).
- Custom completions are now also loaded for commands that contain tildes or variables like ``~/bin/fish`` or ``$PWD/fish`` (:issue:`8442`).
- Command lines spanning multiple lines will not be overwritten by the completion pager when it fills the entire terminal (:issue:`8509`, :issue:`8405`).
- When redrawing a multiline prompt, the old prompt is now properly cleared (:issue:`8163`).
- Interactive completion would occasionally ignore the last word on the command line due to a race condition. This has been fixed (:issue:`8175`).
- Propagation of universal variables from a fish process that is closing is faster (:issue:`8209`).
- The command line is drawn in the correct place if the prompt ends with a newline (:issue:`8298`).
- ``history`` learned a new subcommand ``clear-session`` to erase all history from the current session (:issue:`5791`).
- Pressing :kbd:`ctrl-c` in ``fish_key_reader`` will no longer print the incorrect "Press [ctrl-C] again to exit" message (:issue:`8510`).
- The default command-not-found handler for Fedora/PackageKit now passes the whole command line, allowing for functionality such as running the suggested command directly (:issue:`8579`).
- When looking for locale information, the Debian configuration is now used when available (:issue:`8557`).
- Pasting text containing quotes from the clipboard trims spaces more appropriately (:issue:`8550`).
- The clipboard bindings ignore X-based clipboard programs if the ``DISPLAY`` environment variable is not set, which helps prefer the Windows clipboard when it is available (such as on WSL).
- ``funcsave`` will remove a saved copy of a function that has been erased with ``functions --erase``.
- The Web-based configuration tool gained a number of improvements, including the ability to set pager colors.
- The default ``fish_title`` prints a shorter title with shortened $PWD and no more redundant "fish" (:issue:`8641`).
- Holding down an arrow key won't freeze the terminal with long periods of flashing (:issue:`8610`).
- Multi-char bindings are no longer interrupted if a signal handler enqueues an event. (:issue:`8628`).

New or improved bindings
^^^^^^^^^^^^^^^^^^^^^^^^
- :kbd:`escape` can now bound without breaking arrow key bindings (:issue:`8428`).
- The :kbd:`alt-h` binding (to open a command’s manual page) now also ignores ``command`` (:issue:`8447`).

Improved prompts
^^^^^^^^^^^^^^^^
- The ``fish_status_to_signal`` helper function returns the correct signal names for the current platform, rather than Linux (:issue:`8530`).
- The ``prompt_pwd`` helper function learned a ``--full-length-dirs N`` option to keep the last N directory components unshortened. In addition the number of characters to shorten each component should be shortened to can now be given as ``-d N`` or ``--dir-length N``. (:issue:`8208`)::

    > prompt_pwd --full-length-dirs 2 -d 1 ~/dev/fish-shell/share/tools/web_config
    ~/d/f/s/tools/web_config

Completions
^^^^^^^^^^^
- Added completions for:

  - Apple's ``shortcuts``
  - ``argparse`` (:issue:`8434`)
  - ``asd`` (:issue:`8759`)
  - ``az`` (:issue:`8141`)
  - ``black`` (:issue:`8123`)
  - ``clasp`` (:issue:`8373`)
  - ``cpupower`` (:issue:`8302`)
  - ``dart`` (:issue:`8315`)
  - ``dscacheutil``
  - ``elvish`` (:issue:`8416`)
  - ``ethtool`` (:issue:`8283`)
  - ``exif`` (:issue:`8246`)
  - ``findstr`` (:issue:`8481`)
  - ``git-sizer`` (:issue:`8156`)
  - ``gnome-extensions`` (:issue:`8732`)
  - ``gping`` (:issue:`8181`)
  - ``isatty`` (:issue:`8609`)
  - ``istioctl`` (:issue:`8343`)
  - ``kmutil``
  - ``kubectl`` (:issue:`8734`)
  - ``matlab`` (:issue:`8505`)
  - ``mono`` (:issue:`8415`) and related tools ``csharp``, ``gacutil``, ``gendarme``, ``ikdasm``, ``ilasm``, ``mkbundle``, ``monodis``, ``monop``, ``sqlsharp`` and ``xsp`` (:issue:`8452`)
  -  Angular's ``ng`` (:issue:`8111`)
  - ``nodeenv`` (:issue:`8533`)
  - ``octave`` (:issue:`8505`)
  - ``pabcnet_clear`` (:issue:`8421`)
  - ``qmk`` (:issue:`8180`)
  - ``rakudo`` (:issue:`8113`)
  - ``rc-status`` (:issue:`8757`)
  - ``roswell`` (:issue:`8330`)
  - ``sbcl`` (:issue:`8330`)
  - ``starship`` (:issue:`8520`)
  - ``topgrade`` (:issue:`8651`)
  -  ``wine``, ``wineboot`` and ``winemaker`` (:issue:`8411`)
  -  Windows Subsystem for Linux (WSL)'s ``wslpath`` (:issue:`8364`)
  -  Windows' ``color`` (:issue:`8483`), ``attrib``, ``attributes``, ``choice``, ``clean``, ``cleanmgr``, ``cmd``, ``cmdkey``, ``comp``, ``forfiles``, ``powershell``, ``reg``, ``schtasks``, ``setx`` (:issue:`8486`)
  - ``zef`` (:issue:`8114`)

- Improvements to many completions, especially for ``git`` aliases (:issue:`8129`), subcommands (:issue:`8134`) and submodules (:issue:`8716`).
- Many adjustments to complete correct options for system utilities on BSD and macOS.
- When evaluating custom completions, the command line state no longer includes variable overrides (``var=val``). This unbreaks completions that read ``commandline -op``.

Improved terminal support
^^^^^^^^^^^^^^^^^^^^^^^^^
- Dynamic terminal titles are enabled on WezTerm (:issue:`8121`).
- Directory history navigation works out of the box with Apple Terminal's default key settings (:issue:`2330`).
- fish now assumes Unicode 9+ widths for emoji under iTerm 2 (:issue:`8200`).
- Skin-tone emoji modifiers (U+1F3FB through U+1F3FF) are now measured as width 0 (:issue:`8275`).
- fish's escape sequence removal now also knows Tmux's wrapped escapes.
- Vi mode cursors are enabled in Apple Terminal.app (:issue:`8167`).
- Vi cursor shaping and $PWD reporting is now also enabled on foot (:issue:`8422`).
- ``ls`` will use colors also on newer versions of Apple Terminal.app (:issue:`8309`).
- The :kbd:`delete` and :kbd:`shift-tab` keys work more reliably under ``st`` (:issue:`8352`, :issue:`8354`).

Other improvements
------------------
- Fish's test suite now uses ``ctest``, and has become much faster to run. It is now also possible to run only specific tests with targets named ``test_$filename`` - ``make test_set.fish`` only runs the set.fish test. (:issue:`7851`)
- The HTML version of the documentation now includes copy buttons for code examples (:issue:`8218`).
- The HTML version of the documentation and the web-based configuration tool now pick more modern system fonts instead of falling back to Arial and something like Courier New most of the time (:issue:`8632`).
- The Debian & Ubuntu package linked from fishshell.com is now a single package, rather than split into ``fish`` and ``fish-common`` (:issue:`7845`).
- The macOS installer does not assert that Rosetta is required to install fish on machines with Apple Silicon (:issue:`8566`).
- The macOS installer now cleans up previous .pkg installations when upgrading. (:issue:`2963`).

For distributors
----------------
- The minimum version of CMake required to build fish is now 3.5.0.
- The CMake installation supports absolute paths for ``CMAKE_INSTALL_DATADIR`` (:issue:`8150`).
- Building using NetBSD curses works on any platform (:issue:`8087`).
- The build system now uses the default linker instead of forcing use of the gold or lld linker (:issue:`8152`).

--------------

fish 3.3.1 (released July 6, 2021)
==================================

This release of fish fixes the following problems identified in fish 3.3.0:

- The prompt and command line are redrawn correctly in response to universal variable changes (:issue:`8088`).
- A superfluous error that was produced when setting the ``PATH`` or ``CDPATH`` environment variables to include colon-delimited components that do not exist was removed (:issue:`8095`).
- The Vi mode indicator in the prompt is repainted correctly after :kbd:`ctrl-c` cancels the current command (:issue:`8103`).
- fish builds correctly on platforms that do not have a ``spawn.h`` header, such as old versions of OS X (:issue:`8097`).

A number of improvements to the documentation, and fixes for completions, are included as well.

If you are upgrading from version 3.2.2 or before, please also review the release notes for 3.3.0 (included below).

--------------

fish 3.3.0 (released June 28, 2021)
===================================


Notable improvements and fixes
------------------------------
- ``fish_config`` gained a ``prompt`` subcommand to show and pick from the sample prompts directly in the terminal, instead of having to open a webbrowser. For example ``fish_config prompt choose default`` loads the default prompt in the current session (:issue:`7958`).
- The documentation has been reorganized to be easier to understand (:issue:`7773`).

Deprecations and removed features
---------------------------------
- The ``$fish_history`` value "default" is no longer special. It used to be treated the same as "fish" (:issue:`7650`).
- Redirection to standard error with the ``^`` character has been disabled by default. It can be turned back on using the ``stderr-nocaret`` feature flag, but will eventually be disabled completely (:issue:`7105`).
- Specifying an initial tab to ``fish_config`` now only works with ``fish_config browse`` (eg ``fish_config browse variables``), otherwise it would interfere with the new ``prompt`` subcommand (see below) (:issue:`7958`).

Scripting improvements
----------------------
- ``math`` gained new functions ``log2`` (like the documentation claimed), ``max`` and ``min`` (:issue:`7856`). ``math`` functions can be used without the parentheses (eg ``math sin 2 + 6``), and functions have the lowest precedence in the order of operations (:issue:`7877`).
- Shebang (``#!``) lines are no longer required within shell scripts, improving support for scripts with concatenated binary contents. If a file fails to execute and passes a (rudimentary) binary safety check, fish will re-invoke it using ``/bin/sh`` (:issue:`7802`).
- Exit codes are better aligned with bash. A failed execution now reports ``$status`` of 127 if the file is not found, and 126 if it is not executable.
- ``echo`` no longer writes its output one byte at a time, improving performance and allowing use with Linux's special API files (``/proc``, ``/sys`` and such) (:issue:`7836`).
- fish should now better handle ``cd`` on filesystems with broken ``stat(3)`` responses (:issue:`7577`).
- Builtins now properly report a ``$status`` of 1 upon unsuccessful writes (:issue:`7857`).
- ``string match`` with unmatched capture groups and without the ``--all`` flag now sets an empty variable instead of a variable containing the empty string. It also correctly imports the first match if multiple arguments are provided, matching the documentation. (:issue:`7938`).
- fish produces more specific errors when a command in a command substitution wasn't found or is not allowed. This now prints something like "Unknown command" instead of "Unknown error while evaluating command substitution".
- ``fish_indent`` allows inline variable assignments (``FOO=BAR command``) to use line continuation, instead of joining them into one line (:issue:`7955`).
- fish gained a ``--no-config`` option to disable configuration files. This applies to user-specific and the systemwide ``config.fish`` (typically in ``/etc/fish/config.fish``), and configuration snippets (typically in ``conf.d`` directories). It also disables universal variables, history, and loading of functions from system or user configuration directories (:issue:`7921`, :issue:`1256`).
- When universal variables are unavailable for some reason, setting a universal variable now sets a global variable instead (:issue:`7921`).
- ``$last_pid`` now contains the process ID of the last process in the pipeline, allowing it to be used in scripts (:issue:`5036`, :issue:`5832`, :issue:`7721`). Previously, this value contained the process group ID, but in scripts this was the same as the running fish's process ID.
- ``process-exit`` event handlers now receive the same value as ``$status`` in all cases, instead of receiving -1 when the exit was due to a signal.
- ``process-exit`` event handlers for PID 0 also received ``JOB_EXIT`` events; this has been fixed.
- ``job-exit`` event handlers may now be created with any of the PIDs from the job. The handler is passed the last PID in the job as its second argument, instead of the process group.
- Trying to set an empty variable name with ``set`` no longer works (these variables could not be used in expansions anyway).
- ``fish_add_path`` handles an undefined ``PATH`` environment variable correctly (:issue:`8082`).

Interactive improvements
-------------------------
- Commands entered before the previous command finishes will now be properly syntax highlighted.
- fish now automatically creates ``config.fish`` and the configuration directories in ``$XDG_CONFIG_HOME/fish`` (by default ``~/.config/fish``) if they do not already exist (:issue:`7402`).
- ``$SHLVL`` is no longer incremented in non-interactive shells. This means it won't be set to values larger than 1 just because your environment happens to run some scripts in $SHELL in its startup path (:issue:`7864`).
- fish no longer rings the bell when flashing the command line. The flashing should already be enough notification and the bell can be annoying (:issue:`7875`).
- ``fish --help`` is more helpful if the documentation isn't installed (:issue:`7824`).
- ``funced`` won't include an entry on where a function is defined, thanks to the new ``functions --no-details`` option (:issue:`7879`).
- A new variable, ``fish_killring``, containing entries from the killring, is now available (:issue:`7445`).
- ``fish --private`` prints a note on private mode on startup even if ``$fish_greeting`` is an empty list (:issue:`7974`).
- fish no longer attempts to lock history or universal variable files on remote filesystems, including NFS and Samba mounts. In rare cases, updates to these files may be dropped if separate fish instances modify them simultaneously. (:issue:`7968`).
- ``wait`` and ``on-process-exit`` work correctly with jobs that have already exited (:issue:`7210`).
- ``__fish_print_help`` (used for ``--help`` output for fish's builtins) now respects the ``LESS`` environment variable, and if not set, uses better default pager settings (:issue:`7997`).
- Errors from ``alias`` are now printed to standard error, matching other builtins and functions (:issue:`7925`).
- ``ls`` output is colorized on OpenBSD if colorls utility is installed (:issue:`8035`)
- The default pager color looks better in terminals with light backgrounds (:issue:`3412`).
- Further robustness improvements to the bash history import (:issue:`7874`).
- fish now tries to find a Unicode-aware locale for encoding (``LC_CTYPE``) if started without any locale information, improving the display of emoji and other non-ASCII text on misconfigured systems (:issue:`8031`). To allow a C locale, set the variable ``fish_allow_singlebyte_locale`` to 1.
- The Web-based configuration and documentation now feature a dark mode if the browser requests it (:issue:`8043`).
- Color variables can now also be given like ``--background red`` and ``-b red``, not just ``--background=red`` (:issue:`8053`).
- ``exit`` run within ``fish_prompt`` now exits properly (:issue:`8033`).
- When attempting to execute the unsupported POSIX-style brace command group (``{ ... }``) fish will suggest its equivalent ``begin; ...; end`` commands (:issue:`6415`).

New or improved bindings
^^^^^^^^^^^^^^^^^^^^^^^^
- Pasting in Vi mode puts text in the right place in normal mode (:issue:`7847`).
- Vi mode's :kbd:`u` is bound to ``undo`` instead of ``history-search-backward``, following GNU readline's behavior. Similarly, :kbd:`ctrl-r` is bound to ``redo`` instead of ``history-search-backward``, following Vim (:issue:`7908`).
- :kbd:`s` in Vi visual mode now does the same thing as :kbd:`c` (:issue:`8039`).
- The binding for :kbd:`",*,y` now uses ``fish_clipboard_copy``, allowing it to support more than just ``xsel``.
- The :kbd:`ctrl-space` binding can be correctly customised (:issue:`7922`).
- ``exit`` works correctly in bindings (:issue:`7967`).
- The :kbd:`f1` binding, which opens the manual page for the current command, now works around a bug in certain ``less`` versions that fail to clear the screen (:issue:`7863`).
- The binding for :kbd:`alt-s` now toggles whether ``sudo`` is prepended, even when it took the commandline from history instead of only adding it.
- The new functions ``fish_commandline_prepend`` and ``fish_commandline_append`` allow toggling the presence of a prefix/suffix on the current commandline. (:issue:`7905`).
- ``backward-kill-path-component`` :kbd:`ctrl-w`) no longer erases parts of two tokens when the cursor is positioned immediately after ``/``. (:issue:`6258`).

Improved prompts
^^^^^^^^^^^^^^^^
- The default Vi mode prompt now uses foreground instead of background colors, making it less obtrusive (:issue:`7880`).
- Performance of the "informative" git prompt is improved somewhat (:issue:`7871`). This is still slower than the non-informative version by its very nature. In particular it is IO-bound, so it will be very slow on slow disks or network mounts.
- The sample prompts were updated. Some duplicated prompts, like the various classic variants, or less useful ones, like the "justadollar" prompt were removed, some prompts were cleaned up, and in some cases renamed. A new "simple" and "disco" prompt were added (:issue:`7884`, :issue:`7897`, :issue:`7930`). The new prompts will only take effect when selected and existing installed prompts will remain unchanged.
- A new ``prompt_login`` helper function to describe the kind of "login" (user, host and chroot status) for use in prompts. This replaces the old "debian chroot" prompt and has been added to the default and terlar prompts (:issue:`7932`).
- The Web-based configuration's prompt picker now shows and installs right prompts (:issue:`7930`).
- The git prompt now has the same symbol order in normal and "informative" mode, and it's customizable via ``$__fish_git_prompt_status_order`` (:issue:`7926`).

Completions
^^^^^^^^^^^
- Added completions for:

  - ``firewall-cmd`` (:issue:`7900`)
  - ``sv`` (:issue:`8069`)

- Improvements to plenty of completions!
- Commands that wrap ``cd`` (using ``complete --wraps cd``) get the same completions as ``cd`` (:issue:`4693`).
- The ``--force-files`` option to ``complete`` works for bare arguments, not just options (:issue:`7920`).
- Completion descriptions for functions don't include the function definition, making them more concise (:issue:`7911`).
- The ``kill`` completions no longer error on MSYS2 (:issue:`8046`).
- Completion scripts are now loaded when calling a command via a relative path (like ``./git``) (:issue:`6001`, :issue:`7992`).
- When there are multiple completion candidates, fish inserts their shared prefix. This prefix was computed in a case-insensitive way, resulting in wrong case in the completion pager. This was fixed by only inserting prefixes with matching case (:issue:`7744`).

Improved terminal support
^^^^^^^^^^^^^^^^^^^^^^^^^
- fish no longer tries to detect a missing new line during startup, preventing an erroneous ``⏎`` from appearing if the terminal is resized at the wrong time, which can happen in tiling window managers (:issue:`7893`).
- fish behaves better when it disagrees with the terminal on the width of characters. In particular, staircase effects with right prompts should be gone in most cases (:issue:`8011`).
- If the prompt takes up the entire line, the last character should no longer be chopped off in certain terminals (:issue:`8002`).
- fish's reflow handling has been disabled by default for kitty (:issue:`7961`).
- The default prompt no longer produces errors when used with a dumb terminal (:issue:`7904`).
- Terminal size variables are updated for window size change signal handlers (``SIGWINCH``).
- Pasting within a multi-line command using a terminal that supports bracketed paste works correctly, instead of producing an error (:issue:`7782`).
- ``set_color`` produces an error when used with invalid arguments, rather than empty output which interacts badly with Cartesian product expansion.

For distributors
----------------
- fish runs correctly on platforms without the ``O_CLOEXEC`` flag for ``open(2)`` (:issue:`8023`).

--------------

fish 3.2.2 (released April 7, 2021)
====================================

This release of fish fixes a number of additional issues identified in the fish 3.2 series:

- The command-not-found handler used suggestions from ``pacman`` on Arch Linux, but this caused major slowdowns on some systems and has been disabled (:issue:`7841`).
- fish will no longer hang on exit if another process is in the foreground on macOS (:issue:`7901`).
- Certain programs (such as ``lazygit``) could create situations where fish would not receive keystrokes correctly, but it is now more robust in these situations (:issue:`7853`).
- Arguments longer than 1024 characters no longer trigger excessive CPU usage on macOS (:issue:`7837`).
- fish builds correctly on macOS when using new versions of Xcode (:issue:`7838`).
- Completions for ``aura`` (:issue:`7865`) and ``tshark`` (:issue:`7858`) should no longer produce errors.
- Background jobs no longer interfere with syntax highlighting (a regression introduced in fish 3.2.1, :issue:`7842`).

If you are upgrading from version 3.1.2 or before, please also review the release notes for 3.2.1 and 3.2.0 (included below).

--------------

fish 3.2.1 (released March 18, 2021)
====================================

This release of fish fixes the following problems identified in fish 3.2.0:

-  Commands in key bindings are run with fish's internal terminal modes, instead of the terminal modes typically used for commands. This fixes a bug introduced in 3.2.0, where text would unexpectedly appear on the terminal, especially when pasting (:issue:`7770`).
-  Prompts which use the internal ``__fish_print_pipestatus`` function will display correctly rather than carrying certain modifiers (such as bold) further than intended (:issue:`7771`).
-  Redirections to internal file descriptors is allowed again, reversing the changes in 3.2.0. This fixes a problem with Midnight Commander (:issue:`7769`).
-  Universal variables should be fully reliable regardless of operating system again (:issue:`7774`).
-  ``fish_git_prompt`` no longer causes screen flickering in certain terminals (:issue:`7775`).
-  ``fish_add_path`` manipulates the ``fish_user_paths`` variable correctly when moving multiple paths (:issue:`7776`).
-  Pasting with a multi-line command no longer causes a ``__fish_tokenizer_state`` error (:issue:`7782`).
-  ``psub`` inside event handlers cleans up temporary files properly (:issue:`7792`).
-  Event handlers declared with ``--on-job-exit $fish_pid`` no longer run constantly (:issue:`7721`), although these functions should use ``--on-event fish_exit`` instead.
-  Changing terminal modes inside ``config.fish`` works (:issue:`7783`).
-  ``set_color --print-colors`` no longer prints all colors in bold (:issue:`7805`)
-  Completing commands starting with a ``-`` no longer prints an error (:issue:`7809`).
-  Running ``fish_command_not_found`` directly no longer produces an error on macOS or other OSes which do not have a handler available (:issue:`7777`).
-  The new ``type`` builtin now has the (deprecated) ``--quiet`` long form of ``-q`` (:issue:`7766`).

It also includes some small enhancements:

-  ``help`` and ``fish_config`` work correctly when fish is running in a Chrome OS Crostini Linux VM (:issue:`7789`).
-  The history file can be made a symbolic link without it being overwritten (:issue:`7754`), matching a similar improvement for the universal variable file in 3.2.0.
-  An unhelpful error ("access: No error"), seen on Cygwin, is no longer produced (:issue:`7785`).
-  Improvements to the ``rsync`` completions (:issue:`7763`), some completion descriptions (:issue:`7788`), and completions that use IP address (:issue:`7787`).
-  Improvements to the appearance of ``fish_config`` (:issue:`7811`).

If you are upgrading from version 3.1.2 or before, please also review
the release notes for 3.2.0 (included below).

--------------

fish 3.2.0 (released March 1, 2021)
===================================

Notable improvements and fixes
------------------------------

-  **Undo and redo support** for the command-line editor and pager search (:issue:`1367`). By default, undo is bound to Control+Z, and redo to Alt+/.
-  **Builtins can now output before all data is read**. For example, ``string replace`` no longer has to read all of stdin before it can begin to output.
   This makes it usable also for pipes where the previous command hasn't finished yet, like::

    # Show all dmesg lines related to "usb"
    dmesg -w | string match '*usb*'

-  **Prompts will now be truncated** instead of replaced with ``"> "`` if they are wider than the terminal (:issue:`904`).
   For example::

     ~/dev/build/fish-shell-git/src/fish-shell/build (makepkg)>

   will turn into::

     …h-shell/build (makepkg)>

   It is still possible to react to the ``COLUMNS`` variable inside the prompt to implement smarter behavior.
-  **fish completes ambiguous completions** after pressing :kbd:`tab` even when they
   have a common prefix, without the user having to press :kbd:`tab` again
   (:issue:`6924`).
-  fish is less aggressive about resetting terminal modes, such as flow control, after every command.
   Although flow control remains off by default, enterprising users can now enable it with
   ``stty`` (:issue:`2315`, :issue:`7704`).
-  A new **"fish_add_path" helper function to add paths to $PATH** without producing duplicates,
   to be used interactively or in ``config.fish`` (:issue:`6960`, :issue:`7028`).
   For example::

     fish_add_path /opt/mycoolthing/bin

   will add /opt/mycoolthing/bin to the beginning of $fish_user_path without creating duplicates,
   so it can be called safely from config.fish or interactively, and the path will just be there, once.
-  **Better errors with "test"** (:issue:`6030`)::

    > test 1 = 2 and echo true or false
    test: Expected a combining operator like '-a' at index 4
    1 = 2 and echo true or echo false
          ^

   This includes numbering the index from 1 instead of 0, like fish lists.
-  **A new theme for the documentation and Web-based configuration** (:issue:`6500`, :issue:`7371`, :issue:`7523`), matching the design on fishshell.com.
-  ``fish --no-execute`` **will no longer complain about unknown commands**
   or non-matching wildcards, as these could be defined differently at
   runtime (especially for functions). This makes it usable as a static syntax checker (:issue:`977`).
-  ``string match --regex`` now integrates **named PCRE2 capture groups as fish variables**, allowing variables to be set directly from ``string match`` (:issue:`7459`). To support this functionality, ``string`` is now a reserved word and can no longer be wrapped in a function.
-  Globs and other **expansions are limited to 512,288 results** (:issue:`7226`). Because operating systems limit the number of arguments to commands, larger values are unlikely to work anyway, and this helps to avoid hangs.
-  A new **"fish for bash users" documentation page** gives a quick overview of the scripting differences between bash and fish (:issue:`2382`), and the completion tutorial has also been moved out into its own document (:issue:`6709`).

Syntax changes and new commands
-------------------------------

-  Range limits in index range expansions like ``$x[$start..$end]`` may be omitted: ``$start`` and ``$end`` default to 1 and -1 (the last item) respectively (:issue:`6574`)::

     echo $var[1..]
     echo $var[..-1]
     echo $var[..]

   All print the full list ``$var``.
-  When globbing, a segment which is exactly ``**`` may now match zero directories. For example ``**/foo`` may match ``foo`` in the current directory (:issue:`7222`).

Scripting improvements
----------------------

-  The ``type``, ``_`` (gettext), ``.`` (source) and ``:`` (no-op) functions
   are now implemented builtins for performance purposes (:issue:`7342`, :issue:`7036`, :issue:`6854`).
-  ``set`` and backgrounded jobs no longer overwrite ``$pipestatus`` (:issue:`6820`), improving its use in command substitutions (:issue:`6998`).
-  Computed ("electric") variables such as ``status`` are now only global in scope, so ``set -Uq status`` returns false (:issue:`7032`).
-  The output for ``set --show`` has been shortened, only mentioning the scopes in which a variable exists (:issue:`6944`).
   In addition, it now shows if a variable is a path variable.
-  A new variable, ``fish_kill_signal``, is set to the signal that terminated the last foreground job, or ``0`` if the job exited normally (:issue:`6824`, :issue:`6822`).
-  A new subcommand, ``string pad``, allows extending strings to a given width (:issue:`7340`, :issue:`7102`).
-  ``string sub`` has a new ``--end`` option to specify the end index of
   a substring (:issue:`6765`, :issue:`5974`).
-  ``string split`` has a new ``--fields`` option to specify fields to
   output, similar to ``cut -f`` (:issue:`6770`).
-  ``string trim`` now also trims vertical tabs by default (:issue:`6795`).
-  ``string replace`` no longer prints an error if a capturing group wasn't matched, instead treating it as empty (:issue:`7343`).
-  ``string`` subcommands now quit early when used with ``--quiet`` (:issue:`7495`).
-  ``string repeat`` now handles multiple arguments, repeating each one (:issue:`5988`).
-  ``printf`` no longer prints an error if not given an argument (not
   even a format string).
-  The ``true`` and ``false`` builtins ignore any arguments, like other shells (:issue:`7030`).
-  ``fish_indent`` now removes unnecessary quotes in simple cases (:issue:`6722`)
   and gained a ``--check`` option to just check if a file is indented correctly (:issue:`7251`).
-  ``fish_indent`` indents continuation lines that follow a line ending in a backslash, ``|``, ``&&`` or ``||``.
-  ``pushd`` only adds a directory to the stack if changing to it was successful (:issue:`6947`).
-  A new ``fish_job_summary`` function is called whenever a
   background job stops or ends, or any job terminates from a signal (:issue:`6959`, :issue:`2727`, :issue:`4319`).
   The default behaviour can now be customized by redefining it.
-  ``status`` gained new ``dirname`` and ``basename`` convenience subcommands
   to get just the directory to the running script or the name of it,
   to simplify common tasks such as running ``(dirname (status filename))`` (:issue:`7076`, :issue:`1818`).
-  Broken pipelines are now handled more smoothly; in particular, bad redirection mid-pipeline
   results in the job continuing to run but with the broken file descriptor replaced with a closed
   file descriptor. This allows better error recovery and is more in line with other shells'
   behaviour (:issue:`7038`).
-  ``jobs --quiet PID`` no longer prints "no suitable job" if the job for PID does not exist (eg because it has finished) (:issue:`6809`, :issue:`6812`).
-  ``jobs`` now shows continued child processes correctly (:issue:`6818`)
-  ``disown`` should no longer create zombie processes when job control is off, such as in ``config.fish`` (:issue:`7183`).
-  ``command``, ``jobs`` and ``type`` builtins support ``--query`` as the long form of ``-q``, matching other builtins.
   The long form ``--quiet`` is deprecated (:issue:`7276`).
-  ``argparse`` no longer requires a short flag letter for long-only options (:issue:`7585`)
   and only prints a backtrace with invalid options to argparse itself (:issue:`6703`).
-  ``argparse`` now passes the validation variables (e.g. ``$_flag_value``) as local-exported variables,
   avoiding the need for ``--no-scope-shadowing`` in validation functions.
-  ``complete`` takes the first argument as the name of the command if the ``--command``/``-c`` option is not used,
   so ``complete git`` is treated like ``complete --command git``,
   and it can show the loaded completions for specific commands with ``complete COMMANDNAME`` (:issue:`7321`).
-  ``set_color -b`` (without an argument) no longer prints an error message, matching other invalid invocations of this command (:issue:`7154`).
-  ``exec`` no longer produces a syntax error when the command cannot be found (:issue:`6098`).
-  ``set --erase`` and ``abbr --erase`` can now erase multiple things in one go, matching ``functions --erase`` (:issue:`7377`).
-  ``abbr --erase`` no longer prints errors when used with no arguments or on an unset abbreviation (:issue:`7376`, :issue:`7732`).
-  ``test -t``, for testing whether file descriptors are connected to a terminal, works for file descriptors 0, 1, and 2 (:issue:`4766`).
   It can still return incorrect results in other cases (:issue:`1228`).
-  Trying to execute scripts with Windows line endings (CRLF) produces a sensible error (:issue:`2783`).
-  Trying to execute commands with arguments that exceed the operating system limit now produces a specific error (:issue:`6800`).
-  An ``alias`` that delegates to a command with the same name no longer triggers an error about recursive completion (:issue:`7389`).
-  ``math`` now has a ``--base`` option to output the result in hexadecimal or octal (:issue:`7496`) and produces more specific error messages (:issue:`7508`).
-  ``math`` learned bitwise functions ``bitand``, ``bitor`` and ``bitxor``, used like ``math "bitand(0xFE, 5)"`` (:issue:`7281`).
-  ``math`` learned tau for those who don't like typing "2 * pi".
-  Failed redirections will now set ``$status`` (:issue:`7540`).
-  fish sets exit status in a more consistent manner after errors, including invalid expansions like ``$foo[``.
-  Using ``read --silent`` while fish is in private mode was adding these potentially-sensitive entries to the history; this has been fixed (:issue:`7230`).
-  ``read`` can now read interactively from other files, and can be used to read from the terminal via ``read </dev/tty`` (if the operating system provides ``/dev/tty``) (:issue:`7358`).
-  A new ``fish_status_to_signal`` function for transforming exit statuses to signal names has been added (:issue:`7597`, :issue:`7595`).
-  The fallback ``realpath`` builtin supports the ``-s``/``--no-symlinks`` option, like GNU realpath (:issue:`7574`).
-  ``functions`` and ``type`` now explain when a function was defined via ``source`` instead of just saying ``Defined in -``.
-  Significant performance improvements when globbing, appending to variables or in ``math``.
-  ``echo`` no longer interprets options at the beginning of an argument (eg ``echo "-n foo"``) (:issue:`7614`).
-  fish now finds user configuration even if the ``HOME`` environment variable is not set (:issue:`7620`).
-  fish no longer crashes when started from a Windows-style working directory (eg ``F:\path``) (:issue:`7636`).
-  ``fish -c`` now reads the remaining arguments into ``$argv`` (:issue:`2314`).
-  The ``pwd`` command supports the long options ``--logical`` and ``--physical``, matching other implementations (:issue:`6787`).
-  ``fish --profile`` now only starts profiling after fish is ready to execute commands (all configuration is completed). There is a new ``--profile-startup`` option that only profiles the startup and configuration process (:issue:`7648`).
-  Builtins return a maximum exit status of 255, rather than potentially overflowing. In particular, this affects ``exit``, ``return``, ``functions --query``, and ``set --query`` (:issue:`7698`, :issue:`7702`).
- It is no longer an error to run builtin with closed stdin. For example ``count <&-`` now prints 0, instead of failing.
- Blocks, functions, and builtins no longer permit redirecting to file descriptors other than 0 (standard input), 1 (standard output) and 2 (standard error). For example, ``echo hello >&5`` is now an error. This prevents corruption of internal state (:issue:`3303`).

Interactive improvements
------------------------

-  fish will now always attempt to become process group leader in interactive mode (:issue:`7060`). This helps avoid hangs in certain circumstances, and allows tmux's current directory introspection to work (:issue:`5699`).
-  The interactive reader now allows ending a line in a logical operators (``&&`` and ``||``) instead of complaining about a missing command. (This was already syntactically valid, but interactive sessions didn't know about it yet).
-  The prompt is reprinted after a background job exits (:issue:`1018`).
-  fish no longer inserts a space after a completion ending in ``.``, ``,`` or ``-`` is accepted, improving completions for tools that provide dynamic completions (:issue:`6928`).
-  If a filename is invalid when first pressing :kbd:`tab`, but becomes valid, it will be completed properly on the next attempt (:issue:`6863`).
- ``help string match/replace/<subcommand>`` will show the help for string subcommands (:issue:`6786`).
-  ``fish_key_reader`` sets the exit status to 0 when used with ``--help`` or ``--version`` (:issue:`6964`).
-  ``fish_key_reader`` and ``fish_indent`` send output from ``--version`` to standard output, matching other fish binaries (:issue:`6964`).
-  A new variable ``$status_generation`` is incremented only when the previous command produces an exit status (:issue:`6815`). This can be used, for example, to check whether a failure status is a holdover due to a background job, or actually produced by the last run command.
-  ``fish_greeting`` is now a function that reads a variable of the same name, and defaults to setting it globally.
   This removes a universal variable by default and helps with updating the greeting.
   However, to disable the greeting it is now necessary to explicitly specify universal scope (``set -U fish_greeting``) or to disable it in config.fish (:issue:`7265`).
-  Events are properly emitted after a job is cancelled (:issue:`2356`).
-  ``fish_preexec`` and ``fish_postexec`` events are no longer triggered for empty commands (:issue:`4829`, :issue:`7085`).
-  Functions triggered by the ``fish_exit`` event are correctly run when the terminal is closed or the shell receives SIGHUP (:issue:`7014`).
-  The ``fish_prompt`` event no longer fires when ``read`` is used. If
   you need a function to run any time ``read`` is invoked by a script,
   use the new ``fish_read`` event instead (:issue:`7039`).
-  A new ``fish_posterror`` event is emitted when attempting to execute a command with syntax errors (:issue:`6880`, :issue:`6816`).
-  The debugging system has now fully switched from the old numbered level to the new named category system introduced in 3.1. A number of new debugging categories have been added, including ``config``, ``path``, ``reader`` and ``screen`` (:issue:`6511`). See the output of ``fish --print-debug-categories`` for the full list.
-  The warning about read-only filesystems has been moved to a new "warning-path" debug category
   and can be disabled by setting a debug category of ``-warning-path`` (:issue:`6630`)::

     fish --debug=-warning-path

-  The enabled debug categories are now printed on shell startup (:issue:`7007`).
-  The ``-o`` short option to fish, for ``--debug-output``, works correctly instead of producing an
   invalid option error (:issue:`7254`).
-  fish's debugging can now also be enabled via ``FISH_DEBUG`` and ``FISH_DEBUG_OUTPUT`` environment variables.
   This helps with debugging when no commandline options can be passed, like when fish is called in a shebang (:issue:`7359`).
-  Abbreviations are now expanded after all command terminators (eg ``;`` or ``|``), not just space,
   as in fish 2.7.1 and before (:issue:`6970`), and after closing a command substitution (:issue:`6658`).
-  The history file is now created with user-private permissions,
   matching other shells (:issue:`6926`). The directory containing the history
   file was already private, so there should not have been any private data
   revealed.
-  The output of ``time`` is now properly aligned in all cases (:issue:`6726`, :issue:`6714`) and no longer depends on locale (:issue:`6757`).
-  The command-not-found handling has been simplified.
   When it can't find a command, fish now just executes a function called ``fish_command_not_found``
   instead of firing an event, making it easier to replace and reason about.
   Previously-defined ``__fish_command_not_found_handler`` functions with an appropriate event listener will still work (:issue:`7293`).
-  :kbd:`ctrl-c` handling has been reimplemented in C++ and is therefore quicker (:issue:`5259`), no longer occasionally prints an "unknown command" error (:issue:`7145`) or overwrites multiline prompts (:issue:`3537`).
-  :kbd:`ctrl-c` no longer kills background jobs for which job control is
   disabled, matching POSIX semantics (:issue:`6828`, :issue:`6861`).
-  Autosuggestions work properly after :kbd:`ctrl-c` cancels the current commmand line (:issue:`6937`).
-  History search is now case-insensitive unless the search string contains an uppercase character (:issue:`7273`).
-  ``fish_update_completions`` gained a new ``--keep`` option, which improves speed by skipping completions that already exist (:issue:`6775`, :issue:`6796`).
-  Aliases containing an embedded backslash appear properly in the output of ``alias`` (:issue:`6910`).
-  ``open`` no longer hangs indefinitely on certain systems, as a bug in ``xdg-open`` has been worked around (:issue:`7215`).
-  Long command lines no longer add a blank line after execution (:issue:`6826`) and behave better with :kbd:`backspace` (:issue:`6951`).
-  ``functions -t`` works like the long option ``--handlers-type``, as documented, instead of producing an error (:issue:`6985`).
-  History search now flashes when it found no more results (:issue:`7362`)
-  fish now creates the path in the environment variable ``XDG_RUNTIME_DIR`` if it does not exist, before using it for runtime data storage (:issue:`7335`).
-  ``set_color --print-colors`` now also respects the bold, dim, underline, reverse, italic and background modifiers, to better show their effect (:issue:`7314`).
-  The fish Web configuration tool (``fish_config``) shows prompts correctly on Termux for Android (:issue:`7298`) and detects Windows Services for Linux 2 properly (:issue:`7027`). It no longer shows the ``history`` variable as it may be too large (one can use the History tab instead). It also starts the browser in another thread, avoiding hangs in some circumstances, especially with Firefox's Developer Edition (:issue:`7158`). Finally, a bug in the Source Code Pro font may cause browsers to hang, so this font is no longer chosen by default (:issue:`7714`).
-  ``funcsave`` gained a new ``--directory`` option to specify the location of the saved function (:issue:`7041`).
-  ``help`` works properly on MSYS2 (:issue:`7113`) and only uses ``cmd.exe`` if running on WSL (:issue:`6797`).
-  Resuming a piped job by its number, like ``fg %1``, works correctly (:issue:`7406`). Resumed jobs show the correct title in the terminal emulator (:issue:`7444`).
-  Commands run from key bindings now use the same TTY modes as normal commands (:issue:`7483`).
-  Autosuggestions from history are now case-sensitive (:issue:`3978`).
-  ``$status`` from completion scripts is no longer passed outside the completion, which keeps the status display in the prompt as the last command's status (:issue:`7555`).
-  Updated localisations for pt_BR (:issue:`7480`).
-  ``fish_trace`` output now starts with ``->`` (like ``fish --profile``), making the depth more visible (:issue:`7538`).
-  Resizing the terminal window no longer produces a corrupted prompt (:issue:`6532`, :issue:`7404`).
-  ``functions`` produces an error rather than crashing on certain invalid arguments (:issue:`7515`).
-  A crash in completions with inline variable assignment (eg ``A= b``) has been fixed (:issue:`7344`).
-  ``fish_private_mode`` may now be changed dynamically using ``set`` (:issue:`7589`), and history is kept in memory in private mode (but not stored permanently) (:issue:`7590`).
-  Commands with leading spaces may be retrieved from history with up-arrow until a new command is run, matching zsh's ``HIST_IGNORE_SPACE`` (:issue:`1383`).
-  Importing bash history or reporting errors with recursive globs (``**``) no longer hangs (:issue:`7407`, :issue:`7497`).
-  ``bind`` now shows ``\x7f`` for the del key instead of a literal DEL character (:issue:`7631`)
-  Paths containing variables or tilde expansion are only suggested when they are still valid (:issue:`7582`).
-  Syntax highlighting can now color a command as invalid even if executed quickly (:issue:`5912`).
-  Redirection targets are no longer highlighted as error if they contain variables which will likely be defined by the current commandline (:issue:`6654`).
-  fish is now more resilient against broken terminal modes (:issue:`7133`, :issue:`4873`).
-  fish handles being in control of the TTY without owning its own process group better, avoiding some hangs in special configurations (:issue:`7388`).
-  Keywords can now be colored differently by setting the ``fish_color_keyword`` variable (``fish_color_command`` is used as a fallback) (:issue:`7678`).
-  Just like ``fish_indent``, the interactive reader will indent continuation lines that follow a line ending in a backslash, ``|``, ``&&`` or ``||`` (:issue:`7694`).
-  Commands with a trailing escaped space are saved in history correctly (:issue:`7661`).
-  ``fish_prompt`` no longer mangles Unicode characters in the private-use range U+F600-U+F700. (:issue:`7723`).
-  The universal variable file, ``fish_variables``, can be made a symbolic link without it being overwritten (:issue:`7466`).
-  fish is now more resilient against ``mktemp`` failing (:issue:`7482`).


New or improved bindings
^^^^^^^^^^^^^^^^^^^^^^^^

-  As mentioned above, new special input functions ``undo`` (:kbd:`ctrl-_` or :kbd:`ctrl-z`) and ``redo`` (:kbd:`alt-/`) can be used to revert changes to the command line or the pager search field (:issue:`6570`).
-  :kbd:`ctrl-z` is now available for binding (:issue:`7152`).
-  Additionally, using the ``cancel`` special input function (bound to :kbd:`escape` by default) right after fish picked an unambiguous completion will undo that (:issue:`7433`).
- ``fish_clipboard_paste`` (:kbd:`ctrl-v`) trims indentation from multiline commands, because fish already indents (:issue:`7662`).
-  Vi mode bindings now support ``dh``, ``dl``, ``c0``, ``cf``, ``ct``, ``cF``, ``cT``, ``ch``, ``cl``, ``y0``, ``ci``, ``ca``, ``yi``, ``ya``, ``di``, ``da``, ``d;``, ``d,``, ``o``, ``O`` and Control+left/right keys to navigate by word (:issue:`6648`, :issue:`6755`, :issue:`6769`, :issue:`7442`, :issue:`7516`).
-  Vi mode bindings support :kbd:`~` (tilde) to toggle the case of the selected character (:issue:`6908`).
-  Functions ``up-or-search`` and ``down-or-search`` (:kbd:`up` and :kbd:`down`) can cross empty lines, and don't activate search mode if the search fails, which makes them easier to use to move between lines in some situations.
-  If history search fails to find a match, the cursor is no longer moved. This is useful when accidentally starting a history search on a multi-line commandline.
-  The special input function ``beginning-of-history`` (:kbd:`pageup`) now moves to the oldest search instead of the youngest - that's ``end-of-history`` (:kbd:`pagedown`).
-  A new special input function ``forward-single-char`` moves one character to the right, and if an autosuggestion is available, only take a single character from it (:issue:`7217`, :issue:`4984`).
-  Special input functions can now be joined with ``or`` as a modifier (adding to ``and``), though only some commands set an exit status (:issue:`7217`). This includes ``suppress-autosuggestion`` to reflect whether an autosuggestion was suppressed (:issue:`1419`)
-  A new function ``__fish_preview_current_file``, bound to :kbd:`alt-o`, opens the
   current file at the cursor in a pager (:issue:`6838`, :issue:`6855`).
-  ``edit_command_buffer`` (:kbd:`alt-e` and :kbd:`alt-v`) passes the cursor position
   to the external editor if the editor is recognized (:issue:`6138`, :issue:`6954`).
-  ``__fish_prepend_sudo`` (:kbd:`alt-s`) now toggles a ``sudo`` prefix (:issue:`7012`) and avoids shifting the cursor (:issue:`6542`).
-  ``__fish_prepend_sudo`` (:kbd:`alt-s`) now uses the previous commandline if the current one is empty,
   to simplify rerunning the previous command with ``sudo`` (:issue:`7079`).
-  ``__fish_toggle_comment_commandline`` (:kbd:`alt-#`) now uncomments and presents the last comment
   from history if the commandline is empty (:issue:`7137`).
-  ``__fish_whatis_current_token`` (:kbd:`alt-w`) prints descriptions for functions and builtins (:issue:`7191`, :issue:`2083`).
-  The definition of "word" and "bigword" for movements was refined, fixing (eg) vi mode's behavior with :kbd:`e` on the second-to-last char, and bigword's behavior with single-character words and non-blank non-graphical characters (:issue:`7353`, :issue:`7354`, :issue:`4025`, :issue:`7328`, :issue:`7325`)
-  fish's clipboard bindings now also support Windows Subsystem for Linux via PowerShell and clip.exe (:issue:`7455`, :issue:`7458`) and will properly copy newlines in multi-line commands.
-  Using the ``*-jump`` special input functions before typing anything else no longer crashes fish.
-  Completing variable overrides (``foo=bar``) could replace the entire thing with just the completion in some circumstances. This has been fixed (:issue:`7398`).

Improved prompts
^^^^^^^^^^^^^^^^

-  The default and example prompts print the correct exit status for
   commands prefixed with ``not`` (:issue:`6566`).
-  git prompts include all untracked files in the repository, not just those in the current
   directory (:issue:`6086`).
-  The git prompts correctly show stash states (:issue:`6876`, :issue:`7136`) and clean states (:issue:`7471`).
-  The Mercurial prompt correctly shows untracked status (:issue:`6906`), and by default only shows the branch for performance reasons.
   A new variable ``$fish_prompt_hg_show_informative_status`` can be set to enable more information.
-  The ``fish_vcs_prompt`` passes its arguments to the various VCS prompts that it calls (:issue:`7033`).
-  The Subversion prompt was broken in a number of ways in 3.1.0 and has been restored (:issue:`6715`, :issue:`7278`).
-  A new helper function ``fish_is_root_user`` simplifies checking for superuser privilege (:issue:`7031`, :issue:`7123`).
-  New colorschemes - ``ayu Light``, ``ayu Dark`` and ``ayu Mirage`` (:issue:`7596`).
-  Bugs related to multiline prompts, including repainting (:issue:`5860`) or navigating directory history (:issue:`3550`) leading to graphical glitches have been fixed.
-  The ``nim`` prompt now handles vi mode better (:issue:`6802`)

Improved terminal support
^^^^^^^^^^^^^^^^^^^^^^^^^

-  A new variable, ``fish_vi_force_cursor``, can be set to force ``fish_vi_cursor`` to attempt changing the cursor
   shape in vi mode, regardless of terminal (:issue:`6968`). The ``fish_vi_cursor`` option ``--force-iterm`` has been deprecated.
-  ``diff`` will now colourize output, if supported (:issue:`7308`).
-  Autosuggestions appear when the cursor passes the right prompt (:issue:`6948`) or wraps to the next line (:issue:`7213`).
-  The cursor shape in Vi mode changes properly in Windows Terminal (:issue:`6999`, :issue:`6478`).
-  The spurious warning about terminal size in small terminals has been removed (:issue:`6980`).
-  Dynamic titles are now enabled in Alacritty (:issue:`7073`) and emacs' vterm (:issue:`7122`).
-  Current working directory updates are enabled in foot (:issue:`7099`) and WezTerm (:issue:`7649`).
-  The width computation for certain emoji agrees better with terminals (especially flags). (:issue:`7237`).
-  Long command lines are wrapped in all cases, instead of sometimes being put on a new line (:issue:`5118`).
-  The pager is properly rendered with long command lines selected (:issue:`2557`).
-  Sessions with right prompts can be resized correctly in terminals that handle reflow, like GNOME Terminal (and other VTE-based terminals), upcoming Konsole releases and Alacritty. This detection can be overridden with the new ``fish_handle_reflow`` variable (:issue:`7491`).
-  fish now sets terminal modes sooner, which stops output from appearing before the greeting and prompt are ready (:issue:`7489`).
-  Better detection of new Konsole versions for true color support and cursor shape changing.
-  fish no longer attempts to modify the terminal size via ``TIOCSWINSZ``, improving compatibility with Kitty (:issue:`6994`).

Completions
^^^^^^^^^^^

-  Added completions for

   -  ``7z``, ``7za`` and ``7zr`` (:issue:`7220`)
   -  ``alias`` (:issue:`7035`)
   -  ``alternatives`` (:issue:`7616`)
   -  ``apk`` (:issue:`7108`)
   -  ``asciidoctor`` (:issue:`7000`)
   -  ``avifdec`` and ``avifenc`` (:issue:`7674`)
   -  ``bluetoothctl`` (:issue:`7438`)
   -  ``cjxl`` and ``djxl`` (:issue:`7673`)
   -  ``cmark`` (:issue:`7000`)
   -  ``create_ap`` (:issue:`7096`)
   -  ``deno`` (:issue:`7138`)
   -  ``dhclient`` (:issue:`6684`)
   -  Postgres-related commands ``dropdb``, ``createdb``, ``pg_restore``, ``pg_dump`` and
      ``pg_dumpall`` (:issue:`6620`)
   -  ``dotnet`` (:issue:`7558`)
   -  ``downgrade`` (:issue:`6751`)
   -  ``gapplication``, ``gdbus``, ``gio`` and ``gresource`` (:issue:`7300`)
   -  ``gh`` (:issue:`7112`)
   -  ``gitk``
   -  ``groups`` (:issue:`6889`)
   -  ``hashcat`` (:issue:`7746`)
   -  ``hikari`` (:issue:`7083`)
   -  ``icdiff`` (:issue:`7503`)
   -  ``imv`` (:issue:`6675`)
   -  ``john`` (:issue:`7746`)
   -  ``julia`` (:issue:`7468`)
   -  ``k3d`` (:issue:`7202`)
   -  ``ldapsearch`` (:issue:`7578`)
   -  ``lightdm`` and ``dm-tool`` (:issue:`7624`)
   -  ``losetup`` (:issue:`7621`)
   -  ``micro`` (:issue:`7339`)
   -  ``mpc`` (:issue:`7169`)
   -  Metasploit's ``msfconsole``, ``msfdb`` and ``msfvenom`` (:issue:`6930`)
   -  ``mtr`` (:issue:`7638`)
   -  ``mysql`` (:issue:`6819`)
   -  ``ncat``, ``nc.openbsd``, ``nc.traditional`` and ``nmap`` (:issue:`6873`)
   -  ``openssl`` (:issue:`6845`)
   -  ``prime-run`` (:issue:`7241`)
   -  ``ps2pdf{12,13,14,wr}`` (:issue:`6673`)
   -  ``pyenv`` (:issue:`6551`)
   -  ``rst2html``, ``rst2html4``, ``rst2html5``, ``rst2latex``,
      ``rst2man``, ``rst2odt``, ``rst2pseudoxml``, ``rst2s5``,
      ``rst2xetex``, ``rst2xml`` and ``rstpep2html`` (:issue:`7019`)
   -  ``spago`` (:issue:`7381`)
   -  ``sphinx-apidoc``, ``sphinx-autogen``, ``sphinx-build`` and
      ``sphinx-quickstart`` (:issue:`7000`)
   -  ``strace`` (:issue:`6656`)
   -  systemd's ``bootctl``, ``coredumpctl``, ``hostnamectl`` (:issue:`7428`), ``homectl`` (:issue:`7435`), ``networkctl`` (:issue:`7668`) and ``userdbctl`` (:issue:`7667`)
   -  ``tcpdump`` (:issue:`6690`)
   -  ``tig``
   -  ``traceroute`` and ``tracepath`` (:issue:`6803`)
   -  ``windscribe`` (:issue:`6788`)
   -  ``wireshark``, ``tshark``, and ``dumpcap``
   -  ``xbps-*`` (:issue:`7239`)
   -  ``xxhsum``, ``xxh32sum``, ``xxh64sum`` and ``xxh128sum`` (:issue:`7103`)
   -  ``yadm`` (:issue:`7100`)
   -  ``zopfli`` and ``zopflipng`` (:issue:`6872`)

-  Lots of improvements to completions, including:

   -  ``git`` completions can complete the right and left parts of a commit range like ``from..to`` or ``left...right``.
   -  Completion scripts for custom Git subcommands like ``git-xyz`` are now loaded with Git completions. The completions can now be defined directly on the subcommand (using ``complete git-xyz``), and completion for ``git xyz`` will work. (:issue:`7075`, :issue:`7652`, :issue:`4358`)
   -  ``make`` completions no longer second-guess make's file detection, fixing target completion in some cases (:issue:`7535`).
   -  Command completions now correctly print the description even if the command was fully matched (like in ``ls<TAB>``).
   -  ``set`` completions no longer hide variables starting with ``__``, they are sorted last instead.

-  Improvements to the manual page completion generator (:issue:`7086`, :issue:`6879`, :issue:`7187`).
-  Significant performance improvements to completion of the available commands (:issue:`7153`), especially on macOS Big Sur where there was a significant regression (:issue:`7365`, :issue:`7511`).
-  Suffix completion using ``__fish_complete_suffix`` uses the same fuzzy matching logic as normal file completion, and completes any file but sorts files with matching suffix first (:issue:`7040`, :issue:`7547`). Previously, it only completed files with matching suffix.

For distributors
----------------

-  fish has a new interactive test driver based on pexpect, removing the optional dependency on expect (and adding an optional dependency on pexpect) (:issue:`5451`, :issue:`6825`).
-  The CHANGELOG was moved to restructured text, allowing it to be included in the documentation (:issue:`7057`).
-  fish handles ncurses installed in a non-standard prefix better (:issue:`6600`, :issue:`7219`), and uses variadic tparm on NetBSD curses (:issue:`6626`).
-  The Web-based configuration tool no longer uses an obsolete Angular version (:issue:`7147`).
-  The fish project has adopted the Contributor Covenant code of conduct (:issue:`7151`).

Deprecations and removed features
---------------------------------

-  The ``fish_color_match`` variable is no longer used. (Previously this controlled the color of matching quotes and parens when using ``read``).
-  fish 3.2.0 will be the last release in which the redirection to standard error with the ``^`` character is enabled.
   The ``stderr-nocaret`` feature flag will be changed to "on" in future releases.
-  ``string`` is now a reserved word and cannot be used for function names (see above).
-  ``fish_vi_cursor``'s option ``--force-iterm`` has been deprecated (see above).
-  ``command``, ``jobs`` and ``type`` long-form option ``--quiet`` is deprecated in favor of ``--query`` (see above).
-  The ``fish_command_not_found`` event is no longer emitted, instead there is a function of that name.
   By default it will call a previously-defined ``__fish_command_not_found_handler``. To emit the event manually use ``emit fish_command_not_found``.
-  The ``fish_prompt`` event no longer fires when ``read`` is used. If
   you need a function to run any time ``read`` is invoked by a script,
   use the new ``fish_read`` event instead (:issue:`7039`).
-  To disable the greeting message permanently it is no longer enough to just run ``set fish_greeting`` interactively as it is
   no longer implicitly a universal variable. Use ``set -U fish_greeting`` or disable it in config.fish with ``set -g fish_greeting``.
-  The long-deprecated and non-functional ``-m``/``--read-mode`` options to ``read`` were removed in 3.1b1. Using the short form, or a never-implemented ``-B`` option, no longer crashes fish (:issue:`7659`).
-  With the addition of new categories for debug options, the old numbered debugging levels have been removed.

For distributors and developers
-------------------------------

-  fish source tarballs are now distributed using the XZ compression
   method (:issue:`5460`).
-  The fish source tarball contains an example FreeDesktop entry and icon.
-  The CMake variable ``MAC_CODESIGN_ID`` can now be set to "off" to disable code-signing (:issue:`6952`, :issue:`6792`).
-  Building on on macOS earlier than 10.13.6 succeeds, instead of failing on code-signing (:issue:`6791`).
-  The pkg-config file now uses variables to ensure paths used are portable across prefixes.
-  The default values for the ``extra_completionsdir``, ``extra_functionsdir``
   and ``extra_confdir`` options now use the installation prefix rather than ``/usr/local`` (:issue:`6778`).
-  A new CMake variable ``FISH_USE_SYSTEM_PCRE2`` controls whether fish
   builds with the system-installed PCRE2, or the version it bundles. By
   default it prefers the system library if available, unless Mac
   codesigning is enabled (:issue:`6952`).
-  Running the full interactive test suite now requires Python 3.5+ and the pexpect package (:issue:`6825`); the expect package is no longer required.
-  Support for Python 2 in fish's tools (``fish_config`` and the manual page completion generator) is no longer guaranteed. Please use Python 3.5 or later (:issue:`6537`).
-  The Web-based configuration tool is compatible with Python 3.10  (:issue:`7600`) and no longer requires Python's distutils package (:issue:`7514`).
-  fish 3.2 is the last release to support Red Hat Enterprise Linux & CentOS version 6.

--------------

fish 3.1.2 (released April 29, 2020)
====================================

This release of fish fixes a major issue discovered in fish 3.1.1:

-  Commands such as ``fzf`` and ``enhancd``, when used with ``eval``,
   would hang. ``eval`` buffered output too aggressively, which has been
   fixed (:issue:`6955`).

If you are upgrading from version 3.0.0 or before, please also review
the release notes for 3.1.1, 3.1.0 and 3.1b1 (included below).

--------------

fish 3.1.1 (released April 27, 2020)
====================================

This release of fish fixes a number of major issues discovered in fish
3.1.0.

-  Commands which involve ``. ( ... | psub)`` now work correctly, as a
   bug in the ``function --on-job-exit`` option has been fixed (:issue:`6613`).
-  Conflicts between upstream packages for ripgrep and bat, and the fish
   packages, have been resolved (:issue:`5822`).
-  Starting fish in a directory without read access, such as via ``su``,
   no longer crashes (:issue:`6597`).
-  Glob ordering changes which were introduced in 3.1.0 have been
   reverted, returning the order of globs to the previous state (:issue:`6593`).
-  Redirections using the deprecated caret syntax to a file descriptor
   (eg ``^&2``) work correctly (:issue:`6591`).
-  Redirections that append to a file descriptor (eg ``2>>&1``) work
   correctly (:issue:`6614`).
-  Building fish on macOS (:issue:`6602`) or with new versions of GCC (:issue:`6604`,
   :issue:`6609`) is now successful.
-  ``time`` is now correctly listed in the output of ``builtin -n``, and
   ``time --help`` works correctly (:issue:`6598`).
-  Exported universal variables now update properly (:issue:`6612`).
-  ``status current-command`` gives the expected output when used with
   an environment override - that is, ``F=B status current-command``
   returns ``status`` instead of ``F=B`` (:issue:`6635`).
-  ``test`` no longer crashes when used with “``nan``” or “``inf``”
   arguments, erroring out instead (:issue:`6655`).
-  Copying from the end of the command line no longer crashes fish
   (:issue:`6680`).
-  ``read`` no longer removes multiple separators when splitting a
   variable into a list, restoring the previous behaviour from fish 3.0
   and before (:issue:`6650`).
-  Functions using ``--on-job-exit`` and ``--on-process-exit`` work
   reliably again (:issue:`6679`).
-  Functions using ``--on-signal INT`` work reliably in interactive
   sessions, as they did in fish 2.7 and before (:issue:`6649`). These handlers
   have never worked in non-interactive sessions, and making them work
   is an ongoing process.
-  Functions using ``--on-variable`` work reliably with variables which
   are set implicitly (rather than with ``set``), such as
   “``fish_bind_mode``” and “``PWD``” (:issue:`6653`).
-  256 colors are properly enabled under certain conditions that were
   incorrectly detected in fish 3.1.0 (``$TERM`` begins with xterm, does
   not include “``256color``”, and ``$TERM_PROGRAM`` is not set)
   (:issue:`6701`).
-  The Mercurial (``hg``) prompt no longer produces an error when the
   current working directory is removed (:issue:`6699`). Also, for performance
   reasons it shows only basic information by default; to restore the
   detailed status, set ``$fish_prompt_hg_show_informative_status``.
-  The VCS prompt, ``fish_vcs_prompt``, no longer displays Subversion
   (``svn``) status by default, due to the potential slowness of this
   operation (:issue:`6681`).
-  Pasting of commands has been sped up (:issue:`6713`).
-  Using extended Unicode characters, such as emoji, in a non-Unicode
   capable locale (such as the ``C`` or ``POSIX`` locale) no longer
   renders all output blank (:issue:`6736`).
-  ``help`` prefers to use ``xdg-open``, avoiding the use of ``open`` on
   Debian systems where this command is actually ``openvt`` (:issue:`6739`).
-  Command lines starting with a space, which are not saved in history,
   now do not get autosuggestions. This fixes an issue with Midnight
   Commander integration (:issue:`6763`), but may be changed in a future
   version.
-  Copying to the clipboard no longer inserts a newline at the end of
   the content, matching fish 2.7 and earlier (:issue:`6927`).
-  ``fzf`` in complex pipes no longer hangs. More generally, code run as
   part of command substitutions or ``eval`` will no longer have
   separate process groups. (:issue:`6624`, :issue:`6806`).

This release also includes:

-  several changes to improve macOS compatibility with code signing
   and notarization;
-  several improvements to completions; and
-  several content and formatting improvements to the documentation.

If you are upgrading from version 3.0.0 or before, please also review
the release notes for 3.1.0 and 3.1b1 (included below).

Errata for fish 3.1
-------------------

A new builtin, ``time``, was introduced in the fish 3.1 releases. This
builtin is a reserved word (like ``test``, ``function``, and others)
because of the way it is implemented, and functions can no longer be
named ``time``. This was not clear in the fish 3.1b1 changelog.

--------------

fish 3.1.0 (released February 12, 2020)
=======================================

Compared to the beta release of fish 3.1b1, fish version 3.1.0:

-  Fixes a regression where spaces after a brace were removed despite
   brace expansion not occurring (:issue:`6564`).
-  Fixes a number of problems in compiling and testing on Cygwin
   (:issue:`6549`) and Solaris-derived systems such as Illumos (:issue:`6553`, :issue:`6554`,
   :issue:`6555`, :issue:`6556`, and :issue:`6558`).
-  Fixes the process for building macOS packages.
-  Fixes a regression where excessive error messages are printed if
   Unicode characters are emitted in non-Unicode-capable locales
   (:issue:`6584`).
-  Contains some improvements to the documentation and a small number
   of completions.

If you are upgrading from version 3.0.0 or before, please also review
the release notes for 3.1b1 (included below).

--------------

fish 3.1b1 (released January 26, 2020)
======================================

.. _notable-improvements-and-fixes-1:

Notable improvements and fixes
------------------------------

-  A new ``$pipestatus`` variable contains a list of exit statuses of
   the previous job, for each of the separate commands in a pipeline
   (:issue:`5632`).
-  fish no longer buffers pipes to the last function in a pipeline,
   improving many cases where pipes appeared to block or hang (:issue:`1396`).
-  An overhaul of error messages for builtin commands, including a
   removal of the overwhelming usage summary, more readable stack traces
   (:issue:`3404`, :issue:`5434`), and stack traces for ``test`` (aka ``[``) (:issue:`5771`).
-  fish’s debugging arguments have been significantly improved. The
   ``--debug-level`` option has been removed, and a new ``--debug``
   option replaces it. This option accepts various categories, which may
   be listed via ``fish --print-debug-categories`` (:issue:`5879`). A new
   ``--debug-output`` option allows for redirection of debug output.
-  ``string`` has a new ``collect`` subcommand for use in command
   substitutions, producing a single output instead of splitting on new
   lines (similar to ``"$(cmd)"`` in other shells) (:issue:`159`).
-  The fish manual, tutorial and FAQ are now available in ``man`` format
   as ``fish-doc``, ``fish-tutorial`` and ``fish-faq`` respectively
   (:issue:`5521`).
-  Like other shells, ``cd`` now always looks for its argument in the
   current directory as a last resort, even if the ``CDPATH`` variable
   does not include it or “.” (:issue:`4484`).
-  fish now correctly handles ``CDPATH`` entries that start with ``..``
   (:issue:`6220`) or contain ``./`` (:issue:`5887`).
-  The ``fish_trace`` variable may be set to trace execution (:issue:`3427`).
   This performs a similar role as ``set -x`` in other shells.
-  fish uses the temporary directory determined by the system, rather
   than relying on ``/tmp`` (:issue:`3845`).
-  The fish Web configuration tool (``fish_config``) prints a list of
   commands it is executing, to help understanding and debugging
   (:issue:`5584`).
-  Major performance improvements when pasting (:issue:`5866`), executing lots
   of commands (:issue:`5905`), importing history from bash (:issue:`6295`), and when
   completing variables that might match ``$history`` (:issue:`6288`).

.. _syntax-changes-and-new-commands-1:

Syntax changes and new commands
-------------------------------

-  A new builtin command, ``time``, which allows timing of fish
   functions and builtins as well as external commands (:issue:`117`).
-  Brace expansion now only takes place if the braces include a “,” or a
   variable expansion, meaning common commands such as
   ``git reset HEAD@{0}`` do not require escaping (:issue:`5869`).
-  New redirections ``&>`` and ``&|`` may be used to redirect or pipe
   stdout, and also redirect stderr to stdout (:issue:`6192`).
-  ``switch`` now allows arguments that expand to nothing, like empty
   variables (:issue:`5677`).
-  The ``VAR=val cmd`` syntax can now be used to run a command in a
   modified environment (:issue:`6287`).
-  ``and`` is no longer recognised as a command, so that nonsensical
   constructs like ``and and and`` produce a syntax error (:issue:`6089`).
-  ``math``\ ‘s exponent operator,’\ ``^``\ ‘, was previously
   left-associative, but now uses the more commonly-used
   right-associative behaviour (:issue:`6280`). This means that
   ``math '3^0.5^2'`` was previously calculated as’(3\ :sup:`0.5)`\ 2’,
   but is now calculated as ‘3\ :sup:`(0.5`\ 2)’.
-  In fish 3.0, the variable used with ``for`` loops inside command
   substitutions could leak into enclosing scopes; this was an
   inadvertent behaviour change and has been reverted (:issue:`6480`).

.. _scripting-improvements-1:

Scripting improvements
----------------------

-  ``string split0`` now returns 0 if it split something (:issue:`5701`).
-  In the interest of consistency, ``builtin -q`` and ``command -q`` can
   now be used to query if a builtin or command exists (:issue:`5631`).
-  ``math`` now accepts ``--scale=max`` for the maximum scale (:issue:`5579`).
-  ``builtin $var`` now works correctly, allowing a variable as the
   builtin name (:issue:`5639`).
-  ``cd`` understands the ``--`` argument to make it possible to change
   to directories starting with a hyphen (:issue:`6071`).
-  ``complete --do-complete`` now also does fuzzy matches (:issue:`5467`).
-  ``complete --do-complete`` can be used inside completions, allowing
   limited recursion (:issue:`3474`).
-  ``count`` now also counts lines fed on standard input (:issue:`5744`).
-  ``eval`` produces an exit status of 0 when given no arguments, like
   other shells (:issue:`5692`).
-  ``printf`` prints what it can when input hasn’t been fully converted
   to a number, but still prints an error (:issue:`5532`).
-  ``complete -C foo`` now works as expected, rather than requiring
   ``complete -Cfoo``.
-  ``complete`` has a new ``--force-files`` option, to re-enable file
   completions. This allows ``sudo -E`` and ``pacman -Qo`` to complete
   correctly (:issue:`5646`).
-  ``argparse`` now defaults to showing the current function name
   (instead of ``argparse``) in its errors, making ``--name`` often
   superfluous (:issue:`5835`).
-  ``argparse`` has a new ``--ignore-unknown`` option to keep
   unrecognized options, allowing multiple argparse passes to parse
   options (:issue:`5367`).
-  ``argparse`` correctly handles flag value validation of options that
   only have short names (:issue:`5864`).
-  ``read -S`` (short option of ``--shell``) is recognised correctly
   (:issue:`5660`).
-  ``read`` understands ``--list``, which acts like ``--array`` in
   reading all arguments into a list inside a single variable, but is
   better named (:issue:`5846`).
-  ``read`` has a new option, ``--tokenize``, which splits a string into
   variables according to the shell’s tokenization rules, considering
   quoting, escaping, and so on (:issue:`3823`).
-  ``read`` interacts more correctly with the deprecated ``$IFS``
   variable, in particular removing multiple separators when splitting a
   variable into a list (:issue:`6406`), matching other shells.
-  ``fish_indent`` now handles semicolons better, including leaving them
   in place for ``; and`` and ``; or`` instead of breaking the line
   (:issue:`5859`).
-  ``fish_indent --write`` now supports multiple file arguments,
   indenting them in turn.
-  The default read limit has been increased to 100MiB (:issue:`5267`).
-  ``math`` now also understands ``x`` for multiplication, provided it
   is followed by whitespace (:issue:`5906`).
-  ``math`` reports the right error when incorrect syntax is used inside
   parentheses (:issue:`6063`), and warns when unsupported logical operations
   are used (:issue:`6096`).
-  ``functions --erase`` now also prevents fish from autoloading a
   function for the first time (:issue:`5951`).
-  ``jobs --last`` returns 0 to indicate success when a job is found
   (:issue:`6104`).
-  ``commandline -p`` and ``commandline -j`` now split on ``&&`` and
   ``||`` in addition to ``;`` and ``&`` (:issue:`6214`).
-  A bug where ``string split`` would drop empty strings if the output
   was only empty strings has been fixed (:issue:`5987`).
-  ``eval`` no long creates a new local variable scope, but affects
   variables in the scope it is called from (:issue:`4443`). ``source`` still
   creates a new local scope.
-  ``abbr`` has a new ``--query`` option to check for the existence of
   an abbreviation.
-  Local values for ``fish_complete_path`` and ``fish_function_path``
   are now ignored; only their global values are respected.
-  Syntax error reports now display a marker in the correct position
   (:issue:`5812`).
-  Empty universal variables may now be exported (:issue:`5992`).
-  Exported universal variables are no longer imported into the global
   scope, preventing shadowing. This makes it easier to change such
   variables for all fish sessions and avoids breakage when the value is
   a list of multiple elements (:issue:`5258`).
-  A bug where ``for`` could use invalid variable names has been fixed
   (:issue:`5800`).
-  A bug where local variables would not be exported to functions has
   been fixed (:issue:`6153`).
-  The null command (``:``) now always exits successfully, rather than
   passing through the previous exit status (:issue:`6022`).
-  The output of ``functions FUNCTION`` matches the declaration of the
   function, correctly including comments or blank lines (:issue:`5285`), and
   correctly includes any ``--wraps`` flags (:issue:`1625`).
-  ``type`` supports a new option, ``--short``, which suppress function
   expansion (:issue:`6403`).
-  ``type --path`` with a function argument will now output the path to
   the file containing the definition of that function, if it exists.
-  ``type --force-path`` with an argument that cannot be found now
   correctly outputs nothing, as documented (:issue:`6411`).
-  The ``$hostname`` variable is no longer truncated to 32 characters
   (:issue:`5758`).
-  Line numbers in function backtraces are calculated correctly (:issue:`6350`).
-  A new ``fish_cancel`` event is emitted when the command line is
   cancelled, which is useful for terminal integration (:issue:`5973`).

.. _interactive-improvements-1:

Interactive improvements
------------------------

-  New Base16 color options are available through the Web-based
   configuration (:issue:`6504`).
-  fish only parses ``/etc/paths`` on macOS in login shells, matching
   the bash implementation (:issue:`5637`) and avoiding changes to path ordering
   in child shells (:issue:`5456`). It now ignores blank lines like the bash
   implementation (:issue:`5809`).
-  The locale is now reloaded when the ``LOCPATH`` variable is changed
   (:issue:`5815`).
-  ``read`` no longer keeps a history, making it suitable for operations
   that shouldn’t end up there, like password entry (:issue:`5904`).
-  ``dirh`` outputs its stack in the correct order (:issue:`5477`), and behaves
   as documented when universal variables are used for its stack
   (:issue:`5797`).
-  ``funced`` and the edit-commandline-in-buffer bindings did not work
   in fish 3.0 when the ``$EDITOR`` variable contained spaces; this has
   been corrected (:issue:`5625`).
-  Builtins now pipe their help output to a pager automatically (:issue:`6227`).
-  ``set_color`` now colors the ``--print-colors`` output in the
   matching colors if it is going to a terminal.
-  fish now underlines every valid entered path instead of just the last
   one (:issue:`5872`).
-  When syntax highlighting a string with an unclosed quote, only the
   quote itself will be shown as an error, instead of the whole
   argument.
-  Syntax highlighting works correctly with variables as commands
   (:issue:`5658`) and redirections to close file descriptors (:issue:`6092`).
-  ``help`` works properly on Windows Subsytem for Linux (:issue:`5759`, :issue:`6338`).
-  A bug where ``disown`` could crash the shell has been fixed (:issue:`5720`).
-  fish will not autosuggest files ending with ``~`` unless there are no
   other candidates, as these are generally backup files (:issue:`985`).
-  Escape in the pager works correctly (:issue:`5818`).
-  Key bindings that call ``fg`` no longer leave the terminal in a
   broken state (:issue:`2114`).
-  Brackets (:issue:`5831`) and filenames containing ``$`` (:issue:`6060`) are completed
   with appropriate escaping.
-  The output of ``complete`` and ``functions`` is now colorized in
   interactive terminals.
-  The Web-based configuration handles aliases that include single
   quotes correctly (:issue:`6120`), and launches correctly under Termux (:issue:`6248`)
   and OpenBSD (:issue:`6522`).
-  ``function`` now correctly validates parameters for
   ``--argument-names`` as valid variable names (:issue:`6147`) and correctly
   parses options following ``--argument-names``, as in
   “``--argument-names foo --description bar``” (:issue:`6186`).
-  History newly imported from bash includes command lines using ``&&``
   or ``||``.
-  The automatic generation of completions from manual pages is better
   described in job and process listings, and no longer produces a
   warning when exiting fish (:issue:`6269`).
-  In private mode, setting ``$fish_greeting`` to an empty string before
   starting the private session will prevent the warning about history
   not being saved from being printed (:issue:`6299`).
-  In the interactive editor, a line break (Enter) inside unclosed
   brackets will insert a new line, rather than executing the command
   and producing an error (:issue:`6316`).
-  Ctrl-C always repaints the prompt (:issue:`6394`).
-  When run interactively from another program (such as Python), fish
   will correctly start a new process group, like other shells (:issue:`5909`).
-  Job identifiers (for example, for background jobs) are assigned more
   logically (:issue:`6053`).
-  A bug where history would appear truncated if an empty command was
   executed was fixed (:issue:`6032`).

.. _new-or-improved-bindings-1:

New or improved bindings
^^^^^^^^^^^^^^^^^^^^^^^^

-  Pasting strips leading spaces to avoid pasted commands being omitted
   from the history (:issue:`4327`).
-  Shift-Left and Shift-Right now default to moving backwards and
   forwards by one bigword (words separated by whitespace) (:issue:`1505`).
-  The default escape delay (to differentiate between the escape key and
   an alt-combination) has been reduced to 30ms, down from 300ms for the
   default mode and 100ms for Vi mode (:issue:`3904`).
-  The ``forward-bigword`` binding now interacts correctly with
   autosuggestions (:issue:`5336`).
-  The ``fish_clipboard_*`` functions support Wayland by using
   `wl-clipboard <https://github.com/bugaevc/wl-clipboard>`_
   (:issue:`5450`).
-  The ``nextd`` and ``prevd`` functions no longer print “Hit end of
   history”, instead using a bell. They correctly store working
   directories containing symbolic links (:issue:`6395`).
-  If a ``fish_mode_prompt`` function exists, Vi mode will only execute
   it on mode-switch instead of the entire prompt. This should make it
   much more responsive with slow prompts (:issue:`5783`).
-  The path-component bindings (like Ctrl-w) now also stop at “:” and
   “@”, because those are used to denote user and host in commands such
   as ``ssh`` (:issue:`5841`).
-  The NULL character can now be bound via ``bind -k nul``. Terminals
   often generate this character via control-space. (:issue:`3189`).
-  A new readline command ``expand-abbr`` can be used to trigger
   abbreviation expansion (:issue:`5762`).
-  A new readline command, ``delete-or-exit``, removes a character to
   the right of the cursor or exits the shell if the command line is
   empty (moving this functionality out of the ``delete-or-exit``
   function).
-  The ``self-insert`` readline command will now insert the binding
   sequence, if not empty.
-  A new binding to prepend ``sudo``, bound to Alt-S by default (:issue:`6140`).
-  The Alt-W binding to describe a command should now work better with
   multiline prompts (:issue:`6110`)
-  The Alt-H binding to open a command’s man page now tries to ignore
   ``sudo`` (:issue:`6122`).
-  A new pair of bind functions, ``history-prefix-search-backward`` (and
   ``forward``), was introduced (:issue:`6143`).
-  Vi mode now supports R to enter replace mode (:issue:`6342`), and ``d0`` to
   delete the current line (:issue:`6292`).
-  In Vi mode, hitting Enter in replace-one mode no longer erases the
   prompt (:issue:`6298`).
-  Selections in Vi mode are inclusive, matching the actual behaviour of
   Vi (:issue:`5770`).

.. _improved-prompts-1:

Improved prompts
^^^^^^^^^^^^^^^^

-  The Git prompt in informative mode now shows the number of stashes if
   enabled.
-  The Git prompt now has an option
   (``$__fish_git_prompt_use_informative_chars``) to use the (more
   modern) informative characters without enabling informative mode.
-  The default prompt now also features VCS integration and will color
   the host if running via SSH (:issue:`6375`).
-  The default and example prompts print the pipe status if an earlier
   command in the pipe fails.
-  The default and example prompts try to resolve exit statuses to
   signal names when appropriate.

.. _improved-terminal-output-1:

Improved terminal output
^^^^^^^^^^^^^^^^^^^^^^^^

-  New ``fish_pager_color_`` options have been added to control more
   elements of the pager’s colors (:issue:`5524`).
-  Better detection and support for using fish from various system
   consoles, where limited colors and special characters are supported
   (:issue:`5552`).
-  fish now tries to guess if the system supports Unicode 9 (and
   displays emoji as wide), eliminating the need to set
   ``$fish_emoji_width`` in most cases (:issue:`5722`).
-  Improvements to the display of wide characters, particularly Korean
   characters and emoji (:issue:`5583`, :issue:`5729`).
-  The Vi mode cursor is correctly redrawn when regaining focus under
   terminals that report focus (eg tmux) (:issue:`4788`).
-  Variables that control background colors (such as
   ``fish_pager_color_search_match``) can now use ``--reverse``.

.. _completions-1:

Completions
^^^^^^^^^^^

-  Added completions for

   -  ``aws``
   -  ``bat`` (:issue:`6052`)
   -  ``bosh`` (:issue:`5700`)
   -  ``btrfs``
   -  ``camcontrol``
   -  ``cf`` (:issue:`5700`)
   -  ``chronyc`` (:issue:`6496`)
   -  ``code`` (:issue:`6205`)
   -  ``cryptsetup`` (:issue:`6488`)
   -  ``csc`` and ``csi`` (:issue:`6016`)
   -  ``cwebp`` (:issue:`6034`)
   -  ``cygpath`` and ``cygstart`` (:issue:`6239`)
   -  ``epkginfo`` (:issue:`5829`)
   -  ``ffmpeg``, ``ffplay``, and ``ffprobe`` (:issue:`5922`)
   -  ``fsharpc`` and ``fsharpi`` (:issue:`6016`)
   -  ``fzf`` (:issue:`6178`)
   -  ``g++`` (:issue:`6217`)
   -  ``gpg1`` (:issue:`6139`)
   -  ``gpg2`` (:issue:`6062`)
   -  ``grub-mkrescue`` (:issue:`6182`)
   -  ``hledger`` (:issue:`6043`)
   -  ``hwinfo`` (:issue:`6496`)
   -  ``irb`` (:issue:`6260`)
   -  ``iw`` (:issue:`6232`)
   -  ``kak``
   -  ``keepassxc-cli`` (:issue:`6505`)
   -  ``keybase`` (:issue:`6410`)
   -  ``loginctl`` (:issue:`6501`)
   -  ``lz4``, ``lz4c`` and ``lz4cat`` (:issue:`6364`)
   -  ``mariner`` (:issue:`5718`)
   -  ``nethack`` (:issue:`6240`)
   -  ``patool`` (:issue:`6083`)
   -  ``phpunit`` (:issue:`6197`)
   -  ``plutil`` (:issue:`6301`)
   -  ``pzstd`` (:issue:`6364`)
   -  ``qubes-gpg-client`` (:issue:`6067`)
   -  ``resolvectl`` (:issue:`6501`)
   -  ``rg``
   -  ``rustup``
   -  ``sfdx`` (:issue:`6149`)
   -  ``speedtest`` and ``speedtest-cli`` (:issue:`5840`)
   -  ``src`` (:issue:`6026`)
   -  ``tokei`` (:issue:`6085`)
   -  ``tsc`` (:issue:`6016`)
   -  ``unlz4`` (:issue:`6364`)
   -  ``unzstd`` (:issue:`6364`)
   -  ``vbc`` (:issue:`6016`)
   -  ``zpaq`` (:issue:`6245`)
   -  ``zstd``, ``zstdcat``, ``zstdgrep``, ``zstdless`` and ``zstdmt``
      (:issue:`6364`)

-  Lots of improvements to completions.
-  Selecting short options which also have a long name from the
   completion pager is possible (:issue:`5634`).
-  Tab completion will no longer add trailing spaces if they already
   exist (:issue:`6107`).
-  Completion of subcommands to builtins like ``and`` or ``not`` now
   works correctly (:issue:`6249`).
-  Completion of arguments to short options works correctly when
   multiple short options are used together (:issue:`332`).
-  Activating completion in the middle of an invalid completion does not
   move the cursor any more, making it easier to fix a mistake (:issue:`4124`).
-  Completion in empty commandlines now lists all available commands.
-  Functions listed as completions could previously leak parts of the
   function as other completions; this has been fixed.

.. _deprecations-and-removed-features-1:

Deprecations and removed features
---------------------------------

-  The vcs-prompt functions have been promoted to names without
   double-underscore, so \__fish_git_prompt is now fish_git_prompt,
   \__fish_vcs_prompt is now fish_vcs_prompt, \__fish_hg_prompt is now
   fish_hg_prompt and \__fish_svn_prompt is now fish_svn_prompt. Shims
   at the old names have been added, and the variables have kept their
   old names (:issue:`5586`).
-  ``string replace`` has an additional round of escaping in the
   replacement expression, so escaping backslashes requires many escapes
   (eg ``string replace -ra '([ab])' '\\\\\\\$1' a``). The new feature
   flag ``regex-easyesc`` can be used to disable this, so that the same
   effect can be achieved with
   ``string replace -ra '([ab])' '\\\\$1' a`` (:issue:`5556`). As a reminder,
   the intention behind feature flags is that this will eventually
   become the default and then only option, so scripts should be
   updated.
-  The ``fish_vi_mode`` function, deprecated in fish 2.3, has been
   removed. Use ``fish_vi_key_bindings`` instead (:issue:`6372`).

.. _for-distributors-and-developers-1:

For distributors and developers
-------------------------------

-  fish 3.0 introduced a CMake-based build system. In fish 3.1, both the
   Autotools-based build and legacy Xcode build system have been
   removed, leaving only the CMake build system. All distributors and
   developers must install CMake.
-  fish now depends on the common ``tee`` external command, for the
   ``psub`` process substitution function.
-  The documentation is now built with Sphinx. The old Doxygen-based
   documentation system has been removed. Developers, and distributors
   who wish to rebuild the documentation, must install Sphinx.
-  The ``INTERNAL_WCWIDTH`` build option has been removed, as fish now
   always uses an internal ``wcwidth`` function. It has a number of
   configuration options that make it more suitable for general use
   (:issue:`5777`).
-  mandoc can now be used to format the output from ``--help`` if
   ``nroff`` is not installed, reducing the number of external
   dependencies on systems with ``mandoc`` installed (:issue:`5489`).
-  Some bugs preventing building on Solaris-derived systems such as
   Illumos were fixed (:issue:`5458`, :issue:`5461`, :issue:`5611`).
-  Completions for ``npm``, ``bower`` and ``yarn`` no longer require the
   ``jq`` utility for full functionality, but will use Python instead if
   it is available.
-  The paths for completions, functions and configuration snippets have
   been extended. On systems that define ``XDG_DATA_DIRS``, each of the
   directories in this variable are searched in the subdirectories
   ``fish/vendor_completions.d``, ``fish/vendor_functions.d``, and
   ``fish/vendor_conf.d`` respectively. On systems that do not define
   this variable in the environment, the vendor directories are searched
   for in both the installation prefix and the default “extra”
   directory, which now defaults to ``/usr/local`` (:issue:`5029`).

--------------

fish 3.0.2 (released February 19, 2019)
=======================================

This release of fish fixes an issue discovered in fish 3.0.1.

Fixes and improvements
----------------------

-  The PWD environment variable is now ignored if it does not resolve to
   the true working directory, fixing strange behaviour in terminals
   started by editors and IDEs (:issue:`5647`).

If you are upgrading from version 2.7.1 or before, please also review
the release notes for 3.0.1, 3.0.0 and 3.0b1 (included below).


fish 3.0.1 (released February 11, 2019)
=======================================

This release of fish fixes a number of major issues discovered in fish
3.0.0.

.. _fixes-and-improvements-1:

Fixes and improvements
----------------------

-  ``exec`` does not complain about running foreground jobs when called
   (:issue:`5449`).
-  while loops now evaluate to the last executed command in the loop
   body (or zero if the body was empty), matching POSIX semantics
   (:issue:`4982`).
-  ``read --silent`` no longer echoes to the tty when run from a
   non-interactive script (:issue:`5519`).
-  On macOS, path entries with spaces in ``/etc/paths`` and
   ``/etc/paths.d`` now correctly set path entries with spaces.
   Likewise, ``MANPATH`` is correctly set from ``/etc/manpaths`` and
   ``/etc/manpaths.d`` (:issue:`5481`).
-  fish starts correctly under Cygwin/MSYS2 (:issue:`5426`).
-  The ``pager-toggle-search`` binding (Ctrl-S by default) will now
   activate the search field, even when the pager is not focused.
-  The error when a command is not found is now printed a single time,
   instead of once per argument (:issue:`5588`).
-  Fixes and improvements to the git completions, including printing
   correct paths with older git versions, fuzzy matching again, reducing
   unnecessary offers of root paths (starting with ``:/``) (:issue:`5578`,
   :issue:`5574`, :issue:`5476`), and ignoring shell aliases, so enterprising users can
   set up the wrapping command (via
   ``set -g __fish_git_alias_$command $whatitwraps``) (:issue:`5412`).
-  Significant performance improvements to core shell functions (:issue:`5447`)
   and to the ``kill`` completions (:issue:`5541`).
-  Starting in symbolically-linked working directories works correctly
   (:issue:`5525`).
-  The default ``fish_title`` function no longer contains extra spaces
   (:issue:`5517`).
-  The ``nim`` prompt now works correctly when chosen in the Web-based
   configuration (:issue:`5490`).
-  ``string`` now prints help to stdout, like other builtins (:issue:`5495`).
-  Killing the terminal while fish is in vi normal mode will no longer
   send it spinning and eating CPU. (:issue:`5528`)
-  A number of crashes have been fixed (:issue:`5550`, :issue:`5548`, :issue:`5479`, :issue:`5453`).
-  Improvements to the documentation and certain completions.

Known issues
------------

There is one significant known issue that was not corrected before the
release:

-  fish does not run correctly under Windows Services for Linux before
   Windows 10 version 1809/17763, and the message warning of this may
   not be displayed (:issue:`5619`).

If you are upgrading from version 2.7.1 or before, please also review
the release notes for 3.0.0 and 3.0b1 (included below).

--------------

fish 3.0.0 (released December 28, 2018)
=======================================

fish 3 is a major release, which introduces some breaking changes
alongside improved functionality. Although most existing scripts will
continue to work, they should be reviewed against the list contained in
the 3.0b1 release notes below.

Compared to the beta release of fish 3.0b1, fish version 3.0.0:

-  builds correctly against musl libc (:issue:`5407`)
-  handles huge numeric arguments to ``test`` correctly (:issue:`5414`)
-  removes the history colouring introduced in 3.0b1, which did not
   always work correctly

There is one significant known issue which was not able to be corrected
before the release:

-  fish 3.0.0 builds on Cygwin (:issue:`5423`), but does not run correctly
   (:issue:`5426`) and will result in a hanging terminal when started. Cygwin
   users are encouraged to continue using 2.7.1 until a release which
   corrects this is available.

If you are upgrading from version 2.7.1 or before, please also review
the release notes for 3.0b1 (included below).

--------------

fish 3.0b1 (released December 11, 2018)
=======================================

fish 3 is a major release, which introduces some breaking changes
alongside improved functionality. Although most existing scripts will
continue to work, they should be reviewed against the list below.

Notable non-backward compatible changes
---------------------------------------

-  Process and job expansion has largely been removed. ``%`` will no
   longer perform these expansions, except for ``%self`` for the PID of
   the current shell. Additionally, job management commands (``disown``,
   ``wait``, ``bg``, ``fg`` and ``kill``) will expand job specifiers
   starting with ``%`` (:issue:`4230`, :issue:`1202`).
-  ``set x[1] x[2] a b``, to set multiple elements of an array at once,
   is no longer valid syntax (:issue:`4236`).
-  A literal ``{}`` now expands to itself, rather than nothing. This
   makes working with ``find -exec`` easier (:issue:`1109`, :issue:`4632`).
-  Literally accessing a zero-index is now illegal syntax and is caught
   by the parser (:issue:`4862`). (fish indices start at 1)
-  Successive commas in brace expansions are handled in less surprising
   manner. For example, ``{,,,}`` expands to four empty strings rather
   than an empty string, a comma and an empty string again (:issue:`3002`,
   :issue:`4632`).
-  ``for`` loop control variables are no longer local to the ``for``
   block (:issue:`1935`).
-  Variables set in ``if`` and ``while`` conditions are available
   outside the block (:issue:`4820`).
-  Local exported (``set -lx``) vars are now visible to functions
   (:issue:`1091`).
-  The new ``math`` builtin (see below) does not support logical
   expressions; ``test`` should be used instead (:issue:`4777`).
-  Range expansion will now behave sensibly when given a single positive
   and negative index (``$foo[5..-1]`` or ``$foo[-1..5]``), clamping to
   the last valid index without changing direction if the list has fewer
   elements than expected.
-  ``read`` now uses ``-s`` as short for ``--silent`` (à la ``bash``);
   ``--shell``\ ’s abbreviation (formerly ``-s``) is now ``-S`` instead
   (:issue:`4490`).
-  ``cd`` no longer resolves symlinks. fish now maintains a virtual
   path, matching other shells (:issue:`3350`).
-  ``source`` now requires an explicit ``-`` as the filename to read
   from the terminal (:issue:`2633`).
-  Arguments to ``end`` are now errors, instead of being silently
   ignored.
-  The names ``argparse``, ``read``, ``set``, ``status``, ``test`` and
   ``[`` are now reserved and not allowed as function names. This
   prevents users unintentionally breaking stuff (:issue:`3000`).
-  The ``fish_user_abbreviations`` variable is no longer used;
   abbreviations will be migrated to the new storage format
   automatically.
-  The ``FISH_READ_BYTE_LIMIT`` variable is now called
   ``fish_byte_limit`` (:issue:`4414`).
-  Environment variables are no longer split into arrays based on the
   record separator character on startup. Instead, variables are not
   split, unless their name ends in PATH, in which case they are split
   on colons (:issue:`436`).
-  The ``history`` builtin’s ``--with-time`` option has been removed;
   this has been deprecated in favor of ``--show-time`` since 2.7.0
   (:issue:`4403`).
-  The internal variables ``__fish_datadir`` and ``__fish_sysconfdir``
   are now known as ``__fish_data_dir`` and ``__fish_sysconf_dir``
   respectively.

Deprecations
------------

With the release of fish 3, a number of features have been marked for
removal in the future. All users are encouraged to explore alternatives.
A small number of these features are currently behind feature flags,
which are turned on at present but may be turned off by default in the
future.

A new feature flags mechanism is added for staging deprecations and
breaking changes. Feature flags may be specified at launch with
``fish --features ...`` or by setting the universal ``fish_features``
variable. (:issue:`4940`)

-  The use of the ``IFS`` variable for ``read`` is deprecated; ``IFS``
   will be ignored in the future (:issue:`4156`). Use the ``read --delimiter``
   option instead.
-  The ``function --on-process-exit`` switch will be removed in future
   (:issue:`4700`). Use the ``fish_exit`` event instead:
   ``function --on-event fish_exit``.
-  ``$_`` is deprecated and will removed in the future (:issue:`813`). Use
   ``status current-command`` in a command substitution instead.
-  ``^`` as a redirection deprecated and will be removed in the future.
   (:issue:`4394`). Use ``2>`` to redirect stderr. This is controlled by the
   ``stderr-nocaret`` feature flag.
-  ``?`` as a glob (wildcard) is deprecated and will be removed in the
   future (:issue:`4520`). This is controlled by the ``qmark-noglob`` feature
   flag.

Notable fixes and improvements
------------------------------

.. _syntax-changes-and-new-commands-2:

Syntax changes and new commands
-------------------------------

-  fish now supports ``&&`` (like ``and``), ``||`` (like ``or``), and
   ``!`` (like ``not``), for better migration from POSIX-compliant
   shells (:issue:`4620`).
-  Variables may be used as commands (:issue:`154`).
-  fish may be started in private mode via ``fish --private``. Private
   mode fish sessions do not have access to the history file and any
   commands evaluated in private mode are not persisted for future
   sessions. A session variable ``$fish_private_mode`` can be queried to
   detect private mode and adjust the behavior of scripts accordingly to
   respect the user’s wish for privacy.
-  A new ``wait`` command for waiting on backgrounded processes (:issue:`4498`).
-  ``math`` is now a builtin rather than a wrapper around ``bc``
   (:issue:`3157`). Floating point computations is now used by default, and can
   be controlled with the new ``--scale`` option (:issue:`4478`).
-  Setting ``$PATH`` no longer warns on non-existent directories,
   allowing for a single $PATH to be shared across machines (eg via
   dotfiles) (:issue:`2969`).
-  ``while`` sets ``$status`` to a non-zero value if the loop is not
   executed (:issue:`4982`).
-  Command substitution output is now limited to 10 MB by default,
   controlled by the ``fish_read_limit`` variable (:issue:`3822`). Notably, this
   is larger than most operating systems’ argument size limit, so trying
   to pass argument lists this size to external commands has never
   worked.
-  The machine hostname, where available, is now exposed as the
   ``$hostname`` reserved variable. This removes the dependency on the
   ``hostname`` executable (:issue:`4422`).
-  Bare ``bind`` invocations in config.fish now work. The
   ``fish_user_key_bindings`` function is no longer necessary, but will
   still be executed if it exists (:issue:`5191`).
-  ``$fish_pid`` and ``$last_pid`` are available as replacements for
   ``%self`` and ``%last``.

New features in commands
------------------------

-  ``alias`` has a new ``--save`` option to save the generated function
   immediately (:issue:`4878`).
-  ``bind`` has a new ``--silent`` option to ignore bind requests for
   named keys not available under the current terminal (:issue:`4188`, :issue:`4431`).
-  ``complete`` has a new ``--keep-order`` option to show the provided
   or dynamically-generated argument list in the same order as
   specified, rather than alphabetically (:issue:`361`).
-  ``exec`` prompts for confirmation if background jobs are running.
-  ``funced`` has a new ``--save`` option to automatically save the
   edited function after successfully editing (:issue:`4668`).
-  ``functions`` has a new ``--handlers`` option to show functions
   registered as event handlers (:issue:`4694`).
-  ``history search`` supports globs for wildcard searching (:issue:`3136`) and
   has a new ``--reverse`` option to show entries from oldest to newest
   (:issue:`4375`).
-  ``jobs`` has a new ``--quiet`` option to silence the output.
-  ``read`` has a new ``--delimiter`` option for splitting input into
   arrays (:issue:`4256`).
-  ``read`` writes directly to stdout if called without arguments
   (:issue:`4407`).
-  ``read`` can now read individual lines into separate variables
   without consuming the input in its entirety via the new ``/--line``
   option.
-  ``set`` has new ``--append`` and ``--prepend`` options (:issue:`1326`).
-  ``string match`` with an empty pattern and ``--entire`` in glob mode
   now matches everything instead of nothing (:issue:`4971`).
-  ``string split`` supports a new ``--no-empty`` option to exclude
   empty strings from the result (:issue:`4779`).
-  ``string`` has new subcommands ``split0`` and ``join0`` for working
   with NUL-delimited output.
-  ``string`` no longer stops processing text after NUL characters
   (:issue:`4605`)
-  ``string escape`` has a new ``--style regex`` option for escaping
   strings to be matched literally in ``string`` regex operations.
-  ``test`` now supports floating point values in numeric comparisons.

.. _interactive-improvements-2:

Interactive improvements
------------------------

-  A pipe at the end of a line now allows the job to continue on the
   next line (:issue:`1285`).
-  Italics and dim support out of the box on macOS for Terminal.app and
   iTerm (:issue:`4436`).
-  ``cd`` tab completions no longer descend into the deepest unambiguous
   path (:issue:`4649`).
-  Pager navigation has been improved. Most notably, moving down now
   wraps around, moving up from the commandline now jumps to the last
   element and moving right and left now reverse each other even when
   wrapping around (:issue:`4680`).
-  Typing normal characters while the completion pager is active no
   longer shows the search field. Instead it enters them into the
   command line, and ends paging (:issue:`2249`).
-  A new input binding ``pager-toggle-search`` toggles the search field
   in the completions pager on and off. By default, this is bound to
   Ctrl-S.
-  Searching in the pager now does a full fuzzy search (:issue:`5213`).
-  The pager will now show the full command instead of just its last
   line if the number of completions is large (:issue:`4702`).
-  Abbreviations can be tab-completed (:issue:`3233`).
-  Tildes in file names are now properly escaped in completions (:issue:`2274`).
-  Wrapping completions (from ``complete --wraps`` or
   ``function --wraps``) can now inject arguments. For example,
   ``complete gco --wraps 'git checkout'`` now works properly (:issue:`1976`).
   The ``alias`` function has been updated to respect this behavior.
-  Path completions now support expansions, meaning expressions like
   ``python ~/<TAB>`` now provides file suggestions just like any other
   relative or absolute path. (This includes support for other
   expansions, too.)
-  Autosuggestions try to avoid arguments that are already present in
   the command line.
-  Notifications about crashed processes are now always shown, even in
   command substitutions (:issue:`4962`).
-  The screen is no longer reset after a BEL, fixing graphical glitches
   (:issue:`3693`).
-  vi-mode now supports ‘;’ and ‘,’ motions. This introduces new
   {forward,backward}-jump-till and repeat-jump{,-reverse} bind
   functions (:issue:`5140`).
-  The ``*y`` vi-mode binding now works (:issue:`5100`).
-  True color is now enabled in neovim by default (:issue:`2792`).
-  Terminal size variables (``$COLUMNS``/``$LINES``) are now updated
   before ``fish_prompt`` is called, allowing the prompt to react
   (:issue:`904`).
-  Multi-line prompts no longer repeat when the terminal is resized
   (:issue:`2320`).
-  ``xclip`` support has been added to the clipboard integration
   (:issue:`5020`).
-  The Alt-P keybinding paginates the last command if the command line
   is empty.
-  ``$cmd_duration`` is no longer reset when no command is executed
   (:issue:`5011`).
-  Deleting a one-character word no longer erases the next word as well
   (:issue:`4747`).
-  Token history search (Alt-Up) omits duplicate entries (:issue:`4795`).
-  The ``fish_escape_delay_ms`` timeout, allowing the use of the escape
   key both on its own and as part of a control sequence, was applied to
   all control characters; this has been reduced to just the escape key.
-  Completing a function shows the description properly (:issue:`5206`).
-  ``commandline`` can now be used to set the commandline for the next command, restoring a behavior in 3.4.1 (:issue:`8807`).
-  Added completions for

   -  ``ansible``, including ``ansible-galaxy``, ``ansible-playbook``
      and ``ansible-vault`` (:issue:`4697`)
   -  ``bb-power`` (:issue:`4800`)
   -  ``bd`` (:issue:`4472`)
   -  ``bower``
   -  ``clang`` and ``clang++`` (:issue:`4174`)
   -  ``conda`` (:issue:`4837`)
   -  ``configure`` (for autoconf-generated files only)
   -  ``curl``
   -  ``doas`` (:issue:`5196`)
   -  ``ebuild`` (:issue:`4911`)
   -  ``emaint`` (:issue:`4758`)
   -  ``eopkg`` (:issue:`4600`)
   -  ``exercism`` (:issue:`4495`)
   -  ``hjson``
   -  ``hugo`` (:issue:`4529`)
   -  ``j`` (from autojump :issue:`4344`)
   -  ``jbake`` (:issue:`4814`)
   -  ``jhipster`` (:issue:`4472`)
   -  ``kitty``
   -  ``kldload``
   -  ``kldunload``
   -  ``makensis`` (:issue:`5242`)
   -  ``meson``
   -  ``mkdocs`` (:issue:`4906`)
   -  ``ngrok`` (:issue:`4642`)
   -  OpenBSD’s ``pkg_add``, ``pkg_delete``, ``pkg_info``, ``pfctl``,
      ``rcctl``, ``signify``, and ``vmctl`` (:issue:`4584`)
   -  ``openocd``
   -  ``optipng``
   -  ``opkg`` (:issue:`5168`)
   -  ``pandoc`` (:issue:`2937`)
   -  ``port`` (:issue:`4737`)
   -  ``powerpill`` (:issue:`4800`)
   -  ``pstack`` (:issue:`5135`)
   -  ``serve`` (:issue:`5026`)
   -  ``ttx``
   -  ``unzip``
   -  ``virsh`` (:issue:`5113`)
   -  ``xclip`` (:issue:`5126`)
   -  ``xsv``
   -  ``zfs`` and ``zpool`` (:issue:`4608`)

-  Lots of improvements to completions (especially ``darcs`` (:issue:`5112`),
   ``git``, ``hg`` and ``sudo``).
-  Completions for ``yarn`` and ``npm`` now require the
   ``all-the-package-names`` NPM package for full functionality.
-  Completions for ``bower`` and ``yarn`` now require the ``jq`` utility
   for full functionality.
-  Improved French translations.

Other fixes and improvements
----------------------------

-  Significant performance improvements to ``abbr`` (:issue:`4048`), setting
   variables (:issue:`4200`, :issue:`4341`), executing functions, globs (:issue:`4579`),
   ``string`` reading from standard input (:issue:`4610`), and slicing history
   (in particular, ``$history[1]`` for the last executed command).
-  Fish’s internal wcwidth function has been updated to deal with newer
   Unicode, and the width of some characters can be configured via the
   ``fish_ambiguous_width`` (:issue:`5149`) and ``fish_emoji_width`` (:issue:`2652`)
   variables. Alternatively, a new build-time option INTERNAL_WCWIDTH
   can be used to use the system’s wcwidth instead (:issue:`4816`).
-  ``functions`` correctly supports ``-d`` as the short form of
   ``--description``. (:issue:`5105`)
-  ``/etc/paths`` is now parsed like macOS’ bash ``path_helper``, fixing
   $PATH order (:issue:`4336`, :issue:`4852`) on macOS.
-  Using a read-only variable in a ``for`` loop produces an error,
   rather than silently producing incorrect results (:issue:`4342`).
-  The universal variables filename no longer contains the hostname or
   MAC address. It is now at the fixed location
   ``.config/fish/fish_variables`` (:issue:`1912`).
-  Exported variables in the global or universal scope no longer have
   their exported status affected by local variables (:issue:`2611`).
-  Major rework of terminal and job handling to eliminate bugs (:issue:`3805`,
   :issue:`3952`, :issue:`4178`, :issue:`4235`, :issue:`4238`, :issue:`4540`, :issue:`4929`, :issue:`5210`).
-  Improvements to the manual page completion generator (:issue:`2937`, :issue:`4313`).
-  ``suspend --force`` now works correctly (:issue:`4672`).
-  Pressing Ctrl-C while running a script now reliably terminates fish
   (:issue:`5253`).

.. _for-distributors-and-developers-2:

For distributors and developers
-------------------------------

-  fish ships with a new build system based on CMake. CMake 3.2 is the
   minimum required version. Although the autotools-based Makefile and
   the Xcode project are still shipped with this release, they will be
   removed in the near future. All distributors and developers are
   encouraged to migrate to the CMake build.
-  Build scripts for most platforms no longer require bash, using the
   standard sh instead.
-  The ``hostname`` command is no longer required for fish to operate.

–

fish 2.7.1 (released December 23, 2017)
=======================================

This release of fish fixes an issue where iTerm 2 on macOS would display
a warning about paste bracketing being left on when starting a new fish
session (:issue:`4521`).

If you are upgrading from version 2.6.0 or before, please also review
the release notes for 2.7.0 and 2.7b1 (included below).

–

fish 2.7.0 (released November 23, 2017)
=======================================

There are no major changes between 2.7b1 and 2.7.0. If you are upgrading
from version 2.6.0 or before, please also review the release notes for
2.7b1 (included below).

Xcode builds and macOS packages could not be produced with 2.7b1, but
this is fixed in 2.7.0.

–

fish 2.7b1 (released October 31, 2017)
======================================

Notable improvements
--------------------

-  A new ``cdh`` (change directory using recent history) command
   provides a more friendly alternative to prevd/nextd and pushd/popd
   (:issue:`2847`).
-  A new ``argparse`` command is available to allow fish script to parse
   arguments with the same behavior as builtin commands. This also
   includes the ``fish_opt`` helper command. (:issue:`4190`).
-  Invalid array indexes are now silently ignored (:issue:`826`, :issue:`4127`).
-  Improvements to the debugging facility, including a prompt specific
   to the debugger (``fish_breakpoint_prompt``) and a
   ``status is-breakpoint`` subcommand (:issue:`1310`).
-  ``string`` supports new ``lower`` and ``upper`` subcommands, for
   altering the case of strings (:issue:`4080`). The case changing is not
   locale-aware yet.- ``string escape`` has a new ``--style=xxx`` flag
   where ``xxx`` can be ``script``, ``var``, or ``url`` (:issue:`4150`), and can
   be reversed with ``string unescape`` (:issue:`3543`).
-  History can now be split into sessions with the ``fish_history``
   variable, or not saved to disk at all (:issue:`102`).
-  Read history is now controlled by the ``fish_history`` variable
   rather than the ``--mode-name`` flag (:issue:`1504`).
-  ``command`` now supports an ``--all`` flag to report all directories
   with the command. ``which`` is no longer a runtime dependency
   (:issue:`2778`).
-  fish can run commands before starting an interactive session using
   the new ``--init-command``/``-C`` options (:issue:`4164`).
-  ``set`` has a new ``--show`` option to show lots of information about
   variables (:issue:`4265`).

Other significant changes
-------------------------

-  The ``COLUMNS`` and ``LINES`` environment variables are now correctly
   set the first time ``fish_prompt`` is run (:issue:`4141`).

-  ``complete``\ ’s ``--no-files`` option works as intended (:issue:`112`).

-  ``echo -h`` now correctly echoes ``-h`` in line with other shells
   (:issue:`4120`).

-  The ``export`` compatibility function now returns zero on success,
   rather than always returning 1 (:issue:`4435`).

-  Stop converting empty elements in MANPATH to “.” (:issue:`4158`). The
   behavior being changed was introduced in fish 2.6.0.

-  ``count -h`` and ``count --help`` now return 1 rather than produce
   command help output (:issue:`4189`).

-  An attempt to ``read`` which stops because too much data is available
   still defines the variables given as parameters (:issue:`4180`).

-  A regression in fish 2.4.0 which prevented ``pushd +1`` from working
   has been fixed (:issue:`4091`).

-  A regression in fish 2.6.0 where multiple ``read`` commands in
   non-interactive scripts were broken has been fixed (:issue:`4206`).

-  A regression in fish 2.6.0 involving universal variables with
   side-effects at startup such as ``set -U fish_escape_delay_ms 10``
   has been fixed (:issue:`4196`).

-  Added completions for:

   -  ``as`` (:issue:`4130`)
   -  ``cdh`` (:issue:`2847`)
   -  ``dhcpd`` (:issue:`4115`)
   -  ``ezjail-admin`` (:issue:`4324`)
   -  Fabric’s ``fab`` (:issue:`4153`)
   -  ``grub-file`` (:issue:`4119`)
   -  ``grub-install`` (:issue:`4119`)
   -  ``jest`` (:issue:`4142`)
   -  ``kdeconnect-cli``
   -  ``magneto`` (:issue:`4043`, :issue:`4108`)
   -  ``mdadm`` (:issue:`4198`)
   -  ``passwd`` (:issue:`4209`)
   -  ``pip`` and ``pipenv`` (:issue:`4448`)
   -  ``s3cmd`` (:issue:`4332`)
   -  ``sbt`` (:issue:`4347`)
   -  ``snap`` (:issue:`4215`)
   -  Sublime Text 3’s ``subl`` (:issue:`4277`)

-  Lots of improvements to completions.

-  Updated Chinese and French translations.

-  Improved completions for:

   -  ``apt``

   -  ``cd`` (:issue:`4061`)

   -  ``composer`` (:issue:`4295`)

   -  ``eopkg``

   -  ``flatpak`` (:issue:`4456`)

   -  ``git`` (:issue:`4117`, :issue:`4147`, :issue:`4329`, :issue:`4368`)

   -  ``gphoto2``

   -  ``killall`` (:issue:`4052`)

   -  ``ln``

   -  ``npm`` (:issue:`4241`)

   -  ``ssh`` (:issue:`4377`)

   -  ``tail``

   -  ``xdg-mime`` (:issue:`4333`)

   -  ``zypper`` (:issue:`4325`)

fish 2.6.0 (released June 3, 2017)
==================================

Since the beta release of fish 2.6b1, fish version 2.6.0 contains a
number of minor fixes, new completions for ``magneto`` (:issue:`4043`), and
improvements to the documentation.

.. _known-issues-1:

Known issues
------------

-  Apple macOS Sierra 10.12.5 introduced a problem with launching web
   browsers from other programs using AppleScript. This affects the fish
   Web configuration (``fish_config``); users on these platforms will
   need to manually open the address displayed in the terminal, such as
   by copying and pasting it into a browser. This problem will be fixed
   with macOS 10.12.6.

If you are upgrading from version 2.5.0 or before, please also review
the release notes for 2.6b1 (included below).

--------------

fish 2.6b1 (released May 14, 2017)
==================================

.. _notable-fixes-and-improvements-1:

Notable fixes and improvements
------------------------------

-  Jobs running in the background can now be removed from the list of
   jobs with the new ``disown`` builtin, which behaves like the same
   command in other shells (:issue:`2810`).
-  Command substitutions now have access to the terminal, like in other
   shells. This allows tools like ``fzf`` to work properly (:issue:`1362`,
   :issue:`3922`).
-  In cases where the operating system does not report the size of the
   terminal, the ``COLUMNS`` and ``LINES`` environment variables are
   used; if they are unset, a default of 80x24 is assumed.
-  New French (:issue:`3772` & :issue:`3788`) and improved German (:issue:`3834`) translations.
-  fish no longer depends on the ``which`` external command.

.. _other-significant-changes-1:

Other significant changes
-------------------------

-  Performance improvements in launching processes, including major
   reductions in signal blocking. Although this has been heavily tested,
   it may cause problems in some circumstances; set the
   ``FISH_NO_SIGNAL_BLOCK`` variable to 0 in your fish configuration
   file to return to the old behaviour (:issue:`2007`).
-  Performance improvements in prompts and functions that set lots of
   colours (:issue:`3793`).
-  The Delete key no longer deletes backwards (a regression in 2.5.0).
-  ``functions`` supports a new ``--details`` option, which identifies
   where the function was loaded from (:issue:`3295`), and a
   ``--details --verbose`` option which includes the function
   description (:issue:`597`).
-  ``read`` will read up to 10 MiB by default, leaving the target
   variable empty and exiting with status 122 if the line is too long.
   You can set a different limit with the ``FISH_READ_BYTE_LIMIT``
   variable.
-  ``read`` supports a new ``--silent`` option to hide the characters
   typed (:issue:`838`), for when reading sensitive data from the terminal.
   ``read`` also now accepts simple strings for the prompt (rather than
   scripts) with the new ``-P`` and ``--prompt-str`` options (:issue:`802`).
-  ``export`` and ``setenv`` now understand colon-separated ``PATH``,
   ``CDPATH`` and ``MANPATH`` variables.
-  ``setenv`` is no longer a simple alias for ``set -gx`` and will
   complain, just like the csh version, if given more than one value
   (:issue:`4103`).
-  ``bind`` supports a new ``--list-modes`` option (:issue:`3872`).
-  ``bg`` will check all of its arguments before backgrounding any jobs;
   any invalid arguments will cause a failure, but non-existent (eg
   recently exited) jobs are ignored (:issue:`3909`).
-  ``funced`` warns if the function being edited has not been modified
   (:issue:`3961`).
-  ``printf`` correctly outputs “long long” integers (:issue:`3352`).
-  ``status`` supports a new ``current-function`` subcommand to print
   the current function name (:issue:`1743`).
-  ``string`` supports a new ``repeat`` subcommand (:issue:`3864`).
   ``string match`` supports a new ``--entire`` option to emit the
   entire line matched by a pattern (:issue:`3957`). ``string replace`` supports
   a new ``--filter`` option to only emit lines which underwent a
   replacement (:issue:`3348`).
-  ``test`` supports the ``-k`` option to test for sticky bits (:issue:`733`).
-  ``umask`` understands symbolic modes (:issue:`738`).
-  Empty components in the ``CDPATH``, ``MANPATH`` and ``PATH``
   variables are now converted to “.” (:issue:`2106`, :issue:`3914`).
-  New versions of ncurses (6.0 and up) wipe terminal scrollback buffers
   with certain commands; the ``C-l`` binding tries to avoid this
   (:issue:`2855`).
-  Some systems’ ``su`` implementations do not set the ``USER``
   environment variable; it is now reset for root users (:issue:`3916`).
-  Under terminals which support it, bracketed paste is enabled,
   escaping problematic characters for security and convience (:issue:`3871`).
   Inside single quotes (``'``), single quotes and backslashes in pasted
   text are escaped (:issue:`967`). The ``fish_clipboard_paste`` function (bound
   to ``C-v`` by default) is still the recommended pasting method where
   possible as it includes this functionality and more.
-  Processes in pipelines are no longer signalled as soon as one command
   in the pipeline has completed (:issue:`1926`). This behaviour matches other
   shells mre closely.
-  All functions requiring Python work with whichever version of Python
   is installed (:issue:`3970`). Python 3 is preferred, but Python 2.6 remains
   the minimum version required.
-  The color of the cancellation character can be controlled by the
   ``fish_color_cancel`` variable (:issue:`3963`).
-  Added completions for:
-  ``caddy`` (:issue:`4008`)
-  ``castnow`` (:issue:`3744`)
-  ``climate`` (:issue:`3760`)
-  ``flatpak``
-  ``gradle`` (:issue:`3859`)
-  ``gsettings`` (:issue:`4001`)
-  ``helm`` (:issue:`3829`)
-  ``i3-msg`` (:issue:`3787`)
-  ``ipset`` (:issue:`3924`)
-  ``jq`` (:issue:`3804`)
-  ``light`` (:issue:`3752`)
-  ``minikube`` (:issue:`3778`)
-  ``mocha`` (:issue:`3828`)
-  ``mkdosfs`` (:issue:`4017`)
-  ``pv`` (:issue:`3773`)
-  ``setsid`` (:issue:`3791`)
-  ``terraform`` (:issue:`3960`)
-  ``usermod`` (:issue:`3775`)
-  ``xinput``
-  ``yarn`` (:issue:`3816`)
-  Improved completions for ``adb`` (:issue:`3853`), ``apt`` (:issue:`3771`), ``bzr``
   (:issue:`3769`), ``dconf``, ``git`` (including :issue:`3743`), ``grep`` (:issue:`3789`),
   ``go`` (:issue:`3789`), ``help`` (:issue:`3789`), ``hg`` (:issue:`3975`), ``htop`` (:issue:`3789`),
   ``killall`` (:issue:`3996`), ``lua``, ``man`` (:issue:`3762`), ``mount`` (:issue:`3764` &
   :issue:`3841`), ``obnam`` (:issue:`3924`), ``perl`` (:issue:`3856`), ``portmaster`` (:issue:`3950`),
   ``python`` (:issue:`3840`), ``ssh`` (:issue:`3781`), ``scp`` (:issue:`3781`), ``systemctl``
   (:issue:`3757`) and ``udisks`` (:issue:`3764`).

--------------

fish 2.5.0 (released February 3, 2017)
======================================

There are no major changes between 2.5b1 and 2.5.0. If you are upgrading
from version 2.4.0 or before, please also review the release notes for
2.5b1 (included below).

.. _notable-fixes-and-improvements-2:

Notable fixes and improvements
------------------------------

-  The Home, End, Insert, Delete, Page Up and Page Down keys work in
   Vi-style key bindings (:issue:`3731`).

--------------

fish 2.5b1 (released January 14, 2017)
======================================

Platform Changes
----------------

Starting with version 2.5, fish requires a more up-to-date version of
C++, specifically C++11 (from 2011). This affects some older platforms:

Linux
^^^^^

For users building from source, GCC’s g++ 4.8 or later, or LLVM’s clang
3.3 or later, are known to work. Older platforms may require a newer
compiler installed.

Unfortunately, because of the complexity of the toolchain, binary
packages are no longer published by the fish-shell developers for the
following platforms:

-  Red Hat Enterprise Linux and CentOS 5 & 6 for 64-bit builds
-  Ubuntu 12.04 (EoLTS April 2017)
-  Debian 7 (EoLTS May 2018)

Installing newer version of fish on these systems will require building
from source.

OS X SnowLeopard
^^^^^^^^^^^^^^^^

Starting with version 2.5, fish requires a C++11 standard library on OS
X 10.6 (“SnowLeopard”). If this library is not installed, you will see
this error: ``dyld: Library not loaded: /usr/lib/libc++.1.dylib``

MacPorts is the easiest way to obtain this library. After installing the
SnowLeopard MacPorts release from the install page, run:

::

   sudo port -v install libcxx

Now fish should launch successfully. (Please open an issue if it does
not.)

This is only necessary on 10.6. OS X 10.7 and later include the required
library by default.

.. _other-significant-changes-2:

Other significant changes
-------------------------

-  Attempting to exit with running processes in the background produces
   a warning, then signals them to terminate if a second attempt to exit
   is made. This brings the behaviour for running background processes
   into line with stopped processes. (:issue:`3497`)
-  ``random`` can now have start, stop and step values specified, or the
   new ``choice`` subcommand can be used to pick an argument from a list
   (:issue:`3619`).
-  A new key bindings preset, ``fish_hybrid_key_bindings``, including
   all the Emacs-style and Vi-style bindings, which behaves like
   ``fish_vi_key_bindings`` in fish 2.3.0 (:issue:`3556`).
-  ``function`` now returns an error when called with invalid options,
   rather than defining the function anyway (:issue:`3574`). This was a
   regression present in fish 2.3 and 2.4.0.
-  fish no longer prints a warning when it identifies a running instance
   of an old version (2.1.0 and earlier). Changes to universal variables
   may not propagate between these old versions and 2.5b1.
-  Improved compatiblity with Android (:issue:`3585`), MSYS/mingw (:issue:`2360`), and
   Solaris (:issue:`3456`, :issue:`3340`).
-  Like other shells, the ``test`` builting now returns an error for
   numeric operations on invalid integers (:issue:`3346`, :issue:`3581`).
-  ``complete`` no longer recognises ``--authoritative`` and
   ``--unauthoritative`` options, and they are marked as obsolete.
-  ``status`` accepts subcommands, and should be used like
   ``status is-interactive``. The old options continue to be supported
   for the foreseeable future (:issue:`3526`), although only one subcommand or
   option can be specified at a time.
-  Selection mode (used with “begin-selection”) no longer selects a
   character the cursor does not move over (:issue:`3684`).
-  List indexes are handled better, and a bit more liberally in some
   cases (``echo $PATH[1 .. 3]`` is now valid) (:issue:`3579`).
-  The ``fish_mode_prompt`` function is now simply a stub around
   ``fish_default_mode_prompt``, which allows the mode prompt to be
   included more easily in customised prompt functions (:issue:`3641`).

.. _notable-fixes-and-improvements-3:

Notable fixes and improvements
------------------------------

-  ``alias``, run without options or arguments, lists all defined
   aliases, and aliases now include a description in the function
   signature that identifies them.
-  ``complete`` accepts empty strings as descriptions (:issue:`3557`).
-  ``command`` accepts ``-q``/``--quiet`` in combination with
   ``--search`` (:issue:`3591`), providing a simple way of checking whether a
   command exists in scripts.
-  Abbreviations can now be renamed with
   ``abbr --rename OLD_KEY NEW_KEY`` (:issue:`3610`).
-  The command synopses printed by ``--help`` options work better with
   copying and pasting (:issue:`2673`).
-  ``help`` launches the browser specified by the
   ``$fish_help_browser variable`` if it is set (:issue:`3131`).
-  History merging could lose items under certain circumstances and is
   now fixed (:issue:`3496`).
-  The ``$status`` variable is now set to 123 when a syntactically
   invalid command is entered (:issue:`3616`).
-  Exiting fish now signals all background processes to terminate, not
   just stopped jobs (:issue:`3497`).
-  A new ``prompt_hostname`` function which prints a hostname suitable
   for use in prompts (:issue:`3482`).
-  The ``__fish_man_page`` function (bound to Alt-h by default) now
   tries to recognize subcommands (e.g. ``git add`` will now open the
   “git-add” man page) (:issue:`3678`).
-  A new function ``edit_command_buffer`` (bound to Alt-e & Alt-v by
   default) to edit the command buffer in an external editor (:issue:`1215`,
   :issue:`3627`).
-  ``set_color`` now supports italics (``--italics``), dim (``--dim``)
   and reverse (``--reverse``) modes (:issue:`3650`).
-  Filesystems with very slow locking (eg incorrectly-configured NFS)
   will no longer slow fish down (:issue:`685`).
-  Improved completions for ``apt`` (:issue:`3695`), ``fusermount`` (:issue:`3642`),
   ``make`` (:issue:`3628`), ``netctl-auto`` (:issue:`3378`), ``nmcli`` (:issue:`3648`),
   ``pygmentize`` (:issue:`3378`), and ``tar`` (:issue:`3719`).
-  Added completions for:
-  ``VBoxHeadless`` (:issue:`3378`)
-  ``VBoxSDL`` (:issue:`3378`)
-  ``base64`` (:issue:`3378`)
-  ``caffeinate`` (:issue:`3524`)
-  ``dconf`` (:issue:`3638`)
-  ``dig`` (:issue:`3495`)
-  ``dpkg-reconfigure`` (:issue:`3521` & :issue:`3522`)
-  ``feh`` (:issue:`3378`)
-  ``launchctl`` (:issue:`3682`)
-  ``lxc`` (:issue:`3554` & :issue:`3564`),
-  ``mddiagnose`` (:issue:`3524`)
-  ``mdfind`` (:issue:`3524`)
-  ``mdimport`` (:issue:`3524`)
-  ``mdls`` (:issue:`3524`)
-  ``mdutil`` (:issue:`3524`)
-  ``mkvextract`` (:issue:`3492`)
-  ``nvram`` (:issue:`3524`)
-  ``objdump`` (:issue:`3378`)
-  ``sysbench`` (:issue:`3491`)
-  ``tmutil`` (:issue:`3524`)

--------------

fish 2.4.0 (released November 8, 2016)
======================================

There are no major changes between 2.4b1 and 2.4.0.

.. _notable-fixes-and-improvements-4:

Notable fixes and improvements
------------------------------

-  The documentation is now generated properly and with the correct
   version identifier.
-  Automatic cursor changes are now only enabled on the subset of XTerm
   versions known to support them, resolving a problem where older
   versions printed garbage to the terminal before and after every
   prompt (:issue:`3499`).
-  Improved the title set in Apple Terminal.app.
-  Added completions for ``defaults`` and improved completions for
   ``diskutil`` (:issue:`3478`).

--------------

fish 2.4b1 (released October 18, 2016)
======================================

Significant changes
-------------------

-  The clipboard integration has been revamped with explicit bindings.
   The killring commands no longer copy from, or paste to, the X11
   clipboard - use the new copy (``C-x``) and paste (``C-v``) bindings
   instead. The clipboard is now available on OS X as well as systems
   using X11 (e.g. Linux). (:issue:`3061`)
-  ``history`` uses subcommands (``history delete``) rather than options
   (``history --delete``) for its actions (:issue:`3367`). You can no longer
   specify multiple actions via flags (e.g.,
   ``history --delete --save something``).
-  New ``history`` options have been added, including ``--max=n`` to
   limit the number of history entries, ``--show-time`` option to show
   timestamps (:issue:`3175`, :issue:`3244`), and ``--null`` to null terminate history
   entries in the search output.
-  ``history search`` is now case-insensitive by default (which also
   affects ``history delete``) (:issue:`3236`).
-  ``history delete`` now correctly handles multiline commands (:issue:`31`).
-  Vi-style bindings no longer include all of the default emacs-style
   bindings; instead, they share some definitions (:issue:`3068`).
-  If there is no locale set in the environment, various known system
   configuration files will be checked for a default. If no locale can
   be found, ``en_US-UTF.8`` will be used (:issue:`277`).
-  A number followed by a caret (e.g. ``5^``) is no longer treated as a
   redirection (:issue:`1873`).
-  The ``$version`` special variable can be overwritten, so that it can
   be used for other purposes if required.

.. _notable-fixes-and-improvements-5:

Notable fixes and improvements
------------------------------

-  The ``fish_realpath`` builtin has been renamed to ``realpath`` and
   made compatible with GNU ``realpath`` when run without arguments
   (:issue:`3400`). It is used only for systems without a ``realpath`` or
   ``grealpath`` utility (:issue:`3374`).
-  Improved color handling on terminals/consoles with 8-16 colors,
   particularly the use of bright named color (:issue:`3176`, :issue:`3260`).
-  ``fish_indent`` can now read from files given as arguments, rather
   than just standard input (:issue:`3037`).
-  Fuzzy tab completions behave in a less surprising manner (:issue:`3090`,
   :issue:`3211`).
-  ``jobs`` should only print its header line once (:issue:`3127`).
-  Wildcards in redirections are highlighted appropriately (:issue:`2789`).
-  Suggestions will be offered more often, like after removing
   characters (:issue:`3069`).
-  ``history --merge`` now correctly interleaves items in chronological
   order (:issue:`2312`).
-  Options for ``fish_indent`` have been aligned with the other binaries
   - in particular, ``-d`` now means ``--debug``. The ``--dump`` option
   has been renamed to ``--dump-parse-tree`` (:issue:`3191`).
-  The display of bindings in the Web-based configuration has been
   greatly improved (:issue:`3325`), as has the rendering of prompts (:issue:`2924`).
-  fish should no longer hang using 100% CPU in the C locale (:issue:`3214`).
-  A bug in FreeBSD 11 & 12, Dragonfly BSD & illumos prevented fish from
   working correctly on these platforms under UTF-8 locales; fish now
   avoids the buggy behaviour (:issue:`3050`).
-  Prompts which show git repository information (via
   ``__fish_git_prompt``) are faster in large repositories (:issue:`3294`) and
   slow filesystems (:issue:`3083`).
-  fish 2.3.0 reintroduced a problem where the greeting was printed even
   when using ``read``; this has been corrected again (:issue:`3261`).
-  Vi mode changes the cursor depending on the current mode (:issue:`3215`).
-  Command lines with escaped space characters at the end tab-complete
   correctly (:issue:`2447`).
-  Added completions for:

   -  ``arcanist`` (:issue:`3256`)
   -  ``connmanctl`` (:issue:`3419`)
   -  ``figlet`` (:issue:`3378`)
   -  ``mdbook`` (:issue:`3378`)
   -  ``ninja`` (:issue:`3415`)
   -  ``p4``, the Perforce client (:issue:`3314`)
   -  ``pygmentize`` (:issue:`3378`)
   -  ``ranger`` (:issue:`3378`)

-  Improved completions for ``aura`` (:issue:`3297`), ``abbr`` (:issue:`3267`), ``brew``
   (:issue:`3309`), ``chown`` (:issue:`3380`, :issue:`3383`),\ ``cygport`` (:issue:`3392`), ``git``
   (:issue:`3274`, :issue:`3226`, :issue:`3225`, :issue:`3094`, :issue:`3087`, :issue:`3035`, :issue:`3021`, :issue:`2982`, :issue:`3230`),
   ``kill`` & ``pkill`` (:issue:`3200`), ``screen`` (:issue:`3271`), ``wget`` (:issue:`3470`),
   and ``xz`` (:issue:`3378`).
-  Distributors, packagers and developers will notice that the build
   process produces more succinct output by default; use ``make V=1`` to
   get verbose output (:issue:`3248`).
-  Improved compatibility with minor platforms including musl (:issue:`2988`),
   Cygwin (:issue:`2993`), Android (:issue:`3441`, :issue:`3442`), Haiku (:issue:`3322`) and Solaris .

--------------

fish 2.3.1 (released July 3, 2016)
==================================

This is a functionality and bugfix release. This release does not
contain all the changes to fish since the last release, but fixes a
number of issues directly affecting users at present and includes a
small number of new features.

.. _significant-changes-1:

Significant changes
-------------------

-  A new ``fish_key_reader`` binary for decoding interactive keypresses
   (:issue:`2991`).
-  ``fish_mode_prompt`` has been updated to reflect the changes in the
   way the Vi input mode is set up (:issue:`3067`), making this more reliable.
-  ``fish_config`` can now properly be launched from the OS X app bundle
   (:issue:`3140`).

.. _notable-fixes-and-improvements-6:

Notable fixes and improvements
------------------------------

-  Extra lines were sometimes inserted into the output under Windows
   (Cygwin and Microsoft Windows Subsystem for Linux) due to TTY
   timestamps not being updated (:issue:`2859`).
-  The ``string`` builtin’s ``match`` mode now handles the combination
   of ``-rnv`` (match, invert and count) correctly (:issue:`3098`).
-  Improvements to TTY special character handling (:issue:`3064`), locale
   handling (:issue:`3124`) and terminal environment variable handling (:issue:`3060`).
-  Work towards handling the terminal modes for external commands
   launched from initialisation files (:issue:`2980`).
-  Ease the upgrade path from fish 2.2.0 and before by warning users to
   restart fish if the ``string`` builtin is not available (:issue:`3057`).
-  ``type -a`` now syntax-colorizes function source output.
-  Added completions for ``alsamixer``, ``godoc``, ``gofmt``,
   ``goimports``, ``gorename``, ``lscpu``, ``mkdir``, ``modinfo``,
   ``netctl-auto``, ``poweroff``, ``termite``, ``udisksctl`` and ``xz``
   (:issue:`3123`).
-  Improved completions for ``apt`` (:issue:`3097`), ``aura`` (:issue:`3102`),\ ``git``
   (:issue:`3114`), ``npm`` (:issue:`3158`), ``string`` and ``suspend`` (:issue:`3154`).

--------------

fish 2.3.0 (released May 20, 2016)
==================================

There are no significant changes between 2.3.0 and 2.3b2.

Other notable fixes and improvements
------------------------------------

-  ``abbr`` now allows non-letter keys (:issue:`2996`).
-  Define a few extra colours on first start (:issue:`2987`).
-  Multiple documentation updates.
-  Added completions for rmmod (:issue:`3007`).
-  Improved completions for git (:issue:`2998`).

.. _known-issues-2:

Known issues
------------

-  Interactive commands started from fish configuration files or from
   the ``-c`` option may, under certain circumstances, be started with
   incorrect terminal modes and fail to behave as expected. A fix is
   planned but requires further testing (:issue:`2619`).

--------------

fish 2.3b2 (released May 5, 2016)
=================================

.. _significant-changes-2:

Significant changes
-------------------

-  A new ``fish_realpath`` builtin and associated function to allow the
   use of ``realpath`` even on those platforms that don’t ship an
   appropriate command (:issue:`2932`).
-  Alt-# toggles the current command line between commented and
   uncommented states, making it easy to save a command in history
   without executing it.
-  The ``fish_vi_mode`` function is now deprecated in favour of
   ``fish_vi_key_bindings``.

.. _other-notable-fixes-and-improvements-1:

Other notable fixes and improvements
------------------------------------

-  Fix the build on Cygwin (:issue:`2952`) and RedHat Enterprise Linux/CentOS 5
   (:issue:`2955`).
-  Avoid confusing the terminal line driver with non-printing characters
   in ``fish_title`` (:issue:`2453`).
-  Improved completions for busctl, git (:issue:`2585`, :issue:`2879`, :issue:`2984`), and
   netctl.

--------------

fish 2.3b1 (released April 19, 2016)
====================================

.. _significant-changes-3:

Significant Changes
-------------------

-  A new ``string`` builtin to handle… strings! This builtin will
   measure, split, search and replace text strings, including using
   regular expressions. It can also be used to turn lists into plain
   strings using ``join``. ``string`` can be used in place of ``sed``,
   ``grep``, ``tr``, ``cut``, and ``awk`` in many situations. (:issue:`2296`)
-  Allow using escape as the Meta modifier key, by waiting after seeing
   an escape character wait up to 300ms for an additional character.
   This is consistent with readline (e.g. bash) and can be configured
   via the ``fish_escape_delay_ms variable``. This allows using escape
   as the Meta modifier. (:issue:`1356`)
-  Add new directories for vendor functions and configuration snippets
   (:issue:`2500`)
-  A new ``fish_realpath`` builtin and associated ``realpath`` function
   should allow scripts to resolve path names via ``realpath``
   regardless of whether there is an external command of that name;
   albeit with some limitations. See the associated documentation.

Backward-incompatible changes
-----------------------------

-  Unmatched globs will now cause an error, except when used with
   ``for``, ``set`` or ``count`` (:issue:`2719`)
-  ``and`` and ``or`` will now bind to the closest ``if`` or ``while``,
   allowing compound conditions without ``begin`` and ``end`` (:issue:`1428`)
-  ``set -ql`` now searches up to function scope for variables (:issue:`2502`)
-  ``status -f`` will now behave the same when run as the main script or
   using ``source`` (:issue:`2643`)
-  ``source`` no longer puts the file name in ``$argv`` if no arguments
   are given (:issue:`139`)
-  History files are stored under the ``XDG_DATA_HOME`` hierarchy (by
   default, in ``~/.local/share``), and existing history will be moved
   on first use (:issue:`744`)

.. _other-notable-fixes-and-improvements-2:

Other notable fixes and improvements
------------------------------------

-  Fish no longer silences errors in config.fish (:issue:`2702`)
-  Directory autosuggestions will now descend as far as possible if
   there is only one child directory (:issue:`2531`)
-  Add support for bright colors (:issue:`1464`)
-  Allow Ctrl-J (``\cj``) to be bound separately from Ctrl-M
   (``\cm``) (:issue:`217`)
-  psub now has a “-s”/“–suffix” option to name the temporary file with
   that suffix
-  Enable 24-bit colors on select terminals (:issue:`2495`)
-  Support for SVN status in the prompt (:issue:`2582`)
-  Mercurial and SVN support have been added to the Classic + Git (now
   Classic + VCS) prompt (via the new \__fish_vcs_prompt function)
   (:issue:`2592`)
-  export now handles variables with a “=” in the value (:issue:`2403`)
-  New completions for:

   -  alsactl
   -  Archlinux’s asp, makepkg
   -  Atom’s apm (:issue:`2390`)
   -  entr - the “Event Notify Test Runner” (:issue:`2265`)
   -  Fedora’s dnf (:issue:`2638`)
   -  OSX diskutil (:issue:`2738`)
   -  pkgng (:issue:`2395`)
   -  pulseaudio’s pacmd and pactl
   -  rust’s rustc and cargo (:issue:`2409`)
   -  sysctl (:issue:`2214`)
   -  systemd’s machinectl (:issue:`2158`), busctl (:issue:`2144`), systemd-nspawn,
      systemd-analyze, localectl, timedatectl
   -  and more

-  Fish no longer has a function called sgrep, freeing it for user
   customization (:issue:`2245`)
-  A rewrite of the completions for cd, fixing a few bugs (:issue:`2299`, :issue:`2300`,
   :issue:`562`)
-  Linux VTs now run in a simplified mode to avoid issues (:issue:`2311`)
-  The vi-bindings now inherit from the emacs bindings
-  Fish will also execute ``fish_user_key_bindings`` when in vi-mode
-  ``funced`` will now also check $VISUAL (:issue:`2268`)
-  A new ``suspend`` function (:issue:`2269`)
-  Subcommand completion now works better with split /usr (:issue:`2141`)
-  The command-not-found-handler can now be overridden by defining a
   function called ``__fish_command_not_found_handler`` in config.fish
   (:issue:`2332`)
-  A few fixes to the Sorin theme
-  PWD shortening in the prompt can now be configured via the
   ``fish_prompt_pwd_dir_length`` variable, set to the length per path
   component (:issue:`2473`)
-  fish no longer requires ``/etc/fish/config.fish`` to correctly start,
   and now ships a skeleton file that only contains some documentation
   (:issue:`2799`)

--------------

fish 2.2.0 (released July 12, 2015)
===================================

.. _significant-changes-4:

Significant changes
-------------------

-  Abbreviations: the new ``abbr`` command allows for
   interactively-expanded abbreviations, allowing quick access to
   frequently-used commands (:issue:`731`).
-  Vi mode: run ``fish_vi_mode`` to switch fish into the key bindings
   and prompt familiar to users of the Vi editor (:issue:`65`).
-  New inline and interactive pager, which will be familiar to users of
   zsh (:issue:`291`).
-  Underlying architectural changes: the ``fishd`` universal variable
   server has been removed as it was a source of many bugs and security
   problems. Notably, old fish sessions will not be able to communicate
   universal variable changes with new fish sessions. For best results,
   restart all running instances of ``fish``.
-  The web-based configuration tool has been redesigned, featuring a
   prompt theme chooser and other improvements.
-  New German, Brazilian Portuguese, and Chinese translations.

.. _backward-incompatible-changes-1:

Backward-incompatible changes
-----------------------------

These are kept to a minimum, but either change undocumented features or
are too hard to use in their existing forms. These changes may break
existing scripts.

-  ``commandline`` no longer interprets functions “in reverse”, instead
   behaving as expected (:issue:`1567`).
-  The previously-undocumented ``CMD_DURATION`` variable is now set for
   all commands and contains the execution time of the last command in
   milliseconds (:issue:`1585`). It is no longer exported to other commands
   (:issue:`1896`).
-  ``if`` / ``else`` conditional statements now return values consistent
   with the Single Unix Specification, like other shells (:issue:`1443`).
-  A new “top-level” local scope has been added, allowing local
   variables declared on the commandline to be visible to subsequent
   commands. (:issue:`1908`)

.. _other-notable-fixes-and-improvements-3:

Other notable fixes and improvements
------------------------------------

-  New documentation design (:issue:`1662`), which requires a Doxygen version
   1.8.7 or newer to build.
-  Fish now defines a default directory for other packages to provide
   completions. By default this is
   ``/usr/share/fish/vendor-completions.d``; on systems with
   ``pkgconfig`` installed this path is discoverable with
   ``pkg-config --variable completionsdir fish``.
-  A new parser removes many bugs; all existing syntax should keep
   working.
-  New ``fish_preexec`` and ``fish_postexec`` events are fired before
   and after job execution respectively (:issue:`1549`).
-  Unmatched wildcards no longer prevent a job from running. Wildcards
   used interactively will still print an error, but the job will
   proceed and the wildcard will expand to zero arguments (:issue:`1482`).
-  The ``.`` command is deprecated and the ``source`` command is
   preferred (:issue:`310`).
-  ``bind`` supports “bind modes”, which allows bindings to be set for a
   particular named mode, to support the implementation of Vi mode.
-  A new ``export`` alias, which behaves like other shells (:issue:`1833`).
-  ``command`` has a new ``--search`` option to print the name of the
   disk file that would be executed, like other shells’ ``command -v``
   (:issue:`1540`).
-  ``commandline`` has a new ``--paging-mode`` option to support the new
   pager.
-  ``complete`` has a new ``--wraps`` option, which allows a command to
   (recursively) inherit the completions of a wrapped command (:issue:`393`),
   and ``complete -e`` now correctly erases completions (:issue:`380`).
-  Completions are now generated from manual pages by default on the
   first run of fish (:issue:`997`).
-  ``fish_indent`` can now produce colorized (``--ansi``) and HTML
   (``--html``) output (:issue:`1827`).
-  ``functions --erase`` now prevents autoloaded functions from being
   reloaded in the current session.
-  ``history`` has a new ``--merge`` option, to incorporate history from
   other sessions into the current session (:issue:`825`).
-  ``jobs`` returns 1 if there are no active jobs (:issue:`1484`).
-  ``read`` has several new options:
-  ``--array`` to break input into an array (:issue:`1540`)
-  ``--null`` to break lines on NUL characters rather than newlines
   (:issue:`1694`)
-  ``--nchars`` to read a specific number of characters (:issue:`1616`)
-  ``--right-prompt`` to display a right-hand-side prompt during
   interactive read (:issue:`1698`).
-  ``type`` has a new ``-q`` option to suppress output (:issue:`1540` and, like
   other shells, ``type -a`` now prints all matches for a command
   (:issue:`261`).
-  Pressing :kbd:`f1` now shows the manual page for the current command
   (:issue:`1063`).
-  ``fish_title`` functions have access to the arguments of the
   currently running argument as ``$argv[1]`` (:issue:`1542`).
-  The OS command-not-found handler is used on Arch Linux (:issue:`1925`), nixOS
   (:issue:`1852`), openSUSE and Fedora (:issue:`1280`).
-  ``Alt``\ +\ ``.`` searches backwards in the token history, mapping to
   the same behavior as inserting the last argument of the previous
   command, like other shells (:issue:`89`).
-  The ``SHLVL`` environment variable is incremented correctly (:issue:`1634` &
   :issue:`1693`).
-  Added completions for ``adb`` (:issue:`1165` & :issue:`1211`), ``apt`` (:issue:`2018`),
   ``aura`` (:issue:`1292`), ``composer`` (:issue:`1607`), ``cygport`` (:issue:`1841`),
   ``dropbox`` (:issue:`1533`), ``elixir`` (:issue:`1167`), ``fossil``, ``heroku``
   (:issue:`1790`), ``iex`` (:issue:`1167`), ``kitchen`` (:issue:`2000`), ``nix`` (:issue:`1167`),
   ``node``/``npm`` (:issue:`1566`), ``opam`` (:issue:`1615`), ``setfacl`` (:issue:`1752`),
   ``tmuxinator`` (:issue:`1863`), and ``yast2`` (:issue:`1739`).
-  Improved completions for ``brew`` (:issue:`1090` & :issue:`1810`), ``bundler``
   (:issue:`1779`), ``cd`` (:issue:`1135`), ``emerge`` (:issue:`1840`),\ ``git`` (:issue:`1680`, :issue:`1834` &
   :issue:`1951`), ``man`` (:issue:`960`), ``modprobe`` (:issue:`1124`), ``pacman`` (:issue:`1292`),
   ``rpm`` (:issue:`1236`), ``rsync`` (:issue:`1872`), ``scp`` (:issue:`1145`), ``ssh`` (:issue:`1234`),
   ``sshfs`` (:issue:`1268`), ``systemctl`` (:issue:`1462`, :issue:`1950` & :issue:`1972`), ``tmux``
   (:issue:`1853`), ``vagrant`` (:issue:`1748`), ``yum`` (:issue:`1269`), and ``zypper``
   (:issue:`1787`).

--------------

fish 2.1.2 (released Feb 24, 2015)
==================================

fish 2.1.2 contains a workaround for a filesystem bug in Mac OS X
Yosemite. :issue:`1859`

Specifically, after installing fish 2.1.1 and then rebooting, “Verify
Disk” in Disk Utility will report “Invalid number of hard links.” We
don’t have any reports of data loss or other adverse consequences. fish
2.1.2 avoids triggering the bug, but does not repair an already affected
filesystem. To repair the filesystem, you can boot into Recovery Mode
and use Repair Disk from Disk Utility. Linux and versions of OS X prior
to Yosemite are believed to be unaffected.

There are no other changes in this release.

--------------

fish 2.1.1 (released September 26, 2014)
========================================

**Important:** if you are upgrading, stop all running instances of
``fishd`` as soon as possible after installing this release; it will be
restarted automatically. On most systems, there will be no further
action required. Note that some environments (where ``XDG_RUNTIME_DIR``
is set), such as Fedora 20, will require a restart of all running fish
processes before universal variables work as intended.

Distributors are highly encouraged to call ``killall fishd``,
``pkill fishd`` or similar in installation scripts, or to warn their
users to do so.

Security fixes
--------------

-  The fish_config web interface now uses an authentication token to
   protect requests and only responds to requests from the local machine
   with this token, preventing a remote code execution attack. (closing
   CVE-2014-2914). :issue:`1438`
-  ``psub`` and ``funced`` are no longer vulnerable to attacks which
   allow local privilege escalation and data tampering (closing
   CVE-2014-2906 and CVE-2014-3856). :issue:`1437`
-  ``fishd`` uses a secure path for its socket, preventing a local
   privilege escalation attack (closing CVE-2014-2905). :issue:`1436`
-  ``__fish_print_packages`` is no longer vulnerable to attacks which
   would allow local privilege escalation and data tampering (closing
   CVE-2014-3219). :issue:`1440`

Other fixes
-----------

-  ``fishd`` now ignores SIGPIPE, fixing crashes using tools like GNU
   Parallel and which occurred more often as a result of the other
   ``fishd`` changes. :issue:`1084` & :issue:`1690`

--------------

fish 2.1.0
==========

.. _significant-changes-5:

Significant Changes
-------------------

-  **Tab completions will fuzzy-match files.** :issue:`568`

   When tab-completing a file, fish will first attempt prefix matches
   (``foo`` matches ``foobar``), then substring matches (``ooba``
   matches ``foobar``), and lastly subsequence matches (``fbr`` matches
   ``foobar``). For example, in a directory with files foo1.txt,
   foo2.txt, foo3.txt…, you can type only the numeric part and hit tab
   to fill in the rest.

   This feature is implemented for files and executables. It is not yet
   implemented for options (like ``--foobar``), and not yet implemented
   across path components (like ``/u/l/b`` to match ``/usr/local/bin``).

-  **Redirections now work better across pipelines.** :issue:`110`, :issue:`877`

   In particular, you can pipe stderr and stdout together, for example,
   with ``cmd ^&1 | tee log.txt``, or the more familiar
   ``cmd 2>&1 | tee log.txt``.

-  **A single ``%`` now expands to the last job backgrounded.** :issue:`1008`

   Previously, a single ``%`` would pid-expand to either all
   backgrounded jobs, or all jobs owned by your user. Now it expands to
   the last job backgrounded. If no job is in the background, it will
   fail to expand. In particular, ``fg %`` can be used to put the most
   recent background job in the foreground.

Other Notable Fixes
-------------------

-  alt-U and alt+C now uppercase and capitalize words, respectively.
   :issue:`995`

-  VTE based terminals should now know the working directory. :issue:`906`

-  The autotools build now works on Mavericks. :issue:`968`

-  The end-of-line binding (ctrl+E) now accepts autosuggestions. :issue:`932`

-  Directories in ``/etc/paths`` (used on OS X) are now prepended
   instead of appended, similar to other shells. :issue:`927`

-  Option-right-arrow (used for partial autosuggestion completion) now
   works on iTerm2. :issue:`920`

-  Tab completions now work properly within nested subcommands. :issue:`913`

-  ``printf`` supports ``\e``, the escape character. :issue:`910`

-  ``fish_config history`` no longer shows duplicate items. :issue:`900`

-  ``$fish_user_paths`` is now prepended to $PATH instead of appended.
   :issue:`888`

-  Jobs complete when all processes complete. :issue:`876`

   For example, in previous versions of fish, ``sleep 10 | echo Done``
   returns control immediately, because echo does not read from stdin.
   Now it does not complete until sleep exits (presumably after 10
   seconds).

-  Better error reporting for square brackets. :issue:`875`

-  fish no longer tries to add ``/bin`` to ``$PATH`` unless PATH is
   totally empty. :issue:`852`

-  History token substitution (alt-up) now works correctly inside
   subshells. :issue:`833`

-  Flow control is now disabled, freeing up ctrl-S and ctrl-Q for other
   uses. :issue:`814`

-  sh-style variable setting like ``foo=bar`` now produces better error
   messages. :issue:`809`

-  Commands with wildcards no longer produce autosuggestions. :issue:`785`

-  funced no longer freaks out when supplied with no arguments. :issue:`780`

-  fish.app now works correctly in a directory containing spaces. :issue:`774`

-  Tab completion cycling no longer occasionally fails to repaint. :issue:`765`

-  Comments now work in eval’d strings. :issue:`684`

-  History search (up-arrow) now shows the item matching the
   autosuggestion, if that autosuggestion was truncated. :issue:`650`

-  Ctrl-T now transposes characters, as in other shells. :issue:`128`

--------------

fish 2.0.0
==========

.. _significant-changes-6:

Significant Changes
-------------------

-  **Command substitutions now modify ``$status`` :issue:`547`.** Previously the
   exit status of command substitutions (like ``(pwd)``) was ignored;
   however now it modifies $status. Furthermore, the ``set`` command now
   only sets $status on failure; it is untouched on success. This allows
   for the following pattern:

   .. code:: sh

      if set python_path (which python)
         ...
      end

   Because set does not modify $status on success, the if branch
   effectively tests whether ``which`` succeeded, and if so, whether the
   ``set`` also succeeded.

-  Improvements to PATH handling. There is a new variable, fish_user_paths,
   which can be set universally, and whose contents are appended to
   $PATH :issue:`527`

   -  /etc/paths and /etc/paths.d are now respected on OS X
   -  fish no longer modifies $PATH to find its own binaries

-  **Long lines no longer use ellipsis for line breaks**, and copy and
   paste should no longer include a newline even if the line was broken
   :issue:`300`

-  **New syntax for index ranges** (sometimes known as “slices”) :issue:`212`

-  **fish now supports an ``else if`` statement** :issue:`134`

-  **Process and pid completion now works on OS X** :issue:`129`

-  **fish is now relocatable**, and no longer depends on compiled-in
   paths :issue:`125`

-  **fish now supports a right prompt (RPROMPT)** through the
   fish_right_prompt function :issue:`80`

-  **fish now uses posix_spawn instead of fork when possible**, which is
   much faster on BSD and OS X :issue:`11`

.. _other-notable-fixes-1:

Other Notable Fixes
-------------------

-  Updated VCS completions (darcs, cvs, svn, etc.)
-  Avoid calling getcwd on the main thread, as it can hang :issue:`696`
-  Control-D (forward delete) no longer stops at a period :issue:`667`
-  Completions for many new commands
-  fish now respects rxvt’s unique keybindings :issue:`657`
-  xsel is no longer built as part of fish. It will still be invoked if
   installed separately :issue:`633`
-  \__fish_filter_mime no longer spews :issue:`628`
-  The –no-execute option to fish no longer falls over when reaching the
   end of a block :issue:`624`
-  fish_config knows how to find fish even if it’s not in the $PATH :issue:`621`
-  A leading space now prevents writing to history, as is done in bash
   and zsh :issue:`615`
-  Hitting enter after a backslash only goes to a new line if it is
   followed by whitespace or the end of the line :issue:`613`
-  printf is now a builtin :issue:`611`
-  Event handlers should no longer fire if signals are blocked :issue:`608`
-  set_color is now a builtin :issue:`578`
-  man page completions are now located in a new generated_completions
   directory, instead of your completions directory :issue:`576`
-  tab now clears autosuggestions :issue:`561`
-  tab completion from within a pair of quotes now attempts to
   “appropriate” the closing quote :issue:`552`
-  $EDITOR can now be a list: for example, ``set EDITOR gvim -f``) :issue:`541`
-  ``case`` bodies are now indented :issue:`530`
-  The profile switch ``-p`` no longer crashes :issue:`517`
-  You can now control-C out of ``read`` :issue:`516`
-  ``umask`` is now functional on OS X :issue:`515`
-  Avoid calling getpwnam on the main thread, as it can hang :issue:`512`
-  Alt-F or Alt-right-arrow (Option-F or option-right-arrow) now accepts
   one word of an autosuggestion :issue:`435`
-  Setting fish as your login shell no longer kills OpenSUSE :issue:`367`
-  Backslashes now join lines, instead of creating multiple commands
   :issue:`347`
-  echo now implements the -e flag to interpret escapes :issue:`337`
-  When the last token in the user’s input contains capital letters, use
   its case in preference to that of the autosuggestion :issue:`335`
-  Descriptions now have their own muted color :issue:`279`
-  Wildcards beginning with a . (for example, ``ls .*``) no longer match
   . and .. :issue:`270`
-  Recursive wildcards now handle symlink loops :issue:`268`
-  You can now delete history items from the fish_config web interface
   :issue:`250`
-  The OS X build now weak links ``wcsdup`` and ``wcscasecmp`` :issue:`240`
-  fish now saves and restores the process group, which prevents certain
   processes from being erroneously reported as stopped :issue:`197`
-  funced now takes an editor option :issue:`187`
-  Alternating row colors are available in fish pager through
   ``fish_pager_color_secondary`` :issue:`186`
-  Universal variable values are now stored based on your MAC address,
   not your hostname :issue:`183`
-  The caret ^ now only does a stderr redirection if it is the first
   character of a token, making git users happy :issue:`168`
-  Autosuggestions will no longer cause line wrapping :issue:`167`
-  Better handling of Unicode combining characters :issue:`155`
-  fish SIGHUPs processes more often :issue:`138`
-  fish no longer causes ``sudo`` to ask for a password every time
-  fish behaves better under Midnight Commander :issue:`121`
-  ``set -e`` no longer crashes :issue:`100`
-  fish now will automatically import history from bash, if there is no
   fish history :issue:`66`
-  Backslashed-newlines inside quoted strings now behave more
   intuitively :issue:`52`
-  Tab titles should be shown correctly in iTerm2 :issue:`47`
-  scp remote path completion now sometimes works :issue:`42`
-  The ``read`` builtin no longer shows autosuggestions :issue:`29`
-  Custom key bindings can now be set via the ``fish_user_key_bindings``
   function :issue:`21`
-  All Python scripts now run correctly under both Python 2 and Python 3
   :issue:`14`
-  The “accept autosuggestion” key can now be configured :issue:`19`
-  Autosuggestions will no longer suggest invalid commands :issue:`6`

--------------

fishfish Beta r2
================

Bug Fixes
---------

-  **Implicit cd** is back, for paths that start with one or two dots, a
   slash, or a tilde.
-  **Overrides of default functions should be fixed.** The “internalized
   scripts” feature is disabled for now.
-  **Disabled delayed suspend.** This is a strange job-control feature
   of BSD systems, including OS X. Disabling it frees up Control Y for
   other purposes; in particular, for yank, which now works on OS X.
-  **fish_indent is fixed.** In particular, the ``funced`` and
   ``funcsave`` functions work again.
-  A SIGTERM now ends the whole execution stack again (resolving :issue:`13`).
-  Bumped the \__fish_config_interactive version number so the default
   fish_color_autosuggestion kicks in.
-  fish_config better handles combined term256 and classic colors like
   “555 yellow”.

New Features
------------

-  **A history builtin**, and associated interactive function that
   enables deleting history items. Example usage: \* Print all history
   items beginning with echo: ``history --prefix echo`` \* Print all
   history items containing foo: ``history --contains foo`` \*
   Interactively delete some items containing foo:
   ``history --delete --contains foo``

Credit to @siteshwar for implementation. Thanks @siteshwar!

--------------

fishfish Beta r1
================

Scripting
---------

-  No changes! All existing fish scripts, config files, completions,
   etc. from trunk should continue to work.

.. _new-features-1:

New Features
------------

-  **Autosuggestions**. Think URL fields in browsers. When you type a
   command, fish will suggest the rest of the command after the cursor,
   in a muted gray when possible. You can accept the suggestion with the
   right arrow key or Ctrl-F. Suggestions come from command history,
   completions, and some custom code for cd; there’s a lot of potential
   for improvement here. The suggestions are computed on a background
   pthread, so they never slow down your typing. The autosuggestion
   feature is incredible. I miss it dearly every time I use anything
   else.

-  **term256 support** where available, specifically modern xterms and
   OS X Lion. You can specify colors the old way (‘set_color cyan’) or
   by specifying RGB hex values (‘set_color FF3333’); fish will pick the
   closest supported color. Some xterms do not advertise term256 support
   either in the $TERM or terminfo max_colors field, but nevertheless
   support it. For that reason, fish will default into using it on any
   xterm (but it can be disabled with an environment variable).

-  **Web-based configuration** page. There is a new function
   ‘fish_config’. This spins up a simple Python web server and opens a
   browser window to it. From this web page, you can set your shell
   colors and view your functions, variables, and history; all changes
   apply immediately to all running shells. Eventually all configuration
   ought to be supported via this mechanism (but in addition to, not
   instead of, command line mechanisms).

-  **Man page completions**. There is a new function
   ‘fish_update_completions’. This function reads all the man1 files
   from your manpath, removes the roff formatting, parses them to find
   the commands and options, and outputs fish completions into
   ~/.config/fish/completions. It won’t overwrite existing completion
   files (except ones that it generated itself).

Programmatic Changes
--------------------

-  fish is now entirely in C++. I have no particular love for C++, but
   it provides a ready memory-model to replace halloc. We’ve made an
   effort to keep it to a sane and portable subset (no C++11, no boost,
   no going crazy with templates or smart pointers), but we do use the
   STL and a little tr1.
-  halloc is entirely gone, replaced by normal C++ ownership semantics.
   If you don’t know what halloc is, well, now you have two reasons to
   be happy.
-  All the crufty C data structures are entirely gone. array_list_t,
   priority_queue_t, hash_table_t, string_buffer_t have been removed and
   replaced by STL equivalents like std::vector, std::map, and
   std::wstring. A lot of the string handling now uses std::wstring
   instead of wchar_t \*
-  fish now spawns pthreads for tasks like syntax highlighting that
   require blocking I/O.
-  History has been completely rewritten. History files now use an
   extensible YAML-style syntax. History “merging” (multiple shells
   writing to the same history file) now works better. There is now a
   maximum history length of about 250k items (256 \* 1024).
-  The parser has been “instanced,” so you can now create more than one.
-  Total #LoC has shrunk slightly even with the new features.

Performance
-----------

-  fish now runs syntax highlighting in a background thread, so typing
   commands is always responsive even on slow filesystems.
-  echo, test, and pwd are now builtins, which eliminates many forks.
-  The files in share/functions and share/completions now get
   ‘internalized’ into C strings that get compiled in with fish. This
   substantially reduces the number of files touched at startup. A
   consequence is that you cannot change these functions without
   recompiling, but often other functions depend on these “standard”
   functions, so changing them is perhaps not a good idea anyways.

Here are some system call counts for launching and then exiting fish
with the default configuration, on OS X. The first column is fish trunk,
the next column is with our changes, and the last column is bash for
comparison. This data was collected via dtrace.

.. raw:: html

   <table>

.. raw:: html

   <tr>

.. raw:: html

   <th>

.. raw:: html

   <th>

before

.. raw:: html

   <th>

after

.. raw:: html

   <th>

bash

.. raw:: html

   <tr>

.. raw:: html

   <th>

open

.. raw:: html

   <td>

9

.. raw:: html

   <td>

4

.. raw:: html

   <td>

5

.. raw:: html

   <tr>

.. raw:: html

   <th>

fork

.. raw:: html

   <td>

28

.. raw:: html

   <td>

14

.. raw:: html

   <td>

0

.. raw:: html

   <tr>

.. raw:: html

   <th>

stat

.. raw:: html

   <td>

131

.. raw:: html

   <td>

85

.. raw:: html

   <td>

11

.. raw:: html

   <tr>

.. raw:: html

   <th>

lstat

.. raw:: html

   <td>

670

.. raw:: html

   <td>

0

.. raw:: html

   <td>

0

.. raw:: html

   <tr>

.. raw:: html

   <th>

read

.. raw:: html

   <td>

332

.. raw:: html

   <td>

80

.. raw:: html

   <td>

4

.. raw:: html

   <tr>

.. raw:: html

   <th>

write

.. raw:: html

   <td>

172

.. raw:: html

   <td>

149

.. raw:: html

   <td>

0

.. raw:: html

   </table>

The large number of forks relative to bash are due to fish’s insanely
expensive default prompt, which is unchanged in my version. If we switch
to a prompt comparable to bash’s (lame) default, the forks drop to 16
with trunk, 4 after our changes.

The large reduction in lstat() numbers is due to fish no longer needing
to call ttyname() on OS X.

We’ve got some work to do to be as lean as bash, but we’re on the right
track.

.. vim: ft=rst : tw=0 :
