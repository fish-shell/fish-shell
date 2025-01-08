# NOTE: Fish helper functions are your best friend here!
# see https://github.com/fish-shell/fish-shell/blob/master/share/functions/__fish_seen_subcommand_from.fish
# and https://github.com/fish-shell/fish-shell/blob/master/share/functions/__fish_use_subcommand.fish

# help commands
complete -f -c bzr -n __fish_use_subcommand -a help -d 'Show help'
complete -f -c bzr -n '__fish_seen_subcommand_from help' -a topics -d 'List all help topics'
complete -f -c bzr -n '__fish_seen_subcommand_from help' -a commands -d 'List all commands'
complete -f -c bzr -n '__fish_seen_subcommand_from help' -a formats -d 'Help about supported formats'
complete -f -c bzr -n '__fish_seen_subcommand_from help' -a current-formats -d 'Help about current supported formats'
complete -f -c bzr -n '__fish_seen_subcommand_from help' -a revisionspec -d 'How to specify revisions in bzr'
complete -f -c bzr -n '__fish_seen_subcommand_from help' -a bugs -d 'Show bug tracker settings in bzr'
# TODO: Add help about specific commands generating the output

# init command
complete -f -c bzr -n __fish_use_subcommand -a init -d 'Makes this directory a versioned branch'
complete -f -c bzr -n '__fish_seen_subcommand_from init' -l create-prefix -d 'Create the path leading up to the branch if it does not already exists'
complete -f -c bzr -n '__fish_seen_subcommand_from init' -l no-tree -d 'Create a branch without a working tree'
complete -f -c bzr -n '__fish_seen_subcommand_from init' -l append-revisions-only -d 'Never change revnos or the existing log. Append revisions to it only'
# TODO: Add init using path directory

# branch command
complete -f -c bzr -n __fish_use_subcommand -a branch -d 'Make a copy of another branch'
complete -f -c bzr -n '__fish_seen_subcommand_from branch' -l use-existing-dir -d 'Proceed even if directory exits'
complete -f -c bzr -n '__fish_seen_subcommand_from branch' -l stacked -d 'Create a stacked branch referring to the source branch'
complete -f -c bzr -n '__fish_seen_subcommand_from branch' -l standalone -d 'Do not use a shared repository, even if available'
complete -f -c bzr -n '__fish_seen_subcommand_from branch' -l switch -d 'Switch the checkout in the current directory to the new branch'
complete -f -c bzr -n '__fish_seen_subcommand_from branch' -l hardlink -d 'Hard-link working tree files where possible'
complete -f -c bzr -n '__fish_seen_subcommand_from branch' -l bind -d 'Bind new branch to from location'
complete -f -c bzr -n '__fish_seen_subcommand_from branch' -l no-tree -d 'Create a branch without a working-tree'
# TODO: Add source/destination branch or directory

# add command
complete -f -c bzr -n __fish_use_subcommand -a add -d 'Make files or directories versioned'
complete -f -c bzr -n '__fish_seen_subcommand_from add' -l no-recurse -s N -d 'Don\'t recursively add the contents of directories'

# ignore command
complete -f -c bzr -n __fish_use_subcommand -a ignore -d 'Ignore a file or pattern'
complete -f -c bzr -n '__fish_seen_subcommand_from ignore' -l default-rules -d 'Display the default ignore rules that bzr uses'

# mv command
complete -f -c bzr -n __fish_use_subcommand -a mv -d 'Move or rename a versioned file'
complete -f -c bzr -n '__fish_seen_subcommand_from mv' -l auto -d 'Automatically guess renames'
complete -f -c bzr -n '__fish_seen_subcommand_from mv' -l after -d 'Move only the bzr identifier of the file, because the file has already been moved'

# status command
complete -f -c bzr -n __fish_use_subcommand -a status -d 'Summarize changes in working copy'
complete -f -c bzr -n '__fish_seen_subcommand_from status' -l short -s S -d 'Use short status indicators'
complete -f -c bzr -n '__fish_seen_subcommand_from status' -l versioned -s V -d 'Only show versioned files'
complete -f -c bzr -n '__fish_seen_subcommand_from status' -l no-pending -d 'Don\'t show pending merges'
complete -f -c bzr -n '__fish_seen_subcommand_from status' -l no-classify -d 'Do not mark object type using indicator'
complete -f -c bzr -n '__fish_seen_subcommand_from status' -l show-ids -d 'Show internal object ids'

# diff command
complete -f -c bzr -n __fish_use_subcommand -a diff -d 'Show detailed diffs'
# TODO: Add file diff options

# merge command
complete -f -c bzr -n __fish_use_subcommand -a merge -d 'Pull in changes from another branch'
complete -f -c bzr -n '__fish_seen_subcommand_from merge' -l pull -d 'If destination is already merged, pull rather than merging'
complete -f -c bzr -n '__fish_seen_subcommand_from merge' -l remember -d 'Remember the specified location as a default'
complete -f -c bzr -n '__fish_seen_subcommand_from merge' -l force -d 'Merge even if the destination tree has uncommitted changes'
complete -f -c bzr -n '__fish_seen_subcommand_from merge' -l reprocess -d 'Reprocess to reduce spurious conflicts'
complete -f -c bzr -n '__fish_seen_subcommand_from merge' -l uncommitted -d 'Apply uncommitted changes from a working copy, instead of branch changes'
complete -f -c bzr -n '__fish_seen_subcommand_from merge' -l show-base -d 'Show base revision text in conflicts'
complete -f -c bzr -n '__fish_seen_subcommand_from merge' -l preview -d 'Instead of merging, show a diff of the merge'
complete -f -c bzr -n '__fish_seen_subcommand_from merge' -l interactive -s i -d 'Select changes interactively'
complete -f -c bzr -n '__fish_seen_subcommand_from merge' -l directory -s d -d 'Branch to merge into, rather than the one containing the working directory'
complete -f -c bzr -n '__fish_seen_subcommand_from merge' -l change -s c -d 'Select changes introduced by the specified revision'
complete -f -c bzr -n '__fish_seen_subcommand_from merge' -l revision -s r -d 'Select changes introduced by the specified revision'

# commit command
complete -f -c bzr -n __fish_use_subcommand -a commit -d 'Save some or all changes'
complete -f -c bzr -n '__fish_seen_subcommand_from commit' -l show-diff -s p -d 'When no message is supplied, show diff along with status summary in message editor'
complete -f -c bzr -n '__fish_seen_subcommand_from commit' -l file -s F -d 'Take commit message from this file'
complete -f -c bzr -n '__fish_seen_subcommand_from commit' -l exclude -s x -d 'Do not consider changes made to a given path'
complete -f -c bzr -n '__fish_seen_subcommand_from commit' -l message -s m -d 'Description of the new revision'
complete -f -c bzr -n '__fish_seen_subcommand_from commit' -l author -d 'Set the author\'s name, if it\'s different from the commiter'
complete -f -c bzr -n '__fish_seen_subcommand_from commit' -l commit-time -d 'Manually set a commit time using commit date format'
complete -f -c bzr -n '__fish_seen_subcommand_from commit' -l unchanged -d 'Commit even if nothing has changed'
complete -f -c bzr -n '__fish_seen_subcommand_from commit' -l fixes -d 'Mark a bug as being fixed by this revision'
complete -f -c bzr -n '__fish_seen_subcommand_from commit' -l strict -d 'Refuse to commit if there are unknown files in the working tree'
complete -f -c bzr -n '__fish_seen_subcommand_from commit' -l lossy -d 'When committing to a foreign vcs do not push data that can not be natively represented'
complete -f -c bzr -n '__fish_seen_subcommand_from commit' -l local -d 'Perform a local commit in a bound branch (pushed to master on normal commit)'

# send command
complete -f -c bzr -n __fish_use_subcommand -a send -d 'Send changes via email'
complete -f -c bzr -n '__fish_seen_subcommand_from send' -l body -d 'Body for the email'
complete -f -c bzr -n '__fish_seen_subcommand_from send' -l remember -d 'Remember submit and public branch'
complete -f -c bzr -n '__fish_seen_subcommand_from send' -l mail-to -d 'Mail the request to this address'
complete -f -c bzr -n '__fish_seen_subcommand_from send' -l format -d 'Use the specified output format'
complete -f -c bzr -n '__fish_seen_subcommand_from send' -l no-bundle -d 'Do not include a bundle in the merge directive'
complete -f -c bzr -n '__fish_seen_subcommand_from send' -l strict -d 'Refuse to send if there are uncommitted changes in the working tree'
complete -f -c bzr -n '__fish_seen_subcommand_from send' -l no-patch -d 'Do not include a preview patch in the merge directive'

# log command
complete -f -c bzr -n __fish_use_subcommand -a log -d 'Show history of changes'

# check command
complete -f -c bzr -n __fish_use_subcommand -a check -d 'Validate storage'
complete -f -c bzr -n '__fish_seen_subcommand_from check' -l tree -d 'Check the working tree related to the current directory'
complete -f -c bzr -n '__fish_seen_subcommand_from check' -l repo -d 'Check the repository related to the current directory'
complete -f -c bzr -n '__fish_seen_subcommand_from check' -l branch -d 'Check the branch related to the current directory'

# Common long/short options 
set -l $cmds init branch add ignore mv status diff merge commit send log check
complete -f -c bzr -n '__fish_seen_subcommand_from $cmds' -l usage -d 'Show usage message and options'
complete -f -c bzr -n '__fish_seen_subcommand_from $cmds' -s h -l help -d 'Show help message'
complete -f -c bzr -n '__fish_seen_subcommand_from $cmds' -s q -l quiet -d 'Only displays errors and warnings'
complete -f -c bzr -n '__fish_seen_subcommand_from $cmds' -s v -l verbose -d 'Display more information'

# Commands with dry-run option
complete -f -c bzr -n '__fish_seen_subcommand_from add mv' -l dry-run -d 'Show what would be done'
