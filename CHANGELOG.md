# fish $nextversion (unreleased)

## Significant changes

- The clipboard integration has been removed in favor of explicit bindings (#3061)

# fish 2.3.0 (released May 20, 2016)

There are no significant changes between 2.3.0 and 2.3b2.

## Other notable fixes and improvements

- `abbr` now allows non-letter keys (#2996).
- Define a few extra colours on first start (#2987).
- Multiple documentation updates.
- Added completions for rmmod (#3007).
- Improved completions for git (#2998).

## Known issues

- Interactive commands started from fish configuration files or from the `-c` option may, under certain circumstances, be started with incorrect terminal modes and fail to behave as expected. A fix is planned but requires further testing (#2619).

---

# fish 2.3b2 (released May 5, 2016)

## Significant changes

- A new `fish_realpath` builtin and associated function to allow the use of `realpath` even on those platforms that don't ship an appropriate command (#2932).
- Alt-# toggles the current command line between commented and uncommented states, making it easy to save a command in history without executing it.
- The `fish_vi_mode` function is now deprecated in favour of `fish_vi_key_bindings`.

## Other notable fixes and improvements

- Fix the build on Cygwin (#2952) and RedHat Enterprise Linux/CentOS 5 (#2955).
- Avoid confusing the terminal line driver with non-printing characters in `fish_title` (#2453).
- Improved completions for busctl, git (#2585, #2879, #2984), and netctl.

---

# fish 2.3b1 (released April 19, 2016)

## Significant Changes

- A new `string` builtin to handle... strings! This builtin will measure, split, search and replace text strings, including using regular expressions. It can also be used to turn lists into plain strings using `join`. `string` can be used in place of `sed`, `grep`, `tr`, `cut`, and `awk` in many situations. (#2296)
- Allow using escape as the Meta modifier key, by waiting after seeing an escape character wait up to 300ms for an additional character. This is consistent with readline (e.g. bash) and can be configured via the `fish_escape_delay_ms variable`. This allows using escape as the Meta modifier. (#1356)
- Add new directories for vendor functions and configuration snippets (#2500)
- A new `fish_realpath` builtin and associated `realpath` function should allow scripts to resolve path names via `realpath` regardless of whether there is an external command of that name; albeit with some limitations. See the associated documentation.

## Backward-incompatible changes

- Unmatched globs will now cause an error, except when used with `for`, `set` or `count` (#2719)
- `and` and `or` will now bind to the closest `if` or `while`, allowing compound conditions without `begin` and `end` (#1428)
- `set -ql` now searches up to function scope for variables (#2502)
- `status -f` will now behave the same when run as the main script or using `source` (#2643)
- `source` no longer puts the file name in `$argv` if no arguments are given (#139)
- History files are stored under the `XDG_DATA_HOME` hierarchy (by default, in `~/.local/share`), and existing history will be moved on first use (#744)

## Other notable fixes and improvements

- Fish no longer silences errors in config.fish (#2702)
- Directory autosuggestions will now descend as far as possible if there is only one child directory (#2531)
- Add support for bright colors (#1464)
- Allow Ctrl-J (\cj) to be bound separately from Ctrl-M (\cm) (#217)
- psub now has a "-s"/"&#x2013;suffix" option to name the temporary file with that suffix
- Enable 24-bit colors on select terminals (#2495)
- Support for SVN status in the prompt (#2582)
- Mercurial and SVN support have been added to the Classic + Git (now Classic + VCS) prompt (via the new \__fish_vcs_prompt function) (#2592)
- export now handles variables with a "=" in the value (#2403)
- New completions for:
    -   alsactl
    -   Archlinux's asp, makepkg
    -   Atom's apm (#2390)
    -   entr - the "Event Notify Test Runner" (#2265)
    -   Fedora's dnf (#2638)
    -   OSX diskutil (#2738)
    -   pkgng (#2395)
    -   pulseaudio's pacmd and pactl
    -   rust's rustc and cargo (#2409)
    -   sysctl (#2214)
    -   systemd's machinectl (#2158), busctl (#2144), systemd-nspawn, systemd-analyze, localectl, timedatectl
    -   and more
- Fish no longer has a function called sgrep, freeing it for user customization (#2245)
- A rewrite of the completions for cd, fixing a few bugs (#2299, #2300, #562)
- Linux VTs now run in a simplified mode to avoid issues (#2311)
- The vi-bindings now inherit from the emacs bindings
- Fish will also execute `fish_user_key_bindings` when in vi-mode
- `funced` will now also check $VISUAL (#2268)
- A new `suspend` function (#2269)
- Subcommand completion now works better with split /usr (#2141)
- The command-not-found-handler can now be overridden by defining a function called `__fish_command_not_found_handler` in config.fish (#2332)
- A few fixes to the Sorin theme
- PWD shortening in the prompt can now be configured via the `fish_prompt_pwd_dir_length` variable, set to the length per path component (#2473)
- fish no longer requires `/etc/fish/config.fish` to correctly start, and now ships a skeleton file that only contains some documentation (#2799)

---

# fish 2.2.0 (released July 12, 2015)

### Significant changes ###

 * Abbreviations: the new `abbr` command allows for interactively-expanded abbreviations, allowing quick access to frequently-used commands (#731).
 * Vi mode: run `fish_vi_mode` to switch fish into the key bindings and prompt familiar to users of the Vi editor (#65).
 * New inline and interactive pager, which will be familiar to users of zsh (#291).
 * Underlying architectural changes: the `fishd` universal variable server has been removed as it was a source of many bugs and security problems. Notably, old fish sessions will not be able to communicate universal variable changes with new fish sessions. For best results, restart all running instances of `fish`.
 * The web-based configuration tool has been redesigned, featuring a prompt theme chooser and other improvements.
 * New German, Brazilian Portuguese, and Chinese translations.

### Backward-incompatible changes ###

These are kept to a minimum, but either change undocumented features or are too hard to use in their existing forms. These changes may break existing scripts.

 * `commandline` no longer interprets functions "in reverse", instead behaving as expected (#1567).
 * The previously-undocumented `CMD_DURATION` variable is now set for all commands and contains the execution time of the last command in milliseconds (#1585). It is no longer exported to other commands (#1896).
 * `if` / `else` conditional statements now return values consistent with the Single Unix Specification, like other shells (#1443).
 * A new "top-level" local scope has been added, allowing local variables declared on the commandline to be visible to subsequent commands. (#1908)

### Other notable fixes and improvements ###

 * New documentation design (#1662), which requires a Doxygen version 1.8.7 or newer to build.
 * Fish now defines a default directory for other packages to provide completions. By default this is `/usr/share/fish/vendor-completions.d`; on systems with `pkgconfig` installed this path is discoverable with `pkg-config --variable completionsdir fish`.
 * A new parser removes many bugs; all existing syntax should keep working.
 * New `fish_preexec` and `fish_postexec` events are fired before and after job execution respectively (#1549).
 * Unmatched wildcards no longer prevent a job from running. Wildcards used interactively will still print an error, but the job will proceed and the wildcard will expand to zero arguments (#1482).
 * The `.` command is deprecated and the `source` command is preferred (#310).
 * `bind` supports "bind modes", which allows bindings to be set for a particular named mode, to support the implementation of Vi mode.
 * A new `export` alias, which behaves like other shells (#1833).
 * `command` has a new `--search` option to print the name of the disk file that would be executed, like other shells' `command -v` (#1540).
 * `commandline` has a new `--paging-mode` option to support the new pager.
 * `complete` has a new `--wraps` option, which allows a command to (recursively) inherit the completions of a wrapped command (#393), and `complete -e` now correctly erases completions (#380).
 * Completions are now generated from manual pages by default on the first run of fish (#997).
 * `fish_indent` can now produce colorized (`--ansi`) and HTML (`--html`) output (#1827).
 * `functions --erase` now prevents autoloaded functions from being reloaded in the current session.
 * `history` has a new `--merge` option, to incorporate history from other sessions into the current session (#825).
 * `jobs` returns 1 if there are no active jobs (#1484).
 * `read` has several new options:
  * `--array` to break input into an array (#1540)
  * `--null` to break lines on NUL characters rather than newlines (#1694)
  * `--nchars` to read a specific number of characters (#1616)
  * `--right-prompt` to display a right-hand-side prompt during interactive read (#1698).
 * `type` has a new `-q` option to suppress output (#1540 and, like other shells, `type -a` now prints all matches for a command (#261).
 * Pressing F1 now shows the manual page for the current command (#1063).
 * `fish_title` functions have access to the arguments of the currently running argument as `$argv[1]` (#1542).
 * The OS command-not-found handler is used on Arch Linux (#1925), nixOS (#1852), openSUSE and Fedora (#1280).
 * `Alt`+`.` searches backwards in the token history, mapping to the same behavior as inserting the last argument of the previous command, like other shells (#89).
 * The `SHLVL` environment variable is incremented correctly (#1634 & #1693).
 * Added completions for `adb` (#1165 & #1211), `apt` (#2018), `aura` (#1292), `composer` (#1607), `cygport` (#1841), `dropbox` (#1533), `elixir` (#1167), `fossil`, `heroku` (#1790), `iex` (#1167), `kitchen` (#2000), `nix` (#1167), `node`/`npm` (#1566), `opam` (#1615), `setfacl` (#1752), `tmuxinator` (#1863), and `yast2` (#1739).
 * Improved completions for `brew` (#1090 & #1810), `bundler` (#1779), `cd` (#1135), `emerge` (#1840),`git` (#1680, #1834 & #1951), `man` (#960), `modprobe` (#1124), `pacman` (#1292), `rpm` (#1236), `rsync` (#1872), `scp` (#1145), `ssh` (#1234), `sshfs` (#1268), `systemctl` (#1462, #1950 & #1972), `tmux` (#1853), `vagrant` (#1748), `yum` (#1269), and `zypper` (#1787).

---

# fish 2.1.2  (released Feb 24, 2015)

fish 2.1.2 contains a workaround for a filesystem bug in Mac OS X Yosemite. #1859

Specifically, after installing fish 2.1.1 and then rebooting, "Verify Disk" in Disk Utility will report "Invalid number of hard links." We don't have any reports of data loss or other adverse consequences. fish 2.1.2 avoids triggering the bug, but does not repair an already affected filesystem. To repair the filesystem, you can boot into Recovery Mode and use Repair Disk from Disk Utility. Linux and versions of OS X prior to Yosemite are believed to be unaffected.

There are no other changes in this release.

---

# fish 2.1.1 (released September 26, 2014)

__Important:__ if you are upgrading, stop all running instances of `fishd` as soon as possible after installing this release; it will be restarted automatically. On most systems, there will be no further action required. Note that some environments (where `XDG_RUNTIME_DIR` is set), such as Fedora 20, will require a restart of all running fish processes before universal variables work as intended.

Distributors are highly encouraged to call `killall fishd`, `pkill fishd` or similar in installation scripts, or to warn their users to do so.

### Security fixes
 * The fish_config web interface now uses an authentication token to protect requests and only responds to requests from the local machine with this token, preventing a remote code execution attack. (closing CVE-2014-2914). #1438
 * `psub` and `funced` are no longer vulnerable to attacks which allow local privilege escalation and data tampering (closing CVE-2014-2906 and CVE-2014-3856). #1437
 * `fishd` uses a secure path for its socket, preventing a local privilege escalation attack (closing CVE-2014-2905). #1436
 * `__fish_print_packages` is no longer vulnerable to attacks which would allow local privilege escalation and data tampering (closing CVE-2014-3219). #1440

### Other fixes
 * `fishd` now ignores SIGPIPE, fixing crashes using tools like GNU Parallel and which occurred more often as a result of the other `fishd` changes. #1084 & #1690

---

# fish 2.1.0

Significant Changes
-------------------

* **Tab completions will fuzzy-match files.** #568

  When tab-completing a file, fish will first attempt prefix matches (`foo` matches `foobar`), then substring matches (`ooba` matches `foobar`), and lastly subsequence matches (`fbr` matches `foobar`). For example, in a directory with files foo1.txt, foo2.txt, foo3.txt…, you can type only the numeric part and hit tab to fill in the rest.

  This feature is implemented for files and executables. It is not yet implemented for options (like `--foobar`), and not yet implemented across path components (like `/u/l/b` to match `/usr/local/bin`).

* **Redirections now work better across pipelines.** #110, #877

  In particular, you can pipe stderr and stdout together, for example, with `cmd ^&1 | tee log.txt`, or the more familiar `cmd 2>&1 | tee log.txt`.

* **A single `%` now expands to the last job backgrounded.** #1008

  Previously, a single `%` would pid-expand to either all backgrounded jobs, or all jobs owned by your user. Now it expands to the last job backgrounded. If no job is in the background, it will fail to expand. In particular, `fg %` can be used to put the most recent background job in the foreground.

Other Notable Fixes
-------------------

* alt-U and alt+C now uppercase and capitalize words, respectively. #995

* VTE based terminals should now know the working directory. #906

* The autotools build now works on Mavericks. #968

* The end-of-line binding (ctrl+E) now accepts autosuggestions. #932

* Directories in `/etc/paths` (used on OS X) are now prepended instead of appended, similar to other shells. #927

* Option-right-arrow (used for partial autosuggestion completion) now works on iTerm2. #920

* Tab completions now work properly within nested subcommands. #913

* `printf` supports \e, the escape character. #910

* `fish_config history` no longer shows duplicate items. #900

* `$fish_user_paths` is now prepended to $PATH instead of appended. #888

* Jobs complete when all processes complete. #876


  For example, in previous versions of fish, `sleep 10 | echo Done` returns control immediately, because echo does not read from stdin. Now it does not complete until sleep exits (presumably after 10 seconds).

* Better error reporting for square brackets. #875

* fish no longer tries to add `/bin` to `$PATH` unless PATH is totally empty. #852

* History token substitution (alt-up) now works correctly inside subshells. #833

* Flow control is now disabled, freeing up ctrl-S and ctrl-Q for other uses. #814

* sh-style variable setting like `foo=bar` now produces better error messages. #809

* Commands with wildcards no longer produce autosuggestions. #785

* funced no longer freaks out when supplied with no arguments. #780

* fish.app now works correctly in a directory containing spaces. #774

* Tab completion cycling no longer occasionally fails to repaint. #765

* Comments now work in eval'd strings. #684

* History search (up-arrow) now shows the item matching the autosuggestion, if that autosuggestion was truncated. #650

* Ctrl-T now transposes characters, as in other shells. #128

---

# fish 2.0.0

Significant Changes
-------------------

* **Command substitutions now modify `$status` #547.**
  Previously the exit status of command substitutions (like `(pwd)`) was ignored; however now it modifies $status. Furthermore, the `set` command now only sets $status on failure; it is untouched on success. This allows for the following pattern:

  ```sh
  if set python_path (which python)
     ...
  end
  ```
  Because set does not modify $status on success, the if branch effectively tests whether `which` succeeded, and if so, whether the `set` also succeeded.
* **Improvements to $PATH handling.**
    * There is a new variable, `$fish_user_paths`, which can be set universally, and whose contents are appended to $PATH #527
    * /etc/paths and /etc/paths.d are now respected on OS X
    * fish no longer modifies $PATH to find its own binaries
* **Long lines no longer use ellipsis for line breaks**, and copy and paste
  should no longer include a newline even if the line was broken #300
* **New syntax for index ranges** (sometimes known as "slices") #212
* **fish now supports an `else if` statement** #134
* **Process and pid completion now works on OS X** #129
* **fish is now relocatable**, and no longer depends on compiled-in paths #125
* **fish now supports a right prompt (RPROMPT)** through the fish_right_prompt function #80
* **fish now uses posix_spawn instead of fork when possible**, which is much faster on BSD and OS X #11

Other Notable Fixes
-------------------

* Updated VCS completions (darcs, cvs, svn, etc.)
* Avoid calling getcwd on the main thread, as it can hang #696
* Control-D (forward delete) no longer stops at a period #667
* Completions for many new commands
* fish now respects rxvt's unique keybindings #657
* xsel is no longer built as part of fish. It will still be invoked if installed separately #633
* __fish_filter_mime no longer spews #628
* The --no-execute option to fish no longer falls over when reaching the end of a block #624
* fish_config knows how to find fish even if it's not in the $PATH #621
* A leading space now prevents writing to history, as is done in bash and zsh #615
* Hitting enter after a backslash only goes to a new line if it is followed by whitespace or the end of the line #613
* printf is now a builtin #611
* Event handlers should no longer fire if signals are blocked #608
* set_color is now a builtin #578
* man page completions are now located in a new generated_completions directory, instead of your completions directory #576
* tab now clears autosuggestions #561
* tab completion from within a pair of quotes now attempts to "appropriate" the closing quote #552
* $EDITOR can now be a list: for example, `set EDITOR gvim -f`) #541
* `case` bodies are now indented #530
* The profile switch `-p` no longer crashes #517
* You can now control-C out of `read` #516
* `umask` is now functional on OS X #515
* Avoid calling getpwnam on the main thread, as it can hang #512
* Alt-F or Alt-right-arrow (Option-F or option-right-arrow) now accepts one word of an autosuggestion #435
* Setting fish as your login shell no longer kills OpenSUSE #367
* Backslashes now join lines, instead of creating multiple commands #347
* echo now implements the -e flag to interpret escapes #337
* When the last token in the user's input contains capital letters, use its case in preference to that of the autosuggestion #335
* Descriptions now have their own muted color #279
* Wildcards beginning with a . (for example, `ls .*`) no longer match . and .. #270
* Recursive wildcards now handle symlink loops #268
* You can now delete history items from the fish_config web interface #250
* The OS X build now weak links `wcsdup` and `wcscasecmp` #240
* fish now saves and restores the process group, which prevents certain processes from being erroneously reported as stopped #197
* funced now takes an editor option #187
* Alternating row colors are available in fish pager through `fish_pager_color_secondary` #186
* Universal variable values are now stored based on your MAC address, not your hostname #183
* The caret ^ now only does a stderr redirection if it is the first character of a token, making git users happy #168
* Autosuggestions will no longer cause line wrapping #167
* Better handling of Unicode combining characters #155
* fish SIGHUPs processes more often #138
* fish no longer causes `sudo` to ask for a password every time
* fish behaves better under Midnight Commander #121
* `set -e` no longer crashes #100
* fish now will automatically import history from bash, if there is no fish history #66
* Backslashed-newlines inside quoted strings now behave more intuitively #52
* Tab titles should be shown correctly in iTerm2 #47
* scp remote path completion now sometimes works #42
* The `read` builtin no longer shows autosuggestions #29
* Custom key bindings can now be set via the `fish_user_key_bindings` function #21
* All Python scripts now run correctly under both Python 2 and Python 3 #14
* The "accept autosuggestion" key can now be configured #19
* Autosuggestions will no longer suggest invalid commands #6

---

# fishfish Beta r2

Bug Fixes
---------

* **Implicit cd** is back, for paths that start with one or two dots, a slash, or a tilde.
* **Overrides of default functions should be fixed.** The "internalized scripts" feature is disabled for now.
* **Disabled delayed suspend.** This is a strange job-control feature of BSD systems, including OS X. Disabling it frees up Control Y for other purposes; in particular, for yank, which now works on OS X.
* **fish_indent is fixed.** In particular, the `funced` and `funcsave` functions work again.
* A SIGTERM now ends the whole execution stack again (resolving #13).
* Bumped the __fish_config_interactive version number so the default fish_color_autosuggestion kicks in.
* fish_config better handles combined term256 and classic colors like "555 yellow". 

New Features
------------

* **A history builtin**, and associated interactive function that enables deleting history items. Example usage:
      * Print all history items beginning with echo: `history --prefix echo`
      * Print all history items containing foo: `history --contains foo`
      * Interactively delete some items containing foo: `history --delete --contains foo`

Credit to @siteshwar for implementation. Thanks @siteshwar!

---

# fishfish Beta r1

## Scripting
* No changes! All existing fish scripts, config files, completions, etc. from trunk should continue to work.

## New Features
* **Autosuggestions**. Think URL fields in browsers. When you type a command, fish will suggest the rest of the command after the cursor, in a muted gray when possible. You can accept the suggestion with the right arrow key or Ctrl-F. Suggestions come from command history, completions, and some custom code for cd; there's a lot of potential for improvement here. The suggestions are computed on a background pthread, so they never slow down your typing. The autosuggestion feature is incredible. I miss it dearly every time I use anything else.

* **term256 support** where available, specifically modern xterms and OS X Lion. You can specify colors the old way ('set_color cyan') or by specifying RGB hex values ('set_color FF3333'); fish will pick the closest supported color. Some xterms do not advertise term256 support either in the $TERM or terminfo max_colors field, but nevertheless support it. For that reason, fish will default into using it on any xterm (but it can be disabled with an environment variable).

* **Web-based configuration** page. There is a new function 'fish_config'. This spins up a simple Python web server and opens a browser window to it. From this web page, you can set your shell colors and view your functions, variables, and history; all changes apply immediately to all running shells. Eventually all configuration ought to be supported via this mechanism (but in addition to, not instead of, command line mechanisms).

* **Man page completions**. There is a new function 'fish_update_completions'. This function reads all the man1 files from your manpath, removes the roff formatting, parses them to find the commands and options, and outputs fish completions into ~/.config/fish/completions. It won't overwrite existing completion files (except ones that it generated itself).

## Programmatic Changes
* fish is now entirely in C++. I have no particular love for C++, but it provides a ready memory-model to replace halloc. We've made an effort to keep it to a sane and portable subset (no C++11, no boost, no going crazy with templates or smart pointers), but we do use the STL and a little tr1.
* halloc is entirely gone, replaced by normal C++ ownership semantics. If you don't know what halloc is, well, now you have two reasons to be happy.
* All the crufty C data structures are entirely gone. array_list_t, priority_queue_t, hash_table_t, string_buffer_t have been removed and replaced by STL equivalents like std::vector, std::map, and std::wstring. A lot of the string handling now uses std::wstring instead of wchar_t *
* fish now spawns pthreads for tasks like syntax highlighting that require blocking I/O.
* History has been completely rewritten. History files now use an extensible YAML-style syntax. History "merging" (multiple shells writing to the same history file) now works better. There is now a maximum history length of about 250k items (256 * 1024).
* The parser has been "instanced," so you can now create more than one.
* Total #LoC has shrunk slightly even with the new features.

## Performance
* fish now runs syntax highlighting in a background thread, so typing commands is always responsive even on slow filesystems.
* echo, test, and pwd are now builtins, which eliminates many forks.
* The files in share/functions and share/completions now get 'internalized' into C strings that get compiled in with fish. This substantially reduces the number of files touched at startup. A consequence is that you cannot change these functions without recompiling, but often other functions depend on these "standard" functions, so changing them is perhaps not a good idea anyways.

Here are some system call counts for launching and then exiting fish with the default configuration, on OS X. The first column is fish trunk, the next column is with our changes, and the last column is bash for comparison. This data was collected via dtrace.

<table>
<tr> <th> <th> before <th> after <th> bash
<tr> <th> open <td> 9 <td> 4 <td> 5
<tr> <th> fork <td> 28 <td> 14 <td> 0
<tr> <th> stat <td> 131 <td> 85 <td> 11
<tr> <th> lstat <td> 670 <td> 0 <td> 0
<tr> <th> read <td> 332 <td> 80 <td> 4
<tr> <th> write <td> 172 <td> 149 <td> 0
</table>

The large number of forks relative to bash are due to fish's insanely expensive default prompt, which is unchanged in my version. If we switch to a prompt comparable to bash's (lame) default, the forks drop to 16 with trunk, 4 after our changes.

The large reduction in lstat() numbers is due to fish no longer needing to call ttyname() on OS X.

We've got some work to do to be as lean as bash, but we're on the right track. 
