# fish completion for git
# Use 'command git' to avoid interactions for aliases from git to (e.g.) hub

function __fish_git_commits
    # Complete commits with their subject line as the description
    # This allows filtering by subject with the new pager!
    # Because even subject lines can be quite long,
    # trim them (abbrev'd hash+tab+subject) to 73 characters
    command git log --pretty=tformat:"%h"\t"%<(64,trunc)%s" --all --max-count=1000 2>/dev/null
end

function __fish_git_recent_commits
    # Like __fish_git_commits, but not on all branches and limited to
    # the last 50 commits. Used for fixup, where only the current branch
    # and the latest commits make sense.
    command git log --pretty=tformat:"%h"\t"%<(64,trunc)%s" --max-count=50 2>/dev/null
end

function __fish_git_branches
    command git branch --no-color -a $argv 2>/dev/null \
    # Filter out detached heads and such ("(HEAD detached at SOMESHA)", localized).
    | string match -v '\* (*)' | string match -r -v ' -> ' | string trim -c "* " \
    # We assume anything that's not remote is a local branch.
    | string replace -r '^(?!remotes/)(.*)' '$1\tLocal Branch' \
    | string replace -r "^remotes/(.*)" '$1\tRemote Branch'
end

function __fish_git_tags
    command git tag --sort=-creatordate 2>/dev/null
end

function __fish_git_dir
    command git rev-parse --git-dir 2>/dev/null
end

function __fish_git_heads
    set -l gitdir (__fish_git_dir)
    for head in HEAD FETCH_HEAD ORIG_HEAD MERGE_HEAD
        if test -f $gitdir/$head
            echo $head
        end
    end
end

function __fish_git_refs
    __fish_git_branches
    __fish_git_tags
    __fish_git_heads
end

function __fish_git_remotes
    command git remote 2>/dev/null
end

function __fish_git_files
    # A function to show various kinds of files git knows about,
    # by parsing `git status --porcelain`.
    #
    # This accepts arguments to denote the kind of files:
    # - added: Staged added files (unstaged adds are untracked)
    # - copied
    # - deleted
    # - deleted-staged
    # - ignored
    # - modified: Files that have been modified (but aren't staged)
    # - modified-staged: Staged modified files
    # - renamed
    # - untracked
    # and as a convenience "all-staged"
    # to get _all_ kinds of staged files.

    # Save the repo root to remove it from the path later.
    set -l root (command git rev-parse --show-toplevel 2>/dev/null)

    # Cache the translated descriptions so we don't have to get it
    # once per file.
    # This is slightly slower for < 8 files, but that is fast enough anyway.
    set -l unmerged_desc (_ "Unmerged File")
    set -l added_desc (_ "Added file")
    set -l modified_desc (_ "Modified file")
    set -l staged_modified_desc (_ "Staged modified file")
    set -l deleted_desc (_ "Deleted file")
    set -l staged_deleted_desc (_ "Staged deleted file")
    set -l untracked_desc (_ "Untracked file")
    set -l ignored_desc (_ "Ignored file")

    # git status --porcelain gives us all the info we need, in a format we don't.
    # The v2 format has better documentation and doesn't use " " to denote anything,
    # but it's only been added in git 2.11.0, which was released November 2016.
    # Instead, we use the v1 format, without explicitly specifying it (since that errors out as well).
    #
    # Also, we ignore submodules because they aren't useful as arguments (generally),
    # and they slow things down quite significantly.
    # E.g. `git reset $submodule` won't do anything (not even print an error).
    # --ignore-submodules=all was added in git 1.7.2, released July 2010.
    set -l use_next
    command git status --porcelain -z --ignore-submodules=all \
    | while read -lz -d '' line
        # The entire line is the "from" from a rename.
        if set -q use_next[1]
            if contains -- $use_next $argv
                string replace -f -- "$PWD/" "" "$root/$line"
                or string replace -- "$root/" ":/" "$root/$line"
            end
            set -e use_next[1]
            continue
        end

        # The format is two characters for status, then a space and then
        # up to a NUL for the filename.
        #
        # Use IFS to handle newlines in filenames.
        set -l IFS
        set -l stat (string sub -l 2 -- $line)
        set -l file (string sub -s 4 -- $line)
        # Print files from the current $PWD as-is, prepend all others with ":/" (relative to toplevel in git-speak)
        # This is a bit simplistic but finding the lowest common directory
        # and then replacing everything else in $PWD with ".." is a bit annoying
        set file (string replace -f -- "$PWD/" "" "$root/$file"; or string replace -- "$root/" ":/" "$root/$file")
        set -e IFS

        # The basic status format is "XY", where X is "our" state (meaning the staging area),
        # and "Y" is "their" state (meaning the work tree).
        # A " " means it's unmodified.
        #
        # Be careful about the ordering here!
        switch "$stat"
            case DD AU UD UA DU AA UU
                # Unmerged
                # TODO: It might be useful to split this up.
                contains -- unmerged $argv
                and printf '%s\t%s\n' "$file" $unmerged_desc
            case 'R ' RM RD
                # Renamed/Copied
                # These have the "from" name as the next batch.
                # TODO: Do we care about the new name?
                set use_next renamed
                continue
            case 'C ' CM CD
                set use_next copied
                continue
            case 'A ' AM AD
                # Additions are only shown here if they are staged.
                # Otherwise it's an untracked file.
                contains -- added $argv; or contains -- all-staged $argv
                and printf '%s\t%s\n' "$file" $added_desc
            case '*M'
                # Modified
                contains -- modified $argv
                and printf '%s\t%s\n' "$file" $modified_desc
            case 'M*'
                # If the character is first ("M "), then that means it's "our" change,
                # which means it is staged.
                # This is useless for many commands - e.g. `checkout` won't do anything with this.
                # So it needs to be requested explicitly.
                contains -- modified-staged $argv; or contains -- all-staged $argv
                and printf '%s\t%s\n' "$file" $staged_modified_desc
            case '*D'
                contains -- deleted $argv
                and printf '%s\t%s\n' "$file" $deleted_desc
            case 'D*'
                # TODO: The docs are unclear on this.
                # There is both X unmodified and Y either M or D ("not updated")
                # and Y is D and X is unmodified or [MARC] ("deleted in work tree").
                # For our purposes, we assume this is a staged deletion.
                contains -- deleted-staged $argv; or contains -- all-staged $argv
                and printf '%s\t%s\n' "$file" $staged_deleted_desc
            case '\?\?'
                # Untracked
                contains -- untracked $argv
                and printf '%s\t%s\n' "$file" $untracked_desc
            case '!!'
                # Ignored
                contains -- ignored $argv
                and printf '%s\t%s\n' "$file" $ignored_desc
        end
    end
end

function __fish_git_ranges
    set -l both (commandline -ot | string split "..")
    set -l from $both[1]
    # If we didn't need to split (or there's nothing _to_ split), complete only the first part
    # Note that status here is from `string split` because `set` doesn't alter it
    if test -z "$from" -o $status -gt 0
        __fish_git_refs
        return 0
    end

    set -l to $both[2]
    # Remove description from the from-ref, not the to-ref.
    for from_ref in (__fish_git_refs | string match "$from" | string replace -r \t'.*$' '')
        for to_ref in (__fish_git_refs | string match "*$to*") # if $to is empty, this correctly matches everything
            printf "%s..%s\n" $from_ref $to_ref
        end
    end
end

function __fish_git_needs_command
    set cmd (commandline -opc)
    set -l skip_next 1
    set -q cmd[2]; or return 0
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


# HACK: Aliases
# Git allows aliases, so we need to see what command the current command-token corresponds to
# (so we can complete e.g. `lg` like `log`).
# Checking `git config` for a lot of aliases can be quite slow if it is called
# for every possible command.
# Ideally, we'd `complete --wraps` this, but that is not currently possible, as is
# using `complete -C` like
# complete -c git -n '__fish_git_using_command lg' -a '(complete -C"git log ")'
#
# So instead, we store the aliases in global variables, named after the alias, containing the command.
# This is because alias:command is an n:1 mapping (an alias can only have one corresponding command,
#                                                  but a command can be aliased multiple times)
git config -z --get-regexp 'alias\..*' | while read -lz alias command _
    # Git aliases can contain chars that variable names can't - escape them.
    if test (count $command) -ne 1
        printf (_ "Warning: alias '%s' has more than one command: '%s'") $alias "$command" >&2
    end
    set alias (string replace 'alias.' '' -- $alias | string escape --style=var)
    set -g __fish_git_alias_$alias $command
end

function __fish_git_using_command
    set -l cmd (__fish_git_needs_command)
    test -z "$cmd"
    and return 1
    contains -- $cmd $argv
    and return 0

    # Check aliases.
    set -l varname __fish_git_alias_(string escape --style=var -- $cmd)
    set -q $varname
    and contains -- $$varname $argv
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
    command git stash list --format=%gd:%gs 2>/dev/null | string replace ":" \t
end

function __fish_git_aliases
    command git config -z --get-regexp '^alias\.' 2>/dev/null | while read -lz key value
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
    command git reflog --no-decorate 2>/dev/null | string replace -r '[0-9a-f]* (.+@\{[0-9]+\}): (.*)$' '$1\t$2'
end

function __fish_git_diff_opt -a option
    switch $option
        case diff-algorithm
            printf "%b" "
default\tBasic greedy diff algorithm
myers\tBasic greedy diff algorithm
minimal\tMake smallest diff possible
patience\tPatience diff algorithm
histogram\tPatience algorithm with low-occurrence common elements"
        case diff-filter
            printf "%b" "
A\tAdded files
C\tCopied files
D\tDeleted files
M\tModified files
R\tRenamed files
T\tType changed files
U\tUnmerged files
X\tUnknown files
B\tBroken pairing files"
        case dirstat
            printf "%b" "
changes\tCount lines that have been removed from the source / added to the destination
lines\tRegular line-based diff analysis
files\tCount the number of files changed
cumulative\tCount changes in a child directory for the parent directory as well"
        case ignore-submodules
            printf "%b" "
none\tUntracked/modified files
untracked\tNot considered dirty when they only contain untracked content
dirty\tIgnore all changes to the work tree of submodules
all\tHide all changes to submodules (default)"
        case submodule
            printf "%b" "
short\tShow the name of the commits at the beginning and end of the range
log\tList the commits in the range
diff\tShow an inline diff of the changes"
        case ws-error-highlight
            printf "%b" "
context\tcontext lines of the diff
old\told lines of the diff
new\tnew lines of the diff
none\treset previous values
default\treset the list to 'new'
all\tShorthand for 'old,new,context'"
    end
end

# general options
complete -f -c git -l help -d 'Display the manual of a git command'
complete -f -c git -n '__fish_git_needs_command' -l version -d 'Display version'
complete -x -c git -n '__fish_git_needs_command' -s C -a '(__fish_complete_directories)' -d 'Run as if git was started in this directory'
complete -x -c git -n '__fish_git_needs_command' -s c -a '(command git config -l 2>/dev/null | string replace = \t)' -d 'Set a configuration option'
complete -x -c git -n '__fish_git_needs_command' -l exec-path -a '(__fish_git_complete_directories)' -d 'Get or set the path to the git programs'
complete -f -c git -n '__fish_git_needs_command' -l html-path -d 'Print the path to the html documentation'
complete -f -c git -n '__fish_git_needs_command' -l man-path -d 'Print the path to the man documentation'
complete -f -c git -n '__fish_git_needs_command' -l info-path -d 'Print the path to the info documentation'
complete -f -c git -n '__fish_git_needs_command' -s p -l paginate -d 'Pipe output into a pager'
complete -f -c git -n '__fish_git_needs_command' -l no-pager -d 'Do not pipe output into a pager'
complete -f -c git -n '__fish_git_needs_command' -l git-dir -d 'Set the path to the repository'
complete -f -c git -n '__fish_git_needs_command' -l work-tree -d 'Set the path to the working tree'
complete -f -c git -n '__fish_git_needs_command' -l namespace -d 'Set the namespace'
complete -f -c git -n '__fish_git_needs_command' -l bare -d 'Treat the repository as bare'
complete -f -c git -n '__fish_git_needs_command' -l no-replace-objects -d 'Do not use replacement refs to replace git objects'
complete -f -c git -n '__fish_git_needs_command' -l literal-pathspecs -d 'Treat pathspecs literally'
complete -f -c git -n '__fish_git_needs_command' -l glob-pathspecs -d 'Treat pathspecs as globs'
complete -f -c git -n '__fish_git_needs_command' -l noglob-pathspecs -d "Don't treat pathspecs as globs"
complete -f -c git -n '__fish_git_needs_command' -l icase-pathspecs -d 'Match pathspecs case-insensitively'

# Options shared between multiple commands
complete -f -c git -n '__fish_git_using_command log show diff-tree rev-list' -l pretty -a 'oneline short medium full fuller email raw format:'

complete -c git -n '__fish_git_using_command diff show' -l abbrev -d 'Show only a partial prefix instead of the full 40-byte hexadecimal object name'
complete -c git -n '__fish_git_using_command diff show' -l binary -d 'Output a binary diff that can be applied with "git-apply"'
complete -c git -n '__fish_git_using_command diff show' -l check -d 'Warn if changes introduce conflict markers or whitespace errors'
complete -c git -n '__fish_git_using_command diff show' -l color -d 'Show colored diff'
complete -c git -n '__fish_git_using_command diff show' -l color-moved -d 'Moved lines of code are colored differently'
complete -c git -n '__fish_git_using_command diff show' -l color-words -d 'Equivalent to --word-diff=color plus --word-diff-regex=<regex>'
complete -c git -n '__fish_git_using_command diff show' -l compact-summary -d 'Output a condensed summary of extended header information'
complete -c git -n '__fish_git_using_command diff show' -l dst-prefix -d 'Show the given destination prefix instead of "b/"'
complete -c git -n '__fish_git_using_command diff show' -l ext-diff -d 'Allow an external diff helper to be executed'
complete -c git -n '__fish_git_using_command diff show' -l find-copies-harder -d 'Inspect unmodified files as candidates for the source of copy'
complete -c git -n '__fish_git_using_command diff show' -l find-object -d 'Look for differences that change the number of occurrences of the specified object'
complete -c git -n '__fish_git_using_command diff show' -l full-index -d 'Show the full pre- and post-image blob object names on the "index" line'
complete -c git -n '__fish_git_using_command diff show' -l histogram -d 'Generate a diff using the "histogram diff" algorithm'
complete -c git -n '__fish_git_using_command diff show' -l ignore-blank-lines -d 'Ignore changes whose lines are all blank'
complete -c git -n '__fish_git_using_command diff show' -l ignore-cr-at-eol -d 'Ignore carrige-return at the end of line when doing a comparison'
complete -c git -n '__fish_git_using_command diff show' -l ignore-space-at-eol -d 'Ignore changes in whitespace at EOL'
complete -c git -n '__fish_git_using_command diff show' -l indent-heuristic -d 'Enable the heuristic that shift diff hunk boundaries'
complete -c git -n '__fish_git_using_command diff show' -l inter-hunk-context -d 'Show the context between diff hunks, up to the specified number of lines'
complete -c git -n '__fish_git_using_command diff show' -l ita-invisible-in-index -d 'Make the entry appear as a new file in "git diff" and non-existent in "git diff -l cached"'
complete -c git -n '__fish_git_using_command diff show' -l line-prefix -d 'Prepend an additional prefix to every line of output'
complete -c git -n '__fish_git_using_command diff show' -l minimal -d 'Spend extra time to make sure the smallest possible diff is produced'
complete -c git -n '__fish_git_using_command diff show' -l name-only -d 'Show only names of changed files'
complete -c git -n '__fish_git_using_command diff show' -l name-status -d 'Show only names and status of changed files'
complete -c git -n '__fish_git_using_command diff show' -l no-color -d 'Turn off colored diff'
complete -c git -n '__fish_git_using_command diff show' -l no-ext-diff -d 'Disallow external diff drivers'
complete -c git -n '__fish_git_using_command diff show' -l no-indent-heuristic -d 'Disable the indent heuristic'
complete -c git -n '__fish_git_using_command diff show' -l no-prefix -d 'Do not show any source or destination prefix'
complete -c git -n '__fish_git_using_command diff show' -l no-renames -d 'Turn off rename detection'
complete -c git -n '__fish_git_using_command diff show' -l no-textconv -d 'Disallow external text conversion filters to be run when comparing binary files'
complete -c git -n '__fish_git_using_command diff show' -l numstat -d 'Shows number of added/deleted lines in decimal notation'
complete -c git -n '__fish_git_using_command diff show' -l patch-with-raw -d 'Synonym for -p --raw'
complete -c git -n '__fish_git_using_command diff show' -l patch-with-stat -d 'Synonym for -p --stat'
complete -c git -n '__fish_git_using_command diff show' -l patience -d 'Generate a diff using the "patience diff" algorithm'
complete -c git -n '__fish_git_using_command diff show' -l pickaxe-all -d 'When -S or -G finds a change, show all the changes in that changeset'
complete -c git -n '__fish_git_using_command diff show' -l pickaxe-regex -d 'Treat the <string> given to -S as an extended POSIX regular expression to match'
complete -c git -n '__fish_git_using_command diff show' -l relative -d 'Exclude changes outside the directory and show relative pathnames'
complete -c git -n '__fish_git_using_command diff show' -l shortstat -d 'Output only the last line of the --stat format containing total number of modified files'
complete -c git -n '__fish_git_using_command diff show' -l src-prefix -d 'Show the given source prefix instead of "a/"'
complete -c git -n '__fish_git_using_command diff show' -l stat -d 'Generate a diffstat'
complete -c git -n '__fish_git_using_command diff show' -l summary -d 'Output a condensed summary of extended header information'
complete -c git -n '__fish_git_using_command diff show' -l textconv -d 'Allow external text conversion filters to be run when comparing binary files'
complete -c git -n '__fish_git_using_command diff show' -l word-diff -d 'Show a word diff'
complete -c git -n '__fish_git_using_command diff show' -l word-diff-regex  -d 'Use <regex> to decide what a word is'
complete -c git -n '__fish_git_using_command diff show' -s a -l text -d 'Treat all files as text'
complete -c git -n '__fish_git_using_command diff show' -s B -l break-rewrites -d 'Break complete rewrite changes into pairs of delete and create'
complete -c git -n '__fish_git_using_command diff show' -s b -l ignore-space-change -d 'Ignore changes in amount of whitespace'
complete -c git -n '__fish_git_using_command diff show' -s C -l find-copies -d 'Detect copies as well as renames'
complete -c git -n '__fish_git_using_command diff show' -s D -l irreversible-delete -d 'Omit the preimage for deletes'
complete -c git -n '__fish_git_using_command diff show' -s G -d 'Look for differences whose patch text contains added/removed lines that match <regex>'
complete -c git -n '__fish_git_using_command diff show' -s M -l find-renames -d 'Detect and report renames'
complete -c git -n '__fish_git_using_command diff show' -s R -d 'Show differences from index or on-disk file to tree contents'
complete -c git -n '__fish_git_using_command diff show' -s S -d 'Look for differences that change the number of occurrences of the specified string'
complete -c git -n '__fish_git_using_command diff show' -s W -l function-context -d 'Show whole surrounding functions of changes'
complete -c git -n '__fish_git_using_command diff show' -s w -l ignore-all-space -d 'Ignore whitespace when comparing lines'
complete -c git -n '__fish_git_using_command diff show' -s z -d 'Use NULs as output field/commit terminators'
complete -r -c git -n '__fish_git_using_command diff show' -s O -d 'Control the order in which files appear in the output'
complete -f -c git -n '__fish_git_using_command diff show' -l anchored -d 'Generate a diff using the "anchored diff" algorithm'
complete -x -c git -n '__fish_git_using_command diff show' -s l -d 'Prevents rename/copy detection if the number of rename/copy targets exceeds the specified number'
complete -x -c git -n '__fish_git_using_command diff show' -l diff-filter -a '(__fish_git_diff_opt diff-filter)' -d 'Choose diff filters'
complete -x -c git -n '__fish_git_using_command diff log show' -l diff-algorithm -a '(__fish_git_diff_opt diff-algorithm)' -d 'Choose a diff algorithm'
complete -x -c git -n '__fish_git_using_command diff log show' -l dirstat -a '(__fish_git_diff_opt dirstat)' -d 'Output the distribution of relative amount of changes for each sub-directory'
complete -x -c git -n '__fish_git_using_command diff log show' -l ignore-submodules -a '(__fish_git_diff_opt ignore-submodules)' -d 'Ignore changes to submodules in the diff generation'
complete -x -c git -n '__fish_git_using_command diff log show' -l submodule -a '(__fish_git_diff_opt submodule)' -d 'Specify how differences in submodules are shown'
complete -x -c git -n '__fish_git_using_command diff log show' -l ws-error-highlight -a '(__fish_git_diff_opt ws-error-highlight)' -d 'Highlight whitespace errors in lines of the diff'

#### fetch
complete -f -c git -n '__fish_git_needs_command' -a fetch -d 'Download objects and refs from another repository'
# Suggest "repository", then "refspec" - this also applies to e.g. push/pull
complete -f -c git -n '__fish_git_using_command fetch; and not __fish_git_branch_for_remote' -a '(__fish_git_remotes)' -d 'Remote'
complete -f -c git -n '__fish_git_using_command fetch; and __fish_git_branch_for_remote' -a '(__fish_git_branch_for_remote)'
complete -f -c git -n '__fish_git_using_command fetch' -s q -l quiet -d 'Be quiet'
complete -f -c git -n '__fish_git_using_command fetch' -s v -l verbose -d 'Be verbose'
complete -f -c git -n '__fish_git_using_command fetch' -s a -l append -d 'Append ref names and object names'
# TODO --upload-pack
complete -f -c git -n '__fish_git_using_command fetch' -s f -l force -d 'Force update of local branches'
# TODO other options

#### filter-branch
complete -f -c git -n '__fish_git_needs_command' -a filter-branch -d 'Rewrite branches'
complete -f -c git -n '__fish_git_using_command filter-branch' -l env-filter -d 'This filter may be used if you only need to modify the environment'
complete -f -c git -n '__fish_git_using_command filter-branch' -l tree-filter -d 'This is the filter for rewriting the tree and its contents'
complete -f -c git -n '__fish_git_using_command filter-branch' -l index-filter -d 'This is the filter for rewriting the index'
complete -f -c git -n '__fish_git_using_command filter-branch' -l parent-filter -d 'This is the filter for rewriting the commit'
complete -f -c git -n '__fish_git_using_command filter-branch' -l msg-filter -d 'This is the filter for rewriting the commit messages'
complete -f -c git -n '__fish_git_using_command filter-branch' -l commit-filter -d 'This is the filter for performing the commit'
complete -f -c git -n '__fish_git_using_command filter-branch' -l tag-name-filter -d 'This is the filter for rewriting tag names'
complete -f -c git -n '__fish_git_using_command filter-branch' -l subdirectory-filter -d 'Only look at the history which touches the given subdirectory'
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
complete -f -c git -n '__fish_git_using_command show' -a '(__fish_git_branches)'
complete -f -c git -n '__fish_git_using_command show' -a '(__fish_git_tags)' -d 'Tag'
complete -f -c git -n '__fish_git_using_command show' -a '(__fish_git_commits)'
complete -f -c git -n '__fish_git_using_command show' -l stat -d 'Generate a diffstat, showing the number of changed lines of each file'
# TODO options

### show-branch
complete -f -c git -n '__fish_git_needs_command' -a show-branch -d 'Shows the commits on branches'
complete -f -c git -n '__fish_git_using_command show-branch' -a '(__fish_git_refs)' -d 'Rev'
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
# Renames also show up as untracked + deleted, and to get git to show it as a rename _both_ need to be added.
# However, we can't do that as it is two tokens, so we don't need renamed here.
complete -f -c git -n '__fish_git_using_command add' -a '(__fish_git_files modified untracked deleted unmerged)'
# TODO options

### checkout
complete -f -c git -n '__fish_git_needs_command' -a checkout -d 'Checkout and switch to a branch'
complete -k -f -c git -n '__fish_git_using_command checkout' -a '(__fish_git_branches)'
complete -k -f -c git -n '__fish_git_using_command checkout' -a '(__fish_git_heads)' -d 'Head'
complete -k -f -c git -n '__fish_git_using_command checkout' -a '(__fish_git_tags)' -d 'Tag'
complete -k -f -c git -n '__fish_git_using_command checkout' -a '(__fish_git_files modified deleted)'
complete -f -c git -n '__fish_git_using_command checkout' -s b -d 'Create a new branch'
complete -f -c git -n '__fish_git_using_command checkout' -s t -l track -d 'Track a new branch'
complete -f -c git -n '__fish_git_using_command checkout' -l theirs -d 'Keep staged changes'
complete -f -c git -n '__fish_git_using_command checkout' -l ours -d 'Keep unmerged changes'
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
complete -f -c git -n '__fish_git_using_command branch' -a '(__fish_git_branches)'
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
complete -f -c git -n '__fish_git_using_command cherry-pick' -a '(__fish_git_branches --no-merged)'
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
complete -f -c git -n '__fish_git_using_command commit' -a '(__fish_git_files modified)'
complete -f -c git -n '__fish_git_using_command commit' -l fixup -d 'Fixup commit to be used with rebase --autosquash'
complete -f -c git -n '__fish_git_using_command commit; and __fish_contains_opt fixup' -k -a '(__fish_git_recent_commits)'
# TODO options

### diff
complete -c git -n '__fish_git_needs_command' -a diff -d 'Show changes between commits, commit and working tree, etc'
complete -c git -n '__fish_git_using_command diff' -a '(__fish_git_ranges)'
complete -c git -n '__fish_git_using_command diff' -l cached -d 'Show diff of changes in the index'
complete -c git -n '__fish_git_using_command diff' -l no-index -d 'Compare two paths on the filesystem'
complete -f -c git -n '__fish_git_using_command diff' -a '(__fish_git_files modified deleted)'
# TODO options

### difftool
complete -c git -n '__fish_git_needs_command' -a difftool -d 'Open diffs in a visual tool'
complete -c git -n '__fish_git_using_command difftool' -a '(__fish_git_ranges)'
complete -c git -n '__fish_git_using_command difftool' -l cached -d 'Visually show diff of changes in the index'
complete -f -c git -n '__fish_git_using_command difftool' -a '(__fish_git_files modified deleted)'
# TODO options


### grep
complete -c git -n '__fish_git_needs_command' -a grep -d 'Print lines matching a pattern'
# TODO options

### init
complete -f -c git -n '__fish_git_needs_command' -a init -d 'Create an empty git repository or reinitialize an existing one'
# TODO options

### log
complete -c git -n '__fish_git_needs_command'    -a shortlog -d 'Show commit shortlog'
complete -c git -n '__fish_git_needs_command' -a log -d 'Show commit logs'
complete -c git -n '__fish_git_using_command log; and not contains -- -- (commandline -op)' -a '(__fish_git_refs) (__fish_git_ranges)'

complete -c git -n '__fish_git_using_command log' -l follow -d 'Continue listing file history beyond renames'
complete -c git -n '__fish_git_using_command log' -l no-decorate -d 'Don\'t print ref names'
complete -f -c git -n '__fish_git_using_command log' -l decorate -a 'short\tHide\ prefixes full\tShow\ full\ ref\ names auto\tHide\ prefixes\ if\ printed\ to\ terminal no\tDon\\\'t\ display\ ref' -d 'Print out ref names'
complete -c git -n '__fish_git_using_command log' -l source -d 'Print ref name by which each commit was reached'
complete -c git -n '__fish_git_using_command log' -l use-mailmap
complete -c git -n '__fish_git_using_command log' -l full-diff
complete -c git -n '__fish_git_using_command log' -l log-size
complete -x -c git -n '__fish_git_using_command log' -s L
complete -x -c git -n '__fish_git_using_command log' -s n -l max-count -d 'Limit the number of commits before starting to show the commit output'
complete -x -c git -n '__fish_git_using_command log' -l skip -d 'Skip given number of commits'
complete -x -c git -n '__fish_git_using_command log' -l since -d 'Show commits more recent than specified date'
complete -x -c git -n '__fish_git_using_command log' -l after -d 'Show commits more recent than specified date'
complete -x -c git -n '__fish_git_using_command log' -l until -d 'Show commits older than specified date'
complete -x -c git -n '__fish_git_using_command log' -l before -d 'Show commits older than specified date'
complete -x -c git -n '__fish_git_using_command log' -l author -d 'Limit commits from given author'
complete -x -c git -n '__fish_git_using_command log' -l committer -d 'Limit commits from given committer'
complete -x -c git -n '__fish_git_using_command log' -l grep-reflog -d 'Limit commits to ones with reflog entries matching given pattern'
complete -x -c git -n '__fish_git_using_command log' -l grep -d 'Limit commits with message that match given pattern'
complete -c git -n '__fish_git_using_command log' -l all-match -d 'Limit commits to ones that match all given --grep'
complete -c git -n '__fish_git_using_command log' -l invert-grep -d 'Limit commits to ones with message that don\'t match --grep'
complete -c git -n '__fish_git_using_command log' -l regexp-ignore-case -s i -d 'Case insensitive match'
complete -c git -n '__fish_git_using_command log' -l basic-regexp -d 'Patterns are basic regular expressions (default)'
complete -c git -n '__fish_git_using_command log' -l extended-regexp -s E -d 'Patterns are extended regular expressions'
complete -c git -n '__fish_git_using_command log' -l fixed-strings -s F -d 'Patterns are fixed strings'
complete -c git -n '__fish_git_using_command log' -l perl-regexp -d 'Patterns are Perl-compatible regular expressions'
complete -c git -n '__fish_git_using_command log' -l remove-empty -d 'Stop when given path disappears from tree'
complete -c git -n '__fish_git_using_command log' -l merges -d 'Print only merge commits'
complete -c git -n '__fish_git_using_command log' -l no-merges -d 'Don\'t print commits with more than one parent'
complete -x -c git -n '__fish_git_using_command log' -l min-parents -d 'Show only commit with at least the given number of parents'
complete -x -c git -n '__fish_git_using_command log' -l max-parents -d 'Show only commit with at most the given number of parents'
complete -c git -n '__fish_git_using_command log' -l no-min-parents -d 'Show only commit without a mimimum number of parents'
complete -c git -n '__fish_git_using_command log' -l no-max-parents -d 'Show only commit without a maximum number of parents'
complete -c git -n '__fish_git_using_command log' -l first-parent -d 'Follow only the first parent commit upon seeing a merge commit'
complete -c git -n '__fish_git_using_command log' -l not -d 'Reverse meaning of ^ prefix'
complete -c git -n '__fish_git_using_command log' -l all -d 'Pretend as if all refs in refs/ are listed on the command line as <commit>'
complete -f -c git -n '__fish_git_using_command log' -l branches -d 'Pretend as if all refs are in refs/heads are listed on the command line as <commit>'
complete -f -c git -n '__fish_git_using_command log' -l tags -d 'Pretend as if all refs are in ref/tags are listed on the command line as <commit>'
complete -f -c git -n '__fish_git_using_command log' -l remotes -d 'Pretend as if all refs in refs/remotes  are listed on the command line as <commit>'
complete -x -c git -n '__fish_git_using_command log' -l glob -d 'Pretend as if all refs matching shell glob are listed on the command line as <commit>'
complete -x -c git -n '__fish_git_using_command log' -l exclude -d 'Do not include refs matching given glob pattern'
complete -c git -n '__fish_git_using_command log' -l reflog -d 'Pretend as if all objcets mentioned by reflogs are listed on the command line as <commit>'
complete -c git -n '__fish_git_using_command log' -l ingnore-missing -d 'Ignore invalid object names'
complete -c git -n '__fish_git_using_command log' -l bisect
complete -c git -n '__fish_git_using_command log' -l stdin -d 'Read commits from stdin'
complete -c git -n '__fish_git_using_command log' -l cherry-mark -d 'Mark equivalent commits with = and inequivalent with +'
complete -c git -n '__fish_git_using_command log' -l cherry-pick -d 'Omit equivalent commits'
complete -c git -n '__fish_git_using_command log' -l left-only
complete -c git -n '__fish_git_using_command log' -l rigth-only
complete -c git -n '__fish_git_using_command log' -l cherry
complete -c git -n '__fish_git_using_command log' -l walk-reflogs -s g
complete -c git -n '__fish_git_using_command log' -l merge
complete -c git -n '__fish_git_using_command log' -l boundary
complete -c git -n '__fish_git_using_command log' -l simplify-by-decoration
complete -c git -n '__fish_git_using_command log' -l full-history
complete -c git -n '__fish_git_using_command log' -l dense
complete -c git -n '__fish_git_using_command log' -l sparse
complete -c git -n '__fish_git_using_command log' -l simplify-merges
complete -c git -n '__fish_git_using_command log' -l ancestry-path
complete -c git -n '__fish_git_using_command log' -l date-order
complete -c git -n '__fish_git_using_command log' -l author-date-order
complete -c git -n '__fish_git_using_command log' -l topo-order
complete -c git -n '__fish_git_using_command log' -l reverse
complete -f -c git -n '__fish_git_using_command log' -l no-walk -a "sorted unsorted"
complete -c git -n '__fish_git_using_command log' -l do-walk
complete -c git -n '__fish_git_using_command log' -l format
complete -c git -n '__fish_git_using_command log' -l abbrev-commit
complete -c git -n '__fish_git_using_command log' -l no-abbrev-commit
complete -c git -n '__fish_git_using_command log' -l oneline
complete -x -c git -n '__fish_git_using_command log' -l encoding -a '(__fish_print_encodings)'
complete -f -c git -n '__fish_git_using_command log' -l expand-tabs
complete -c git -n '__fish_git_using_command log' -l no-expand-tabs
complete -f -c git -n '__fish_git_using_command log' -l notes
complete -c git -n '__fish_git_using_command log' -l no-notes
complete -f -c git -n '__fish_git_using_command log' -l show-notes
complete -c git -n '__fish_git_using_command log' -l standard-notes
complete -c git -n '__fish_git_using_command log' -l no-standard-notes
complete -c git -n '__fish_git_using_command log' -l show-signature
complete -c git -n '__fish_git_using_command log' -l relative-date
complete -x -c git -n '__fish_git_using_command log' -l date -a '
  relative
  local
  iso
  iso-local
  iso8601
  iso8601-local
  iso-strict
  iso-strict-local
  iso8601-strict
  iso8601-strict-local
  rfc-local
  rfc2822-local
  short
  short-local
  raw
  unix
  format:
  default
  default-local
'
complete -c git -n '__fish_git_using_command log' -l parents
complete -c git -n '__fish_git_using_command log' -l children
complete -c git -n '__fish_git_using_command log' -l left-right
complete -c git -n '__fish_git_using_command log' -l graph
complete -f -c git -n '__fish_git_using_command log' -l show-linear-break
complete -c git -n '__fish_git_using_command log' -s c
complete -c git -n '__fish_git_using_command log' -l cc
complete -c git -n '__fish_git_using_command log' -s m
complete -c git -n '__fish_git_using_command log' -s r
complete -c git -n '__fish_git_using_command log' -s t
complete -c git -n '__fish_git_using_command log' -l patch -s p
complete -c git -n '__fish_git_using_command log' -s u
complete -c git -n '__fish_git_using_command log' -l no-patch -s s
complete -x -c git -n '__fish_git_using_command log' -l unified -s U
complete -c git -n '__fish_git_using_command log' -l raw
complete -c git -n '__fish_git_using_command log' -l patch-with-raw
complete -c git -n '__fish_git_using_command log' -l indent-heuristic
complete -c git -n '__fish_git_using_command log' -l no-indent-heuristic
complete -c git -n '__fish_git_using_command log' -l compaction-heuristic
complete -c git -n '__fish_git_using_command log' -l no-compaction-heuristic
complete -c git -n '__fish_git_using_command log' -l minimal
complete -c git -n '__fish_git_using_command log' -l patience
complete -c git -n '__fish_git_using_command log' -l histogram
complete -f -x -c git -n '__fish_git_using_command log' -l stat
complete -c git -n '__fish_git_using_command log' -l numstat
complete -c git -n '__fish_git_using_command log' -l shortstat
complete -c git -n '__fish_git_using_command log' -l summary
complete -c git -n '__fish_git_using_command log' -l patch-with-stat
complete -c git -n '__fish_git_using_command log' -s z
complete -c git -n '__fish_git_using_command log' -l name-only
complete -c git -n '__fish_git_using_command log' -l name-status
complete -f -c git -n '__fish_git_using_command log' -l color -a 'always never auto'
complete -c git -n '__fish_git_using_command log' -l no-color
complete -f -c git -n '__fish_git_using_command log' -l word-diff -a '
  color
  plain
  porcelain
  none
'
complete -f -c git -n '__fish_git_using_command log' -l color-words
complete -c git -n '__fish_git_using_command log' -l no-renames
complete -c git -n '__fish_git_using_command log' -l check
complete -c git -n '__fish_git_using_command log' -l full-index
complete -c git -n '__fish_git_using_command log' -l binary
complete -f -c git -n '__fish_git_using_command log' -l abbrev
complete -f -c git -n '__fish_git_using_command log' -l break-rewrittes -s B
complete -f -c git -n '__fish_git_using_command log' -l find-renames -s M
complete -f -c git -n '__fish_git_using_command log' -l find-copies -s C
complete -c git -n '__fish_git_using_command log' -l find-copies-harder
complete -c git -n '__fish_git_using_command log' -l irreversible-delete -s D
complete -f -c git -n '__fish_git_using_command log' -s l

function __fish__git_append_letters_nosep
    set -l token (commandline -tc)
    printf "%s\n" $token$argv
end

complete -x -c git -n '__fish_git_using_command log' -l diff-filter -a '(__fish__git_append_letters_nosep a\tExclude\ added c\tExclude\ copied d\tExclude\ deleted m\tExclude\ modified r\tExclude\ renamed t\tExclude\ type\ changed u\tExclude\ unmerged x\tExclude\ unknown b\tExclude\ broken A\tAdded C\tCopied D\tDeleted M\tModified R\tRenamed T\tType\ Changed U\tUnmerged X\tUnknown B\tBroken)'

complete -x -c git -n '__fish_git_using_command log' -s S
complete -x -c git -n '__fish_git_using_command log' -s G
complete -c git -n '__fish_git_using_command log' -l pickaxe-all
complete -f -c git -n '__fish_git_using_command log' -s O
complete -c git -n '__fish_git_using_command log' -s R
complete -c git -n '__fish_git_using_command log' -l relative
complete -c git -n '__fish_git_using_command log' -l text -s a
complete -c git -n '__fish_git_using_command log' -l ignore-space-at-eol
complete -c git -n '__fish_git_using_command log' -l ignore-space-change -s b
complete -c git -n '__fish_git_using_command log' -l ignore-all-space -s w
complete -c git -n '__fish_git_using_command log' -l ignore-blank-lines
complete -x -c git -n '__fish_git_using_command log' -l inter-hunk-context
complete -c git -n '__fish_git_using_command log' -l function-context -s W
complete -c git -n '__fish_git_using_command log' -l ext-diff
complete -c git -n '__fish_git_using_command log' -l no-ext-diff
complete -c git -n '__fish_git_using_command log' -l textconv
complete -c git -n '__fish_git_using_command log' -l no-textconv
complete -x -c git -n '__fish_git_using_command log' -l src-prefix
complete -x -c git -n '__fish_git_using_command log' -l dst-prefix
complete -c git -n '__fish_git_using_command log' -l no-prefix
complete -x -c git -n '__fish_git_using_command log' -l line-prefix
complete -c git -n '__fish_git_using_command log' -l ita-invisible-in-index

### merge
complete -f -c git -n '__fish_git_needs_command' -a merge -d 'Join two or more development histories together'
complete -f -c git -n '__fish_git_using_command merge' -a '(__fish_git_branches)'
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

### mergetool

function __fish_git_mergetools
    set -l tools diffuse diffmerge ecmerge emerge kdiff3 meld opendiff tkdiff vimdiff gvimdiff xxdiff araxis p4merge bc codecompare
    for tool in $tools
        if command -sq $tool
            echo "$tool"
        end
    end
end

complete -f -c git -n '__fish_git_needs_command' -a mergetool -d 'Run merge conflict resolution tools to resolve merge conflicts'
complete -f -c git -n '__fish_git_using_command mergetool' -s t -l tool -d "Use specific merge resolution program" -a "(__fish_git_mergetools)"
complete -f -c git -n '__fish_git_using_command mergetool' -a "(__fish_git_files unmerged)"


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
complete -f -c git -n '__fish_git_using_command pull; and __fish_git_branch_for_remote' -a '(__fish_git_branch_for_remote)'
# TODO other options

### push
complete -f -c git -n '__fish_git_needs_command' -a push -d 'Update remote refs along with associated objects'
complete -f -c git -n '__fish_git_using_command push; and not __fish_git_branch_for_remote' -a '(__fish_git_remotes)' -d 'Remote alias'
complete -f -c git -n '__fish_git_using_command push; and __fish_git_branch_for_remote' -a '(__fish_git_branches)'
# The "refspec" here is an optional "+" to signify a force-push
complete -f -c git -n '__fish_git_using_command push; and __fish_git_branch_for_remote; and string match -q "+*" -- (commandline -ct)' -a '+(__fish_git_branches | string replace -r \t".*" "")' -d 'Force-push branch'
# git push REMOTE :BRANCH deletes BRANCH on remote REMOTE
complete -f -c git -n '__fish_git_using_command push; and __fish_git_branch_for_remote; and string match -q ":*" -- (commandline -ct)' -a ':(__fish_git_branch_for_remote | string replace -r \t".*" "")' -d 'Delete remote branch'
# then src:dest (where both src and dest are git objects, so we want to complete branches)
complete -f -c git -n '__fish_git_using_command push; and __fish_git_branch_for_remote; and string match -q "+*:*" -- (commandline -ct)' -a '+(__fish_git_branches | string replace -r \t".*" ""):(__fish_git_branch_for_remote | string replace -r \t".*" "")' -d 'Force-push local branch to remote branch'
complete -f -c git -n '__fish_git_using_command push; and __fish_git_branch_for_remote; and string match -q "*:*" -- (commandline -ct)' -a '(__fish_git_branches | string replace -r \t".*" ""):(__fish_git_branch_for_remote | string replace -r \t".*" "")' -d 'Push local branch to remote branch'
complete -f -c git -n '__fish_git_using_command push' -l all -d 'Push all refs under refs/heads/'
complete -f -c git -n '__fish_git_using_command push' -l prune -d "Remove remote branches that don't have a local counterpart"
complete -f -c git -n '__fish_git_using_command push' -l mirror -d 'Push all refs under refs/'
complete -f -c git -n '__fish_git_using_command push' -l delete -d 'Delete all listed refs from the remote repository'
complete -f -c git -n '__fish_git_using_command push' -l tags -d 'Push all refs under refs/tags'
complete -f -c git -n '__fish_git_using_command push' -l follow-tags -d 'Push all usual refs plus the ones under refs/tags'
complete -f -c git -n '__fish_git_using_command push' -s n -l dry-run -d 'Do everything except actually send the updates'
complete -f -c git -n '__fish_git_using_command push' -l porcelain -d 'Produce machine-readable output'
complete -f -c git -n '__fish_git_using_command push' -s f -l force -d 'Force update of remote refs'
complete -f -c git -n '__fish_git_using_command push' -s f -l force-with-lease -d 'Force update of remote refs, stopping if other\'s changes would be overwritten'
complete -f -c git -n '__fish_git_using_command push' -s u -l set-upstream -d 'Add upstream (tracking) reference'
complete -f -c git -n '__fish_git_using_command push' -s q -l quiet -d 'Be quiet'
complete -f -c git -n '__fish_git_using_command push' -s v -l verbose -d 'Be verbose'
complete -f -c git -n '__fish_git_using_command push' -l progress -d 'Force progress status'
# TODO --recurse-submodules=check|on-demand

### rebase
complete -f -c git -n '__fish_git_needs_command' -a rebase -d 'Forward-port local commits to the updated upstream head'
complete -f -c git -n '__fish_git_using_command rebase' -a '(__fish_git_remotes)' -d 'Remote alias'
complete -f -c git -n '__fish_git_using_command rebase' -a '(__fish_git_branches)'
complete -f -c git -n '__fish_git_using_command rebase' -a '(__fish_git_heads)' -d 'Head'
complete -f -c git -n '__fish_git_using_command rebase' -a '(__fish_git_tags)' -d 'Tag'
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
complete -c git -n '__fish_git_using_command reset' -a '(__fish_git_branches)'
# reset can either undo changes to versioned modified files,
# or remove files from the staging area.
# TODO: Deleted files seem to need a "--" separator.
complete -f -c git -n '__fish_git_using_command reset' -a '(__fish_git_files all-staged modified)'
complete -f -c git -n '__fish_git_using_command reset' -a '(__fish_git_reflog)' -d 'Reflog'
# TODO options

### revert
complete -f -c git -n '__fish_git_needs_command' -a revert -d 'Revert an existing commit'
complete -f -c git -n '__fish_git_using_command revert' -a '(__fish_git_commits)'
# TODO options

### rm
complete -c git -n '__fish_git_needs_command' -a rm -d 'Remove files from the working tree and the index'
complete -c git -n '__fish_git_using_command rm' -l cached -d 'Unstage files from the index'
complete -c git -n '__fish_git_using_command rm; and __fish_contains_opt cached' -f -a '(__fish_git_files all-staged)'
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
complete -f -c git -n '__fish_git_using_command tag; and __fish_not_contain_opt -s d; and __fish_not_contain_opt -s v; and test (count (commandline -opc | string match -r -v \'^-\')) -eq 3' -a '(__fish_git_branches)'
complete -f -c git -n '__fish_git_using_command tag' -s a -l annotate -d 'Make an unsigned, annotated tag object'
complete -f -c git -n '__fish_git_using_command tag' -s s -l sign -d 'Make a GPG-signed tag'
complete -f -c git -n '__fish_git_using_command tag' -s d -l delete -d 'Remove a tag'
complete -f -c git -n '__fish_git_using_command tag' -s v -l verify -d 'Verify signature of a tag'
complete -f -c git -n '__fish_git_using_command tag' -s f -l force -d 'Force overwriting exising tag'
complete -f -c git -n '__fish_git_using_command tag' -s l -l list -d 'List tags'
complete -f -c git -n '__fish_git_using_command tag' -l contains -xa '(__fish_git_commits)' -d 'List tags that contain a commit'
complete -f -c git -n '__fish_git_using_command tag; and __fish_contains_opt -s d delete -s v verify' -a '(__fish_git_tags)' -d 'Tag'
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
complete -f -c git -n '__fish_git_using_command format-patch' -a '(__fish_git_branches)'
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

### git blame
complete -f -c git -n '__fish_git_needs_command' -a blame -d 'Show what revision and author last modified each line of a file'
complete -f -c git -n '__fish_git_using_command blame' -s b -d 'Show blank SHA-1 for boundary commits'
complete -f -c git -n '__fish_git_using_command blame' -l root -d 'Do not treat root commits as boundaries'
complete -f -c git -n '__fish_git_using_command blame' -l show-stats -d 'Include additional statistics'
complete -f -c git -n '__fish_git_using_command blame' -s L -d 'Annotate only the given line range'
complete -f -c git -n '__fish_git_using_command blame' -s l -d 'Show long rev'
complete -f -c git -n '__fish_git_using_command blame' -s t -d 'Show raw timestamp'
complete -r -c git -n '__fish_git_using_command blame' -s S -d 'Use revisions from named file instead of calling rev-list'
complete -f -c git -n '__fish_git_using_command blame' -l reverse -d 'Walk history forward instead of backward'
complete -f -c git -n '__fish_git_using_command blame' -s p -l porcelain -d 'Show in a format designed for machine consumption'
complete -f -c git -n '__fish_git_using_command blame' -l line-porcelain -d 'Show the porcelain format'
complete -f -c git -n '__fish_git_using_command blame' -l incremental -d 'Show the result incrementally'
complete -r -c git -n '__fish_git_using_command blame' -l contents -d 'Instead of working tree, use the contents of the named file'
complete -x -c git -n '__fish_git_using_command blame' -l date -d 'Specifies the format used to output dates'
complete -f -c git -n '__fish_git_using_command blame' -s M -d 'Detect moved or copied lines within a file'
complete -f -c git -n '__fish_git_using_command blame' -s C -d 'Detect lines moved or copied from other files that were modified in the same commit'
complete -f -c git -n '__fish_git_using_command blame' -s h -d 'Show help message'
complete -f -c git -n '__fish_git_using_command blame' -s c -d 'Use the same output mode as git-annotate'
complete -f -c git -n '__fish_git_using_command blame' -s f -l show-name -d 'Show the filename in the original commit'
complete -f -c git -n '__fish_git_using_command blame' -s n -l show-number -d 'Show the line number in the original commit'
complete -f -c git -n '__fish_git_using_command blame' -s s -d 'Suppress the author name and timestamp from the output'
complete -f -c git -n '__fish_git_using_command blame' -s e -l show-email -d 'Show the author email instead of author name'
complete -f -c git -n '__fish_git_using_command blame' -s w -d 'Ignore whitespace changes'


## Custom commands (git-* commands installed in the PATH)
complete -c git -n '__fish_git_needs_command' -a '(__fish_git_custom_commands)' -d 'Custom command'
