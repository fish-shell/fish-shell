# fish completion for git

# Use this instead of calling git directly; it passes the commands that are
# already present on the commandline to git. This is relevant for --work-tree etc, see issue #6219.
function __fish_git
    set -l saved_args $argv
    set -l global_args
    set -l cmd (commandline -xpc)
    # We assume that git is the first command until we have a better awareness of subcommands, see #2705.
    set -e cmd[1]
    if argparse -s (__fish_git_global_optspecs) -- $cmd 2>/dev/null
        # All arguments that were parsed by argparse are global git options.
        set -l num_global_args (math (count $cmd) - (count $argv))
        if test $num_global_args -ne 0
            set global_args $cmd[1..$num_global_args]
        end
    end
    # Using 'command git' to avoid interactions for aliases from git to (e.g.) hub
    command git $global_args $saved_args 2>/dev/null
end

# Print an optspec for argparse to handle git's options that are independent of any subcommand.
function __fish_git_global_optspecs
    string join \n v-version h/help C= c=+ 'e-exec-path=?' H-html-path M-man-path I-info-path p/paginate \
        P/no-pager o-no-replace-objects b-bare G-git-dir= W-work-tree= N-namespace= S-super-prefix= \
        l-literal-pathspecs g-glob-pathspecs O-noglob-pathspecs i-icase-pathspecs
end

function __fish_git_commits
    # Complete commits with their subject line as the description
    # This allows filtering by subject with the new pager!
    # Because even subject lines can be quite long,
    # trim them (abbrev'd hash+tab+subject) to 73 characters
    #
    # Hashes we just truncate ourselves to 10 characters, without disambiguating.
    # That technically means that sometimes we don't give usable SHAs,
    # but according to https://stackoverflow.com/a/37403152/3150338,
    # that happens for 3 commits out of 600k.
    # For fish, at the time of writing, out of 12200 commits, 7 commits need 8 characters.
    # And since this takes about 1/3rd of the time that disambiguating takes...
    __fish_git log --no-show-signature --pretty=tformat:"%H"\t"%<(64,trunc)%s" --all --max-count=1000 2>/dev/null \
        | string replace -r '^([0-9a-f]{10})[0-9a-f]*\t(.*)' '$1\t$2'
end

function __fish_git_recent_commits
    # Like __fish_git_commits, but not on all branches and limited to
    # the last 50 commits. Used for fixup, where only the current branch
    # and the latest commits make sense.
    __fish_git log --no-show-signature --pretty=tformat:"%h"\t"%<(64,trunc)%s" --max-count=50 $argv 2>/dev/null
end

function __fish_git_branches
    # This is much faster than using `git branch` and avoids dealing with localized "detached HEAD" messages.
    # We intentionally only sort local branches by recency. See discussion in #9248.
    __fish_git for-each-ref --format='%(refname:strip=2)%09Local Branch' --sort=-committerdate refs/heads/ 2>/dev/null
    __fish_git for-each-ref --format='%(refname:strip=2)%09Remote Branch' refs/remotes/ 2>/dev/null
end

function __fish_git_submodules
    __fish_git submodule 2>/dev/null \
        | string replace -r '^.[^ ]+ ([^ ]+).*$' '$1'
end

function __fish_git_local_branches
    __fish_git for-each-ref --format='%(refname:strip=2)%09Local Branch' --sort=-committerdate refs/heads/ 2>/dev/null
end

function __fish_git_unique_remote_branches
    # `git checkout` accepts remote branches without the remote part
    # if they are unambiguous.
    # E.g. if only alice has a "frobulate" branch
    # `git checkout frobulate` is equivalent to `git checkout -b frobulate --track alice/frobulate`.
    __fish_git for-each-ref --format="%(refname:strip=3)" \
        --sort="refname:strip=3" \
        "refs/remotes/*/$match*" "refs/remotes/*/*/**" 2>/dev/null | uniq -u
end

function __fish_git_tags
    __fish_git tag --sort=-creatordate 2>/dev/null
end

function __fish_git_heads
    set -l gitdir (__fish_git rev-parse --git-dir 2>/dev/null)
    or return # No git dir, no need to even test.
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
    __fish_git remote 2>/dev/null
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
    set -l root (__fish_git rev-parse --show-toplevel --is-bare-repository 2>/dev/null)
    or return

    # Skip bare repositories.
    test "$root[2]" = true
    and return
    or set -e root[2]

    # Cache the translated descriptions so we don't have to get it
    # once per file.
    contains -- all-staged $argv; and set -l all_staged
    contains -- unmerged $argv; and set -l unmerged
    and set -l unmerged_desc "Unmerged File"
    contains -- added $argv; or set -ql all_staged; and set -l added
    and set -l added_desc "Added file"
    contains -- modified $argv; and set -l modified
    and set -l modified_desc "Modified file"
    contains -- untracked $argv; and set -l untracked
    and set -l untracked_desc "Untracked file"
    contains -- modified-staged $argv; or set -ql all_staged; and set -l modified_staged
    and set -l staged_modified_desc "Staged modified file"
    contains -- modified-staged-deleted $argv; or set -ql modified_staged; and set -l modified_staged_deleted
    and set -l modified_staged_deleted_desc "Staged modified and deleted file"
    contains -- deleted $argv; and set -l deleted
    and set -l deleted_desc "Deleted file"
    contains -- deleted-staged $argv; or set -ql all_staged; and set -l deleted_staged
    and set -l staged_deleted_desc "Staged deleted file"
    contains -- ignored $argv; and set -l ignored
    and set -l ignored_desc "Ignored file"
    contains -- renamed $argv; and set -l renamed
    and set -l renamed_desc "Renamed file"
    contains -- copied $argv; and set -l copied
    and set -l copied_desc "Copied file"

    # git status --porcelain gives us all the info we need, in a format we don't.
    # The v2 format has better documentation and doesn't use " " to denote anything,
    # but it's only been added in git 2.11.0, which was released November 2016.

    # Also, we ignore submodules because they aren't useful as arguments (generally),
    # and they slow things down quite significantly.
    # E.g. `git reset $submodule` won't do anything (not even print an error).
    # --ignore-submodules=all was added in git 1.7.2, released July 2010.
    #
    set -l status_opt --ignore-submodules=all

    # If we aren't looking for ignored files, let git status skip them.
    # (don't use --ignored=no because that was only added in git 2.16, from Jan 2018.
    set -q ignored; and set -a status_opt --ignored

    # If we're looking for untracked files, we give untracked files even inside untracked directories.
    # This makes it nicer if e.g. you're in an untracked directory and want to just add one file.
    set -q untracked; and set -a status_opt -uall
    or set -a status_opt -uno

    # We need to set status.relativePaths to true because the porcelain v2 format still honors that,
    # and core.quotePath to false so characters > 0x80 (i.e. non-ASCII) aren't considered special.
    # We explicitly enable globs so we can use that to match the current token.
    set -l git_opt -c status.relativePaths -c core.quotePath=

    # If the token starts with `./`, we need to prepend that
    string match -q './*' -- (commandline -ct)
    and set -l rel ./
    or set -l rel

    # If the token starts with `:`, it's from the repo root
    string match -q ':*' -- (commandline -ct)
    and set -l colon 1
    or set -l colon

    # We pick the v2 format if we can, because it shows relative filenames (if used without "-z").
    # We fall back on the v1 format by reading git's _version_, because trying v2 first is too slow.
    set -l ver (__fish_git --version | string replace -rf 'git version (\d+)\.(\d+)\.?.*' '$1\n$2')
    # Version >= 2.11.* has the v2 format.
    if test "$ver[1]" -gt 2 2>/dev/null; or test "$ver[1]" -eq 2 -a "$ver[2]" -ge 11 2>/dev/null
        set -l stats (__fish_git $git_opt status --porcelain=2 $status_opt)
        if set -ql untracked
            # Fast path for untracked files - it is extremely easy to get a lot of these,
            # so we handle them first
            set -l files (string match -rg '^\? "?(.*)"?' -- $stats)
            set stats (string match -rv '^\? ' -- $stats)
            printf "$rel%s\n" $files\t$untracked_desc
            if set -ql colon[1]
                or set files (string match '../*' -- $files)
                set files (path resolve -- $files | string replace -- "$root/" ":/:")
                and printf '%s\n' $files\t$untracked_desc
            end
        end
        printf %s\n $stats | while read -la -d ' ' line
            set -l file
            set -l desc
            # The basic status format is "XY", where X is "our" state (meaning the staging area),
            # and "Y" is "their" state.
            # A "." means it's unmodified.
            switch "$line[1..2]"
                case 'u *'
                    # Unmerged
                    # "Unmerged entries have the following format; the first character is a "u" to distinguish from ordinary changed entries."
                    # "u <xy> <sub> <m1> <m2> <m3> <mW> <h1> <h2> <h3> <path>"
                    # This is first to distinguish it from normal modifications et al.
                    set -ql unmerged
                    and set file "$line[11..-1]"
                    and set desc $unmerged_desc
                case '2 .R*' '2 R.*'
                    # Renamed/Copied
                    # From the docs: "Renamed or copied entries have the following format:"
                    # "2 <XY> <sub> <mH> <mI> <mW> <hH> <hI> <X><score> <path><sep><origPath>"
                    # Since <sep> is \t, we can't really parse it unambiguously.
                    # The "-z" format would be great here!
                    set -ql renamed
                    and set file (string replace -r '\t[^\t]*' '' -- "$line[10..-1]")
                    and set desc $renamed_desc
                case '2 RM*' '2 RT*'
                    # Staged as renamed, with unstaged modifications (issue #6031)
                    set -ql renamed
                    or set -ql modified
                    and set file (string replace -r '\t[^\t]*' '' -- "$line[10..-1]")
                    set -ql renamed
                    and set desc $renamed_desc
                    set -ql modified
                    and set --append desc $modified_desc
                case '2 RD*'
                    # Staged as renamed, but deleted in the worktree
                    set -ql renamed
                    or set -ql deleted
                    and set file (string replace -r '\t[^\t]*' '' -- "$line[10..-1]")
                    set -ql renamed
                    and set desc $renamed_desc
                    set -ql deleted
                    and set --append desc $deleted_desc
                case '2 .C*' '2 C.*'
                    set -ql copied
                    and set file (string replace -r '\t[^\t].*' '' -- "$line[10..-1]")
                    and set desc $copied_desc
                case '1 A.*'
                    # Additions are only shown here if they are staged.
                    # Otherwise it's an untracked file.
                    set -ql added
                    and set file "$line[9..-1]"
                    and set desc $added_desc
                case '1 AD*'
                    # Added files that were since deleted
                    if set -ql added
                        set file "$line[9..-1]"
                        set desc $added_desc
                    else if set -ql deleted
                        set file "$line[9..-1]"
                        set desc $deleted_desc
                    end
                case "1 AM*" "1 AT*"
                    # Added files with additional modifications
                    # ("T" is type-changed. As of git 2.33 this appears to be undocumented.
                    # it happens when e.g. a file is replaced with a symlink.
                    # For our purposes it's the same as modified)
                    if set -ql added
                        set file "$line[9..-1]"
                        set desc $added_desc
                    else if set -ql modified
                        set file "$line[9..-1]"
                        set desc $modified_desc
                    end
                case '1 .A*'
                    # Files added with git add --intent-to-add.
                    set -ql untracked
                    and set file "$line[9..-1]"
                    and set desc $untracked_desc
                case '1 .M*' '1 .T*'
                    # Modified
                    # From the docs: "Ordinary changed entries have the following format:"
                    # "1 <XY> <sub> <mH> <mI> <mW> <hH> <hI> <path>"
                    # Since <path> can contain spaces, print from element 9 onwards
                    set -ql modified
                    and set file "$line[9..-1]"
                    and set desc $modified_desc
                case '1 MD*' '1 TD*'
                    set -ql modified_staged_deleted
                    and set file "$line[9..-1]"
                    and set desc $modified_staged_deleted_desc
                case '1 M.*' '1 T.*'
                    # If the character is first ("M."), then that means it's "our" change,
                    # which means it is staged.
                    # This is useless for many commands - e.g. `checkout` won't do anything with this.
                    # So it needs to be requested explicitly.
                    set -ql modified_staged
                    and set file "$line[9..-1]"
                    and set desc $staged_modified_desc
                case '1 MM*' '1 MT*' '1 TM*' '1 TT*'
                    # Staged-modified with unstaged modifications
                    # These need to be offered for both kinds of modified.
                    if set -ql modified
                        set file "$line[9..-1]"
                        set desc $modified_desc
                    else if set -ql modified_staged
                        set file "$line[9..-1]"
                        set desc $staged_modified_desc
                    end
                case '1 .D*'
                    set -ql deleted
                    and set file "$line[9..-1]"
                    and set desc $deleted_desc
                case '1 D.*'
                    # TODO: The docs are unclear on this.
                    # There is both X unmodified and Y either M or D ("not updated")
                    # and Y is D and X is unmodified or [MARC] ("deleted in work tree").
                    # For our purposes, we assume this is a staged deletion.
                    set -ql deleted_staged
                    and set file "$line[9..-1]"
                    and set desc $staged_deleted_desc
                case '! *'
                    # Ignored
                    # "! <path>" - print from element 2 on.
                    set -ql ignored
                    and set file "$line[2..-1]"
                    and set desc $ignored_desc
            end
            # Only try printing if the file was selected.
            if set -q file[1]
                # Without "-z", git sometimes _quotes_ filenames.
                # It adds quotes around it _and_ escapes the character.
                # e.g. `"a\\b"`.
                # We just remove the quotes and hope it works out.
                # If this contains newlines or tabs,
                # there is nothing we can do, but that's a general issue with scripted completions.
                set file (string trim -c \" -- $file)
                # The (possibly relative) filename.
                printf "$rel%s\n" "$file"\t$desc
                # Now from repo root.
                # Only do this if the filename isn't a simple child,
                # or the current token starts with ":"
                if set -ql colon[1]; or string match -q '../*' -- $file
                    set -l fromroot (path resolve -- $file 2>/dev/null)
                    # `:` starts pathspec "magic", and the second `:` terminates it.
                    # `/` is the magic letter for "from repo root".
                    # If we didn't terminate it we'd have to escape any special chars
                    # (non-alphanumeric, glob or regex special characters, in whatever dialect git uses)
                    and set fromroot (string replace -- "$root/" ":/:" "$fromroot")
                    and printf '%s\n' "$fromroot"\t$desc
                end
            end
        end
    else
        # v1 format logic
        # This is pretty terrible and reuqires us to do a lot of weird work.

        # A literal "?" for use in `case`.
        set -l q '\\?'
        if status test-feature qmark-noglob
            set q '?'
        end
        # Whether we need to use the next line - some entries have two lines.
        set -l use_next

        # We need to compute relative paths on our own, which is slow.
        # Pre-remove the root at least, so we have fewer components to deal with.
        set -l _pwd_list (string replace "$root/" "" -- $PWD/ | string split /)
        test -z "$_pwd_list[-1]"; and set -e _pwd_list[-1]
        # Cache the previous relative path because these are sorted, so we can reuse it
        # often for files in the same directory.
        set -l previous
        # Note that we can't use space as a delimiter between status and filename, because
        # the status can contain spaces - " M" is different from "M ".
        __fish_git $git_opt status --porcelain -z $status_opt \
            | while read -lz -d' ' line
            set -l desc
            # The entire line is the "from" from a rename.
            if set -q use_next[1]
                if contains -- $use_next $argv
                    set -l var "$use_next"_desc
                    set desc $$var
                    set -e use_next[1]
                else
                    set -e use_next[1]
                    continue
                end
            end

            # The format is two characters for status, then a space and then
            # up to a NUL for the filename.
            #
            set -l stat (string sub -l 2 -- $line)
            # The basic status format is "XY", where X is "our" state (meaning the staging area),
            # and "Y" is "their" state (meaning the work tree).
            # A " " means it's unmodified.
            #
            # Be careful about the ordering here!
            switch "$stat"
                case DD AU UD UA DU AA UU
                    # Unmerged
                    set -ql unmerged
                    and set desc $unmerged_desc
                case 'R ' RM RD
                    # Renamed/Copied
                    # These have the "from" name as the next batch.
                    # TODO: Do we care about the new name?
                    set use_next renamed
                    continue
                case 'C ' CM CD
                    set use_next copied
                    continue
                case AM
                    if set -ql added
                        set file "$line[9..-1]"
                        set desc $added_desc
                    else if set -ql modified
                        set file "$line[9..-1]"
                        set desc $modified_desc
                    end
                case AD
                    if set -ql added
                        set file "$line[9..-1]"
                        set desc $added_desc
                    else if set -ql deleted
                        set file "$line[9..-1]"
                        set desc $deleted_desc
                    end
                case 'A '
                    # Additions are only shown here if they are staged.
                    # Otherwise it's an untracked file.
                    set -ql added
                    and set desc $added_desc
                case '*M'
                    # Modified
                    set -ql modified
                    and set desc $modified_desc
                case 'M*'
                    # If the character is first ("M "), then that means it's "our" change,
                    # which means it is staged.
                    # This is useless for many commands - e.g. `checkout` won't do anything with this.
                    # So it needs to be requested explicitly.
                    set -ql modified_staged
                    and set desc $staged_modified_desc
                case '*D'
                    set -ql deleted
                    and set desc $deleted_desc
                case 'D*'
                    # TODO: The docs are unclear on this.
                    # There is both X unmodified and Y either M or D ("not updated")
                    # and Y is D and X is unmodified or [MARC] ("deleted in work tree").
                    # For our purposes, we assume this is a staged deletion.
                    set -ql deleted_staged
                    and set desc $staged_deleted_desc
                case "$q$q"
                    # Untracked
                    set -ql untracked
                    and set desc $untracked_desc
                case '!!'
                    # Ignored
                    set -ql ignored
                    and set desc $ignored_desc
            end
            if set -q desc[1]
                # Again: "XY filename", so the filename starts on character 4.
                set -l relfile (string sub -s 4 -- $line)

                set -l file
                # Computing relative path by hand.
                set -l abs (string split / -- $relfile)
                # If it's in the same directory, we just need to change the filename.
                if test "$abs[1..-2]" = "$previous[1..-2]"
                    # If we didn't have a previous file, and the current file is in the current directory,
                    # this would error out.
                    #
                    # See #5728.
                    set -q previous[1]
                    and set previous[-1] $abs[-1]
                    or set previous $abs
                else
                    set -l pwd_list $_pwd_list
                    # Remove common prefix
                    while test "$pwd_list[1]" = "$abs[1]"
                        set -e pwd_list[1]
                        set -e abs[1]
                    end
                    # Go a dir up for every entry left in pwd_list, then into $abs
                    set previous (string replace -r '.*' '..' -- $pwd_list) $abs
                end
                set -a file (string join / -- $previous)

                # The filename with ":/:" prepended.
                if set -ql colon[1]; or string match -q '../*' -- $file
                    set file (string replace -- "$root/" ":/:" "$root/$relfile")
                end

                if test "$root/$relfile" -ef "$relfile"
                    and not set -ql colon[1]
                    set file $relfile
                end

                printf '%s\n' $file\t$desc
            end
        end
    end
end

# Lists files included in the index of a commit, branch, or tag (not necessarily HEAD)
function __fish_git_rev_files
    set -l rev $argv[1]
    set -l path $argv[2]

    # Strip any partial files from the path before handing it to `git show`
    set -l path (string replace -r -- '(.*/|).*' '$1' $path)

    # List files in $rev's index, skipping the "tree ..." header, but appending
    # the parent path, which git does not include in the output (and fish requires)
    string join \n -- $path(__fish_git show $rev:$path | sed '1,2d')
end

# Provides __fish_git_rev_files completions for the current token
function __fish_git_complete_rev_files
    set -l split (string split -m 1 ":" -- (commandline -xt))
    set -l rev $split[1]
    set -l path $split[2]

    printf "$rev:%s\n" (__fish_git_rev_files $rev $path)
end

# Determines whether we can/should complete with __fish_git_rev_files
function __fish_git_needs_rev_files
    # git (as of 2.20) accepts the rev:path syntax for a number of subcommands,
    # but then doesn't emit the expected (or any?) output, e.g. `git log master:foo`
    #
    # This definitely works with `git show` to retrieve a copy of a file as it exists
    # in the index of revision $rev, it should be updated to include others as they
    # are identified.
    __fish_git_using_command show; and string match -r "^[^-].*:" -- (commandline -xt)
end

function __fish_git_ranges
    set -l both (commandline -xt | string replace -r '\.{2,3}' \n\$0\n)
    set -l from $both[1]
    set -l dots $both[2]
    # If we didn't need to split (or there's nothing _to_ split), complete only the first part
    # Note that status here is from `string replace` because `set` doesn't alter it
    if test -z "$from" -o $status -gt 0
        if commandline -ct | string match -q '*..*'
            # The cursor is right of a .. range operator, make sure to include them first.
            __fish_git_refs | string replace -r '' "$dots"
        else
            __fish_git_refs | string replace \t "$dots"\t
        end
        return 0
    end

    set -l from_refs
    if commandline -ct | string match -q '*..*'
        # If the cursor is right of a .. range operator, only complete the right part.
        set from_refs $from
    else
        set from_refs (__fish_git_refs | string match -e "$from" | string replace -r \t'.*$' '')
    end

    set -l to $both[3]
    # Remove description from the from-ref, not the to-ref.
    for from_ref in $from_refs
        for to_ref in (__fish_git_refs | string match "*$to*") # if $to is empty, this correctly matches everything
            printf "%s%s%s\n" $from_ref $dots $to_ref
        end
    end
end

function __fish_git_needs_command
    # Figure out if the current invocation already has a command.
    #
    # This is called hundreds of times and the argparse is kinda slow,
    # so we cache it as long as the commandline doesn't change.
    set -l cmdline "$(commandline -c)"
    if set -q __fish_git_cmdline; and test "$cmdline" = "$__fish_git_cmdline"
        if set -q __fish_git_cmd[1]
            echo -- $__fish_git_cmd
            return 1
        end
        return 0
    end
    set -g __fish_git_cmdline $cmdline

    set -l cmd (commandline -xpc)
    set -e cmd[1]
    argparse -s (__fish_git_global_optspecs) -- $cmd 2>/dev/null
    or return 0
    # These flags function as commands, effectively.
    set -q _flag_version; and return 1
    set -q _flag_html_path; and return 1
    set -q _flag_man_path; and return 1
    set -q _flag_info_path; and return 1
    if set -q argv[1]
        # Also print the command, so this can be used to figure out what it is.
        set -g __fish_git_cmd $argv[1]
        echo $argv[1]
        return 1
    end
    set -g __fish_git_cmd
    return 0
end

function __fish_git_config_keys
    # Print already defined config values first
    # Config keys may span multiple lines, so parse using null char
    # With -z, key and value are separated by space, not "="
    __fish_git config -lz | while read -lz key value
        # Print only first line of value(with an ellipsis) if multiline
        printf '%s\t%s\n' $key (string shorten -N -- $value)
    end
    # Print all recognized config keys; duplicates are not shown twice by fish
    printf '%s\n' (__fish_git help --config)[1..-2] # Last line is a footer; ignore it
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

# Approximately duplicates the logic from https://github.com/git/git/blob/d486ca60a51c9cb1fe068803c3f540724e95e83a/contrib/completion/git-completion.bash#L1130
# The bash script also finds aliases that reference other aliases via a loop but we handle that separately
function __fish_git_aliased_command
    for word in (string split ' ' -- $argv)
        switch $word
            case !gitk gitk
                echo gitk
                return
                # Adding " to the list
            case '!*' '-*' '*=*' git '()' '{' : '\'*' '"*'
                continue
            case '*'
                echo $word
                return
        end
    end
end

git config -z --get-regexp 'alias\..*' | while read -lz alias cmdline
    set -l command (__fish_git_aliased_command $cmdline)
    string match -q --regex '\w+' -- $command; or continue
    # Git aliases can contain chars that variable names can't - escape them.
    set -l alias (string replace 'alias.' '' -- $alias | string escape --style=var)
    set -g __fish_git_alias_$alias $command $cmdline
    set --append -g __fish_git_aliases $alias
end

# Resolve aliases that call another alias
for alias in $__fish_git_aliases
    set -l handled $alias

    while true
        set -l alias_varname __fish_git_alias_$alias
        set -l aliased_command $$alias_varname[1][1]
        set -l aliased_escaped (string escape --style=var -- $aliased_command)
        set -l aliased_varname __fish_git_alias_$aliased_escaped
        set -q $aliased_varname
        or break

        # stop infinite recursion
        contains $aliased_escaped $handled
        and break

        # expand alias in cmdline
        set -l aliased_cmdline $$alias_varname[1][2]
        set -l aliased_cmdline (string replace " $aliased_command " " $$aliased_varname[1][2..-1] " -- " $aliased_cmdline ")
        set -g $alias_varname $$aliased_varname[1][1] (string trim "$aliased_cmdline")
        set --append handled $aliased_escaped
    end
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
    and contains -- $$varname[1][1] $argv
    and return 0
    return 1
end

function __fish_git_contains_opt
    # Check if an option has been given
    # First check the commandline normally
    __fish_contains_opt $argv
    and return

    # Now check the alias
    argparse s= -- $argv
    set -l cmd (__fish_git_needs_command)
    set -l varname __fish_git_alias_(string escape --style=var -- $cmd)
    if set -q $varname
        echo -- $$varname | read -lat toks
        set toks (string replace -r '(-.*)=.*' '' -- $toks)
        for i in $argv
            if contains -- --$i $toks
                return 0
            end
        end

        for i in $_flag_s
            if string match -qr -- "^-$i|^-[^-]*$i" $toks
                return 0
            end
        end
    end

    return 1
end
function __fish_git_stash_using_command
    set -l cmd (commandline -xpc)
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
    set -l cmd (commandline -xpc)
    __fish_git_using_command stash
    or return 2
    set cmd $cmd[(contains -i -- "stash" $cmd)..-1]
    set -q cmd[2]
    and return 1
    return 0
end

function __fish_git_complete_worktrees
    __fish_git worktree list --porcelain | string replace --regex --filter '^worktree\s*' ''
end

function __fish_git_complete_stashes
    __fish_git stash list --format=%gd:%gs 2>/dev/null | string replace ":" \t
end

function __fish_git_aliases
    __fish_git config -z --get-regexp '^alias\.' 2>/dev/null | while read -lz key value
        begin
            set -l name (string replace -r '^.*\.' '' -- $key)
            set -l val (string shorten --no-newline -m 36 -- $value)
            printf "%s\t%s\n" $name "alias: $val"
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
    set -l cmd (commandline -xpc)
    for r in $remotes
        if contains -- $r $cmd
            set remote $r
            break
        end
    end
    set -q remote[1]
    or return 1
    __fish_git_branches | string replace -f -- "$remote/" ''
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
    __fish_git reflog --no-decorate 2>/dev/null | string replace -r '[0-9a-f]* (.+@\{[0-9]+\}): (.*)$' '$1\t$2'
end

function __fish_git_help_all_concepts
    git help -g | string match -e -r '^   \w+' | while read -l concept desc
        printf "%s\tConcept: %s\n" $concept (string trim $desc)
    end
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

function __fish_git_show_opt -a option
    switch $option
        case format pretty
            printf "%b" "
oneline\t<sha1> <title line>
short\t<sha1> / <author> / <title line>
medium\t<sha1> / <author> / <author date> / <title> / <commit msg>
full\t<sha1> / <author> / <committer> / <title> / <commit msg>
fuller\t<sha1> / <author> / <author date> / <committer> / <committer date> / <title> / <commit msg>
reference\t<abbrev-hash> (<title-line>, <short-author-date>)
email\t<sha1> <date> / <author> / <author date> / <title> / <commit msg>
mboxrd\tLike email, but lines in the commit message starting with \"From \" are quoted with \">\"
raw\tShow the entire commit exactly as stored in the commit object
format:\tSpecify which information to show
"
            __fish_git config -z --get-regexp '^pretty\.' 2>/dev/null | while read -lz key value
                set -l name (string replace -r '^.*?\.' '' -- $key)
                printf "%s\t%s\n" $name $value
            end
    end
end

function __fish_git_is_rebasing
    test -e (__fish_git rev-parse --absolute-git-dir)/rebase-merge
end

function __fish_git_filters
    printf "%s\n" \
        blob:none\t"omits all blobs" \
        blob:limit=\t"omits blobs by size" \
        object:type={tag,commit,tree,blob}\t"omit object which are not of the requested type" \
        sparse:oid=\t"omit blobs not required for a sparse checkout" \
        tree:\t"omits all blobs and trees"
end

# general options
complete git -f -l help -s h -d 'Display manual of a Git command'
complete git -f -n __fish_git_needs_command -l version -s v -d 'display git version'
complete git -x -n __fish_git_needs_command -s C -a '(__fish_complete_directories)' -d 'run as if started in dir'
complete git -x -n __fish_git_needs_command -s c -a '(__fish_git config -l 2>/dev/null | string replace = \t)' -d 'set config option (conf-key=val)'
complete git -x -n __fish_git_needs_command -l config-env -a '(__fish_git config -l 2>/dev/null | string replace = \t)' -d 'like -c but environment var (conf-key=ENVVAR)'
complete git -x -n __fish_git_needs_command -l exec-path -a '(__fish_complete_directories)' -d 'get or set the path to git'
complete git -f -n __fish_git_needs_command -l html-path -d 'print html docs path'
complete git -f -n __fish_git_needs_command -l man-path -d 'print man pages path'
complete git -f -n __fish_git_needs_command -l info-path -d 'print info docs path'
complete git -f -n __fish_git_needs_command -s p -l paginate -d 'pipe output into pager'
complete git -f -n __fish_git_needs_command -s P -l no-pager -d 'don\'t pipe output into pager'
complete git -r -n __fish_git_needs_command -l git-dir -d 'set path to the repo'
complete git -r -n __fish_git_needs_command -l work-tree -d 'set path to the working tree'
complete git -f -n __fish_git_needs_command -l namespace -d 'set Git namespace'
complete git -f -n __fish_git_needs_command -l bare -d 'treat the repo as bare'
complete git -f -n __fish_git_needs_command -l no-replace-objects -d 'disable replacement references'
complete git -f -n __fish_git_needs_command -l literal-pathspecs -d 'treat pathspecs literally'
complete git -f -n __fish_git_needs_command -l glob-pathspecs -d 'treat pathspecs as globs'
complete git -f -n __fish_git_needs_command -l noglob-pathspecs -d 'don\'t treat pathspecs as globs'
complete git -f -n __fish_git_needs_command -l icase-pathspecs -d 'match pathspecs case-insensitively'
complete git -f -n __fish_git_needs_command -l no-optional-locks -d 'skip optional operations requiring locks'
complete git -x -n __fish_git_needs_command -l list-cmds -d 'list commands by group' -k -a "builtins\t
parseopt\t'builtins using parse-options'
others\t'git- commands in \$PATH'
nohelpers\t'exclude helper commands'
config\t'list completion.commands'"

# Options shared between multiple commands
complete -f -c git -n '__fish_git_using_command log show diff-tree rev-list' -l pretty -a '(__fish_git_show_opt pretty)'

complete -c git -n '__fish_git_using_command diff show range-diff' -l abbrev -d 'Show only a partial prefix instead of the full 40-byte hexadecimal object name'
complete -c git -n '__fish_git_using_command diff show range-diff' -l binary -d 'Output a binary diff that can be applied with "git-apply"'
complete -c git -n '__fish_git_using_command diff show range-diff' -l check -d 'Warn if changes introduce conflict markers or whitespace errors'
complete -c git -n '__fish_git_using_command diff show range-diff' -l color -d 'Show colored diff'
complete -x -c git -n '__fish_git_using_command diff show range-diff' -l color-moved -d 'Moved lines of code are colored differently' -a 'no default plain blocks zebra dimmed-zebra'
complete -x -c git -n '__fish_git_using_command diff show range-diff' -l color-moved-ws -d 'Ignore whitespace when using --color-moved' -a 'no ignore-space-at-eol ignore-space-change ignore-all-space allow-indentation-change'
complete -c git -n '__fish_git_using_command diff show range-diff' -l color-words -d 'Equivalent to --word-diff=color plus --word-diff-regex=<regex>'
complete -c git -n '__fish_git_using_command diff show range-diff' -l compact-summary -d 'Output a condensed summary of extended header information'
complete -c git -n '__fish_git_using_command diff log show range-diff' -l dst-prefix -d 'Show the given destination prefix instead of "b/"'
complete -c git -n '__fish_git_using_command diff show range-diff' -l ext-diff -d 'Allow an external diff helper to be executed'
complete -c git -n '__fish_git_using_command diff show range-diff' -l find-copies-harder -d 'Inspect unmodified files as candidates for the source of copy'
complete -c git -n '__fish_git_using_command diff show range-diff' -l find-object -d 'Look for differences that change the number of occurrences of the object'
complete -c git -n '__fish_git_using_command diff show range-diff' -l full-index -d 'Show the full pre- and post-image blob object names on the "index" line'
complete -c git -n '__fish_git_using_command diff show range-diff' -l histogram -d 'Generate a diff using the "histogram diff" algorithm'
complete -c git -n '__fish_git_using_command diff show range-diff' -l ignore-blank-lines -d 'Ignore changes whose lines are all blank'
complete -c git -n '__fish_git_using_command diff show range-diff' -l ignore-cr-at-eol -d 'Ignore carrige-return at the end of line when doing a comparison'
complete -c git -n '__fish_git_using_command diff show range-diff' -l ignore-space-at-eol -d 'Ignore changes in whitespace at EOL'
complete -c git -n '__fish_git_using_command diff show range-diff' -l indent-heuristic -d 'Enable the heuristic that shift diff hunk boundaries'
complete -c git -n '__fish_git_using_command diff log show range-diff' -l inter-hunk-context -d 'Show the context between diff hunks, up to the specified number of lines'
complete -c git -n '__fish_git_using_command diff log show range-diff' -l ita-invisible-in-index -d 'Make the entry appear as a new file in "git diff" and non-existent in "git diff -l cached"'
complete -c git -n '__fish_git_using_command diff log show range-diff' -l line-prefix -d 'Prepend an additional prefix to every line of output'
complete -c git -n '__fish_git_using_command diff show range-diff' -l minimal -d 'Spend extra time to make sure the smallest possible diff is produced'
complete -c git -n '__fish_git_using_command diff show range-diff' -l name-only -d 'Show only names of changed files'
complete -c git -n '__fish_git_using_command diff show range-diff' -l name-status -d 'Show only names and status of changed files'
complete -c git -n '__fish_git_using_command diff show range-diff' -l no-color -d 'Turn off colored diff'
complete -c git -n '__fish_git_using_command diff show range-diff' -l no-ext-diff -d 'Disallow external diff drivers'
complete -c git -n '__fish_git_using_command diff show range-diff' -l no-indent-heuristic -d 'Disable the indent heuristic'
complete -c git -n '__fish_git_using_command diff log show range-diff' -l no-prefix -d 'Do not show any source or destination prefix'
complete -c git -n '__fish_git_using_command diff show range-diff' -l no-renames -d 'Turn off rename detection'
complete -c git -n '__fish_git_using_command diff show range-diff' -l no-textconv -d 'Disallow external text conversion filters to be run when comparing binary files'
complete -c git -n '__fish_git_using_command diff show range-diff' -l numstat -d 'Shows number of added/deleted lines in decimal notation'
complete -c git -n '__fish_git_using_command diff show range-diff' -l patch-with-raw -d 'Synonym for -p --raw'
complete -c git -n '__fish_git_using_command diff show range-diff' -l patch-with-stat -d 'Synonym for -p --stat'
complete -c git -n '__fish_git_using_command diff show range-diff' -l patience -d 'Generate a diff using the "patience diff" algorithm'
complete -c git -n '__fish_git_using_command diff log show range-diff' -l pickaxe-all -d 'When -S or -G finds a change, show all the changes in that changeset'
complete -c git -n '__fish_git_using_command diff log show range-diff' -l pickaxe-regex -d 'Treat the <string> given to -S as an extended POSIX regular expression to match'
complete -c git -n '__fish_git_using_command diff log show range-diff' -l relative -d 'Exclude changes outside the directory and show relative pathnames'
complete -c git -n '__fish_git_using_command diff show range-diff' -l shortstat -d 'Output only the last line of the --stat format containing the total'
complete -c git -n '__fish_git_using_command diff log show range-diff' -l src-prefix -d 'Show the given source prefix instead of "a/"'
complete -c git -n '__fish_git_using_command diff show range-diff' -l stat -d 'Generate a diffstat'
complete -c git -n '__fish_git_using_command diff show range-diff apply' -l stat -d 'Generate a diffstat'
complete -c git -n '__fish_git_using_command diff show range-diff' -l summary -d 'Output a condensed summary of extended header information'
complete -c git -n '__fish_git_using_command diff show range-diff' -l textconv -d 'Allow external text conversion filters to be run when comparing binary files'
complete -c git -n '__fish_git_using_command diff show range-diff' -l word-diff -d 'Show a word diff'
complete -c git -n '__fish_git_using_command diff show range-diff' -l word-diff-regex -d 'Use <regex> to decide what a word is'
complete -c git -n '__fish_git_using_command diff log show range-diff' -s a -l text -d 'Treat all files as text'
complete -c git -n '__fish_git_using_command diff log show range-diff' -s B -l break-rewrites -d 'Break complete rewrite changes into pairs of delete and create'
complete -c git -n '__fish_git_using_command diff show range-diff' -s b -l ignore-space-change -d 'Ignore changes in amount of whitespace'
complete -c git -n '__fish_git_using_command diff log show range-diff' -s C -l find-copies -d 'Detect copies as well as renames'
complete -c git -n '__fish_git_using_command diff log show range-diff' -s D -l irreversible-delete -d 'Omit the preimage for deletes'
complete -c git -n '__fish_git_using_command diff log show range-diff' -s G -d "Look for differences where <regex> matches the patch's added/removed lines"
complete -c git -n '__fish_git_using_command diff log show range-diff' -s M -l find-renames -d 'Detect and report renames'
complete -c git -n '__fish_git_using_command diff log show range-diff' -s R -d 'Swap inputs to create a reverse diff'
complete -c git -n '__fish_git_using_command diff log show range-diff' -s S -d 'Look for differences that change the number of occurrences of the string'
complete -c git -n '__fish_git_using_command diff show range-diff' -s W -l function-context -d 'Show whole surrounding functions of changes'
complete -c git -n '__fish_git_using_command diff show range-diff' -s w -l ignore-all-space -d 'Ignore whitespace when comparing lines'
complete -c git -n '__fish_git_using_command diff show range-diff' -s z -d 'Use NULs as output field/commit terminators'
complete -r -c git -n '__fish_git_using_command diff log show range-diff' -s O -d 'Control the order in which files appear in the output'
complete -f -c git -n '__fish_git_using_command diff show range-diff' -l anchored -d 'Generate a diff using the "anchored diff" algorithm'
complete -x -c git -n '__fish_git_using_command diff log show range-diff' -s l -d 'Prevents rename/copy detection when rename/copy targets exceed the given number'
complete -x -c git -n '__fish_git_using_command diff show range-diff' -l diff-filter -a '(__fish_git_diff_opt diff-filter)' -d 'Choose diff filters'
complete -x -c git -n '__fish_git_using_command diff log show range-diff' -l diff-algorithm -a '(__fish_git_diff_opt diff-algorithm)' -d 'Choose a diff algorithm'
complete -x -c git -n '__fish_git_using_command diff log show range-diff' -l dirstat -a '(__fish_git_diff_opt dirstat)' -d 'Output the distribution of relative amount of changes for each sub-directory'
complete -x -c git -n '__fish_git_using_command diff log show range-diff' -l ignore-submodules -a '(__fish_git_diff_opt ignore-submodules)' -d 'Ignore changes to submodules in the diff generation'
complete -x -c git -n '__fish_git_using_command diff log show range-diff' -l submodule -a '(__fish_git_diff_opt submodule)' -d 'Specify how differences in submodules are shown'
complete -x -c git -n '__fish_git_using_command diff log show range-diff' -l ws-error-highlight -a '(__fish_git_diff_opt ws-error-highlight)' -d 'Highlight whitespace errors in lines of the diff'

complete -f -c git -n '__fish_git_using_command fetch pull' -l unshallow -d 'Convert a shallow repository to a complete one'
complete -f -c git -n '__fish_git_using_command fetch pull' -l set-upstream -d 'Add upstream (tracking) reference'

#### fetch
complete -f -c git -n __fish_git_needs_command -a fetch -d 'Download objects from another repo'
# Suggest "repository", then "refspec" - this also applies to e.g. push/pull
complete -f -c git -n '__fish_git_using_command fetch' -n 'not __fish_git_branch_for_remote' -a '(__fish_git_remotes)' -d Remote
complete -f -c git -n '__fish_git_using_command fetch' -n __fish_git_branch_for_remote -ka '(__fish_git_branch_for_remote)'
complete -f -c git -n '__fish_git_using_command fetch' -s q -l quiet -d 'Be more quiet'
complete -f -c git -n '__fish_git_using_command fetch' -s v -l verbose -d 'Be more verbose'
complete -f -c git -n '__fish_git_using_command fetch' -s a -l append -d 'Append to .git/FETCH_HEAD instead of overwriting'
complete -f -c git -n '__fish_git_using_command fetch' -l upload-pack -d 'Path to upload pack on remote end'
complete -f -c git -n '__fish_git_using_command fetch' -s f -l force -d 'Force update of local branches'
complete -f -c git -n '__fish_git_using_command fetch' -s p -l prune -d 'Prune remote-tracking branches no longer on remote'
complete -f -c git -n '__fish_git_using_command fetch' -l all -d 'Fetch all remotes'
complete -f -c git -n '__fish_git_using_command fetch' -l atomic -d 'Use atomic transfer to update references'
complete -f -c git -n '__fish_git_using_command fetch' -s m -l multiple -d 'Fetch from multiple remotes'
complete -f -c git -n '__fish_git_using_command fetch' -s t -l tags -d 'Fetch all tags and associated objects'
complete -f -c git -n '__fish_git_using_command fetch' -s P -l prune-tags -d 'Prune local tags no longer on remote and clobber changed tags'
complete -f -c git -n '__fish_git_using_command fetch' -l prefetch -d 'Modify the refspec to replace all refs within refs/prefetch/'
complete -f -c git -n '__fish_git_using_command fetch' -s j -l jobs -d 'Numbers of submodules fetched in parallel'
complete -f -c git -n '__fish_git_using_command fetch' -s n -d 'Do not fetch all tags (--no-tags)'
complete -f -c git -n '__fish_git_using_command fetch' -l dry-run -d 'Dry run'
complete -f -c git -n '__fish_git_using_command fetch' -l depth -d 'Limit number of commits'
complete -f -c git -n '__fish_git_using_command fetch' -l with-fetch-head -d 'Write fetched references to the FETCH_HEAD file'
complete -f -c git -n '__fish_git_using_command fetch' -l update-shallow -d 'Accept refs that update .git/shallow'
complete -f -c git -n '__fish_git_using_command fetch' -s k -l keep -d 'Keep downloaded pack'
complete -f -c git -n '__fish_git_using_command fetch' -s u -l update-head-ok -d 'Allow updating of HEAD ref'
complete -f -c git -n '__fish_git_using_command fetch' -l progress -d 'Force progress reporting'
complete -f -c git -n '__fish_git_using_command fetch' -l deepen -d 'Deepen history of shallow clones'
complete -f -c git -n '__fish_git_using_command fetch' -l shallow-since -d 'Deepen history of shallow repository based on time'
complete -f -c git -n '__fish_git_using_command fetch' -l shallow-exclude -d 'Deepen history of shallow clone, excluding rev'
complete -f -c git -n '__fish_git_using_command fetch' -l unshallow -d 'Convert to a complete repository'
complete -f -c git -n '__fish_git_using_command fetch' -l refetch -d 'Re-fetch without negotiating common commits'
complete -f -c git -n '__fish_git_using_command fetch' -l negotiation-tip -d 'Only report commits reachable from these tips' -kxa '(__fish_git_commits; __fish_git_branches)'
complete -f -c git -n '__fish_git_using_command fetch' -l negotiate-only -d "Don't fetch, only show commits in common with the server"
complete -f -c git -n '__fish_git_using_command fetch' -l filter -ra '(__fish_git_filters)' -d 'Request a subset of objects from server'

# TODO other options

#### filter-branch
complete -f -c git -n __fish_git_needs_command -a filter-branch -d 'Rewrite branches'
complete -f -c git -n '__fish_git_using_command filter-branch' -l env-filter -d 'Filter for rewriting env vars like author name/email'
complete -f -c git -n '__fish_git_using_command filter-branch' -l tree-filter -d 'Filter for rewriting the tree and its contents'
complete -f -c git -n '__fish_git_using_command filter-branch' -l index-filter -d 'Filter for rewriting the index'
complete -f -c git -n '__fish_git_using_command filter-branch' -l parent-filter -d 'Filter for rewriting the commit'
complete -f -c git -n '__fish_git_using_command filter-branch' -l msg-filter -d 'Filter for rewriting the commit messages'
complete -f -c git -n '__fish_git_using_command filter-branch' -l commit-filter -d 'Filter for performing the commit'
complete -f -c git -n '__fish_git_using_command filter-branch' -l tag-name-filter -d 'Filter for rewriting tag names'
complete -f -c git -n '__fish_git_using_command filter-branch' -l subdirectory-filter -d 'Only look at the history which touches the given subdirectory'
complete -f -c git -n '__fish_git_using_command filter-branch' -l prune-empty -d 'Ignore empty commits generated by filters'
complete -f -c git -n '__fish_git_using_command filter-branch' -l original -d 'Use this option to set the namespace where the original commits will be stored'
complete -r -c git -n '__fish_git_using_command filter-branch' -s d -d 'Use this option to set the path to the temporary directory used for rewriting'
complete -c git -n '__fish_git_using_command filter-branch' -s f -l force -d 'Filter even with refs in refs/original or existing temp directory'

### remote
set -l remotecommands add rm remove show prune update rename set-head set-url set-branches get-url
complete -f -c git -n __fish_git_needs_command -a remote -d 'Manage tracked repositories'
complete -f -c git -n "__fish_git_using_command remote" -n "__fish_seen_subcommand_from $remotecommands" -a '(__fish_git_remotes)'
complete -f -c git -n "__fish_git_using_command remote" -n "not __fish_seen_subcommand_from $remotecommands" -s v -l verbose -d 'Be verbose'
complete -f -c git -n "__fish_git_using_command remote" -n "not __fish_seen_subcommand_from $remotecommands" -a add -d 'Adds a new remote'
complete -f -c git -n "__fish_git_using_command remote" -n "not __fish_seen_subcommand_from $remotecommands" -a rm -d 'Removes a remote'
complete -f -c git -n "__fish_git_using_command remote" -n "not __fish_seen_subcommand_from $remotecommands" -a remove -d 'Removes a remote'
complete -f -c git -n "__fish_git_using_command remote" -n "not __fish_seen_subcommand_from $remotecommands" -a show -d 'Shows a remote'
complete -f -c git -n "__fish_git_using_command remote" -n "not __fish_seen_subcommand_from $remotecommands" -a prune -d 'Deletes all stale tracking branches'
complete -f -c git -n "__fish_git_using_command remote" -n "not __fish_seen_subcommand_from $remotecommands" -a update -d 'Fetches updates'
complete -f -c git -n "__fish_git_using_command remote" -n "not __fish_seen_subcommand_from $remotecommands" -a rename -d 'Renames a remote'
complete -f -c git -n "__fish_git_using_command remote" -n "not __fish_seen_subcommand_from $remotecommands" -a set-head -d 'Sets the default branch for a remote'
complete -f -c git -n "__fish_git_using_command remote" -n "not __fish_seen_subcommand_from $remotecommands" -a set-url -d 'Changes URLs for a remote'
complete -f -c git -n "__fish_git_using_command remote" -n "not __fish_seen_subcommand_from $remotecommands" -a get-url -d 'Retrieves URLs for a remote'
complete -f -c git -n "__fish_git_using_command remote" -n "not __fish_seen_subcommand_from $remotecommands" -a set-branches -d 'Changes the list of branches tracked by a remote'
complete -f -c git -n "__fish_git_using_command remote" -n "__fish_seen_subcommand_from add " -s f -d 'Once the remote information is set up git fetch <name> is run'
complete -f -c git -n "__fish_git_using_command remote" -n "__fish_seen_subcommand_from add " -l tags -d 'Import every tag from a remote with git fetch <name>'
complete -f -c git -n "__fish_git_using_command remote" -n "__fish_seen_subcommand_from add " -l no-tags -d "Don't import tags from a remote with git fetch <name>"
complete -f -c git -n '__fish_git_using_command remote' -n '__fish_seen_subcommand_from remove' -xa '(__fish_git_remotes)'
complete -f -c git -n "__fish_git_using_command remote" -n "__fish_seen_subcommand_from set-branches" -l add -d 'Add to the list of currently tracked branches instead of replacing it'
complete -f -c git -n "__fish_git_using_command remote" -n "__fish_seen_subcommand_from set-url" -l push -d 'Manipulate push URLs instead of fetch URLs'
complete -f -c git -n "__fish_git_using_command remote" -n "__fish_seen_subcommand_from set-url" -l add -d 'Add new URL instead of changing the existing URLs'
complete -f -c git -n "__fish_git_using_command remote" -n "__fish_seen_subcommand_from set-url" -l delete -d 'Remove URLs that match specified URL'
complete -f -c git -n "__fish_git_using_command remote" -n "__fish_seen_subcommand_from get-url" -l push -d 'Query push URLs rather than fetch URLs'
complete -f -c git -n "__fish_git_using_command remote" -n "__fish_seen_subcommand_from get-url" -l all -d 'All URLs for the remote will be listed'
complete -f -c git -n "__fish_git_using_command remote" -n "__fish_seen_subcommand_from show" -s n -d 'Remote heads are not queried, cached information is used instead'
complete -f -c git -n "__fish_git_using_command remote" -n "__fish_seen_subcommand_from prune" -l dry-run -d 'Report what will be pruned but do not actually prune it'
complete -f -c git -n "__fish_git_using_command remote" -n "__fish_seen_subcommand_from update" -l prune -d 'Prune all remotes that are updated'

### show
complete -f -c git -n __fish_git_needs_command -a show -d 'Show the last commit of a branch'
complete -f -c git -n '__fish_git_using_command show' -n 'not contains -- -- (commandline -xpc)' -ka '(__fish_git_branches)'
complete -f -c git -n '__fish_git_using_command show' -n 'not contains -- -- (commandline -xpc)' -ka '(__fish_git_tags)' -d Tag
complete -f -c git -n '__fish_git_using_command show' -n 'not contains -- -- (commandline -xpc)' -ka '(__fish_git_commits)'
complete -f -c git -n '__fish_git_using_command show' -n 'not contains -- -- (commandline -xpc)' -ka '(__fish_git_complete_stashes)'
complete -f -c git -n __fish_git_needs_rev_files -n 'not contains -- -- (commandline -xpc)' -xa '(__fish_git_complete_rev_files)'
complete -F -c git -n '__fish_git_using_command show' -n 'contains -- -- (commandline -xpc)'
complete -f -c git -n '__fish_git_using_command show' -l format -d 'Pretty-print the contents of the commit logs in a given format' -a '(__fish_git_show_opt format)'
complete -f -c git -n '__fish_git_using_command show' -l abbrev-commit -d 'Show only a partial hexadecimal commit object name'
complete -f -c git -n '__fish_git_using_command show' -l no-abbrev-commit -d 'Show the full 40-byte hexadecimal commit object name'
complete -f -c git -n '__fish_git_using_command show' -l oneline -d 'Shorthand for "--pretty=oneline --abbrev-commit"'
complete -f -c git -n '__fish_git_using_command show' -l encoding -d 'Re-code the commit log message in the encoding'
complete -f -c git -n '__fish_git_using_command show' -l expand-tabs -d 'Perform a tab expansion in the log message'
complete -f -c git -n '__fish_git_using_command show' -l no-expand-tabs -d 'Do not perform a tab expansion in the log message'
complete -f -c git -n '__fish_git_using_command show' -l notes -ka '(__fish_git_refs)' -d 'Show the notes that annotate the commit'
complete -f -c git -n '__fish_git_using_command show' -l no-notes -d 'Do not show notes'
complete -f -c git -n '__fish_git_using_command show' -s s -l no-patch -d 'Suppress diff output'
complete -f -c git -n '__fish_git_using_command show' -l show-signature -d 'Check the validity of a signed commit object'

### show-branch
complete -f -c git -n __fish_git_needs_command -a show-branch -d 'Show the commits on branches'
complete -f -c git -n '__fish_git_using_command show-branch' -ka '(__fish_git_refs)' -d Rev
complete -f -c git -n '__fish_git_using_command show-branch' -s r -l remotes -d "Shows the remote tracking branches"
complete -f -c git -n '__fish_git_using_command show-branch' -s a -l all -d "Show both remote-tracking branches and local branches"
complete -f -c git -n '__fish_git_using_command show-branch' -l current -d "Includes the current branch to the list of revs to be shown"
complete -f -c git -n '__fish_git_using_command show-branch' -l topo-order -d "Makes commits appear in topological order"
complete -f -c git -n '__fish_git_using_command show-branch' -l date-order -d "Makes commits appear in date order"
complete -f -c git -n '__fish_git_using_command show-branch' -l sparse -d "Shows merges only reachable from one tip"
complete -f -c git -n '__fish_git_using_command show-branch' -l no-name -d "Do not show naming strings for each commit"
complete -f -c git -n '__fish_git_using_command show-branch' -l sha1-name -d "Name commits with unique prefix"
complete -f -c git -n '__fish_git_using_command show-branch' -l no-color -d "Turn off colored output"
complete -f -c git -n '__fish_git_using_command show-branch' -l merge-base -d "Determine merge bases for the given commits"
complete -f -c git -n '__fish_git_using_command show-branch' -l independent -d "Show which refs can't be reached from any other"
complete -f -c git -n '__fish_git_using_command show-branch' -l topics -d "Show only commits that are not on the first given branch"
# TODO options

### add
complete -c git -n __fish_git_needs_command -a add -d 'Add file contents to the staging area'
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
complete -c git -n '__fish_git_using_command add' -l chmod -xa "-x\t'Track file as non-executable' +x\t'Track file as executable'"
complete -c git -n '__fish_git_using_command add' -l ignore-errors -d 'Ignore errors'
complete -c git -n '__fish_git_using_command add' -l ignore-missing -d 'Check if any of the given files would be ignored'
# Renames also show up as untracked + deleted, and to get git to show it as a rename _both_ need to be added.
# However, we can't do that as it is two tokens, so we don't need renamed here.
complete -f -c git -n '__fish_git_using_command add' -a '(__fish_git_files modified untracked deleted unmerged modified-staged-deleted)'
# TODO options

### am
complete -c git -n __fish_git_needs_command -a am -d 'Apply patches from a mailbox'
complete -f -c git -n '__fish_git_using_command am' -s s -l signoff -d 'Add a Signed-off-By trailer to commit message'
complete -f -c git -n '__fish_git_using_command am' -l keep-non-patch -d 'Only strip bracket pairs containing \'PATCH\''
complete -f -c git -n '__fish_git_using_command am' -l no-keep-cr -d 'Override am.keepcr to false'
complete -f -c git -n '__fish_git_using_command am' -s c -l scissors -d 'Remove everything in body before scissors'
complete -f -c git -n '__fish_git_using_command am' -l no-scissors -d 'Ignore scissor lines'
complete -x -c git -n '__fish_git_using_command am' -l quoted-cr -a 'nowarn warn strip' -d 'What to do when an email ends with CRLF'
complete -f -c git -n '__fish_git_using_command am' -l no-messageid -d 'Do not add message id to commit message'
complete -f -c git -n '__fish_git_using_command am' -s q -l quiet -d 'Supress logs'
complete -f -c git -n '__fish_git_using_command am' -l no-utf8 -d 'Disable all charset re-encoding of metadata'
complete -f -c git -n '__fish_git_using_command am' -s 3 -l 3way -d 'Fall back to three way merge on patch failure'
complete -f -c git -n '__fish_git_using_command am' -l no-3way -d 'Do not fall back to three way merge on patch failure'
complete -f -c git -n '__fish_git_using_command am' -l rerere-autoupdate -d 'Allow rerere to update index if possible'
complete -f -c git -n '__fish_git_using_command am' -l ignore-space-change -d 'Pass --ignore-space-change to git apply'
complete -F -c git -n '__fish_git_using_command am' -l directory -d 'Pass --directory to git apply'
complete -F -c git -n '__fish_git_using_command am' -l exclude -d 'Pass --exclude to git apply'
complete -F -c git -n '__fish_git_using_command am' -l include -d 'Pass --include to git apply'
complete -f -c git -n '__fish_git_using_command am' -l reject -d 'Pass --reject to git apply'
complete -x -f git -n '__fish_git_using_command am' -l patch-format -a 'mbox mboxrd stgit stgit-series hg' -d 'Specify the patch format'
complete -f -c git -n '__fish_git_using_command am' -s i -l interactive -d 'Run interactively'
complete -f -c git -n '__fish_git_using_command am' -l committer-date-is-author-date -d 'Treat committer date as author date'
complete -f -c git -n '__fish_git_using_command am' -l ignore-date -d 'Treat author date as committer date'
complete -f -c git -n '__fish_git_using_command am' -l skip -d 'Skip current patch'
complete -x -c git -n '__fish_git_using_command am' -s S -l gpg-sign -a '(type -q gpg && __fish_complete_gpg_key_id gpg)' -d 'Sign commits with gpg'
complete -f -c git -n '__fish_git_using_command am' -l no-gpg-sign -d 'Do not sign commits'
complete -f -c git -n '__fish_git_using_command am' -s r -l resolved -l continue -d 'Mark patch failures as resolved'
complete -x -c git -n '__fish_git_using_command am' -l resolvemsg -d 'Message to print after patch failure'
complete -f -c git -n '__fish_git_using_command am' -l abort -d 'Abort patch operation and restore branch'
complete -f -c git -n '__fish_git_using_command am' -l quit -d 'Abort without restoring branch'
complete -c git -n '__fish_git_using_command am' -l show-current-patch -a 'diff raw' -d 'Show message at which patch failures occured'

### checkout
complete -F -c git -n '__fish_git_using_command checkout' -n 'contains -- -- (commandline -xpc)'
complete -f -c git -n __fish_git_needs_command -a checkout -d 'Checkout and switch to a branch'

# The following dynamic, order-preserved (-k) completions will be shown in reverse order (see #9221)
complete -f -c git -n '__fish_git_using_command checkout' -n 'not contains -- -- (commandline -xpc)' -ka '(__fish_git_recent_commits --all)'
complete -f -c git -n '__fish_git_using_command checkout' -n 'not contains -- -- (commandline -xpc)' -ka '(__fish_git_tags)' -d Tag
complete -f -c git -n '__fish_git_using_command checkout' -n 'not contains -- -- (commandline -xpc)' -ka '(__fish_git_heads)' -d Head
complete -f -c git -n '__fish_git_using_command checkout' -n 'not contains -- -- (commandline -xpc)' -ka '(__fish_git_unique_remote_branches)' -d 'Unique Remote Branch'
complete -f -c git -n '__fish_git_using_command checkout' -n 'not contains -- -- (commandline -xpc)' -ka '(__fish_git_branches)'
# In the presence of changed files, `git checkout ...` assumes highest likelihood is intent to restore so this comes last (aka shown first).
complete -f -c git -n '__fish_git_using_command checkout' -ka '(__fish_git_files modified deleted modified-staged-deleted)'

complete -f -c git -n '__fish_git_using_command checkout' -s b -d 'Create a new branch'
complete -f -c git -n '__fish_git_using_command checkout' -s B -d 'Create a new branch or reset existing to start point'
complete -f -c git -n '__fish_git_using_command checkout' -s t -l track -d 'Track a new branch'
complete -f -c git -n '__fish_git_using_command checkout' -l theirs -d 'Keep staged changes'
complete -f -c git -n '__fish_git_using_command checkout' -l ours -d 'Keep unmerged changes'
complete -f -c git -n '__fish_git_using_command checkout' -l recurse-submodules -d 'Update the work trees of submodules'
complete -f -c git -n '__fish_git_using_command checkout' -l no-recurse-submodules -d 'Do not update the work trees of submodules'
complete -f -c git -n '__fish_git_using_command checkout' -l progress -d 'Report progress even if not connected to a terminal'
complete -f -c git -n '__fish_git_using_command checkout' -l no-progress -d "Don't report progress"
complete -f -c git -n '__fish_git_using_command checkout' -s f -l force -d 'Switch even if working tree differs or unmerged files exist'
# TODO options

### sparse-checkout
# `init` subcommand is deprecated
set -l sparse_co_commands list set add reapply disable check-rules
complete -f -c git -n __fish_git_needs_command -a sparse-checkout -d 'Reduce your working tree to a subset of tracked files'
complete -f -c git -n "__fish_git_using_command sparse-checkout" -n "not __fish_seen_subcommand_from $sparse_co_commands" -a set -d 'Enable the necessary sparse-checkout config settings'
complete -f -c git -n "__fish_git_using_command sparse-checkout" -n "not __fish_seen_subcommand_from $sparse_co_commands" -a list -d 'Describe the directories or patterns in the sparse-checkout file'
complete -x -c git -n "__fish_git_using_command sparse-checkout" -n "not __fish_seen_subcommand_from $sparse_co_commands" -a add -d 'Add directories (in cone mode) or patterns (in non-cone mode)'
complete -f -c git -n "__fish_git_using_command sparse-checkout" -n "not __fish_seen_subcommand_from $sparse_co_commands" -a reapply -d 'Reapply the sparsity pattern rules in case of merge-conflict'
complete -f -c git -n "__fish_git_using_command sparse-checkout" -n "not __fish_seen_subcommand_from $sparse_co_commands" -a disable -d 'Disable sparse-checkout, and restore the working directory to include all files'
complete -f -c git -n "__fish_git_using_command sparse-checkout" -n "not __fish_seen_subcommand_from $sparse_co_commands" -a check-rules -d 'Check whether sparsity rules match one or more paths'

complete -f -c git -n "__fish_git_using_command sparse-checkout" -n "__fish_seen_subcommand_from add set" -l stdin -d 'read patterns from stdin as a newline-delimited list'
complete -f -c git -n "__fish_git_using_command sparse-checkout" -n "__fish_seen_subcommand_from reapply set" -l no-cone -d 'input list is considered a list of patterns instead of directory paths'
complete -f -c git -n "__fish_git_using_command sparse-checkout" -n "__fish_seen_subcommand_from reapply set" -l sparse-index -d 'use a sparse index to be more closely aligned with your sparse-checkout definition'
complete -f -c git -n "__fish_git_using_command sparse-checkout" -n "__fish_seen_subcommand_from reapply set" -l no-sparse-index -d 'not --sparse-index'

### apply
complete -c git -n __fish_git_needs_command -a apply -d 'Apply a patch'
complete -f -c git -n '__fish_git_using_command apply' -l numstat -d 'Show number of additions and deletions'
complete -f -c git -n '__fish_git_using_command apply' -l summary -d 'Output a condensed summary'
complete -f -c git -n '__fish_git_using_command apply' -l check -d 'Just check if the patches can be applied'
complete -f -c git -n '__fish_git_using_command apply' -l index -d 'Apply patch to index and working tree'
complete -f -c git -n '__fish_git_using_command apply' -l cached -d 'Apply patch to index'
complete -f -c git -n '__fish_git_using_command apply' -l intent-to-add -d 'Add entry for file in index with no content'
complete -f -c git -n '__fish_git_using_command apply' -s 3 -l 3way -d 'Attempt a 3 way merge on conflicts'
complete -F -c git -n '__fish_git_using_command apply' -l build-fake-ancestor -d 'Build a temporary index containing these blobs'
complete -f -c git -n '__fish_git_using_command apply' -s R -l reverse -d 'Apply the patch in reverse'
complete -f -c git -n '__fish_git_using_command apply' -l reject -d 'Leave rejected hunks in *.rej files'
complete -f -c git -n '__fish_git_using_command apply' -n '__fish_git_contains_opt numstat' -s z -d 'Do not munge pathnames'
complete -x -c git -n '__fish_git_using_command apply am' -s p -d 'Remove n leading path components'
complete -x -c git -n '__fish_git_using_command apply am' -s C -d 'Ensure n that lines of surrounding context match'
complete -f -c git -n '__fish_git_using_command apply' -l unidiff-zero -d 'Do not break on diffs generated using --unified=0'
complete -f -c git -n '__fish_git_using_command apply' -l apply -d 'Always apply patches'
complete -f -c git -n '__fish_git_using_command apply' -l no-add -d 'Ignore additions made by patches'
complete -f -c git -n '__fish_git_using_command apply' -l allow-binary-replacement -l binary -d 'Also patch binaries'
complete -F -c git -n '__fish_git_using_command apply' -l exclude -d 'Dont apply changes to files matching given pattern'
complete -F -c git -n '__fish_git_using_command apply' -l include -d 'Apply changes to files matching given pattern'
complete -f -c git -n '__fish_git_using_command apply am' -l ignore-space-change -l ignore-whitespace -d 'Ignore whitespace change in context lines'
complete -x -c git -n '__fish_git_using_command apply am' -l whitespace -a 'nowarn warn fix error error-all' -d 'Action to take when there are whitespace errors'
complete -f -c git -n '__fish_git_using_command apply' -l inaccurate-eof -d 'Work around some diff versions not detecting newlines at end of file'
complete -f -c git -n '__fish_git_using_command apply' -s v -l verbose -d 'Report progress to stderr'
complete -f -c git -n '__fish_git_using_command apply' -l recount -d 'Do not trust the line counts in the hunk headers'
complete -F -c git -n '__fish_git_using_command apply' -l directory -d 'Prepend given path to all filenames'
complete -f -c git -n '__fish_git_using_command apply' -l unsafe-paths -d 'Allow patches that work outside working area'

### archive
complete -f -c git -n __fish_git_needs_command -a archive -d 'Create an archive of files from a tree'
complete -f -c git -n '__fish_git_using_command archive' -s l -l list -d "Show all available formats"
complete -f -c git -n '__fish_git_using_command archive' -s v -l verbose -d "Be verbose"
complete -f -c git -n '__fish_git_using_command archive' -l worktree-attributes -d "Look for attributes in .gitattributes files in the working tree as well"
# TODO options

### bisect
complete -f -c git -n __fish_git_needs_command -a bisect -d 'Use binary search to find what introduced a bug'
complete -f -c git -n '__fish_git_using_command bisect' -n '__fish_prev_arg_in bisect' -xa "
start\t'Start a new bisect session'
bad\t'Mark a commit as bad'
new\t'Mark a commit as new'
good\t'Mark a commit as good'
old\t'Mark a commit as old'
terms\t'Show terms used for new/old states'
skip\t'Skip some commits'
reset\t'Exit a bisect session and reset HEAD'
visualize\t'See remaining commits in gitk'
replay\t'Replay a bisect log file'
log\t'Record a bisect log file'
run\t'Bisect automatically with the given command as discriminator'
help\t'Print a synopsis of all commands'
"
complete -c git -n '__fish_git_using_command bisect' -n '__fish_seen_argument --' -F
complete -f -c git -n '__fish_git_using_command bisect' -n '__fish_seen_subcommand_from start' -l term-new -l term-bad -x -d 'Use another term instead of new/bad'
complete -f -c git -n '__fish_git_using_command bisect' -n '__fish_seen_subcommand_from start' -l term-old -l term-good -x -d 'Use another term instead of old/good'
complete -f -c git -n '__fish_git_using_command bisect' -n '__fish_seen_subcommand_from start' -l no-checkout -d 'Do not checkout tree, only update BISECT_HEAD'
complete -f -c git -n '__fish_git_using_command bisect' -n '__fish_seen_subcommand_from start' -l first-parent -d 'On merge commits, follow only the first parent commit'
complete -f -c git -n '__fish_git_using_command bisect' -n '__fish_seen_subcommand_from start' -n 'not contains -- -- (commandline -xpc)' -a '(__fish_git_refs)'
complete -f -c git -n '__fish_git_using_command bisect' -n '__fish_seen_subcommand_from bad new good old' -a '(__fish_git_refs)'
complete -f -c git -n '__fish_git_using_command bisect' -n '__fish_seen_subcommand_from terms' -l --term-good -d 'Print the term for the old state'
complete -f -c git -n '__fish_git_using_command bisect' -n '__fish_seen_subcommand_from terms' -l --term-bad -d 'Print the term for the new state'
complete -f -c git -n '__fish_git_using_command bisect' -n '__fish_seen_subcommand_from skip' -a '(__fish_git_ranges)'
complete -f -c git -n '__fish_git_using_command bisect' -n '__fish_seen_subcommand_from reset' -a '(__fish_git_refs)'
complete -c git -n '__fish_git_using_command bisect' -n '__fish_seen_subcommand_from replay' -F
complete -f -c git -n '__fish_git_using_command bisect' -n '__fish_seen_subcommand_from run' -xa '(__fish_complete_subcommand --fcs-skip=3)'

### branch
complete -f -c git -n __fish_git_needs_command -a branch -d 'List, create, or delete branches'
complete -f -c git -n '__fish_git_using_command branch' -ka '(__fish_git_branches)'
complete -f -c git -n '__fish_git_using_command branch' -s d -l delete -d 'Delete branch' -xa '(__fish_git_local_branches)'
complete -f -c git -n '__fish_git_using_command branch' -s D -d 'Force deletion of branch' -xa '(__fish_git_local_branches)'
complete -f -c git -n '__fish_git_using_command branch' -s f -l force -d 'Reset branch even if it already exists'
complete -f -c git -n '__fish_git_using_command branch' -s m -l move -d 'Rename branch'
complete -f -c git -n '__fish_git_using_command branch' -s M -d 'Force rename branch'
complete -f -c git -n '__fish_git_using_command branch' -s c -l copy -d 'Copy branch'
complete -f -c git -n '__fish_git_using_command branch' -s C -d 'Force copy branch'
complete -f -c git -n '__fish_git_using_command branch' -s a -l all -d 'Lists both local and remote branches'
complete -f -c git -n '__fish_git_using_command branch' -s r -l remotes -d 'List or delete (if used with -d) the remote-tracking branches.'
complete -f -c git -n '__fish_git_using_command branch' -s t -l track -l track -d 'Track remote branch'
complete -f -c git -n '__fish_git_using_command branch' -l no-track -d 'Do not track remote branch'
complete -f -c git -n '__fish_git_using_command branch' -l set-upstream-to -d 'Set remote branch to track' -ka '(__fish_git_branches)'
complete -f -c git -n '__fish_git_using_command branch' -l merged -d 'List branches that have been merged'
complete -f -c git -n '__fish_git_using_command branch' -l no-merged -d 'List branches that have not been merged'
complete -f -c git -n '__fish_git_using_command branch' -l unset-upstream -d 'Remove branch upstream information'
complete -f -c git -n '__fish_git_using_command branch' -l edit-description -d 'Edit branch description in editor'
complete -f -c git -n '__fish_git_using_command branch' -l contains -d 'List branches that contain the specified commit' -xa '(__fish_git_commits)'
complete -f -c git -n '__fish_git_using_command branch' -l no-contains -d 'List branches that don\'t contain the specified commit' -xa '(__fish_git_commits)'

### bundle
set -l bundlecommands create verify list-heads unbundle
complete -f -c git -n __fish_git_needs_command -a bundle -d 'Create, unpack, and manipulate "bundle" files'
complete -f -c git -n "__fish_git_using_command bundle" -n "not __fish_seen_subcommand_from $bundlecommands" -a "create\t'Create a bundle'
verify\t'Check that the bundle is valid and will apply cleanly'
list-heads\t'List the references defined in the bundle'
unbundle\t'Build a pack index file and print all defined references'"
complete -f -c git -n "__fish_git_using_command bundle" -n "__fish_seen_subcommand_from create verify" -s q -l quiet -d 'Do not show progress meter'
complete -f -c git -n "__fish_git_using_command bundle" -n "__fish_seen_subcommand_from create unbundle" -l progress -d 'Show progress meter'
complete -f -c git -n "__fish_git_using_command bundle" -n "__fish_seen_subcommand_from create" -l all-progress -d 'Show progress meter during object writing phase'
complete -f -c git -n "__fish_git_using_command bundle" -n "__fish_seen_subcommand_from create" -l all-progress-implied -d 'Similar to --all-progress when progress meter is shown'
complete -x -c git -n "__fish_git_using_command bundle" -n "__fish_seen_subcommand_from create" -l version -d 'Specify bundle format version'
# FIXME: <file> should be suggested first and <git-rev-list args> second (also, '--all' is only valid in rev-list)
complete -c git -n '__fish_git_using_command bundle' -n "__fish_seen_subcommand_from create" -ka '--all\t"All refs"'
complete -c git -n '__fish_git_using_command bundle' -n "__fish_seen_subcommand_from create" -ka '(__fish_git_ranges)'

### cherry
complete -f -c git -n __fish_git_needs_command -a cherry -d 'Find commits yet to be applied to upstream'
complete -f -c git -n '__fish_git_using_command cherry' -s v -d 'Show the commit subjects next to the SHA1s'
complete -f -c git -n '__fish_git_using_command cherry' -ka '(__fish_git_refs)' -d Upstream

### cherry-pick
complete -f -c git -n __fish_git_needs_command -a cherry-pick -d 'Reapply a commit on another branch'
complete -f -c git -n '__fish_git_using_command cherry-pick' -ka '(__fish_git_ranges)'
# TODO: Filter further
complete -f -c git -n '__fish_git_using_command cherry-pick' -n __fish_git_possible_commithash -ka '(__fish_git_commits)'
complete -f -c git -n '__fish_git_using_command cherry-pick' -s e -l edit -d 'Edit the commit message prior to committing'
complete -f -c git -n '__fish_git_using_command cherry-pick' -s x -d 'Append info in generated commit on the origin of the cherry-picked change'
complete -f -c git -n '__fish_git_using_command cherry-pick' -s n -l no-commit -d 'Apply changes without making any commit'
complete -f -c git -n '__fish_git_using_command cherry-pick' -s s -l signoff -d 'Add Signed-off-by line to the commit message'
complete -f -c git -n '__fish_git_using_command cherry-pick' -l ff -d 'Fast-forward if possible'
complete -f -c git -n '__fish_git_using_command cherry-pick' -l continue -d 'Continue the operation in progress'
complete -f -c git -n '__fish_git_using_command cherry-pick' -l abort -d 'Cancel the operation'
complete -f -c git -n '__fish_git_using_command cherry-pick' -l skip -d 'Skip the current commit and continue with the rest of the sequence'

### clone
complete -f -c git -n __fish_git_needs_command -a clone -d 'Clone a repository into a new directory'
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
complete -f -c git -n '__fish_git_using_command clone' -l filter -ra '(__fish_git_filters)' -d 'Partial clone by requesting a subset of objects from server'

### commit
complete -c git -n __fish_git_needs_command -a commit -d 'Record changes to the repository'
complete -c git -n '__fish_git_using_command commit' -l amend -d 'Amend the log message of the last commit'
complete -f -c git -n '__fish_git_using_command commit && not string match -rq -- "^(--fixup\b|--squash\b|-C|-c)" (commandline -xpc)[-1]' -a '(__fish_git_files modified deleted modified-staged-deleted untracked)'
complete -c git -n '__fish_git_using_command commit' -s a -l all -d 'Automatically stage modified and deleted files'
complete -c git -n '__fish_git_using_command commit' -s p -l patch -d 'Use interactive patch selection interface'
complete -f -c git -n '__fish_git_using_command commit' -l fixup -d 'Fixup commit to be used with rebase --autosquash'
complete -f -c git -n '__fish_git_using_command commit' -l squash -d 'Squash commit to be used with rebase --autosquash'
complete -c git -n '__fish_git_using_command commit' -l reset-author -d 'When amending, reset author of commit to the committer'
complete -x -c git -n '__fish_git_using_command commit' -l author -d 'Override the commit author'
complete -x -c git -n '__fish_git_using_command commit' -l cleanup -a "strip\t'Leading/trailing whitespace/empty lines, #commentary'
 whitespace\t'Like strip but keep #commentary'
 verbatim\t'Do not change the message'
 scissors\t'Like whitespace but also remove after scissor lines'
 default\t'Like strip if the message is to be edited, whitespace otherwise'" -d 'How to clean up the commit message'
complete -x -c git -n '__fish_git_using_command commit' -l date -d 'Override the author date'
complete -x -c git -n '__fish_git_using_command commit' -s m -l message -d 'Use the given message as the commit message'
complete -f -c git -n '__fish_git_using_command commit' -l no-edit -d 'Use the selected commit message without launching an editor'
complete -f -c git -n '__fish_git_using_command commit' -l no-gpg-sign -d 'Do not sign commit'
complete -f -c git -n '__fish_git_using_command commit' -s n -l no-verify -d 'Do not run pre-commit and commit-msg hooks'
complete -f -c git -n '__fish_git_using_command commit' -n 'string match -rq -- "^(--fixup\b|--squash\b|-C|-c|--reuse-message\b|--reedit-message\b)" (commandline -xpc)[-1]' -ka '(__fish_git_recent_commits)'
complete -f -c git -n '__fish_git_using_command commit' -l allow-empty -d 'Create a commit with no changes'
complete -f -c git -n '__fish_git_using_command commit' -l allow-empty-message -d 'Create a commit with no commit message'
complete -f -c git -n '__fish_git_using_command commit' -s s -l signoff -d 'Append Signed-off-by trailer to commit message'
complete -f -c git -n '__fish_git_using_command commit' -l no-signoff -d 'Do not append Signed-off-by trailer to commit message'
complete -f -c git -n '__fish_git_using_command commit' -s C -l reuse-message -d 'Reuse log message and authorship of an existing commit'
complete -f -c git -n '__fish_git_using_command commit' -s c -l reedit-message -d 'Like --reuse-message, but allow editing commit message'
complete -f -c git -n '__fish_git_using_command commit' -s e -l edit -d 'Further edit the message taken from -m, -C, or -F'
# TODO options

### count-objects
complete -f -c git -n __fish_git_needs_command -a count-objects -d 'Count number of objects and disk consumption'
complete -f -c git -n '__fish_git_using_command count-objects' -s v -l verbose -d 'Be verbose'
complete -f -c git -n '__fish_git_using_command count-objects' -s H -l human-readable -d 'Print in human readable format'

### daemon
complete -c git -n __fish_git_needs_command -a daemon -d 'A simple server for git repositories'
complete -f -c git -n '__fish_git_using_command daemon' -l strict-paths -d 'Match paths exactly'
complete -F -c git -n '__fish_git_using_command daemon' -l base-path -d 'Git Root'
complete -f -c git -n '__fish_git_using_command daemon' -l base-path-relaxed -d 'When looking up with base path fails, try without it'
complete -F -c git -n '__fish_git_using_command daemon' -l interpolated-path -d 'Construct a path from the given template'
complete -f -c git -n '__fish_git_using_command daemon' -l export-all -d 'Allow pulling from all directories'
complete -f -c git -n '__fish_git_using_command daemon' -l inetd -d 'Run as inetd service'
complete -x -c git -n '__fish_git_using_command daemon' -l listen -d 'Listen on this IP'
complete -x -c git -n '__fish_git_using_command daemon' -l port -d 'Listen on this port'
complete -x -c git -n '__fish_git_using_command daemon' -l init-timeout -d 'Connection timeout'
complete -x -c git -n '__fish_git_using_command daemon' -l timeout -d 'Timeout for each request'
complete -x -c git -n '__fish_git_using_command daemon' -l max-connections -d 'Maximum parallel clients'
complete -f -c git -n '__fish_git_using_command daemon' -l syslog -d '--log-destination=syslog'
complete -x -c git -n '__fish_git_using_command daemon' -l log-destination -a 'stderr syslog none' -d 'Log destination'
complete -x -c git -n '__fish_git_using_command daemon' -l user-path -d 'Allow ~user notation to be used'
complete -f -c git -n '__fish_git_using_command daemon' -l verbose -d 'Log all details'
complete -f -c git -n '__fish_git_using_command daemon' -l reuseaddr -d 'Reuse address when binding to listening server'
complete -f -c git -n '__fish_git_using_command daemon' -l detach -d 'Detach from shell'
complete -x -c git -n '__fish_git_using_command daemon' -l reuseaddr -d 'Save the process id in file'
complete -x -c git -n '__fish_git_using_command daemon' -l user -d 'Change daemon\'s uid'
complete -x -c git -n '__fish_git_using_command daemon' -l group -d 'Change daemon\'s gid'
complete -x -c git -n '__fish_git_using_command daemon' -l enable -a 'upload-pack upload-archive receive-pack' -d 'Enable service'
complete -x -c git -n '__fish_git_using_command daemon' -l disable -a 'upload-pack upload-archive receive-pack' -d 'Disable service'
complete -x -c git -n '__fish_git_using_command daemon' -l allow-override -a 'upload-pack upload-archive receive-pack' -d 'Allow overriding site-wide default per repository configuration'
complete -x -c git -n '__fish_git_using_command daemon' -l forbid-override -a 'upload-pack upload-archive receive-pack' -d 'Forbid overriding site-wide default per repository configuration'
complete -f -c git -n '__fish_git_using_command daemon' -l informative-errors -d 'Report more verbose errors to clients'
complete -f -c git -n '__fish_git_using_command daemon' -l no-informative-errors -d 'Report less verbose errors to clients'
complete -x -c git -n '__fish_git_using_command daemon' -l access-hook -d 'Hook to run whenever a client connects'

### describe
complete -c git -n __fish_git_needs_command -a describe -d 'Give an object a human readable name'
complete -f -c git -n '__fish_git_using_command describe' -ka '(__fish_git_tags)' -d Tag
complete -f -c git -n '__fish_git_using_command describe' -ka '(__fish_git_branches)'
complete -f -c git -n '__fish_git_using_command describe' -ka '(__fish_git_heads)' -d Head
complete -f -c git -n '__fish_git_using_command describe' -l dirty -d 'Describe the state of the working tree, append dirty if there are local changes'
complete -f -c git -n '__fish_git_using_command describe' -l broken -d 'Describe the state of the working tree, append -broken instead of erroring'
complete -f -c git -n '__fish_git_using_command describe' -l all -d 'Use all tags, not just annotated'
complete -f -c git -n '__fish_git_using_command describe' -l tags -d 'Use all commits/tags, not just annotated tags'
complete -f -c git -n '__fish_git_using_command describe' -l contains -d 'Find the tag that comes after the commit'
complete -f -c git -n '__fish_git_using_command describe' -l abbrev -d 'Use <n> digits, or as many digits as needed to form a unique object name'
complete -f -c git -n '__fish_git_using_command describe' -l candidates -d 'Consider up to <n> candidates'
complete -f -c git -n '__fish_git_using_command describe' -l exact-match -d 'Only output exact matches'
complete -f -c git -n '__fish_git_using_command describe' -l debug -d 'Display debug info'
complete -f -c git -n '__fish_git_using_command describe' -l long -d 'Always output the long format'
complete -f -c git -n '__fish_git_using_command describe' -l match -d 'Only consider tags matching the given glob pattern'
complete -f -c git -n '__fish_git_using_command describe' -l exclude -d 'Do not consider tags matching the given glob pattern'
complete -f -c git -n '__fish_git_using_command describe' -l always -d 'Show uniquely abbreviated commit object as fallback'
complete -f -c git -n '__fish_git_using_command describe' -l first-parent -d 'Follow only the first parent of a merge commit'

### diff
complete -c git -n __fish_git_needs_command -a diff -d 'Show changes between commits and working tree'
complete -c git -n '__fish_git_using_command diff' -n 'not contains -- -- (commandline -xpc)' -ka '(__fish_git_ranges)'
complete -c git -n '__fish_git_using_command diff' -n 'not contains -- -- (commandline -xpc)' -ka '(__fish_git_complete_stashes)'
complete -c git -n '__fish_git_using_command diff' -n 'not contains -- -- (commandline -xpc)' -ka '(__fish_git_recent_commits --all)'
complete -c git -n '__fish_git_using_command diff' -l cached -d 'Show diff of changes in the index'
complete -c git -n '__fish_git_using_command diff' -l staged -d 'Show diff of changes in the index'
complete -c git -n '__fish_git_using_command diff' -l no-index -d 'Compare two paths on the filesystem'
complete -c git -n '__fish_git_using_command diff' -l exit-code -d 'Exit with 1 if there were differences or 0 if no differences'
complete -c git -n '__fish_git_using_command diff' -l quiet -d 'Disable all output of the program, implies --exit-code'
complete -c git -n '__fish_git_using_command diff' -s 1 -l base -d 'Compare the working tree with the "base" version'
complete -c git -n '__fish_git_using_command diff' -s 2 -l ours -d 'Compare the working tree with the "our branch"'
complete -c git -n '__fish_git_using_command diff' -s 3 -l theirs -d 'Compare the working tree with the "their branch"'
complete -c git -n '__fish_git_using_command diff' -s 0 -d 'Omit diff output for unmerged entries and just show "Unmerged"'
complete -c git -n '__fish_git_using_command diff' -k -n 'not __fish_git_contains_opt cached staged' -a '(
    set -l kinds modified
    contains -- -- (commandline -xpc) && set -a kinds deleted modified-staged-deleted
    __fish_git_files $kinds
)'
complete -c git -n '__fish_git_using_command diff' -n '__fish_git_contains_opt cached staged' -fa '(__fish_git_files all-staged)'

### Function to list available tools for git difftool and mergetool

function __fish_git_diffmerge_tools -a cmd
    git $cmd --tool-help | while read -l line
        string match -q 'The following tools are valid, but not currently available:' -- $line
        and break
        string replace -f -r '^\t\t(\w+).*$' '$1' -- $line
    end
end

### difftool
complete -c git -n __fish_git_needs_command -a difftool -d 'Open diffs in a visual tool'
complete -c git -n '__fish_git_using_command difftool' -n 'not contains -- -- (commandline -xpc)' -ka '(__fish_git_ranges)'
complete -c git -n '__fish_git_using_command difftool' -l cached -d 'Visually show diff of changes in the index'
complete -f -c git -n '__fish_git_using_command difftool' -a '(
    set -l kinds modified
    contains -- -- (commandline -xpc) && set -a kinds deleted modified-staged-deleted
    __fish_git_files $kinds
)'
complete -f -c git -n '__fish_git_using_command difftool' -s g -l gui -d 'Use `diff.guitool` instead of `diff.tool`'
complete -f -c git -n '__fish_git_using_command difftool' -s d -l dir-diff -d 'Perform a full-directory diff'
complete -c git -n '__fish_git_using_command difftool' -l prompt -d 'Prompt before each invocation of the diff tool'
complete -f -c git -n '__fish_git_using_command difftool' -s y -l no-prompt -d 'Do not prompt before launching a diff tool'
complete -f -c git -n '__fish_git_using_command difftool' -l symlinks -d 'Use symlinks in dir-diff mode'
complete -f -c git -n '__fish_git_using_command difftool' -s t -l tool -d 'Use the specified diff tool' -a "(__fish_git_diffmerge_tools difftool)"
complete -f -c git -n '__fish_git_using_command difftool' -l tool-help -d 'Print a list of diff tools that may be used with `--tool`'
complete -f -c git -n '__fish_git_using_command difftool' -l trust-exit-code -d 'Exit when an invoked diff tool returns a non-zero exit code'
complete -f -c git -n '__fish_git_using_command difftool' -s x -l extcmd -d 'Specify a custom command for viewing diffs'
complete -f -c git -n '__fish_git_using_command difftool' -l no-gui -d 'Overrides --gui setting'
# TODO options

### gc
complete -f -c git -n __fish_git_needs_command -a gc -d 'Collect garbage (unreachable commits etc)'
complete -f -c git -n '__fish_git_using_command gc' -l aggressive -d 'Aggressively optimize the repository'
complete -f -c git -n '__fish_git_using_command gc' -l auto -d 'Checks any housekeeping is required and then run'
complete -f -c git -n '__fish_git_using_command gc' -l prune -d 'Prune loose objects older than date'
complete -f -c git -n '__fish_git_using_command gc' -l no-prune -d 'Do not prune any loose objects'
complete -f -c git -n '__fish_git_using_command gc' -l quiet -d 'Be quiet'
complete -f -c git -n '__fish_git_using_command gc' -l force -d 'Force `git gc` to run'
complete -f -c git -n '__fish_git_using_command gc' -l keep-largest-pack -d 'Ignore `gc.bigPackThreshold`'

### grep
complete -c git -n __fish_git_needs_command -a grep -d 'Print lines matching a pattern'
complete -f -c git -n '__fish_git_using_command grep' -l cached -d 'Search blobs registered in the index file'
complete -f -c git -n '__fish_git_using_command grep' -l no-index -d 'Search files in the current directory not managed by Git'
complete -f -c git -n '__fish_git_using_command grep' -l untracked -d 'Search also in untracked files'
complete -f -c git -n '__fish_git_using_command grep' -l no-exclude-standard -d 'Also search in ignored files by not honoring the .gitignore mechanism'
complete -f -c git -n '__fish_git_using_command grep' -l exclude-standard -d 'Do not search ignored files specified via the .gitignore mechanism'
complete -f -c git -n '__fish_git_using_command grep' -l recurse-submodules -d 'Recursively search in each submodule that is active and checked out in the repository'
complete -f -c git -n '__fish_git_using_command grep' -s a -l text -d 'Process binary files as if they were text'
complete -f -c git -n '__fish_git_using_command grep' -l textconv -d 'Honor textconv filter settings'
complete -f -c git -n '__fish_git_using_command grep' -l no-textconv -d 'Do not honor textconv filter settings'
complete -f -c git -n '__fish_git_using_command grep' -s i -l ignore-case -d 'Ignore case differences between the patterns and the files'
complete -f -c git -n '__fish_git_using_command grep' -s I -d 'Don\'t match the pattern in binary files'
complete -f -c git -n '__fish_git_using_command grep' -s r -l recursive -d 'Descend into levels of directories endlessly'
complete -f -c git -n '__fish_git_using_command grep' -l no-recursive -d 'Do not descend into directories'
complete -f -c git -n '__fish_git_using_command grep' -s w -l word-regexp -d 'Match the pattern only at word boundary'
complete -f -c git -n '__fish_git_using_command grep' -s v -l invert-match -d 'Select non-matching lines'
complete -f -c git -n '__fish_git_using_command grep' -l full-name -d 'Forces paths to be output relative to the project top directory'
complete -f -c git -n '__fish_git_using_command grep' -s E -l extended-regexp -d 'Use POSIX extended regexp for patterns'
complete -f -c git -n '__fish_git_using_command grep' -s G -l basic-regexp -d 'Use POSIX basic regexp for patterns'
complete -f -c git -n '__fish_git_using_command grep' -s P -l perl-regexp -d 'Use Perl-compatible regular expressions for patterns'
complete -f -c git -n '__fish_git_using_command grep' -s F -l fixed-strings -d 'Don\'t interpret pattern as a regex'
complete -f -c git -n '__fish_git_using_command grep' -s n -l line-number -d 'Prefix the line number to matching lines'
complete -f -c git -n '__fish_git_using_command grep' -l column -d 'Prefix the 1-indexed byte-offset of the first match from the start of the matching line'
complete -f -c git -n '__fish_git_using_command grep' -s l -l files-with-matches -d 'Show only the names of files that contain matches'
complete -f -c git -n '__fish_git_using_command grep' -s L -l files-without-match -d 'Show only the names of files that do not contain matches'
complete -f -c git -n '__fish_git_using_command grep' -s z -l null -d 'Use \\0 as the delimiter for pathnames in the output, and print them verbatim'
complete -f -c git -n '__fish_git_using_command grep' -s o -l only-matching -d 'Print only the matched parts of a matching line'
complete -f -c git -n '__fish_git_using_command grep' -s c -l count -d 'Instead of showing every matched line, show the number of lines that match'
complete -f -c git -n '__fish_git_using_command grep' -l no-color -d 'Turn off match highlighting'
complete -f -c git -n '__fish_git_using_command grep' -l break -d 'Print an empty line between matches from different files'
complete -f -c git -n '__fish_git_using_command grep' -l heading -d 'Show the filename once above the matches in that file'
complete -f -c git -n '__fish_git_using_command grep' -s p -l show-function -d 'Show the function name for the match'
complete -f -c git -n '__fish_git_using_command grep' -s W -l function-context -d 'Show the surrounding function'
complete -f -c git -n '__fish_git_using_command grep' -s e -d 'The next parameter is the pattern'
complete -f -c git -n '__fish_git_using_command grep' -l and -d 'Combine patterns using and'
complete -f -c git -n '__fish_git_using_command grep' -l or -d 'Combine patterns using or'
complete -f -c git -n '__fish_git_using_command grep' -l not -d 'Combine patterns using not'
complete -f -c git -n '__fish_git_using_command grep' -l all-match -d 'Only match files that can match all the pattern expressions when giving multiple'
complete -f -c git -n '__fish_git_using_command grep' -s q -l quiet -d 'Just exit with status 0 when there is a match and with non-zero status when there isn\'t'
complete -c git -n '__fish_git_using_command grep' -n 'not contains -- -- (commandline -xpc)' -ka '(__fish_git_refs)'
# TODO options, including max-depth, h, open-files-in-pager, contexts, threads, file

### init
complete -f -c git -n __fish_git_needs_command -a init -d 'Create an empty git repository'
complete -f -c git -n '__fish_git_using_command init' -s q -l quiet -d 'Only print error and warning messages'
complete -f -c git -n '__fish_git_using_command init' -l bare -d 'Create a bare repository'
# TODO options

### log
complete -c git -n __fish_git_needs_command -a shortlog -d 'Show commit shortlog'
complete -c git -n __fish_git_needs_command -a log -d 'Show commit logs'
complete -c git -n '__fish_git_using_command log' -a '(__fish_git ls-files)'
complete -c git -n '__fish_git_using_command log' -n 'not contains -- -- (commandline -xpc)' -ka '(__fish_git_ranges)'
complete -c git -n '__fish_git_using_command log' -l follow -d 'Continue listing file history beyond renames'
complete -c git -n '__fish_git_using_command log' -l no-decorate -d 'Don\'t print ref names'
complete -f -c git -n '__fish_git_using_command log' -l decorate -a 'short\tHide\ prefixes full\tShow\ full\ ref\ names auto\tHide\ prefixes\ if\ printed\ to\ terminal no\tDon\\\'t\ display\ ref' -d 'Print out ref names'
complete -c git -n '__fish_git_using_command log' -l source -d 'Print ref name by which each commit was reached'
complete -c git -n '__fish_git_using_command log' -l use-mailmap
complete -c git -n '__fish_git_using_command log' -l full-diff
complete -c git -n '__fish_git_using_command log' -l log-size
complete -r -F -c git -n '__fish_git_using_command log' -s L -d 'Trace the evolution of the line range given by <start>,<end>, or regex <funcname>, within the <file>'
complete -x -c git -n '__fish_git_using_command log rev-list' -s n -l max-count -d 'Limit the number of commits before starting to show the commit output'
complete -x -c git -n '__fish_git_using_command log rev-list' -l skip -d 'Skip given number of commits'
complete -x -c git -n '__fish_git_using_command log rev-list' -l since -d 'Show commits more recent than specified date'
complete -x -c git -n '__fish_git_using_command log rev-list' -l after -d 'Show commits more recent than specified date'
complete -x -c git -n '__fish_git_using_command log rev-list' -l until -d 'Show commits older than specified date'
complete -x -c git -n '__fish_git_using_command log rev-list' -l before -d 'Show commits older than specified date'
complete -x -c git -n '__fish_git_using_command log rev-list' -l author -d 'Limit commits from given author'
complete -x -c git -n '__fish_git_using_command log rev-list' -l committer -d 'Limit commits from given committer'
complete -x -c git -n '__fish_git_using_command log rev-list' -l grep-reflog -d 'Limit commits to ones with reflog entries matching given pattern'
complete -x -c git -n '__fish_git_using_command log rev-list' -l grep -d 'Limit commits with message that match given pattern'
complete -c git -n '__fish_git_using_command log rev-list' -l all-match -d 'Limit commits to ones that match all given --grep'
complete -c git -n '__fish_git_using_command log rev-list' -l invert-grep -d 'Limit commits to ones with message that don\'t match --grep'
complete -c git -n '__fish_git_using_command log rev-list' -l regexp-ignore-case -s i -d 'Case insensitive match'
complete -c git -n '__fish_git_using_command log rev-list' -l basic-regexp -d 'Patterns are basic regular expressions (default)'
complete -c git -n '__fish_git_using_command log rev-list' -l extended-regexp -s E -d 'Patterns are extended regular expressions'
complete -c git -n '__fish_git_using_command log rev-list' -l fixed-strings -s F -d 'Patterns are fixed strings'
complete -c git -n '__fish_git_using_command log rev-list' -l perl-regexp -d 'Patterns are Perl-compatible regular expressions'
complete -c git -n '__fish_git_using_command log rev-list' -l remove-empty -d 'Stop when given path disappears from tree'
complete -c git -n '__fish_git_using_command log rev-list' -l merges -d 'Print only merge commits'
complete -c git -n '__fish_git_using_command log rev-list' -l no-merges -d 'Don\'t print commits with more than one parent'
complete -x -c git -n '__fish_git_using_command log rev-list' -l min-parents -d 'Show only commit with at least the given number of parents'
complete -x -c git -n '__fish_git_using_command log rev-list' -l max-parents -d 'Show only commit with at most the given number of parents'
complete -c git -n '__fish_git_using_command log rev-list' -l no-min-parents -d 'Show only commit without a minimum number of parents'
complete -c git -n '__fish_git_using_command log rev-list' -l no-max-parents -d 'Show only commit without a maximum number of parents'
complete -c git -n '__fish_git_using_command log rev-list' -l first-parent -d 'Follow only the first parent commit upon seeing a merge commit'
complete -c git -n '__fish_git_using_command log rev-list' -l not -d 'Reverse meaning of ^ prefix'
complete -c git -n '__fish_git_using_command log rev-list' -l all -d 'Show log for all branches, tags, and remotes'
complete -f -c git -n '__fish_git_using_command log rev-list' -l branches -d 'Show log for all matching branches'
complete -f -c git -n '__fish_git_using_command log rev-list' -l tags -d 'Show log for all matching tags'
complete -f -c git -n '__fish_git_using_command log rev-list' -l remotes -d 'Show log for all matching remotes'
complete -x -c git -n '__fish_git_using_command log rev-list' -l glob -d 'Show log for all matching branches, tags, and remotes'
complete -x -c git -n '__fish_git_using_command log rev-list' -l exclude -d 'Do not include refs matching given glob pattern'
complete -c git -n '__fish_git_using_command log rev-list' -l reflog -d 'Show log for all reflogs entries'
complete -c git -n '__fish_git_using_command log rev-list' -l ignore-missing -d 'Ignore invalid object names'
complete -c git -n '__fish_git_using_command log rev-list' -l bisect
complete -c git -n '__fish_git_using_command log rev-list' -l stdin -d 'Read commits from stdin'
complete -c git -n '__fish_git_using_command log rev-list' -l cherry-mark -d 'Mark equivalent commits with = and inequivalent with +'
complete -c git -n '__fish_git_using_command log rev-list' -l cherry-pick -d 'Omit equivalent commits'
complete -f -c git -n '__fish_git_using_command rev-list' -l filter -ra '(__fish_git_filters)' -d 'Omits objects from the list of printed objects'
complete -c git -n '__fish_git_using_command log' -l left-only
complete -c git -n '__fish_git_using_command log' -l right-only
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
  human
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
complete -f -c git -n '__fish_git_using_command log' -s l

function __fish__git_append_letters_nosep
    set -l token (commandline -tc)
    printf "%s\n" $token$argv
end

complete -x -c git -n '__fish_git_using_command log' -l diff-filter -a '(__fish__git_append_letters_nosep a\tExclude\ added c\tExclude\ copied d\tExclude\ deleted m\tExclude\ modified r\tExclude\ renamed t\tExclude\ type\ changed u\tExclude\ unmerged x\tExclude\ unknown b\tExclude\ broken A\tAdded C\tCopied D\tDeleted M\tModified R\tRenamed T\tType\ Changed U\tUnmerged X\tUnknown B\tBroken)'

### ls-files
complete -c git -n __fish_git_needs_command -a ls-files -d 'Show information about files'
complete -c git -n '__fish_git_using_command ls-files'
complete -c git -n '__fish_git_using_command ls-files' -s c -l cached -d 'Show cached files in the output'
complete -c git -n '__fish_git_using_command ls-files' -s d -l deleted -d 'Show deleted files in the output'
complete -c git -n '__fish_git_using_command ls-files' -s m -l modified -d 'Show modified files in the output'
complete -c git -n '__fish_git_using_command ls-files' -s o -l others -d 'Show other (i.e. untracked) files in the output'
complete -c git -n '__fish_git_using_command ls-files' -s i -l ignored -d 'Show only ignored files in the output'
complete -c git -n '__fish_git_using_command ls-files' -s s -l stage -d "Show staged contents' mode bits, object name and stage number in the output"
complete -c git -n '__fish_git_using_command ls-files' -l directory -d 'If a whole directory is classified as "other", show just its name'
complete -c git -n '__fish_git_using_command ls-files' -l no-empty-directory -d 'Do not list empty directories'
complete -c git -n '__fish_git_using_command ls-files' -s u -l unmerged -d 'Show unmerged files in the output'
complete -c git -n '__fish_git_using_command ls-files' -s k -l killed -d 'Show files on the filesystem that need to be removed for checkout-index to succeed'
complete -c git -n '__fish_git_using_command ls-files' -s z -d 'Use \0 delimiter'
complete -c git -n '__fish_git_using_command ls-files' -s x -l exclude -d 'Skip untracked files matching pattern'
complete -c git -n '__fish_git_using_command ls-files' -s X -l exclude-from -d 'Read exclude patterns from <file>; 1 per line'
complete -c git -n '__fish_git_using_command ls-files' -l exclude-per-directory -d 'Read extra exclude patterns that apply only to the dir and its subdirs in <file>'
complete -c git -n '__fish_git_using_command ls-files' -l exclude-standard -d 'Add the standard Git exclusions'
complete -c git -n '__fish_git_using_command ls-files' -l error-unmatch -d 'If any <file> does not appear in the index, treat this as an error'
complete -c git -n '__fish_git_using_command ls-files' -l with-tree
complete -c git -n '__fish_git_using_command ls-files' -s t -d 'Identifies the file status'
complete -c git -n '__fish_git_using_command ls-files' -s v -d 'Show file status, use lowercase letters for files assumed unchanged'
complete -c git -n '__fish_git_using_command ls-files' -s f -d 'Show file status, use lowercase letters for files marked as fsmonitor valid'
complete -c git -n '__fish_git_using_command ls-files' -l full-name -d 'Force paths to be output relative to the project top directory'
complete -c git -n '__fish_git_using_command ls-files' -l recurse-submodules -d 'Recursively calls ls-files on each submodule in the repository'
complete -c git -n '__fish_git_using_command ls-files' -l abbrev -d 'Show only a partial prefix'
complete -c git -n '__fish_git_using_command ls-files' -l debug -d 'After each line that describes a file, add more data about its cache entry'
complete -c git -n '__fish_git_using_command ls-files' -l eol -d 'Show <eolinfo> and <eolattr> of files'

### mailinfo
complete -f -c git -n __fish_git_needs_command -a mailinfo -d 'Extracts patch and authorship from an e-mail'
complete -f -c git -n '__fish_git_using_command mailinfo am' -s k -d 'Do not remove email cruft from subject'
complete -f -c git -n '__fish_git_using_command mailinfo' -s b -d 'Only strip bracket pairs containing \'PATCH\''
complete -f -c git -n '__fish_git_using_command mailinfo am' -s u -d 'Do not reencode author name and email'
complete -x -c git -n '__fish_git_using_command mailinfo' -l encoding -d 'Re-encode to given charset'
complete -f -c git -n '__fish_git_using_command mailinfo' -s n -d 'Disable all charset re-encoding of metadata'
complete -f -c git -n '__fish_git_using_command mailinfo am' -s m -l message-id -d 'Copy message id to the end of commit message'
complete -f -c git -n '__fish_git_using_command mailinfo' -l scissors -d 'Remove everything above scissor line'
complete -f -c git -n '__fish_git_using_command mailinfo' -l no-scissors -d 'Ignore scissor lines'
complete -x -c git -n '__fish_git_using_command mailinfo' -l quoted-cr -a 'nowarn warn strip' -d 'Action when processed email message end with CRLF instead of LF'

### mailsplit
complete -c git -n __fish_git_needs_command -a mailsplit -d 'mbox splitter'
complete -F -c git -n '__fish_git_using_command mailsplit' -s o -d 'Directory to place individual messages'
complete -f -c git -n '__fish_git_using_command mailsplit' -s b -d 'Treat files not starting with From line as single mail message'
complete -x -c git -n '__fish_git_using_command mailsplit' -s d -d 'File name prefix digit precision'
complete -x -c git -n '__fish_git_using_command mailsplit' -s f -d 'Skip first n numbers'
complete -f -c git -n '__fish_git_using_command mailsplit am' -l keep-cr -d 'Do not remove \\r from lines starting with \\n\\r'
complete -f -c git -n '__fish_git_using_command mailsplit' -l mboxrd -d 'Input is of mboxrd form'

### maintenance
complete -f -c git -n __fish_git_needs_command -a maintenance -d 'Run tasks to optimize Git repository data'
complete -f -c git -n '__fish_git_using_command maintenance' -a register -d 'Initialize Git config vars for maintenance'
complete -f -c git -n '__fish_git_using_command maintenance' -a run -d 'Run one or more maintenance tasks'
complete -f -c git -n '__fish_git_using_command maintenance' -a start -d 'Start maintenance'
complete -f -c git -n '__fish_git_using_command maintenance' -a stop -d 'Halt background maintenance'
complete -f -c git -n '__fish_git_using_command maintenance' -a unregister -d 'Remove repository from background maintenance'
complete -f -c git -n '__fish_git_using_command maintenance' -l quiet -d 'Supress logs'
complete -x -c git -n '__fish_git_using_command maintenance' -l task -a 'commit-graph prefetch gc loose-objects incremental-repack pack-refs' -d 'Tasks to run'
complete -f -c git -n '__fish_git_using_command maintenance' -l auto -d 'Run maintenance only when necessary'
complete -f -c git -n '__fish_git_using_command maintenance' -l schedule -d 'Run maintenance on certain intervals'

### merge
complete -f -c git -n __fish_git_needs_command -a merge -d 'Join multiple development histories'
complete -f -c git -n '__fish_git_using_command merge' -ka '(__fish_git_branches)'
complete -f -c git -n '__fish_git_using_command merge' -l commit -d "Autocommit the merge"
complete -f -c git -n '__fish_git_using_command merge' -l no-commit -d "Don't autocommit the merge"
complete -f -c git -n '__fish_git_using_command merge' -s e -l edit -d 'Edit auto-generated merge message'
complete -f -c git -n '__fish_git_using_command merge' -l no-edit -d "Don't edit auto-generated merge message"
complete -f -c git -n '__fish_git_using_command merge' -l ff -d "Don't generate a merge commit if merge is fast-forward"
complete -f -c git -n '__fish_git_using_command merge' -l no-ff -d "Generate a merge commit even if merge is fast-forward"
complete -f -c git -n '__fish_git_using_command merge' -l ff-only -d 'Refuse to merge unless fast-forward possible'
complete -f -c git -n '__fish_git_using_command merge' -s S -l gpg-sign -d 'GPG-sign the merge commit'
complete -f -c git -n '__fish_git_using_command merge' -l log -d 'Populate the log message with one-line descriptions'
complete -f -c git -n '__fish_git_using_command merge' -l no-log -d "Don't populate the log message with one-line descriptions"
complete -f -c git -n '__fish_git_using_command merge' -l signoff -d 'Add Signed-off-by line at the end of the merge commit message'
complete -f -c git -n '__fish_git_using_command merge' -l no-signoff -d 'Do not add a Signed-off-by line at the end of the merge commit message'
complete -f -c git -n '__fish_git_using_command merge' -l stat -d "Show diffstat of the merge"
complete -f -c git -n '__fish_git_using_command merge' -s n -l no-stat -d "Don't show diffstat of the merge"
complete -f -c git -n '__fish_git_using_command merge' -l squash -d "Squash changes from other branch as a single commit"
complete -f -c git -n '__fish_git_using_command merge' -l no-squash -d "Don't squash changes"
complete -x -c git -n '__fish_git_using_command merge' -s s -l strategy -d 'Use the given merge strategy'
complete -r -c git -n '__fish_git_using_command merge' -s X -l strategy-option -d 'Pass given option to the merge strategy'
complete -f -c git -n '__fish_git_using_command merge' -l verify-signatures -d 'Abort merge if other branch tip commit is not signed with a valid key'
complete -f -c git -n '__fish_git_using_command merge' -l no-verify-signatures -d 'Do not abort merge if other branch tip commit is not signed with a valid key'
complete -f -c git -n '__fish_git_using_command merge' -s q -l quiet -d 'Be quiet'
complete -f -c git -n '__fish_git_using_command merge' -s v -l verbose -d 'Be verbose'
complete -f -c git -n '__fish_git_using_command merge' -l progress -d 'Force progress status'
complete -f -c git -n '__fish_git_using_command merge' -l no-progress -d 'Force no progress status'
complete -f -c git -n '__fish_git_using_command merge' -l allow-unrelated-histories -d 'Allow merging even when branches do not share a common history'
complete -x -c git -n '__fish_git_using_command merge' -s m -d 'Set the commit message'
complete -f -c git -n '__fish_git_using_command merge' -l rerere-autoupdate -d 'If possible, use previous conflict resolutions'
complete -f -c git -n '__fish_git_using_command merge' -l no-rerere-autoupdate -d 'Do not use previous conflict resolutions'
complete -f -c git -n '__fish_git_using_command merge' -l abort -d 'Abort the current conflict resolution process'
complete -f -c git -n '__fish_git_using_command merge' -l continue -d 'Conclude current conflict resolution process'
complete -f -c git -n '__fish_git_using_command merge' -l autostash -d 'Before starting merge, stash local changes, and apply stash when done'
complete -f -c git -n '__fish_git_using_command merge' -l no-autostash -d 'Do not stash local changes before starting merge'

### merge-base
complete -f -c git -n __fish_git_needs_command -a merge-base -d 'Find a common ancestor for a merge'
complete -f -c git -n '__fish_git_using_command merge-base' -ka '(__fish_git_branches)'
complete -f -c git -n '__fish_git_using_command merge-base' -s a -l all -d 'Output all merge bases for the commits, instead of just one'
complete -f -c git -n '__fish_git_using_command merge-base' -l octopus -d 'Compute the best common ancestors of all supplied commits'
complete -f -c git -n '__fish_git_using_command merge-base' -l independent -d 'Print a minimal subset of the supplied commits with the same ancestors'
complete -f -c git -n '__fish_git_using_command merge-base' -l is-ancestor -d 'Check if the first commit is an ancestor of the second commit'
complete -f -c git -n '__fish_git_using_command merge-base' -l fork-point -d 'Find the point at which a branch forked from another branch ref'

### mergetool

complete -f -c git -n __fish_git_needs_command -a mergetool -d 'Run merge conflict resolution tool'
complete -f -c git -n '__fish_git_using_command mergetool' -s t -l tool -d "Use specific merge resolution program" -a "(__fish_git_diffmerge_tools mergetool)"
complete -f -c git -n '__fish_git_using_command mergetool' -l tool-help -d 'Print a list of merge tools that may be used with `--tool`'
complete -f -c git -n '__fish_git_using_command mergetool' -a "(__fish_git_files unmerged)"
complete -f -c git -n '__fish_git_using_command mergetool' -s y -l no-prompt -d 'Do not prompt before launching a diff tool'
complete -f -c git -n '__fish_git_using_command mergetool' -l prompt -d 'Prompt before each invocation of the merge resolution program'
complete -c git -n '__fish_git_using_command mergetool' -s O -d 'Process files in the order specified in the file passed as argument'

### mv
complete -c git -n __fish_git_needs_command -a mv -d 'Move or rename a file'
complete -f -c git -n '__fish_git_using_command mv' -a '(__fish_git ls-files)'
complete -f -c git -n '__fish_git_using_command mv' -s f -l force -d 'Force rename/moving even if target exists'
complete -f -c git -n '__fish_git_using_command mv' -s k -d 'Skip rename/move which can lead to error'
complete -f -c git -n '__fish_git_using_command mv' -s n -l dry-run -d 'Only show what would happen'
complete -f -c git -n '__fish_git_using_command mv' -s v -l verbose -d 'Report names of files as they are changed'

### notes
set -l notescommands add copy append edit show merge remove # list prune get-ref
complete -c git -n __fish_git_needs_command -a notes -d 'Add or inspect object notes'
complete -f -c git -n "__fish_git_using_command notes" -n "not __fish_seen_subcommand_from $notescommands" -a list -d 'List notes for given object'
complete -f -c git -n "__fish_git_using_command notes" -n "not __fish_seen_subcommand_from $notescommands" -a add -d 'Add notes for a given object'
complete -f -c git -n "__fish_git_using_command notes" -n "not __fish_seen_subcommand_from $notescommands" -a copy -d 'Copy notes from object1 to object2'
complete -f -c git -n "__fish_git_using_command notes" -n "not __fish_seen_subcommand_from $notescommands" -a append -d 'Append to the notes of existing object'
complete -f -c git -n "__fish_git_using_command notes" -n "not __fish_seen_subcommand_from $notescommands" -a edit -d 'Edit notes for a given object'
complete -f -c git -n "__fish_git_using_command notes" -n "not __fish_seen_subcommand_from $notescommands" -a show -d 'Show notes for given object'
complete -f -c git -n "__fish_git_using_command notes" -n "not __fish_seen_subcommand_from $notescommands" -a merge -d 'Merge the given notes ref to current notes ref'
complete -f -c git -n "__fish_git_using_command notes" -n "not __fish_seen_subcommand_from $notescommands" -a remove -d 'Remove notes for given object'
complete -f -c git -n "__fish_git_using_command notes" -n "not __fish_seen_subcommand_from $notescommands" -a prune -d 'Remove notes for non-existing/unreachable objects'
complete -f -c git -n "__fish_git_using_command notes" -n "not __fish_seen_subcommand_from $notescommands" -a get-ref -d 'Print current notes ref'
complete -f -c git -n "__fish_git_using_command notes" -n "__fish_seen_subcommand_from $notescommands" -ka '(__fish_git_commits)'
complete -f -c git -n "__fish_git_using_command notes" -n "__fish_seen_subcommand_from add copy" -s f -l force -d 'Overwrite existing notes'
complete -f -c git -n "__fish_git_using_command notes" -n "__fish_seen_subcommand_from add append edit" -l allow-empty -d 'Allow empty note'
complete -r -c git -n "__fish_git_using_command notes" -n "__fish_seen_subcommand_from add append" -s F -l file -d 'Read note message from file'
complete -x -c git -n "__fish_git_using_command notes" -n "__fish_seen_subcommand_from add append" -s m -l message -d 'Use this note message'
complete -f -c git -n "__fish_git_using_command notes" -n "__fish_seen_subcommand_from add append" -s C -l reuse-message -a '(__fish_git_commits)' -d 'Copy note from object'
complete -f -c git -n "__fish_git_using_command notes" -n "__fish_seen_subcommand_from add append" -s c -l reedit-message -a '(__fish_git_commits)' -d 'Copy and edit note from object'
complete -f -c git -n "__fish_git_using_command notes" -n "__fish_seen_subcommand_from copy remove" -l stdin -d 'Read object names from stdin'
complete -f -c git -n "__fish_git_using_command notes" -n "__fish_seen_subcommand_from merge remove prune" -s v -l verbose -d 'Be more verbose'
complete -f -c git -n "__fish_git_using_command notes" -n "__fish_seen_subcommand_from merge remove prune" -s q -l quiet -d 'Operate quietly'
complete -x -c git -n "__fish_git_using_command notes" -n "__fish_seen_subcommand_from merge" -s s -l strategy -d 'Merge strategy to use to resolve conflicts' -a "
	manual\t'Instruct the user to resolve merge conflicts'
	ours\t'Resolve conflicts in favour of local version'
	theirs\t'Resolve conflicts in favour of remote version'
	union\t'Resolve conflicts by concatenating local and remote versions'
	cat_sort_uniq\t'Concatenate, sort and remove duplicate lines'
	"
complete -f -c git -n "__fish_git_using_command notes" -n "__fish_seen_subcommand_from merge" -l commit -d 'Finalize git notes merge'
complete -f -c git -n "__fish_git_using_command notes" -n "__fish_seen_subcommand_from merge" -l abort -d 'Abort git notes merge'
complete -f -c git -n "__fish_git_using_command notes" -n "__fish_seen_subcommand_from remove" -l ignore-missing -d 'Do not throw error on deleting non-existing object note'

### prune
complete -f -c git -n __fish_git_needs_command -a prune -d 'Prune unreachable objects from the database'
complete -f -c git -n '__fish_git_using_command prune' -s n -l dry-run -d 'Just report what it would remove'
complete -f -c git -n '__fish_git_using_command prune' -s v -l verbose -d 'Report all removed objects'
complete -f -c git -n '__fish_git_using_command prune' -l progress -d 'Show progress'
# TODO options

### pull
complete -f -c git -n __fish_git_needs_command -a pull -d 'Fetch from and merge with another repo or branch'
complete -f -c git -n '__fish_git_using_command pull' -s q -l quiet -d 'Be quiet'
complete -f -c git -n '__fish_git_using_command pull' -s v -l verbose -d 'Be verbose'
# Options related to fetching
complete -f -c git -n '__fish_git_using_command pull' -l all -d 'Fetch all remotes'
complete -f -c git -n '__fish_git_using_command pull' -s a -l append -d 'Append ref names and object names'
complete -f -c git -n '__fish_git_using_command pull' -s f -l force -d 'Force update of local branches'
complete -f -c git -n '__fish_git_using_command pull' -s k -l keep -d 'Keep downloaded pack'
complete -f -c git -n '__fish_git_using_command pull' -l no-tags -d 'Disable automatic tag following'
complete -f -c git -n '__fish_git_using_command pull' -s p -l prune -d 'Remove remote-tracking references that no longer exist on the remote'
# TODO --upload-pack
complete -f -c git -n '__fish_git_using_command pull' -l progress -d 'Force progress status'
complete -f -c git -n '__fish_git_using_command pull' -n 'not __fish_git_branch_for_remote' -a '(__fish_git_remotes)' -d 'Remote alias'
complete -f -c git -n '__fish_git_using_command pull' -n __fish_git_branch_for_remote -ka '(__fish_git_branch_for_remote)'
# Options related to merging
complete -f -c git -n '__fish_git_using_command pull' -l commit -d "Autocommit the merge"
complete -f -c git -n '__fish_git_using_command pull' -l no-commit -d "Don't autocommit the merge"
complete -f -c git -n '__fish_git_using_command pull' -s e -l edit -d 'Edit auto-generated merge message'
complete -f -c git -n '__fish_git_using_command pull' -l no-edit -d "Don't edit auto-generated merge message"
complete -f -c git -n '__fish_git_using_command pull' -l ff -d "Don't generate a merge commit if merge is fast-forward"
complete -f -c git -n '__fish_git_using_command pull' -l no-ff -d "Generate a merge commit even if merge is fast-forward"
complete -f -c git -n '__fish_git_using_command pull' -l ff-only -d 'Refuse to merge unless fast-forward possible'
complete -f -c git -n '__fish_git_using_command pull' -s S -l gpg-sign -d 'GPG-sign the merge commit'
complete -f -c git -n '__fish_git_using_command pull' -l log -d 'Populate the log message with one-line descriptions'
complete -f -c git -n '__fish_git_using_command pull' -l no-log -d "Don't populate the log message with one-line descriptions"
complete -f -c git -n '__fish_git_using_command pull' -l signoff -d 'Add Signed-off-by line at the end of the merge commit message'
complete -f -c git -n '__fish_git_using_command pull' -l no-signoff -d 'Do not add a Signed-off-by line at the end of the merge commit message'
complete -f -c git -n '__fish_git_using_command pull' -l stat -d "Show diffstat of the merge"
complete -f -c git -n '__fish_git_using_command pull' -s n -l no-stat -d "Don't show diffstat of the merge"
complete -f -c git -n '__fish_git_using_command pull' -l squash -d "Squash changes from upstream branch as a single commit"
complete -f -c git -n '__fish_git_using_command pull' -l no-squash -d "Don't squash changes"
complete -x -c git -n '__fish_git_using_command pull' -s s -l strategy -d 'Use the given merge strategy'
complete -r -c git -n '__fish_git_using_command pull' -s X -l strategy-option -d 'Pass given option to the merge strategy'
complete -f -c git -n '__fish_git_using_command pull' -l verify-signatures -d 'Abort merge if upstream branch tip commit is not signed with a valid key'
complete -f -c git -n '__fish_git_using_command pull' -l no-verify-signatures -d 'Do not abort merge if upstream branch tip commit is not signed with a valid key'
complete -f -c git -n '__fish_git_using_command pull' -l allow-unrelated-histories -d 'Allow merging even when branches do not share a common history'
complete -f -c git -n '__fish_git_using_command pull' -s r -l rebase -d 'Rebase the current branch on top of the upstream branch'
complete -f -c git -n '__fish_git_using_command pull' -l no-rebase -d 'Do not rebase the current branch on top of the upstream branch'
complete -f -c git -n '__fish_git_using_command pull' -l autostash -d 'Before starting rebase, stash local changes, and apply stash when done'
complete -f -c git -n '__fish_git_using_command pull' -l no-autostash -d 'Do not stash local changes before starting rebase'
# TODO other options

### range-diff
complete -f -c git -n __fish_git_needs_command -a range-diff -d 'Compare two commit ranges'
complete -f -c git -n '__fish_git_using_command range-diff' -ka '(__fish_git_ranges)'
complete -f -c git -n '__fish_git_using_command range-diff' -l creation-factor -d 'Percentage by which creation is weighted'
complete -f -c git -n '__fish_git_using_command range-diff' -l no-dual-color -d 'Use simple diff colors'

### push
complete -f -c git -n __fish_git_needs_command -a push -d 'Push changes elsewhere'
complete -f -c git -n '__fish_git_using_command push' -n 'not __fish_git_branch_for_remote' -a '(__fish_git_remotes)' -d 'Remote alias'
complete -f -c git -n '__fish_git_using_command push' -n __fish_git_branch_for_remote -ka '(__fish_git_tags)' -d Tag
complete -f -c git -n '__fish_git_using_command push' -n __fish_git_branch_for_remote -ka '(__fish_git_branches)'
complete -f -c git -n '__fish_git_using_command push' -n __fish_git_branch_for_remote -ka '(__fish_git_heads)'
# The "refspec" here is an optional "+" to signify a force-push
complete -f -c git -n '__fish_git_using_command push' -n __fish_git_branch_for_remote -n 'string match -q "+*" -- (commandline -ct)' -ka '+(__fish_git_branches | string replace -r \t".*" "")' -d 'Force-push branch'
# git push REMOTE :BRANCH deletes BRANCH on remote REMOTE
complete -f -c git -n '__fish_git_using_command push' -n __fish_git_branch_for_remote -n 'string match -q ":*" -- (commandline -ct)' -ka ':(__fish_git_branch_for_remote | string replace -r \t".*" "")' -d 'Delete remote branch'
# then src:dest (where both src and dest are git objects, so we want to complete branches)
complete -f -c git -n '__fish_git_using_command push' -n __fish_git_branch_for_remote -n 'string match -q "+*:*" -- (commandline -ct)' -ka '(commandline -ct | string replace -r ":.*" ""):(__fish_git_branch_for_remote | string replace -r \t".*" "")' -d 'Force-push local branch to remote branch'
complete -f -c git -n '__fish_git_using_command push' -n __fish_git_branch_for_remote -n 'string match -q "*:*" -- (commandline -ct)' -ka '(commandline -ct | string replace -r ":.*" ""):(__fish_git_branch_for_remote | string replace -r \t".*" "")' -d 'Push local branch to remote branch'
complete -f -c git -n '__fish_git_using_command push' -l all -d 'Push all refs under refs/heads/'
complete -f -c git -n '__fish_git_using_command push' -l prune -d "Remove remote branches that don't have a local counterpart"
complete -f -c git -n '__fish_git_using_command push' -l mirror -d 'Push all refs under refs/'
complete -f -c git -n '__fish_git_using_command push' -l delete -d 'Delete all listed refs from the remote repository'
complete -f -c git -n '__fish_git_using_command push' -l tags -d 'Push all refs under refs/tags'
complete -f -c git -n '__fish_git_using_command push' -l follow-tags -d 'Push all usual refs plus the ones under refs/tags'
complete -f -c git -n '__fish_git_using_command push' -s n -l dry-run -d 'Do everything except actually send the updates'
complete -f -c git -n '__fish_git_using_command push' -l porcelain -d 'Produce machine-readable output'
complete -f -c git -n '__fish_git_using_command push' -s f -l force -d 'Force update of remote refs'
complete -f -c git -n '__fish_git_using_command push' -l force-with-lease -d 'Force update of remote refs, stopping if other\'s changes would be overwritten'
complete -f -c git -n '__fish_git_using_command push' -l force-if-includes -d 'Force an update only if the tip of the remote-tracking ref has been integrated locally'
complete -f -c git -n '__fish_git_using_command push' -s u -l set-upstream -d 'Add upstream (tracking) reference'
complete -f -c git -n '__fish_git_using_command push' -s q -l quiet -d 'Be quiet'
complete -f -c git -n '__fish_git_using_command push' -s v -l verbose -d 'Be verbose'
complete -f -c git -n '__fish_git_using_command push' -l progress -d 'Force progress status'
# TODO --recurse-submodules=check|on-demand

### rebase
complete -f -c git -n __fish_git_needs_command -a rebase -d 'Reapply commit sequence on a new base'
complete -f -c git -n '__fish_git_using_command rebase' -a '(__fish_git_remotes)' -d 'Remote alias'
complete -f -c git -n '__fish_git_using_command rebase' -ka '(__fish_git_branches)'
complete -f -c git -n '__fish_git_using_command rebase' -a '(__fish_git_heads)' -d Head
complete -f -c git -n '__fish_git_using_command rebase' -ka '(__fish_git_tags)' -d Tag -k
complete -f -c git -n '__fish_git_using_command rebase' -a '(__fish_git_recent_commits)' -k
complete -f -c git -n '__fish_git_using_command rebase' -n __fish_git_is_rebasing -l continue -d 'Restart the rebasing process'
complete -f -c git -n '__fish_git_using_command rebase' -n __fish_git_is_rebasing -l abort -d 'Abort the rebase operation'
complete -f -c git -n '__fish_git_using_command rebase' -n __fish_git_is_rebasing -l edit-todo -d 'Edit the todo list'
complete -f -c git -n '__fish_git_using_command rebase' -l keep-empty -d "Keep the commits that don't change anything"
complete -f -c git -n '__fish_git_using_command rebase' -n __fish_git_is_rebasing -l skip -d 'Restart the rebasing process by skipping the current patch'
complete -f -c git -n '__fish_git_using_command rebase' -s m -l merge -d 'Use merging strategies to rebase'
complete -f -c git -n '__fish_git_using_command rebase' -s q -l quiet -d 'Be quiet'
complete -f -c git -n '__fish_git_using_command rebase' -s v -l verbose -d 'Be verbose'
complete -f -c git -n '__fish_git_using_command rebase' -l stat -d "Show diffstat of the rebase"
complete -f -c git -n '__fish_git_using_command rebase' -s n -l no-stat -d "Don't show diffstat of the rebase"
complete -f -c git -n '__fish_git_using_command rebase' -l verify -d "Allow the pre-rebase hook to run"
complete -f -c git -n '__fish_git_using_command rebase' -l no-verify -d "Don't allow the pre-rebase hook to run"
complete -f -c git -n '__fish_git_using_command rebase' -s f -l force-rebase -d 'Force the rebase'
complete -f -c git -n '__fish_git_using_command rebase' -l committer-date-is-author-date -d "Use the author date as the committer date"
complete -f -c git -n '__fish_git_using_command rebase' -l ignore-date -d "Use the committer date as the author date"
complete -f -c git -n '__fish_git_using_command rebase' -s i -l interactive -d 'Interactive mode'
complete -f -c git -n '__fish_git_using_command rebase' -s p -l preserve-merges -d 'Try to recreate merges'
complete -f -c git -n '__fish_git_using_command rebase' -s r -l rebase-merges -a 'rebase-cousins no-rebase-cousins' -d 'Preserve branch structure'
complete -f -c git -n '__fish_git_using_command rebase' -l root -d 'Rebase all reachable commits'
complete -f -c git -n '__fish_git_using_command rebase' -l autosquash -d 'Automatic squashing'
complete -f -c git -n '__fish_git_using_command rebase' -l no-autosquash -d 'No automatic squashing'
complete -f -c git -n '__fish_git_using_command rebase' -l autostash -d 'Before starting rebase, stash local changes, and apply stash when done'
complete -f -c git -n '__fish_git_using_command rebase' -l no-autostash -d 'Do not stash local changes before starting rebase'
complete -f -c git -n '__fish_git_using_command rebase' -l no-ff -d 'No fast-forward'
complete -f -c git -n '__fish_git_using_command rebase' -l onto -d 'Rebase current branch onto given upstream or newbase' -ka '(__fish_git_branches)'
complete -f -c git -n '__fish_git_using_command rebase' -l update-refs -d 'Update any branches that point to commits being rebased'
complete -f -c git -n '__fish_git_using_command rebase' -l no-update-refs -d 'Don\'t update any branches that point to commits being rebased'
# This actually takes script for $SHELL, but completing that is... complicated.
complete -r -c git -n '__fish_git_using_command rebase' -l exec -d 'Execute shellscript'

### reflog
set -l reflogcommands show expire delete exists
complete -f -c git -n __fish_git_needs_command -a reflog -d 'Manage reflog information'
complete -f -c git -n '__fish_git_using_command reflog' -ka '(__fish_git_branches)'
complete -f -c git -n '__fish_git_using_command reflog' -ka '(__fish_git_heads)' -d Head

complete -f -c git -n "__fish_git_using_command reflog" -n "not __fish_seen_subcommand_from $reflogcommands" -a "$reflogcommands"

### reset
complete -c git -n __fish_git_needs_command -a reset -d 'Reset current HEAD to the specified state'
complete -f -c git -n '__fish_git_using_command reset' -l hard -d 'Reset the index and the working tree'
complete -f -c git -n '__fish_git_using_command reset' -l soft -d 'Reset head without touching the index or the working tree'
complete -f -c git -n '__fish_git_using_command reset' -l mixed -d 'The default: reset the index but not the working tree'
complete -c git -n '__fish_git_using_command reset' -n 'not contains -- -- (commandline -xpc)' -ka '(__fish_git_branches)'
# reset can either undo changes to versioned modified files,
# or remove files from the staging area.
# Deleted files seem to need a "--" separator.
complete -f -c git -n '__fish_git_using_command reset' -n 'not contains -- -- (commandline -xpc)' -a '(__fish_git_files all-staged modified)'
complete -F -c git -n '__fish_git_using_command reset' -n 'contains -- -- (commandline -xpc)'
complete -f -c git -n '__fish_git_using_command reset' -n 'not contains -- -- (commandline -xpc)' -a '(__fish_git_reflog)' -d Reflog
# TODO options

### restore and switch
# restore options
complete -f -c git -n __fish_git_needs_command -a restore -d 'Restore working tree files'
complete -f -c git -n '__fish_git_using_command restore' -r -s s -l source -d 'Specify the source tree used to restore the working tree' -ka '(__fish_git_refs)'
complete -f -c git -n '__fish_git_using_command restore' -s p -l patch -d 'Interactive mode'
complete -f -c git -n '__fish_git_using_command restore' -s W -l worktree -d 'Restore working tree (default)'
complete -f -c git -n '__fish_git_using_command restore' -s S -l staged -d 'Restore the index'
complete -f -c git -n '__fish_git_using_command restore' -l ours -d 'When restoring files, use stage #2 (ours)'
complete -f -c git -n '__fish_git_using_command restore' -l theirs -d 'When restoring files, use stage #3 (theirs)'
complete -f -c git -n '__fish_git_using_command restore' -s m -l merge -d 'Recreate the conflicted merge in the unmerged paths when restoring files'
complete -f -c git -n '__fish_git_using_command restore' -l ignore-unmerged -d 'When restoring files, do not abort the operation if there are unmerged entries'
complete -f -c git -n '__fish_git_using_command restore' -l ignore-skip-worktree-bits -d 'Ignore the sparse-checkout file and unconditionally restore any files in <pathspec>'
complete -f -c git -n '__fish_git_using_command restore' -l overlay -d 'Never remove files when restoring'
complete -f -c git -n '__fish_git_using_command restore' -l no-overlay -d 'Remove files when restoring (default)'
complete -f -c git -n '__fish_git_using_command restore' -n 'not __fish_git_contains_opt -s S staged' -a '(__fish_git_files modified deleted modified-staged-deleted)'
complete -f -c git -n '__fish_git_using_command restore' -n '__fish_git_contains_opt -s S staged' -a '(__fish_git_files added modified-staged deleted-staged renamed copied unmerged)'
complete -F -c git -n '__fish_git_using_command restore' -n '__fish_git_contains_opt -s s source'
# switch options
complete -f -c git -n __fish_git_needs_command -a switch -d 'Switch to a branch'
complete -f -c git -n '__fish_git_using_command switch' -ka '(__fish_git_unique_remote_branches)' -d 'Unique Remote Branch'
complete -f -c git -n '__fish_git_using_command switch' -ka '(__fish_git_branches)'
complete -f -c git -n '__fish_git_using_command switch' -s c -l create -d 'Create a new branch'
complete -f -c git -n '__fish_git_using_command switch' -s C -l force-create -d 'Force create a new branch'
complete -f -c git -n '__fish_git_using_command switch' -s d -l detach -rka '(__fish_git_recent_commits --all)'
complete -f -c git -n '__fish_git_using_command switch' -s d -l detach -d 'Switch to a commit for inspection and discardable experiment' -rka '(__fish_git_refs)'
complete -f -c git -n '__fish_git_using_command switch' -l guess -d 'Guess branch name from remote branch (default)'
complete -f -c git -n '__fish_git_using_command switch' -l no-guess -d 'Do not guess branch name from remote branch'
complete -f -c git -n '__fish_git_using_command switch' -s f -l force -l discard-changes -d 'Proceed even if the index or the working tree differs from HEAD'
complete -f -c git -n '__fish_git_using_command switch' -s m -l merge -d 'Merge the current branch and contents of the working tree into a new branch'
complete -f -c git -n '__fish_git_using_command switch' -s t -l track -d 'Track remote branch when creating a new branch'
complete -f -c git -n '__fish_git_using_command switch' -l no-track -d 'Do not track remote branch when creating a new branch'
complete -f -c git -n '__fish_git_using_command switch' -r -l orphan -d 'Create a new orphan branch'
complete -f -c git -n '__fish_git_using_command switch' -l ignore-other-worktrees -d 'Force check out of the reference'
complete -f -c git -n '__fish_git_using_command switch' -l recurse-submodules -d 'Update the work trees of submodules'
complete -f -c git -n '__fish_git_using_command switch' -l no-recurse-submodules -d 'Do not update the work trees of submodules'
# common options
complete -f -c git -n '__fish_git_using_command restore switch' -s q -l quiet -d 'Suppress messages'
complete -f -c git -n '__fish_git_using_command restore switch' -l progress -d 'Report progress status to stderr (default)'
complete -f -c git -n '__fish_git_using_command restore switch' -l no-progress -d 'Do not report progress status to stderr'
complete -f -c git -n '__fish_git_using_command restore switch' -l 'conflict=merge' -d 'Same as --merge, but specify \'merge\' as the conflicting hunk style (default)'
complete -f -c git -n '__fish_git_using_command restore switch' -l 'conflict=diff3' -d 'Same as --merge, but specify \'diff3\' as the conflicting hunk style'

### rev-list
# (options see log)
complete -f -c git -n __fish_git_needs_command -a rev-list -d 'List commits in chronological order'

### rev-parse
complete -f -c git -n __fish_git_needs_command -a rev-parse -d 'Parse revision names or give repo information'
complete -f -c git -n '__fish_git_using_command rev-parse' -ka '(__fish_git_branches)'
complete -f -c git -n '__fish_git_using_command rev-parse' -a '(__fish_git_heads)' -d Head
complete -c git -n '__fish_git_using_command rev-parse' -ka '(__fish_git_tags)' -d Tag
complete -c git -n '__fish_git_using_command rev-parse' -l abbrev-ref -d 'Output non-ambiguous short object names'

### revert
complete -f -c git -n __fish_git_needs_command -a revert -d 'Revert an existing commit'
complete -f -c git -n '__fish_git_using_command revert' -ka '(__fish_git_commits)'
complete -f -c git -n '__fish_git_using_command revert' -l continue -d 'Continue the operation in progress'
complete -f -c git -n '__fish_git_using_command revert' -l abort -d 'Cancel the operation'
complete -f -c git -n '__fish_git_using_command revert' -l skip -d 'Skip the current commit and continue with the rest of the sequence'
complete -f -c git -n '__fish_git_using_command revert' -l quit -d 'Forget about the current operation in progress'
complete -f -c git -n '__fish_git_using_command revert' -l no-edit -d 'Do not start the commit message editor'
complete -f -c git -n '__fish_git_using_command revert' -s n -l no-commit -d 'Apply changes to index but don\'t create a commit'
complete -f -c git -n '__fish_git_using_command revert' -s s -l signoff -d 'Add a Signed-off-by trailer at the end of the commit message'
complete -f -c git -n '__fish_git_using_command revert' -l rerere-autoupdate -d 'Allow the rerere mechanism to update the index automatically'
complete -f -c git -n '__fish_git_using_command revert' -l no-rerere-autoupdate -d 'Prevent the rerere mechanism from updating the index with auto-conflict resolution'
# TODO options

### rm
complete -c git -n __fish_git_needs_command -a rm -d 'Remove files from the working tree and/or staging area'
complete -c git -n '__fish_git_using_command rm' -l cached -d 'Unstage files from the index'
complete -c git -n '__fish_git_using_command rm' -n '__fish_git_contains_opt cached' -f -a '(__fish_git_files all-staged)'
complete -c git -n '__fish_git_using_command rm' -l ignore-unmatch -d 'Exit with a zero status even if no files matched'
complete -c git -n '__fish_git_using_command rm' -s r -d 'Allow recursive removal'
complete -c git -n '__fish_git_using_command rm' -s q -l quiet -d 'Be quiet'
complete -c git -n '__fish_git_using_command rm' -s f -l force -d 'Override the up-to-date check'
complete -c git -n '__fish_git_using_command rm' -s n -l dry-run -d 'Dry run'
complete -c git -n '__fish_git_using_command rm' -l sparse -d 'Allow updating index entries outside of the sparse-checkout cone'
# TODO options

### status
complete -f -c git -n __fish_git_needs_command -a status -d 'Show the working tree status'
complete -f -c git -n '__fish_git_using_command status' -s s -l short -d 'Give the output in the short-format'
complete -f -c git -n '__fish_git_using_command status' -s b -l branch -d 'Show the branch and tracking info even in short-format'
complete -f -c git -n '__fish_git_using_command status' -l porcelain -d 'Give the output in a stable, easy-to-parse format'
complete -f -c git -n '__fish_git_using_command status' -s z -d 'Terminate entries with null character'
complete -f -c git -n '__fish_git_using_command status' -s u -l untracked-files -x -a 'no normal all' -d 'The untracked files handling mode'
complete -f -c git -n '__fish_git_using_command status' -l ignore-submodules -x -a 'none untracked dirty all' -d 'Ignore changes to submodules'
complete -f -c git -n '__fish_git_using_command status' -s v -l verbose -d 'Also show the textual changes that are staged to be committed'
complete -f -c git -n '__fish_git_using_command status' -l no-ahead-behind -d 'Do not display detailed ahead/behind upstream-branch counts'
complete -f -c git -n '__fish_git_using_command status' -l renames -d 'Turn on rename detection regardless of user configuration'
complete -f -c git -n '__fish_git_using_command status' -l no-renames -d 'Turn off rename detection regardless of user configuration'
# TODO options

### stripspace
complete -f -c git -n __fish_git_needs_command -a stripspace -d 'Remove unnecessary whitespace'
complete -f -c git -n '__fish_git_using_command stripspace' -s s -l strip-comments -d 'Strip all lines starting with comment character'
complete -f -c git -n '__fish_git_using_command stripspace' -s c -l comment-lines -d 'Prepend comment character to each line'

### tag
complete -f -c git -n __fish_git_needs_command -a tag -d 'Create, list, delete or verify a tag object signed with GPG'
complete -f -c git -n '__fish_git_using_command tag' -n '__fish_not_contain_opt -s d' -n '__fish_not_contain_opt -s v' -n 'test (count (commandline -xpc | string match -r -v \'^-\')) -eq 3' -ka '(__fish_git_branches)'
complete -f -c git -n '__fish_git_using_command tag' -s a -l annotate -d 'Make an unsigned, annotated tag object'
complete -f -c git -n '__fish_git_using_command tag' -s s -l sign -d 'Make a GPG-signed tag'
complete -f -c git -n '__fish_git_using_command tag' -s d -l delete -d 'Remove a tag'
complete -f -c git -n '__fish_git_using_command tag' -s v -l verify -d 'Verify signature of a tag'
complete -f -c git -n '__fish_git_using_command tag' -s f -l force -d 'Force overwriting existing tag'
complete -f -c git -n '__fish_git_using_command tag' -s l -l list -d 'List tags'
complete -f -c git -n '__fish_git_using_command tag' -l contains -xka '(__fish_git_commits)' -d 'List tags that contain a commit'
complete -f -c git -n '__fish_git_using_command tag' -n '__fish_git_contains_opt -s d delete -s v verify -s f force' -ka '(__fish_git_tags)' -d Tag
# TODO options

### update-index
complete -c git -n __fish_git_needs_command -a update-index -d 'Register file contents in the working tree to the index'
complete -f -c git -n '__fish_git_using_command update-index' -l add -d 'Add specified files to the index'
complete -f -c git -n '__fish_git_using_command update-index' -l remove -d 'Remove specified files from the index'
complete -f -c git -n '__fish_git_using_command update-index' -l refresh -d 'Refresh current index'
complete -f -c git -n '__fish_git_using_command update-index' -s q -d 'Continue refresh after error'
complete -f -c git -n '__fish_git_using_command update-index' -l ignore-submodules -d 'Do not try to update submodules'
complete -f -c git -n '__fish_git_using_command update-index' -l unmerged -d 'Continue on unmerged changes in the index'
complete -f -c git -n '__fish_git_using_command update-index' -l ignore-missing -d 'Ignores missing files during a refresh'
complete -f -c git -n '__fish_git_using_command update-index' -l index-info -d 'Read index information from stdin'
complete -x -c git -n '__fish_git_using_command update-index' -l chmod -a '+x\tAdd\ execute\ permissions -x\tRemove\ execute\ permissions' -d 'Set execute permissions'
complete -f -c git -n '__fish_git_using_command update-index' -l assume-unchanged -d 'Set the "assume unchanged" bit for the paths'
complete -f -c git -n '__fish_git_using_command update-index' -l no-assume-unchanged -d 'Unset the "assume unchanged" bit'
complete -f -c git -n '__fish_git_using_command update-index' -l really-refresh -d 'Refresh but check stat info unconditionally'
complete -f -c git -n '__fish_git_using_command update-index' -l skip-worktree -d 'Set the "fsmonitor valid" bit'
complete -f -c git -n '__fish_git_using_command update-index' -l no-skip-worktree -d 'Unset the "fsmonitor valid" bit'
complete -f -c git -n '__fish_git_using_command update-index' -l fsmonitor-valid -d 'Set the "fsmonitor valid" bit'
complete -f -c git -n '__fish_git_using_command update-index' -l no-fsmonitor-valid -d 'Unset the "fsmonitor valid" bit'
complete -f -c git -n '__fish_git_using_command update-index' -s g -l again -d 'Run git update-index on paths with differing index'
complete -f -c git -n '__fish_git_using_command update-index' -l unresolve -d 'Restores the state of a file during a merge'
complete -r -c git -n '__fish_git_using_command update-index' -l info-only -d 'Do not create objects in the object database'
complete -f -c git -n '__fish_git_using_command update-index' -l force-remove -d 'Forcefully remove the file from the index'
complete -f -c git -n '__fish_git_using_command update-index' -l replace -d 'Replace conflicting entries'
complete -f -c git -n '__fish_git_using_command update-index' -l stdin -d 'Read list of paths from stdin'
complete -f -c git -n '__fish_git_using_command update-index' -l verbose -d 'Report changes to index'
complete -x -c git -n '__fish_git_using_command update-index' -l index-version -a "2\t\t3\t\t4" -d 'Set index-version'
complete -f -c git -n '__fish_git_using_command update-index' -s z -d 'Seperate paths with NUL instead of LF'
complete -f -c git -n '__fish_git_using_command update-index' -l split-index -d 'Enable split index mode'
complete -f -c git -n '__fish_git_using_command update-index' -l no-split-index -d 'Disable split index mode'
complete -f -c git -n '__fish_git_using_command update-index' -l untracked-cache -d 'Enable untracked cache feature'
complete -f -c git -n '__fish_git_using_command update-index' -l no-untracked-cache -d 'Disable untracked cache feature'
complete -f -c git -n '__fish_git_using_command update-index' -l test-untracked-cache -d 'Only perform tests on the working directory'
complete -f -c git -n '__fish_git_using_command update-index' -l force-untracked-cache -d 'Same as --untracked-cache'
complete -f -c git -n '__fish_git_using_command update-index' -l fsmonitor -d 'Enable files system monitor feature'
complete -f -c git -n '__fish_git_using_command update-index' -l no-fsmonitor -d 'Disable files system monitor feature'

### worktree
set -l git_worktree_commands add list lock move prune remove unlock
complete -c git -n __fish_git_needs_command -a worktree -d 'Manage multiple working trees'
complete -f -c git -n "__fish_git_using_command worktree" -n "not __fish_seen_subcommand_from $git_worktree_commands" -a add -d 'Create a working tree'
complete -f -c git -n "__fish_git_using_command worktree" -n "not __fish_seen_subcommand_from $git_worktree_commands" -a list -d 'List details of each worktree'
complete -f -c git -n "__fish_git_using_command worktree" -n "not __fish_seen_subcommand_from $git_worktree_commands" -a lock -d 'Lock a working tree'
complete -f -c git -n "__fish_git_using_command worktree" -n "not __fish_seen_subcommand_from $git_worktree_commands" -a move -d 'Move a working tree to a new location'
complete -f -c git -n "__fish_git_using_command worktree" -n "not __fish_seen_subcommand_from $git_worktree_commands" -a prune -d 'Prune working tree information in $GIT_DIR/worktrees'
complete -f -c git -n "__fish_git_using_command worktree" -n "not __fish_seen_subcommand_from $git_worktree_commands" -a remove -d 'Remove a working tree'
complete -f -c git -n "__fish_git_using_command worktree" -n "not __fish_seen_subcommand_from $git_worktree_commands" -a unlock -d 'Unlock a working tree'

complete -f -c git -n '__fish_git_using_command worktree' -n '__fish_seen_subcommand_from add move remove' -s f -l force -d 'Override safeguards'

complete -c git -n '__fish_git_using_command worktree' -n '__fish_seen_subcommand_from add'
complete -c git -n '__fish_git_using_command worktree' -n '__fish_seen_subcommand_from add' -ka '(__fish_git_branches)'
complete -c git -n '__fish_git_using_command worktree' -n '__fish_seen_subcommand_from add' -ka '(__fish_git_heads)' -d Head
complete -c git -n '__fish_git_using_command worktree' -n '__fish_seen_subcommand_from add' -ka '(__fish_git_tags)' -d Tag
complete -c git -n '__fish_git_using_command worktree' -n '__fish_seen_subcommand_from add' -ka '(__fish_git_unique_remote_branches)' -d 'Unique Remote Branch'
complete -c git -n '__fish_git_using_command worktree' -n '__fish_seen_subcommand_from add' -ka '(__fish_git_local_branches)'
complete -x -c git -n '__fish_git_using_command worktree' -n '__fish_seen_subcommand_from add' -s b -d 'Create a new branch'
complete -x -c git -n '__fish_git_using_command worktree' -n '__fish_seen_subcommand_from add' -s B -d 'Create a new branch even if it already exists'
complete -f -c git -n '__fish_git_using_command worktree' -n '__fish_seen_subcommand_from add' -l detach -d 'Detach HEAD in the new working tree'
complete -f -c git -n '__fish_git_using_command worktree' -n '__fish_seen_subcommand_from add' -l checkout -d 'Checkout <commit-ish> after creating working tree'
complete -f -c git -n '__fish_git_using_command worktree' -n '__fish_seen_subcommand_from add' -l no-checkout -d 'Suppress checkout'
complete -f -c git -n '__fish_git_using_command worktree' -n '__fish_seen_subcommand_from add' -l guess-remote
complete -f -c git -n '__fish_git_using_command worktree' -n '__fish_seen_subcommand_from add' -l no-guess-remote
complete -f -c git -n '__fish_git_using_command worktree' -n '__fish_seen_subcommand_from add' -l track -d 'Mark <commit-ish> as "upstream" from the new branch'
complete -f -c git -n '__fish_git_using_command worktree' -n '__fish_seen_subcommand_from add' -l no-track -d 'Don\'t mark <commit-ish> as "upstream" from the new branch'
complete -f -c git -n '__fish_git_using_command worktree' -n '__fish_seen_subcommand_from add' -l lock -d 'Lock working tree after creation'
complete -f -c git -n '__fish_git_using_command worktree' -n '__fish_seen_subcommand_from add' -s q -l quiet -d 'Suppress feedback messages'
complete -f -c git -n '__fish_git_using_command worktree' -n '__fish_seen_subcommand_from list' -l porcelain -d 'Output in an easy-to-parse format for scripts'
complete -f -c git -n '__fish_git_using_command worktree' -n '__fish_seen_subcommand_from lock' -a '(__fish_git_complete_worktrees)' -d Worktree
complete -x -c git -n '__fish_git_using_command worktree' -n '__fish_seen_subcommand_from lock' -l reason -d 'An explanation why the working tree is locked'
complete -x -c git -n '__fish_git_using_command worktree' -n '__fish_seen_subcommand_from move' -n 'not __fish_any_arg_in (__fish_git_complete_worktrees)' -a '(__fish_git_complete_worktrees)' -d Worktree
complete -x -c git -n '__fish_git_using_command worktree' -n '__fish_seen_subcommand_from move' -n '__fish_any_arg_in (__fish_git_complete_worktrees)' -a '(__fish_complete_directories)'
complete -f -c git -n '__fish_git_using_command worktree' -n '__fish_seen_subcommand_from prune' -s n -l dry-run -d 'Do not remove anything'
complete -f -c git -n '__fish_git_using_command worktree' -n '__fish_seen_subcommand_from prune' -s v -l verbose -d 'Report all removals'
complete -x -c git -n '__fish_git_using_command worktree' -n '__fish_seen_subcommand_from prune' -l expire -d 'Only expire unused working trees older than <time>'
complete -f -c git -n '__fish_git_using_command worktree' -n '__fish_seen_subcommand_from remove' -a '(__fish_git_complete_worktrees)' -d Worktree
complete -f -c git -n '__fish_git_using_command worktree' -n '__fish_seen_subcommand_from unlock' -a '(__fish_git_complete_worktrees)' -d Worktree

### stash
complete -c git -n __fish_git_needs_command -a stash -d 'Stash away changes'
complete -f -c git -n '__fish_git_using_command stash' -n __fish_git_stash_not_using_subcommand -a list -d 'List stashes'
complete -f -c git -n '__fish_git_using_command stash' -n __fish_git_stash_not_using_subcommand -a show -d 'Show the changes recorded in the stash'
complete -f -c git -n '__fish_git_using_command stash' -n __fish_git_stash_not_using_subcommand -a pop -d 'Apply and remove a single stashed state'
complete -f -c git -n '__fish_git_using_command stash' -n __fish_git_stash_not_using_subcommand -a apply -d 'Apply a single stashed state'
complete -f -c git -n '__fish_git_using_command stash' -n __fish_git_stash_not_using_subcommand -a clear -d 'Remove all stashed states'
complete -f -c git -n '__fish_git_using_command stash' -n __fish_git_stash_not_using_subcommand -a drop -d 'Remove a single stashed state from the stash list'
complete -f -c git -n '__fish_git_using_command stash' -n __fish_git_stash_not_using_subcommand -a create -d 'Create a stash'
complete -f -c git -n '__fish_git_using_command stash' -n __fish_git_stash_not_using_subcommand -a save -d 'Save a new stash'
complete -f -c git -n '__fish_git_using_command stash' -n __fish_git_stash_not_using_subcommand -a branch -d 'Create a new branch from a stash'
complete -f -c git -n '__fish_git_using_command stash' -n __fish_git_stash_not_using_subcommand -a push -d 'Create a new stash with given files'

complete -f -c git -n '__fish_git_using_command stash' -n '__fish_git_stash_using_command apply' -ka '(__fish_git_complete_stashes)'
complete -f -c git -n '__fish_git_using_command stash' -n '__fish_git_stash_using_command branch' -ka '(__fish_git_complete_stashes)'
complete -f -c git -n '__fish_git_using_command stash' -n '__fish_git_stash_using_command drop' -ka '(__fish_git_complete_stashes)'
complete -f -c git -n '__fish_git_using_command stash' -n '__fish_git_stash_using_command pop' -ka '(__fish_git_complete_stashes)'
complete -f -c git -n '__fish_git_using_command stash' -n '__fish_git_stash_using_command show' -ka '(__fish_git_complete_stashes)'

complete -f -c git -n '__fish_git_using_command stash' -n '__fish_git_stash_using_command push' -a '(__fish_git_files modified deleted modified-staged-deleted)'
complete -f -c git -n '__fish_git_using_command stash' -n '__fish_git_stash_using_command push' -s a -l all -d 'Stash ignored and untracked files'
complete -f -c git -n '__fish_git_using_command stash' -n '__fish_git_stash_using_command push' -s k -l keep-index -d 'Keep changes in index intact'
complete -f -c git -n '__fish_git_using_command stash' -n '__fish_git_stash_using_command push' -s p -l patch -d 'Interactively select hunks'
complete -f -c git -n '__fish_git_using_command stash' -n '__fish_git_stash_using_command push' -s m -l message -d 'Add a description'
complete -f -c git -n '__fish_git_using_command stash' -n '__fish_git_stash_using_command push' -l no-keep-index -d 'Don\'t keep changes in index intact'
complete -f -c git -n '__fish_git_using_command stash' -n '__fish_git_stash_using_command push' -s S -l staged -d 'Stash only staged changes'
complete -f -c git -n '__fish_git_using_command stash' -n '__fish_git_stash_using_command push' -s u -l include-untracked -d 'Stash untracked files'

### config
complete -f -c git -n __fish_git_needs_command -a config -d 'Set and read git configuration variables'
# TODO options

### format-patch
complete -f -c git -n __fish_git_needs_command -a format-patch -d 'Generate patch series to send upstream'
complete -f -c git -n '__fish_git_using_command format-patch' -ka '(__fish_git_branches)'
complete -c git -n '__fish_git_using_command format-patch' -s o -l output-directory -xa '(__fish_complete_directories)'
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
complete -f -c git -n '__fish_git_using_command format-patch log' -l find-copies-harder -d "Also inspect unmodified files as source for a copy"
complete -f -c git -n '__fish_git_using_command format-patch' -l text -s a -d "Treat all files as text"
complete -f -c git -n '__fish_git_using_command format-patch log' -l ignore-space-at-eol -d "Ignore changes in whitespace at EOL"
complete -f -c git -n '__fish_git_using_command format-patch log' -l ignore-space-change -s b -d "Ignore changes in amount of whitespace"
complete -f -c git -n '__fish_git_using_command format-patch log' -l ignore-all-space -s w -d "Ignore whitespace when comparing lines"
complete -f -c git -n '__fish_git_using_command format-patch log' -l ignore-blank-lines -d "Ignore changes whose lines are all blank"
complete -f -c git -n '__fish_git_using_command format-patch log' -l function-context -s W -d "Show whole surrounding functions of changes"
complete -f -c git -n '__fish_git_using_command format-patch log' -l ext-diff -d "Allow an external diff helper to be executed"
complete -f -c git -n '__fish_git_using_command format-patch log' -l no-ext-diff -d "Disallow external diff helpers"
complete -f -c git -n '__fish_git_using_command format-patch log' -l no-textconv -d "Disallow external text conversion filters for binary files (Default)"
complete -f -c git -n '__fish_git_using_command format-patch log' -l textconv -d "Allow external filters for binary files (Resulting diff is unappliable)"
complete -f -c git -n '__fish_git_using_command format-patch log' -l no-prefix -d "Do not show source or destination prefix"
complete -f -c git -n '__fish_git_using_command format-patch' -l numbered -s n -d "Name output in [Patch n/m] format, even with a single patch"
complete -f -c git -n '__fish_git_using_command format-patch' -l no-numbered -s N -d "Name output in [Patch] format, even with multiple patches"

## git submodule
set -l submodulecommands add status init deinit update set-branch set-url summary foreach sync absorbgitdirs
complete -f -c git -n __fish_git_needs_command -a submodule -d 'Initialize, update or inspect submodules'
complete -f -c git -n "__fish_git_using_command submodule" -n "not __fish_seen_subcommand_from $submodulecommands" -a add -d 'Add a submodule'
complete -f -c git -n "__fish_git_using_command submodule" -n "not __fish_seen_subcommand_from $submodulecommands" -a status -d 'Show submodule status'
complete -f -c git -n "__fish_git_using_command submodule" -n "not __fish_seen_subcommand_from $submodulecommands" -a init -d 'Initialize all submodules'
complete -f -c git -n "__fish_git_using_command submodule" -n "not __fish_seen_subcommand_from $submodulecommands" -a deinit -d 'Unregister the given submodules'
complete -f -c git -n "__fish_git_using_command submodule" -n "not __fish_seen_subcommand_from $submodulecommands" -a update -d 'Update all submodules'
complete -x -c git -n "__fish_git_using_command submodule" -n "not __fish_seen_subcommand_from $submodulecommands" -a set-branch -d 'Set the default remote tracking branch'
complete -f -c git -n "__fish_git_using_command submodule" -n "not __fish_seen_subcommand_from $submodulecommands" -a set-url -d 'Sets the URL of the specified submodule'
complete -f -c git -n "__fish_git_using_command submodule" -n "not __fish_seen_subcommand_from $submodulecommands" -a summary -d 'Show commit summary'
complete -f -c git -n "__fish_git_using_command submodule" -n "not __fish_seen_subcommand_from $submodulecommands" -a foreach -d 'Run command on each submodule'
complete -f -c git -n "__fish_git_using_command submodule" -n "not __fish_seen_subcommand_from $submodulecommands" -a sync -d 'Sync submodules\' URL with .gitmodules'
complete -f -c git -n "__fish_git_using_command submodule" -n "not __fish_seen_subcommand_from $submodulecommands" -a absorbgitdirs -d 'Move submodule\'s git directory to current .git/module directory'
complete -f -c git -n "__fish_git_using_command submodule" -n "not __fish_seen_subcommand_from $submodulecommands" -s q -l quiet -d "Only print error messages"
complete -f -c git -n '__fish_git_using_command submodule' -n '__fish_seen_subcommand_from update' -l init -d "Initialize all submodules"
complete -f -c git -n '__fish_git_using_command submodule' -n '__fish_seen_subcommand_from update' -l checkout -d "Checkout the superproject's commit on a detached HEAD in the submodule"
complete -f -c git -n '__fish_git_using_command submodule' -n '__fish_seen_subcommand_from update' -l merge -d "Merge the superproject's commit into the current branch of the submodule"
complete -f -c git -n '__fish_git_using_command submodule' -n '__fish_seen_subcommand_from update' -l rebase -d "Rebase current branch onto the superproject's commit"
complete -f -c git -n '__fish_git_using_command submodule' -n '__fish_seen_subcommand_from update' -s N -l no-fetch -d "Don't fetch new objects from the remote"
complete -f -c git -n '__fish_git_using_command submodule' -n '__fish_seen_subcommand_from update' -l remote -d "Instead of using superproject's SHA-1, use the state of the submodule's remote-tracking branch"
complete -f -c git -n '__fish_git_using_command submodule' -n '__fish_seen_subcommand_from update' -l force -d "Discard local changes when switching to a different commit & always run checkout"
complete -f -c git -n '__fish_git_using_command submodule' -n '__fish_seen_subcommand_from update' -l filter -ra '(__fish_git_filters)' -d 'Request a subset of objects from server'
complete -f -c git -n '__fish_git_using_command submodule' -n '__fish_seen_subcommand_from add' -l force -d "Also add ignored submodule path"
complete -f -c git -n '__fish_git_using_command submodule' -n '__fish_seen_subcommand_from deinit' -l force -d "Remove even with local changes"
complete -f -c git -n '__fish_git_using_command submodule' -n '__fish_seen_subcommand_from deinit' -l all -d "Remove all submodules"
complete -f -c git -n '__fish_git_using_command submodule' -n '__fish_seen_subcommand_from deinit' -n 'not contains -- -- (commandline -xpc)' -a '(__fish_git_submodules)' -d Submodule
complete -f -c git -n '__fish_git_using_command submodule' -n '__fish_seen_subcommand_from set-branch' -s b -l branch -d "Specify the branch to use"
complete -f -c git -n '__fish_git_using_command submodule' -n '__fish_seen_subcommand_from set-branch' -s d -l default -d "Use default branch of the submodule"
complete -f -c git -n '__fish_git_using_command submodule' -n '__fish_seen_subcommand_from status summary' -l cached -d "Use the commit stored in the index"
complete -f -c git -n '__fish_git_using_command submodule' -n '__fish_seen_subcommand_from status' -n 'not contains -- -- (commandline -xpc)' -a '(__fish_git_submodules)' -d Submodule
complete -f -c git -n '__fish_git_using_command submodule' -n '__fish_seen_subcommand_from summary' -l files -d "Compare the commit in the index with submodule HEAD"
complete -f -c git -n '__fish_git_using_command submodule' -n '__fish_seen_subcommand_from foreach update status' -l recursive -d "Traverse submodules recursively"
complete -f -c git -n '__fish_git_using_command submodule' -n '__fish_seen_subcommand_from foreach' -a "(__fish_complete_subcommand --fcs-skip=3)"

## git whatchanged
complete -f -c git -n __fish_git_needs_command -a whatchanged -d 'Show logs with difference each commit introduces'

## Aliases (custom user-defined commands)
complete -c git -n __fish_git_needs_command -a '(__fish_git_aliases)'

### git clean
complete -f -c git -n __fish_git_needs_command -a clean -d 'Remove untracked files from the working tree'
complete -f -c git -n '__fish_git_using_command clean' -s f -l force -d 'Force run'
complete -f -c git -n '__fish_git_using_command clean' -s i -l interactive -d 'Show what would be done and clean files interactively'
complete -f -c git -n '__fish_git_using_command clean' -s n -l dry-run -d 'Don\'t actually remove anything, just show what would be done'
complete -f -c git -n '__fish_git_using_command clean' -s q -l quiet -d 'Be quiet, only report errors'
complete -f -c git -n '__fish_git_using_command clean' -s d -d 'Remove untracked directories in addition to untracked files'
complete -f -c git -n '__fish_git_using_command clean' -s x -d 'Remove ignored files, as well'
complete -f -c git -n '__fish_git_using_command clean' -s X -d 'Remove only ignored files'
# TODO -e option

### git blame
complete -f -c git -n __fish_git_needs_command -a blame -d 'Show what last modified each line of a file'
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
complete -f -c git -n '__fish_git_using_command blame' -s C -d 'Detect lines moved or copied from other files modified in the same commit'
complete -f -c git -n '__fish_git_using_command blame' -s h -d 'Show help message'
complete -f -c git -n '__fish_git_using_command blame' -s c -d 'Use the same output mode as git-annotate'
complete -f -c git -n '__fish_git_using_command blame' -s f -l show-name -d 'Show the filename in the original commit'
complete -f -c git -n '__fish_git_using_command blame' -s n -l show-number -d 'Show the line number in the original commit'
complete -f -c git -n '__fish_git_using_command blame' -s s -d 'Suppress the author name and timestamp from the output'
complete -f -c git -n '__fish_git_using_command blame' -s e -l show-email -d 'Show the author email instead of author name'
complete -f -c git -n '__fish_git_using_command blame' -s w -d 'Ignore whitespace changes'

### help
complete -f -c git -n __fish_git_needs_command -a help -d 'Display help information about Git'
complete -f -c git -n '__fish_git_using_command help' -a '(__fish_git_help_all_concepts)'
complete -f -c git -n '__fish_git_using_command help' -a add -d 'Add file contents to the index'
complete -f -c git -n '__fish_git_using_command help' -a am -d 'Apply a series of patches from a mailbox'
complete -f -c git -n '__fish_git_using_command help' -a apply -d 'Apply a patch on a git index file and a working tree'
complete -f -c git -n '__fish_git_using_command help' -a archive -d 'Create an archive of files from a named tree'
complete -f -c git -n '__fish_git_using_command help' -a bisect -d 'Find the change that introduced a bug by binary search'
complete -f -c git -n '__fish_git_using_command help' -a blame -d 'Show what revision and author last modified each line of a file'
complete -f -c git -n '__fish_git_using_command help' -a branch -d 'List, create, or delete branches'
complete -f -c git -n '__fish_git_using_command help' -a checkout -d 'Checkout and switch to a branch'
complete -f -c git -n '__fish_git_using_command help' -a cherry-pick -d 'Apply the change introduced by an existing commit'
complete -f -c git -n '__fish_git_using_command help' -a clean -d 'Remove untracked files from the working tree'
complete -f -c git -n '__fish_git_using_command help' -a clone -d 'Clone a repository into a new directory'
complete -f -c git -n '__fish_git_using_command help' -a commit -d 'Record changes to the repository'
complete -f -c git -n '__fish_git_using_command help' -a config -d 'Set and read git configuration variables'
complete -f -c git -n '__fish_git_using_command help' -a count-objects -d 'Count unpacked number of objects and their disk consumption'
complete -f -c git -n '__fish_git_using_command help' -a describe -d 'Give an object a human-readable name'
complete -f -c git -n '__fish_git_using_command help' -a diff -d 'Show changes between commits, commit and working tree, etc'
complete -f -c git -n '__fish_git_using_command help' -a daemon -d 'A really simple server for Git repositories'
complete -f -c git -n '__fish_git_using_command help' -a difftool -d 'Open diffs in a visual tool'
complete -f -c git -n '__fish_git_using_command help' -a fetch -d 'Download objects and refs from another repository'
complete -f -c git -n '__fish_git_using_command help' -a filter-branch -d 'Rewrite branches'
complete -f -c git -n '__fish_git_using_command help' -a format-patch -d 'Generate patch series to send upstream'
complete -f -c git -n '__fish_git_using_command help' -a gc -d 'Cleanup unnecessary files and optimize the local repository'
complete -f -c git -n '__fish_git_using_command help' -a grep -d 'Print lines matching a pattern'
complete -f -c git -n '__fish_git_using_command help' -a init -d 'Create an empty git repository or reinitialize an existing one'
complete -f -c git -n '__fish_git_using_command help' -a log -d 'Show commit logs'
complete -f -c git -n '__fish_git_using_command help' -a ls-files -d 'Show information about files in the index and the working tree'
complete -f -c git -n '__fish_git_using_command help' -a mailinfo -d 'Extracts patch and authorship from a single e-mail message'
complete -f -c git -n '__fish_git_using_command help' -a mailsplit -d 'Simple UNIX mbox splitter program'
complete -f -c git -n '__fish_git_using_command help' -a maintenance -d 'Run tasks to optimize Git repository data'
complete -f -c git -n '__fish_git_using_command help' -a merge -d 'Join two or more development histories together'
complete -f -c git -n '__fish_git_using_command help' -a merge-base -d 'Find as good common ancestors as possible for a merge'
complete -f -c git -n '__fish_git_using_command help' -a mergetool -d 'Run merge conflict resolution tools to resolve merge conflicts'
complete -f -c git -n '__fish_git_using_command help' -a mv -d 'Move or rename a file, a directory, or a symlink'
complete -f -c git -n '__fish_git_using_command help' -a notes -d 'Add or inspect object notes'
complete -f -c git -n '__fish_git_using_command help' -a prune -d 'Prune all unreachable objects from the object database'
complete -f -c git -n '__fish_git_using_command help' -a pull -d 'Fetch from and merge with another repository or a local branch'
complete -f -c git -n '__fish_git_using_command help' -a push -d 'Update remote refs along with associated objects'
complete -f -c git -n '__fish_git_using_command help' -a range-diff -d 'Compare two commit ranges (e.g. two versions of a branch)'
complete -f -c git -n '__fish_git_using_command help' -a rebase -d 'Forward-port local commits to the updated upstream head'
complete -f -c git -n '__fish_git_using_command help' -a reflog -d 'Manage reflog information'
complete -f -c git -n '__fish_git_using_command help' -a remote -d 'Manage set of tracked repositories'
complete -f -c git -n '__fish_git_using_command help' -a reset -d 'Reset current HEAD to the specified state'
complete -f -c git -n '__fish_git_using_command help' -a restore -d 'Restore working tree files'
complete -f -c git -n '__fish_git_using_command help' -a revert -d 'Revert an existing commit'
complete -f -c git -n '__fish_git_using_command help' -a rev-parse -d 'Pick out and massage parameters'
complete -f -c git -n '__fish_git_using_command help' -a rm -d 'Remove files from the working tree and from the index'
complete -f -c git -n '__fish_git_using_command help' -a show -d 'Shows the last commit of a branch'
complete -f -c git -n '__fish_git_using_command help' -a show-branch -d 'Shows the commits on branches'
complete -f -c git -n '__fish_git_using_command help' -a stash -d 'Stash away changes'
complete -f -c git -n '__fish_git_using_command help' -a status -d 'Show the working tree status'
complete -f -c git -n '__fish_git_using_command help' -a submodule -d 'Initialize, update or inspect submodules'
complete -f -c git -n '__fish_git_using_command help' -a stripspace -d 'Remove unnecessary whitespace'
complete -f -c git -n '__fish_git_using_command help' -a switch -d 'Switch to a branch'
complete -f -c git -n '__fish_git_using_command help' -a tag -d 'Create, list, delete or verify a tag object signed with GPG'
complete -f -c git -n '__fish_git_using_command help' -a update-index -d 'Register file contents in the working tree to the index'
complete -f -c git -n '__fish_git_using_command help' -a whatchanged -d 'Show logs with difference each commit introduces'
complete -f -c git -n '__fish_git_using_command help' -a worktree -d 'Manage multiple working trees'

# Complete both options and possible parameters to `git config`
complete -f -c git -n '__fish_git_using_command config' -l global -d 'Get/set global configuration'
complete -f -c git -n '__fish_git_using_command config' -l system -d 'Get/set system configuration'
complete -f -c git -n '__fish_git_using_command config' -l local -d 'Get/set local repo configuration'
complete -F -c git -n '__fish_git_using_command config' -s f -l file -d 'Read config from file' -r
complete -F -c git -n '__fish_git_using_command config' -l blob -d 'Read config from blob' -r

# For config options that have the user select one from a set, this function completes possible options
function __fish_git_complete_key_values
    set -l config_key (__fish_nth_token 2)

    switch $config_key
        case diff.algorithm
            printf "%s\n" myers patience histogram minimal
        case init.defaultBranch
            printf "%s\n" master main trunk dev next
        case '*'
            __fish_complete_path
    end
end

# If no argument is specified, it's as if --get was used
# Use -k with `__fish_git_config_keys` so that user defined values are shown first
complete -c git -n '__fish_git_using_command config' -n '__fish_is_nth_token 2' -kfa '(__fish_git_config_keys)'
complete -c git -n '__fish_git_using_command config' -n '__fish_is_nth_token 3' -fa '(__fish_git_complete_key_values)'
complete -f -c git -n '__fish_git_using_command config' -l get -d 'Get config with name' -kra '(__fish_git_config_keys)'
complete -f -c git -n '__fish_git_using_command config' -l get-all -d 'Get all values matching key' -ka '(__fish_git_config_keys)'
complete -f -c git -n '__fish_git_using_command config' -l get-urlmatch -d 'Get value specific for the section url' -r
complete -f -c git -n '__fish_git_using_command config' -l replace-all -d 'Replace all matching variables' -kra '(__fish_git_config_keys)'
complete -f -c git -n '__fish_git_using_command config' -l add -d 'Add a new variable' -r
complete -f -c git -n '__fish_git_using_command config' -l unset -d 'Remove a variable' -ka '(__fish_git_config_keys)'
complete -f -c git -n '__fish_git_using_command config' -l unset-all -d 'Remove matching variables' -ka '(__fish_git_config_keys)'
complete -f -c git -n '__fish_git_using_command config' -l rename-section -d 'Rename section' -r
complete -f -c git -n '__fish_git_using_command config' -s l -l list -d 'List all variables'
complete -f -c git -n '__fish_git_using_command config' -s e -l edit -d 'Open configuration in an editor'

complete -f -c git -n '__fish_git_using_command config' -s t -l type -d 'Value is of given type'
complete -f -c git -n '__fish_git_using_command config' -l bool -d 'Value is \'true\' or \'false\''
complete -f -c git -n '__fish_git_using_command config' -l int -d 'Value is a decimal number'
complete -f -c git -n '__fish_git_using_command config' -l bool-or-int -d 'Value is --bool or --int'
complete -f -c git -n '__fish_git_using_command config' -l path -d 'Value is a path'
complete -f -c git -n '__fish_git_using_command config' -l expiry-date -d 'Value is an expiry date'

complete -f -c git -n '__fish_git_using_command config' -s z -l null -d 'Terminate values with NUL byte'
complete -f -c git -n '__fish_git_using_command config' -l name-only -d 'Show variable names only'
complete -f -c git -n '__fish_git_using_command config' -l includes -d 'Respect include directives'
complete -f -c git -n '__fish_git_using_command config' -l show-origin -d 'Show origin of configuration'
complete -f -c git -n '__fish_git_using_command config' -n '__fish_seen_argument get' -l default -d 'Use default value when missing entry'

### for-each-ref
complete -f -c git -n __fish_git_needs_command -a for-each-ref -d "Format and output info on each ref"
complete -f -c git -n '__fish_git_using_command for-each-ref' -l count -d "Limit to n results"
# Any one of --shell, --perl, --python, or --tcl
set -l for_each_ref_interpreters shell perl python tcl
for intr in $for_each_ref_interpreters
    complete -f -c git -n '__fish_git_using_command for-each-ref' \
        -n "not __fish_seen_argument --$for_each_ref_interpreters" \
        -l $intr -d "%(fieldname) placeholders are $intr scripts"
end
complete -f -c git -n '__fish_git_using_command for-each-ref' -x -l format -d "Format string with %(fieldname) placeholders"
complete -f -c git -n '__fish_git_using_command for-each-ref' -f -l color -d "When to color" -a "always never auto"
complete -f -c git -n '__fish_git_using_command for-each-ref' -l points-at -d "Only list refs pointing at object" -ka '(__fish_git_commits)'
complete -f -c git -n '__fish_git_using_command for-each-ref' -x -l merged -d "Only list refs reachable from specified commit" -ka '(__fish_git_commits)'
complete -f -c git -n '__fish_git_using_command for-each-ref' -x -l no-merged -d "Only list refs not reachable from specified commit" -ka '(__fish_git_commits)'
complete -f -c git -n '__fish_git_using_command for-each-ref' -x -l contains -d "Only list refs containing the specified commit" -ka '(__fish_git_commits)'
complete -f -c git -n '__fish_git_using_command for-each-ref' -x -l no-contains -d "Only list refs not containing the specified commit" -ka '(__fish_git_commits)'
complete -f -c git -n '__fish_git_using_command for-each-ref' -x -l ignore-case -d "Sorting and filtering refs are case insensitive"

### send-email
complete -f -c git -n __fish_git_needs_command -a send-email -d 'Send a collection of patches as emails'
complete -f -c git -n '__fish_git_using_command send-email' -l annotate -d 'Edit patches before sending'
complete -f -c git -n '__fish_git_using_command send-email' -l bcc -r -d 'Specify Bcc: value'
complete -f -c git -n '__fish_git_using_command send-email' -l cc -r -d 'Specify Cc: value'
complete -f -c git -n '__fish_git_using_command send-email' -l compose -d 'Add an introductory message'
complete -f -c git -n '__fish_git_using_command send-email' -l from -r -d Sender
complete -f -c git -n '__fish_git_using_command send-email' -l reply-to -r -d 'Address for replies'
complete -f -c git -n '__fish_git_using_command send-email' -l in-reply-to -r -d 'Reply in thread to given Message-ID'
complete -f -c git -n '__fish_git_using_command send-email' -l subject -r -d Subject
complete -f -c git -n '__fish_git_using_command send-email' -l to -r -d Recipient
complete -f -c git -n '__fish_git_using_command send-email' -l 8bit-encoding -ra UTF-8 -d 'Encoding for non-ASCII messages'
complete -f -c git -n '__fish_git_using_command send-email' -l compose-encoding -ra UTF-8 -d 'Encoding for the compose message'
complete -f -c git -n '__fish_git_using_command send-email' -l transfer-encoding -ra '7bit 8bit quoted-printable base64 auto' -d 'Encoding for transferring over SMTP'
complete -f -c git -n '__fish_git_using_command send-email' -l xmailer -d 'Add X-Mailer: header'
complete -f -c git -n '__fish_git_using_command send-email' -l no-xmailer -d 'Suppress X-Mailer: header'
complete -f -c git -n '__fish_git_using_command send-email' -l envelope-sender -r
complete -f -c git -n '__fish_git_using_command send-email' -l sendmail-cmd -ra '(__fish_complete_command)' -d 'Command to send email'
complete -f -c git -n '__fish_git_using_command send-email' -l smtp-encryption -ra 'ssl tls'
complete -f -c git -n '__fish_git_using_command send-email' -l smtp-domain -r
complete -f -c git -n '__fish_git_using_command send-email' -l smtp-auth -r -d 'Restrict auth mechanisms'
complete -f -c git -n '__fish_git_using_command send-email' -l smtp-pass -r
complete -f -c git -n '__fish_git_using_command send-email' -l no-smtp-auth
complete -f -c git -n '__fish_git_using_command send-email' -l smtp-server -r
complete -f -c git -n '__fish_git_using_command send-email' -l smtp-server-port -r
complete -f -c git -n '__fish_git_using_command send-email' -l smtp-server-option -r
complete -c git -n '__fish_git_using_command send-email' -l smtp-ssl-cert-path -r
complete -f -c git -n '__fish_git_using_command send-email' -l smtp-user -r
complete -f -c git -n '__fish_git_using_command send-email' -l smt-debug -ra '0 1' -d 'SMTP debug output'
complete -f -c git -n '__fish_git_using_command send-email' -l batch-size -r -d 'Reconnect after sending this many messages'
complete -f -c git -n '__fish_git_using_command send-email' -l relogin-dleay -r -d 'Seconds to wait before reconnecting'
complete -f -c git -n '__fish_git_using_command send-email' -l no-to -d 'Clear To:'
complete -f -c git -n '__fish_git_using_command send-email' -l no-cc -d 'Clear Cc:'
complete -f -c git -n '__fish_git_using_command send-email' -l no-bcc -d 'Clear Bcc:'
complete -f -c git -n '__fish_git_using_command send-email' -l no-identity
complete -f -c git -n '__fish_git_using_command send-email' -l to-cmd -ra '(__fish_complete_command)' -d 'Command to generate To: lines'
complete -f -c git -n '__fish_git_using_command send-email' -l cc-cmd -ra '(__fish_complete_command)' -d 'Command to generate Cc: lines'
complete -f -c git -n '__fish_git_using_command send-email' -l header-cmd -ra '(__fish_complete_command)' -d 'Command to generate header lines'
complete -f -c git -n '__fish_git_using_command send-email' -l no-header-cmd -d 'Disable any header command'
complete -f -c git -n '__fish_git_using_command send-email' -l chain-reply-to -d 'Each email is sent as reply to the previous one'
complete -f -c git -n '__fish_git_using_command send-email' -l no-chain-reply-to -d 'Each email is sent as reply to the first one'
complete -f -c git -n '__fish_git_using_command send-email' -l identity -r -d 'Configuration identity'
complete -f -c git -n '__fish_git_using_command send-email' -l signed-off-by-cc -d 'Add addresses in Signed-off-by trailers to Cc'
complete -f -c git -n '__fish_git_using_command send-email' -l no-signed-off-by-cc -d 'Do not add Signed-off-by trailers to Cc'
complete -f -c git -n '__fish_git_using_command send-email' -l cc-cover -d 'Copy Cc: list from first message to the others'
complete -f -c git -n '__fish_git_using_command send-email' -l no-cc-cover -d 'Do not copy Cc: list from first message'
complete -f -c git -n '__fish_git_using_command send-email' -l to-cover -d 'Copy To: list from first message to the others'
complete -f -c git -n '__fish_git_using_command send-email' -l no-to-cover -d 'Do not copy To: list from first message'
complete -f -c git -n '__fish_git_using_command send-email' -l suppress-cc -ra '
    author\t"Do not auto-cc patch author"
    self\t"Do not auto-cc sender"
    cc\t"Do not auto-cc address from Cc lines in patch header"
    bodycc\t"Do not auto-cc addresses from Cc lines in patch body"
    sob\t"Do not auto-cc address from Signed-off-by trailers"
    misc-by\t"Do not auto-cc addresses from commit trailers"
    cccmd\t"Do not run --cc-cmd"
    body\t"equivalent to sob + bodycc + misc-by"
    all\t"Do not auto-cc anyone"
' -d 'Suppress auto-cc of recipient categories'
complete -f -c git -n '__fish_git_using_command send-email' -l suppress-from -d 'Do not cc the From: address'
complete -f -c git -n '__fish_git_using_command send-email' -l no-suppress-from -d 'Do cc the From: address'
complete -f -c git -n '__fish_git_using_command send-email' -l thread -d 'Create an email thread'
complete -f -c git -n '__fish_git_using_command send-email' -l no-thread -d 'Do not create an email thread'
complete -f -c git -n '__fish_git_using_command send-email' -l confirm -ra '
    always\t"Always confirm before sending"
    never\t"Never confirm before sending"
    cc\t"Confirm before sending if CC lines have been added"
    compose\t"Confirm before sending if using --compose"
    auto\t"Equivalent to cc + compose"
' -d 'Confirm just before sending'
complete -f -c git -n '__fish_git_using_command send-email' -l dry-run -d 'Do everything except actually send the emails'
complete -f -c git -n '__fish_git_using_command send-email' -l format-patch -d 'Ambiguous arguments are for format-patch, not filenames'
complete -f -c git -n '__fish_git_using_command send-email' -l no-format-patch -d 'Interpret ambiguous arguments as filenames, not format-patch arguments'
complete -f -c git -n '__fish_git_using_command send-email' -l quiet -d 'Print only one line per email'
complete -f -c git -n '__fish_git_using_command send-email' -l validate -d 'Perform sanity checks'
complete -f -c git -n '__fish_git_using_command send-email' -l no-validate -d 'Skip sanity checks'
complete -f -c git -n '__fish_git_using_command send-email' -l force -d 'Ignore safety checks'
complete -f -c git -n '__fish_git_using_command send-email' -l dump-aliases -d 'Dump shorthand alias names'

### subcommands supporting --sort (XXX: list may not be complete!)
set -l sortcommands branch for-each-ref tag
# A list of keys one could reasonably sort refs by. This isn't the list of all keys that
# can be used as any git internal key for a ref may be used here, sorted by binary value.
function __fish_git_sort_keys
    echo -objectsize\tSize of branch or commit
    echo -authordate\tWhen the latest commit was actually made
    echo -committerdate\tWhen the branch was last committed or rebased
    echo -creatordate\tWhen the latest commit or tag was created
    echo creator\tThe name of the commit author
    echo objectname\tThe complete SHA1
    echo objectname:short\tThe shortest non-ambiguous SHA1
    echo refname\tThe complete, unambiguous git ref name
    echo refname:short\tThe shortest non-ambiguous ref name
    echo author\tThe name of the author of the latest commit
    echo committer\tThe name of the person who committed latest
    echo tagger\tThe name of the person who created the tag
    echo authoremail\tThe email of the author of the latest commit
    echo committeremail\tThe email of the person who committed last
    echo taggeremail\tThe email of the person who created the tag
end
complete -f -c git -n "__fish_seen_subcommand_from $sortcommands" -l sort -d 'Sort results by' -a "(__fish_git_sort_keys)"

## Custom commands (git-* commands installed in the PATH)
complete -c git -n __fish_git_needs_command -a '(__fish_git_custom_commands)' -d 'Custom command'

function __fish_git_complete_custom_command -a subcommand
    set -l cmd (commandline -xpc)
    set -e cmd[1] # Drop "git".
    set -l subcommand_args
    if argparse -s (__fish_git_global_optspecs) -- $cmd
        set subcommand_args (string escape -- $argv[2..]) # Drop the subcommand.
    end
    complete -C "git-$subcommand $subcommand_args "(commandline -ct)
end

# source git-* commands' autocompletion file if exists
set -l __fish_git_custom_commands_completion
for file in (path filter -xZ $PATH/git-* | path basename)
    # Already seen this command earlier in $PATH.
    contains -- $file $__fish_git_custom_commands_completion
    and continue

    # Running `git foo` ends up running `git-foo`, so we need to ignore the `git-` here.
    set -l cmd (string replace -r '^git-' '' -- $file | string escape)
    complete -c git -f -n "__fish_git_using_command $cmd" -a "(__fish_git_complete_custom_command $cmd)"
    set -a __fish_git_custom_commands_completion $file
end
