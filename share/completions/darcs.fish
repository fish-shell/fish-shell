#
# Completions for the darcs VCS command
#
# Incomplete, the number of switches for darcs is _huge_
#

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
complete -c darcs -s h -l help -d (N_ "Shows brief description of command and its arguments")
complete -c darcs -l disable -d (N_ "Disable this command")
complete -c darcs -l repodir -d (N_ "Specify the repository directory in which to run") -x -a '(__fish_complete_directory (commandline -ct))'
complete -c darcs -s v -l verbose -d (N_ "Verbose mode")
complete -c darcs -l standard-verbosity -d (N_ "Neither verbose nor quiet output")

#
# Here follows a huge list of subcommand-specific completions. These are incomplete.
#

set -l record_opt -c darcs -n 'contains record (commandline -poc)'
complete $record_opt -s m -l patch-name -d (N_ "Name of patch") -x
complete $record_opt -s A -l author -d (N_ "Specify author id") -x
complete $record_opt -l logfile -d (N_ "Give patch name and comment in file") -r
complete $record_opt -s a -l all -d (N_ "Answer yes to all patches")
complete $record_opt -s l -l look-for-adds -d (N_ "In addition to modifications, look for files that are not boring, and thus are potentially pending addition")
complete $record_opt -l delete-logfile -d (N_ "Delete the logfile when done")
complete $record_opt -l no-test -d (N_ "Don't run the test script")
complete $record_opt -l test -d (N_ "Run the test script")
complete $record_opt -l leave-test-directory -d (N_ "Don't remove the test directory")
complete $record_opt -l remove-test-directory -d (N_ "Remove the test directory")
complete $record_opt -l compress -d (N_ "Create compressed patches")
complete $record_opt -l dont-compress -d (N_ "Don't create compressed patches")
complete $record_opt -l pipe -d (N_ "Expect to receive input from a pipe")
complete $record_opt -l interactive -d (N_ "Prompt user interactively")
complete $record_opt -l ask-deps -d (N_ "Ask for extra dependencies")
complete $record_opt -l no-ask-deps -d (N_ "Don't ask for extra dependencies")
complete $record_opt -l edit-long-comment -d (N_ "Edit the long comment by default")
complete $record_opt -l skip-long-comment -d (N_ "Don't give a long comment")
complete $record_opt -l prompt-long-comment -d (N_ "Prompt for whether to edit the long comment")
complete $record_opt -l ignore-times -d (N_ "Don't trust the file modification times")
complete $record_opt -l dont-look-for-adds -d (N_ "Don't look for any files or directories that could be added, and don't add them automatically")


set -l pull_opt -c darcs -n 'contains pull (commandline -poc)'
complete $pull_opt -s p -l patches -d (N_ "Select patches matching REGEXP") -x
complete $pull_opt -s t -l tags -d (N_ "Select tags matching REGEXP") -x
complete $pull_opt -s a -l all -d (N_ "Answer yes to all patches")
complete $pull_opt -s s -l summary -d (N_ "Summarize changes")
complete $pull_opt -s q -l quiet -d (N_ "Suppress informational output")
complete $pull_opt -l matches -d (N_ "Select patches matching PATTERN") -x
complete $pull_opt -l external-merge -d (N_ "Use external tool to merge conflicts") -x
complete $pull_opt -l interactive -d (N_ "Prompt user interactively")
complete $pull_opt -l compress -d (N_ "Create compressed patches")
complete $pull_opt -l dont-compress -d (N_ "Don't create compressed patches")
complete $pull_opt -l test -d (N_ "Run the test script")
complete $pull_opt -l no-test -d (N_ "Don't run the test script")
complete $pull_opt -l dry-run -d (N_ "Don't actually take the action")
complete $pull_opt -l no-summary -d (N_ "Don't summarize changes")
complete $pull_opt -l ignore-times -d (N_ "Don't trust the file modification times")
complete $pull_opt -l no-deps -d (N_ "Don't automatically fulfill dependencies")
complete $pull_opt -l set-default -d (N_ "Set default repository [DEFAULT]")
complete $pull_opt -l no-set-default -d (N_ "Don't set default repository")
complete $pull_opt -l set-scripts-executable -d (N_ "Make scripts executable")
complete $pull_opt -l dont-set-scripts-executable -d (N_ "Don't make scripts executable")


set -l apply_opt  -c darcs -n 'contains apply (commandline -poc)'
complete $apply_opt -s a -l all -d (N_ "Answer yes to all patches")
complete $apply_opt -l verify -d (N_ "Verify that the patch was signed by a key in PUBRING") -r
complete $apply_opt -l verify-ssl -d (N_ "Verify using openSSL with authorized keys from specified file") -r
complete $apply_opt -l sendmail-command -d (N_ "Specify sendmail command") -r
complete $apply_opt -l reply -d (N_ "Reply to email-based patch using FROM address") -x
complete $apply_opt -l cc -d (N_ "Mail results to additional EMAIL(s). Requires --reply") -x
complete $apply_opt -l external-merge -d (N_ "Use external tool to merge conflicts") -r
complete $apply_opt -l no-verify -d (N_ "Don't verify patch signature")
complete $apply_opt -l ignore-times -d (N_ "Don't trust the file modification times")
complete $apply_opt -l compress -d (N_ "Create compressed patches")
complete $apply_opt -l dont-compress -d (N_ "Don't create compressed patches")
complete $apply_opt -l interactive -d (N_ "Prompt user interactively")
complete $apply_opt -l mark-conflicts -d (N_ "Mark conflicts")
complete $apply_opt -l allow-conflicts -d (N_ "Allow conflicts, but don't mark them")
complete $apply_opt -l dont-allow-conflicts -d (N_ "Fail on patches that create conflicts [DEFAULT]")
complete $apply_opt -l no-test -d (N_ "Don't run the test script")
complete $apply_opt -l test -d (N_ "Run the test script")
complete $apply_opt -l happy-forwarding -d (N_ "Forward unsigned messages without extra header")
complete $apply_opt -l leave-test-directory -d (N_ "Don't remove the test directory")
complete $apply_opt -l remove-test-directory -d (N_ "Remove the test directory")
complete $apply_opt -l set-scripts-executable -d (N_ "Make scripts executable")
complete $apply_opt -l dont-set-scripts-executable -d (N_ "Don't make scripts executable")


set -l check_opt  -c darcs -n 'contains check (commandline -poc)'
complete $check_opt -s v -l verbose -d (N_ "Verbose mode")
complete $check_opt -s q -l quiet -d (N_ "Suppress informational output")
complete $check_opt -l complete -d (N_ "Check the entire repository")
complete $check_opt -l partial -d (N_ "Check patches since latest checkpoint")
complete $check_opt -l no-test -d (N_ "Don't run the test script")
complete $check_opt -l test -d (N_ "Run the test script")
complete $check_opt -l leave-test-directory -d (N_ "Don't remove the test directory")
complete $check_opt -l remove-test-directory -d (N_ "Remove the test directory")


set -l mv_opt  -c darcs -n 'contains mv (commandline -poc)'
complete $mv_opt -s v -l verbose -d (N_ "Verbose mode")
complete $mv_opt -l case-ok -d (N_ "Don't refuse to add files differing only in case")


set -l send_opt -c darcs -n 'contains send (commandline -poc)'
complete $send_opt -s v -l verbose -d (N_ "Verbose mode")
complete $send_opt -s q -l quiet -d (N_ "Suppress informational output")
complete $send_opt -xs p -l patches -d (N_ "Select patches matching REGEXP")
complete $send_opt -xs t -l tags -d (N_ "Select tags matching REGEXP")
complete $send_opt -s a -l all -d (N_ "Answer yes to all patches")
complete $send_opt -xs A  -l author -d (N_ "Specify author id")
complete $send_opt -rs o -l output -d (N_ "Specify output filename")
complete $send_opt -s u -l unified -d (N_ "Output patch in a darcs-specific format similar to diff -u")
complete $send_opt -s s -l summary -d (N_ "Summarize changes")
complete $send_opt -xl matches -d (N_ "Select patches matching PATTERN")
complete $send_opt -l interactive -d (N_ "Prompt user interactively")
complete $send_opt -xl from -d (N_ "Specify email address")
complete $send_opt -xl to -d (N_ "Specify destination email")
complete $send_opt -xl cc -d (N_ "Mail results to additional EMAIL(s). Requires --reply")
complete $send_opt -l sign -d (N_ "Sign the patch with your gpg key")
complete $send_opt -xl sign-as -d (N_ "Sign the patch with a given keyid")
complete $send_opt -rl sign-ssl -d (N_ "Sign the patch using openssl with a given private key")
complete $send_opt -l dont-sign -d (N_ "Do not sign the patch")
complete $send_opt -l dry-run -d (N_ "Don't actually take the action")
complete $send_opt -l no-summary -d (N_ "Don't summarize changes")
complete $send_opt -rl context -d (N_ "Send to context stored in FILENAME")
complete $send_opt -l edit-description -d (N_ "Edit the patch bundle description")
complete $send_opt -l set-default -d (N_ "Set default repository [DEFAULT]")
complete $send_opt -l no-set-default -d (N_ "Don't set default repository")
complete $send_opt -rl sendmail-command -d (N_ "Specify sendmail command")


set -l init_opt  -c darcs -n 'contains initialize (commandline -poc)'
complete $init_opt -l plain-pristine-tree -d (N_ "Use a plain pristine tree [DEFAULT]")
complete $init_opt -l no-pristine-tree -d (N_ "Use no pristine tree")


set -l annotate_opt -c darcs -n 'contains annotate (commandline -poc)'
complete $annotate_opt -s s -l summary -d (N_ "Summarize changes")
complete $annotate_opt -l no-summary -d (N_ "Don't summarize changes")
complete $annotate_opt -s u -l unified -d (N_ "Output patch in a darcs-specific format similar to diff -u")
complete $annotate_opt -l human-readable -d (N_ "Give human readable output")
complete $annotate_opt -l xml-output -d (N_ "Generate XML formatted output")
complete $annotate_opt -l match -x -d (N_ "Select patch matching PATTERN")
complete $annotate_opt -s p -l patch -x -d (N_ "Select patch matching REGEXP")
complete $annotate_opt -s t -l tag -x -d (N_ "Select tag matching REGEXP")
complete $annotate_opt -l creator-hash -x -d (N_ "Specify hash of creator patch (see docs)")


set -l changes_opt -c darcs -n 'contains changes (commandline -poc)'
complete $changes_opt -l to-match -x -d (N_ "Select changes up to a patch matching PATTERN")
complete $changes_opt -l to-patch -x -d (N_ "Select changes up to a patch matching REGEXP")
complete $changes_opt -l to-tag -x -d (N_ "Select changes up to a tag matching REGEXP")
complete $changes_opt -l from-match -x -d (N_ "Select changes starting with a patch matching PATTERN")
complete $changes_opt -l from-patch -x -d (N_ "Select changes starting with a patch matching REGEXP")
complete $changes_opt -l from-tag -x -d (N_ "Select changes starting with a tag matching REGEXP")
complete $changes_opt -l last -x -d (N_ "Select the last NUMBER patches")
complete $changes_opt -l matches -x -d (N_ "Select patches matching PATTERN")
complete $changes_opt -s p -l patches -x -d (N_ "Select patches matching REGEXP")
complete $changes_opt -s t -l tags -x -d (N_ "Select tags matching REGEXP")
complete $changes_opt -l context -d (N_ "Give output suitable for get --context")
complete $changes_opt -l xml-output -d (N_ "Generate XML formatted output")
complete $changes_opt -l human-readable -d (N_ "Give human-readable output")
complete $changes_opt -s s -l summary -d (N_ "Summarize changes")
complete $changes_opt -l no-summary -d (N_ "Don't summarize changes")
complete $changes_opt -s q -l quiet -d (N_ "Suppress informational output")
complete $changes_opt -l reverse -d (N_ "Show changes in reverse order")
complete $changes_opt -l repo -x -d (N_ "Specify the repository URL")


set -l add_opt -c darcs -n 'contains add (commandline -poc)'
complete $add_opt -l boring -d (N_ "Don't skip boring files")
complete $add_opt -l case-ok -d (N_ "Don't refuse to add files differing only in case")
complete $add_opt -s r -l recursive -d (N_ "Add contents of subdirectories")
complete $add_opt -l not-recursive -d (N_ "Don't add contents of subdirectories")
complete $add_opt -l date-trick -d (N_ "Add files with date appended to avoid conflict. [EXPERIMENTAL]")
complete $add_opt -l no-date-trick -d (N_ "Don't use experimental date appending trick. [DEFAULT]")
complete $add_opt -s q -l quiet -d (N_ "Suppress informational output")
complete $add_opt -l dry-run -d (N_ "Don't actually take the action")
complete $add_opt -s q -l quiet -d (N_ "Suppress informational output")


set -l optimize_opt -c darcs -n 'contains optimize (commandline -poc)'
complete $optimize_opt -l checkpoint -d (N_ "Create a checkpoint file")
complete $optimize_opt -l compress -d (N_ "Create compressed patches")
complete $optimize_opt -l dont-compress -d (N_ "Don't create compressed patches")
complete $optimize_opt -l uncompress -d (N_ "Uncompress patches")
complete $optimize_opt -s t -l tag -r -d (N_ "Name of version to checkpoint")
complete $optimize_opt -l modernize-patches -d (N_ "Rewrite all patches in current darcs format")
complete $optimize_opt -l reorder-patches -d (N_ "Reorder the patches in the repository")
complete $optimize_opt -l sibling -r -d (N_ "Specify a sibling directory")
complete $optimize_opt -l relink -d (N_ "Relink random internal data to a sibling")
complete $optimize_opt -l relink-pristine -d (N_ "Relink pristine tree (not recommended)")
complete $optimize_opt -l posthook -r -d (N_ "Specify command to run after this darcs command.")
complete $optimize_opt -l no-posthook -d (N_ "Do not run posthook command.B")
complete $optimize_opt -l prompt-posthook -d (N_ "Prompt before running posthook. [DEFAULT]")
complete $optimize_opt -l run-posthook -d (N_ "Run posthook command without prompting.")

set -l setpref_opt -c darcs -n 'contains setpref (commandline -poc)'
complete $setpref_opt -l posthook -r -d (N_ "Specify command to run after this darcs command.")
complete $setpref_opt -l no-posthook -d (N_ "Do not run posthook command.B")
complete $setpref_opt -l prompt-posthook -d (N_ "Prompt before running posthook. [DEFAULT]")
complete $setpref_opt -l run-posthook -d (N_ "Run posthook command without prompting.")

