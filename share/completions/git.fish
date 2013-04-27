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
    set -l aliased (command git config --get "alias.$cmd[2]" | sed 's/ .*$//')
    if [ $argv[1] = "$aliased" ]
      return 0
    end
  end
  return 1
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
# TODO options

### checkout
complete -f -c git -n '__fish_git_needs_command'    -a checkout -d 'Checkout and switch to a branch'
complete -f -c git -n '__fish_git_using_command checkout'  -a '(__fish_git_branches)' --description 'Branch'
complete -f -c git -n '__fish_git_using_command checkout'  -a '(__fish_git_tags)' --description 'Tag'
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
complete -f -c git -n '__fish_git_using_command merge' -l stat -d "Show diffstat of the merge"
complete -f -c git -n '__fish_git_using_command merge' -s n -l no-stat -d "Don't show diffstat of the merge"
complete -f -c git -n '__fish_git_using_command merge' -l squash -d "Squash changes from other branch as a single commit"
complete -f -c git -n '__fish_git_using_command merge' -l no-squash -d "Don't squash changes"
complete -f -c git -n '__fish_git_using_command merge' -l ff -d "Don't generate a merge commit if merge is fast forward"
complete -f -c git -n '__fish_git_using_command merge' -l no-ff -d "Generate a merge commit even if merge is fast forward"

# TODO options

### mv
complete -c git -n '__fish_git_needs_command'    -a mv -d 'Move or rename a file, a directory, or a symlink'
# TODO options

### prune
complete -f -c git -n '__fish_git_needs_command' -a prune -d 'Prune all unreachable objects from the object database'
# TODO options

### pull
complete -f -c git -n '__fish_git_needs_command' -a pull -d 'Fetch from and merge with another repository or a local branch'
# TODO options

### push
complete -f -c git -n '__fish_git_needs_command' -a push -d 'Update remote refs along with associated objects'
# TODO options

### rebase
complete -f -c git -n '__fish_git_needs_command' -a rebase -d 'Forward-port local commits to the updated upstream head'
complete -f -c git -n '__fish_git_using_command rebase' -a '(__fish_git_branches)' -d 'Branch'
# TODO options

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
complete -c git -n '__fish_git_using_command rm' -s q -l quiet -d 'Suppress the output'
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
complete -f -c git -n '__fish_git_using_command tag' -s d -d 'Remove a tag'
complete -f -c git -n '__fish_git_using_command tag' -s v -d 'Verify signature of a tag'
complete -f -c git -n '__fish_git_using_command tag' -s f -d 'Force overwriting exising tag'
complete -f -c git -n '__fish_git_using_command tag' -s s -d 'Make a GPG-signed tag'
complete -f -c git -n '__fish_contains_opt -s d' -a '(__fish_git_tags)' -d 'Tag'
complete -f -c git -n '__fish_contains_opt -s v' -a '(__fish_git_tags)' -d 'Tag'
# TODO options

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
complete -c git -n '__fish_git_needs_command' -a '(command git config --get-regexp alias | sed "s/^alias\.\([^ ]*\).*/\1/")' -d 'Alias (user-defined command)'
