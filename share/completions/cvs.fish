#
# Completions for the cvs VCS command
#
# Incomplete, the number of switches for cvs is _huge_
#

function __fish_no_cvs_subcommand
	set -l cvscommands add admin annotate checkout commit diff edit editors export history import init kserver log login logout pserver rannotate rdiff release remove rlog rtag server status tag unedit update version watch watchers
	set -l cmd (commandline -poc)
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

complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'add' --description "Add a new file/directory to the repository"
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'admin' --description "Administration front end for rcs"
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'annotate' --description "Show last revision where each line was modified"
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'checkout' --description "Checkout sources for editing"
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'commit' --description "Check files into the repository"
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'diff' --description "Show differences between revisions"
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'edit' --description "Get ready to edit a watched file"
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'editors' --description "See who is editing a watched file"
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'export' --description "Export sources from CVS, similar to checkout"
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'history' --description "Show repository access history"
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'import' --description "Import sources into CVS, using vendor branches"
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'init' --description "Create a CVS repository if it doesnt exist"
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'kserver' --description "Kerberos server mode"
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'log' --description "Print out history information for files"
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'login' --description "Prompt for password for authenticating server"
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'logout' --description "Removes entry in .cvspass for remote repository"
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'pserver' --description "Password server mode"
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'rannotate' --description "Show last revision where each line of module was modified"
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'rdiff' --description "Create "patch" format diffs between releases"
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'release' --description "Indicate that a Module is no longer in use"
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'remove' --description "Remove an entry from the repository"
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'rlog' --description "Print out history information for a module"
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'rtag' --description "Add a symbolic tag to a module"
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'server' --description "Server mode"
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'status' --description "Display status information on checked out files"
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'tag' --description "Add a symbolic tag to checked out version of files"
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'unedit' --description "Undo an edit command"
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'update' --description "Bring work tree in sync with repository"
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'version' --description "Display version and exit"
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'watch' --description "Set watches"
complete -c cvs -n '__fish_no_cvs_subcommand' -xa 'watchers' --description "See who is watching a file"

#
# cvs_options switches, which must be specified before a command.
# 

complete -c cvs -n '__fish_no_cvs_subcommand' -x -l allow-root --description "Specify legal cvsroot directory."
complete -c cvs -n '__fish_no_cvs_subcommand' -s a --description "Authenticate all net traffic"
complete -c cvs -n '__fish_no_cvs_subcommand' -r -s T --description "Use "tmpdir" for temporary files"
complete -c cvs -n '__fish_no_cvs_subcommand' -x -s d --description "Overrides $CVSROOT as the root of the CVS tree"
complete -c cvs -n '__fish_no_cvs_subcommand' -x -s e --description "Use "editor" for editing log information"
complete -c cvs -n '__fish_no_cvs_subcommand' -s f --description "Do not use the ~/.cvsrc file"
complete -c cvs -n '__fish_no_cvs_subcommand' -s H --description "Displays usage information for command"
complete -c cvs -n '__fish_no_cvs_subcommand' -s n --description "Do not change any files"
complete -c cvs -n '__fish_no_cvs_subcommand' -s Q --description "Cause CVS to be really quiet"
complete -c cvs -n '__fish_no_cvs_subcommand' -s R --description "Read-only repository mode"
complete -c cvs -n '__fish_no_cvs_subcommand' -s q --description "Cause CVS to be somewhat quiet"
complete -c cvs -n '__fish_no_cvs_subcommand' -s r --description "Make checked-out files read-only"
complete -c cvs -n '__fish_no_cvs_subcommand' -x -s s --description "Set CVS user variable"
complete -c cvs -n '__fish_no_cvs_subcommand' -s t --description "Show trace of program execution -- try with -n"
complete -c cvs -n '__fish_no_cvs_subcommand' -x -s v --description "Display version and exit"
complete -c cvs -n '__fish_no_cvs_subcommand' -s w --description "Make checked-out files read-write (default)"
complete -c cvs -n '__fish_no_cvs_subcommand' -s x --description "Encrypt all net traffic"
complete -c cvs -n '__fish_no_cvs_subcommand' -x -s z --description "Compression level for net traffic" -a '1 2 3 4 5 6 7 8 9'

#
# Universal cvs options, which can be applied to any cvs command.
#

complete -c cvs -n '__fish_seen_subcommand_from checkout diff export history rdiff rtag update' -x -s D --description "Use the most recent revision no later than date"
complete -c cvs -n '__fish_seen_subcommand_from annotate export rdiff rtag update' -s f --description "Retrieve files even when no match for tag/date"
complete -c cvs -n '__fish_seen_subcommand_from add checkout diff import update' -x -s k --description "Alter default keyword processing"
complete -c cvs -n '__fish_seen_subcommand_from annotate checkout commit diff edit editors export log rdiff remove rtag status tag unedit update watch watchers' -s l --description "Don't recurse"
complete -c cvs -n '__fish_seen_subcommand_from add commit import' -x -s m --description "Specify log message instead of invoking editor"
complete -c cvs -n '__fish_seen_subcommand_from checkout commit export rtag' -s n --description "Don't run any tag programs"
complete -c cvs -n 'not __fish_no_cvs_subcommand' -s P --description "Prune empty directories"
complete -c cvs -n '__fish_seen_subcommand_from checkout update' -s p --description "Pipe files to stdout"
complete -c cvs -n '__fish_seen_subcommand_from annotate checkout commit diff edit editors export rdiff remove rtag status tag unedit update watch watchers' -s R --description "Process directories recursively"
complete -c cvs -n '__fish_seen_subcommand_from annotate checkout commit diff history export rdiff rtag update' -x -s r --description "Use a specified tag"
complete -c cvs -n '__fish_seen_subcommand_from import update' -r -s W --description "Specify filenames to be filtered"

#
# cvs options for admin. Note that all options marked as "useless", "might not
#  work", "excessively dangerous" or "does nothing" are not included!
#

set -l admin_opt -c cvs -n 'contains admin (commandline -poc)'
complete $admin_opt -x -s k --description "Set the default keyword substitution"
complete $admin_opt    -s l --description "Lock a revision"
complete $admin_opt -x -s m --description "Replace a log message"
complete $admin_opt -x -s n --description "Force name/rev association"
complete $admin_opt -x -s N --description "Make a name/rev association"
complete $admin_opt    -s q --description "Run quietly"
complete $admin_opt -x -s s --description "Set a state attribute for a revision"
complete $admin_opt    -s t --description "Write descriptive text from a file into RCS"
complete $admin_opt -x -o t- --description "Write descriptive text into RCS"
complete $admin_opt -x -s l --description "Unlock a revision"

#
# cvs options for annotate.
#

set -l annotate_opt -c cvs -n 'contains annotate (commandline -poc)'
complete $annotate_opt -s f --description "Annotate binary files"

#
# cvs options for checkout.
#

set -l checkout_opt -c cvs -n 'contains checkout (commandline -poc)'
complete $checkout_opt    -s A --description "Reset sticky tags/dates/k-opts"
complete $checkout_opt    -s c --description "Copy module file to stdout"
complete $checkout_opt -x -s d --description "Name directory for working files"
complete $checkout_opt -x -s j --description "Merge revisions"
complete $checkout_opt    -s N --description "For -d. Don't shorten paths"

#
# cvs options for commit.
#

set -l commit_opt -c cvs -n 'contains commit (commandline -poc)'
complete $commit_opt -r -s F --description "Read log message from file"
complete $commit_opt    -s f --description "Force new revision"

#
# cvs options for diff.
#

set -l diff_opt -c cvs -n 'contains diff (commandline -poc)'
complete $diff_opt    -s a -l text --description "Treat all files as text"
complete $diff_opt    -s b -l ignore-space-change --description "Treat all whitespace as one space"
complete $diff_opt    -s B -l ignore-blank-lines --description "Ignore blank line only changes"
complete $diff_opt    -l binary --description "Binary mode"
complete $diff_opt    -l brief --description "Report only whether files differ"
complete $diff_opt    -s c --description "Use context format"
complete $diff_opt -r -s C --description "Set context size"
complete $diff_opt    -l context --description "Set context format and, optionally, size"
complete $diff_opt    -l changed-group-format --description "Set line group format"
complete $diff_opt    -s d -l minimal --description "Try to find a smaller set of changes"
complete $diff_opt    -s e -l ed --description "Make output a valid ed script"
complete $diff_opt    -s t -l expand-tabs --description "Expand tabs to spaces"
complete $diff_opt    -s f -l forward-ed --description "Output that looks like an ed script"
complete $diff_opt -x -s F --description "Set regexp for context, unified formats"
complete $diff_opt    -s H -l speed-large-files --description "Speed handling of large files with small changes"
complete $diff_opt -x -l horizon-lines --description "Set horizon lines"
complete $diff_opt    -s i -l ignore-case --description "Ignore changes in case"
complete $diff_opt -x -s I -l ignore-matching-lines --description "Ignore changes matching regexp"
complete $diff_opt -x -l ifdef --description "Make ifdef from diff"
complete $diff_opt    -s w -l ignore-all-space --description "Ignore whitespace"
complete $diff_opt    -s T -l initial-tab --description "Start lines with a tab"
complete $diff_opt -x -s L -l label --description "Use label instead of filename in output"
complete $diff_opt    -l left-column --description "Print only left column"
complete $diff_opt -x -l line-format --description "Use format to produce if-then-else output"
complete $diff_opt    -s n -l rcs --description "Produce RCS-style diffs"
complete $diff_opt    -s N -l new-file --description "Treat files absent from one dir as empty"
complete $diff_opt -l new-group-format -l new-line-format -l old-group-format -l old-line-format --description "Specifies line formatting"
complete $diff_opt    -s p -l show-c-function --description "Identify the C function each change is in"
complete $diff_opt    -s s -l report-identical-files --description "Report identical files"
complete $diff_opt    -s y -l side-by-side --description "Use side-by-side format"
complete $diff_opt    -l suppress-common-lines --description "Suppress common lines in side-by-side"
complete $diff_opt    -s u -l unified --description "Use unified format"
complete $diff_opt -x -s U --description "Set context size in unified"
complete $diff_opt -x -s W -l width --description "Set column width for side-by-side format"

#
# cvs export options.
#

set -l export_opt -c cvs -n 'contains export (commandline -poc)'
complete $export_opt -x -s d --description "Name directory for working files"
complete $export_opt    -s N --description "For -d. Don't shorten paths"

#
# cvs history options.
# Incomplete - many are a pain to describe.
#

set -l history_opt -c cvs -n 'contains history (commandline -poc)'
complete $history_opt -s c --description "Report on each commit"
complete $history_opt -s e --description "Report on everything"
complete $history_opt -x -s m --description "Report on a module"
complete $history_opt -s o --description "Report on checked-out modules"
complete $history_opt -s T --description "Report on all tags"
complete $history_opt -x -s x --description "Specify record type" -a "F\trelease O\tcheckout E\texport T\trtag C\tcollisions G\tmerge U\t'Working file copied from repository' P\t'Working file patched to repository' W\t'Working copy deleted during update' A\t'New file added' M\t'File modified' R\t'File removed.'"
complete $history_opt -s a --description "Show history for all users"
complete $history_opt -s l --description "Show last modification only"
complete $history_opt -s w --description "Show only records for this directory"

#
# cvs import options.
#

set -l import_opt -c cvs -n 'contains import (commandline -poc)'
complete $import_opt -x -s b --description "Multiple vendor branch"
complete $import_opt -r -s I --description "Files to ignore during import"

#
# cvs log options.
#

set -l log_opt -c cvs -n 'contains log (commandline -poc)'
complete $log_opt -s b --description "Print info about revision on default branch"
complete $log_opt -x -s d --description "Specify date range for query"
complete $log_opt -s h --description "Print only file info"
complete $log_opt -s N --description "Do not print tags"
complete $log_opt -s R --description "Print only rcs filename"
complete $log_opt -x -s r --description "Print only given revisions"
complete $log_opt -s S --description "Suppress header if no revisions found"
complete $log_opt -x -s s --description "Specify revision states"
complete $log_opt -s t --description "Same as -h, plus descriptive text"
complete $log_opt -x -s w --description "Specify users for query"

#
# cvs rdiff options.
#

set -l rdiff_opt -c cvs -n 'contains rdiff (commandline -poc)'
complete $rdiff_opt -s c --description "Use context diff format"
complete $rdiff_opt -s s --description "Create summary change report"
complete $rdiff_opt -s t --description "diff top two revisions"
complete $rdiff_opt -s u --description "Use unidiff format"

#
# cvs release options.
#

complete -c cvs -n 'contains release (commandline -poc)' -s d --description "Delete working copy if release succeeds"

#
# cvs update options.
#

set -l update_opt -c cvs -n 'contains update (commandline -poc)'
complete $update_opt -s A --description "Reset sticky tags, dates, and k-opts"
complete $update_opt -s C --description "Overwrite modified files with clean copies"
complete $update_opt -s d --description "Create any missing directories"
complete $update_opt -x -s I --description "Specify files to ignore"
complete $update_opt -x -s j --description "Merge revisions"

