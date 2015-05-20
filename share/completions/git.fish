# fish completion for git
# Use 'command git' to avoid interactions for aliases from git to (e.g.) hub

function __fish_git_branches
  command git branch --no-color -a ^/dev/null | sgrep -v ' -> ' | sed -e 's/^..//' -e 's/^remotes\///'
end

function __fish_git_tags
  command git tag
end

function __fish_git_heads
  __fish_git_branches
  __fish_git_tags
end

function __fish_git_remotes
  command git remote
end

function __fish_git_modified_files
  command git ls-files -m --exclude-standard ^/dev/null
end

function __fish_git_add_files
  command git ls-files -mo --exclude-standard ^/dev/null
end

function __fish_git_ranges
  set -l from (commandline -ot | perl -ne 'if (index($_, "..") > 0) { my @parts = split(/\.\./); print $parts[0]; }')
  if test -z "$from"
    __fish_git_branches
    return 0
  end

  set -l to (commandline -ot | perl -ne 'if (index($_, "..") > 0) { my @parts = split(/\.\./); print $parts[1]; }')
  for from_ref in (__fish_git_heads | sgrep -e "$from")
    for to_ref in (__fish_git_heads | sgrep -e "$to")
      printf "%s..%s\n" $from_ref $to_ref
    end
  end
end

function __fish_git_needs_command
  set cmd (commandline -opc)
  if [ (count $cmd) -eq 1 -a $cmd[1] = 'git' ]
    return 0
  end
  return 1
end

function __fish_git_using_command
  set cmd (commandline -opc)
  if [ (count $cmd) -gt 1 ]
    if [ $argv[1] = $cmd[2] ]
      return 0
    end

    # aliased command
    set -l aliased (command git config --get "alias.$cmd[2]" ^ /dev/null | sed 's/ .*$//')
    if [ $argv[1] = "$aliased" ]
      return 0
    end
  end
  return 1
end

function __fish_git_stash_using_command
  set cmd (commandline -opc)
  if [ (count $cmd) -gt 2 ]
    if [ $cmd[2] = 'stash' -a $argv[1] = $cmd[3] ]
      return 0
    end
  end
  return 1
end

function __fish_git_stash_not_using_subcommand
  set cmd (commandline -opc)
  if [ (count $cmd) -gt 2 -a $cmd[2] = 'stash' ]
    return 1
  end
  return 0
end

function __fish_git_complete_stashes
    set -l IFS ':'
    command git stash list --format=%gd:%gs | while read -l name desc
        echo $name\t$desc
    end
end

function __fish_git_aliases
    set -l IFS \n
    command git config -z --get-regexp '^alias\.' | while read -lz key value
        begin
            set -l IFS "."
            echo -n $key | read -l _ name
            echo $name
        end
    end
end

function __fish_git_custom_commands
    # complete all commands starting with git-
    # however, a few builtin commands are placed into $PATH by git because
    # they're used by the ssh transport. We could filter them out by checking
    # if any of these completion results match the name of the builtin git commands,
    # but it's simpler just to blacklist these names. They're unlikely to change,
    # and the failure mode is we accidentally complete a plumbing command.
    set -l IFS \n
    for name in (builtin complete -Cgit- | sed 's/^git-\([^[:space:]]*\).*/\1/')
        switch $name
            case cvsserver receive-pack shell upload-archive upload-pack
                # skip these
            case \*
                echo $name
        end
    end
end

# general options
complete -f -c git -n 'not __fish_git_needs_command' -l help -d 'Display the manual of a git command'

#### fetch
complete -f -c git -n '__fish_git_needs_command' -a fetch -d 'Download objects and refs from another repository'
complete -f -c git -n '__fish_git_using_command fetch' -a '(__fish_git_remotes)' -d 'Remote'
complete -f -c git -n '__fish_git_using_command fetch' -s q -l quiet -d 'Be quiet'
complete -f -c git -n '__fish_git_using_command fetch' -s v -l verbose -d 'Be verbose'
complete -f -c git -n '__fish_git_using_command fetch' -s a -l append -d 'Append ref names and object names'
# TODO --upload-pack
complete -f -c git -n '__fish_git_using_command fetch' -s f -l force -d 'Force update of local branches'
# TODO other options

#### filter-branch
complete -f -c git -n '__fish_git_needs_command' -a filter-branch -d 'Rewrite branches'
complete -f -c git -n '__fish_git_using_command filter-branch' -l env-filter -d 'This filter may be used if you only need to modify the environment'
complete -f -c git -n '__fish_git_using_command filter-branch' -l tree-filter -d 'This is the filter for rewriting the tree and its contents.'
complete -f -c git -n '__fish_git_using_command filter-branch' -l index-filter -d 'This is the filter for rewriting the index.'
complete -f -c git -n '__fish_git_using_command filter-branch' -l parent-filter -d 'This is the filter for rewriting the commit\\(cqs parent list.'
complete -f -c git -n '__fish_git_using_command filter-branch' -l msg-filter -d 'This is the filter for rewriting the commit messages.'
complete -f -c git -n '__fish_git_using_command filter-branch' -l commit-filter -d 'This is the filter for performing the commit.'
complete -f -c git -n '__fish_git_using_command filter-branch' -l tag-name-filter -d 'This is the filter for rewriting tag names.'
complete -f -c git -n '__fish_git_using_command filter-branch' -l subdirectory-filter -d 'Only look at the history which touches the given subdirectory.'
complete -f -c git -n '__fish_git_using_command filter-branch' -l prune-empty -d 'Ignore empty commits generated by filters'
complete -f -c git -n '__fish_git_using_command filter-branch' -l original -d 'Use this option to set the namespace where the original commits will be stored'
complete -r -c git -n '__fish_git_using_command filter-branch' -s d -d 'Use this option to set the path to the temporary directory used for rewriting'
complete -c git -n '__fish_git_using_command filter-branch' -s f -l force -d 'Force filter branch to start even w/ refs in refs/original or existing temp directory'

### remote
complete -f -c git -n '__fish_git_needs_command' -a remote -d 'Manage set of tracked repositories'
complete -f -c git -n '__fish_git_using_command remote' -a '(__fish_git_remotes)'
complete -f -c git -n '__fish_git_using_command remote' -s v -l verbose -d 'Be verbose'
complete -f -c git -n '__fish_git_using_command remote' -a add -d 'Adds a new remote'
complete -f -c git -n '__fish_git_using_command remote' -a rm -d 'Removes a remote'
complete -f -c git -n '__fish_git_using_command remote' -a show -d 'Shows a remote'
complete -f -c git -n '__fish_git_using_command remote' -a prune -d 'Deletes all stale tracking branches'
complete -f -c git -n '__fish_git_using_command remote' -a update -d 'Fetches updates'
# TODO options

### show
complete -f -c git -n '__fish_git_needs_command' -a show -d 'Shows the last commit of a branch'
complete -f -c git -n '__fish_git_using_command show' -a '(__fish_git_branches)' -d 'Branch'
# TODO options

### show-branch
complete -f -c git -n '__fish_git_needs_command' -a show-branch -d 'Shows the commits on branches'
complete -f -c git -n '__fish_git_using_command show-branch' -a '(__fish_git_heads)' --description 'Branch'
# TODO options

### add
complete -c git -n '__fish_git_needs_command'    -a add -d 'Add file contents to the index'
complete -c git -n '__fish_git_using_command add' -s n -l dry-run -d "Don't actually add the file(s)"
complete -c git -n '__fish_git_using_command add' -s v -l verbose -d 'Be verbose'
complete -c git -n '__fish_git_using_command add' -s f -l force -d 'Allow adding otherwise ignored files'
complete -c git -n '__fish_git_using_command add' -s i -l interactive -d 'Interactive mode'
complete -c git -n '__fish_git_using_command add' -s p -l patch -d 'Interactively choose hunks to stage'
complete -c git -n '__fish_git_using_command add' -s e -l edit -d 'Manually create a patch'
complete -c git -n '__fish_git_using_command add' -s u -l update -d 'Only match tracked files'
complete -c git -n '__fish_git_using_command add' -s A -l all -d 'Match files both in working tree and index'
complete -c git -n '__fish_git_using_command add' -s N -l intent-to-add -d 'Record only the fact that the path will be added later'
complete -c git -n '__fish_git_using_command add' -l refresh -d "Don't add the file(s), but only refresh their stat"
complete -c git -n '__fish_git_using_command add' -l ignore-errors -d 'Ignore errors'
complete -c git -n '__fish_git_using_command add' -l ignore-missing -d 'Check if any of the given files would be ignored'
complete -f -c git -n '__fish_git_using_command add; and __fish_contains_opt -s p patch' -a '(__fish_git_modified_files)'
complete -f -c git -n '__fish_git_using_command add' -a '(__fish_git_add_files)'
# TODO options

### checkout
complete -f -c git -n '__fish_git_needs_command'    -a checkout -d 'Checkout and switch to a branch'
complete -f -c git -n '__fish_git_using_command checkout'  -a '(__fish_git_branches)' --description 'Branch'
complete -f -c git -n '__fish_git_using_command checkout'  -a '(__fish_git_tags)' --description 'Tag'
complete -f -c git -n '__fish_git_using_command checkout' -a '(__fish_git_modified_files)' --description 'File'
complete -f -c git -n '__fish_git_using_command checkout' -s b -d 'Create a new branch'
complete -f -c git -n '__fish_git_using_command checkout' -s t -l track -d 'Track a new branch'
# TODO options

### apply
complete -f -c git -n '__fish_git_needs_command' -a apply -d 'Apply a patch on a git index file and a working tree'
# TODO options

### archive
complete -f -c git -n '__fish_git_needs_command' -a archive -d 'Create an archive of files from a named tree'
# TODO options

### bisect
complete -f -c git -n '__fish_git_needs_command' -a bisect -d 'Find the change that introduced a bug by binary search'
# TODO options

### branch
complete -f -c git -n '__fish_git_needs_command' -a branch -d 'List, create, or delete branches'
complete -f -c git -n '__fish_git_using_command branch' -a '(__fish_git_branches)' -d 'Branch'
complete -f -c git -n '__fish_git_using_command branch' -s d -d 'Delete branch'
complete -f -c git -n '__fish_git_using_command branch' -s D -d 'Force deletion of branch'
complete -f -c git -n '__fish_git_using_command branch' -s m -d 'Rename branch'
complete -f -c git -n '__fish_git_using_command branch' -s M -d 'Force renaming branch'
complete -f -c git -n '__fish_git_using_command branch' -s a -d 'Lists both local and remote branches'
complete -f -c git -n '__fish_git_using_command branch' -s t -l track -d 'Track remote branch'
complete -f -c git -n '__fish_git_using_command branch' -l no-track -d 'Do not track remote branch'
complete -f -c git -n '__fish_git_using_command branch' -l set-upstream -d 'Set remote branch to track'
complete -f -c git -n '__fish_git_using_command branch' -l merged -d 'List branches that have been merged'
complete -f -c git -n '__fish_git_using_command branch' -l no-merged -d 'List branches that have not been merged'

### cherry-pick
complete -f -c git -n '__fish_git_needs_command' -a cherry-pick -d 'Apply the change introduced by an existing commit'
complete -f -c git -n '__fish_git_using_command cherry-pick' -a '(__fish_git_branches)' -d 'Branch'
# TODO options

### clone
complete -f -c git -n '__fish_git_needs_command' -a clone -d 'Clone a repository into a new directory'
# TODO options

### commit
complete -c git -n '__fish_git_needs_command'    -a commit -d 'Record changes to the repository'
complete -c git -n '__fish_git_using_command commit' -l amend -d 'Amend the log message of the last commit'
# TODO options

### diff
complete -c git -n '__fish_git_needs_command'    -a diff -d 'Show changes between commits, commit and working tree, etc'
complete -c git -n '__fish_git_using_command diff' -a '(__fish_git_ranges)' -d 'Branch'
complete -c git -n '__fish_git_using_command diff' -l cached -d 'Show diff of changes in the index'
# TODO options

### difftool
complete -c git -n '__fish_git_needs_command'    -a difftool -d 'Open diffs in a visual tool'
complete -c git -n '__fish_git_using_command difftool' -a '(__fish_git_ranges)' -d 'Branch'
complete -c git -n '__fish_git_using_command difftool' -l cached -d 'Visually show diff of changes in the index'
# TODO options


### grep
complete -c git -n '__fish_git_needs_command'    -a grep -d 'Print lines matching a pattern'
# TODO options

### init
complete -f -c git -n '__fish_git_needs_command' -a init -d 'Create an empty git repository or reinitialize an existing one'
# TODO options

### log
complete -c git -n '__fish_git_needs_command'    -a log -d 'Show commit logs'
complete -c git -n '__fish_git_using_command log' -a '(__fish_git_heads) (__fish_git_ranges)' -d 'Branch'
complete -f -c git -n '__fish_git_using_command log' -l pretty -a 'oneline short medium full fuller email raw format:'
# TODO options

### merge
complete -f -c git -n '__fish_git_needs_command' -a merge -d 'Join two or more development histories together'
complete -f -c git -n '__fish_git_using_command merge' -a '(__fish_git_branches)' -d 'Branch'
complete -f -c git -n '__fish_git_using_command merge' -l commit -d "Autocommit the merge"
complete -f -c git -n '__fish_git_using_command merge' -l no-commit -d "Don't autocommit the merge"
complete -f -c git -n '__fish_git_using_command merge' -l edit -d 'Edit auto-generated merge message'
complete -f -c git -n '__fish_git_using_command merge' -l no-edit -d "Don't edit auto-generated merge message"
complete -f -c git -n '__fish_git_using_command merge' -l ff -d "Don't generate a merge commit if merge is fast-forward"
complete -f -c git -n '__fish_git_using_command merge' -l no-ff -d "Generate a merge commit even if merge is fast-forward"
complete -f -c git -n '__fish_git_using_command merge' -l ff-only -d 'Refuse to merge unless fast-forward possible'
complete -f -c git -n '__fish_git_using_command merge' -l log -d 'Populate the log message with one-line descriptions'
complete -f -c git -n '__fish_git_using_command merge' -l no-log -d "Don't populate the log message with one-line descriptions"
complete -f -c git -n '__fish_git_using_command merge' -l stat -d "Show diffstat of the merge"
complete -f -c git -n '__fish_git_using_command merge' -s n -l no-stat -d "Don't show diffstat of the merge"
complete -f -c git -n '__fish_git_using_command merge' -l squash -d "Squash changes from other branch as a single commit"
complete -f -c git -n '__fish_git_using_command merge' -l no-squash -d "Don't squash changes"
complete -f -c git -n '__fish_git_using_command merge' -s q -l quiet -d 'Be quiet'
complete -f -c git -n '__fish_git_using_command merge' -s v -l verbose -d 'Be verbose'
complete -f -c git -n '__fish_git_using_command merge' -l progress -d 'Force progress status'
complete -f -c git -n '__fish_git_using_command merge' -l no-progress -d 'Force no progress status'
complete -f -c git -n '__fish_git_using_command merge' -s m -d 'Set the commit message'
complete -f -c git -n '__fish_git_using_command merge' -l abort -d 'Abort the current conflict resolution process'

# TODO options

### mv
complete -c git -n '__fish_git_needs_command'    -a mv -d 'Move or rename a file, a directory, or a symlink'
# TODO options

### prune
complete -f -c git -n '__fish_git_needs_command' -a prune -d 'Prune all unreachable objects from the object database'
# TODO options

### pull
complete -f -c git -n '__fish_git_needs_command' -a pull -d 'Fetch from and merge with another repository or a local branch'
complete -f -c git -n '__fish_git_using_command pull' -s q -l quiet -d 'Be quiet'
complete -f -c git -n '__fish_git_using_command pull' -s v -l verbose -d 'Be verbose'
# Options related to fetching
complete -f -c git -n '__fish_git_using_command pull' -l all -d 'Fetch all remotes'
complete -f -c git -n '__fish_git_using_command pull' -s a -l append -d 'Append ref names and object names'
complete -f -c git -n '__fish_git_using_command pull' -s f -l force -d 'Force update of local branches'
complete -f -c git -n '__fish_git_using_command pull' -s k -l keep -d 'Keep downloaded pack'
complete -f -c git -n '__fish_git_using_command pull' -l no-tags -d 'Disable automatic tag following'
# TODO --upload-pack
complete -f -c git -n '__fish_git_using_command pull' -l progress -d 'Force progress status'
complete -f -c git -n '__fish_git_using_command pull' -a '(git remote)' -d 'Remote alias'
complete -f -c git -n '__fish_git_using_command pull' -a '(__fish_git_branches)' -d 'Branch'
# TODO other options

### push
complete -f -c git -n '__fish_git_needs_command' -a push -d 'Update remote refs along with associated objects'
complete -f -c git -n '__fish_git_using_command push' -a '(git remote)' -d 'Remote alias'
complete -f -c git -n '__fish_git_using_command push' -a '(__fish_git_branches)' -d 'Branch'
complete -f -c git -n '__fish_git_using_command push' -l all -d 'Push all refs under refs/heads/'
complete -f -c git -n '__fish_git_using_command push' -l prune -d "Remove remote branches that don't have a local counterpart"
complete -f -c git -n '__fish_git_using_command push' -l mirror -d 'Push all refs under refs/'
complete -f -c git -n '__fish_git_using_command push' -l delete -d 'Delete all listed refs from the remote repository'
complete -f -c git -n '__fish_git_using_command push' -l tags -d 'Push all refs under refs/tags'
complete -f -c git -n '__fish_git_using_command push' -s n -l dry-run -d 'Do everything except actually send the updates'
complete -f -c git -n '__fish_git_using_command push' -l porcelain -d 'Produce machine-readable output'
complete -f -c git -n '__fish_git_using_command push' -s f -l force -d 'Force update of remote refs'
complete -f -c git -n '__fish_git_using_command push' -s u -l set-upstream -d 'Add upstream (tracking) reference'
complete -f -c git -n '__fish_git_using_command push' -s q -l quiet -d 'Be quiet'
complete -f -c git -n '__fish_git_using_command push' -s v -l verbose -d 'Be verbose'
complete -f -c git -n '__fish_git_using_command push' -l progress -d 'Force progress status'
# TODO --recurse-submodules=check|on-demand

### rebase
complete -f -c git -n '__fish_git_needs_command' -a rebase -d 'Forward-port local commits to the updated upstream head'
complete -f -c git -n '__fish_git_using_command rebase' -a '(git remote)' -d 'Remote alias'
complete -f -c git -n '__fish_git_using_command rebase' -a '(__fish_git_branches)' -d 'Branch'
complete -f -c git -n '__fish_git_using_command rebase' -l continue -d 'Restart the rebasing process'
complete -f -c git -n '__fish_git_using_command rebase' -l abort -d 'Abort the rebase operation'
complete -f -c git -n '__fish_git_using_command rebase' -l keep-empty -d "Keep the commits that don't cahnge anything"
complete -f -c git -n '__fish_git_using_command rebase' -l skip -d 'Restart the rebasing process by skipping the current patch'
complete -f -c git -n '__fish_git_using_command rebase' -s m -l merge -d 'Use merging strategies to rebase'
complete -f -c git -n '__fish_git_using_command rebase' -s q -l quiet -d 'Be quiet'
complete -f -c git -n '__fish_git_using_command rebase' -s v -l verbose -d 'Be verbose'
complete -f -c git -n '__fish_git_using_command rebase' -l stat -d "Show diffstat of the rebase"
complete -f -c git -n '__fish_git_using_command rebase' -s n -l no-stat -d "Don't show diffstat of the rebase"
complete -f -c git -n '__fish_git_using_command rebase' -l verify -d "Allow the pre-rebase hook to run"
complete -f -c git -n '__fish_git_using_command rebase' -l no-verify -d "Don't allow the pre-rebase hook to run"
complete -f -c git -n '__fish_git_using_command rebase' -s f -l force-rebase -d 'Force the rebase'
complete -f -c git -n '__fish_git_using_command rebase' -s i -l interactive -d 'Interactive mode'
complete -f -c git -n '__fish_git_using_command rebase' -s p -l preserve-merges -d 'Try to recreate merges'
complete -f -c git -n '__fish_git_using_command rebase' -l root -d 'Rebase all reachable commits'
complete -f -c git -n '__fish_git_using_command rebase' -l autosquash -d 'Automatic squashing'
complete -f -c git -n '__fish_git_using_command rebase' -l no-autosquash -d 'No automatic squashing'
complete -f -c git -n '__fish_git_using_command rebase' -l no-ff -d 'No fast-forward'

### reset
complete -c git -n '__fish_git_needs_command'    -a reset -d 'Reset current HEAD to the specified state'
complete -f -c git -n '__fish_git_using_command reset' -l hard -d 'Reset files in working directory'
complete -c git -n '__fish_git_using_command reset' -a '(__fish_git_branches)'
# TODO options

### revert
complete -f -c git -n '__fish_git_needs_command' -a revert -d 'Revert an existing commit'
# TODO options

### rm
complete -c git -n '__fish_git_needs_command'    -a rm     -d 'Remove files from the working tree and from the index'
complete -c git -n '__fish_git_using_command rm' -f
complete -c git -n '__fish_git_using_command rm' -l cached -d 'Keep local copies'
complete -c git -n '__fish_git_using_command rm' -l ignore-unmatch -d 'Exit with a zero status even if no files matched'
complete -c git -n '__fish_git_using_command rm' -s r -d 'Allow recursive removal'
complete -c git -n '__fish_git_using_command rm' -s q -l quiet -d 'Be quiet'
complete -c git -n '__fish_git_using_command rm' -s f -l force -d 'Override the up-to-date check'
complete -c git -n '__fish_git_using_command rm' -s n -l dry-run -d 'Dry run'
# TODO options

### status
complete -f -c git -n '__fish_git_needs_command' -a status -d 'Show the working tree status'
complete -f -c git -n '__fish_git_using_command status' -s s -l short -d 'Give the output in the short-format'
complete -f -c git -n '__fish_git_using_command status' -s b -l branch -d 'Show the branch and tracking info even in short-format'
complete -f -c git -n '__fish_git_using_command status'      -l porcelain -d 'Give the output in a stable, easy-to-parse format'
complete -f -c git -n '__fish_git_using_command status' -s z -d 'Terminate entries with null character'
complete -f -c git -n '__fish_git_using_command status' -s u -l untracked-files -x -a 'no normal all' -d 'The untracked files handling mode'
complete -f -c git -n '__fish_git_using_command status' -l ignore-submodules -x -a 'none untracked dirty all' -d 'Ignore changes to submodules'
# TODO options

### tag
complete -f -c git -n '__fish_git_needs_command' -a tag -d 'Create, list, delete or verify a tag object signed with GPG'
complete -f -c git -n '__fish_git_using_command tag; and __fish_not_contain_opt -s d; and __fish_not_contain_opt -s v; and test (count (commandline -opc | sgrep -v -e \'^-\')) -eq 3' -a '(__fish_git_branches)' -d 'Branch'
complete -f -c git -n '__fish_git_using_command tag' -s a -l annotate -d 'Make an unsigned, annotated tag object'
complete -f -c git -n '__fish_git_using_command tag' -s s -l sign -d 'Make a GPG-signed tag'
complete -f -c git -n '__fish_git_using_command tag' -s d -l delete -d 'Remove a tag'
complete -f -c git -n '__fish_git_using_command tag' -s v -l verify -d 'Verify signature of a tag'
complete -f -c git -n '__fish_git_using_command tag' -s f -l force -d 'Force overwriting exising tag'
complete -f -c git -n '__fish_git_using_command tag' -s l -l list -d 'List tags'
complete -f -c git -n '__fish_contains_opt -s d' -a '(__fish_git_tags)' -d 'Tag'
complete -f -c git -n '__fish_contains_opt -s v' -a '(__fish_git_tags)' -d 'Tag'
# TODO options

### stash
complete -c git -n '__fish_git_needs_command' -a stash -d 'Stash away changes'
complete -f -c git -n '__fish_git_using_command stash; and __fish_git_stash_not_using_subcommand' -a list -d 'List stashes'
complete -f -c git -n '__fish_git_using_command stash; and __fish_git_stash_not_using_subcommand' -a show -d 'Show the changes recorded in the stash'
complete -f -c git -n '__fish_git_using_command stash; and __fish_git_stash_not_using_subcommand' -a pop -d 'Apply and remove a single stashed state'
complete -f -c git -n '__fish_git_using_command stash; and __fish_git_stash_not_using_subcommand' -a apply -d 'Apply a single stashed state'
complete -f -c git -n '__fish_git_using_command stash; and __fish_git_stash_not_using_subcommand' -a clear -d 'Remove all stashed states'
complete -f -c git -n '__fish_git_using_command stash; and __fish_git_stash_not_using_subcommand' -a drop -d 'Remove a single stashed state from the stash list'
complete -f -c git -n '__fish_git_using_command stash; and __fish_git_stash_not_using_subcommand' -a create -d 'Create a stash'
complete -f -c git -n '__fish_git_using_command stash; and __fish_git_stash_not_using_subcommand' -a save -d 'Save a new stash'
complete -f -c git -n '__fish_git_using_command stash; and __fish_git_stash_not_using_subcommand' -a branch -d 'Create a new branch from a stash'

complete -f -c git -n '__fish_git_stash_using_command apply' -a '(__fish_git_complete_stashes)'
complete -f -c git -n '__fish_git_stash_using_command branch' -a '(__fish_git_complete_stashes)'
complete -f -c git -n '__fish_git_stash_using_command drop' -a '(__fish_git_complete_stashes)'
complete -f -c git -n '__fish_git_stash_using_command pop' -a '(__fish_git_complete_stashes)'
complete -f -c git -n '__fish_git_stash_using_command show' -a '(__fish_git_complete_stashes)'

### config
complete -f -c git -n '__fish_git_needs_command' -a config -d 'Set and read git configuration variables'
# TODO options

### format-patch
complete -f -c git -n '__fish_git_needs_command' -a format-patch -d 'Generate patch series to send upstream'
complete -f -c git -n '__fish_git_using_command format-patch' -a '(__fish_git_branches)' -d 'Branch'

## git submodule
complete -f -c git -n '__fish_git_needs_command' -a submodule -d 'Initialize, update or inspect submodules'
complete -f -c git -n '__fish_git_using_command submodule' -a 'add' -d 'Add a submodule'
complete -f -c git -n '__fish_git_using_command submodule' -a 'status' -d 'Show submodule status'
complete -f -c git -n '__fish_git_using_command submodule' -a 'init' -d 'Initialize all submodules'
complete -f -c git -n '__fish_git_using_command submodule' -a 'update' -d 'Update all submodules'
complete -f -c git -n '__fish_git_using_command submodule' -a 'summary' -d 'Show commit summary'
complete -f -c git -n '__fish_git_using_command submodule' -a 'foreach' -d 'Run command on each submodule'
complete -f -c git -n '__fish_git_using_command submodule' -a 'sync' -d 'Sync submodules\' URL with .gitmodules'

## git whatchanged
complete -f -c git -n '__fish_git_needs_command' -a whatchanged -d 'Show logs with difference each commit introduces'

## Aliases (custom user-defined commands)
complete -c git -n '__fish_git_needs_command' -a '(__fish_git_aliases)' -d 'Alias (user-defined command)'

### git clean
complete -f -c git -n '__fish_git_needs_command' -a clean -d 'Remove untracked files from the working tree'
complete -f -c git -n '__fish_git_using_command clean' -s f -l force -d 'Force run'
complete -f -c git -n '__fish_git_using_command clean' -s i -l interactive -d 'Show what would be done and clean files interactively'
complete -f -c git -n '__fish_git_using_command clean' -s n -l dry-run -d 'Don\'t actually remove anything, just show what would be done'
complete -f -c git -n '__fish_git_using_command clean' -s q -l quite -d 'Be quiet, only report errors'
complete -f -c git -n '__fish_git_using_command clean' -s d -d 'Remove untracked directories in addition to untracked files'
complete -f -c git -n '__fish_git_using_command clean' -s x -d 'Remove ignored files, as well'
complete -f -c git -n '__fish_git_using_command clean' -s X -d 'Remove only ignored files'
# TODO -e option

## Custom commands (git-* commands installed in the PATH)
complete -c git -n '__fish_git_needs_command' -a '(__fish_git_custom_commands)' -d 'Custom command'
