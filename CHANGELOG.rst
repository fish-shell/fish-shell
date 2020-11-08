fish 3.2.0 (released ???)
=========================

Notable improvements and fixes
------------------------------

-  Undo and redo support for the command-line editor and pager search (#1367). By default, undo is bound to Control+Z, and redo to Alt+/.
-  builtins may now output before all data is read. For example, ``string replace`` no longer has to read all of stdin before it can begin to output.
   This makes it usable also for pipes where the previous command hasn't finished yet, like::

    # Show all dmesg lines related to "usb"
    dmesg -w | string match '*usb*'

-  A new variable, ``fish_kill_signal``, is set to the signal that terminated the last foreground job, or ``0`` if the job exited normally (#6824).
-  Control-C no longer kills background jobs for which job control is
   disabled, matching POSIX semantics (#6828).
-  fish is less aggressive about resetting terminal modes, such as flow control, after every command.
   Although flow control remains off by default, enterprising users can now enable it for external commands with
   ``stty`` (#2315). 
-  A new ``fish_add_path`` helper function to add paths to $PATH without producing duplicates, to be used interactively or in ``config.fish`` (#6960).
   For example::

     fish_add_path /opt/mycoolthing/bin

   will add /opt/mycoolthing/bin to the beginning of $fish_user_path without creating duplicates,
   so it can be called again and again from config.fish or just once interactively, and the path will just be there, once.
-  ``fish_preexec`` and ``fish_postexec`` events are no longer triggered for empty commands (#4829).
-  The ``test`` builtin now better shows where an error occured (#6030)::

    > test 1 = 2 and echo true or false
    test: Expected a combining operator like '-a' at index 4
    1 = 2 and echo true or echo false
          ^

-  ``set`` and backgrounded jobs no longer overwrite ``$pipestatus`` (#6820), improving its use in command substitutions (#6998).
-  The documentation is now presented in a new theme (#6500, #7371).
-  ``fish --no-execute`` will no longer complain about unknown commands
   or non-matching wildcards, as these could be defined differently at
   runtime (especially for functions). This makes it usable as a static syntax checker (#977).
-  ``type`` is now a builtin and therefore much faster (#7342).

Syntax changes and new commands
-------------------------------
-  Range limits in index range expansions like ``$x[$start..$end]`` may be omitted: ``$start`` and ``$end`` default to 1 and -1 (the last item) respectively.
-  Logical operators ``&&`` and ``||`` can be followed by newlines before their right operand, matching POSIX shells.

Scripting improvements
----------------------
-  A new subcommand, ``string pad``, allows extending strings to a given width (#7340).
-  ``string sub`` has a new ``--end`` option to specify the end index of
   a substring (#6765).
-  ``string split`` has a new ``--fields`` option to specify fields to
   output, similar to ``cut -f`` (#6770).
-  ``printf`` no longer prints an error if not given an argument (not
   even a format string)
-  The ``true`` and ``false`` builtins ignore any arguments, like other shells (#7030).
-  Computed ("electric") variables such as ``status`` are now only global in scope, so ``set -Uq status`` returns false (#7032).
-  The output for ``set --show`` has been shortened, only mentioning the scopes in which a variable exists (#6944).
-  A new ``fish_posterror`` event is emitted when attempting to execute a command with syntax errors (#6880).
- ``fish_indent`` now removes spurious quotes in simple cases (#6722)
   and learned a ``--check`` option to just check if a file is indented correctly (#7251).
- ``pushd`` only adds a directory to the stack if changing to it was successful (#6947).
-  Added a ``fish_job_summary`` function which is called whenever a
   background job stops or ends, or any job terminates from a signal (#6959).
   The default behaviour can now be customized by redefining this
   function.
-  The ``fish_prompt`` event no longer fires when ``read`` is used. If
   you need a function to run any time ``read`` is invoked by a script,
   use the new ``fish_read`` event instead (#7039).
-  ``status`` gained new ``dirname`` and ``basename`` convenience subcommands
   to get just the directory to the running script or the name of it,
   to simplify common tasks such as running ``(dirname (status filename))`` (#7076).
-  The ``_`` gettext function is now implemented as a builtin for performance purposes (#7036).
-  Broken pipelines are now handled more smoothly; in particular, bad redirection mid-pipeline
   results in the job continuing to run but with the broken file descriptor replaced with a closed
   file descriptor. This allows better error recovery and is more in line with other shells'
   behaviour (#7038).
-  ``jobs --quiet PID`` no longer prints "no suitable job" if the job for PID does not exist (eg because it has finished) (#6809).
-  All builtins that query if something exists now take ``--query`` as the long form for ``-q``. ``--quiet`` is deprecated for ``command``, ``jobs`` and ``type`` (#7276).
-  ``argparse`` now only prints a backtrace with invalid options to argparse itself (#6703).
-  ``complete`` takes the first argument as the name of the command if the ``--command``/``-c`` option is not used (``complete git`` is treated like ``complete --command git``), and can show the loaded completions for specific commands with ``complete COMMANDNAME`` (#7321).
-  ``set_color -b`` (without an argument) no longer prints an error message, matching other invalid invocations of this command (#7154).
-  Functions triggered by the ``fish_exit`` event are correctly run when the terminal is closed or the shell receives SIGHUP (#7014).
-  ``string replace`` no longer errors if a capturing group wasn't matched, instead treating it as empty (#7343).
-  ``exec`` no longer produces a syntax error when the command cannot be found (#6098).
-  ``disown`` should no longer create zombie processes when job control is off, such as in ``config.fish`` (#7183).
-  Using ``read --silent`` while fish is in private mode was adding these potentially-sensitive entries to the history; this has been fixed (#7230).
-  ``set --erase`` and ``abbr --erase`` can now erase multiple things in one go, matching ``functions --erase`` (#7377).
-  ``abbr --erase`` no longer errors on an unset abbreviation (#7376).
-  ``test -t``, for testing whether file descriptors are connected to a terminal, works for file descriptors 0, 1, and 2 (#4766). It can still return incorrect results in other cases (#1228).
-  Trying to run scripts with Windows line endings (CRLF) via the shebang produces a sensible error (#2783).
-  An ``alias`` that delegates to a command with the same name no longer triggers an error about recursive completion (#7389).

Interactive improvements
------------------------

-  The prompt is reprinted after a background job exits (#1018).
-  Prompts whose width exceeds $COLUMNS will now be truncated instead of replaced with `"> "` (#904).
-  fish no longer inserts a space after a completion ending in ``.`` or
   ``,`` is accepted (#6928).
-  When pressing Tab, fish displays ambiguous completions even when they
   have a common prefix, without the user having to press Tab again
   (#6924).
-  If a filename is invalid when first pressing Tab, but becomes valid, it will be completed properly on the next attempt (#6863).
-  Control-Z is now available for binding (#7152).
- ``help string match/replace/<subcommand>`` will show the help for string subcommands (#6786).
-  ``fish_key_reader`` sets the exit status to 0 when used with ``--help`` or ``--version`` (#6964).
-  ``fish_key_reader`` and ``fish_indent`` send output from ``--version`` to standard output, matching other fish binaries (#6964).
-  A new variable ``$status_generation`` is incremented only when the previous command produces a status (#6815). This can be used, for example, to check whether a failure status is a holdover due to a background job, or actually produced by the last run command.
-  ``fish_greeting`` is now a function that reads a variable of the same name, and defaults to setting it globally. This removes a universal variable by default and helps with updating the greeting. However, to disable the greeting it is now necessary to explicitly specify universal scope (``set -U fish_greeting``) or to disable it in config.fish (#7265).
-  Events are properly emitted after a job is cancelled (#2356).
-  A number of new debugging categories have been added, including ``config``, ``path``, ``reader`` and ``screen`` (#6511). See the output of ``fish --print-debug-categories`` for the full list.
-  The enabled debug categories are now printed on shell startup (#7007).
- The ``-o`` short option to fish, for ``--debug-output``, works correctly instead of producing an
  invalid option error (#7254).
-  Abbreviations are now expanded after all command terminators (eg ``;`` or ``|``), not just space, as in fish 2.7.1 and before (#6970), and after closing a command substitution (#6658).
-  The history file is now created with user-private permissions,
   matching other shells (#6926). The directory containing the history
   file was already private, so there should not have been any private data
   revealed.
-  The output of ``time`` is now properly aligned in all cases (#6726).
-  The ``pwd`` command supports the long options ``--logical`` and ``--physical``, matching other implementations (#6787).
-  The command-not-found handling has been simplified. When it can't find a command, fish now just executes a function called ``fish_command_not_found`` instead of firing an event, making it easier to replace and reason about. Shims for backwards-compatibility have been added (#7293).
-  Control-C no longer occasionally prints an "unknown command" error (#7145).
-  Autocompletions work properly after Control-C to cancel the commmand line (#6937).
-  History search is now case-insensitive unless the search string contains an uppercase character (#7273).
-  ``fish_update_completions`` has a new ``-keep`` option, which improves speed by skipping completions that already exist (#6775).
-  Aliases containing an embedded backslash appear properly in the output of ``alias`` (#6910).
-  ``open`` no longer hangs indefinitely as a bug in ``xdg-open`` has been worked around (#7215).
-  Long command lines no longer add a blank line after execution (#6826) and behave better with backspace (#6951).
-  ``functions -t`` works like the long option ``--handlers-type``, as documented, instead of producing an error (#6985).
-  History search now flashes when it found no more results (#7362)
-  Fish's debugging can now also be enabled via $FISH_DEBUG and $FISH_DEBUG_OUTPUT from the outside. This helps with debugging when no commandline options can be passed, like when fish is called in a shebang (#7359).
-  Fish now creates $XDG_RUNTIME_DIR if it does not exist (#7335).
-  ``set_color --print-colors`` now also respects the bold, dim, underline, reverse, italic and background modifiers, to better show their effect (#7314).
-  The fish Web configuration tool (``fish_config``) shows prompts correctly on Termux for Android (#7298) and detects Windows Services for Linux 2 properly (#7027).
-  ``funcsave`` has a new ``--directory`` option to specify the location of the saved function (#7041). 
-  ``help`` works properly on MSYS2 (#7113).
-  Resuming a piped job by its number, like ``fg %1`` has been fixed (#7406).


New or improved bindings
^^^^^^^^^^^^^^^^^^^^^^^^

-  As mentioned above, new readline commands ``undo`` (Control+\_ or Control+Z) and ``redo`` (Alt-/) can be used to revert changes to the command line or the pager search field (#6570).
-  Additionally, using the ``cancel`` readline command (bound to escape by default) right after fish picked an unambiguous completion will undo that (#7433).
-  Vi mode bindings now support ``dh``, ``dl``, ``c0``, ``cf``, ``ct``, ``cF``, ``cT``, ``ch``, ``cl``, ``y0``, ``ci``, ``ca``, ``yi``, ``ya``, ``di``, ``da``, and Control+left/right keys to navigate by word (#6648, #6755, #6769).
-  Vi mode bindings support ``~`` (tilde) to toggle the case of the selected character (#6908).
-  Functions ``up-or-search`` and ``down-or-search`` (up-arrow and down-arrow) can cross empty lines and don't activate search mode if the search fails which makes it easier to use them to move between lines in some situations.
- The readline command ``beginning-of-history`` (Page Up) now moves to the oldest search instead of the youngest - that's ``end-of-history`` (Page Down).
-  A new readline command ``forward-single-char`` moves one character to the right, and if an autosuggestion is available, only take a single character from it (#7217).
-  Readline commands can now be joined with ``or`` as a modifier (adding to ``and``), though only some commands report success or failure (#7217).
-  A new function ``__fish_preview_current_file``, bound to Alt+O, opens the
   current file at the cursor in a pager (#6838).
-  ``edit_command_buffer`` (Alt-E and Alt-V) passes the cursor position
   to the external editor if the editor is recognized (#6138, #6954).
-  ``__fish_prepend_sudo`` (Alt-S) now toggles a ``sudo`` prefix (#7012) and avoids shifting the cursor (#6542).
-  ``__fish_prepend_sudo`` (Alt-S) now uses the previous commandline if the current one is empty,
   to simplify rerunning the previous command with ``sudo`` (#7079).
- ``__fish_toggle_comment_commandline`` (Alt-#) now uncomments and presents the last comment
  from history if the commandline is empty (#7137).
- ``__fish_whatis_current_token`` (Alt-W) prints descriptions for functions and builtins (#7191).
- The definition of "word" and "bigword" for movements was refined, fixing e.g. vi mode's behavior with ``e`` on the second-to-last char, and bigword's behavior with single-char words and non-blank non-graphic characters (#7353, #7354, #4025, #7328, #7325)


Improved prompts
^^^^^^^^^^^^^^^^

-  The default and example prompts print the correct exit status for
   commands prefixed with ``not`` (#6566).
-  git prompts include all untracked files in the repository, not just those in the current
   directory (#6086).
-  The git prompts correctly show stash states (#6876, #7136).
-  The Mercurial prompt correctly shows untracked status (#6906).
-  The ``fish_vcs_prompt`` passes its arguments to the various VCS prompts that it calls (#7033).
-  The Subversion prompt was broken in a number of ways in 3.1.0 and has been restored (#7278).
-  A new helper function ``fish_is_root_user`` simplifies checking for superuser privilege (#7031).

Improved terminal output
^^^^^^^^^^^^^^^^^^^^^^^^

-  A new variable, ``$fish_vi_force_cursor``, can be set to force ``fish_vi_cursor`` to attempt changing the cursor
   shape in vi mode, regardless of terminal (#6968). The ``fish_vi_cursor`` option ``--force-iterm`` has been deprecated.
-  ``diff`` will now colourise output, if supported (#7308).
-  Autosuggestions now show up also when the cursor passes the right
   prompt (#6948) or wraps to the next line (#7213).
-  The cursor shape in Vi mode changes properly in Windows Terminal (#6999).
-  The spurious warning about terminal size in small terminals has been removed (#6980).
-  Dynamic titles are now enabled in Alacritty (#7073).
-  Current working directory updates are enabled in foot (#7099).
-  The width computation for certain emoji agrees better with terminals. In particular, flags now have width 2. (#7237).
-  An issue producing strange status output from commands involving ``not`` has been fixed (#6566).
-  Long command lines are wrapped in all cases, instead of sometimes being put on a new line (#5118).
-  The pager is properly rendered with long command lines selected (#2557).

Completions
^^^^^^^^^^^

-  Added completions for

   -  ``7z``, ``7za`` and ``7zr`` (#7220)
   -  ``alias`` (#7035)
   -  ``apk`` (#7108)
   -  ``asciidoctor`` (#7000)
   -  ``cmark`` (#7000)
   -  ``create_ap`` (#7096)
   -  ``deno`` (#7138)
   -  ``dhclient``
   -  Postgres-related commands ``dropdb``, ``createdb``, ``pg_restore``, ``pg_dump`` and
      ``pg_dumpall`` (#6620)
   -  ``downgrade`` (#6751)
   -  ``gapplication``, ``gdbus``, ``gio`` and ``gresource`` (#7300)
   -  ``gh`` (#7112)
   -  ``gitk``
   -  ``hikari`` (#7083)
   -  ``imv`` (#6675)
   -  ``julia`` (#7468)
   -  ``k3d`` (#7202)
   -  ``micro`` (#7339)
   -  ``mpc`` (#7169)
   -  Metasploit's ``msfconsole``, ``msfdb`` and ``msfvenom`` (#6930)
   -  ``ncat``, ``nc.openbsd`` and ``nc.traditional`` (#6873)
   -  ``nmap`` (#6873)
   -  ``prime-run`` (#7241)
   -  ``ps2pdf{12,13,14,wr}`` (#6673)
   -  ``pyenv`` (#6551)
   -  ``rst2html``, ``rst2html4``, ``rst2html5``, ``rst2latex``,
      ``rst2man``, ``rst2odt``, ``rst2pseudoxml``, ``rst2s5``,
      ``rst2xetex``, ``rst2xml`` and ``rstpep2html`` (#7019)
   -  ``spago`` (#7381)
   -  ``sphinx-apidoc``, ``sphinx-autogen``, ``sphinx-build`` and
      ``sphinx-quickstart`` (#7000)
   -  ``strace`` (#6656)
   -  ``tcpdump`` (#6690)
   -  ``tig``
   -  ``windscribe`` (#6788)
   -  ``wireshark``, ``tshark``, and ``dumpcap``
   -  ``xbps-*`` (#7239)
   -  ``xxhsum``, ``xxh32sum``, ``xxh64sum`` and ``xxh128sum`` (#7103)
   -  ``yadm`` (#7100)
   -  ``zopfli`` and ``zopflipng``

- Lots of improvements to completions.
- Improvements to the manpage completion generator (#7086).
- Significant performance improvements to completion of the available commands (#7153).

Deprecations and removed features
---------------------------------
- fish no longer attempts to modify the terminal size via `TIOCSWINSZ` (#6994).
- The `fish_color_match` variable is no longer used. (Previously this controlled the color of matching quotes and parens when using `read`).
- fish 3.2.0 will be the last release in which the redirection to standard error with the ``^`` character is enabled. The ``stderr-nocaret`` feature flag will be changed to "on" in future releases. 

For distributors and developers
-------------------------------

-  fish source tarballs are now distributed using the XZ compression
   method (#5460).
-  The CMake variable ``MAC_CODESIGN_ID`` can now be set to "off" to disable code-signing (#6952).
-  Building on on macOS earlier than 10.13.6 succeeds, instead of failing on code-signing (#6791).
-  The pkg-config file now uses variables to ensure paths used are portable across prefixes.
-  The default values for the ``extra_completionsdir``, ``extra_functionsdir``
   and ``extra_confdir`` options now use the installation prefix rather than ``/usr/local``.
-  A new CMake variable ``FISH_USE_SYSTEM_PCRE2`` controls whether fish
   builds with the system-installed PCRE2, or the version it bundles. By
   default it prefers the system library if available, unless Mac
   codesigning is enabled (#6952).
-  Running the full interactive test suite now requires Python 3.5+ and the pexpect package (#6825); the expect package is no longer required.
-  Support for Python 2 in fish's tools (``fish_config`` and the manual page completion generator) is no longer guaranteed. Please use Python 3.5 or later (#6537).

--------------

fish 3.1.2 (released April 29, 2020)
====================================

This release of fish fixes a major issue discovered in fish 3.1.1:

-  Commands such as ``fzf`` and ``enhancd``, when used with ``eval``,
   would hang. ``eval`` buffered output too aggressively, which has been
   fixed (#6955).

If you are upgrading from version 3.0.0 or before, please also review
the release notes for 3.1.1, 3.1.0 and 3.1b1 (included below).

--------------

fish 3.1.1 (released April 27, 2020)
====================================

This release of fish fixes a number of major issues discovered in fish
3.1.0.

-  Commands which involve ``. ( ... | psub)`` now work correctly, as a
   bug in the ``function --on-job-exit`` option has been fixed (#6613).
-  Conflicts between upstream packages for ripgrep and bat, and the fish
   packages, have been resolved (#5822).
-  Starting fish in a directory without read access, such as via ``su``,
   no longer crashes (#6597).
-  Glob ordering changes which were introduced in 3.1.0 have been
   reverted, returning the order of globs to the previous state (#6593).
-  Redirections using the deprecated caret syntax to a file descriptor
   (eg ``^&2``) work correctly (#6591).
-  Redirections that append to a file descriptor (eg ``2>>&1``) work
   correctly (#6614).
-  Building fish on macOS (#6602) or with new versions of GCC (#6604,
   #6609) is now successful.
-  ``time`` is now correctly listed in the output of ``builtin -n``, and
   ``time --help`` works correctly (#6598).
-  Exported universal variables now update properly (#6612).
-  ``status current-command`` gives the expected output when used with
   an environment override - that is, ``F=B status current-command``
   returns ``status`` instead of ``F=B`` (#6635).
-  ``test`` no longer crashes when used with “``nan``” or “``inf``”
   arguments, erroring out instead (#6655).
-  Copying from the end of the command line no longer crashes fish
   (#6680).
-  ``read`` no longer removes multiple separators when splitting a
   variable into a list, restoring the previous behaviour from fish 3.0
   and before (#6650).
-  Functions using ``--on-job-exit`` and ``--on-process-exit`` work
   reliably again (#6679).
-  Functions using ``--on-signal INT`` work reliably in interactive
   sessions, as they did in fish 2.7 and before (#6649). These handlers
   have never worked in non-interactive sessions, and making them work
   is an ongoing process.
-  Functions using ``--on-variable`` work reliably with variables which
   are set implicitly (rather than with ``set``), such as
   “``fish_bind_mode``” and “``PWD``” (#6653).
-  256 colors are properly enabled under certain conditions that were
   incorrectly detected in fish 3.1.0 (``$TERM`` begins with xterm, does
   not include “``256color``”, and ``$TERM_PROGRAM`` is not set)
   (#6701).
-  The Mercurial (``hg``) prompt no longer produces an error when the
   current working directory is removed (#6699). Also, for performance
   reasons it shows only basic information by default; to restore the
   detailed status, set ``$fish_prompt_hg_show_informative_status``.
-  The VCS prompt, ``fish_vcs_prompt``, no longer displays Subversion
   (``svn``) status by default, due to the potential slowness of this
   operation (#6681).
-  Pasting of commands has been sped up (#6713).
-  Using extended Unicode characters, such as emoji, in a non-Unicode
   capable locale (such as the ``C`` or ``POSIX`` locale) no longer
   renders all output blank (#6736).
-  ``help`` prefers to use ``xdg-open``, avoiding the use of ``open`` on
   Debian systems where this command is actually ``openvt`` (#6739).
-  Command lines starting with a space, which are not saved in history,
   now do not get autosuggestions. This fixes an issue with Midnight
   Commander integration (#6763), but may be changed in a future
   version.
-  Copying to the clipboard no longer inserts a newline at the end of
   the content, matching fish 2.7 and earlier (#6927).
-  ``fzf`` in complex pipes no longer hangs. More generally, code run as
   part of command substitutions or ``eval`` will no longer have
   separate process groups. (#6624, #6806).

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
   brace expansion not occurring (#6564).
-  Fixes a number of problems in compiling and testing on Cygwin
   (#6549) and Solaris-derived systems such as Illumos (#6553, #6554,
   #6555, #6556, and #6558).
-  Fixes the process for building macOS packages.
-  Fixes a regression where excessive error messages are printed if
   Unicode characters are emitted in non-Unicode-capable locales
   (#6584).
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
   (#5632).
-  fish no longer buffers pipes to the last function in a pipeline,
   improving many cases where pipes appeared to block or hang (#1396).
-  An overhaul of error messages for builtin commands, including a
   removal of the overwhelming usage summary, more readable stack traces
   (#3404, #5434), and stack traces for ``test`` (aka ``[``) (#5771).
-  fish’s debugging arguments have been significantly improved. The
   ``--debug-level`` option has been removed, and a new ``--debug``
   option replaces it. This option accepts various categories, which may
   be listed via ``fish --print-debug-categories`` (#5879). A new
   ``--debug-output`` option allows for redirection of debug output.
-  ``string`` has a new ``collect`` subcommand for use in command
   substitutions, producing a single output instead of splitting on new
   lines (similar to ``"$(cmd)"`` in other shells) (#159).
-  The fish manual, tutorial and FAQ are now available in ``man`` format
   as ``fish-doc``, ``fish-tutorial`` and ``fish-faq`` respectively
   (#5521).
-  Like other shells, ``cd`` now always looks for its argument in the
   current directory as a last resort, even if the ``CDPATH`` variable
   does not include it or “.” (#4484).
-  fish now correctly handles ``CDPATH`` entries that start with ``..``
   (#6220) or contain ``./`` (#5887).
-  The ``fish_trace`` variable may be set to trace execution (#3427).
   This performs a similar role as ``set -x`` in other shells.
-  fish uses the temporary directory determined by the system, rather
   than relying on ``/tmp`` (#3845).
-  The fish Web configuration tool (``fish_config``) prints a list of
   commands it is executing, to help understanding and debugging
   (#5584).
-  Major performance improvements when pasting (#5866), executing lots
   of commands (#5905), importing history from bash (#6295), and when
   completing variables that might match ``$history`` (#6288).

.. _syntax-changes-and-new-commands-1:

Syntax changes and new commands
-------------------------------

-  A new builtin command, ``time``, which allows timing of fish
   functions and builtins as well as external commands (#117).
-  Brace expansion now only takes place if the braces include a “,” or a
   variable expansion, meaning common commands such as
   ``git reset HEAD@{0}`` do not require escaping (#5869).
-  New redirections ``&>`` and ``&|`` may be used to redirect or pipe
   stdout, and also redirect stderr to stdout (#6192).
-  ``switch`` now allows arguments that expand to nothing, like empty
   variables (#5677).
-  The ``VAR=val cmd`` syntax can now be used to run a command in a
   modified environment (#6287).
-  ``and`` is no longer recognised as a command, so that nonsensical
   constructs like ``and and and`` produce a syntax error (#6089).
-  ``math``\ ‘s exponent operator,’\ ``^``\ ‘, was previously
   left-associative, but now uses the more commonly-used
   right-associative behaviour (#6280). This means that
   ``math '3^0.5^2'`` was previously calculated as’(3\ :sup:`0.5)`\ 2’,
   but is now calculated as ‘3\ :sup:`(0.5`\ 2)’.
-  In fish 3.0, the variable used with ``for`` loops inside command
   substitutions could leak into enclosing scopes; this was an
   inadvertent behaviour change and has been reverted (#6480).

.. _scripting-improvements-1:

Scripting improvements
----------------------

-  ``string split0`` now returns 0 if it split something (#5701).
-  In the interest of consistency, ``builtin -q`` and ``command -q`` can
   now be used to query if a builtin or command exists (#5631).
-  ``math`` now accepts ``--scale=max`` for the maximum scale (#5579).
-  ``builtin $var`` now works correctly, allowing a variable as the
   builtin name (#5639).
-  ``cd`` understands the ``--`` argument to make it possible to change
   to directories starting with a hyphen (#6071).
-  ``complete --do-complete`` now also does fuzzy matches (#5467).
-  ``complete --do-complete`` can be used inside completions, allowing
   limited recursion (#3474).
-  ``count`` now also counts lines fed on standard input (#5744).
-  ``eval`` produces an exit status of 0 when given no arguments, like
   other shells (#5692).
-  ``printf`` prints what it can when input hasn’t been fully converted
   to a number, but still prints an error (#5532).
-  ``complete -C foo`` now works as expected, rather than requiring
   ``complete -Cfoo``.
-  ``complete`` has a new ``--force-files`` option, to re-enable file
   completions. This allows ``sudo -E`` and ``pacman -Qo`` to complete
   correctly (#5646).
-  ``argparse`` now defaults to showing the current function name
   (instead of ``argparse``) in its errors, making ``--name`` often
   superfluous (#5835).
-  ``argparse`` has a new ``--ignore-unknown`` option to keep
   unrecognized options, allowing multiple argparse passes to parse
   options (#5367).
-  ``argparse`` correctly handles flag value validation of options that
   only have short names (#5864).
-  ``read -S`` (short option of ``--shell``) is recognised correctly
   (#5660).
-  ``read`` understands ``--list``, which acts like ``--array`` in
   reading all arguments into a list inside a single variable, but is
   better named (#5846).
-  ``read`` has a new option, ``--tokenize``, which splits a string into
   variables according to the shell’s tokenization rules, considering
   quoting, escaping, and so on (#3823).
-  ``read`` interacts more correctly with the deprecated ``$IFS``
   variable, in particular removing multiple separators when splitting a
   variable into a list (#6406), matching other shells.
-  ``fish_indent`` now handles semicolons better, including leaving them
   in place for ``; and`` and ``; or`` instead of breaking the line
   (#5859).
-  ``fish_indent --write`` now supports multiple file arguments,
   indenting them in turn.
-  The default read limit has been increased to 100MiB (#5267).
-  ``math`` now also understands ``x`` for multiplication, provided it
   is followed by whitespace (#5906).
-  ``math`` reports the right error when incorrect syntax is used inside
   parentheses (#6063), and warns when unsupported logical operations
   are used (#6096).
- ``math`` learned bitwise functions ``bitand``, ``bitor`` and ``bitxor``, used like ``math "bitand(0xFE, 5)"`` (#7281).
- ``math`` learned tau for those wishing to cut down on typing "2 * pi".
-  ``functions --erase`` now also prevents fish from autoloading a
   function for the first time (#5951).
-  ``jobs --last`` returns 0 to indicate success when a job is found
   (#6104).
-  ``commandline -p`` and ``commandline -j`` now split on ``&&`` and
   ``||`` in addition to ``;`` and ``&`` (#6214).
-  A bug where ``string split`` would drop empty strings if the output
   was only empty strings has been fixed (#5987).
-  ``eval`` no long creates a new local variable scope, but affects
   variables in the scope it is called from (#4443). ``source`` still
   creates a new local scope.
-  ``abbr`` has a new ``--query`` option to check for the existence of
   an abbreviation.
-  Local values for ``fish_complete_path`` and ``fish_function_path``
   are now ignored; only their global values are respected.
-  Syntax error reports now display a marker in the correct position
   (#5812).
-  Empty universal variables may now be exported (#5992).
-  Exported universal variables are no longer imported into the global
   scope, preventing shadowing. This makes it easier to change such
   variables for all fish sessions and avoids breakage when the value is
   a list of multiple elements (#5258).
-  A bug where ``for`` could use invalid variable names has been fixed
   (#5800).
-  A bug where local variables would not be exported to functions has
   been fixed (#6153).
-  The null command (``:``) now always exits successfully, rather than
   passing through the previous exit status (#6022).
-  The output of ``functions FUNCTION`` matches the declaration of the
   function, correctly including comments or blank lines (#5285), and
   correctly includes any ``--wraps`` flags (#1625).
-  ``type`` supports a new option, ``--short``, which suppress function
   expansion (#6403).
-  ``type --path`` with a function argument will now output the path to
   the file containing the definition of that function, if it exists.
-  ``type --force-path`` with an argument that cannot be found now
   correctly outputs nothing, as documented (#6411).
-  The ``$hostname`` variable is no longer truncated to 32 characters
   (#5758).
-  Line numbers in function backtraces are calculated correctly (#6350).
-  A new ``fish_cancel`` event is emitted when the command line is
   cancelled, which is useful for terminal integration (#5973).

.. _interactive-improvements-1:

Interactive improvements
------------------------

-  New Base16 color options are available through the Web-based
   configuration (#6504).
-  fish only parses ``/etc/paths`` on macOS in login shells, matching
   the bash implementation (#5637) and avoiding changes to path ordering
   in child shells (#5456). It now ignores blank lines like the bash
   implementation (#5809).
-  The locale is now reloaded when the ``LOCPATH`` variable is changed
   (#5815).
-  ``read`` no longer keeps a history, making it suitable for operations
   that shouldn’t end up there, like password entry (#5904).
-  ``dirh`` outputs its stack in the correct order (#5477), and behaves
   as documented when universal variables are used for its stack
   (#5797).
-  ``funced`` and the edit-commandline-in-buffer bindings did not work
   in fish 3.0 when the ``$EDITOR`` variable contained spaces; this has
   been corrected (#5625).
-  Builtins now pipe their help output to a pager automatically (#6227).
-  ``set_color`` now colors the ``--print-colors`` output in the
   matching colors if it is going to a terminal.
-  fish now underlines every valid entered path instead of just the last
   one (#5872).
-  When syntax highlighting a string with an unclosed quote, only the
   quote itself will be shown as an error, instead of the whole
   argument.
-  Syntax highlighting works correctly with variables as commands
   (#5658) and redirections to close file descriptors (#6092).
-  ``help`` works properly on Windows Subsytem for Linux (#5759, #6338).
-  A bug where ``disown`` could crash the shell has been fixed (#5720).
-  fish will not autosuggest files ending with ``~`` unless there are no
   other candidates, as these are generally backup files (#985).
-  Escape in the pager works correctly (#5818).
-  Key bindings that call ``fg`` no longer leave the terminal in a
   broken state (#2114).
-  Brackets (#5831) and filenames containing ``$`` (#6060) are completed
   with appropriate escaping.
-  The output of ``complete`` and ``functions`` is now colorized in
   interactive terminals.
-  The Web-based configuration handles aliases that include single
   quotes correctly (#6120), and launches correctly under Termux (#6248)
   and OpenBSD (#6522).
-  ``function`` now correctly validates parameters for
   ``--argument-names`` as valid variable names (#6147) and correctly
   parses options following ``--argument-names``, as in
   “``--argument-names foo --description bar``” (#6186).
-  History newly imported from bash includes command lines using ``&&``
   or ``||``.
-  The automatic generation of completions from manual pages is better
   described in job and process listings, and no longer produces a
   warning when exiting fish (#6269).
-  In private mode, setting ``$fish_greeting`` to an empty string before
   starting the private session will prevent the warning about history
   not being saved from being printed (#6299).
-  In the interactive editor, a line break (Enter) inside unclosed
   brackets will insert a new line, rather than executing the command
   and producing an error (#6316).
-  Ctrl-C always repaints the prompt (#6394).
-  When run interactively from another program (such as Python), fish
   will correctly start a new process group, like other shells (#5909).
-  Job identifiers (for example, for background jobs) are assigned more
   logically (#6053).
-  A bug where history would appear truncated if an empty command was
   executed was fixed (#6032).

.. _new-or-improved-bindings-1:

New or improved bindings
^^^^^^^^^^^^^^^^^^^^^^^^

-  Pasting strips leading spaces to avoid pasted commands being omitted
   from the history (#4327).
-  Shift-Left and Shift-Right now default to moving backwards and
   forwards by one bigword (words separated by whitespace) (#1505).
-  The default escape delay (to differentiate between the escape key and
   an alt-combination) has been reduced to 30ms, down from 300ms for the
   default mode and 100ms for Vi mode (#3904).
-  The ``forward-bigword`` binding now interacts correctly with
   autosuggestions (#5336).
-  The ``fish_clipboard_*`` functions support Wayland by using
   `wl-clipboard <https://github.com/bugaevc/wl-clipboard>`_
   (#5450).
-  The ``nextd`` and ``prevd`` functions no longer print “Hit end of
   history”, instead using a bell. They correctly store working
   directories containing symbolic links (#6395).
-  If a ``fish_mode_prompt`` function exists, Vi mode will only execute
   it on mode-switch instead of the entire prompt. This should make it
   much more responsive with slow prompts (#5783).
-  The path-component bindings (like Ctrl-w) now also stop at “:” and
   “@”, because those are used to denote user and host in commands such
   as ``ssh`` (#5841).
-  The NULL character can now be bound via ``bind -k nul``. Terminals
   often generate this character via control-space. (#3189).
-  A new readline command ``expand-abbr`` can be used to trigger
   abbreviation expansion (#5762).
-  A new readline command, ``delete-or-exit``, removes a character to
   the right of the cursor or exits the shell if the command line is
   empty (moving this functionality out of the ``delete-or-exit``
   function).
-  The ``self-insert`` readline command will now insert the binding
   sequence, if not empty.
-  A new binding to prepend ``sudo``, bound to Alt-S by default (#6140).
-  The Alt-W binding to describe a command should now work better with
   multiline prompts (#6110)
-  The Alt-H binding to open a command’s man page now tries to ignore
   ``sudo`` (#6122).
-  A new pair of bind functions, ``history-prefix-search-backward`` (and
   ``forward``), was introduced (#6143).
-  Vi mode now supports R to enter replace mode (#6342), and ``d0`` to
   delete the current line (#6292).
-  In Vi mode, hitting Enter in replace-one mode no longer erases the
   prompt (#6298).
-  Selections in Vi mode are inclusive, matching the actual behaviour of
   Vi (#5770).

.. _improved-prompts-1:

Improved prompts
^^^^^^^^^^^^^^^^

-  The Git prompt in informative mode now shows the number of stashes if
   enabled.
-  The Git prompt now has an option
   (``$__fish_git_prompt_use_informative_chars``) to use the (more
   modern) informative characters without enabling informative mode.
-  The default prompt now also features VCS integration and will color
   the host if running via SSH (#6375).
-  The default and example prompts print the pipe status if an earlier
   command in the pipe fails.
-  The default and example prompts try to resolve exit statuses to
   signal names when appropriate.

.. _improved-terminal-output-1:

Improved terminal output
^^^^^^^^^^^^^^^^^^^^^^^^

-  New ``fish_pager_color_`` options have been added to control more
   elements of the pager’s colors (#5524).
-  Better detection and support for using fish from various system
   consoles, where limited colors and special characters are supported
   (#5552).
-  fish now tries to guess if the system supports Unicode 9 (and
   displays emoji as wide), eliminating the need to set
   ``$fish_emoji_width`` in most cases (#5722).
-  Improvements to the display of wide characters, particularly Korean
   characters and emoji (#5583, #5729).
-  The Vi mode cursor is correctly redrawn when regaining focus under
   terminals that report focus (eg tmux) (#4788).
-  Variables that control background colors (such as
   ``fish_pager_color_search_match``) can now use ``--reverse``.

.. _completions-1:

Completions
^^^^^^^^^^^

-  Added completions for

   -  ``aws``
   -  ``bat`` (#6052)
   -  ``bosh`` (#5700)
   -  ``btrfs``
   -  ``camcontrol``
   -  ``cf`` (#5700)
   -  ``chronyc`` (#6496)
   -  ``code`` (#6205)
   -  ``cryptsetup`` (#6488)
   -  ``csc`` and ``csi`` (#6016)
   -  ``cwebp`` (#6034)
   -  ``cygpath`` and ``cygstart`` (#6239)
   -  ``epkginfo`` (#5829)
   -  ``ffmpeg``, ``ffplay``, and ``ffprobe`` (#5922)
   -  ``fsharpc`` and ``fsharpi`` (#6016)
   -  ``fzf`` (#6178)
   -  ``g++`` (#6217)
   -  ``gpg1`` (#6139)
   -  ``gpg2`` (#6062)
   -  ``grub-mkrescue`` (#6182)
   -  ``hledger`` (#6043)
   -  ``hwinfo`` (#6496)
   -  ``irb`` (#6260)
   -  ``iw`` (#6232)
   -  ``kak``
   -  ``keepassxc-cli`` (#6505)
   -  ``keybase`` (#6410)
   -  ``loginctl`` (#6501)
   -  ``lz4``, ``lz4c`` and ``lz4cat`` (#6364)
   -  ``mariner`` (#5718)
   -  ``nethack`` (#6240)
   -  ``patool`` (#6083)
   -  ``phpunit`` (#6197)
   -  ``plutil`` (#6301)
   -  ``pzstd`` (#6364)
   -  ``qubes-gpg-client`` (#6067)
   -  ``resolvectl`` (#6501)
   -  ``rg``
   -  ``rustup``
   -  ``sfdx`` (#6149)
   -  ``speedtest`` and ``speedtest-cli`` (#5840)
   -  ``src`` (#6026)
   -  ``tokei`` (#6085)
   -  ``tsc`` (#6016)
   -  ``unlz4`` (#6364)
   -  ``unzstd`` (#6364)
   -  ``vbc`` (#6016)
   -  ``zpaq`` (#6245)
   -  ``zstd``, ``zstdcat``, ``zstdgrep``, ``zstdless`` and ``zstdmt``
      (#6364)

-  Lots of improvements to completions.
-  Selecting short options which also have a long name from the
   completion pager is possible (#5634).
-  Tab completion will no longer add trailing spaces if they already
   exist (#6107).
-  Completion of subcommands to builtins like ``and`` or ``not`` now
   works correctly (#6249).
-  Completion of arguments to short options works correctly when
   multiple short options are used together (#332).
-  Activating completion in the middle of an invalid completion does not
   move the cursor any more, making it easier to fix a mistake (#4124).
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
   old names (#5586).
-  ``string replace`` has an additional round of escaping in the
   replacement expression, so escaping backslashes requires many escapes
   (eg ``string replace -ra '([ab])' '\\\\\\\$1' a``). The new feature
   flag ``regex-easyesc`` can be used to disable this, so that the same
   effect can be achieved with
   ``string replace -ra '([ab])' '\\\\$1' a`` (#5556). As a reminder,
   the intention behind feature flags is that this will eventually
   become the default and then only option, so scripts should be
   updated.
-  The ``fish_vi_mode`` function, deprecated in fish 2.3, has been
   removed. Use ``fish_vi_key_bindings`` instead (#6372).

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
   (#5777).
-  mandoc can now be used to format the output from ``--help`` if
   ``nroff`` is not installed, reducing the number of external
   dependencies on systems with ``mandoc`` installed (#5489).
-  Some bugs preventing building on Solaris-derived systems such as
   Illumos were fixed (#5458, #5461, #5611).
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
   directory, which now defaults to ``/usr/local`` (#5029).

--------------

fish 3.0.2 (released February 19, 2019)
=======================================

This release of fish fixes an issue discovered in fish 3.0.1.

Fixes and improvements
----------------------

-  The PWD environment variable is now ignored if it does not resolve to
   the true working directory, fixing strange behaviour in terminals
   started by editors and IDEs (#5647).

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
   (#5449).
-  while loops now evaluate to the last executed command in the loop
   body (or zero if the body was empty), matching POSIX semantics
   (#4982).
-  ``read --silent`` no longer echoes to the tty when run from a
   non-interactive script (#5519).
-  On macOS, path entries with spaces in ``/etc/paths`` and
   ``/etc/paths.d`` now correctly set path entries with spaces.
   Likewise, ``MANPATH`` is correctly set from ``/etc/manpaths`` and
   ``/etc/manpaths.d`` (#5481).
-  fish starts correctly under Cygwin/MSYS2 (#5426).
-  The ``pager-toggle-search`` binding (Ctrl-S by default) will now
   activate the search field, even when the pager is not focused.
-  The error when a command is not found is now printed a single time,
   instead of once per argument (#5588).
-  Fixes and improvements to the git completions, including printing
   correct paths with older git versions, fuzzy matching again, reducing
   unnecessary offers of root paths (starting with ``:/``) (#5578,
   #5574, #5476), and ignoring shell aliases, so enterprising users can
   set up the wrapping command (via
   ``set -g __fish_git_alias_$command $whatitwraps``) (#5412).
-  Significant performance improvements to core shell functions (#5447)
   and to the ``kill`` completions (#5541).
-  Starting in symbolically-linked working directories works correctly
   (#5525).
-  The default ``fish_title`` function no longer contains extra spaces
   (#5517).
-  The ``nim`` prompt now works correctly when chosen in the Web-based
   configuration (#5490).
-  ``string`` now prints help to stdout, like other builtins (#5495).
-  Killing the terminal while fish is in vi normal mode will no longer
   send it spinning and eating CPU. (#5528)
-  A number of crashes have been fixed (#5550, #5548, #5479, #5453).
-  Improvements to the documentation and certain completions.

Known issues
------------

There is one significant known issue that was not corrected before the
release:

-  fish does not run correctly under Windows Services for Linux before
   Windows 10 version 1809/17763, and the message warning of this may
   not be displayed (#5619).

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

-  builds correctly against musl libc (#5407)
-  handles huge numeric arguments to ``test`` correctly (#5414)
-  removes the history colouring introduced in 3.0b1, which did not
   always work correctly

There is one significant known issue which was not able to be corrected
before the release:

-  fish 3.0.0 builds on Cygwin (#5423), but does not run correctly
   (#5426) and will result in a hanging terminal when started. Cygwin
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
   starting with ``%`` (#4230, #1202).
-  ``set x[1] x[2] a b``, to set multiple elements of an array at once,
   is no longer valid syntax (#4236).
-  A literal ``{}`` now expands to itself, rather than nothing. This
   makes working with ``find -exec`` easier (#1109, #4632).
-  Literally accessing a zero-index is now illegal syntax and is caught
   by the parser (#4862). (fish indices start at 1)
-  Successive commas in brace expansions are handled in less surprising
   manner. For example, ``{,,,}`` expands to four empty strings rather
   than an empty string, a comma and an empty string again (#3002,
   #4632).
-  ``for`` loop control variables are no longer local to the ``for``
   block (#1935).
-  Variables set in ``if`` and ``while`` conditions are available
   outside the block (#4820).
-  Local exported (``set -lx``) vars are now visible to functions
   (#1091).
-  The new ``math`` builtin (see below) does not support logical
   expressions; ``test`` should be used instead (#4777).
-  Range expansion will now behave sensibly when given a single positive
   and negative index (``$foo[5..-1]`` or ``$foo[-1..5]``), clamping to
   the last valid index without changing direction if the list has fewer
   elements than expected.
-  ``read`` now uses ``-s`` as short for ``--silent`` (à la ``bash``);
   ``--shell``\ ’s abbreviation (formerly ``-s``) is now ``-S`` instead
   (#4490).
-  ``cd`` no longer resolves symlinks. fish now maintains a virtual
   path, matching other shells (#3350).
-  ``source`` now requires an explicit ``-`` as the filename to read
   from the terminal (#2633).
-  Arguments to ``end`` are now errors, instead of being silently
   ignored.
-  The names ``argparse``, ``read``, ``set``, ``status``, ``test`` and
   ``[`` are now reserved and not allowed as function names. This
   prevents users unintentionally breaking stuff (#3000).
-  The ``fish_user_abbreviations`` variable is no longer used;
   abbreviations will be migrated to the new storage format
   automatically.
-  The ``FISH_READ_BYTE_LIMIT`` variable is now called
   ``fish_byte_limit`` (#4414).
-  Environment variables are no longer split into arrays based on the
   record separator character on startup. Instead, variables are not
   split, unless their name ends in PATH, in which case they are split
   on colons (#436).
-  The ``history`` builtin’s ``--with-time`` option has been removed;
   this has been deprecated in favor of ``--show-time`` since 2.7.0
   (#4403).
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
variable. (#4940)

-  The use of the ``IFS`` variable for ``read`` is deprecated; ``IFS``
   will be ignored in the future (#4156). Use the ``read --delimiter``
   option instead.
-  The ``function --on-process-exit`` switch will be removed in future
   (#4700). Use the ``fish_exit`` event instead:
   ``function --on-event fish_exit``.
-  ``$_`` is deprecated and will removed in the future (#813). Use
   ``status current-command`` in a command substitution instead.
-  ``^`` as a redirection deprecated and will be removed in the future.
   (#4394). Use ``2>`` to redirect stderr. This is controlled by the
   ``stderr-nocaret`` feature flag.
-  ``?`` as a glob (wildcard) is deprecated and will be removed in the
   future (#4520). This is controlled by the ``qmark-noglob`` feature
   flag.

Notable fixes and improvements
------------------------------

.. _syntax-changes-and-new-commands-2:

Syntax changes and new commands
-------------------------------

-  fish now supports ``&&`` (like ``and``), ``||`` (like ``or``), and
   ``!`` (like ``not``), for better migration from POSIX-compliant
   shells (#4620).
-  Variables may be used as commands (#154).
-  fish may be started in private mode via ``fish --private``. Private
   mode fish sessions do not have access to the history file and any
   commands evaluated in private mode are not persisted for future
   sessions. A session variable ``$fish_private_mode`` can be queried to
   detect private mode and adjust the behavior of scripts accordingly to
   respect the user’s wish for privacy.
-  A new ``wait`` command for waiting on backgrounded processes (#4498).
-  ``math`` is now a builtin rather than a wrapper around ``bc``
   (#3157). Floating point computations is now used by default, and can
   be controlled with the new ``--scale`` option (#4478).
-  Setting ``$PATH`` no longer warns on non-existent directories,
   allowing for a single $PATH to be shared across machines (eg via
   dotfiles) (#2969).
-  ``while`` sets ``$status`` to a non-zero value if the loop is not
   executed (#4982).
-  Command substitution output is now limited to 10 MB by default,
   controlled by the ``fish_read_limit`` variable (#3822). Notably, this
   is larger than most operating systems’ argument size limit, so trying
   to pass argument lists this size to external commands has never
   worked.
-  The machine hostname, where available, is now exposed as the
   ``$hostname`` reserved variable. This removes the dependency on the
   ``hostname`` executable (#4422).
-  Bare ``bind`` invocations in config.fish now work. The
   ``fish_user_key_bindings`` function is no longer necessary, but will
   still be executed if it exists (#5191).
-  ``$fish_pid`` and ``$last_pid`` are available as replacements for
   ``%self`` and ``%last``.

New features in commands
------------------------

-  ``alias`` has a new ``--save`` option to save the generated function
   immediately (#4878).
-  ``bind`` has a new ``--silent`` option to ignore bind requests for
   named keys not available under the current terminal (#4188, #4431).
-  ``complete`` has a new ``--keep-order`` option to show the provided
   or dynamically-generated argument list in the same order as
   specified, rather than alphabetically (#361).
-  ``exec`` prompts for confirmation if background jobs are running.
-  ``funced`` has a new ``--save`` option to automatically save the
   edited function after successfully editing (#4668).
-  ``functions`` has a new ``--handlers`` option to show functions
   registered as event handlers (#4694).
-  ``history search`` supports globs for wildcard searching (#3136) and
   has a new ``--reverse`` option to show entries from oldest to newest
   (#4375).
-  ``jobs`` has a new ``--quiet`` option to silence the output.
-  ``read`` has a new ``--delimiter`` option for splitting input into
   arrays (#4256).
-  ``read`` writes directly to stdout if called without arguments
   (#4407).
-  ``read`` can now read individual lines into separate variables
   without consuming the input in its entirety via the new ``/--line``
   option.
-  ``set`` has new ``--append`` and ``--prepend`` options (#1326).
-  ``string match`` with an empty pattern and ``--entire`` in glob mode
   now matches everything instead of nothing (#4971).
-  ``string split`` supports a new ``--no-empty`` option to exclude
   empty strings from the result (#4779).
-  ``string`` has new subcommands ``split0`` and ``join0`` for working
   with NUL-delimited output.
-  ``string`` no longer stops processing text after NUL characters
   (#4605)
-  ``string escape`` has a new ``--style regex`` option for escaping
   strings to be matched literally in ``string`` regex operations.
-  ``test`` now supports floating point values in numeric comparisons.

.. _interactive-improvements-2:

Interactive improvements
------------------------

-  A pipe at the end of a line now allows the job to continue on the
   next line (#1285).
-  Italics and dim support out of the box on macOS for Terminal.app and
   iTerm (#4436).
-  ``cd`` tab completions no longer descend into the deepest unambiguous
   path (#4649).
-  Pager navigation has been improved. Most notably, moving down now
   wraps around, moving up from the commandline now jumps to the last
   element and moving right and left now reverse each other even when
   wrapping around (#4680).
-  Typing normal characters while the completion pager is active no
   longer shows the search field. Instead it enters them into the
   command line, and ends paging (#2249).
-  A new input binding ``pager-toggle-search`` toggles the search field
   in the completions pager on and off. By default, this is bound to
   Ctrl-S.
-  Searching in the pager now does a full fuzzy search (#5213).
-  The pager will now show the full command instead of just its last
   line if the number of completions is large (#4702).
-  Abbreviations can be tab-completed (#3233).
-  Tildes in file names are now properly escaped in completions (#2274).
-  Wrapping completions (from ``complete --wraps`` or
   ``function --wraps``) can now inject arguments. For example,
   ``complete gco --wraps 'git checkout'`` now works properly (#1976).
   The ``alias`` function has been updated to respect this behavior.
-  Path completions now support expansions, meaning expressions like
   ``python ~/<TAB>`` now provides file suggestions just like any other
   relative or absolute path. (This includes support for other
   expansions, too.)
-  Autosuggestions try to avoid arguments that are already present in
   the command line.
-  Notifications about crashed processes are now always shown, even in
   command substitutions (#4962).
-  The screen is no longer reset after a BEL, fixing graphical glitches
   (#3693).
-  vi-mode now supports ‘;’ and ‘,’ motions. This introduces new
   {forward,backward}-jump-till and repeat-jump{,-reverse} bind
   functions (#5140).
-  The ``*y`` vi-mode binding now works (#5100).
-  True color is now enabled in neovim by default (#2792).
-  Terminal size variables (``$COLUMNS``/``$LINES``) are now updated
   before ``fish_prompt`` is called, allowing the prompt to react
   (#904).
-  Multi-line prompts no longer repeat when the terminal is resized
   (#2320).
-  ``xclip`` support has been added to the clipboard integration
   (#5020).
-  The Alt-P keybinding paginates the last command if the command line
   is empty.
-  ``$cmd_duration`` is no longer reset when no command is executed
   (#5011).
-  Deleting a one-character word no longer erases the next word as well
   (#4747).
-  Token history search (Alt-Up) omits duplicate entries (#4795).
-  The ``fish_escape_delay_ms`` timeout, allowing the use of the escape
   key both on its own and as part of a control sequence, was applied to
   all control characters; this has been reduced to just the escape key.
-  Completing a function shows the description properly (#5206).
-  Added completions for

   -  ``ansible``, including ``ansible-galaxy``, ``ansible-playbook``
      and ``ansible-vault`` (#4697)
   -  ``bb-power`` (#4800)
   -  ``bd`` (#4472)
   -  ``bower``
   -  ``clang`` and ``clang++`` (#4174)
   -  ``conda`` (#4837)
   -  ``configure`` (for autoconf-generated files only)
   -  ``curl``
   -  ``doas`` (#5196)
   -  ``ebuild`` (#4911)
   -  ``emaint`` (#4758)
   -  ``eopkg`` (#4600)
   -  ``exercism`` (#4495)
   -  ``hjson``
   -  ``hugo`` (#4529)
   -  ``j`` (from autojump #4344)
   -  ``jbake`` (#4814)
   -  ``jhipster`` (#4472)
   -  ``kitty``
   -  ``kldload``
   -  ``kldunload``
   -  ``makensis`` (#5242)
   -  ``meson``
   -  ``mkdocs`` (#4906)
   -  ``ngrok`` (#4642)
   -  OpenBSD’s ``pkg_add``, ``pkg_delete``, ``pkg_info``, ``pfctl``,
      ``rcctl``, ``signify``, and ``vmctl`` (#4584)
   -  ``openocd``
   -  ``optipng``
   -  ``opkg`` (#5168)
   -  ``pandoc`` (#2937)
   -  ``port`` (#4737)
   -  ``powerpill`` (#4800)
   -  ``pstack`` (#5135)
   -  ``serve`` (#5026)
   -  ``ttx``
   -  ``unzip``
   -  ``virsh`` (#5113)
   -  ``xclip`` (#5126)
   -  ``xsv``
   -  ``zfs`` and ``zpool`` (#4608)

-  Lots of improvements to completions (especially ``darcs`` (#5112),
   ``git``, ``hg`` and ``sudo``).
-  Completions for ``yarn`` and ``npm`` now require the
   ``all-the-package-names`` NPM package for full functionality.
-  Completions for ``bower`` and ``yarn`` now require the ``jq`` utility
   for full functionality.
-  Improved French translations.

Other fixes and improvements
----------------------------

-  Significant performance improvements to ``abbr`` (#4048), setting
   variables (#4200, #4341), executing functions, globs (#4579),
   ``string`` reading from standard input (#4610), and slicing history
   (in particular, ``$history[1]`` for the last executed command).
-  Fish’s internal wcwidth function has been updated to deal with newer
   Unicode, and the width of some characters can be configured via the
   ``fish_ambiguous_width`` (#5149) and ``fish_emoji_width`` (#2652)
   variables. Alternatively, a new build-time option INTERNAL_WCWIDTH
   can be used to use the system’s wcwidth instead (#4816).
-  ``functions`` correctly supports ``-d`` as the short form of
   ``--description``. (#5105)
-  ``/etc/paths`` is now parsed like macOS’ bash ``path_helper``, fixing
   $PATH order (#4336, #4852) on macOS.
-  Using a read-only variable in a ``for`` loop produces an error,
   rather than silently producing incorrect results (#4342).
-  The universal variables filename no longer contains the hostname or
   MAC address. It is now at the fixed location
   ``.config/fish/fish_variables`` (#1912).
-  Exported variables in the global or universal scope no longer have
   their exported status affected by local variables (#2611).
-  Major rework of terminal and job handling to eliminate bugs (#3805,
   #3952, #4178, #4235, #4238, #4540, #4929, #5210).
-  Improvements to the manual page completion generator (#2937, #4313).
-  ``suspend --force`` now works correctly (#4672).
-  Pressing Ctrl-C while running a script now reliably terminates fish
   (#5253).

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
session (#4521).

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
   (#2847).
-  A new ``argparse`` command is available to allow fish script to parse
   arguments with the same behavior as builtin commands. This also
   includes the ``fish_opt`` helper command. (#4190).
-  Invalid array indexes are now silently ignored (#826, #4127).
-  Improvements to the debugging facility, including a prompt specific
   to the debugger (``fish_breakpoint_prompt``) and a
   ``status is-breakpoint`` subcommand (#1310).
-  ``string`` supports new ``lower`` and ``upper`` subcommands, for
   altering the case of strings (#4080). The case changing is not
   locale-aware yet.- ``string escape`` has a new ``--style=xxx`` flag
   where ``xxx`` can be ``script``, ``var``, or ``url`` (#4150), and can
   be reversed with ``string unescape`` (#3543).
-  History can now be split into sessions with the ``fish_history``
   variable, or not saved to disk at all (#102).
-  Read history is now controlled by the ``fish_history`` variable
   rather than the ``--mode-name`` flag (#1504).
-  ``command`` now supports an ``--all`` flag to report all directories
   with the command. ``which`` is no longer a runtime dependency
   (#2778).
-  fish can run commands before starting an interactive session using
   the new ``--init-command``/``-C`` options (#4164).
-  ``set`` has a new ``--show`` option to show lots of information about
   variables (#4265).

Other significant changes
-------------------------

-  The ``COLUMNS`` and ``LINES`` environment variables are now correctly
   set the first time ``fish_prompt`` is run (#4141).

-  ``complete``\ ’s ``--no-files`` option works as intended (#112).

-  ``echo -h`` now correctly echoes ``-h`` in line with other shells
   (#4120).

-  The ``export`` compatibility function now returns zero on success,
   rather than always returning 1 (#4435).

-  Stop converting empty elements in MANPATH to “.” (#4158). The
   behavior being changed was introduced in fish 2.6.0.

-  ``count -h`` and ``count --help`` now return 1 rather than produce
   command help output (#4189).

-  An attempt to ``read`` which stops because too much data is available
   still defines the variables given as parameters (#4180).

-  A regression in fish 2.4.0 which prevented ``pushd +1`` from working
   has been fixed (#4091).

-  A regression in fish 2.6.0 where multiple ``read`` commands in
   non-interactive scripts were broken has been fixed (#4206).

-  A regression in fish 2.6.0 involving universal variables with
   side-effects at startup such as ``set -U fish_escape_delay_ms 10``
   has been fixed (#4196).

-  Added completions for:

   -  ``as`` (#4130)
   -  ``cdh`` (#2847)
   -  ``dhcpd`` (#4115)
   -  ``ezjail-admin`` (#4324)
   -  Fabric’s ``fab`` (#4153)
   -  ``grub-file`` (#4119)
   -  ``grub-install`` (#4119)
   -  ``jest`` (#4142)
   -  ``kdeconnect-cli``
   -  ``magneto`` (#4043, #4108)
   -  ``mdadm`` (#4198)
   -  ``passwd`` (#4209)
   -  ``pip`` and ``pipenv`` (#4448)
   -  ``s3cmd`` (#4332)
   -  ``sbt`` (#4347)
   -  ``snap`` (#4215)
   -  Sublime Text 3’s ``subl`` (#4277)

-  Lots of improvements to completions.

-  Updated Chinese and French translations.

-  Improved completions for:

   -  ``apt``

   -  ``cd`` (#4061)

   -  ``composer`` (#4295)

   -  ``eopkg``

   -  ``flatpak`` (#4456)

   -  ``git`` (#4117, #4147, #4329, #4368)

   -  ``gphoto2``

   -  ``killall`` (#4052)

   -  ``ln``

   -  ``npm`` (#4241)

   -  ``ssh`` (#4377)

   -  ``tail``

   -  ``xdg-mime`` (#4333)

   -  ``zypper`` (#4325)

fish 2.6.0 (released June 3, 2017)
==================================

Since the beta release of fish 2.6b1, fish version 2.6.0 contains a
number of minor fixes, new completions for ``magneto`` (#4043), and
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
   command in other shells (#2810).
-  Command substitutions now have access to the terminal, like in other
   shells. This allows tools like ``fzf`` to work properly (#1362,
   #3922).
-  In cases where the operating system does not report the size of the
   terminal, the ``COLUMNS`` and ``LINES`` environment variables are
   used; if they are unset, a default of 80x24 is assumed.
-  New French (#3772 & #3788) and improved German (#3834) translations.
-  fish no longer depends on the ``which`` external command.

.. _other-significant-changes-1:

Other significant changes
-------------------------

-  Performance improvements in launching processes, including major
   reductions in signal blocking. Although this has been heavily tested,
   it may cause problems in some circumstances; set the
   ``FISH_NO_SIGNAL_BLOCK`` variable to 0 in your fish configuration
   file to return to the old behaviour (#2007).
-  Performance improvements in prompts and functions that set lots of
   colours (#3793).
-  The Delete key no longer deletes backwards (a regression in 2.5.0).
-  ``functions`` supports a new ``--details`` option, which identifies
   where the function was loaded from (#3295), and a
   ``--details --verbose`` option which includes the function
   description (#597).
-  ``read`` will read up to 10 MiB by default, leaving the target
   variable empty and exiting with status 122 if the line is too long.
   You can set a different limit with the ``FISH_READ_BYTE_LIMIT``
   variable.
-  ``read`` supports a new ``--silent`` option to hide the characters
   typed (#838), for when reading sensitive data from the terminal.
   ``read`` also now accepts simple strings for the prompt (rather than
   scripts) with the new ``-P`` and ``--prompt-str`` options (#802).
-  ``export`` and ``setenv`` now understand colon-separated ``PATH``,
   ``CDPATH`` and ``MANPATH`` variables.
-  ``setenv`` is no longer a simple alias for ``set -gx`` and will
   complain, just like the csh version, if given more than one value
   (#4103).
-  ``bind`` supports a new ``--list-modes`` option (#3872).
-  ``bg`` will check all of its arguments before backgrounding any jobs;
   any invalid arguments will cause a failure, but non-existent (eg
   recently exited) jobs are ignored (#3909).
-  ``funced`` warns if the function being edited has not been modified
   (#3961).
-  ``printf`` correctly outputs “long long” integers (#3352).
-  ``status`` supports a new ``current-function`` subcommand to print
   the current function name (#1743).
-  ``string`` supports a new ``repeat`` subcommand (#3864).
   ``string match`` supports a new ``--entire`` option to emit the
   entire line matched by a pattern (#3957). ``string replace`` supports
   a new ``--filter`` option to only emit lines which underwent a
   replacement (#3348).
-  ``test`` supports the ``-k`` option to test for sticky bits (#733).
-  ``umask`` understands symbolic modes (#738).
-  Empty components in the ``CDPATH``, ``MANPATH`` and ``PATH``
   variables are now converted to “.” (#2106, #3914).
-  New versions of ncurses (6.0 and up) wipe terminal scrollback buffers
   with certain commands; the ``C-l`` binding tries to avoid this
   (#2855).
-  Some systems’ ``su`` implementations do not set the ``USER``
   environment variable; it is now reset for root users (#3916).
-  Under terminals which support it, bracketed paste is enabled,
   escaping problematic characters for security and convience (#3871).
   Inside single quotes (``'``), single quotes and backslashes in pasted
   text are escaped (#967). The ``fish_clipboard_paste`` function (bound
   to ``C-v`` by default) is still the recommended pasting method where
   possible as it includes this functionality and more.
-  Processes in pipelines are no longer signalled as soon as one command
   in the pipeline has completed (#1926). This behaviour matches other
   shells mre closely.
-  All functions requiring Python work with whichever version of Python
   is installed (#3970). Python 3 is preferred, but Python 2.6 remains
   the minimum version required.
-  The color of the cancellation character can be controlled by the
   ``fish_color_cancel`` variable (#3963).
-  Added completions for:
-  ``caddy`` (#4008)
-  ``castnow`` (#3744)
-  ``climate`` (#3760)
-  ``flatpak``
-  ``gradle`` (#3859)
-  ``gsettings`` (#4001)
-  ``helm`` (#3829)
-  ``i3-msg`` (#3787)
-  ``ipset`` (#3924)
-  ``jq`` (#3804)
-  ``light`` (#3752)
-  ``minikube`` (#3778)
-  ``mocha`` (#3828)
-  ``mkdosfs`` (#4017)
-  ``pv`` (#3773)
-  ``setsid`` (#3791)
-  ``terraform`` (#3960)
-  ``usermod`` (#3775)
-  ``xinput``
-  ``yarn`` (#3816)
-  Improved completions for ``adb`` (#3853), ``apt`` (#3771), ``bzr``
   (#3769), ``dconf``, ``git`` (including #3743), ``grep`` (#3789),
   ``go`` (#3789), ``help`` (#3789), ``hg`` (#3975), ``htop`` (#3789),
   ``killall`` (#3996), ``lua``, ``man`` (#3762), ``mount`` (#3764 &
   #3841), ``obnam`` (#3924), ``perl`` (#3856), ``portmaster`` (#3950),
   ``python`` (#3840), ``ssh`` (#3781), ``scp`` (#3781), ``systemctl``
   (#3757) and ``udisks`` (#3764).

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
   Vi-style key bindings (#3731).

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
   into line with stopped processes. (#3497)
-  ``random`` can now have start, stop and step values specified, or the
   new ``choice`` subcommand can be used to pick an argument from a list
   (#3619).
-  A new key bindings preset, ``fish_hybrid_key_bindings``, including
   all the Emacs-style and Vi-style bindings, which behaves like
   ``fish_vi_key_bindings`` in fish 2.3.0 (#3556).
-  ``function`` now returns an error when called with invalid options,
   rather than defining the function anyway (#3574). This was a
   regression present in fish 2.3 and 2.4.0.
-  fish no longer prints a warning when it identifies a running instance
   of an old version (2.1.0 and earlier). Changes to universal variables
   may not propagate between these old versions and 2.5b1.
-  Improved compatiblity with Android (#3585), MSYS/mingw (#2360), and
   Solaris (#3456, #3340).
-  Like other shells, the ``test`` builting now returns an error for
   numeric operations on invalid integers (#3346, #3581).
-  ``complete`` no longer recognises ``--authoritative`` and
   ``--unauthoritative`` options, and they are marked as obsolete.
-  ``status`` accepts subcommands, and should be used like
   ``status is-interactive``. The old options continue to be supported
   for the foreseeable future (#3526), although only one subcommand or
   option can be specified at a time.
-  Selection mode (used with “begin-selection”) no longer selects a
   character the cursor does not move over (#3684).
-  List indexes are handled better, and a bit more liberally in some
   cases (``echo $PATH[1 .. 3]`` is now valid) (#3579).
-  The ``fish_mode_prompt`` function is now simply a stub around
   ``fish_default_mode_prompt``, which allows the mode prompt to be
   included more easily in customised prompt functions (#3641).

.. _notable-fixes-and-improvements-3:

Notable fixes and improvements
------------------------------

-  ``alias``, run without options or arguments, lists all defined
   aliases, and aliases now include a description in the function
   signature that identifies them.
-  ``complete`` accepts empty strings as descriptions (#3557).
-  ``command`` accepts ``-q``/``--quiet`` in combination with
   ``--search`` (#3591), providing a simple way of checking whether a
   command exists in scripts.
-  Abbreviations can now be renamed with
   ``abbr --rename OLD_KEY NEW_KEY`` (#3610).
-  The command synopses printed by ``--help`` options work better with
   copying and pasting (#2673).
-  ``help`` launches the browser specified by the
   ``$fish_help_browser variable`` if it is set (#3131).
-  History merging could lose items under certain circumstances and is
   now fixed (#3496).
-  The ``$status`` variable is now set to 123 when a syntactically
   invalid command is entered (#3616).
-  Exiting fish now signals all background processes to terminate, not
   just stopped jobs (#3497).
-  A new ``prompt_hostname`` function which prints a hostname suitable
   for use in prompts (#3482).
-  The ``__fish_man_page`` function (bound to Alt-h by default) now
   tries to recognize subcommands (e.g. ``git add`` will now open the
   “git-add” man page) (#3678).
-  A new function ``edit_command_buffer`` (bound to Alt-e & Alt-v by
   default) to edit the command buffer in an external editor (#1215,
   #3627).
-  ``set_color`` now supports italics (``--italics``), dim (``--dim``)
   and reverse (``--reverse``) modes (#3650).
-  Filesystems with very slow locking (eg incorrectly-configured NFS)
   will no longer slow fish down (#685).
-  Improved completions for ``apt`` (#3695), ``fusermount`` (#3642),
   ``make`` (#3628), ``netctl-auto`` (#3378), ``nmcli`` (#3648),
   ``pygmentize`` (#3378), and ``tar`` (#3719).
-  Added completions for:
-  ``VBoxHeadless`` (#3378)
-  ``VBoxSDL`` (#3378)
-  ``base64`` (#3378)
-  ``caffeinate`` (#3524)
-  ``dconf`` (#3638)
-  ``dig`` (#3495)
-  ``dpkg-reconfigure`` (#3521 & #3522)
-  ``feh`` (#3378)
-  ``launchctl`` (#3682)
-  ``lxc`` (#3554 & #3564),
-  ``mddiagnose`` (#3524)
-  ``mdfind`` (#3524)
-  ``mdimport`` (#3524)
-  ``mdls`` (#3524)
-  ``mdutil`` (#3524)
-  ``mkvextract`` (#3492)
-  ``nvram`` (#3524)
-  ``objdump`` (#3378)
-  ``sysbench`` (#3491)
-  ``tmutil`` (#3524)

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
   prompt (#3499).
-  Improved the title set in Apple Terminal.app.
-  Added completions for ``defaults`` and improved completions for
   ``diskutil`` (#3478).

--------------

fish 2.4b1 (released October 18, 2016)
======================================

Significant changes
-------------------

-  The clipboard integration has been revamped with explicit bindings.
   The killring commands no longer copy from, or paste to, the X11
   clipboard - use the new copy (``C-x``) and paste (``C-v``) bindings
   instead. The clipboard is now available on OS X as well as systems
   using X11 (e.g. Linux). (#3061)
-  ``history`` uses subcommands (``history delete``) rather than options
   (``history --delete``) for its actions (#3367). You can no longer
   specify multiple actions via flags (e.g.,
   ``history --delete --save something``).
-  New ``history`` options have been added, including ``--max=n`` to
   limit the number of history entries, ``--show-time`` option to show
   timestamps (#3175, #3244), and ``--null`` to null terminate history
   entries in the search output.
-  ``history search`` is now case-insensitive by default (which also
   affects ``history delete``) (#3236).
-  ``history delete`` now correctly handles multiline commands (#31).
-  Vi-style bindings no longer include all of the default emacs-style
   bindings; instead, they share some definitions (#3068).
-  If there is no locale set in the environment, various known system
   configuration files will be checked for a default. If no locale can
   be found, ``en_US-UTF.8`` will be used (#277).
-  A number followed by a caret (e.g. ``5^``) is no longer treated as a
   redirection (#1873).
-  The ``$version`` special variable can be overwritten, so that it can
   be used for other purposes if required.

.. _notable-fixes-and-improvements-5:

Notable fixes and improvements
------------------------------

-  The ``fish_realpath`` builtin has been renamed to ``realpath`` and
   made compatible with GNU ``realpath`` when run without arguments
   (#3400). It is used only for systems without a ``realpath`` or
   ``grealpath`` utility (#3374).
-  Improved color handling on terminals/consoles with 8-16 colors,
   particularly the use of bright named color (#3176, #3260).
-  ``fish_indent`` can now read from files given as arguments, rather
   than just standard input (#3037).
-  Fuzzy tab completions behave in a less surprising manner (#3090,
   #3211).
-  ``jobs`` should only print its header line once (#3127).
-  Wildcards in redirections are highlighted appropriately (#2789).
-  Suggestions will be offered more often, like after removing
   characters (#3069).
-  ``history --merge`` now correctly interleaves items in chronological
   order (#2312).
-  Options for ``fish_indent`` have been aligned with the other binaries
   - in particular, ``-d`` now means ``--debug``. The ``--dump`` option
   has been renamed to ``--dump-parse-tree`` (#3191).
-  The display of bindings in the Web-based configuration has been
   greatly improved (#3325), as has the rendering of prompts (#2924).
-  fish should no longer hang using 100% CPU in the C locale (#3214).
-  A bug in FreeBSD 11 & 12, Dragonfly BSD & illumos prevented fish from
   working correctly on these platforms under UTF-8 locales; fish now
   avoids the buggy behaviour (#3050).
-  Prompts which show git repository information (via
   ``__fish_git_prompt``) are faster in large repositories (#3294) and
   slow filesystems (#3083).
-  fish 2.3.0 reintroduced a problem where the greeting was printed even
   when using ``read``; this has been corrected again (#3261).
-  Vi mode changes the cursor depending on the current mode (#3215).
-  Command lines with escaped space characters at the end tab-complete
   correctly (#2447).
-  Added completions for:

   -  ``arcanist`` (#3256)
   -  ``connmanctl`` (#3419)
   -  ``figlet`` (#3378)
   -  ``mdbook`` (#3378)
   -  ``ninja`` (#3415)
   -  ``p4``, the Perforce client (#3314)
   -  ``pygmentize`` (#3378)
   -  ``ranger`` (#3378)

-  Improved completions for ``aura`` (#3297), ``abbr`` (#3267), ``brew``
   (#3309), ``chown`` (#3380, #3383),\ ``cygport`` (#3392), ``git``
   (#3274, #3226, #3225, #3094, #3087, #3035, #3021, #2982, #3230),
   ``kill`` & ``pkill`` (#3200), ``screen`` (#3271), ``wget`` (#3470),
   and ``xz`` (#3378).
-  Distributors, packagers and developers will notice that the build
   process produces more succinct output by default; use ``make V=1`` to
   get verbose output (#3248).
-  Improved compatibility with minor platforms including musl (#2988),
   Cygwin (#2993), Android (#3441, #3442), Haiku (#3322) and Solaris .

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
   (#2991).
-  ``fish_mode_prompt`` has been updated to reflect the changes in the
   way the Vi input mode is set up (#3067), making this more reliable.
-  ``fish_config`` can now properly be launched from the OS X app bundle
   (#3140).

.. _notable-fixes-and-improvements-6:

Notable fixes and improvements
------------------------------

-  Extra lines were sometimes inserted into the output under Windows
   (Cygwin and Microsoft Windows Subsystem for Linux) due to TTY
   timestamps not being updated (#2859).
-  The ``string`` builtin’s ``match`` mode now handles the combination
   of ``-rnv`` (match, invert and count) correctly (#3098).
-  Improvements to TTY special character handling (#3064), locale
   handling (#3124) and terminal environment variable handling (#3060).
-  Work towards handling the terminal modes for external commands
   launched from initialisation files (#2980).
-  Ease the upgrade path from fish 2.2.0 and before by warning users to
   restart fish if the ``string`` builtin is not available (#3057).
-  ``type -a`` now syntax-colorizes function source output.
-  Added completions for ``alsamixer``, ``godoc``, ``gofmt``,
   ``goimports``, ``gorename``, ``lscpu``, ``mkdir``, ``modinfo``,
   ``netctl-auto``, ``poweroff``, ``termite``, ``udisksctl`` and ``xz``
   (#3123).
-  Improved completions for ``apt`` (#3097), ``aura`` (#3102),\ ``git``
   (#3114), ``npm`` (#3158), ``string`` and ``suspend`` (#3154).

--------------

fish 2.3.0 (released May 20, 2016)
==================================

There are no significant changes between 2.3.0 and 2.3b2.

Other notable fixes and improvements
------------------------------------

-  ``abbr`` now allows non-letter keys (#2996).
-  Define a few extra colours on first start (#2987).
-  Multiple documentation updates.
-  Added completions for rmmod (#3007).
-  Improved completions for git (#2998).

.. _known-issues-2:

Known issues
------------

-  Interactive commands started from fish configuration files or from
   the ``-c`` option may, under certain circumstances, be started with
   incorrect terminal modes and fail to behave as expected. A fix is
   planned but requires further testing (#2619).

--------------

fish 2.3b2 (released May 5, 2016)
=================================

.. _significant-changes-2:

Significant changes
-------------------

-  A new ``fish_realpath`` builtin and associated function to allow the
   use of ``realpath`` even on those platforms that don’t ship an
   appropriate command (#2932).
-  Alt-# toggles the current command line between commented and
   uncommented states, making it easy to save a command in history
   without executing it.
-  The ``fish_vi_mode`` function is now deprecated in favour of
   ``fish_vi_key_bindings``.

.. _other-notable-fixes-and-improvements-1:

Other notable fixes and improvements
------------------------------------

-  Fix the build on Cygwin (#2952) and RedHat Enterprise Linux/CentOS 5
   (#2955).
-  Avoid confusing the terminal line driver with non-printing characters
   in ``fish_title`` (#2453).
-  Improved completions for busctl, git (#2585, #2879, #2984), and
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
   ``grep``, ``tr``, ``cut``, and ``awk`` in many situations. (#2296)
-  Allow using escape as the Meta modifier key, by waiting after seeing
   an escape character wait up to 300ms for an additional character.
   This is consistent with readline (e.g. bash) and can be configured
   via the ``fish_escape_delay_ms variable``. This allows using escape
   as the Meta modifier. (#1356)
-  Add new directories for vendor functions and configuration snippets
   (#2500)
-  A new ``fish_realpath`` builtin and associated ``realpath`` function
   should allow scripts to resolve path names via ``realpath``
   regardless of whether there is an external command of that name;
   albeit with some limitations. See the associated documentation.

Backward-incompatible changes
-----------------------------

-  Unmatched globs will now cause an error, except when used with
   ``for``, ``set`` or ``count`` (#2719)
-  ``and`` and ``or`` will now bind to the closest ``if`` or ``while``,
   allowing compound conditions without ``begin`` and ``end`` (#1428)
-  ``set -ql`` now searches up to function scope for variables (#2502)
-  ``status -f`` will now behave the same when run as the main script or
   using ``source`` (#2643)
-  ``source`` no longer puts the file name in ``$argv`` if no arguments
   are given (#139)
-  History files are stored under the ``XDG_DATA_HOME`` hierarchy (by
   default, in ``~/.local/share``), and existing history will be moved
   on first use (#744)

.. _other-notable-fixes-and-improvements-2:

Other notable fixes and improvements
------------------------------------

-  Fish no longer silences errors in config.fish (#2702)
-  Directory autosuggestions will now descend as far as possible if
   there is only one child directory (#2531)
-  Add support for bright colors (#1464)
-  Allow Ctrl-J (`\cj`) to be bound separately from Ctrl-M
   (`\cm`) (#217)
-  psub now has a “-s”/“–suffix” option to name the temporary file with
   that suffix
-  Enable 24-bit colors on select terminals (#2495)
-  Support for SVN status in the prompt (#2582)
-  Mercurial and SVN support have been added to the Classic + Git (now
   Classic + VCS) prompt (via the new \__fish_vcs_prompt function)
   (#2592)
-  export now handles variables with a “=” in the value (#2403)
-  New completions for:

   -  alsactl
   -  Archlinux’s asp, makepkg
   -  Atom’s apm (#2390)
   -  entr - the “Event Notify Test Runner” (#2265)
   -  Fedora’s dnf (#2638)
   -  OSX diskutil (#2738)
   -  pkgng (#2395)
   -  pulseaudio’s pacmd and pactl
   -  rust’s rustc and cargo (#2409)
   -  sysctl (#2214)
   -  systemd’s machinectl (#2158), busctl (#2144), systemd-nspawn,
      systemd-analyze, localectl, timedatectl
   -  and more

-  Fish no longer has a function called sgrep, freeing it for user
   customization (#2245)
-  A rewrite of the completions for cd, fixing a few bugs (#2299, #2300,
   #562)
-  Linux VTs now run in a simplified mode to avoid issues (#2311)
-  The vi-bindings now inherit from the emacs bindings
-  Fish will also execute ``fish_user_key_bindings`` when in vi-mode
-  ``funced`` will now also check $VISUAL (#2268)
-  A new ``suspend`` function (#2269)
-  Subcommand completion now works better with split /usr (#2141)
-  The command-not-found-handler can now be overridden by defining a
   function called ``__fish_command_not_found_handler`` in config.fish
   (#2332)
-  A few fixes to the Sorin theme
-  PWD shortening in the prompt can now be configured via the
   ``fish_prompt_pwd_dir_length`` variable, set to the length per path
   component (#2473)
-  fish no longer requires ``/etc/fish/config.fish`` to correctly start,
   and now ships a skeleton file that only contains some documentation
   (#2799)

--------------

fish 2.2.0 (released July 12, 2015)
===================================

.. _significant-changes-4:

Significant changes
-------------------

-  Abbreviations: the new ``abbr`` command allows for
   interactively-expanded abbreviations, allowing quick access to
   frequently-used commands (#731).
-  Vi mode: run ``fish_vi_mode`` to switch fish into the key bindings
   and prompt familiar to users of the Vi editor (#65).
-  New inline and interactive pager, which will be familiar to users of
   zsh (#291).
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
   behaving as expected (#1567).
-  The previously-undocumented ``CMD_DURATION`` variable is now set for
   all commands and contains the execution time of the last command in
   milliseconds (#1585). It is no longer exported to other commands
   (#1896).
-  ``if`` / ``else`` conditional statements now return values consistent
   with the Single Unix Specification, like other shells (#1443).
-  A new “top-level” local scope has been added, allowing local
   variables declared on the commandline to be visible to subsequent
   commands. (#1908)

.. _other-notable-fixes-and-improvements-3:

Other notable fixes and improvements
------------------------------------

-  New documentation design (#1662), which requires a Doxygen version
   1.8.7 or newer to build.
-  Fish now defines a default directory for other packages to provide
   completions. By default this is
   ``/usr/share/fish/vendor-completions.d``; on systems with
   ``pkgconfig`` installed this path is discoverable with
   ``pkg-config --variable completionsdir fish``.
-  A new parser removes many bugs; all existing syntax should keep
   working.
-  New ``fish_preexec`` and ``fish_postexec`` events are fired before
   and after job execution respectively (#1549).
-  Unmatched wildcards no longer prevent a job from running. Wildcards
   used interactively will still print an error, but the job will
   proceed and the wildcard will expand to zero arguments (#1482).
-  The ``.`` command is deprecated and the ``source`` command is
   preferred (#310).
-  ``bind`` supports “bind modes”, which allows bindings to be set for a
   particular named mode, to support the implementation of Vi mode.
-  A new ``export`` alias, which behaves like other shells (#1833).
-  ``command`` has a new ``--search`` option to print the name of the
   disk file that would be executed, like other shells’ ``command -v``
   (#1540).
-  ``commandline`` has a new ``--paging-mode`` option to support the new
   pager.
-  ``complete`` has a new ``--wraps`` option, which allows a command to
   (recursively) inherit the completions of a wrapped command (#393),
   and ``complete -e`` now correctly erases completions (#380).
-  Completions are now generated from manual pages by default on the
   first run of fish (#997).
-  ``fish_indent`` can now produce colorized (``--ansi``) and HTML
   (``--html``) output (#1827).
-  ``functions --erase`` now prevents autoloaded functions from being
   reloaded in the current session.
-  ``history`` has a new ``--merge`` option, to incorporate history from
   other sessions into the current session (#825).
-  ``jobs`` returns 1 if there are no active jobs (#1484).
-  ``read`` has several new options:
-  ``--array`` to break input into an array (#1540)
-  ``--null`` to break lines on NUL characters rather than newlines
   (#1694)
-  ``--nchars`` to read a specific number of characters (#1616)
-  ``--right-prompt`` to display a right-hand-side prompt during
   interactive read (#1698).
-  ``type`` has a new ``-q`` option to suppress output (#1540 and, like
   other shells, ``type -a`` now prints all matches for a command
   (#261).
-  Pressing F1 now shows the manual page for the current command
   (#1063).
-  ``fish_title`` functions have access to the arguments of the
   currently running argument as ``$argv[1]`` (#1542).
-  The OS command-not-found handler is used on Arch Linux (#1925), nixOS
   (#1852), openSUSE and Fedora (#1280).
-  ``Alt``\ +\ ``.`` searches backwards in the token history, mapping to
   the same behavior as inserting the last argument of the previous
   command, like other shells (#89).
-  The ``SHLVL`` environment variable is incremented correctly (#1634 &
   #1693).
-  Added completions for ``adb`` (#1165 & #1211), ``apt`` (#2018),
   ``aura`` (#1292), ``composer`` (#1607), ``cygport`` (#1841),
   ``dropbox`` (#1533), ``elixir`` (#1167), ``fossil``, ``heroku``
   (#1790), ``iex`` (#1167), ``kitchen`` (#2000), ``nix`` (#1167),
   ``node``/``npm`` (#1566), ``opam`` (#1615), ``setfacl`` (#1752),
   ``tmuxinator`` (#1863), and ``yast2`` (#1739).
-  Improved completions for ``brew`` (#1090 & #1810), ``bundler``
   (#1779), ``cd`` (#1135), ``emerge`` (#1840),\ ``git`` (#1680, #1834 &
   #1951), ``man`` (#960), ``modprobe`` (#1124), ``pacman`` (#1292),
   ``rpm`` (#1236), ``rsync`` (#1872), ``scp`` (#1145), ``ssh`` (#1234),
   ``sshfs`` (#1268), ``systemctl`` (#1462, #1950 & #1972), ``tmux``
   (#1853), ``vagrant`` (#1748), ``yum`` (#1269), and ``zypper``
   (#1787).

--------------

fish 2.1.2 (released Feb 24, 2015)
==================================

fish 2.1.2 contains a workaround for a filesystem bug in Mac OS X
Yosemite. #1859

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
   CVE-2014-2914). #1438
-  ``psub`` and ``funced`` are no longer vulnerable to attacks which
   allow local privilege escalation and data tampering (closing
   CVE-2014-2906 and CVE-2014-3856). #1437
-  ``fishd`` uses a secure path for its socket, preventing a local
   privilege escalation attack (closing CVE-2014-2905). #1436
-  ``__fish_print_packages`` is no longer vulnerable to attacks which
   would allow local privilege escalation and data tampering (closing
   CVE-2014-3219). #1440

Other fixes
-----------

-  ``fishd`` now ignores SIGPIPE, fixing crashes using tools like GNU
   Parallel and which occurred more often as a result of the other
   ``fishd`` changes. #1084 & #1690

--------------

fish 2.1.0
==========

.. _significant-changes-5:

Significant Changes
-------------------

-  **Tab completions will fuzzy-match files.** #568

   When tab-completing a file, fish will first attempt prefix matches
   (``foo`` matches ``foobar``), then substring matches (``ooba``
   matches ``foobar``), and lastly subsequence matches (``fbr`` matches
   ``foobar``). For example, in a directory with files foo1.txt,
   foo2.txt, foo3.txt…, you can type only the numeric part and hit tab
   to fill in the rest.

   This feature is implemented for files and executables. It is not yet
   implemented for options (like ``--foobar``), and not yet implemented
   across path components (like ``/u/l/b`` to match ``/usr/local/bin``).

-  **Redirections now work better across pipelines.** #110, #877

   In particular, you can pipe stderr and stdout together, for example,
   with ``cmd ^&1 | tee log.txt``, or the more familiar
   ``cmd 2>&1 | tee log.txt``.

-  **A single ``%`` now expands to the last job backgrounded.** #1008

   Previously, a single ``%`` would pid-expand to either all
   backgrounded jobs, or all jobs owned by your user. Now it expands to
   the last job backgrounded. If no job is in the background, it will
   fail to expand. In particular, ``fg %`` can be used to put the most
   recent background job in the foreground.

Other Notable Fixes
-------------------

-  alt-U and alt+C now uppercase and capitalize words, respectively.
   #995

-  VTE based terminals should now know the working directory. #906

-  The autotools build now works on Mavericks. #968

-  The end-of-line binding (ctrl+E) now accepts autosuggestions. #932

-  Directories in ``/etc/paths`` (used on OS X) are now prepended
   instead of appended, similar to other shells. #927

-  Option-right-arrow (used for partial autosuggestion completion) now
   works on iTerm2. #920

-  Tab completions now work properly within nested subcommands. #913

-  ``printf`` supports `\e`, the escape character. #910

-  ``fish_config history`` no longer shows duplicate items. #900

-  ``$fish_user_paths`` is now prepended to $PATH instead of appended.
   #888

-  Jobs complete when all processes complete. #876

   For example, in previous versions of fish, ``sleep 10 | echo Done``
   returns control immediately, because echo does not read from stdin.
   Now it does not complete until sleep exits (presumably after 10
   seconds).

-  Better error reporting for square brackets. #875

-  fish no longer tries to add ``/bin`` to ``$PATH`` unless PATH is
   totally empty. #852

-  History token substitution (alt-up) now works correctly inside
   subshells. #833

-  Flow control is now disabled, freeing up ctrl-S and ctrl-Q for other
   uses. #814

-  sh-style variable setting like ``foo=bar`` now produces better error
   messages. #809

-  Commands with wildcards no longer produce autosuggestions. #785

-  funced no longer freaks out when supplied with no arguments. #780

-  fish.app now works correctly in a directory containing spaces. #774

-  Tab completion cycling no longer occasionally fails to repaint. #765

-  Comments now work in eval’d strings. #684

-  History search (up-arrow) now shows the item matching the
   autosuggestion, if that autosuggestion was truncated. #650

-  Ctrl-T now transposes characters, as in other shells. #128

--------------

fish 2.0.0
==========

.. _significant-changes-6:

Significant Changes
-------------------

-  **Command substitutions now modify ``$status`` #547.** Previously the
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
   $PATH #527

   -  /etc/paths and /etc/paths.d are now respected on OS X
   -  fish no longer modifies $PATH to find its own binaries

-  **Long lines no longer use ellipsis for line breaks**, and copy and
   paste should no longer include a newline even if the line was broken
   #300

-  **New syntax for index ranges** (sometimes known as “slices”) #212

-  **fish now supports an ``else if`` statement** #134

-  **Process and pid completion now works on OS X** #129

-  **fish is now relocatable**, and no longer depends on compiled-in
   paths #125

-  **fish now supports a right prompt (RPROMPT)** through the
   fish_right_prompt function #80

-  **fish now uses posix_spawn instead of fork when possible**, which is
   much faster on BSD and OS X #11

.. _other-notable-fixes-1:

Other Notable Fixes
-------------------

-  Updated VCS completions (darcs, cvs, svn, etc.)
-  Avoid calling getcwd on the main thread, as it can hang #696
-  Control-D (forward delete) no longer stops at a period #667
-  Completions for many new commands
-  fish now respects rxvt’s unique keybindings #657
-  xsel is no longer built as part of fish. It will still be invoked if
   installed separately #633
-  \__fish_filter_mime no longer spews #628
-  The –no-execute option to fish no longer falls over when reaching the
   end of a block #624
-  fish_config knows how to find fish even if it’s not in the $PATH #621
-  A leading space now prevents writing to history, as is done in bash
   and zsh #615
-  Hitting enter after a backslash only goes to a new line if it is
   followed by whitespace or the end of the line #613
-  printf is now a builtin #611
-  Event handlers should no longer fire if signals are blocked #608
-  set_color is now a builtin #578
-  man page completions are now located in a new generated_completions
   directory, instead of your completions directory #576
-  tab now clears autosuggestions #561
-  tab completion from within a pair of quotes now attempts to
   “appropriate” the closing quote #552
-  $EDITOR can now be a list: for example, ``set EDITOR gvim -f``) #541
-  ``case`` bodies are now indented #530
-  The profile switch ``-p`` no longer crashes #517
-  You can now control-C out of ``read`` #516
-  ``umask`` is now functional on OS X #515
-  Avoid calling getpwnam on the main thread, as it can hang #512
-  Alt-F or Alt-right-arrow (Option-F or option-right-arrow) now accepts
   one word of an autosuggestion #435
-  Setting fish as your login shell no longer kills OpenSUSE #367
-  Backslashes now join lines, instead of creating multiple commands
   #347
-  echo now implements the -e flag to interpret escapes #337
-  When the last token in the user’s input contains capital letters, use
   its case in preference to that of the autosuggestion #335
-  Descriptions now have their own muted color #279
-  Wildcards beginning with a . (for example, ``ls .*``) no longer match
   . and .. #270
-  Recursive wildcards now handle symlink loops #268
-  You can now delete history items from the fish_config web interface
   #250
-  The OS X build now weak links ``wcsdup`` and ``wcscasecmp`` #240
-  fish now saves and restores the process group, which prevents certain
   processes from being erroneously reported as stopped #197
-  funced now takes an editor option #187
-  Alternating row colors are available in fish pager through
   ``fish_pager_color_secondary`` #186
-  Universal variable values are now stored based on your MAC address,
   not your hostname #183
-  The caret ^ now only does a stderr redirection if it is the first
   character of a token, making git users happy #168
-  Autosuggestions will no longer cause line wrapping #167
-  Better handling of Unicode combining characters #155
-  fish SIGHUPs processes more often #138
-  fish no longer causes ``sudo`` to ask for a password every time
-  fish behaves better under Midnight Commander #121
-  ``set -e`` no longer crashes #100
-  fish now will automatically import history from bash, if there is no
   fish history #66
-  Backslashed-newlines inside quoted strings now behave more
   intuitively #52
-  Tab titles should be shown correctly in iTerm2 #47
-  scp remote path completion now sometimes works #42
-  The ``read`` builtin no longer shows autosuggestions #29
-  Custom key bindings can now be set via the ``fish_user_key_bindings``
   function #21
-  All Python scripts now run correctly under both Python 2 and Python 3
   #14
-  The “accept autosuggestion” key can now be configured #19
-  Autosuggestions will no longer suggest invalid commands #6

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
-  A SIGTERM now ends the whole execution stack again (resolving #13).
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
