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

complete -c darcs -n '__fish_use_subcommand' -xa "
	initialize\t'Create new project'
	get\t'Create a local copy of another repository'
	add\t'Add one or more new files or directories'
	remove\t'Remove one or more files or directories from the repository'
	mv\t'Move/rename one or more files or directories'
	replace\t'Replace a token with a new value for that token'
	record\t'Save changes in the working copy to the repository as a patch'
	pull\t'Copy and apply patches from another repository to this one'
	send\t'Send by email a bundle of one or more patches'
	apply\t'Apply patches (from an email bundle) to the repository'
	push\t'Copy and apply patches from this repository to another one'
	whatsnew\t'Display unrecorded changes in the working copy'
	changes\t'Gives a changelog style summary of the repo history'
	unrecord\t'Remove recorded patches without changing the working copy'
	amend-record\t'Replace a recorded patch with a better version'
	revert\t'Revert to the recorded version (safe the first time only)'
	unrevert\t'Undo the last revert (may fail if changes after the revert)'
	unpull\t'Opposite of pull; unsafe if the patch is not in remote repo'
	rollback\t'Record an inverse patch without changing the working directory'
	tag\t'Tag the contents of the repository with a version name'
	setpref\t'Set a value for a preference (test, predist, ...)'
	diff\t'Create a diff between two versions of the repository'
	annotate\t'Display which patch last modified something'
	optimize\t'Optimize the repository'
	check\t'Check the repository for consistency'
	resolve\t'Mark any conflicts to the working copy for manual resolution'
	dist\t'Create a distribution tarball'
	trackdown\t'Locate the most recent version lacking an error'
	repair\t'Repair the corrupted repository'
"

#
# These switches are universal
#
complete -c darcs -s h -l help -d "shows brief description of command and its arguments"
complete -c darcs -l disable -d "Disable this command"
complete -c darcs -l repodir -d "Specify the repository directory in which to run" -x -a '(__fish_complete_directory (commandline -ct))'
complete -c darcs -s v -l verbose -d "give verbose output"

#
# Here follows a huge list of subcommand-specific completions
#

set -- record_opt -c darcs -n 'contains record (commandline -poc)'
complete $record_opt -s m -l patch-name -d "Name of patch" -x
complete $record_opt -s A -l author -d "Specify author id" -x
complete $record_opt -l logfile -d "Give patch name and comment in file" -r
complete $record_opt -s a -l all -d "answer yes to all patches"
complete $record_opt -s l -l look-for-adds -d "In addition to modifications, look for files that are not boring, and thus are potentially pending addition"
complete $record_opt -l delete-logfile -d "Delete the logfile when done"
complete $record_opt -l standard-verbosity -d "Don"\'"t give verbose output"
complete $record_opt -l no-test -d "Don"\'"t run the test script"
complete $record_opt -l test -d "Run the test script"
complete $record_opt -l leave-test-directory -d "Don"\'"t remove the test directory"
complete $record_opt -l remove-test-directory -d "Remove the test directory"
complete $record_opt -l compress -d "Create compressed patches"
complete $record_opt -l dont-compress -d "Don"\'"t create compressed patches"
complete $record_opt -l pipe -d "Expect to receive input from a pipe"
complete $record_opt -l interactive -d "Prompt user interactively"
complete $record_opt -l ask-deps -d "Ask for extra dependencies"
complete $record_opt -l no-ask-deps -d "Don"\'"t ask for extra dependencies"
complete $record_opt -l edit-long-comment -d "Edit the long comment by default"
complete $record_opt -l skip-long-comment -d "Don"\'"t give a long comment"
complete $record_opt -l prompt-long-comment -d "Prompt for whether to edit the long comment"
complete $record_opt -l ignore-times -d "Don"\'"t trust the file modification times"
complete $record_opt -l dont-look-for-adds -d "Don"\'"t look for any files or directories that could be added, and don"\'"t add them automatically"
set -e record_opt


set -- pull_opt -c darcs -n 'contains pull (commandline -poc)'
complete $pull_opt -s p -l patches -d "select patches matching REGEXP" -x
complete $pull_opt -s t -l tags -d "select tags matching REGEXP" -x
complete $pull_opt -s a -l all -d "answer yes to all patches"
complete $pull_opt -s s -l summary -d "summarize changes"
complete $pull_opt -s q -l quiet -d "suppress informational output"
complete $pull_opt -l matches -d "select patches matching PATTERN" -x
complete $pull_opt -l external-merge -d "Use external tool to merge conflicts" -x
complete $pull_opt -l interactive -d "prompt user interactively"
complete $pull_opt -l compress -d "create compressed patches"
complete $pull_opt -l dont-compress -d "don"\'"t create compressed patches"
complete $pull_opt -l test -d "run the test script"
complete $pull_opt -l no-test -d "don"\'"t run the test script"
complete $pull_opt -l dry-run -d "don"\'"t actually take the action"
complete $pull_opt -l no-summary -d "don"\'"t summarize changes"
complete $pull_opt -l standard-verbosity -d "neither verbose nor quiet output"
complete $pull_opt -l ignore-times -d "don"\'"t trust the file modification times"
complete $pull_opt -l no-deps -d "don"\'"t automatically fulfill dependencies"
complete $pull_opt -l set-default -d "set default repository [DEFAULT]"
complete $pull_opt -l no-set-default -d "don"\'"t set default repository"
complete $pull_opt -l set-scripts-executable -d "make scripts executable"
complete $pull_opt -l dont-set-scripts-executable -d "don"\'"t make scripts executable"
set -e pull_opt


set -- apply_opt  -c darcs -n 'contains apply (commandline -poc)'
complete $apply_opt -s a -l all -d "answer yes to all patches"
complete $apply_opt -l verify -d "verify that the patch was signed by a key in PUBRING" -r
complete $apply_opt -l verify-ssl -d "verify using openSSL with authorized keys from file "\'"KEYS"\'"" -r
complete $apply_opt -l sendmail-command -d "specify sendmail command" -r
complete $apply_opt -l reply -d "reply to email-based patch using FROM address" -x
complete $apply_opt -l cc -d "mail results to additional EMAIL(s). Requires --reply" -x
complete $apply_opt -l external-merge -d "Use external tool to merge conflicts" -r
complete $apply_opt -l no-verify -d "don"\'"t verify patch signature"
complete $apply_opt -l standard-verbosity -d "don"\'"t give verbose output"
complete $apply_opt -l ignore-times -d "don"\'"t trust the file modification times"
complete $apply_opt -l compress -d "create compressed patches"
complete $apply_opt -l dont-compress -d "don"\'"t create compressed patches"
complete $apply_opt -l interactive -d "prompt user interactively"
complete $apply_opt -l mark-conflicts -d "mark conflicts"
complete $apply_opt -l allow-conflicts -d "allow conflicts, but don"\'"t mark them"
complete $apply_opt -l no-resolve-conflicts -d "equivalent to --dont-allow-conflicts, for backwards compatibility"
complete $apply_opt -l dont-allow-conflicts -d "fail on patches that create conflicts [DEFAULT]"
complete $apply_opt -l no-test -d "don"\'"t run the test script"
complete $apply_opt -l test -d "run the test script"
complete $apply_opt -l happy-forwarding -d "forward unsigned messages without extra header"
complete $apply_opt -l leave-test-directory -d "don"\'"t remove the test directory"
complete $apply_opt -l remove-test-directory -d "remove the test directory"
complete $apply_opt -l set-scripts-executable -d "make scripts executable"
complete $apply_opt -l dont-set-scripts-executable -d "don"\'"t make scripts executable"
set -e apply_opt

set -- check_opt  -c darcs -n 'contains check (commandline -poc)'
complete $check_opt -s v -l verbose -d "give verbose output"
complete $check_opt -s q -l quiet -d "suppress informational output"
complete $check_opt -l complete -d "check the entire repository"
complete $check_opt -l partial -d "check patches since latest checkpoint"
complete $check_opt -l standard-verbosity -d "neither verbose nor quiet output"
complete $check_opt -l no-test -d "don"\'"t run the test script"
complete $check_opt -l test -d "run the test script"
complete $check_opt -l leave-test-directory -d "don"\'"t remove the test directory"
complete $check_opt -l remove-test-directory -d "remove the test directory"
set -e check_opt

set -- mv_opt  -c darcs -n 'contains mv (commandline -poc)'
complete $mv_opt -s v -l verbose -d "give verbose output"
complete $mv_opt -l case-ok -d "don"\'"t refuse to add files differing only in case"
complete $mv_opt -l standard-verbosity -d "don"\'"t give verbose output"
set -e mv_opt

set -- send_opt -c darcs -n 'contains send (commandline -poc)'
complete $send_opt -s v -l verbose -d "give verbose output"
complete $send_opt -s q -l quiet -d "suppress informational output"
complete $send_opt -xs p -l patches -d "select patches matching REGEXP"
complete $send_opt -xs t -l tags -d "select tags matching REGEXP"
complete $send_opt -s a -l all -d "answer yes to all patches"
complete $send_opt -xs A  -l author -d "specify author id"
complete $send_opt -rs o -l output -d "specify output filename"
complete $send_opt -s u -l unified -d "output patch in a darcs-specific format similar to diff -u"
complete $send_opt -s s -l summary -d "summarize changes"
complete $send_opt -l standard-verbosity -d "neither verbose nor quiet output"
complete $send_opt -xl matches -d "select patches matching PATTERN"
complete $send_opt -l interactive -d "prompt user interactively"
complete $send_opt -xl from -d "specify email address"
complete $send_opt -xl to -d "specify destination email"
complete $send_opt -xl cc -d "mail results to additional EMAIL(s). Requires --reply"
complete $send_opt -l sign -d "sign the patch with your gpg key"
complete $send_opt -xl sign-as -d "sign the patch with a given keyid"
complete $send_opt -rl sign-ssl -d "sign the patch using openssl with a given private key"
complete $send_opt -l dont-sign -d "do not sign the patch"
complete $send_opt -l dry-run -d "don"\'"t actually take the action"
complete $send_opt -l no-summary -d "don"\'"t summarize changes"
complete $send_opt -rl context -d "send to context stored in FILENAME"
complete $send_opt -l edit-description -d "edit the patch bundle description"
complete $send_opt -l set-default -d "set default repository [DEFAULT]"
complete $send_opt -l no-set-default -d "don"\'"t set default repository"
complete $send_opt -rl sendmail-command -d "specify sendmail command"
set -e send_opt

set -- init_opt  -c darcs -n 'contains initialize (commandline -poc)'
complete $init_opt -l plain-pristine-tree -d "Use a plain pristine tree [DEFAULT]"
complete $init_opt -l no-pristine-tree -d "Use no pristine tree"
set -e init_opt

