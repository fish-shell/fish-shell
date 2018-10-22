# fish 3.0b1

fish 3.0 is a major release which brings with it both improvements in functionality and some breaking changes. All fish scripts should be reviewed

## Deprecations
- The `IFS` variable is deprecated and will be removed in fish 4.0 (#4156).
- The `function --on-process-exit` event will be removed in future (#4700). Use the `fish_exit` event instead.
- `$_` is deprecated and will removed in the future (#813). Use `status current-command` in a subshell instead.
- `^` as a redirection deprecated and will be removed in the future. (#4394). Use `2>` to redirect stderr. This is controlled by the `stderr-nocaret` feature flag.
- `?` as a glob is deprecated and will be removed in the future. (#4520). This is controlled by the `qmark-noglob` feature flag.

## Notable non-backward compatible changes
- `%` is no longer used for process and job expansion, except for `%self` which is retained. Some commands have been wrapped to still understand process expansion, including `bg`, `fg` and `kill` (#4230, #1202)
- Incoming environment variables are no longer split into arrays based on RS. Instead, variables are not split, unless their name ends in PATH, in which case they are split on colons. (#436)
- A literal `{}` now expands to itself, rather than nothing. This makes working with `find -exec` easier. (#1109, #4632)
- Successive commas in brace expansions are handled in less surprising manner (`{,,,}` expands to four empty strings rather than an empty string, a comma and an empty string again). (#3002, #4632).
- `for` loop control variables are no longer local to the `for` block (#1935).
- Variables set in `if` and `while` conditions are available outside the block (#4820).
- Local exported (`set -lx`) vars are now visible to functions (#1091).
- `set x[1] x[2] a b` is no longer valid syntax (#4236).
- The new `math` builtin (see below) does not support logical expressions; `test` should be used instead (#4777).
- Range expansion (`$foo[1..5]`) will now always go forward if only the end is negative, and in reverse if just the start is. This is to enable clamping to the last valid index without changing direction if the list has fewer elements than expected.
- Background jobs not first `disown`'d will be reaped upon `exec`, bringing the behavior in line with that of `exit`.
- `read` now uses `-s` as short for `--silent` (à la `bash`); `--shell`'s abbreviation (formerly `-s`) is now `-S` instead (#4490).
- `cd` no longer resolves symlinks. fish now maintains a virtual path, matching other shells. (#3350).
- `source` now requires an explicit `-` as the filename to read from the terminal (#2633).

## Notable fixes and improvements
### Syntax/semantic changes and new builtins
- fish now supports `&&`, `||`, and `!` (#4620).
- Variables may be used as commands (#154).
- fish may be started in private mode via `fish --private` or `fish -P`. Private mode fish sessions do not have access to the history file and any commands evaluated in private mode are not persisted for future sessions. A session variable `$fish_private_mode` can be queried to detect private mode and adjust the behavior of scripts accordingly to respect the user's wish for privacy.
- A new feature flags mechanism is added for staging deprecations and breaking changes. Feature flags may be specified at launch with `fish --features ...` or by setting the universal `fish_features` variable. (#4940)
- `wait` builtin is added for waiting on processes (#4498).
- `math` is now a builtin rather than a wrapper around `bc` (#3157). The default scale is now 6, so that floating point computations produce decimals (#4478).
- Using a read-only variable in a for loop is now an error. Note that this never worked. It simply failed to set the for loop var and thus silently produced incorrect results (#4342).
- Arguments to `end` are now errors, instead of being silently ignored.
- Setting `$PATH` no longer warns on non-existent directories, allowing for a single $PATH to be shared across machines (e.g. via dotfiles).
- A pipe at the end of a line now allows the job to continue on the next line (#1285).
- The names `argparse`, `read`, `set`, `status`, `test` and `[` are now reserved and not allowed as function names. This prevents users unintentionally breaking stuff (#3000).
- `while` sets `$status` to a more useful value (#4982)
- Command substitution output is now limited to 10 MB by default (#3822).
- The machine hostname, where available, is now exposed as `$hostname` which is now a reserved variable. This drops the dependency on the `hostname` executable (#4422).
- The universal variables file no longer contains the MAC address. It is now at the fixed location `.config/fish/fish_universal_variables` (#1912).
- Bare `bind` invocations in config.fish now work. The `fish_user_key_bindings` function is no longer necessary, but will still be executed if it exists (#5191).
- `$fish_pid` and `$last_pid` are available as an alternatives to `%self` and `%last`.

### New features in builtins
- `alias` now has a `-s` and `--save` option to save the function generated by the alias using `funcsave` (#4878).
- `bind` has a new `--silent` option to ignore bind requests for named keys not available under the current `$TERMINAL` (#4188, #4431).
- `complete` now has a `-k` and `--keep-order` option to keep the order of the `OPTION_ARGUMENTS` (#361).
- `exec` now triggers the same safety features as `exit` and prompts for confirmation if background jobs are running.
- `funced` now has a `-s` and `--save` option to automatically save the edited function after successfully editing (#4668).
- `functions --handlers` can be used to show event handlers (#4694).
- The `-d` option to `functions` to set the description of an existing function now works; before 3.0 it was documented but unimplemented. Note that the long form `--description` continues to work. (#5105)
- `history search` supports globs for wildcard searching (#3136).
- The `jobs` builtin now has a `-q` and `--quiet` option to silence the output.
- `read` has a new `--delimiter` option as a better alternative to the `IFS` variable (#4256).
- `read` writes directly to stdout if called without arguments (#4407)
- `read` can now read one or more individual lines from the input stream without consuming the input in its entirety via `read -L/--line`. Refer to the `read` documentation for more info.
- `set` has a new `--append` and `--prepend` option (#1326).
- `set` has a new `--show` option to show lots of information about variables (#4265).
- `string match` with an empty pattern and "--entire" in glob mode now matches everything instead of nothing (#4971).
- `string split` supports `-n/--no-empty` to exclude empty strings from the result (#4779).
- `string` builtin has new commands `split0` and `join0` for working with NUL-delimited output.
- `string` can now read NULs via stdin, and will no longer stop processing text after them (#4605)
- `test` now supports floating point values in numeric comparisons.

### Performance improvements
- `abbr` has been reimplemented to be faster. This means the old `fish_user_abbreviations` variable is ignored (#4048).
- Setting variables is much faster (#4200, #4341).
- Globs are faster (#4579).
- `string` reads from stdin faster by a factor of 10 (#4610).
- Slicing $history (in particular, `$history[1]` for the last executed command) is much faster.

### Interactive improvements and completions
- `cd` tab completions no longer descend into the deepest unambiguous path (#4649).
- `sudo` completions now provide completions for the target of the sudo command.
- Pager navigation has been improved. Most notably, moving down now wraps around, moving up from the commandline now jumps to the last element and moving right and left now reverse each other even when wrapping around (#4680).
- Typing normal characters while the completion pager is active no longer shows the search field. Instead it enters them into the command line, and ends paging (#2249).
- A new input binding `pager-toggle-search` toggles the search field in the completions pager on and off. By default this is bound to control-s.
- Searching in the pager now does a full fuzzy search (#5213).
- The pager will now show the full command instead of just its last line if the number of completions is large (#4702).
- Tildes in file names are now properly escaped in completions (#2274).
- Wrapping completions (from `complete -w` or `function -w`) can now inject arguments. For example, `complete gco -w 'git checkout'` now works properly (#1976). The `alias` function has been updated to respect this behavior.
- Path completions now support expansions, meaning expressions like `python ~/<TAB>` now provides file suggestions just like any other relative or absolute path. (This includes support for other expansions, too.)
- Autosuggestions try to avoid arguments that are already present in the command line.
- Notification about crashed processes is now always shown, even in command substitutions (#4962).
- The screen is no longer reset after a BEL, fixing graphical glitches (#3693).
- vi-mode now supports ';' and ',' motions. This introduces new {forward,backward}-jump-till and repeat-jump{,-reverse} bind functions (#5140).
- The `*y` vi-mode binding now works (#5100).
- Truecolor is now enabled in neovim by default (#2792).
- Terminal size ($COLUMNS/$LINES) is now updated before fish_prompt is called, allowing the prompt to react (improves #904).
- Multi-line prompts no longer repeat when the terminal is resized (#2320).
- `xclip` support in the clipboard integration (#5020).
- $cmd_duration is no longer updated when no command is executed (#5011).

- Added completions for
  - `bd` (#4472)
  - `bower`
  - `configure` (autoconf only)
  - `doas` (#5196)
  - `hjson`
  - `j` (autojump #4344)
  - `jhipster` (#4472)
  - `kldload`
  - `kldunload`
  - `meson`
  - `ngrok` (#4642)
  - `optipng`
  - `opkg` (#5168)
  - `port`
  - `serve` (#5026)
  - `ttx`
  - `unzip`
  - `xsv`
- Lots of improvements to completions (especially `darcs` (#5112), `git` and `hg`).
- Completions for `yarn` and `npm` now require the `all-the-package-names` NPM package for full
  functionality.
- Completions for `bower` and `yarn` now require the `jq` utility for full functionality.
- Improved French translations.

### Other fixes and improvements
- Fish's internal wcwidth function has been updated to deal with newer unicode, and the width of some characters can be configured via the "$fish_ambiguous_width" (#5149) and "$fish_emoji_width" (#2652) variables.
- Alternatively, a new build-time option INTERNAL_WCWIDTH can be used to use the system's wcwidth instead (#4816).
- Fuzzy translations are no longer used as they often produced nonsense and could even trigger crashes (#4847).
- /etc/paths is now parsed like macOS' path_helper would, fixing $PATH order (#4336, #4852).

--

# fish 2.7.1 (released December 23, 2017)

This release of fish fixes an issue where iTerm 2 on macOS would display a warning about paste bracketing being left on when starting a new fish session (#4521).

If you are upgrading from version 2.6.0 or before, please also review the release notes for 2.7.0 and 2.7b1 (included below).

--

# fish 2.7.0 (released November 23, 2017)

There are no major changes between 2.7b1 and 2.7.0. If you are upgrading from version 2.6.0 or before, please also review the release notes for 2.7b1 (included below).

Xcode builds and macOS packages could not be produced with 2.7b1, but this is fixed in 2.7.0.

--

# fish 2.7b1 (released October 31, 2017)

## Notable improvements
- A new `cdh` (change directory using recent history) command provides a more friendly alternative to prevd/nextd and pushd/popd (#2847).
- A new `argparse` command is available to allow fish script to parse arguments with the same behavior as builtin commands. This also includes the `fish_opt` helper command. (#4190).
- Invalid array indexes are now silently ignored (#826, #4127).
- Improvements to the debugging facility, including a prompt specific to the debugger (`fish_breakpoint_prompt`) and a `status is-breakpoint` subcommand (#1310).
- `string` supports new `lower` and `upper` subcommands, for altering the case of strings (#4080). The case changing is not locale-aware yet.- `string escape` has a new `--style=xxx` flag where `xxx` can be `script`, `var`, or `url` (#4150), and can be reversed with `string unescape` (#3543).
- History can now be split into sessions with the `fish_history` variable, or not saved to disk at all (#102).
- Read history is now controlled by the `fish_history` variable rather than the `--mode-name` flag (#1504).
- `command` now supports an `--all` flag to report all directories with the command. `which` is no longer a runtime dependency (#2778).
- fish can run commands before starting an interactive session using the new `--init-command`/`-C` options (#4164).
- `set` has a new `--show` option to show lots of information about variables (#4265).

## Other significant changes
- The `COLUMNS` and `LINES` environment variables are now correctly set the first time `fish_prompt` is run (#4141).
- `complete`'s `--no-files` option works as intended (#112).
- `echo -h` now correctly echoes `-h` in line with other shells (#4120).
- The `export` compatibility function now returns zero on success, rather than always returning 1 (#4435).
- Stop converting empty elements in MANPATH to "." (#4158). The behavior being changed was introduced in fish 2.6.0.
- `count -h` and `count --help` now return 1 rather than produce command help output (#4189).
- An attempt to `read` which stops because too much data is available still defines the variables given as parameters (#4180).
- A regression in fish 2.4.0 which prevented `pushd +1` from working has been fixed (#4091).
- A regression in fish 2.6.0 where multiple `read` commands in non-interactive scripts were broken has been fixed (#4206).
- A regression in fish 2.6.0 involving universal variables with side-effects at startup such as `set -U fish_escape_delay_ms 10` has been fixed (#4196).
- Added completions for:
  - `as` (#4130)
  - `cdh` (#2847)
  - `dhcpd` (#4115)
  - `ezjail-admin` (#4324)
  - Fabric's `fab` (#4153)
  - `grub-file` (#4119)
  - `grub-install` (#4119)
  - `jest` (#4142)
  - `kdeconnect-cli`
  - `magneto` (#4043, #4108)
  - `mdadm` (#4198)
  - `passwd` (#4209)
  - `pip` and `pipenv` (#4448)
  - `s3cmd` (#4332)
  - `sbt` (#4347)
  - `snap` (#4215)
  - Sublime Text 3's `subl` (#4277)
- Lots of improvements to completions.
- Updated Chinese and French translations.

- Improved completions for:
  - `apt`
  - `cd` (#4061)
  - `composer` (#4295)
  - `eopkg`
  - `flatpak` (#4456)
  - `git` (#4117, #4147, #4329, #4368)
  - `gphoto2`
  - `killall` (#4052)
  - `ln`
  - `npm` (#4241)
  - `ssh` (#4377)
  - `tail`
  - `xdg-mime` (#4333)
  - `zypper` (#4325)
---

# fish 2.6.0 (released June 3, 2017)

Since the beta release of fish 2.6b1, fish version 2.6.0 contains a number of minor fixes, new completions for `magneto` (#4043), and improvements to the documentation.

## Known issues

- Apple macOS Sierra 10.12.5 introduced a problem with launching web browsers from other programs using AppleScript. This affects the fish Web configuration (`fish_config`); users on these platforms will need to manually open the address displayed in the terminal, such as by copying and pasting it into a browser. This problem will be fixed with macOS 10.12.6.

If you are upgrading from version 2.5.0 or before, please also review the release notes for 2.6b1 (included below).

---

# fish 2.6b1 (released May 14, 2017)

## Notable fixes and improvements

- Jobs running in the background can now be removed from the list of jobs with the new `disown` builtin, which behaves like the same command in other shells (#2810).
- Command substitutions now have access to the terminal, like in other shells. This allows tools like `fzf` to work properly (#1362, #3922).
- In cases where the operating system does not report the size of the terminal, the `COLUMNS` and `LINES` environment variables are used; if they are unset, a default of 80x24 is assumed.
- New French (#3772 & #3788) and improved German (#3834) translations.
- fish no longer depends on the `which` external command.

## Other significant changes

- Performance improvements in launching processes, including major reductions in signal blocking. Although this has been heavily tested, it may cause problems in some circumstances; set the `FISH_NO_SIGNAL_BLOCK` variable to 0 in your fish configuration file to return to the old behaviour (#2007).
- Performance improvements in prompts and functions that set lots of colours (#3793).
- The Delete key no longer deletes backwards (a regression in 2.5.0).
- `functions` supports a new `--details` option, which identifies where the function was loaded from (#3295), and a `--details --verbose` option which includes the function description (#597).
- `read` will read up to 10 MiB by default, leaving the target variable empty and exiting with status 122 if the line is too long. You can set a different limit with the `FISH_READ_BYTE_LIMIT` variable.
- `read` supports a new `--silent` option to hide the characters typed (#838), for when reading sensitive data from the terminal. `read` also now accepts simple strings for the prompt (rather than scripts) with the new `-P` and `--prompt-str` options (#802).
- `export` and `setenv` now understand colon-separated `PATH`, `CDPATH` and `MANPATH` variables.
- `setenv` is no longer a simple alias for `set -gx` and will complain, just like the csh version, if given more than one value (#4103).
- `bind` supports a new `--list-modes` option (#3872).
- `bg` will check all of its arguments before backgrounding any jobs; any invalid arguments will cause a failure, but non-existent (eg recently exited) jobs are ignored (#3909).
- `funced` warns if the function being edited has not been modified (#3961).
- `printf` correctly outputs "long long" integers (#3352).
- `status` supports a new `current-function` subcommand to print the current function name (#1743).
- `string` supports a new `repeat` subcommand (#3864). `string match` supports a new `--entire` option to emit the entire line matched by a pattern (#3957). `string replace` supports a new `--filter` option to only emit lines which underwent a replacement (#3348).
- `test` supports the `-k` option to test for sticky bits (#733).
- `umask` understands symbolic modes (#738).
- Empty components in the `CDPATH`, `MANPATH` and `PATH` variables are now converted to "." (#2106, #3914).
- New versions of ncurses (6.0 and up) wipe terminal scrollback buffers with certain commands; the `C-l` binding tries to avoid this (#2855).
- Some systems' `su` implementations do not set the `USER` environment variable; it is now reset for root users (#3916).
- Under terminals which support it, bracketed paste is enabled, escaping problematic characters for security and convience (#3871). Inside single quotes (`'`), single quotes and backslashes in pasted text are escaped (#967). The `fish_clipboard_paste` function (bound to `C-v` by default) is still the recommended pasting method where possible as it includes this functionality and more.
- Processes in pipelines are no longer signalled as soon as one command in the pipeline has completed (#1926). This behaviour matches other shells mre closely.
- All functions requiring Python work with whichever version of Python is installed (#3970). Python 3 is preferred, but Python 2.6 remains the minimum version required.
- The color of the cancellation character can be controlled by the `fish_color_cancel` variable (#3963).
- Added completions for:
 - `caddy` (#4008)
 - `castnow` (#3744)
 - `climate` (#3760)
 - `flatpak`
 - `gradle` (#3859)
 - `gsettings` (#4001)
 - `helm` (#3829)
 - `i3-msg` (#3787)
 - `ipset` (#3924)
 - `jq` (#3804)
 - `light` (#3752)
 - `minikube` (#3778)
 - `mocha` (#3828)
 - `mkdosfs` (#4017)
 - `pv` (#3773)
 - `setsid` (#3791)
 - `terraform` (#3960)
 - `usermod` (#3775)
 - `xinput`
 - `yarn` (#3816)
- Improved completions for `adb` (#3853), `apt` (#3771), `bzr` (#3769), `dconf`, `git` (including #3743), `grep` (#3789), `go` (#3789), `help` (#3789), `hg` (#3975), `htop` (#3789), `killall` (#3996), `lua`, `man` (#3762), `mount` (#3764 & #3841), `obnam` (#3924), `perl` (#3856), `portmaster` (#3950), `python` (#3840), `ssh` (#3781), `scp` (#3781), `systemctl` (#3757) and `udisks` (#3764).

---

# fish 2.5.0 (released February 3, 2017)

There are no major changes between 2.5b1 and 2.5.0. If you are upgrading from version 2.4.0 or before, please also review the release notes for 2.5b1 (included below).

## Notable fixes and improvements

- The Home, End, Insert, Delete, Page Up and Page Down keys work in Vi-style key bindings (#3731).

---

# fish 2.5b1 (released January 14, 2017)

## Platform Changes

Starting with version 2.5, fish requires a more up-to-date version of C++, specifically C++11 (from 2011). This affects some older platforms:

### Linux

For users building from source, GCC's g++ 4.8 or later, or LLVM's clang 3.3 or later, are known to work. Older platforms may require a newer compiler installed.

Unfortunately, because of the complexity of the toolchain, binary packages are no longer published by the fish-shell developers for the following platforms:

 - Red Hat Enterprise Linux and CentOS 5 & 6 for 64-bit builds
 - Ubuntu 12.04 (EoLTS April 2017)
 - Debian 7 (EoLTS May 2018)

Installing newer version of fish on these systems will require building from source.

### OS X SnowLeopard

Starting with version 2.5, fish requires a C++11 standard library on OS X 10.6 ("SnowLeopard"). If this library is not installed, you will see this error: `dyld: Library not loaded: /usr/lib/libc++.1.dylib`

MacPorts is the easiest way to obtain this library. After installing the SnowLeopard MacPorts release from the install page, run:

```
sudo port -v install libcxx
```

Now fish should launch successfully. (Please open an issue if it does not.)

This is only necessary on 10.6. OS X 10.7 and later include the required library by default.

## Other significant changes

- Attempting to exit with running processes in the background produces a warning, then signals them to terminate if a second attempt to exit is made. This brings the behaviour for running background processes into line with stopped processes. (#3497)
- `random` can now have start, stop and step values specified, or the new `choice` subcommand can be used to pick an argument from a list (#3619).
- A new key bindings preset, `fish_hybrid_key_bindings`, including all the Emacs-style and Vi-style bindings, which behaves like `fish_vi_key_bindings` in fish 2.3.0 (#3556).
- `function` now returns an error when called with invalid options, rather than defining the function anyway (#3574). This was a regression present in fish 2.3 and 2.4.0.
- fish no longer prints a warning when it identifies a running instance of an old version (2.1.0 and earlier). Changes to universal variables may not propagate between these old versions and 2.5b1.
- Improved compatiblity with Android (#3585), MSYS/mingw (#2360), and Solaris (#3456, #3340).
- Like other shells, the `test` builting now returns an error for numeric operations on invalid integers (#3346, #3581).
- `complete` no longer recognises `--authoritative` and `--unauthoritative` options, and they are marked as obsolete.
- `status` accepts subcommands, and should be used like `status is-interactive`. The old options continue to be supported for the foreseeable future (#3526), although only one subcommand or option can be specified at a time.
- Selection mode (used with "begin-selection") no longer selects a character the cursor does not move over (#3684).
- List indexes are handled better, and a bit more liberally in some cases (`echo $PATH[1 .. 3]` is now valid) (#3579).
- The `fish_mode_prompt` function is now simply a stub around `fish_default_mode_prompt`, which allows the mode prompt to be included more easily in customised prompt functions (#3641).

## Notable fixes and improvements
- `alias`, run without options or arguments, lists all defined aliases, and aliases now include a description in the function signature that identifies them.
- `complete` accepts empty strings as descriptions (#3557).
- `command` accepts `-q`/`--quiet` in combination with `--search` (#3591), providing a simple way of checking whether a command exists in scripts.
- Abbreviations can now be renamed with `abbr --rename OLD_KEY NEW_KEY` (#3610).
- The command synopses printed by `--help` options work better with copying and pasting (#2673).
- `help` launches the browser specified by the `$fish_help_browser variable` if it is set (#3131).
- History merging could lose items under certain circumstances and is now fixed (#3496).
- The `$status` variable is now set to 123 when a syntactically invalid command is entered (#3616).
- Exiting fish now signals all background processes to terminate, not just stopped jobs (#3497).
- A new `prompt_hostname` function which prints a hostname suitable for use in prompts (#3482).
- The `__fish_man_page` function (bound to Alt-h by default) now tries to recognize subcommands (e.g. `git add` will now open the "git-add" man page) (#3678).
- A new function `edit_command_buffer` (bound to Alt-e & Alt-v by default) to edit the command buffer in an external editor (#1215, #3627).
- `set_color` now supports italics (`--italics`), dim (`--dim`) and reverse (`--reverse`) modes (#3650).
- Filesystems with very slow locking (eg incorrectly-configured NFS) will no longer slow fish down (#685).
- Improved completions for `apt` (#3695), `fusermount` (#3642), `make` (#3628), `netctl-auto` (#3378), `nmcli` (#3648), `pygmentize` (#3378), and `tar` (#3719).
- Added completions for:
 - `VBoxHeadless` (#3378)
 - `VBoxSDL` (#3378)
 - `base64` (#3378)
 - `caffeinate` (#3524)
 - `dconf` (#3638)
 - `dig` (#3495)
 - `dpkg-reconfigure` (#3521 & #3522)
 - `feh` (#3378)
 - `launchctl` (#3682)
 - `lxc` (#3554 & #3564),
 - `mddiagnose` (#3524)
 - `mdfind` (#3524)
 - `mdimport`  (#3524)
 - `mdls` (#3524)
 - `mdutil` (#3524)
 - `mkvextract` (#3492)
 - `nvram` (#3524)
 - `objdump` (#3378)
 - `sysbench` (#3491)
 - `tmutil` (#3524)

---

# fish 2.4.0 (released November 8, 2016)

There are no major changes between 2.4b1 and 2.4.0.

## Notable fixes and improvements
- The documentation is now generated properly and with the correct version identifier.
- Automatic cursor changes are now only enabled on the subset of XTerm versions known to support them, resolving a problem where older versions printed garbage to the terminal before and after every prompt (#3499).
- Improved the title set in Apple Terminal.app.
- Added completions for `defaults` and improved completions for `diskutil` (#3478).

---

# fish 2.4b1 (released October 18, 2016)

## Significant changes
- The clipboard integration has been revamped with explicit bindings. The killring commands no longer copy from, or paste to, the X11 clipboard - use the new copy (`C-x`) and paste (`C-v`) bindings instead. The clipboard is now available on OS X as well as systems using X11 (e.g. Linux). (#3061)
- `history` uses subcommands (`history delete`) rather than options (`history --delete`) for its actions (#3367). You can no longer specify multiple actions via flags (e.g., `history --delete --save something`).
- New `history` options have been added, including `--max=n` to limit the number of history entries, `--show-time` option to show timestamps (#3175, #3244), and `--null` to null terminate history entries in the search output.
- `history search` is now case-insensitive by default (which also affects `history delete`) (#3236).
- `history delete` now correctly handles multiline commands (#31).
- Vi-style bindings no longer include all of the default emacs-style bindings; instead, they share some definitions (#3068).
- If there is no locale set in the environment, various known system configuration files will be checked for a default. If no locale can be found, `en_US-UTF.8` will be used (#277).
- A number followed by a caret (e.g. `5^`) is no longer treated as a redirection (#1873).
- The `$version` special variable can be overwritten, so that it can be used for other purposes if required.

## Notable fixes and improvements
- The `fish_realpath` builtin has been renamed to `realpath` and made compatible with GNU `realpath` when run without arguments (#3400). It is used only for systems without a `realpath` or `grealpath` utility (#3374).
- Improved color handling on terminals/consoles with 8-16 colors, particularly the use of bright named color (#3176, #3260).
- `fish_indent` can now read from files given as arguments, rather than just standard input (#3037).
- Fuzzy tab completions behave in a less surprising manner (#3090, #3211).
- `jobs` should only print its header line once (#3127).
- Wildcards in redirections are highlighted appropriately (#2789).
- Suggestions will be offered more often, like after removing characters (#3069).
- `history --merge` now correctly interleaves items in chronological order (#2312).
- Options for `fish_indent` have been aligned with the other binaries - in particular, `-d` now means `--debug`. The `--dump` option has been renamed to `--dump-parse-tree` (#3191).
- The display of bindings in the Web-based configuration has been greatly improved (#3325), as has the rendering of prompts (#2924).
- fish should no longer hang using 100% CPU in the C locale (#3214).
- A bug in FreeBSD 11 & 12, Dragonfly BSD & illumos prevented fish from working correctly on these platforms under UTF-8 locales; fish now avoids the buggy behaviour (#3050).
- Prompts which show git repository information (via `__fish_git_prompt`) are faster in large repositories (#3294) and slow filesystems (#3083).
- fish 2.3.0 reintroduced a problem where the greeting was printed even when using `read`; this has been corrected again (#3261).
- Vi mode changes the cursor depending on the current mode (#3215).
- Command lines with escaped space characters at the end tab-complete correctly (#2447).
- Added completions for:
  - `arcanist` (#3256)
  - `connmanctl` (#3419)
  - `figlet` (#3378)
  - `mdbook` (#3378)
  -  `ninja` (#3415)
  -  `p4`, the Perforce client (#3314)
  -  `pygmentize` (#3378)
  -  `ranger` (#3378)
- Improved completions for `aura` (#3297), `abbr` (#3267), `brew` (#3309), `chown` (#3380, #3383),`cygport` (#3392), `git` (#3274, #3226, #3225, #3094, #3087, #3035, #3021, #2982, #3230), `kill` & `pkill` (#3200), `screen` (#3271), `wget` (#3470), and `xz` (#3378).
- Distributors, packagers and developers will notice that the build process produces more succinct output by default; use `make V=1` to get verbose output (#3248).
- Improved compatibility with minor platforms including musl (#2988), Cygwin (#2993), Android (#3441, #3442), Haiku (#3322) and Solaris .

---

# fish 2.3.1 (released July 3, 2016)

This is a functionality and bugfix release. This release does not contain all the changes to fish since the last release, but fixes a number of issues directly affecting users at present and includes a small number of new features.

## Significant changes
- A new `fish_key_reader` binary for decoding interactive keypresses (#2991).
- `fish_mode_prompt` has been updated to reflect the changes in the way the Vi input mode is set up (#3067), making this more reliable.
- `fish_config` can now properly be launched from the OS X app bundle (#3140).

## Notable fixes and improvements

- Extra lines were sometimes inserted into the output under Windows (Cygwin and Microsoft Windows Subsystem for Linux) due to TTY timestamps not being updated (#2859).
- The `string` builtin's `match` mode now handles the combination of `-rnv` (match, invert and count) correctly (#3098).
- Improvements to TTY special character handling (#3064), locale handling (#3124) and terminal environment variable handling (#3060).
- Work towards handling the terminal modes for external commands launched from initialisation files (#2980).
- Ease the upgrade path from fish 2.2.0 and before by warning users to restart fish if the `string` builtin is not available (#3057).
- `type -a` now syntax-colorizes function source output.
- Added completions for `alsamixer`, `godoc`, `gofmt`, `goimports`, `gorename`, `lscpu`, `mkdir`, `modinfo`, `netctl-auto`, `poweroff`, `termite`, `udisksctl` and `xz` (#3123).
- Improved completions for `apt` (#3097), `aura` (#3102),`git` (#3114), `npm` (#3158), `string` and `suspend` (#3154).

---

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
