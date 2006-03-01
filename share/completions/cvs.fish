#
# Completions for the cvs VCS command
#
# Incomplete, the number of switches for cvs is _huge_
#

function __fish_no_cvs_subcommand
	set -l cvscommands add admin annotate checkout commit diff edit editors export history import init kserver log login logout pserver rannotate rdiff release remove rlog rtag server status tag unedit update version watch watchers
	set -l -- cmd (commandline -poc)
	set -e cmd[1]
	for i in $cmd
		if contains -- $i $cvscommands
			return 1
		end
	end
	return 0
end

#
# If no subcommand has been specified, complete using all available subcommands
#

complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'add\t"'(_ "Add a new file/directory to the repository")'"'
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'admin\t"'(_ "Administration front end for rcs")'"'
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'annotate\t"'(_ "Show last revision where each line was modified")'"'
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'checkout\t"'(_ "Checkout sources for editing")'"'
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'commit\t"'(_ "Check files into the repository")'"'
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'diff\t"'(_ "Show differences between revisions")'"'
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'edit\t"'(_ "Get ready to edit a watched file")'"'
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'editors\t"'(_ "See who is editing a watched file")'"'
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'export\t"'(_ "Export sources from CVS, similar to checkout")'"'
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'history\t"'(_ "Show repository access history")'"'
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'import\t"'(_ "Import sources into CVS, using vendor branches")'"'
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'init\t"'(_ "Create a CVS repository if it doesnt exist")'"'
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'kserver\t"'(_ "Kerberos server mode")'"'
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'log\t"'(_ "Print out history information for files")'"'
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'login\t"'(_ "Prompt for password for authenticating server")'"'
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'logout\t"'(_ "Removes entry in .cvspass for remote repository")'"'
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'pserver\t"'(_ "Password server mode")'"'
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'rannotate\t"'(_ "Show last revision where each line of module was modified")'"'
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'rdiff\t"'(_ "Create "patch" format diffs between releases")'"'
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'release\t"'(_ "Indicate that a Module is no longer in use")'"'
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'remove\t"'(_ "Remove an entry from the repository")'"'
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'rlog\t"'(_ "Print out history information for a module")'"'
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'rtag\t"'(_ "Add a symbolic tag to a module")'"'
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'server\t"'(_ "Server mode")'"'
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'status\t"'(_ "Display status information on checked out files")'"'
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'tag\t"'(_ "Add a symbolic tag to checked out version of files")'"'
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'unedit\t"'(_ "Undo an edit command")'"'
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'update\t"'(_ "Bring work tree in sync with repository")'"'
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'version\t"'(_ "Display version and exit")'"'
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'watch\t"'(_ "Set watches")'"'
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'watchers\t"'(_ "See who is watching a file")'"'

#
# cvs_options switches, which must be specified before a command.
# 

complete -c cvs -n '__fish_no_cvs_subcommand' -x -l allow-root -d (N_ "Specify legal cvsroot directory." )
complete -c cvs -n '__fish_no_cvs_subcommand' -s a -d (N_ "Authenticate all net traffic")
complete -c cvs -n '__fish_no_cvs_subcommand' -r -s T -d (N_ "Use "tmpdir" for temporary files")
complete -c cvs -n '__fish_no_cvs_subcommand' -x -s d -d (N_ "Overrides $CVSROOT as the root of the CVS tree")
complete -c cvs -n '__fish_no_cvs_subcommand' -x -s e -d (N_ "Use "editor" for editing log information")
complete -c cvs -n '__fish_no_cvs_subcommand' -s f -d (N_ "Do not use the ~/.cvsrc file")
complete -c cvs -n '__fish_no_cvs_subcommand' -s H -d (N_ "Displays usage information for command")
complete -c cvs -n '__fish_no_cvs_subcommand' -s n -d (N_ "Do not change any files")
complete -c cvs -n '__fish_no_cvs_subcommand' -s Q -d (N_ "Cause CVS to be really quiet")
complete -c cvs -n '__fish_no_cvs_subcommand' -s R -d (N_ "Read-only repository mode")
complete -c cvs -n '__fish_no_cvs_subcommand' -s q -d (N_ "Cause CVS to be somewhat quiet")
complete -c cvs -n '__fish_no_cvs_subcommand' -s r -d (N_ "Make checked-out files read-only")
complete -c cvs -n '__fish_no_cvs_subcommand' -x -s s -d (N_ "Set CVS user variable")
complete -c cvs -n '__fish_no_cvs_subcommand' -s t -d (N_ "Show trace of program execution -- try with -n")
complete -c cvs -n '__fish_no_cvs_subcommand' -x -s v -d (N_ "Display version and exit")
complete -c cvs -n '__fish_no_cvs_subcommand' -s w -d (N_ "Make checked-out files read-write (default)")
complete -c cvs -n '__fish_no_cvs_subcommand' -s x -d (N_ "Encrypt all net traffic")
complete -c cvs -n '__fish_no_cvs_subcommand' -x -s z -d (N_ "Compression level for net traffic") -a '1 2 3 4 5 6 7 8 9'

#
# Universal cvs options, which can be applied to any cvs command.
#

complete -c cvs -n '__fish_seen_subcommand_from checkout diff export history rdiff rtag update' -x -s D -d (N_ "Use the most recent revision no later than date")
complete -c cvs -n '__fish_seen_subcommand_from annotate export rdiff rtag update' -s f -d (N_ "Retrieve files even when no match for tag/date")
complete -c cvs -n '__fish_seen_subcommand_from add checkout diff import update' -x -s k -d (N_ "Alter default keyword processing")
complete -c cvs -n '__fish_seen_subcommand_from annotate checkout commit diff edit editors export log rdiff remove rtag status tag unedit update watch watchers' -s l -d (N_ "Don't recurse")
complete -c cvs -n '__fish_seen_subcommand_from add commit import' -x -s m -d (N_ "Specify log message instead of invoking editor")
complete -c cvs -n '__fish_seen_subcommand_from checkout commit export rtag' -s n -d (N_ "Don't run any tag programs")
complete -c cvs -n 'not __fish_no_cvs_subcommand' -s P -d (N_ "Prune empty directories")
complete -c cvs -n '__fish_seen_subcommand_from checkout update' -s p -d (N_ "Pipe files to stdout")
complete -c cvs -n '__fish_seen_subcommand_from annotate checkout commit diff edit editors export rdiff remove rtag status tag unedit update watch watchers' -s R -d (N_ "Process directories recursively")
complete -c cvs -n '__fish_seen_subcommand_from annotate checkout commit diff history export rdiff rtag update' -x -s r -d (N_ "Use a specified tag")
complete -c cvs -n '__fish_seen_subcommand_from import update' -r -s W -d (N_ "Specify filenames to be filtered")

#
# cvs options for admin. Note that all options marked as "useless", "might not
#  work", "excessively dangerous" or "does nothing" are not included!
#

set -l admin_opt -c cvs -n 'contains admin (commandline -poc)'
complete $admin_opt -x -s k -d (N_ "Set the default keyword substitution")
complete $admin_opt    -s l -d (N_ "Lock a revision")
complete $admin_opt -x -s m -d (N_ "Replace a log message")
complete $admin_opt -x -s n -d (N_ "Force name/rev association")
complete $admin_opt -x -s N -d (N_ "Make a name/rev association")
complete $admin_opt    -s q -d (N_ "Run quietly")
complete $admin_opt -x -s s -d (N_ "Set a state attribute for a revision")
complete $admin_opt    -s t -d (N_ "Write descriptive text from a file into RCS")
complete $admin_opt -x -o t- -d (N_ "Write descriptive text into RCS")
complete $admin_opt -x -s l -d (N_ "Unlock a revision")

#
# cvs options for annotate.
#

set -l annotate_opt -c cvs -n 'contains annotate (commandline -poc)'
complete $annotate_opt -s f -d (N_ "Annotate binary files")

#
# cvs options for checkout.
#

set -l checkout_opt -c cvs -n 'contains checkout (commandline -poc)'
complete $checkout_opt    -s A -d (N_ "Reset sticky tags/dates/k-opts")
complete $checkout_opt    -s c -d (N_ "Copy module file to stdout")
complete $checkout_opt -x -s d -d (N_ "Name directory for working files")
complete $checkout_opt -x -s j -d (N_ "Merge revisions")
complete $checkout_opt    -s N -d (N_ "For -d. Don't shorten paths")

#
# cvs options for commit.
#

set -l commit_opt -c cvs -n 'contains commit (commandline -poc)'
complete $commit_opt -r -s F -d (N_ "Read log message from file")
complete $commit_opt    -s f -d (N_ "Force new revision")

#
# cvs options for diff.
#

set -l diff_opt -c cvs -n 'contains diff (commandline -poc)'
complete $diff_opt    -s a -l text -d (N_ "Treat all files as text")
complete $diff_opt    -s b -l ignore-space-change -d (N_ "Treat all whitespace as one space")
complete $diff_opt    -s B -l ignore-blank-lines -d (N_ "Ignore blank line only changes")
complete $diff_opt    -l binary -d (N_ "Binary mode")
complete $diff_opt    -l brief -d (N_ "Report only whether files differ")
complete $diff_opt    -s c -d (N_ "Use context format")
complete $diff_opt -r -s C -d (N_ "Set context size")
complete $diff_opt    -l context -d (N_ "Set context format and, optionally, size")
complete $diff_opt    -l changed-group-format -d (N_ "Set line group format")
complete $diff_opt    -s d -l minimal -d (N_ "Try to find a smaller set of changes")
complete $diff_opt    -s e -l ed -d (N_ "Make output a valid ed script")
complete $diff_opt    -s t -l expand-tabs -d (N_ "Expand tabs to spaces")
complete $diff_opt    -s f -l forward-ed -d (N_ "Output that looks like an ed script")
complete $diff_opt -x -s F -d (N_ "Set regexp for context, unified formats")
complete $diff_opt    -s H -l speed-large-files -d (N_ "Speed handling of large files with small changes")
complete $diff_opt -x -l horizon-lines -d (N_ "Set horizon lines")
complete $diff_opt    -s i -l ignore-case -d (N_ "Ignore changes in case")
complete $diff_opt -x -s I -l ignore-matching-lines -d (N_ "Ignore changes matching regexp")
complete $diff_opt -x -l ifdef -d (N_ "Make ifdef from diff")
complete $diff_opt    -s w -l ignore-all-space -d (N_ "Ignore whitespace")
complete $diff_opt    -s T -l initial-tab -d (N_ "Start lines with a tab")
complete $diff_opt -x -s L -l label -d (N_ "Use label instead of filename in output")
complete $diff_opt    -l left-column -d (N_ "Print only left column")
complete $diff_opt -x -l line-format -d (N_ "Use format to produce if-then-else output")
complete $diff_opt    -s n -l rcs -d (N_ "Produce RCS-style diffs")
complete $diff_opt    -s N -l new-file -d (N_ "Treat files absent from one dir as empty")
complete $diff_opt -l new-group-format -l new-line-format -l old-group-format -l old-line-format -d (N_ "Specifies line formatting")
complete $diff_opt    -s p -l show-c-function -d (N_ "Identify the C function each change is in")
complete $diff_opt    -s s -l report-identical-files -d (N_ "Report identical files")
complete $diff_opt    -s y -l side-by-side -d (N_ "Use side-by-side format")
complete $diff_opt    -l suppress-common-lines -d (N_ "Suppress common lines in side-by-side")
complete $diff_opt    -s u -l unified -d (N_ "Use unified format")
complete $diff_opt -x -s U -d (N_ "Set context size in unified")
complete $diff_opt -x -s W -l width -d (N_ "Set column width for side-by-side format")

#
# cvs export options.
#

set -l export_opt -c cvs -n 'contains export (commandline -poc)'
complete $export_opt -x -s d -d (N_ "Name directory for working files")
complete $export_opt    -s N -d (N_ "For -d. Don't shorten paths")

#
# cvs history options.
# Incomplete - many are a pain to describe.
#

set -l history_opt -c cvs -n 'contains history (commandline -poc)'
complete $history_opt -s c -d (N_ "Report on each commit")
complete $history_opt -s e -d (N_ "Report on everything")
complete $history_opt -x -s m -d (N_ "Report on a module")
complete $history_opt -s o -d (N_ "Report on checked-out modules")
complete $history_opt -s T -d (N_ "Report on all tags")
complete $history_opt -x -s x -d (N_ "Specify record type") -a "F\trelease O\tcheckout E\texport T\trtag C\tcollisions G\tmerge U\t'Working file copied from repository' P\t'Working file patched to repository' W\t'Working copy deleted during update' A\t'New file added' M\t'File modified' R\t'File removed.'"
complete $history_opt -s a -d (N_ "Show history for all users")
complete $history_opt -s l -d (N_ "Show last modification only")
complete $history_opt -s w -d (N_ "Show only records for this directory")

#
# cvs import options.
#

set -l import_opt -c cvs -n 'contains import (commandline -poc)'
complete $import_opt -x -s b -d (N_ "Multiple vendor branch")
complete $import_opt -r -s I -d (N_ "Files to ignore during import")

#
# cvs log options.
#

set -l log_opt -c cvs -n 'contains log (commandline -poc)'
complete $log_opt -s b -d (N_ "Print info about revision on default branch")
complete $log_opt -x -s d -d (N_ "Specify date range for query")
complete $log_opt -s h -d (N_ "Print only file info")
complete $log_opt -s N -d (N_ "Do not print tags")
complete $log_opt -s R -d (N_ "Print only rcs filename")
complete $log_opt -x -s r -d (N_ "Print only given revisions")
complete $log_opt -s S -d (N_ "Suppress header if no revisions found")
complete $log_opt -x -s s -d (N_ "Specify revision states")
complete $log_opt -s t -d (N_ "Same as -h, plus descriptive text")
complete $log_opt -x -s w -d (N_ "Specify users for query")

#
# cvs rdiff options.
#

set -l rdiff_opt -c cvs -n 'contains rdiff (commandline -poc)'
complete $rdiff_opt -s c -d (N_ "Use context diff format")
complete $rdiff_opt -s s -d (N_ "Create summary change report")
complete $rdiff_opt -s t -d (N_ "diff top two revisions")
complete $rdiff_opt -s u -d (N_ "Use unidiff format")

#
# cvs release options.
#

complete -c cvs -n 'contains release (commandline -poc)' -s d -d (N_ "Delete working copy if release succeeds")

#
# cvs update options.
#

set -l update_opt -c cvs -n 'contains update (commandline -poc)'
complete $update_opt -s A -d (N_ "Reset sticky tags, dates, and k-opts")
complete $update_opt -s C -d (N_ "Overwrite modified files with clean copies")
complete $update_opt -s d -d (N_ "Create any missing directories")
complete $update_opt -x -s I -d (N_ "Specify files to ignore")
complete $update_opt -x -s j -d (N_ "Merge revisions")

