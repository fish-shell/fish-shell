# fish completion for arc

function __fish_arc_needs_command
    set -l cmd (commandline -xpc)
    if not set -q cmd[2]
        return 0
    else
        set -l skip_next 1
        # Skip first word because it's "arc" or a wrapper
        for c in $cmd[2..-1]
            switch $c
                # General options that can still take a command
                case --trace --no-ansi --ansi --load-phutil-library config skip-arcconfig arcrc-file --conduit-{uri,token,version,timeout}
                    continue
                case "*"
                    echo $c
                    return 1
            end
        end
        return 0
    end
    return 1
end

function __fish_arc_using_command
    set -l cmd (__fish_arc_needs_command)
    test -z "$cmd"
    and return 1
    contains -- $cmd $argv
    and return 0
end

### Global options
complete -f -c arc -n __fish_arc_needs_command -l trace -d 'Debugging command'
complete -f -c arc -n __fish_arc_needs_command -l no-ansi -d 'Don\'t use color or style for output'
complete -f -c arc -n __fish_arc_needs_command -l ansi -d 'Always use fromatting'
complete -f -c arc -n __fish_arc_needs_command -l no-ansi -d 'Don\'t use color or style for output'
complete -f -c arc -n __fish_arc_needs_command -l load-phutil-library -d 'Specify which libraies to load'
complete -f -c arc -n __fish_arc_needs_command -l conduit-uri -d 'Specify the Conduit URI'
complete -f -c arc -n __fish_arc_needs_command -l conduit-token -d 'Specify the Conduit token'
complete -f -c arc -n __fish_arc_needs_command -l conduit-version -d 'Force a version'
complete -f -c arc -n __fish_arc_needs_command -l conduit-timeout -d 'Sets the timeout'
complete -f -c arc -n __fish_arc_needs_command -l config -d 'Sets a config'
complete -f -c arc -n __fish_arc_needs_command -l skip-arcconfig -d 'Skip the working copy configuration file'
complete -c arc -n __fish_arc_needs_command -l arcrc-file -d 'Use provided file instead of ~/.arcrc'

### shell-complete
complete -f -c arc -n __fish_arc_needs_command -a shell-complete -d 'Implements shell completion'
complete -f -c arc -n '__fish_arc_using_command shell-complete' -l current -d 'Current term in the argument list being completed'

### get-config
complete -f -c arc -n __fish_arc_needs_command -a get-config -d 'Reads an arc configuration option'
complete -f -c arc -n '__fish_arc_using_command get-config' -l verbose -d 'Show detailed information about options'

### download
complete -f -c arc -n __fish_arc_needs_command -a download -d 'Download a file to local disk'
complete -c arc -n '__fish_arc_using_command download' -l as -d 'Save the file with a specific name rather than the default'
complete -f -c arc -n '__fish_arc_using_command download' -l show -d 'Write file to stdout instead of to disk'

### list
complete -f -c arc -n __fish_arc_needs_command -a list -d 'List your open Differential revisions'

### lint
complete -f -c arc -n __fish_arc_needs_command -a lint -d 'Run static analysis on changes to check for mistakes'
complete -f -c arc -n '__fish_arc_using_command lint' -l engine -d 'Override configured lint engine for this project'
complete -f -c arc -n '__fish_arc_using_command lint' -l apply-patches -d 'Apply patches suggested by lint to the working copy'
complete -f -c arc -n '__fish_arc_using_command lint' -l severity -d 'Set minimum message severity'
complete -f -c arc -n '__fish_arc_using_command lint' -l never-apply-patches -d 'Never apply patches suggested by lint'
complete -f -c arc -n '__fish_arc_using_command lint' -l rev -d 'Lint changes since a specific revision'
complete -c arc -n '__fish_arc_using_command lint' -l outfile -d 'Output the linter results to a file'
complete -f -c arc -n '__fish_arc_using_command lint' -l lintall -d 'Show all lint warnings, not just those on changed lines'
complete -f -c arc -n '__fish_arc_using_command lint' -l amend-all -d 'When linting git repositories, amend HEAD with all patches'
complete -f -c arc -n '__fish_arc_using_command lint' -l everything -d 'Lint all files in the project'
complete -f -c arc -n '__fish_arc_using_command lint' -l output -d 'Specify how results will be displayed'
complete -f -c arc -n '__fish_arc_using_command lint' -l only-new -d 'Display only messages not present in the original code'
complete -f -c arc -n '__fish_arc_using_command lint' -l only-changed -d 'Show lint warnings just on changed lines'
complete -f -c arc -n '__fish_arc_using_command lint' -l amend-autofixes -d 'When linting git repositories, amend HEAD with autofix'

### flag
complete -f -c arc -n __fish_arc_needs_command -a flag -d 'In the first form, list objects you\'ve flagged'
complete -f -c arc -n '__fish_arc_using_command flag' -l edit -d 'Edit the flag on an object'
complete -f -c arc -n '__fish_arc_using_command flag' -l color -d 'Set the color of a flag'
complete -f -c arc -n '__fish_arc_using_command flag' -l clear -d 'Delete the flag on an object'
complete -f -c arc -n '__fish_arc_using_command flag' -l note -d 'Set the note on a flag'

### export
complete -c arc -n __fish_arc_needs_command -a export -d 'Export the local changeset to a file'
complete -f -c arc -n '__fish_arc_using_command export' -l unified -d 'Export change as a unified patch'
complete -f -c arc -n '__fish_arc_using_command export' -l git -d 'Export change as a git patch'
complete -f -c arc -n '__fish_arc_using_command export' -l encoding -d 'Attempt to convert non UTF-8 patch into specified encoding'
complete -f -c arc -n '__fish_arc_using_command export' -l arcbundle -d 'Export change as an arc bundle'
complete -f -c arc -n '__fish_arc_using_command export' -l diff -d 'Export from Differential diff'
complete -f -c arc -n '__fish_arc_using_command export' -l revision -d 'Export from a Differential revision'

### browse
complete -c arc -n __fish_arc_needs_command -a browse -d 'Open a file or object in your web browser'
complete -f -c arc -n '__fish_arc_using_command browse' -l force -d 'Open arguments as paths, even if they do not exist in the working copy'
complete -f -c arc -n '__fish_arc_using_command browse' -l branch -d 'Default branch name to view on server'

### todo
complete -f -c arc -n __fish_arc_needs_command -a todo -d 'Quickly create a task for yourself'
complete -f -c arc -n '__fish_arc_using_command todo' -l cc -d 'Other users to CC on the new task'
complete -f -c arc -n '__fish_arc_using_command todo' -l project -d 'Projects to assign to the task'
complete -f -c arc -n '__fish_arc_using_command todo' -l browse -d 'After creating the task, open it in a web browser'

### linters
complete -f -c arc -n __fish_arc_needs_command -a linters -d 'what they do and which versions are installed'
complete -f -c arc -n '__fish_arc_using_command linters' -l search -d 'Search for linters'
complete -f -c arc -n '__fish_arc_using_command linters' -l verbose -d 'Show detailed information, including options'

### time
complete -f -c arc -n __fish_arc_needs_command -a time -d 'Show what you\'re currently tracking in Phrequent'

### stop
complete -f -c arc -n __fish_arc_needs_command -a stop -d 'Stop tracking work in Phrequent'
complete -f -c arc -n '__fish_arc_using_command stop' -l note -d 'A note to attach to the tracked time'

### alias
complete -f -c arc -n __fish_arc_needs_command -a alias -d 'Create an alias'

### set-config
complete -f -c arc -n __fish_arc_needs_command -a set-config -d 'Sets an arc configuration option'
complete -f -c arc -n '__fish_arc_using_command set-config' -l local -d 'Set a local config value instead of a user one'

### start
complete -f -c arc -n __fish_arc_needs_command -a start -d 'Start tracking work in Phrequent'

### close
complete -f -c arc -n __fish_arc_needs_command -a close -d 'Close a task or otherwise update its status'
complete -f -c arc -n '__fish_arc_using_command close' -l message -d 'Provide a comment with your status change'
complete -f -c arc -n '__fish_arc_using_command close' -l list-statuses -d 'Show available status options and exit'

### land
complete -f -c arc -n __fish_arc_needs_command -a land -d 'Publish an accepted revision after review'
complete -f -c arc -n '__fish_arc_using_command land' -l preview -d 'Prints the commits that would be landed'
complete -f -c arc -n '__fish_arc_using_command land' -l remote -d 'Push to a remote other than the default'
complete -f -c arc -n '__fish_arc_using_command land' -l delete-remote -d 'Delete the feature branch in the remote after landing it'
complete -f -c arc -n '__fish_arc_using_command land' -l update-with-rebase -d 'When updating the feature branch, use rebase instead of merge'
complete -f -c arc -n '__fish_arc_using_command land' -l squash -d 'Use squash strategy'
complete -f -c arc -n '__fish_arc_using_command land' -l keep-branch -d 'Keep the feature branch'
complete -f -c arc -n '__fish_arc_using_command land' -l merge -d 'Use merge strategy'
complete -f -c arc -n '__fish_arc_using_command land' -l update-with-merge -d 'When updating the feature branch, use merge instead of rebase'
complete -f -c arc -n '__fish_arc_using_command land' -l hold -d 'Prepare the change to be pushed, but do not actually push it'
complete -f -c arc -n '__fish_arc_using_command land' -l onto -d 'Land feature branch onto a branch other than the default'
complete -f -c arc -n '__fish_arc_using_command land' -l revision -d 'Use the message from a specific revision'

### which
complete -f -c arc -n __fish_arc_needs_command -a which -d 'Show which commits will be selected'
complete -f -c arc -n '__fish_arc_using_command which' -l show-base -d 'Print base commit only and exit'
complete -f -c arc -n '__fish_arc_using_command which' -l base -d 'Additional rules for determining base revision'
complete -f -c arc -n '__fish_arc_using_command which' -l head -d 'Specify the end of the commit range to select'
complete -f -c arc -n '__fish_arc_using_command which' -l any-status -d 'Show committed and abandoned revisions'

### bookmark
complete -f -c arc -n __fish_arc_needs_command -a bookmark -d 'Alias for arc feature'

### amend
complete -f -c arc -n __fish_arc_needs_command -a amend -d 'Amend the working copy'
complete -f -c arc -n '__fish_arc_using_command amend' -l revision -d 'Use the message from a specific revision'
complete -f -c arc -n '__fish_arc_using_command amend' -l show -d 'Show the amended commit message'

### upgrade
complete -f -c arc -n __fish_arc_needs_command -a upgrade -d 'Upgrade arcanist and libphutil to the latest versions'

### help
complete -f -c arc -n __fish_arc_needs_command -a help -d 'Shows the help'
complete -f -c arc -n '__fish_arc_using_command help' -l full -d 'Print detailed information about each command'

### paste
complete -f -c arc -n __fish_arc_needs_command -a paste -d 'Share and grab text using the Paste application'
complete -f -c arc -n '__fish_arc_using_command paste' -l lang -d 'Language for syntax highlighting'
complete -f -c arc -n '__fish_arc_using_command paste' -l json -d 'Output in JSON format'
complete -f -c arc -n '__fish_arc_using_command paste' -l title -d 'Title for the paste'

### commit
complete -f -c arc -n __fish_arc_needs_command -a commit -d 'Commit a revision which has been accepted by a reviewer'
complete -f -c arc -n '__fish_arc_using_command commit' -l revision -d 'Commit a specific revision'
complete -f -c arc -n '__fish_arc_using_command commit' -l show -d 'Show the command which would be issued'

### patch
complete -f -c arc -n __fish_arc_needs_command -a patch -d 'Apply changes to the working copy'
complete -f -c arc -n '__fish_arc_using_command patch' -l force -d 'Do not run any sanity checks'
complete -f -c arc -n '__fish_arc_using_command patch' -l encoding -d 'Attempt to convert non UTF-8 patch into specified encoding'
complete -f -c arc -n '__fish_arc_using_command patch' -l nocommit -d 'Do not commit the changes'
complete -f -c arc -n '__fish_arc_using_command patch' -l update -d 'Update the local working copy before applying the patch'
complete -c arc -n '__fish_arc_using_command patch' -l patch -d 'Apply changes from a git patch file or unified patch file'
complete -f -c arc -n '__fish_arc_using_command patch' -l arcbundle -d 'Apply changes from an arc bundlej'
complete -f -c arc -n '__fish_arc_using_command patch' -l skip-dependencies -d 'Do not apply dependencies'
complete -f -c arc -n '__fish_arc_using_command patch' -l diff -d 'Apply changes from a Differential diff'
complete -f -c arc -n '__fish_arc_using_command patch' -l nobranch -d 'Do not create a branch'
complete -f -c arc -n '__fish_arc_using_command patch' -l revision -d 'Apply changes from a Differential revision'

### install-certificate
complete -f -c arc -n __fish_arc_needs_command -a install-certificate -d 'Installs Conduit credentials into your ~/.arcc'

### revert
complete -f -c arc -n __fish_arc_needs_command -a revert -d 'Please use backout instead'

### upload
complete -c arc -n __fish_arc_needs_command -a upload -d 'Upload a file from local disk'
complete -f -c arc -n '__fish_arc_using_command upload' -l json -d 'Output upload information in JSON format'
complete -f -c arc -n '__fish_arc_using_command upload' -l temporary -d 'Mark the file as temporary'

### branch
complete -f -c arc -n __fish_arc_needs_command -a branch -d 'Alias for arc feature'

### anoid
complete -f -c arc -n __fish_arc_needs_command -a anoid -d 'There\'s only one way to find out'

### cover
complete -f -c arc -n __fish_arc_needs_command -a cover -d 'Show blame for the lines you changed'
complete -f -c arc -n '__fish_arc_using_command cover' -l rev -d 'Cover changes since a specific revision'

### close-revision
complete -f -c arc -n __fish_arc_needs_command -a close-revision -d 'Close a revision'
complete -f -c arc -n '__fish_arc_using_command close-revision' -l quiet -d 'Do not print a success message'
complete -f -c arc -n '__fish_arc_using_command close-revision' -l finalize -d 'Close only if the repository is untracked and the revision is accepted'

### tasks
complete -f -c arc -n __fish_arc_needs_command -a tasks -d 'View all assigned tasks'
complete -f -c arc -n '__fish_arc_using_command tasks' -l status -d 'Show tasks that are open or closed, default is open'
complete -f -c arc -n '__fish_arc_using_command tasks' -l owner -d 'Only show tasks assigned to the given username,'
complete -f -c arc -n '__fish_arc_using_command tasks' -l unassigned -d 'Only show tasks that are not assigned'
complete -f -c arc -n '__fish_arc_using_command tasks' -l limit -d 'Limit the amount of tasks outputted, default is all'
complete -f -c arc -n '__fish_arc_using_command tasks' -l order -d 'Arrange tasks based on priority, created, or modified,'

### feature
complete -f -c arc -n __fish_arc_needs_command -a feature -d 'A wrapper on \'git branch\' or \'hg bookmark'
complete -f -c arc -n '__fish_arc_using_command feature' -l output -d 'Specify the output format'
complete -f -c arc -n '__fish_arc_using_command feature' -l view-all -d 'Include closed and abandoned revisions'
complete -f -c arc -n '__fish_arc_using_command feature' -l by-status -d 'Sort branches by status instead of time'

### unit
complete -f -c arc -n __fish_arc_needs_command -a unit -d 'Run unit tests that cover specified paths'
complete -f -c arc -n '__fish_arc_using_command unit' -l engine -d 'Override configured unit engine for this project'
complete -f -c arc -n '__fish_arc_using_command unit' -l detailed-coverage -d 'Show a detailed coverage report on the CLI'
complete -f -c arc -n '__fish_arc_using_command unit' -l target -d 'Record a copy of the test results on the specified build target'
complete -f -c arc -n '__fish_arc_using_command unit' -l ugly -d 'Use uglier formatting'
complete -f -c arc -n '__fish_arc_using_command unit' -l rev -d 'Run unit tests covering changes since a specific revision'
complete -f -c arc -n '__fish_arc_using_command unit' -l everything -d 'Run every test'
complete -f -c arc -n '__fish_arc_using_command unit' -l json -d 'Report results in JSON format'
complete -f -c arc -n '__fish_arc_using_command unit' -l coverage -d 'Always enable coverage information'
complete -f -c arc -n '__fish_arc_using_command unit' -l output -d 'Specify the output format'
complete -f -c arc -n '__fish_arc_using_command unit' -l no-coverage -d 'Always disable coverage information'

### backout
complete -f -c arc -n __fish_arc_needs_command -a backout -d 'Backouts on a previous commit'

### call-conduit
complete -f -c arc -n __fish_arc_needs_command -a call-conduit -d 'Make a raw Conduit method call'

### diff
complete -f -c arc -n __fish_arc_needs_command -a diff -d 'Generate a Differential diff or revision from local changes'
complete -f -c arc -n '__fish_arc_using_command diff' -l raw-command -d 'Generate diff by executing a specified command'
complete -f -c arc -n '__fish_arc_using_command diff' -l encoding -d 'Attempt to convert non UTF-8 hunks into specified encoding'
complete -f -c arc -n '__fish_arc_using_command diff' -l cc -d 'When creating a revision, add CCs'
complete -f -c arc -n '__fish_arc_using_command diff' -l reviewers -d 'When creating a revision, add reviewers'
complete -f -c arc -n '__fish_arc_using_command diff' -l skip-staging -d 'Do not copy changes to the staging area'
complete -f -c arc -n '__fish_arc_using_command diff' -l raw -d 'Read diff from stdin'
complete -f -c arc -n '__fish_arc_using_command diff' -l uncommitted -d 'Suppress warning about uncommitted changes'
complete -c arc -n '__fish_arc_using_command diff' -l message-file -d 'Read revision information from file'
complete -f -c arc -n '__fish_arc_using_command diff' -l nolint -d 'Do not run lint'
complete -f -c arc -n '__fish_arc_using_command diff' -l message -d 'Use the specified message when updating a revision'
complete -f -c arc -n '__fish_arc_using_command diff' -l plan-changes -d 'Create or update a revision without requesting a code review'
complete -f -c arc -n '__fish_arc_using_command diff' -l browse -d 'After creating a diff or revision, open it in a web browser'
complete -f -c arc -n '__fish_arc_using_command diff' -l create -d 'Always create a new revision'
complete -f -c arc -n '__fish_arc_using_command diff' -l cache -d 'Disable lint cache'
complete -f -c arc -n '__fish_arc_using_command diff' -l use-commit-message -d 'Read revision information from a specific commit'
complete -f -c arc -n '__fish_arc_using_command diff' -l only -d 'Only generate a diff, without running lint, unit tests, or other'
complete -f -c arc -n '__fish_arc_using_command diff' -l skip-binaries -d 'Do not upload binaries'
complete -f -c arc -n '__fish_arc_using_command diff' -l preview -d 'only create a diff'
complete -f -c arc -n '__fish_arc_using_command diff' -l amend-autofixes -d 'When linting git repositories, amend HEAD with autofix'
complete -f -c arc -n '__fish_arc_using_command diff' -l apply-patches -d 'Apply patches suggested by lint'
complete -f -c arc -n '__fish_arc_using_command diff' -l head -d 'Specify the end of the commit range'
complete -f -c arc -n '__fish_arc_using_command diff' -l verbatim -d 'When creating a revision, try to use the working copy commit'
complete -f -c arc -n '__fish_arc_using_command diff' -l less-context -d 'Create a diff with a few lines of context.'
complete -f -c arc -n '__fish_arc_using_command diff' -l advice -d 'Require excuse for lint advice in addition to lint warnings and errors'
complete -f -c arc -n '__fish_arc_using_command diff' -l json -d 'Emit machine-readable JSON'
complete -f -c arc -n '__fish_arc_using_command diff' -l update -d 'Always update a specific revision'
complete -f -c arc -n '__fish_arc_using_command diff' -l ignore-unsound-tests -d 'Ignore unsound test failures without prompting'
complete -f -c arc -n '__fish_arc_using_command diff' -l excuse -d 'Provide a prepared in advance excuse for any lints/tests'
complete -f -c arc -n '__fish_arc_using_command diff' -l base -d 'Additional rules for determining base revision'
complete -f -c arc -n '__fish_arc_using_command diff' -l no-amend -d 'Never amend commits in the working copy with lint patches'
complete -f -c arc -n '__fish_arc_using_command diff' -l add-all -d 'Automatically add all unstaged and uncommitted'
complete -f -c arc -n '__fish_arc_using_command diff' -l never-apply-patches -d 'Never apply patches suggested by lint'
complete -f -c arc -n '__fish_arc_using_command diff' -l edit -d 'Edit revision information'
complete -f -c arc -n '__fish_arc_using_command diff' -l nounit -d 'Do not run unit tests'
complete -f -c arc -n '__fish_arc_using_command diff' -l lintall -d 'Raise all lint warnings'
complete -f -c arc -n '__fish_arc_using_command diff' -l amend-all -d 'When linting git repositories, amend HEAD with all patches'
complete -f -c arc -n '__fish_arc_using_command diff' -l no-diff -d 'Only run lint and unit tests'
complete -f -c arc -n '__fish_arc_using_command diff' -l allow-untracked -d 'Skip checks for untracked files in the working copy'
complete -f -c arc -n '__fish_arc_using_command diff' -l only-new -d 'Display only new lint messages'
complete -f -c arc -n '__fish_arc_using_command diff' -l no-coverage -d 'Always disable coverage information'

### liberate
complete -f -c arc -n __fish_arc_needs_command -a liberate -d 'Create or update a libphutil library'
complete -f -c arc -n '__fish_arc_using_command liberate' -l remap -d 'Run the remap step of liberation'
complete -f -c arc -n '__fish_arc_using_command liberate' -l upgrade -d 'Upgrade library to v2'
complete -f -c arc -n '__fish_arc_using_command liberate' -l verify -d 'Run the verify step of liberation'
complete -f -c arc -n '__fish_arc_using_command liberate' -l all -d 'Drop the module cache before liberating'
complete -f -c arc -n '__fish_arc_using_command liberate' -l force-update -d 'Force the library map to be updated'
complete -f -c arc -n '__fish_arc_using_command liberate' -l library-name -d 'Set the library name'

### version
complete -f -c arc -n __fish_arc_needs_command -a version -d 'Shows the current version of arcanist'
