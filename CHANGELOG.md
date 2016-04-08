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
