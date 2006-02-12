#
# Completions for the svn VCS command
#

function __fish_svn_subcommand_allowed
	set -l svncommands add blame cat checkout cleanup commit copy delete diff di help import info list lock log merge mkdir move propdel propedit propget proplist resolved revert status switch unlock update
	set -l -- cmd (commandline -poc)
	set -e cmd[1]
	for i in $cmd
		if contains -- $i $svncommands
			return 1
		end
	end
	return 0
end

#
# If no subcommand has been specified, complete using all available subcommands
#

complete -c svn -n '__fish_svn_subcommand_allowed' -xa 'add\t"'(_ "Place files or directories under version control")'"'
complete -c svn -n '__fish_svn_subcommand_allowed' -xa 'blame\t"'(_ "Output files/URLs with revision and author information inline")'"'
complete -c svn -n '__fish_svn_subcommand_allowed' -xa 'cat\t"'(_ "Output content of files/URLs")'"'
complete -c svn -n '__fish_svn_subcommand_allowed' -xa 'checkout\t"'(_ "Check out a working copy from the repository")'"'
complete -c svn -n '__fish_svn_subcommand_allowed' -xa 'cleanup\t"'(_ "Recursively clean up the working copy")'"'
complete -c svn -n '__fish_svn_subcommand_allowed' -xa 'commit\t"'(_ "Send changes from your working copy to the repository")'"'
complete -c svn -n '__fish_svn_subcommand_allowed' -xa 'import\t"'(_ "Commit an unversioned file or tree into the repository")'"'
complete -c svn -n '__fish_svn_subcommand_allowed' -xa 'info\t"'(_ "Display information about a local or remote item")'"'
complete -c svn -n '__fish_svn_subcommand_allowed' -xa 'list\t"'(_ "List directory entries in the repository")'"'
complete -c svn -n '__fish_svn_subcommand_allowed' -xa 'lock\t"'(_ "Lock working copy paths or URLs in the repository")'"'
complete -c svn -n '__fish_svn_subcommand_allowed' -xa 'log\t"'(_ "Show the log messages for a set of revision(s) and/or file(s)")'"'
complete -c svn -n '__fish_svn_subcommand_allowed' -xa 'merge\t"'(_ "Apply the differences between two sources to a working copy path")'"'
complete -c svn -n '__fish_svn_subcommand_allowed' -xa 'mkdir\t"'(_ "Create a new directory under version control")'"'
complete -c svn -n '__fish_svn_subcommand_allowed' -xa 'move\t"'(_ "Move and/or rename something in working copy or repository")'"'
complete -c svn -n '__fish_svn_subcommand_allowed' -xa 'propdel\t"'(_ "Remove a property from files, dirs, or revisions")'"'
complete -c svn -n '__fish_svn_subcommand_allowed' -xa 'propedit\t"'(_ "Edit a property with an external editor on targets")'"'
complete -c svn -n '__fish_svn_subcommand_allowed' -xa 'propget\t"'(_ "Print value of a property on files, dirs, or revisions")'"'
complete -c svn -n '__fish_svn_subcommand_allowed' -xa 'proplist\t"'(_ "List all properties on files, dirs, or revisions")'"'
complete -c svn -n '__fish_svn_subcommand_allowed' -xa 'resolved\t"'(_ "Remove conflicted state on working copy files or directories")'"'
complete -c svn -n '__fish_svn_subcommand_allowed' -xa 'revert\t"'(_ "Restore pristine working copy file")'"'
complete -c svn -n '__fish_svn_subcommand_allowed' -xa 'status\t"'(_ "Print the status of working copy files and directories")'"'
complete -c svn -n '__fish_svn_subcommand_allowed' -xa 'switch\t"'(_ "Update the working copy to a different URL")'"'
complete -c svn -n '__fish_svn_subcommand_allowed' -xa 'unlock\t"'(_ "Unlock working copy paths or URLs")'"'
complete -c svn -n '__fish_svn_subcommand_allowed' -xa 'update\t"'(_ "Bring changes from the repository into the working copy")'"'
complete -c svn -n '__fish_svn_subcommand_allowed' -xa 'help\t"'(_ "Describe the usage of this program or its subcommands")'"'

# -s/--revision
complete -c svn -n '__fish_seen_subcommand_from blame cat checkout info list log merge move propdel propedit propget proplist switch update' -x -s r -l revision -d (_ "Specify revision")
# --targets
complete -c svn -n '__fish_seen_subcommand_from add commit import info lock log resolved revert unlock' -r -l targets -d (_ "Pass contents of file as additional args")
# -N/--non-recursive
complete -c svn -n '__fish_seen_subcommand_from add checkout commit import merge status switch update' -s N -l non-recursive -d (_ "Don't recurse")
# -q/--quiet
complete -c svn -n '__fish_seen_subcommand_from add checkout commit import log merge mkdir move propdel proplist resolved revert status switch update help' -s q -l quiet -d (_ "Print as little as possible")
# --force
complete -c svn -n '__fish_seen_subcommand_from add lock merge move propedit unlock' -l force -d (_ "Force operation to run")
# --auto-props
complete -c svn -n '__fish_seen_subcommand_from add' -l auto-props -d (_ "Enable automatic properties")
# --no-auto-props
complete -c svn -n '__fish_seen_subcommand_from add' -l no-auto-props -d (_ "Disable automatic properties")
# -v/--verbose
complete -c svn -n '__fish_seen_subcommand_from blame cat log proplist status' -s v -l verbose -d (_ "Print extra info")
# --username
complete -c svn -n '__fish_seen_subcommand_from blame cat checkout commit import info list lock log merge mkdir move propdel propedit propget proplist status switch unlock update' -x -l username -d (_ "Specify a username")
# --password
complete -c svn -n '__fish_seen_subcommand_from blame cat checkout commit import info list lock log merge mkdir move propdel propedit propget proplist status switch unlock update' -x -l password -d (_ "Specify a password")
# --no-auth-cache
complete -c svn -n '__fish_seen_subcommand_from blame cat checkout commit import info list lock log merge mkdir move propdel propedit propget proplist status switch unlock update' -l no-auth-cache -d (_ "Don't cache auth tokens")
# --non-interactive
complete -c svn -n '__fish_seen_subcommand_from blame cat checkout commit import info list lock log merge mkdir move propdel propedit propget proplist status switch unlock update' -l non-interactive -d (_ "Do no interactive prompting")
# --config-dir
complete -c svn -n '__fish_seen_subcommand_from add cleanup blame cat checkout commit import info list lock log merge mkdir move propdel propedit propget proplist resolved revert status switch unlock update help' -r -l config-dir -d (_ "Read user config files from named directory")
# --no-unlock
complete -c svn -n '__fish_seen_subcommand_from commit import' -l no-unlock -d (_ "Don't unlock targets")
# -m/--message
complete -c svn -n '__fish_seen_subcommand_from commit import lock mkdir move' -x -s m -l message -d (_ "Specify commit message")
# -F/--file
complete -c svn -n '__fish_seen_subcommand_from commit import lock mkdir move' -r -s F -l file -d (_ "Read commit message from file")
# --force-log
complete -c svn -n '__fish_seen_subcommand_from commit import lock mkdir move' -l force-log -d (_ "Force log message source validity")
# --editor-cmd
complete -c svn -n '__fish_seen_subcommand_from commit import mkdir move propedit' -x -l editor-cmd -d (_ "Specify external editor")
# --recursive
complete -c svn -n '__fish_seen_subcommand_from info list propdel propget proplist resolved revert' -s R -l recursive -d (_ "Descend recursively")
# --incremental
complete -c svn -n '__fish_seen_subcommand_from list log' -l incremental -d (_ "Give output suitable for concatenation")
# --xml
complete -c svn -n '__fish_seen_subcommand_from list log' -l xml -d (_ "Output in XML")
# --diff3-cmd
complete -c svn -n '__fish_seen_subcommand_from cleanup merge switch update' -x -l diff3-cmd -d (_ "Specify merge command")
# --encoding
complete -c svn -n '__fish_seen_subcommand_from lock mkdir move propedit' -x -l encoding -d (_ "Force encoding")
# --revprop
complete -c svn -n '__fish_seen_subcommand_from propdel propedit propget' -l revprop -d (_ "Operate on revision property")
# --strict
complete -c svn -n '__fish_seen_subcommand_from propget' -l strict -d (_ "Use strict semantics")
# --ignore-externals
complete -c svn -n '__fish_seen_subcommand_from checkout status update' -l ignore-externals -d (_ "Ignore externals definitions")

#
# Flags for svn log
#
set -l logopt -c svn -n 'contains log (commandline -poc)'
complete $logopt -l stop-on-copy -d (_ "Do not cross copies")
complete $logopt -l limit -d (_ "Maximum number of log entries")

#
# Flags for svn merge
#
set -l mergeopt -c svn -n 'contains merge (commandline -poc)'
complete $mergeopt -l dry-run -d (_ "Make no changes")
complete $mergeopt -l ignore-ancestry -d (_ "Ignore ancestry when calculating merge")

#
# Flags for svn status
#
set -l statusopt -c svn -n 'contains status (commandline -poc)'
complete $statusopt -s u -l show-updates -d (_ "Display update information")
complete $statusopt -l no-ignore -d (_ "Disregard ignores")

#
# Flags for svn switch
#
set -l switchopt -c svn -n 'contains switch (commandline -poc)'
complete $switchopt -l relocate -d (_ "Relocate VIA URL-rewriting")

#
# Flags for svn help
#
set -l helpopt -c svn -n 'contains help (commandline -poc)'
complete $helpopt -l version -d (_ "Print client version info")
