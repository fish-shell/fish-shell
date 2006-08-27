#
# Completions for the svn VCS command
#

#
# If no subcommand has been specified, complete using all available subcommands
#

complete -c svn -n '__fish_use_subcommand' -xa 'add\t"'(_ "Place files or directories under version control")'"'
complete -c svn -n '__fish_use_subcommand' -xa 'blame\t"'(_ "Output files/URLs with revision and author information inline")'"'
complete -c svn -n '__fish_use_subcommand' -xa 'cat\t"'(_ "Output content of files/URLs")'"'
complete -c svn -n '__fish_use_subcommand' -xa 'checkout\t"'(_ "Check out a working copy from the repository")'"'
complete -c svn -n '__fish_use_subcommand' -xa 'cleanup\t"'(_ "Recursively clean up the working copy")'"'
complete -c svn -n '__fish_use_subcommand' -xa 'commit\t"'(_ "Send changes from your working copy to the repository")'"'
complete -c svn -n '__fish_use_subcommand' -xa 'import\t"'(_ "Commit an unversioned file or tree into the repository")'"'
complete -c svn -n '__fish_use_subcommand' -xa 'info\t"'(_ "Display information about a local or remote item")'"'
complete -c svn -n '__fish_use_subcommand' -xa 'list\t"'(_ "List directory entries in the repository")'"'
complete -c svn -n '__fish_use_subcommand' -xa 'lock\t"'(_ "Lock working copy paths or URLs in the repository")'"'
complete -c svn -n '__fish_use_subcommand' -xa 'log\t"'(_ "Show the log messages for a set of revision(s) and/or file(s)")'"'
complete -c svn -n '__fish_use_subcommand' -xa 'merge\t"'(_ "Apply the differences between two sources to a working copy path")'"'
complete -c svn -n '__fish_use_subcommand' -xa 'mkdir\t"'(_ "Create a new directory under version control")'"'
complete -c svn -n '__fish_use_subcommand' -xa 'move\t"'(_ "Move and/or rename something in working copy or repository")'"'
complete -c svn -n '__fish_use_subcommand' -xa 'propdel\t"'(_ "Remove a property from files, dirs, or revisions")'"'
complete -c svn -n '__fish_use_subcommand' -xa 'propedit\t"'(_ "Edit a property with an external editor on targets")'"'
complete -c svn -n '__fish_use_subcommand' -xa 'propget\t"'(_ "Print value of a property on files, dirs, or revisions")'"'
complete -c svn -n '__fish_use_subcommand' -xa 'proplist\t"'(_ "List all properties on files, dirs, or revisions")'"'
complete -c svn -n '__fish_use_subcommand' -xa 'resolved\t"'(_ "Remove conflicted state on working copy files or directories")'"'
complete -c svn -n '__fish_use_subcommand' -xa 'revert\t"'(_ "Restore pristine working copy file")'"'
complete -c svn -n '__fish_use_subcommand' -xa 'status\t"'(_ "Print the status of working copy files and directories")'"'
complete -c svn -n '__fish_use_subcommand' -xa 'switch\t"'(_ "Update the working copy to a different URL")'"'
complete -c svn -n '__fish_use_subcommand' -xa 'unlock\t"'(_ "Unlock working copy paths or URLs")'"'
complete -c svn -n '__fish_use_subcommand' -xa 'update\t"'(_ "Bring changes from the repository into the working copy")'"'
complete -c svn -n '__fish_use_subcommand' -xa 'help\t"'(_ "Describe the usage of this program or its subcommands")'"'

# -s/--revision
complete -c svn -n '__fish_seen_subcommand_from blame cat checkout info list log merge move propdel propedit propget proplist switch update' -x -s r -l revision -d (N_ "Specify revision")
# --targets
complete -c svn -n '__fish_seen_subcommand_from add commit import info lock log resolved revert unlock' -r -l targets -d (N_ "Pass contents of file as additional args")
# -N/--non-recursive
complete -c svn -n '__fish_seen_subcommand_from add checkout commit import merge status switch update' -s N -l non-recursive -d (N_ "Don't recurse")
# -q/--quiet
complete -c svn -n '__fish_seen_subcommand_from add checkout commit import log merge mkdir move propdel proplist resolved revert status switch update help' -s q -l quiet -d (N_ "Print as little as possible")
# --force
complete -c svn -n '__fish_seen_subcommand_from add lock merge move propedit unlock' -l force -d (N_ "Force operation to run")
# --auto-props
complete -c svn -n '__fish_seen_subcommand_from add' -l auto-props -d (N_ "Enable automatic properties")
# --no-auto-props
complete -c svn -n '__fish_seen_subcommand_from add' -l no-auto-props -d (N_ "Disable automatic properties")
# -v/--verbose
complete -c svn -n '__fish_seen_subcommand_from blame cat log proplist status' -s v -l verbose -d (N_ "Print extra info")
# --username
complete -c svn -n '__fish_seen_subcommand_from blame cat checkout commit import info list lock log merge mkdir move propdel propedit propget proplist status switch unlock update' -x -l username -d (N_ "Specify a username")
# --password
complete -c svn -n '__fish_seen_subcommand_from blame cat checkout commit import info list lock log merge mkdir move propdel propedit propget proplist status switch unlock update' -x -l password -d (N_ "Specify a password")
# --no-auth-cache
complete -c svn -n '__fish_seen_subcommand_from blame cat checkout commit import info list lock log merge mkdir move propdel propedit propget proplist status switch unlock update' -l no-auth-cache -d (N_ "Don't cache auth tokens")
# --non-interactive
complete -c svn -n '__fish_seen_subcommand_from blame cat checkout commit import info list lock log merge mkdir move propdel propedit propget proplist status switch unlock update' -l non-interactive -d (N_ "Do no interactive prompting")
# --config-dir
complete -c svn -n '__fish_seen_subcommand_from add cleanup blame cat checkout commit import info list lock log merge mkdir move propdel propedit propget proplist resolved revert status switch unlock update help' -r -l config-dir -d (N_ "Read user config files from named directory")
# --no-unlock
complete -c svn -n '__fish_seen_subcommand_from commit import' -l no-unlock -d (N_ "Don't unlock targets")
# -m/--message
complete -c svn -n '__fish_seen_subcommand_from commit import lock mkdir move' -x -s m -l message -d (N_ "Specify commit message")
# -F/--file
complete -c svn -n '__fish_seen_subcommand_from commit import lock mkdir move' -r -s F -l file -d (N_ "Read commit message from file")
# --force-log
complete -c svn -n '__fish_seen_subcommand_from commit import lock mkdir move' -l force-log -d (N_ "Force log message source validity")
# --editor-cmd
complete -c svn -n '__fish_seen_subcommand_from commit import mkdir move propedit' -x -l editor-cmd -d (N_ "Specify external editor")
# --recursive
complete -c svn -n '__fish_seen_subcommand_from info list propdel propget proplist resolved revert' -s R -l recursive -d (N_ "Descend recursively")
# --incremental
complete -c svn -n '__fish_seen_subcommand_from list log' -l incremental -d (N_ "Give output suitable for concatenation")
# --xml
complete -c svn -n '__fish_seen_subcommand_from list log' -l xml -d (N_ "Output in XML")
# --diff3-cmd
complete -c svn -n '__fish_seen_subcommand_from cleanup merge switch update' -x -l diff3-cmd -d (N_ "Specify merge command")
# --encoding
complete -c svn -n '__fish_seen_subcommand_from lock mkdir move propedit' -x -l encoding -d (N_ "Force encoding")
# --revprop
complete -c svn -n '__fish_seen_subcommand_from propdel propedit propget' -l revprop -d (N_ "Operate on revision property")
# --strict
complete -c svn -n '__fish_seen_subcommand_from propget' -l strict -d (N_ "Use strict semantics")
# --ignore-externals
complete -c svn -n '__fish_seen_subcommand_from checkout status update' -l ignore-externals -d (N_ "Ignore externals definitions")

#
# Flags for svn log
#
set -l logopt -c svn -n 'contains log (commandline -poc)'
complete $logopt -l stop-on-copy -d (N_ "Do not cross copies")
complete $logopt -l limit -d (N_ "Maximum number of log entries")

#
# Flags for svn merge
#
set -l mergeopt -c svn -n 'contains merge (commandline -poc)'
complete $mergeopt -l dry-run -d (N_ "Make no changes")
complete $mergeopt -l ignore-ancestry -d (N_ "Ignore ancestry when calculating merge")

#
# Flags for svn status
#
set -l statusopt -c svn -n 'contains status (commandline -poc)'
complete $statusopt -s u -l show-updates -d (N_ "Display update information")
complete $statusopt -l no-ignore -d (N_ "Disregard ignores")

#
# Flags for svn switch
#
set -l switchopt -c svn -n 'contains switch (commandline -poc)'
complete $switchopt -l relocate -d (N_ "Relocate VIA URL-rewriting")

#
# Flags for svn help
#
set -l helpopt -c svn -n 'contains help (commandline -poc)'
complete $helpopt -l version -d (N_ "Print client version info")
