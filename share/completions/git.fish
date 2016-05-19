# fish completion for git
# Use 'command git' to avoid interactions for aliases from git to (e.g.) hub

function __fish_git_commits
    # Complete commits with their subject line as the description
    # This allows filtering by subject with the new pager!
    # Because even subject lines can be quite long,
    # trim them (abbrev'd hash+tab+subject) to 70 characters
    command git log --pretty=tformat:"%h"\t"%s" --all | string replace -r '(.{70}).+' '$1...'
end

function __fish_git_branches
    command git branch --no-color -a $argv ^/dev/null | string match -r -v ' -> ' | string trim -c "* " | string replace -r "^remotes/" ""
end

function __fish_git_unique_remote_branches
    # Allow all remote branches with one remote without the remote part
    # This is useful for `git checkout` to automatically create a remote-tracking branch
    command git branch --no-color -a $argv ^/dev/null | string match -r -v ' -> ' | string trim -c "* " | string replace -r "^remotes/[^/]*/" "" | sort | uniq -u
end

function __fish_git_tags
    command git tag ^/dev/null
end

function __fish_git_heads
    __fish_git_branches
    __fish_git_tags
end

function __fish_git_remotes
    command git remote ^/dev/null
end

function __fish_git_modified_files
    # git diff --name-only hands us filenames relative to the git toplevel
    set -l root (command git rev-parse --show-toplevel)
    # Print files from the current $PWD as-is, prepend all others with ":/" (relative to toplevel in git-speak)
    # This is a bit simplistic but finding the lowest common directory and then replacing everything else in $PWD with ".." is a bit annoying
    string replace -- "$PWD/" "" "$root/"(command git diff --name-only ^/dev/null) | string replace "$root/" ":/"
end

function __fish_git_staged_files
    set -l root (command git rev-parse --show-toplevel)
    string replace -- "$PWD/" "" "$root/"(command git diff --staged --name-only ^/dev/null) | string replace "$root/" ":/"
end

function __fish_git_add_files
    set -l root (command git rev-parse --show-toplevel)
    string replace -- "$PWD/" "" "$root/"(command git -C $root ls-files -mo --exclude-standard ^/dev/null) | string replace "$root/" ":/"
end

function __fish_git_ranges
    set -l both (commandline -ot | string split "..")
    set -l from $both[1]
    # If we didn't need to split (or there's nothing _to_ split), complete only the first part
    # Note that status here is from `string split` because `set` doesn't alter it
    if test -z "$from" -o $status -gt 0
        __fish_git_heads
        return 0
    end

    set -l to (set -q both[2]; and echo $both[2])
    for from_ref in (__fish_git_heads | string match "$from")
        for to_ref in (__fish_git_heads | string match "*$to*") # if $to is empty, this correctly matches everything
            printf "%s..%s\n" $from_ref $to_ref
        end
    end
end

function __fish_git_needs_command
    set cmd (commandline -opc)
    if [ (count $cmd) -eq 1 ]
        return 0
    else
        set -l skip_next 1
        # Skip first word because it's "git" or a wrapper
        for c in $cmd[2..-1]
            test $skip_next -eq 0
            and set skip_next 1
            and continue
            # git can only take a few options before a command, these are the ones mentioned in the "git" man page
            # e.g. `git --follow log` is wrong, `git --help log` is okay (and `git --help log $branch` is superfluous but works)
            # In case any other option is used before a command, we'll fail, but that's okay since it's invalid anyway
            switch $c
                # General options that can still take a command
                case "--help" "-p" "--paginate" "--no-pager" "--bare" "--no-replace-objects" --{literal,glob,noglob,icase}-pathspecs --{exec-path,git-dir,work-tree,namespace}"=*"
                    continue
                    # General options with an argument we need to skip. The option=value versions have already been handled above
                case --{exec-path,git-dir,work-tree,namespace}
                    set skip_next 0
                    continue
                    # General options that cause git to do something and exit - these behave like commands and everything after them is ignored
                case "--version" --{html,man,info}-path
                    return 1
                    # We assume that any other token that's not an argument to a general option is a command
                case "*"
                    echo $c
                    return 1
            end
        end
        return 0
    end
    return 1
end

function __fish_git_using_command
    set -l cmd (__fish_git_needs_command)
    test -z "$cmd"
    and return 1
    contains -- $cmd $argv
    and return 0

    # aliased command
    set -l aliased (command git config --get "alias.$cmd" ^/dev/null | string split " ")
    contains -- "$aliased[1]" $argv
    and return 0
    return 1
end

function __fish_git_stash_using_command
    set cmd (commandline -opc)
    __fish_git_using_command stash
    or return 2
    # The word after the stash command _must_ be the subcommand
    set cmd $cmd[(contains -i -- "stash" $cmd)..-1]
    set -e cmd[1]
    set -q cmd[1]
    or return 1
    contains -- $cmd[1] $argv
    and return 0
    return 1
end

function __fish_git_stash_not_using_subcommand
    set cmd (commandline -opc)
    __fish_git_using_command stash
    or return 2
    set cmd $cmd[(contains -i -- "stash" $cmd)..-1]
    set -q cmd[2]
    and return 1
    return 0
end

function __fish_git_complete_stashes
    command git stash list --format=%gd:%gs ^/dev/null | string replace ":" \t
end

function __fish_git_aliases
    command git config -z --get-regexp '^alias\.' ^/dev/null | while read -lz key value
        begin
            set -l name (string replace -r '^.*\.' '' -- $key)
            printf "%s\t%s\n" $name "Alias for $value"
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
    for name in (string replace -r "^.*/git-([^/]*)" '$1' $PATH/git-*)
        switch $name
            case cvsserver receive-pack shell upload-archive upload-pack
                # skip these
            case \*
                echo $name
        end
    end
end

# Suggest branches for the specified remote - returns 1 if no known remote is specified
function __fish_git_branch_for_remote
    set -l remotes (__fish_git_remotes)
    set -l remote
    set -l cmd (commandline -opc)
    for r in $remotes
        if contains -- $r $cmd
            set remote $r
            break
        end
    end
    set -q remote[1]
    or return 1
    __fish_git_branches | string match -- "$remote/*" | string replace -- "$remote/" ''
end

# Return 0 if the current token is a possible commit-hash with at least 3 characters
function __fish_git_possible_commithash
    set -q argv[1]
    and set -l token $argv[1]
    or set -l token (commandline -ct)
    if string match -qr '^[0-9a-fA-F]{3,}$' -- $token
        return 0
    end
    return 1
end

function __fish_git_reflog
	command git reflog | string replace -r '[0-9a-f]* (.+@\{[0-9]+\}): (.*)$' '$1\t$2'
end

# general options
complete -f -c git -l help -d 'Display the manual of a git command'
complete -f -c git -n '__fish_git_using_command log show diff-tree rev-list' -l pretty -a 'oneline short medium full fuller email raw format:'

#### fetch
complete -f -c git -n '__fish_git_needs_command' -a fetch -d 'Download objects and refs from another repository'
# Suggest "repository", then "refspec" - this also applies to e.g. push/pull
complete -f -c git -n '__fish_git_using_command fetch; and not __fish_git_branch_for_remote' -a '(__fish_git_remotes)' -d 'Remote'
complete -f -c git -n '__fish_git_using_command fetch; and __fish_git_branch_for_remote' -a '(__fish_git_branch_for_remote)' -d 'Branch'
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
set -l remotecommands add rm show prune update rename set-head set-url set-branches
complete -f -c git -n '__fish_git_needs_command' -a remote -d 'Manage set of tracked repositories'
complete -f -c git -n '__fish_git_using_command remote' -a '(__fish_git_remotes)'
complete -f -c git -n "__fish_git_using_command remote; and not __fish_seen_subcommand_from $remotecommands" -s v -l verbose -d 'Be verbose'
complete -f -c git -n "__fish_git_using_command remote; and not __fish_seen_subcommand_from $remotecommands" -a add -d 'Adds a new remote'
complete -f -c git -n "__fish_git_using_command remote; and not __fish_seen_subcommand_from $remotecommands" -a rm -d 'Removes a remote'
complete -f -c git -n "__fish_git_using_command remote; and not __fish_seen_subcommand_from $remotecommands" -a show -d 'Shows a remote'
complete -f -c git -n "__fish_git_using_command remote; and not __fish_seen_subcommand_from $remotecommands" -a prune -d 'Deletes all stale tracking branches'
complete -f -c git -n "__fish_git_using_command remote; and not __fish_seen_subcommand_from $remotecommands" -a update -d 'Fetches updates'
complete -f -c git -n "__fish_git_using_command remote; and not __fish_seen_subcommand_from $remotecommands" -a rename -d 'Renames a remote'
complete -f -c git -n "__fish_git_using_command remote; and not __fish_seen_subcommand_from $remotecommands" -a set-head -d 'Sets the default branch for a remote'
complete -f -c git -n "__fish_git_using_command remote; and not __fish_seen_subcommand_from $remotecommands" -a set-url -d 'Changes URLs for a remote'
complete -f -c git -n "__fish_git_using_command remote; and not __fish_seen_subcommand_from $remotecommands" -a set-branches -d 'Changes the list of branches tracked by a remote'
complete -f -c git -n "__fish_git_using_command remote; and __fish_seen_subcommand_from add " -s f -d 'Once the remote information is set up git fetch <name> is run'
complete -f -c git -n "__fish_git_using_command remote; and __fish_seen_subcommand_from add " -l tags -d 'Import every tag from a remote with git fetch <name>'
complete -f -c git -n "__fish_git_using_command remote; and __fish_seen_subcommand_from add " -l no-tags -d "Don't import tags from a remote with git fetch <name>"
complete -f -c git -n "__fish_git_using_command remote; and __fish_seen_subcommand_from set-branches" -l add -d 'Add to the list of currently tracked branches instead of replacing it'
complete -f -c git -n "__fish_git_using_command remote; and __fish_seen_subcommand_from set-url" -l push -d 'Manipulate push URLs instead of fetch URLs'
complete -f -c git -n "__fish_git_using_command remote; and __fish_seen_subcommand_from set-url" -l add -d 'Add new URL instead of changing the existing URLs'
complete -f -c git -n "__fish_git_using_command remote; and __fish_seen_subcommand_from set-url" -l delete -d 'Remove URLs that match specified URL'
complete -f -c git -n "__fish_git_using_command remote; and __fish_seen_subcommand_from show" -s n -d 'Remote heads are not queried, cached information is used instead'
complete -f -c git -n "__fish_git_using_command remote; and __fish_seen_subcommand_from prune" -l dry-run -d 'Report what will be pruned but do not actually prune it'
complete -f -c git -n "__fish_git_using_command remote; and __fish_seen_subcommand_from update" -l prune -d 'Prune all remotes that are updated'

### show
complete -f -c git -n '__fish_git_needs_command' -a show -d 'Shows the last commit of a branch'
complete -f -c git -n '__fish_git_using_command show' -a '(__fish_git_branches)' -d 'Branch'
complete -f -c git -n '__fish_git_using_command show' -a '(__fish_git_unique_remote_branches)' -d 'Remote branch'
complete -f -c git -n '__fish_git_using_command show' -a '(__fish_git_tags)' --description 'Tag'
complete -f -c git -n '__fish_git_using_command show' -a '(__fish_git_commits)'
# TODO options

### show-branch
complete -f -c git -n '__fish_git_needs_command' -a show-branch -d 'Shows the commits on branches'
complete -f -c git -n '__fish_git_using_command show-branch' -a '(__fish_git_heads)' --description 'Branch'
# TODO options

### add
complete -c git -n '__fish_git_needs_command' -a add -d 'Add file contents to the index'
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
complete -f -c git -n '__fish_git_needs_command' -a checkout -d 'Checkout and switch to a branch'
complete -f -c git -n '__fish_git_using_command checkout' -a '(__fish_git_branches)' --description 'Branch'
complete -f -c git -n '__fish_git_using_command checkout' -a '(__fish_git_unique_remote_branches)' --description 'Remote branch'
complete -f -c git -n '__fish_git_using_command checkout' -a '(__fish_git_tags)' --description 'Tag'
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
complete -f -c git -n '__fish_git_using_command branch' -l set-upstream-to -d 'Set remote branch to track'
complete -f -c git -n '__fish_git_using_command branch' -l merged -d 'List branches that have been merged'
complete -f -c git -n '__fish_git_using_command branch' -l no-merged -d 'List branches that have not been merged'

### cherry-pick
complete -f -c git -n '__fish_git_needs_command' -a cherry-pick -d 'Apply the change introduced by an existing commit'
complete -f -c git -n '__fish_git_using_command cherry-pick' -a '(__fish_git_branches --no-merged)' -d 'Branch'
complete -f -c git -n '__fish_git_using_command cherry-pick' -a '(__fish_git_unique_remote_branches --no-merged)' -d 'Remote branch'
# TODO: Filter further
complete -f -c git -n '__fish_git_using_command cherry-pick; and __fish_git_possible_commithash' -a '(__fish_git_commits)'
complete -f -c git -n '__fish_git_using_command cherry-pick' -s e -l edit -d 'Edit the commit message prior to committing'
complete -f -c git -n '__fish_git_using_command cherry-pick' -s x -d 'Append info in generated commit on the origin of the cherry-picked change'
complete -f -c git -n '__fish_git_using_command cherry-pick' -s n -l no-commit -d 'Apply changes without making any commit'
complete -f -c git -n '__fish_git_using_command cherry-pick' -s s -l signoff -d 'Add Signed-off-by line to the commit message'
complete -f -c git -n '__fish_git_using_command cherry-pick' -l ff -d 'Fast-forward if possible'

### clone
complete -f -c git -n '__fish_git_needs_command' -a clone -d 'Clone a repository into a new directory'
complete -f -c git -n '__fish_git_using_command clone' -l no-hardlinks -d 'Copy files instead of using hardlinks'
complete -f -c git -n '__fish_git_using_command clone' -s q -l quiet -d 'Operate quietly and do not report progress'
complete -f -c git -n '__fish_git_using_command clone' -s v -l verbose -d 'Provide more information on what is going on'
complete -f -c git -n '__fish_git_using_command clone' -s n -l no-checkout -d 'No checkout of HEAD is performed after the clone is complete'
complete -f -c git -n '__fish_git_using_command clone' -l bare -d 'Make a bare Git repository'
complete -f -c git -n '__fish_git_using_command clone' -l mirror -d 'Set up a mirror of the source repository'
complete -f -c git -n '__fish_git_using_command clone' -s o -l origin -d 'Use a specific name of the remote instead of the default'
complete -f -c git -n '__fish_git_using_command clone' -s b -l branch -d 'Use a specific branch instead of the one used by the cloned repository'
complete -f -c git -n '__fish_git_using_command clone' -l depth -d 'Truncate the history to a specified number of revisions'
complete -f -c git -n '__fish_git_using_command clone' -l recursive -d 'Initialize all submodules within the cloned repository'

### commit
complete -c git -n '__fish_git_needs_command' -a commit -d 'Record changes to the repository'
complete -c git -n '__fish_git_using_command commit' -l amend -d 'Amend the log message of the last commit'
complete -f -c git -n '__fish_git_using_command commit' -a '(__fish_git_modified_files)'
complete -f -c git -n '__fish_git_using_command commit' -l fixup -d 'Fixup commit to be used with rebase --autosquash'
complete -f -c git -n '__fish_git_using_command commit; and __fish_contains_opt fixup' -a '(__fish_git_commits)'
# TODO options

### diff
complete -c git -n '__fish_git_needs_command' -a diff -d 'Show changes between commits, commit and working tree, etc'
complete -c git -n '__fish_git_using_command diff' -a '(__fish_git_ranges)' -d 'Branch'
complete -c git -n '__fish_git_using_command diff' -l cached -d 'Show diff of changes in the index'
complete -c git -n '__fish_git_using_command diff' -l no-index -d 'Compare two paths on the filesystem'
# TODO options

### difftool
complete -c git -n '__fish_git_needs_command' -a difftool -d 'Open diffs in a visual tool'
complete -c git -n '__fish_git_using_command difftool' -a '(__fish_git_ranges)' -d 'Branch'
complete -c git -n '__fish_git_using_command difftool' -l cached -d 'Visually show diff of changes in the index'
# TODO options


### grep
complete -c git -n '__fish_git_needs_command' -a grep -d 'Print lines matching a pattern'
# TODO options

### init
complete -f -c git -n '__fish_git_needs_command' -a init -d 'Create an empty git repository or reinitialize an existing one'
# TODO options

### log
complete -c git -n '__fish_git_needs_command' -a log -d 'Show commit logs'
complete -c git -n '__fish_git_using_command log' -a '(__fish_git_heads) (__fish_git_ranges)' -d 'Branch'
# TODO options

### merge
complete -f -c git -n '__fish_git_needs_command' -a merge -d 'Join two or more development histories together'
complete -f -c git -n '__fish_git_using_command merge' -a '(__fish_git_branches)' -d 'Branch'
complete -f -c git -n '__fish_git_using_command merge' -a '(__fish_git_unique_remote_branches)' -d 'Remote branch'
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
complete -c git -n '__fish_git_needs_command' -a mv -d 'Move or rename a file, a directory, or a symlink'
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
complete -f -c git -n '__fish_git_using_command pull; and not __fish_git_branch_for_remote' -a '(__fish_git_remotes)' -d 'Remote alias'
complete -f -c git -n '__fish_git_using_command pull; and __fish_git_branch_for_remote' -a '(__fish_git_branch_for_remote)' -d 'Branch'
# TODO other options

### push
complete -f -c git -n '__fish_git_needs_command' -a push -d 'Update remote refs along with associated objects'
complete -f -c git -n '__fish_git_using_command push; and not __fish_git_branch_for_remote' -a '(__fish_git_remotes)' -d 'Remote alias'
complete -f -c git -n '__fish_git_using_command push; and __fish_git_branch_for_remote' -a '(__fish_git_branches)' -d 'Branch'
# The "refspec" here is an optional "+" to signify a force-push
complete -f -c git -n '__fish_git_using_command push; and __fish_git_branch_for_remote; and string match -q "+*" -- (commandline -ct)' -a '+(__fish_git_branches)' -d 'Force-push branch'
# then src:dest (where both src and dest are git objects, so we want to complete branches)
complete -f -c git -n '__fish_git_using_command push; and __fish_git_branch_for_remote; and string match -q "+*:*" -- (commandline -ct)' -a '+(__fish_git_branches):(__fish_git_branch_for_remote)' -d 'Force-push local branch to remote branch'
complete -f -c git -n '__fish_git_using_command push; and __fish_git_branch_for_remote; and string match -q "*:*" -- (commandline -ct)' -a '(__fish_git_branches):(__fish_git_branch_for_remote)' -d 'Push local branch to remote branch'
complete -f -c git -n '__fish_git_using_command push' -l all -d 'Push all refs under refs/heads/'
complete -f -c git -n '__fish_git_using_command push' -l prune -d "Remove remote branches that don't have a local counterpart"
complete -f -c git -n '__fish_git_using_command push' -l mirror -d 'Push all refs under refs/'
complete -f -c git -n '__fish_git_using_command push' -l delete -d 'Delete all listed refs from the remote repository'
complete -f -c git -n '__fish_git_using_command push' -l tags -d 'Push all refs under refs/tags'
complete -f -c git -n '__fish_git_using_command push' -s n -l dry-run -d 'Do everything except actually send the updates'
complete -f -c git -n '__fish_git_using_command push' -l porcelain -d 'Produce machine-readable output'
complete -f -c git -n '__fish_git_using_command push' -s f -l force -d 'Force update of remote refs'
complete -f -c git -n '__fish_git_using_command push' -s u -l set-upstream-to -d 'Add upstream (tracking) reference'
complete -f -c git -n '__fish_git_using_command push' -s q -l quiet -d 'Be quiet'
complete -f -c git -n '__fish_git_using_command push' -s v -l verbose -d 'Be verbose'
complete -f -c git -n '__fish_git_using_command push' -l progress -d 'Force progress status'
# TODO --recurse-submodules=check|on-demand

### rebase
complete -f -c git -n '__fish_git_needs_command' -a rebase -d 'Forward-port local commits to the updated upstream head'
complete -f -c git -n '__fish_git_using_command rebase' -a '(__fish_git_remotes)' -d 'Remote alias'
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
complete -c git -n '__fish_git_needs_command' -a reset -d 'Reset current HEAD to the specified state'
complete -f -c git -n '__fish_git_using_command reset' -l hard -d 'Reset files in working directory'
complete -c git -n '__fish_git_using_command reset' -a '(__fish_git_branches)' -d 'Branch'
complete -f -c git -n '__fish_git_using_command reset' -a '(__fish_git_staged_files)' -d 'File'
complete -f -c git -n '__fish_git_using_command reset' -a '(__fish_git_reflog)' -d 'Reflog'
# TODO options

### revert
complete -f -c git -n '__fish_git_needs_command' -a revert -d 'Revert an existing commit'
complete -f -c git -n '__fish_git_using_command revert' -a '(__fish_git_commits)'
# TODO options

### rm
complete -c git -n '__fish_git_needs_command' -a rm -d 'Remove files from the working tree and from the index'
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
complete -f -c git -n '__fish_git_using_command status' -l porcelain -d 'Give the output in a stable, easy-to-parse format'
complete -f -c git -n '__fish_git_using_command status' -s z -d 'Terminate entries with null character'
complete -f -c git -n '__fish_git_using_command status' -s u -l untracked-files -x -a 'no normal all' -d 'The untracked files handling mode'
complete -f -c git -n '__fish_git_using_command status' -l ignore-submodules -x -a 'none untracked dirty all' -d 'Ignore changes to submodules'
# TODO options

### tag
complete -f -c git -n '__fish_git_needs_command' -a tag -d 'Create, list, delete or verify a tag object signed with GPG'
complete -f -c git -n '__fish_git_using_command tag; and __fish_not_contain_opt -s d; and __fish_not_contain_opt -s v; and test (count (commandline -opc | string match -r -v \'^-\')) -eq 3' -a '(__fish_git_branches)' -d 'Branch'
complete -f -c git -n '__fish_git_using_command tag' -s a -l annotate -d 'Make an unsigned, annotated tag object'
complete -f -c git -n '__fish_git_using_command tag' -s s -l sign -d 'Make a GPG-signed tag'
complete -f -c git -n '__fish_git_using_command tag' -s d -l delete -d 'Remove a tag'
complete -f -c git -n '__fish_git_using_command tag' -s v -l verify -d 'Verify signature of a tag'
complete -f -c git -n '__fish_git_using_command tag' -s f -l force -d 'Force overwriting exising tag'
complete -f -c git -n '__fish_git_using_command tag' -s l -l list -d 'List tags'
complete -f -c git -n '__fish_git_using_command tag' -l contains -xa '(__fish_git_commits)' -d 'List tags that contain a commit'
complete -f -c git -n '__fish_git_using_command tag; and __fish_contains_opt -s d' -a '(__fish_git_tags)' -d 'Tag'
complete -f -c git -n '__fish_git_using_command tag; and __fish_contains_opt -s v' -a '(__fish_git_tags)' -d 'Tag'
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
complete -f -c git -n '__fish_git_using_command format-patch' -s p -l no-stat -d "Generate plain patches without diffstat"
complete -f -c git -n '__fish_git_using_command format-patch' -s s -l no-patch -d "Suppress diff output"
complete -f -c git -n '__fish_git_using_command format-patch' -l minimal -d "Spend more time to create smaller diffs"
complete -f -c git -n '__fish_git_using_command format-patch' -l patience -d "Generate diff with the 'patience' algorithm"
complete -f -c git -n '__fish_git_using_command format-patch' -l histogram -d "Generate diff with the 'histogram' algorithm"
complete -f -c git -n '__fish_git_using_command format-patch' -l stdout -d "Print all commits to stdout in mbox format"
complete -f -c git -n '__fish_git_using_command format-patch' -l numstat -d "Show number of added/deleted lines in decimal notation"
complete -f -c git -n '__fish_git_using_command format-patch' -l shortstat -d "Output only last line of the stat"
complete -f -c git -n '__fish_git_using_command format-patch' -l summary -d "Output a condensed summary of extended header information"
complete -f -c git -n '__fish_git_using_command format-patch' -l no-renames -d "Disable rename detection"
complete -f -c git -n '__fish_git_using_command format-patch' -l full-index -d "Show full blob object names"
complete -f -c git -n '__fish_git_using_command format-patch' -l binary -d "Output a binary diff for use with git apply"
complete -f -c git -n '__fish_git_using_command format-patch' -l find-copies-harder -d "Also inspect unmodified files as source for a copy"
complete -f -c git -n '__fish_git_using_command format-patch' -l text -s a -d "Treat all files as text"
complete -f -c git -n '__fish_git_using_command format-patch' -l ignore-space-at-eol -d "Ignore changes in whitespace at EOL"
complete -f -c git -n '__fish_git_using_command format-patch' -l ignore-space-change -s b -d "Ignore changes in amount of whitespace"
complete -f -c git -n '__fish_git_using_command format-patch' -l ignore-all-space -s w -d "Ignore whitespace when comparing lines"
complete -f -c git -n '__fish_git_using_command format-patch' -l ignore-blank-lines -d "Ignore changes whose lines are all blank"
complete -f -c git -n '__fish_git_using_command format-patch' -l function-context -s W -d "Show whole surrounding functions of changes"
complete -f -c git -n '__fish_git_using_command format-patch' -l ext-diff -d "Allow an external diff helper to be executed"
complete -f -c git -n '__fish_git_using_command format-patch' -l no-ext-diff -d "Disallow external diff helpers"
complete -f -c git -n '__fish_git_using_command format-patch' -l no-textconv -d "Disallow external text conversion filters for binary files (Default)"
complete -f -c git -n '__fish_git_using_command format-patch' -l textconv -d "Allow external text conversion filters for binary files (Resulting diff is unappliable)"
complete -f -c git -n '__fish_git_using_command format-patch' -l no-prefix -d "Do not show source or destination prefix"
complete -f -c git -n '__fish_git_using_command format-patch' -l numbered -s n -d "Name output in [Patch n/m] format, even with a single patch"
complete -f -c git -n '__fish_git_using_command format-patch' -l no-numbered -s N -d "Name output in [Patch] format, even with multiple patches"


## git submodule
set -l submodulecommands add status init update summary foreach sync
complete -f -c git -n '__fish_git_needs_command' -a submodule -d 'Initialize, update or inspect submodules'
complete -f -c git -n "__fish_git_using_command submodule; and not __fish_seen_subcommand_from $submodulecommands" -a 'add' -d 'Add a submodule'
complete -f -c git -n "__fish_git_using_command submodule; and not __fish_seen_subcommand_from $submodulecommands" -a 'status' -d 'Show submodule status'
complete -f -c git -n "__fish_git_using_command submodule; and not __fish_seen_subcommand_from $submodulecommands" -a 'init' -d 'Initialize all submodules'
complete -f -c git -n "__fish_git_using_command submodule; and not __fish_seen_subcommand_from $submodulecommands" -a 'update' -d 'Update all submodules'
complete -f -c git -n "__fish_git_using_command submodule; and not __fish_seen_subcommand_from $submodulecommands" -a 'summary' -d 'Show commit summary'
complete -f -c git -n "__fish_git_using_command submodule; and not __fish_seen_subcommand_from $submodulecommands" -a 'foreach' -d 'Run command on each submodule'
complete -f -c git -n "__fish_git_using_command submodule; and not __fish_seen_subcommand_from $submodulecommands" -a 'sync' -d 'Sync submodules\' URL with .gitmodules'
complete -f -c git -n "__fish_git_using_command submodule; and not __fish_seen_subcommand_from $submodulecommands" -s q -l quiet -d "Only print error messages"
complete -f -c git -n '__fish_git_using_command submodule; and __fish_seen_subcommand_from update' -l init -d "Initialize all submodules"
complete -f -c git -n '__fish_git_using_command submodule; and __fish_seen_subcommand_from update' -l checkout -d "Checkout the superproject's commit on a detached HEAD in the submodule"
complete -f -c git -n '__fish_git_using_command submodule; and __fish_seen_subcommand_from update' -l merge -d "Merge the superproject's commit into the current branch of the submodule"
complete -f -c git -n '__fish_git_using_command submodule; and __fish_seen_subcommand_from update' -l rebase -d "Rebase current branch onto the superproject's commit"
complete -f -c git -n '__fish_git_using_command submodule; and __fish_seen_subcommand_from update' -s N -l no-fetch -d "Don't fetch new objects from the remote"
complete -f -c git -n '__fish_git_using_command submodule; and __fish_seen_subcommand_from update' -l remote -d "Instead of using superproject's SHA-1, use the state of the submodule's remote-tracking branch"
complete -f -c git -n '__fish_git_using_command submodule; and __fish_seen_subcommand_from update' -l force -d "Throw away local changes when switching to a different commit and always run checkout"
complete -f -c git -n '__fish_git_using_command submodule; and __fish_seen_subcommand_from add' -l force -d "Also add ignored submodule path"
complete -f -c git -n '__fish_git_using_command submodule; and __fish_seen_subcommand_from deinit' -l force -d "Remove even with local changes"
complete -f -c git -n '__fish_git_using_command submodule; and __fish_seen_subcommand_from status summary' -l cached -d "Use the commit stored in the index"
complete -f -c git -n '__fish_git_using_command submodule; and __fish_seen_subcommand_from summary' -l files -d "Compare the commit in the index with submodule HEAD"
complete -f -c git -n '__fish_git_using_command submodule; and __fish_seen_subcommand_from foreach update status' -l recursive -d "Traverse submodules recursively"
complete -f -c git -n '__fish_git_using_command submodule; and __fish_seen_subcommand_from foreach' -a "(__fish_complete_subcommand --fcs-skip=3)"

## git whatchanged
complete -f -c git -n '__fish_git_needs_command' -a whatchanged -d 'Show logs with difference each commit introduces'

## Aliases (custom user-defined commands)
complete -c git -n '__fish_git_needs_command' -a '(__fish_git_aliases)'

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
