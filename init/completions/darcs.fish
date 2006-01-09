#
# Completions for the darcs VCS command
#
# Incomplete, the number of switches for darcs is _huge_
#

#
# Test if a non-switch argument has been given
#

function __fish_use_subcommand 
	set -l -- cmd (commandline -poc)
	set -e cmd[1]
	for i in $cmd
		switch $i
			case '-*'
				continue
		end
		return 1
	end
	return 0
end

#
# If no subcommand has been specified, complete using all available subcommands
#

complete -c darcs -n '__fish_use_subcommand' -xa 'initialize\t"'(_ "Create new project")'"'
complete -c darcs -n '__fish_use_subcommand' -xa 'get\t"'(_ "Create a local copy of another repository")'"'
complete -c darcs -n '__fish_use_subcommand' -xa 'add\t"'(_ "Add one or more new files or directories")'"'
complete -c darcs -n '__fish_use_subcommand' -xa 'remove\t"'(_ "Remove one or more files or directories from the repository")'"'
complete -c darcs -n '__fish_use_subcommand' -xa 'mv\t"'(_ "Move/rename one or more files or directories")'"'
complete -c darcs -n '__fish_use_subcommand' -xa 'replace\t"'(_ "Replace a token with a new value for that token")'"'
complete -c darcs -n '__fish_use_subcommand' -xa 'record\t"'(_ "Save changes in the working copy to the repository as a patch")'"'
complete -c darcs -n '__fish_use_subcommand' -xa 'pull\t"'(_ "Copy and apply patches from another repository to this one")'"'
complete -c darcs -n '__fish_use_subcommand' -xa 'send\t"'(_ "Send by email a bundle of one or more patches")'"'
complete -c darcs -n '__fish_use_subcommand' -xa 'apply\t"'(_ "Apply patches (from an email bundle) to the repository")'"'
complete -c darcs -n '__fish_use_subcommand' -xa 'push\t"'(_ "Copy and apply patches from this repository to another one")'"'
complete -c darcs -n '__fish_use_subcommand' -xa 'whatsnew\t"'(_ "Display unrecorded changes in the working copy")'"'
complete -c darcs -n '__fish_use_subcommand' -xa 'changes\t"'(_ "Gives a changelog style summary of the repo history")'"'
complete -c darcs -n '__fish_use_subcommand' -xa 'unrecord\t"'(_ "Remove recorded patches without changing the working copy")'"'
complete -c darcs -n '__fish_use_subcommand' -xa 'amend-record\t"'(_ "Replace a recorded patch with a better version")'"'
complete -c darcs -n '__fish_use_subcommand' -xa 'revert\t"'(_ "Revert to the recorded version (safe the first time only)" )'"'
complete -c darcs -n '__fish_use_subcommand' -xa 'unrevert\t"'(_ "Undo the last revert (may fail if changes after the revert)")'"'
complete -c darcs -n '__fish_use_subcommand' -xa 'unpull\t"'(_ "Opposite of pull; unsafe if the patch is not in remote repo")'"'
complete -c darcs -n '__fish_use_subcommand' -xa 'rollback\t"'(_ "Record an inverse patch without changing the working copy" )'"'
complete -c darcs -n '__fish_use_subcommand' -xa 'tag\t"'(_ "Tag the contents of the repository with a version name")'"'
complete -c darcs -n '__fish_use_subcommand' -xa 'setpref\t"'(_ "Set a value for a preference (test, predist, ...)")'"'
complete -c darcs -n '__fish_use_subcommand' -xa 'diff\t"'(_ "Create a diff between two versions of the repository")'"'
complete -c darcs -n '__fish_use_subcommand' -xa 'annotate\t"'(_ "Display which patch last modified something")'"'
complete -c darcs -n '__fish_use_subcommand' -xa 'optimize\t"'(_ "Optimize the repository")'"'
complete -c darcs -n '__fish_use_subcommand' -xa 'check\t"'(_ "Check the repository for consistency")'"'
complete -c darcs -n '__fish_use_subcommand' -xa 'resolve\t"'(_ "Mark any conflicts to the working copy for manual resolution")'"'
complete -c darcs -n '__fish_use_subcommand' -xa 'dist\t"'(_ "Create a distribution tarball")'"'
complete -c darcs -n '__fish_use_subcommand' -xa 'trackdown\t"'(_ "Locate the most recent version lacking an error")'"'
complete -c darcs -n '__fish_use_subcommand' -xa 'repair\t"'(_ "Repair the corrupted repository")'"'
 
#
# These switches are universal
#
complete -c darcs -s h -l help -d (_ "Shows brief description of command and its arguments")
complete -c darcs -l disable -d (_ "Disable this command")
complete -c darcs -l repodir -d (_ "Specify the repository directory in which to run") -x -a '(__fish_complete_directory (commandline -ct))'
complete -c darcs -s v -l verbose -d (_ "Verbose mode")

#
# Here follows a huge list of subcommand-specific completions
#

set -- record_opt -c darcs -n 'contains record (commandline -poc)'
complete $record_opt -s m -l patch-name -d (_ "Name of patch") -x
complete $record_opt -s A -l author -d (_ "Specify author id") -x
complete $record_opt -l logfile -d (_ "Give patch name and comment in file") -r
complete $record_opt -s a -l all -d (_ "Answer yes to all patches")
complete $record_opt -s l -l look-for-adds -d (_ "In addition to modifications, look for files that are not boring, and thus are potentially pending addition")
complete $record_opt -l delete-logfile -d (_ "Delete the logfile when done")
complete $record_opt -l standard-verbosity -d (_ "Don't give verbose output")
complete $record_opt -l no-test -d (_ "Don't run the test script")
complete $record_opt -l test -d (_ "Run the test script")
complete $record_opt -l leave-test-directory -d (_ "Don't remove the test directory")
complete $record_opt -l remove-test-directory -d (_ "Remove the test directory")
complete $record_opt -l compress -d (_ "Create compressed patches")
complete $record_opt -l dont-compress -d (_ "Don't create compressed patches")
complete $record_opt -l pipe -d (_ "Expect to receive input from a pipe")
complete $record_opt -l interactive -d (_ "Prompt user interactively")
complete $record_opt -l ask-deps -d (_ "Ask for extra dependencies")
complete $record_opt -l no-ask-deps -d (_ "Don't ask for extra dependencies")
complete $record_opt -l edit-long-comment -d (_ "Edit the long comment by default")
complete $record_opt -l skip-long-comment -d (_ "Don't give a long comment")
complete $record_opt -l prompt-long-comment -d (_ "Prompt for whether to edit the long comment")
complete $record_opt -l ignore-times -d (_ "Don't trust the file modification times")
complete $record_opt -l dont-look-for-adds -d (_ "Don't look for any files or directories that could be added, and don't add them automatically")
set -e record_opt


set -- pull_opt -c darcs -n 'contains pull (commandline -poc)'
complete $pull_opt -s p -l patches -d (_ "Select patches matching REGEXP") -x
complete $pull_opt -s t -l tags -d (_ "Select tags matching REGEXP") -x
complete $pull_opt -s a -l all -d (_ "Answer yes to all patches")
complete $pull_opt -s s -l summary -d (_ "Summarize changes")
complete $pull_opt -s q -l quiet -d (_ "Suppress informational output")
complete $pull_opt -l matches -d (_ "Select patches matching PATTERN") -x
complete $pull_opt -l external-merge -d (_ "Use external tool to merge conflicts") -x
complete $pull_opt -l interactive -d (_ "Prompt user interactively")
complete $pull_opt -l compress -d (_ "Create compressed patches")
complete $pull_opt -l dont-compress -d (_ "Don't create compressed patches")
complete $pull_opt -l test -d (_ "Run the test script")
complete $pull_opt -l no-test -d (_ "Don't run the test script")
complete $pull_opt -l dry-run -d (_ "Don't actually take the action")
complete $pull_opt -l no-summary -d (_ "Don't summarize changes")
complete $pull_opt -l standard-verbosity -d (_ "Neither verbose nor quiet output")
complete $pull_opt -l ignore-times -d (_ "Don't trust the file modification times")
complete $pull_opt -l no-deps -d (_ "Don't automatically fulfill dependencies")
complete $pull_opt -l set-default -d (_ "Set default repository [DEFAULT]")
complete $pull_opt -l no-set-default -d (_ "Don't set default repository")
complete $pull_opt -l set-scripts-executable -d (_ "Make scripts executable")
complete $pull_opt -l dont-set-scripts-executable -d (_ "Don't make scripts executable")
set -e pull_opt


set -- apply_opt  -c darcs -n 'contains apply (commandline -poc)'
complete $apply_opt -s a -l all -d (_ "Answer yes to all patches")
complete $apply_opt -l verify -d (_ "Verify that the patch was signed by a key in PUBRING") -r
complete $apply_opt -l verify-ssl -d (_ "Verify using openSSL with authorized keys from file 'KEYS'") -r
complete $apply_opt -l sendmail-command -d (_ "Specify sendmail command") -r
complete $apply_opt -l reply -d (_ "Reply to email-based patch using FROM address") -x
complete $apply_opt -l cc -d (_ "Mail results to additional EMAIL(s). Requires --reply") -x
complete $apply_opt -l external-merge -d (_ "Use external tool to merge conflicts") -r
complete $apply_opt -l no-verify -d (_ "Don't verify patch signature")
complete $apply_opt -l standard-verbosity -d (_ "Don't give verbose output")
complete $apply_opt -l ignore-times -d (_ "Don't trust the file modification times")
complete $apply_opt -l compress -d (_ "Create compressed patches")
complete $apply_opt -l dont-compress -d (_ "Don't create compressed patches")
complete $apply_opt -l interactive -d (_ "Prompt user interactively")
complete $apply_opt -l mark-conflicts -d (_ "Mark conflicts")
complete $apply_opt -l allow-conflicts -d (_ "Allow conflicts, but don't mark them")
complete $apply_opt -l dont-allow-conflicts -d (_ "Fail on patches that create conflicts [DEFAULT]")
complete $apply_opt -l no-test -d (_ "Don't run the test script")
complete $apply_opt -l test -d (_ "Run the test script")
complete $apply_opt -l happy-forwarding -d (_ "Forward unsigned messages without extra header")
complete $apply_opt -l leave-test-directory -d (_ "Don't remove the test directory")
complete $apply_opt -l remove-test-directory -d (_ "Remove the test directory")
complete $apply_opt -l set-scripts-executable -d (_ "Make scripts executable")
complete $apply_opt -l dont-set-scripts-executable -d (_ "Don't make scripts executable")
set -e apply_opt

set -- check_opt  -c darcs -n 'contains check (commandline -poc)'
complete $check_opt -s v -l verbose -d (_ "Verbose mode")
complete $check_opt -s q -l quiet -d (_ "Suppress informational output")
complete $check_opt -l complete -d (_ "Check the entire repository")
complete $check_opt -l partial -d (_ "Check patches since latest checkpoint")
complete $check_opt -l standard-verbosity -d (_ "Neither verbose nor quiet output")
complete $check_opt -l no-test -d (_ "Don't run the test script")
complete $check_opt -l test -d (_ "Run the test script")
complete $check_opt -l leave-test-directory -d (_ "Don't remove the test directory")
complete $check_opt -l remove-test-directory -d (_ "Remove the test directory")
set -e check_opt

set -- mv_opt  -c darcs -n 'contains mv (commandline -poc)'
complete $mv_opt -s v -l verbose -d (_ "Verbose mode")
complete $mv_opt -l case-ok -d (_ "Don't refuse to add files differing only in case")
complete $mv_opt -l standard-verbosity -d (_ "Don't give verbose output")
set -e mv_opt

set -- send_opt -c darcs -n 'contains send (commandline -poc)'
complete $send_opt -s v -l verbose -d (_ "Verbose mode")
complete $send_opt -s q -l quiet -d (_ "Suppress informational output")
complete $send_opt -xs p -l patches -d (_ "Select patches matching REGEXP")
complete $send_opt -xs t -l tags -d (_ "Select tags matching REGEXP")
complete $send_opt -s a -l all -d (_ "Answer yes to all patches")
complete $send_opt -xs A  -l author -d (_ "Specify author id")
complete $send_opt -rs o -l output -d (_ "Specify output filename")
complete $send_opt -s u -l unified -d (_ "Output patch in a darcs-specific format similar to diff -u")
complete $send_opt -s s -l summary -d (_ "Summarize changes")
complete $send_opt -l standard-verbosity -d (_ "Neither verbose nor quiet output")
complete $send_opt -xl matches -d (_ "Select patches matching PATTERN")
complete $send_opt -l interactive -d (_ "Prompt user interactively")
complete $send_opt -xl from -d (_ "Specify email address")
complete $send_opt -xl to -d (_ "Specify destination email")
complete $send_opt -xl cc -d (_ "Mail results to additional EMAIL(s). Requires --reply")
complete $send_opt -l sign -d (_ "Sign the patch with your gpg key")
complete $send_opt -xl sign-as -d (_ "Sign the patch with a given keyid")
complete $send_opt -rl sign-ssl -d (_ "Sign the patch using openssl with a given private key")
complete $send_opt -l dont-sign -d (_ "Do not sign the patch")
complete $send_opt -l dry-run -d (_ "Don't actually take the action")
complete $send_opt -l no-summary -d (_ "Don't summarize changes")
complete $send_opt -rl context -d (_ "Send to context stored in FILENAME")
complete $send_opt -l edit-description -d (_ "Edit the patch bundle description")
complete $send_opt -l set-default -d (_ "Set default repository [DEFAULT]")
complete $send_opt -l no-set-default -d (_ "Don't set default repository")
complete $send_opt -rl sendmail-command -d (_ "Specify sendmail command")
set -e send_opt

set -- init_opt  -c darcs -n 'contains initialize (commandline -poc)'
complete $init_opt -l plain-pristine-tree -d (_ "Use a plain pristine tree [DEFAULT]")
complete $init_opt -l no-pristine-tree -d (_ "Use no pristine tree")
set -e init_opt

