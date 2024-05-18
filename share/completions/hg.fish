# fish completion for hg

# Mercurial has a global switch to specify the path to the repository on which
# to run the hg command (-R or --repository).  If that is on the commandline,
# this function echoes the given path and returns 0.  Otherwise, it returns 1.
function __fish_hg_get_repo
    set -l cmdline (commandline -p)
    if set -l match (string match -r -- "(-R|--repository) +([^ ]+)" $cmdline)
        echo $match[3]
        return 0
    else
        return 1
    end
end

# Mercurial also has a global switch to specify a directory to which to switch
# before running the command (--cwd).  If it is on the commandline, this
# function echoes the given path and returns 0.  Otherwise, it returns 1.
function __fish_hg_get_cwd
    set -l cmdline (commandline -p)
    if set -l match (string match -r -- "--cwd +([^ ]+)" $cmdline)
        echo $match[2]
        return 0
    else
        return 1
    end
end

function __fish_hg
    set -lx HGPLAIN 1
    if set -l repo (__fish_hg_get_repo)
        set argv $argv -R $repo
    end
    if set -l cwd (__fish_hg_get_cwd)
        set argv $argv --cwd $cwd
    end
    command hg $argv 2>/dev/null
end

function __fish_hg_commands
    set -l commands (__fish_hg debugcomplete)
    for command in $commands
        switch $command
            case add
                printf "$command\tadd the specified files on the next commit\n"
            case addremove
                printf "$command\tadd all new files, delete all missing files\n"
            case annotate
                printf "$command\tshow changeset information by line for each file\n"
            case archive
                printf "$command\tcreate an unversioned archive of a repository revision\n"
            case backout
                printf "$command\treverse effect of earlier changeset\n"
            case bisect
                printf "$command\tsubdivision search of changesets\n"
            case bookmarks
                printf "$command\tcreate a new bookmark or list existing bookmarks\n"
            case branch
                printf "$command\tset or show the current branch name\n"
            case branches
                printf "$command\tlist repository named branches\n"
            case bundle
                printf "$command\tcreate a bundle file\n"
            case cat
                printf "$command\toutput the current or given revision of files\n"
            case churn
                printf "$command\thistogram of changes to the repository\n"
            case clone
                printf "$command\tmake a copy of an existing repository\n"
            case commit
                printf "$command\tcommit the specified files or all outstanding changes\n"
            case config
                printf "$command\tshow combined config settings from all hgrc files\n"
            case convert
                printf "$command\tconvert a foreign SCM repository to a Mercurial one\n"
            case copy
                printf "$command\tmark files as copied for the next commit\n"
            case diff
                printf "$command\tdiff repository (or selected files)\n"
            case email
                printf "$command\tsend changesets by email\n"
            case export
                printf "$command\tdump the header and diffs for one or more changesets\n"
            case extdiff
                printf "$command\tuse external program to diff repository (or selected files)\n"
            case files
                printf "$command\tlist tracked files\n"
            case forget
                printf "$command\tforget the specified files on the next commit\n"
            case graft
                printf "$command\tcopy changes from other branches onto the current branch\n"
            case grep
                printf "$command\tsearch revision history for a pattern in specified files\n"
            case heads
                printf "$command\tshow branch heads\n"
            case help
                printf "$command\tshow help for a given topic or a help overview\n"
            case histedit
                printf "$command\tinteractively edit changeset history\n"
            case identify
                printf "$command\tidentify the working directory or specified revision\n"
            case import
                printf "$command\timport an ordered set of patches\n"
            case incoming
                printf "$command\tshow new changesets found in source\n"
            case init
                printf "$command\tcreate a new repository in the given directory\n"
            case locate
                printf "$command\tlocate files matching specific patterns\n"
            case log
                printf "$command\tshow revision history of entire repository or files\n"
            case manifest
                printf "$command\toutput the current or given revision of the project manifest\n"
            case merge
                printf "$command\tmerge another revision into working directory\n"
            case outgoing
                printf "$command\tshow changesets not found in the destination\n"
            case parents
                printf "$command\tshow the parents of the working directory or revision\n"
            case paths
                printf "$command\tshow aliases for remote repositories\n"
            case phase
                printf "$command\tset or show the current phase name\n"
            case pull
                printf "$command\tpull changes from the specified source\n"
            case push
                printf "$command\tpush changes to the specified destination\n"
            case qapplied
                printf "$command\tprint the patches already applied\n"
            case qclone
                printf "$command\tclone main and patch repository at same time\n"
            case qcommit
                # deprecated
            case qdelete
                printf "$command\tremove patches from queue\n"
            case qdiff
                printf "$command\tdiff of the current patch and subsequent modifications\n"
            case qfinish
                printf "$command\tmove applied patches into repository history\n"
            case qfold
                printf "$command\tfold the named patches into the current patch\n"
            case qgoto
                printf "$command\tpush or pop patches until named patch is at top of stack\n"
            case qguard
                printf "$command\t set or print guards for a patch\n"
            case qheader
                printf "$command\tprint the header of the topmost or specified patch\n"
            case qimport
                printf "$command\timport a patch or existing changeset\n"
            case qinit
                # deprecated
            case qnew
                printf "$command\tcreate a new patch\n"
            case qnext
                printf "$command\tprint the name of the next pushable patch\n"
            case qpop
                printf "$command\tpop the current patch off the stack\n"
            case qprev
                printf "$command\tprint the name of the preceding applied patch\n"
            case qpush
                printf "$command\tpush the next patch onto the stack\n"
            case qqueue
                printf "$command\tmanage multiple patch queues\n"
            case qrecord
                printf "$command\tinteractively record a new patch\n"
            case qrefresh
                printf "$command\tupdate the current patch\n"
            case qrename
                printf "$command\trename a patch\n"
            case qrestore
                # deprecated
            case qsave
                # deprecated
            case qselect
                printf "$command\tset or print guarded patches to push\n"
            case qseries
                printf "$command\tprint the entire series file\n"
            case qtop
                printf "$command\tprint the name of the current patch\n"
            case qunapplied
                printf "$command\tprint the patches not yet applied\n"
            case rebase
                printf "$command\tmove changeset (and descendants) to a different branch\n"
            case record
                printf "$command\tinteractively select changes to commit\n"
            case recover
                printf "$command\troll back an interrupted transaction\n"
            case remove
                printf "$command\tremove the specified files on the next commit\n"
            case rename
                printf "$command\trename files; equivalent of copy + remove\n"
            case resolve
                printf "$command\tredo merges or set/view the merge status of files\n"
            case revert
                printf "$command\trestore files to their checkout state\n"
            case rollback
                # deprecated
            case root
                printf "$command\tprint the root (top) of the current working directory\n"
            case serve
                printf "$command\tstart stand-alone webserver\n"
            case shelve
                printf "$command\tsave and set aside changes from the working directory\n"
            case status
                printf "$command\tshow changed files in the working directory\n"
            case strip
                printf "$command\tstrip changesets and all their descendants from the repository\n"
            case summary
                printf "$command\tsummarize working directory state\n"
            case tag
                printf "$command\tadd one or more tags for the current or given revision\n"
            case tags
                printf "$command\tlist repository tags\n"
            case tip
                # deprecated
            case unbundle
                printf "$command\tapply one or more bundle files\n"
            case unshelve
                printf "$command\trestore a shelved change to the working directory\n"
            case update
                printf "$command\tupdate working directory (or switch revisions)\n"
            case verify
                printf "$command\tverify the integrity of the repository\n"
            case version
                printf "$command\toutput version and copyright information\n"
            case view
                printf "$command\tstart interactive history viewer\n"
            case "*"
                printf "$command\n"
        end
    end
end

function __fish_hg_labels
    if set -l labels (__fish_hg debuglabelcomplete)
        printf "%s\tlabel\n" $labels
    else
        __fish_hg_branches
        __fish_hg_bookmarks
        for line in (__fish_hg tags)
            set -l parts (string split " " -m 1 $line)
            printf "%s\ttag\n" $parts[1]
        end
    end
end

function __fish_hg_help_topics
    set -l commands (__fish_hg debugcomplete)
    printf "%s\tcommand\n" $commands
    for line in (__fish_hg help | string match -re '^ [a-zA-Z]')
        set -l parts (string trim $line | string split " " -m 1)
        set -l topic $parts[1]
        if not contains $topic $commands
            printf "%s\thelp topic\n" $topic
        end
    end
end

function __fish_hg_config_entries
    for line in (__fish_hg config)
        set -l parts (string split = -m 1 $line)
        printf "%s\tconfig entry\n" $parts[1]
    end
end

function __fish_hg_patches
    printf "%s\tpatch\n" (__fish_hg qseries)
end

function __fish_hg_patch_queues
    for line in (__fish_hg qqueue)
        set -l parts (string split "(" -m 1 $line)
        set -l queue (string trim $parts[1])
        printf "%s\tpatch queue\n" $queue
    end
end

function __fish_hg_status
    set -l token (commandline -ct)
    __fish_hg status -n $argv "glob:$token**"
end

function __fish_hg_locate
    __fish_hg locate $argv
end

function __fish_hg_bookmarks
    set -l bookmarks (__fish_hg bookmarks)
    if string match -q -- "no bookmarks set" "$bookmarks[1]"
        return
    end
    for line in $bookmarks
        # Bookmarks can contain lots of different characters, including spaces,
        # but they can't contain colons.  So we use that to split the line.
        set -l parts (string sub -s 4 $line | string split ":" -m 1)
        set -l parts (string split " " -r -m 1 $parts[1])
        set -l bookmark (string trim $parts[1])
        printf "%s\tbookmark\n" $bookmark
    end
end

function __fish_hg_branches
    for line in (__fish_hg branches)
        # Like with bookmarks, branches can't contain colons, so we use that for
        # splitting.
        set -l parts (string split ":" -m 1 $line)
        set -l parts (string split " " -r -m 1 $parts[1])
        set -l branch (string trim $parts[1])
        printf "%s\tbranch\n" $branch
    end
end

function __fish_hg_merge_tools
    for tool in internal:dump internal:fail internal:forcedump internal:local internal:merge internal:merge-local internal:merge-other internal:merge3 internal:other internal:prompt internal:tagmerge internal:union
        printf "$tool\tmerge tool\n"
    end
    for line in (__fish_hg config merge-tools)
        set -l parts (string split "." -m 2 $line)
        printf "%s\tmerge tool\n" $parts[2]
    end
end

function __fish_hg_sources
    for line in (__fish_hg paths)
        set -l parts (string split = $line)
        printf "%s\tsource\n" (string trim $parts[1])
    end
end

function __fish_hg_shelves
    printf "%s\tshelve\n" (__fish_hg shelve -ql)
end

function __fish_hg_mq_enabled
    if set -l line (__fish_hg config extensions.hgext.mq)
        set -l parts (string split "=" -m 1 $line)
        not string match -r -q -- "^!" $parts[2]
        return
    else
        return 1
    end
end

function __fish_hg_get_command
    set -l cmdline (commandline -pxc)
    set -e cmdline[1]
    set -l lasttoken ""
    for token in $cmdline
        # if the last token was a switch that takes an argument, we just skip
        # the current token
        if string match -r -q -- "-R|--repository|--cwd|--config|--encoding|--encodingmode|--color|--pager" $lasttoken
            set lasttoken ""
            continue
        end
        # if the current token is a switch of any kind, we can skip it
        if string match -q -- "-*" $token
            set lasttoken $token
            continue
        end
        # if we get to here, then we assume that the token is an hg command
        echo $token
        return 0
    end
    # no hg command was found
    return 1
end

function __fish_hg_using_command --argument-names cmd
    if set -l token (__fish_hg_get_command)
        string match -q -- $token $cmd
        return
    else
        return 1
    end
end

function __fish_hg_needs_command
    not __fish_hg_get_command >/dev/null
end

# global options
complete -c hg -s R -l repository -x -d "repository root directory or name of overlay bundle file"
complete -c hg -l cwd -x -a "(__fish_complete_directories (commandline -ct))" -d "change working directory"
complete -c hg -s y -l noninteractive -d "do not prompt, automatically pick the first choice for all prompts"
complete -c hg -s q -l quiet -d "suppress output"
complete -c hg -s v -l verbose -d "enable additional output"
complete -c hg -l config -x -a "(__fish_hg_config_entries)" -d "set/override config option"
complete -c hg -l debug -d "enable debugging output"
complete -c hg -l debugger -d "start debugger"
complete -c hg -l encoding -x -d "set the charset encoding"
complete -c hg -l encodingmode -x -d "set the charset encoding mode"
complete -c hg -l traceback -d "always print a traceback on exception"
complete -c hg -l time -d "time how long the command takes"
complete -c hg -l profile -d "print command execution profile"
complete -c hg -l version -d "output version information and exit"
complete -c hg -s h -l help -d "display help and exit"
complete -c hg -l hidden -d "consider hidden changesets"
complete -c hg -l color -x -a "true false always auto never debug" -d "when to colorize"
complete -c hg -l pager -x -a "true false always auto never" -d "when to paginate"

# subcommands
complete -c hg -n __fish_hg_needs_command -x -a "(__fish_hg_commands)"

# hg add
complete -c hg -n "__fish_hg_using_command add" -f -a "(__fish_hg_status -u)"
complete -c hg -n "__fish_hg_using_command add" -s I -l include -x -d "include names matching the given patterns"
complete -c hg -n "__fish_hg_using_command add" -s X -l exclude -x -d "exclude names matching the given patterns"
complete -c hg -n "__fish_hg_using_command add" -s S -l subrepos -d "recurse into subrepositories"
complete -c hg -n "__fish_hg_using_command add" -s n -l dry-run -d "do not perform actions, just print output"
complete -c hg -n "__fish_hg_using_command add; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"

# hg addremove
for cmd in addr addre addrem addremo addremov addremove
    complete -c hg -n "__fish_hg_using_command $cmd" -f -a "(__fish_hg_status -ud)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s s -l similarity -x -d "guess renamed files by similarity (0<=s<=100)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s S -l subrepos -d "recurse into subrepositories"
    complete -c hg -n "__fish_hg_using_command $cmd" -s I -l include -x -d "include names matching the given patterns"
    complete -c hg -n "__fish_hg_using_command $cmd" -s X -l exclude -x -d "exclude names matching the given patterns"
    complete -c hg -n "__fish_hg_using_command $cmd" -s n -l dry-run -d "do not perform actions, just print output"
    complete -c hg -n "__fish_hg_using_command $cmd; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"
end

# hg annotate
for cmd in an ann anno annot annota annotat annotate bl bla blam blame
    complete -c hg -n "__fish_hg_using_command $cmd" -x -a "(__fish_hg_status -cmdr)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s r -l rev -x -a "(__fish_hg_labels)" -d "annotate the specified revision"
    complete -c hg -n "__fish_hg_using_command $cmd" -l no-follow -d "don't follow copies and renames"
    complete -c hg -n "__fish_hg_using_command $cmd" -s a -l text -d "treat all files as text"
    complete -c hg -n "__fish_hg_using_command $cmd" -s u -l user -d "list the author (long with -v)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s f -l file -d "list the filename"
    complete -c hg -n "__fish_hg_using_command $cmd" -s d -l date -d "list the date (short with -q)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s n -l number -d "list the revision number (default)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s c -l changeset -d "list the changeset"
    complete -c hg -n "__fish_hg_using_command $cmd" -s l -l line-number -d "show line number at the first appearance"
    complete -c hg -n "__fish_hg_using_command $cmd" -s w -l ignore-all-space -d "ignore white space when comparing lines"
    complete -c hg -n "__fish_hg_using_command $cmd" -s b -l ignore-space-change -d "ignore changes in the amount of white space"
    complete -c hg -n "__fish_hg_using_command $cmd" -s B -l ignore-blank-lines -d "ignore changes whose lines are all blank"
    complete -c hg -n "__fish_hg_using_command $cmd" -s Z -l ignore-space-at-eol -d "ignore changes in whitespace at EOL"
    complete -c hg -n "__fish_hg_using_command $cmd" -s I -l include -x -d "include names matching the given patterns"
    complete -c hg -n "__fish_hg_using_command $cmd" -s X -l exclude -x -d "exclude names matching the given patterns"
    complete -c hg -n "__fish_hg_using_command $cmd; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"
end

# hg archive
for cmd in ar arc arch archi archiv archive
    complete -c hg -n "__fish_hg_using_command $cmd" -l no-decode -d "do not pass files through decoders"
    complete -c hg -n "__fish_hg_using_command $cmd" -s p -l prefix -x -d "directory prefix for files in archive"
    complete -c hg -n "__fish_hg_using_command $cmd" -s r -l rev -x -a "(__fish_hg_labels)" -d "revision to distribute"
    complete -c hg -n "__fish_hg_using_command $cmd" -s t -l type -x -a "files tar tbz2 tgz uzip zip" -d "type of distribution to create"
    complete -c hg -n "__fish_hg_using_command $cmd" -s S -l subrepos -d "recurse into subrepositories"
    complete -c hg -n "__fish_hg_using_command $cmd" -s I -l include -x -d "include names matching the given patterns"
    complete -c hg -n "__fish_hg_using_command $cmd" -s X -l exclude -x -d "exclude names matching the given patterns"
    complete -c hg -n "__fish_hg_using_command $cmd; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"
end

# hg backout
for cmd in ba bac back backo backou backout
    complete -c hg -n "__fish_hg_using_command $cmd" -x -a "(__fish_hg_labels)"
    complete -c hg -n "__fish_hg_using_command $cmd" -l merge -d "merge with old dirstate parent after backout"
    complete -c hg -n "__fish_hg_using_command $cmd" -l no-commit -d "do not commit"
    complete -c hg -n "__fish_hg_using_command $cmd" -s r -l rev -x -a "(__fish_hg_labels)" -d "revision to backout"
    complete -c hg -n "__fish_hg_using_command $cmd" -s e -l edit -d "invoke editor on commit messages"
    complete -c hg -n "__fish_hg_using_command $cmd" -s t -l tool -x -a "(__fish_hg_merge_tools)" -d "specify merge tool"
    complete -c hg -n "__fish_hg_using_command $cmd" -s I -l include -x -d "include names matching the given patterns"
    complete -c hg -n "__fish_hg_using_command $cmd" -s X -l exclude -x -d "exclude names matching the given patterns"
    complete -c hg -n "__fish_hg_using_command $cmd" -s m -l message -x -d "use text as commit message"
    complete -c hg -n "__fish_hg_using_command $cmd" -s l -l logfile -r -d "read commit message from file"
    complete -c hg -n "__fish_hg_using_command $cmd" -s d -l date -x -d "record the specified date as commit date"
    complete -c hg -n "__fish_hg_using_command $cmd" -s u -l user -x -d "record the specified user as committer"
    complete -c hg -n "__fish_hg_using_command $cmd; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"
end

# hg bisect
for cmd in bi bis bise bisec bisect
    complete -c hg -n "__fish_hg_using_command $cmd" -f -a "(__fish_hg_labels)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s r -l reset -d "reset bisect state"
    complete -c hg -n "__fish_hg_using_command $cmd" -s g -l good -d "mark changeset good"
    complete -c hg -n "__fish_hg_using_command $cmd" -s b -l bad -d "mark changeset bad"
    complete -c hg -n "__fish_hg_using_command $cmd" -s s -l skip -d "skip testing changeset"
    complete -c hg -n "__fish_hg_using_command $cmd" -s e -l extend -d "extend the bisect range"
    complete -c hg -n "__fish_hg_using_command $cmd" -s c -l command -x -d "use command to check changeset state"
    complete -c hg -n "__fish_hg_using_command $cmd" -s U -l noupdate -d "do not update to target"
    complete -c hg -n "__fish_hg_using_command $cmd; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"
end

# hg bookmarks
for cmd in bo boo book bookm bookma bookmar bookmark bookmarks
    complete -c hg -n "__fish_hg_using_command $cmd" -f -a "(__fish_hg_bookmarks)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s f -l force -d force
    complete -c hg -n "__fish_hg_using_command $cmd" -s r -l rev -x -a "(__fish_hg_labels)" -d "revision for bookmark action"
    complete -c hg -n "__fish_hg_using_command $cmd" -s d -l delete -d "delete a given bookmark"
    complete -c hg -n "__fish_hg_using_command $cmd" -s m -l rename -x -a "(__fish_hg_bookmarks)" -d "rename a given bookmark"
    complete -c hg -n "__fish_hg_using_command $cmd" -s i -l inactive -d "mark a bookmark inactive"
    complete -c hg -n "__fish_hg_using_command $cmd; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"
end

# hg branch
complete -c hg -n "__fish_hg_using_command branch" -f -a "(__fish_hg_branches)"
complete -c hg -n "__fish_hg_using_command branch" -s f -l force -d "set branch name even if it shadows an existing branch"
complete -c hg -n "__fish_hg_using_command branch" -s C -l clean -d "reset branch name to parent branch name"
complete -c hg -n "__fish_hg_using_command branch; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"

# hg branches
for cmd in branche branches
    complete -c hg -n "__fish_hg_using_command $cmd" -s c -l closed -d "show normal and closed branches"
    complete -c hg -n "__fish_hg_using_command $cmd; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"
end

# hg bundle
for cmd in bu bun bund bundl bundle
    complete -c hg -n "__fish_hg_using_command $cmd" -s f -l force -d "run even when the destination is unrelated"
    complete -c hg -n "__fish_hg_using_command $cmd" -s r -l rev -x -a "(__fish_hg_labels)" -d "a changeset intended to be added to the destination"
    complete -c hg -n "__fish_hg_using_command $cmd" -s b -l branch -x -a "(__fish_hg_branches)" -d "a specific branch you would like to bundle"
    complete -c hg -n "__fish_hg_using_command $cmd" -l base -x -a "(__fish_hg_labels)" -d "a base changeset assumed to be available at the destination"
    complete -c hg -n "__fish_hg_using_command $cmd" -s a -l all -d "bundle all changesets in the repository"
    complete -c hg -n "__fish_hg_using_command $cmd" -s t -l type -x -d "bundle compression type to use (default: bzip2)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s e -l ssh -x -d "specify ssh command to use"
    complete -c hg -n "__fish_hg_using_command $cmd" -l remotecmd -x -d "specify hg command to run on the remote side"
    complete -c hg -n "__fish_hg_using_command $cmd" -l insecure -d "do not verify server certificate (ignoring web.cacerts config)"
    complete -c hg -n "__fish_hg_using_command $cmd; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"
end

# hg cat
for cmd in ca cat
    complete -c hg -n "__fish_hg_using_command $cmd" -x -a "(__fish_hg_status -cmrd)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s o -l output -x -d "print output to file with formatted name"
    complete -c hg -n "__fish_hg_using_command $cmd" -s r -l rev -x -a "(__fish_hg_labels)" -d "print the given revision"
    complete -c hg -n "__fish_hg_using_command $cmd" -l decode -d "apply any matching decode filter"
    complete -c hg -n "__fish_hg_using_command $cmd" -s I -l include -x -d "include names matching the given patterns"
    complete -c hg -n "__fish_hg_using_command $cmd" -s X -l exclude -x -d "exclude names matching the given patterns"
    complete -c hg -n "__fish_hg_using_command $cmd; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"
end

# hg clone
for cmd in cl clo clon clone
    complete -c hg -n "__fish_hg_using_command $cmd" -r -a "(__fish_hg_sources)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s U -l noupdate -d "the clone will include an empty working directory (only a repository)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s u -l updaterev -x -d "revision, tag, or branch to check out"
    complete -c hg -n "__fish_hg_using_command $cmd" -s r -l rev -x -d "do not clone everything, but include this changeset and its ancestors"
    complete -c hg -n "__fish_hg_using_command $cmd" -s b -l branch -x -d "do not clone everything, but include this branch's changesets and their ancestors"
    complete -c hg -n "__fish_hg_using_command $cmd" -l pull -d "use pull protocol to copy metadata"
    complete -c hg -n "__fish_hg_using_command $cmd" -l stream -d "clone with minimal data processing"
    complete -c hg -n "__fish_hg_using_command $cmd" -s e -l ssh -x -d "specify ssh command to use"
    complete -c hg -n "__fish_hg_using_command $cmd" -l remotecmd -x -d "specify hg command to run on the remote side"
    complete -c hg -n "__fish_hg_using_command $cmd" -l insecure -d "do not verify server certificate (ignoring web.cacerts config)"
end

# hg commit
for cmd in com comm commi commit ci
    complete -c hg -n "__fish_hg_using_command $cmd" -f -a "(__fish_hg_status -amr)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s A -l addremove -d "mark new/missing files as added/removed before committing"
    complete -c hg -n "__fish_hg_using_command $cmd" -l close-branch -d "mark a branch head as closed"
    complete -c hg -n "__fish_hg_using_command $cmd" -l amend -d "amend the parent of the working directory"
    complete -c hg -n "__fish_hg_using_command $cmd" -s s -l secret -d "use the secret phase for committing"
    complete -c hg -n "__fish_hg_using_command $cmd" -s e -l edit -d "invoke editor on commit messages"
    complete -c hg -n "__fish_hg_using_command $cmd" -s i -l interactive -d "use interactive mode"
    complete -c hg -n "__fish_hg_using_command $cmd" -s I -l include -x -d "include names matching the given patterns"
    complete -c hg -n "__fish_hg_using_command $cmd" -s X -l exclude -x -d "exclude names matching the given patterns"
    complete -c hg -n "__fish_hg_using_command $cmd" -s m -l message -x -d "use text as commit message"
    complete -c hg -n "__fish_hg_using_command $cmd" -s l -l logfile -r -d "read commit message from file"
    complete -c hg -n "__fish_hg_using_command $cmd" -s d -l date -x -d "record the specified date as commit date"
    complete -c hg -n "__fish_hg_using_command $cmd" -s u -l user -x -d "record the specified user as committer"
    complete -c hg -n "__fish_hg_using_command $cmd" -s S -l subrepos -d "recurse into subrepositories"
    complete -c hg -n "__fish_hg_using_command $cmd; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"
end

# hg config
for cmd in conf confi config sh sho show showc showco showcon showconf showconfi showconfig
    complete -c hg -n "__fish_hg_using_command $cmd" -f -a "(__fish_hg_config_entries)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s u -l untrusted -d "show untrusted configuration options"
    complete -c hg -n "__fish_hg_using_command $cmd" -s e -l edit -d "edit user config"
    complete -c hg -n "__fish_hg_using_command $cmd" -s l -l local -d "edit repository config"
    complete -c hg -n "__fish_hg_using_command $cmd" -s g -l global -d "edit global config"
    complete -c hg -n "__fish_hg_using_command $cmd; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"
end

# hg convert
for cmd in conv conve conver convert
    complete -c hg -n "__fish_hg_using_command $cmd" -s s -l source-type -x -a "hg cvs darcs git svn mtn gnuarch bzr p4" -d "source repository type"
    complete -c hg -n "__fish_hg_using_command $cmd" -s d -l dest-type -x -a "hg svn" -d "destination repository type"
    complete -c hg -n "__fish_hg_using_command $cmd" -s r -l rev -x -d "import up to this source revision"
    complete -c hg -n "__fish_hg_using_command $cmd" -s A -l authormap -r -d "remap usernames using this file"
    complete -c hg -n "__fish_hg_using_command $cmd" -l filemap -r -d "remap file names using contents of file"
    complete -c hg -n "__fish_hg_using_command $cmd" -l full -d "apply filemap changes by converting all files again"
    complete -c hg -n "__fish_hg_using_command $cmd" -l splicemap -r -d "splice synthesized history into place"
    complete -c hg -n "__fish_hg_using_command $cmd" -l branchmap -r -d "change branch names while converting"
    complete -c hg -n "__fish_hg_using_command $cmd" -l branchsort -d "try to sort changesets by branches"
    complete -c hg -n "__fish_hg_using_command $cmd" -l datesort -d "try to sort changesets by date"
    complete -c hg -n "__fish_hg_using_command $cmd" -l sourcesort -d "preserve source changesets order"
    complete -c hg -n "__fish_hg_using_command $cmd" -l closesort -d "try to reorder closed revisions"
end

# hg copy
for cmd in cop copy cp
    complete -c hg -n "__fish_hg_using_command $cmd" -x -a "(__fish_hg_status -cmrd)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s A -l after -d "record a copy that has already occurred"
    complete -c hg -n "__fish_hg_using_command $cmd" -s f -l force -d "forcibly copy over an existing managed file"
    complete -c hg -n "__fish_hg_using_command $cmd" -s I -l include -x -d "include names matching the given patterns"
    complete -c hg -n "__fish_hg_using_command $cmd" -s X -l exclude -x -d "exclude names matching the given patterns"
    complete -c hg -n "__fish_hg_using_command $cmd" -s n -l dry-run -d "do not perform actions, just print output"
    complete -c hg -n "__fish_hg_using_command $cmd; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"
end

# hg diff
for cmd in d di dif diff
    complete -c hg -n "__fish_hg_using_command $cmd" -f -a "(__fish_hg_status -amr)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s r -l rev -x -a "(__fish_hg_labels)" -d revision
    complete -c hg -n "__fish_hg_using_command $cmd" -s c -l change -x -a "(__fish_hg_labels)" -d "change made by revision"
    complete -c hg -n "__fish_hg_using_command $cmd" -s a -l text -d "treat all files as text"
    complete -c hg -n "__fish_hg_using_command $cmd" -s g -l git -d "use git extended diff format"
    complete -c hg -n "__fish_hg_using_command $cmd" -l binary -d "generate binary diffs in git mode (default)"
    complete -c hg -n "__fish_hg_using_command $cmd" -l nodates -d "omit dates from diff headers"
    complete -c hg -n "__fish_hg_using_command $cmd" -l noprefix -d "omit a/ and b/ prefixes from filenames"
    complete -c hg -n "__fish_hg_using_command $cmd" -s p -l show-function -d "show which function each change is in"
    complete -c hg -n "__fish_hg_using_command $cmd" -l reverse -d "produce a diff that undoes the changes"
    complete -c hg -n "__fish_hg_using_command $cmd" -s w -l ignore-all-space -d "ignore white space when comparing lines"
    complete -c hg -n "__fish_hg_using_command $cmd" -s b -l ignore-space-change -d "ignore changes in the amount of white space"
    complete -c hg -n "__fish_hg_using_command $cmd" -s B -l ignore-blank-lines -d "ignore changes whose lines are all blank"
    complete -c hg -n "__fish_hg_using_command $cmd" -s Z -l ignore-space-at-eol -d "ignore changes in whitespace at EOL"
    complete -c hg -n "__fish_hg_using_command $cmd" -s U -l unified -x -d "number of lines of context to show"
    complete -c hg -n "__fish_hg_using_command $cmd" -l stat -d "output diffstat-style summary of changes"
    complete -c hg -n "__fish_hg_using_command $cmd" -l root -x -d "produce diffs relative to subdirectory"
    complete -c hg -n "__fish_hg_using_command $cmd" -s I -l include -x -d "include names matching the given patterns"
    complete -c hg -n "__fish_hg_using_command $cmd" -s X -l exclude -x -d "exclude names matching the given patterns"
    complete -c hg -n "__fish_hg_using_command $cmd" -s S -l subrepos -d "recurse into subrepositories"
    complete -c hg -n "__fish_hg_using_command $cmd; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"
end

# hg email
for cmd in em ema emai email
    complete -c hg -n "__fish_hg_using_command $cmd" -f -a "(__fish_hg_labels)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s g -l git -d "use git extended diff format"
    complete -c hg -n "__fish_hg_using_command $cmd" -l plain -d "omit hg patch header"
    complete -c hg -n "__fish_hg_using_command $cmd" -s o -l outgoing -d "send changes not found in the target repository"
    complete -c hg -n "__fish_hg_using_command $cmd" -s b -l bundle -d "send changes not in target as a binary bundle"
    complete -c hg -n "__fish_hg_using_command $cmd" -s B -l bookmark -x -a "(__fish_hg_bookmarks)" -d "send changes only reachable by given bookmark"
    complete -c hg -n "__fish_hg_using_command $cmd" -l bundlename -x -d "name of the bundle attachment file"
    complete -c hg -n "__fish_hg_using_command $cmd" -s r -l rev -x -a "(__fish_hg_labels)" -d "a revision to send"
    complete -c hg -n "__fish_hg_using_command $cmd" -l force -d "run even when remote repository is unrelated (with -b/--bundle)"
    complete -c hg -n "__fish_hg_using_command $cmd" -l base -x -a "(__fish_hg_labels)" -d "a base changeset to specify instead of a destination (with -b/--bundle)"
    complete -c hg -n "__fish_hg_using_command $cmd" -l intro -d "send an introduction email for a single patch"
    complete -c hg -n "__fish_hg_using_command $cmd" -l body -d "send patches as inline message text (default)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s a -l attach -d "send patches as attachments"
    complete -c hg -n "__fish_hg_using_command $cmd" -s i -l inline -d "send patches as inline attachments"
    complete -c hg -n "__fish_hg_using_command $cmd" -l bcc -x -d "email addresses of blind carbon copy recipients"
    complete -c hg -n "__fish_hg_using_command $cmd" -s c -l cc -x -d "email addresses of copy recipients"
    complete -c hg -n "__fish_hg_using_command $cmd" -l confirm -d "ask for confirmation before sending"
    complete -c hg -n "__fish_hg_using_command $cmd" -s d -l diffstat -d "add diffstat output to messages"
    complete -c hg -n "__fish_hg_using_command $cmd" -l date -x -d "use the given date as the sending date"
    complete -c hg -n "__fish_hg_using_command $cmd" -l desc -x -d "use the given file as the series description"
    complete -c hg -n "__fish_hg_using_command $cmd" -s f -l from -x -d "email address of sender"
    complete -c hg -n "__fish_hg_using_command $cmd" -s n -l test -d "print messages that would be sent"
    complete -c hg -n "__fish_hg_using_command $cmd" -s m -l mbox -r -d "write messages to mbox file instead of sending them"
    complete -c hg -n "__fish_hg_using_command $cmd" -l reply-to -x -d "email addresses replies should be sent to"
    complete -c hg -n "__fish_hg_using_command $cmd" -s s -l subject -x -d "subject of first message (intro or single patch)"
    complete -c hg -n "__fish_hg_using_command $cmd" -l in-reply-to -x -d "message identifier to reply to"
    complete -c hg -n "__fish_hg_using_command $cmd" -l flag -x -d "flags to add in subject prefixes"
    complete -c hg -n "__fish_hg_using_command $cmd" -s t -l to -x -d "email addresses of recipients"
    complete -c hg -n "__fish_hg_using_command $cmd" -s e -l ssh -x -d "specify ssh command to use"
    complete -c hg -n "__fish_hg_using_command $cmd" -l remotecmd -x -d "specify hg command to run on the remote side"
    complete -c hg -n "__fish_hg_using_command $cmd" -l insecure -d "do not verify server certificate (ignoring web.cacerts config)"
end

# hg export
for cmd in exp expo expor export
    complete -c hg -n "__fish_hg_using_command $cmd" -f -a "(__fish_hg_labels)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s B -l bookmark -x -a "(__fish_hg_bookmarks)" -d "export changes only reachable by given bookmark"
    complete -c hg -n "__fish_hg_using_command $cmd" -s o -l output -x -d "print output to file with formatted name"
    complete -c hg -n "__fish_hg_using_command $cmd" -l switch-parent -d "diff against the second parent"
    complete -c hg -n "__fish_hg_using_command $cmd" -s r -l rev -x -a "(__fish_hg_labels)" -d "revisions to export"
    complete -c hg -n "__fish_hg_using_command $cmd" -s a -l text -d "treat all files as text"
    complete -c hg -n "__fish_hg_using_command $cmd" -s g -l git -d "use git extended diff format"
    complete -c hg -n "__fish_hg_using_command $cmd" -l binary -d "generate binary diffs in git mode (default)"
    complete -c hg -n "__fish_hg_using_command $cmd" -l nodates -d "omit dates from diff headers"
    complete -c hg -n "__fish_hg_using_command $cmd; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"
end

# hg files
for cmd in fi fil file files
    complete -c hg -n "__fish_hg_using_command $cmd" -s r -l rev -x -a "(__fish_hg_labels)" -d "search the repository as it is in REV"
    complete -c hg -n "__fish_hg_using_command $cmd" -s 0 -l print0 -d "end filenames with NUL, for use with xargs"
    complete -c hg -n "__fish_hg_using_command $cmd" -s I -l include -x -d "include names matching the given patterns"
    complete -c hg -n "__fish_hg_using_command $cmd" -s X -l exclude -x -d "exclude names matching the given patterns"
    complete -c hg -n "__fish_hg_using_command $cmd" -s S -l subrepos -d "recurse into subrepositories"
    complete -c hg -n "__fish_hg_using_command $cmd; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"
end

# hg forget
for cmd in fo for forg forge forget
    complete -c hg -n "__fish_hg_using_command $cmd" -x -a "(__fish_hg_status -ca)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s i -l interactive -d "use interactive mode"
    complete -c hg -n "__fish_hg_using_command $cmd" -s I -l include -x -d "include names matching the given patterns"
    complete -c hg -n "__fish_hg_using_command $cmd" -s X -l exclude -x -d "exclude names matching the given patterns"
    complete -c hg -n "__fish_hg_using_command $cmd" -s n -l dry-run -d "do not perform actions, just print output"
    complete -c hg -n "__fish_hg_using_command $cmd; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"
end

# hg graft
for cmd in gra graf graft
    complete -c hg -n "__fish_hg_using_command $cmd" -x -a "(__fish_hg_labels)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s r -l rev -x -a "(__fish_hg_labels)" -d "revisions to graft"
    complete -c hg -n "__fish_hg_using_command $cmd" -s c -l continue -d "resume interrupted graft"
    complete -c hg -n "__fish_hg_using_command $cmd" -l stop -d "stop interrupted graft"
    complete -c hg -n "__fish_hg_using_command $cmd" -l abort -d "abort interrupted graft"
    complete -c hg -n "__fish_hg_using_command $cmd" -s e -l edit -d "invoke editor on commit messages"
    complete -c hg -n "__fish_hg_using_command $cmd" -l log -d "append graft info to log message"
    complete -c hg -n "__fish_hg_using_command $cmd" -l no-commit -d "don't commit, just apply the changes in working directory"
    complete -c hg -n "__fish_hg_using_command $cmd" -s f -l force -d "force graft"
    complete -c hg -n "__fish_hg_using_command $cmd" -s D -l currentdate -d "record the current date as commit date"
    complete -c hg -n "__fish_hg_using_command $cmd" -s U -l currentuser -d "record the current user as committer"
    complete -c hg -n "__fish_hg_using_command $cmd" -s d -l date -x -d "record the specified date as commit date"
    complete -c hg -n "__fish_hg_using_command $cmd" -s u -l user -x -d "record the specified user as committer"
    complete -c hg -n "__fish_hg_using_command $cmd" -s t -l tool -x -a "(__fish_hg_merge_tools)" -d "specify merge tool"
    complete -c hg -n "__fish_hg_using_command $cmd" -s n -l dry-run -d "do not perform actions, just print output"
    complete -c hg -n "__fish_hg_using_command $cmd; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"
end

# hg grep
for cmd in gre grep
    complete -c hg -n "__fish_hg_using_command $cmd" -f -a "(__fish_hg_status -cmrd)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s 0 -l print0 -d "end fields with NUL"
    complete -c hg -n "__fish_hg_using_command $cmd" -l all -d "print all revisions that match"
    complete -c hg -n "__fish_hg_using_command $cmd" -l diff -d "print all revisions when the term was introduced or removed"
    complete -c hg -n "__fish_hg_using_command $cmd" -s a -l text -d "treat all files as text"
    complete -c hg -n "__fish_hg_using_command $cmd" -s f -l follow -d "follow changeset history, or file history across copies and renames"
    complete -c hg -n "__fish_hg_using_command $cmd" -s i -l ignore-case -d "ignore case when matching"
    complete -c hg -n "__fish_hg_using_command $cmd" -s l -l files-with-matches -d "print only filenames and revisions that match"
    complete -c hg -n "__fish_hg_using_command $cmd" -s n -l line-number -d "print matching line numbers"
    complete -c hg -n "__fish_hg_using_command $cmd" -s r -l rev -x -a "(__fish_hg_labels)" -d "only search files changed within revision range"
    complete -c hg -n "__fish_hg_using_command $cmd" -s u -l user -d "list the author (long with -v)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s d -l date -d "list the date (short with -q)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s I -l include -x -d "include names matching the given patterns"
    complete -c hg -n "__fish_hg_using_command $cmd" -s X -l exclude -x -d "exclude names matching the given patterns"
    complete -c hg -n "__fish_hg_using_command $cmd; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"
end

# hg heads
for cmd in hea head heads
    complete -c hg -n "__fish_hg_using_command $cmd" -x -a "(__fish_hg_labels)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s r -l rev -x -a "(__fish_hg_labels)" -d "show only heads which are descendants of STARTREV"
    complete -c hg -n "__fish_hg_using_command $cmd" -s t -l topo -d "show topological heads only"
    complete -c hg -n "__fish_hg_using_command $cmd" -s c -l closed -d "show normal and closed branch heads"
    complete -c hg -n "__fish_hg_using_command $cmd" -s T -l template -x -d "display with template"
    complete -c hg -n "__fish_hg_using_command $cmd; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"
end

# hg help
for cmd in hel help
    complete -c hg -n "__fish_hg_using_command $cmd" -x -a "(__fish_hg_help_topics)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s e -l extension -d "show only help for extensions"
    complete -c hg -n "__fish_hg_using_command $cmd" -s c -l command -d "show only help for commands"
    complete -c hg -n "__fish_hg_using_command $cmd" -s k -l keyword -d "show topics matching keyword"
    complete -c hg -n "__fish_hg_using_command $cmd" -s s -l system -x -d "show help for specific platform(s)"
end

# hg histedit
for cmd in histe histed histedi histedit
    complete -c hg -n "__fish_hg_using_command $cmd" -x -a "(__fish_hg_labels)"
    complete -c hg -n "__fish_hg_using_command $cmd" -l commands -r -d "read history edits from the specified file"
    complete -c hg -n "__fish_hg_using_command $cmd" -s c -l continue -d "continue an edit already in progress"
    complete -c hg -n "__fish_hg_using_command $cmd" -l edit-plan -d "edit remaining actions list"
    complete -c hg -n "__fish_hg_using_command $cmd" -s k -l keep -d "don't strip old nodes after edit is complete"
    complete -c hg -n "__fish_hg_using_command $cmd" -l abort -d "abort an edit in progress"
    complete -c hg -n "__fish_hg_using_command $cmd" -s o -l outgoing -d "changesets not found in destination"
    complete -c hg -n "__fish_hg_using_command $cmd" -s f -l force -d "force outgoing even for unrelated repositories"
    complete -c hg -n "__fish_hg_using_command $cmd" -s r -l rev -x -a "(__fish_hg_labels)" -d "first revision to be edited"
    complete -c hg -n "__fish_hg_using_command $cmd; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"
end

# hg identify
for cmd in id ide iden ident identi identif identify
    complete -c hg -n "__fish_hg_using_command $cmd" -f -a "(__fish_hg_sources)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s r -l rev -x -a "(__fish_hg_labels)" -d "identify the specified revision"
    complete -c hg -n "__fish_hg_using_command $cmd" -s n -l num -d "show local revision number"
    complete -c hg -n "__fish_hg_using_command $cmd" -s i -l id -d "show global revision id"
    complete -c hg -n "__fish_hg_using_command $cmd" -s b -l branch -d "show branch"
    complete -c hg -n "__fish_hg_using_command $cmd" -s t -l tags -d "show tags"
    complete -c hg -n "__fish_hg_using_command $cmd" -s B -l bookmarks -d "show bookmarks"
    complete -c hg -n "__fish_hg_using_command $cmd" -s e -l ssh -x -d "specify ssh command to use"
    complete -c hg -n "__fish_hg_using_command $cmd" -l remotecmd -x -d "specify hg command to run on the remote side"
    complete -c hg -n "__fish_hg_using_command $cmd" -l insecure -d "do not verify server certificate (ignoring web.cacerts config)"
    complete -c hg -n "__fish_hg_using_command $cmd; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"
end

# hg import
for cmd in im imp impo impor import patc patch
    complete -c hg -n "__fish_hg_using_command $cmd" -s p -l strip -x -d "directory strip option for patch"
    complete -c hg -n "__fish_hg_using_command $cmd" -s e -l edit -d "invoke editor on commit messages"
    complete -c hg -n "__fish_hg_using_command $cmd" -l no-commit -d "don't commit, just update the working directory"
    complete -c hg -n "__fish_hg_using_command $cmd" -l bypass -d "apply patch without touching the working directory"
    complete -c hg -n "__fish_hg_using_command $cmd" -l partial -d "commit even if some hunks fail"
    complete -c hg -n "__fish_hg_using_command $cmd" -l exact -d "abort if patch would apply lossily"
    complete -c hg -n "__fish_hg_using_command $cmd" -l prefix -r -d "apply patch to subdirectory"
    complete -c hg -n "__fish_hg_using_command $cmd" -l import-branch -d "use any branch information in patch (implied by --exact)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s m -l message -x -d "use text as commit message"
    complete -c hg -n "__fish_hg_using_command $cmd" -s l -l logfile -r -d "read commit message from file"
    complete -c hg -n "__fish_hg_using_command $cmd" -s d -l date -x -d "record the specified date as commit date"
    complete -c hg -n "__fish_hg_using_command $cmd" -s u -l user -x -d "record the specified user as committer"
    complete -c hg -n "__fish_hg_using_command $cmd" -s s -l similarity -x -d "guess renamed files by similarity (0<=s<=100)"
    complete -c hg -n "__fish_hg_using_command $cmd; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"
end

# hg incoming
for cmd in inc inco incom incomi incomin incoming in
    complete -c hg -n "__fish_hg_using_command $cmd" -f -a "(__fish_hg_sources)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s f -l force -d "run even if remote repository is unrelated"
    complete -c hg -n "__fish_hg_using_command $cmd" -s n -l newest-first -d "show newest record first"
    complete -c hg -n "__fish_hg_using_command $cmd" -l bundle -r -d "file to store the bundles into"
    complete -c hg -n "__fish_hg_using_command $cmd" -s r -l rev -x -d "a remote changeset intended to be added"
    complete -c hg -n "__fish_hg_using_command $cmd" -s B -l bookmarks -d "compare bookmarks"
    complete -c hg -n "__fish_hg_using_command $cmd" -s b -l branch -x -d "a specific branch you would like to pull"
    complete -c hg -n "__fish_hg_using_command $cmd" -s p -l patch -d "show patch"
    complete -c hg -n "__fish_hg_using_command $cmd" -s g -l git -d "use git extended diff format"
    complete -c hg -n "__fish_hg_using_command $cmd" -s l -l limit -x -d "limit number of changes displayed"
    complete -c hg -n "__fish_hg_using_command $cmd" -s M -l no-merges -d "do not show merges"
    complete -c hg -n "__fish_hg_using_command $cmd" -l stat -d "output diffstat-style summary of changes"
    complete -c hg -n "__fish_hg_using_command $cmd" -s G -l graph -d "show the revision DAG"
    complete -c hg -n "__fish_hg_using_command $cmd" -s T -l template -x -d "display with template"
    complete -c hg -n "__fish_hg_using_command $cmd" -s e -l ssh -x -d "specify ssh command to use"
    complete -c hg -n "__fish_hg_using_command $cmd" -l remotecmd -x -d "specify hg command to run on the remote side"
    complete -c hg -n "__fish_hg_using_command $cmd" -l insecure -d "do not verify server certificate (ignoring web.cacerts config)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s S -l subrepos -d "recurse into subrepositories"
    complete -c hg -n "__fish_hg_using_command $cmd; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"
end

# hg init
for cmd in ini init
    complete -c hg -n "__fish_hg_using_command $cmd" -s e -l ssh -x -d "specify ssh command to use"
    complete -c hg -n "__fish_hg_using_command $cmd" -l remotecmd -x -d "specify hg command to run on the remote side"
    complete -c hg -n "__fish_hg_using_command $cmd" -l insecure -d "do not verify server certificate (ignoring web.cacerts config)"
    complete -c hg -n "__fish_hg_using_command $cmd; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"
end

# hg locate
for cmd in loc loca locat locate
    complete -c hg -n "__fish_hg_using_command $cmd" -s r -l rev -x -a "(__fish_hg_labels)" -d "search the repository as it is in REV"
    complete -c hg -n "__fish_hg_using_command $cmd" -s 0 -l print0 -d "end filenames with NUL, for use with xargs"
    complete -c hg -n "__fish_hg_using_command $cmd" -s f -l fullpath -d "print complete paths from the filesystem root"
    complete -c hg -n "__fish_hg_using_command $cmd" -s I -l include -x -d "include names matching the given patterns"
    complete -c hg -n "__fish_hg_using_command $cmd" -s X -l exclude -x -d "exclude names matching the given patterns"
    complete -c hg -n "__fish_hg_using_command $cmd; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"
end

# hg log
for cmd in log histo histor history
    complete -c hg -n "__fish_hg_using_command "$cmd -f -a "(__fish_hg_status -cmrd)"
    complete -c hg -n "__fish_hg_using_command "$cmd -s f -l follow -d "follow changeset history, or file history across copies and renames"
    complete -c hg -n "__fish_hg_using_command "$cmd -s d -l date -x -d "show revisions matching date spec"
    complete -c hg -n "__fish_hg_using_command "$cmd -s C -l copies -d "show copied files"
    complete -c hg -n "__fish_hg_using_command "$cmd -s k -l keyword -x -d "do case-insensitive search for a given text"
    complete -c hg -n "__fish_hg_using_command "$cmd -s r -l rev -x -a "(__fish_hg_labels)" -d "show the specified revision or revset"
    complete -c hg -n "__fish_hg_using_command "$cmd -l removed -d "include revisions where files were removed"
    complete -c hg -n "__fish_hg_using_command "$cmd -s u -l user -x -d "revisions committed by user"
    complete -c hg -n "__fish_hg_using_command "$cmd -s b -l branch -x -a "(__fish_hg_branches)" -d "show changesets within the given named branch"
    complete -c hg -n "__fish_hg_using_command "$cmd -s P -l prune -x -a "(__fish_hg_labels)" -d "do not display revision or any of its ancestors"
    complete -c hg -n "__fish_hg_using_command "$cmd -s p -l patch -d "show patch"
    complete -c hg -n "__fish_hg_using_command "$cmd -s g -l git -d "use git extended diff format"
    complete -c hg -n "__fish_hg_using_command "$cmd -s l -l limit -x -d "limit number of changes displayed"
    complete -c hg -n "__fish_hg_using_command "$cmd -s M -l no-merges -d "do not show merges"
    complete -c hg -n "__fish_hg_using_command "$cmd -l stat -d "output diffstat-style summary of changes"
    complete -c hg -n "__fish_hg_using_command "$cmd -s G -l graph -d "show the revision DAG"
    complete -c hg -n "__fish_hg_using_command "$cmd -s T -l template -x -d "display with template"
    complete -c hg -n "__fish_hg_using_command "$cmd -s I -l include -x -d "include names matching the given patterns"
    complete -c hg -n "__fish_hg_using_command "$cmd -s X -l exclude -x -d "exclude names matching the given patterns"
    complete -c hg -n "__fish_hg_using_command "$cmd"; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"
end

# hg manifest
for cmd in ma man mani manif manife manifes manifest
    complete -c hg -n "__fish_hg_using_command $cmd" -s r -l rev -x -a "(__fish_hg_labels)" -d "revision to display"
    complete -c hg -n "__fish_hg_using_command $cmd" -l all -d "list files from all revisions"
    complete -c hg -n "__fish_hg_using_command $cmd; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"
end

# hg merge
for cmd in me mer merg merge
    complete -c hg -n "__fish_hg_using_command $cmd" -f -a "(__fish_hg_labels)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s r -l rev -x -a "(__fish_hg_labels)" -d "revision to merge"
    complete -c hg -n "__fish_hg_using_command $cmd" -s P -l preview -d "review revisions to merge (no merge is performed)"
    complete -c hg -n "__fish_hg_using_command $cmd" -l abort -d "abort the ongoing merge"
    complete -c hg -n "__fish_hg_using_command $cmd" -s t -l tool -x -a "(__fish_hg_merge_tools)" -d "specify merge tool"
    complete -c hg -n "__fish_hg_using_command $cmd; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"
end

# hg outgoing
for cmd in o ou out outg outgo outgoi outgoin outgoing
    complete -c hg -n "__fish_hg_using_command $cmd" -f -a "(__fish_hg_sources)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s f -l force -d "run even when the destination is unrelated"
    complete -c hg -n "__fish_hg_using_command $cmd" -s r -l rev -x -a "(__fish_hg_labels)" -d "a changeset intended to be included in the destination"
    complete -c hg -n "__fish_hg_using_command $cmd" -s n -l newest-first -d "show newest record first"
    complete -c hg -n "__fish_hg_using_command $cmd" -s B -l bookmarks -d "compare bookmarks"
    complete -c hg -n "__fish_hg_using_command $cmd" -s b -l branch -x -a "(__fish_hg_branches)" -d "a specific branch you would like to push"
    complete -c hg -n "__fish_hg_using_command $cmd" -s p -l patch -d "show patch"
    complete -c hg -n "__fish_hg_using_command $cmd" -s g -l git -d "use git extended diff format"
    complete -c hg -n "__fish_hg_using_command $cmd" -s l -l limit -x -d "limit number of changes displayed"
    complete -c hg -n "__fish_hg_using_command $cmd" -s M -l no-merges -d "do not show merges"
    complete -c hg -n "__fish_hg_using_command $cmd" -l stat -d "output diffstat-style summary of changes"
    complete -c hg -n "__fish_hg_using_command $cmd" -s G -l graph -d "show the revision DAG"
    complete -c hg -n "__fish_hg_using_command $cmd" -s T -l template -x -d "display with template"
    complete -c hg -n "__fish_hg_using_command $cmd" -s e -l ssh -x -d "specify ssh command to use"
    complete -c hg -n "__fish_hg_using_command $cmd" -l remotecmd -x -d "specify hg command to run on the remote side"
    complete -c hg -n "__fish_hg_using_command $cmd" -l insecure -d "do not verify server certificate (ignoring web.cacerts config)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s S -l subrepos -d "recurse into subrepositories"
    complete -c hg -n "__fish_hg_using_command $cmd; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"
end

# hg parents
for cmd in par pare paren parent parents
    complete -c hg -n "__fish_hg_using_command $cmd" -f -a "(__fish_hg_status -cmrd)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s r -l rev -x -a "(__fish_hg_labels)" -d "show parents of the specified revision"
    complete -c hg -n "__fish_hg_using_command $cmd" -s T -l template -x -d "display with template"
    complete -c hg -n "__fish_hg_using_command $cmd; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"
end

# hg paths
for cmd in path paths
    complete -c hg -n "__fish_hg_using_command $cmd; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"
end

# hg phase
for cmd in ph pha phas phase
    complete -c hg -n "__fish_hg_using_command $cmd" -x -a "(__fish_hg_labels)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s p -l public -d "set changeset phase to public"
    complete -c hg -n "__fish_hg_using_command $cmd" -s d -l draft -d "set changeset phase to draft"
    complete -c hg -n "__fish_hg_using_command $cmd" -s s -l secret -d "set changeset phase to secret"
    complete -c hg -n "__fish_hg_using_command $cmd" -s f -l force -d "allow to move boundary backward"
    complete -c hg -n "__fish_hg_using_command $cmd" -s r -l rev -x -a "(__fish_hg_labels)" -d "target revision"
    complete -c hg -n "__fish_hg_using_command $cmd; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"
end

# hg pull
for cmd in pul pull
    complete -c hg -n "__fish_hg_using_command $cmd" -r -a "(__fish_hg_sources)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s u -l update -d "update to new branch head if new descendants were pulled"
    complete -c hg -n "__fish_hg_using_command $cmd" -s f -l force -d "run even when remote repository is unrelated"
    complete -c hg -n "__fish_hg_using_command $cmd" -s r -l rev -x -d "a remote changeset intended to be added"
    complete -c hg -n "__fish_hg_using_command $cmd" -s B -l bookmark -x -d "bookmark to pull"
    complete -c hg -n "__fish_hg_using_command $cmd" -s b -l branch -x -d "a specific branch you would like to pull"
    complete -c hg -n "__fish_hg_using_command $cmd" -s e -l ssh -x -d "specify ssh command to use"
    complete -c hg -n "__fish_hg_using_command $cmd" -l remotecmd -x -d "specify hg command to run on the remote side"
    complete -c hg -n "__fish_hg_using_command $cmd" -l insecure -d "do not verify server certificate (ignoring web.cacerts config)"
    complete -c hg -n "__fish_hg_using_command $cmd; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"
end

# hg push
for cmd in pus push
    complete -c hg -n "__fish_hg_using_command $cmd" -r -a "(__fish_hg_sources)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s f -l force -d "force push"
    complete -c hg -n "__fish_hg_using_command $cmd" -s r -l rev -x -a "(__fish_hg_labels)" -d "a changeset intended to be included in the destination"
    complete -c hg -n "__fish_hg_using_command $cmd" -s B -l bookmark -x -a "(__fish_hg_bookmarks)" -d "bookmark to push"
    complete -c hg -n "__fish_hg_using_command $cmd" -s b -l branch -x -a "(__fish_hg_branches)" -d "a specific branch you would like to push"
    complete -c hg -n "__fish_hg_using_command $cmd" -l new-branch -d "allow pushing a new branch"
    complete -c hg -n "__fish_hg_using_command $cmd" -s e -l ssh -x -d "specify ssh command to use"
    complete -c hg -n "__fish_hg_using_command $cmd" -l remotecmd -x -d "specify hg command to run on the remote side"
    complete -c hg -n "__fish_hg_using_command $cmd" -l insecure -d "do not verify server certificate (ignoring web.cacerts config)"
    complete -c hg -n "__fish_hg_using_command $cmd; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"
end

# hg qapplied
for cmd in qa qap qapp qappl qappli qapplie qapplied
    complete -c hg -n "__fish_hg_using_command $cmd" -x -a "(__fish_hg_patches)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s 1 -l last -d "show only the preceding applied patch"
    complete -c hg -n "__fish_hg_using_command $cmd" -s s -l summary -d "print first line of patch header"
end

# hg qclone
for cmd in qcl qclo qclon qclone
    complete -c hg -n "__fish_hg_using_command $cmd" -r -a "(__fish_hg_sources)"
    complete -c hg -n "__fish_hg_using_command $cmd" -l pull -d "use pull protocol to copy metadata"
    complete -c hg -n "__fish_hg_using_command $cmd" -s U -l noupdate -d "do not update the new working directories"
    complete -c hg -n "__fish_hg_using_command $cmd" -l uncompressed -d "use uncompressed transfer (fast over LAN)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s p -l patches -d "location of source patch repository"
    complete -c hg -n "__fish_hg_using_command $cmd" -s e -l ssh -x -d "specify ssh command to use"
    complete -c hg -n "__fish_hg_using_command $cmd" -l remotecmd -x -d "specify hg command to run on the remote side"
    complete -c hg -n "__fish_hg_using_command $cmd" -l insecure -d "do not verify server certificate (ignoring web.cacerts config)"
end

# hg qdelete
for cmd in qde qdel qdele qdelet qdelete qrem qremo qremov qremove qrm
    complete -c hg -n "__fish_hg_using_command $cmd" -x -a "(__fish_hg_patches)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s k -l keep -d "keep patch file"
end

# hg qdiff
for cmd in qdi qdif qdiff
    complete -c hg -n "__fish_hg_using_command $cmd" -f -a "(__fish_hg_status -mrd --rev .^)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s a -l text -d "treat all files as text"
    complete -c hg -n "__fish_hg_using_command $cmd" -s g -l git -d "use git extended diff format"
    complete -c hg -n "__fish_hg_using_command $cmd" -l binary -d "generate binary diffs in git mode (default)"
    complete -c hg -n "__fish_hg_using_command $cmd" -l nodates -d "omit dates from diff headers"
    complete -c hg -n "__fish_hg_using_command $cmd" -l noprefix -d "omit a/ and b/ prefixes from filenames"
    complete -c hg -n "__fish_hg_using_command $cmd" -s p -l show-function -d "show which function each change is in"
    complete -c hg -n "__fish_hg_using_command $cmd" -l reverse -d "produce a diff that undoes the changes"
    complete -c hg -n "__fish_hg_using_command $cmd" -s w -l ignore-all-space -d "ignore white space when comparing lines"
    complete -c hg -n "__fish_hg_using_command $cmd" -s b -l ignore-space-change -d "ignore changes in the amount of white space"
    complete -c hg -n "__fish_hg_using_command $cmd" -s B -l ignore-blank-lines -d "ignore changes whose lines are all blank"
    complete -c hg -n "__fish_hg_using_command $cmd" -s Z -l ignore-space-at-eol -d "ignore changes in whitespace at EOL"
    complete -c hg -n "__fish_hg_using_command $cmd" -s U -l unified -x -d "number of lines of context to show"
    complete -c hg -n "__fish_hg_using_command $cmd" -l stat -d "output diffstat-style summary of changes"
    complete -c hg -n "__fish_hg_using_command $cmd" -l root -x -d "produce diffs relative to subdirectory"
    complete -c hg -n "__fish_hg_using_command $cmd" -s I -l include -x -d "include names matching the given patterns"
    complete -c hg -n "__fish_hg_using_command $cmd" -s X -l exclude -x -d "exclude names matching the given patterns"
end

# hg qfinish
for cmd in qfi qfin qfini qfinis qfinish
    complete -c hg -n "__fish_hg_using_command $cmd" -x -a "(__fish_hg_labels; __fish_hg_patches)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s a -l applied -d "finish all applied changesets"
end

# hg qfold
for cmd in qfo qfol qfold
    complete -c hg -n "__fish_hg_using_command $cmd" -x -a "(__fish_hg_patches)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s e -l edit -d "invoke editor on commit messages"
    complete -c hg -n "__fish_hg_using_command $cmd" -s k -l keep -d "keep folded patch files"
    complete -c hg -n "__fish_hg_using_command $cmd" -s m -l message -x -d "use text as commit message"
    complete -c hg -n "__fish_hg_using_command $cmd" -s l -l logfile -r -d "read commit message from file"
end

# hg qgoto
for cmd in qgo qgot qgoto
    complete -c hg -n "__fish_hg_using_command $cmd" -x -a "(__fish_hg_patches)"
    complete -c hg -n "__fish_hg_using_command $cmd" -l keep-changes -d "tolerate non-conflicting local changes"
    complete -c hg -n "__fish_hg_using_command $cmd" -s f -l force -d "overwrite any local changes"
    complete -c hg -n "__fish_hg_using_command $cmd" -l no-backup -d "do not save backup copies of files"
end

# hg qguard
for cmd in qgu qgua qguar qguard
    complete -c hg -n "__fish_hg_using_command $cmd" -x -a "(__fish_hg_patches)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s l -l list -d "list all patches and guards"
    complete -c hg -n "__fish_hg_using_command $cmd" -s n -l none -d "drop all guards"
end

# hg qheader
for cmd in qh qhe qhea qhead qheade qheader
    complete -c hg -n "__fish_hg_using_command $cmd" -x -a "(__fish_hg_patches)"
end

# hg qimport
for cmd in qim qimp qimpo qimpor qimport
    complete -c hg -n "__fish_hg_using_command $cmd" -s e -l existing -d "import file in patch directory"
    complete -c hg -n "__fish_hg_using_command $cmd" -s n -l name -x -d "name of patch file"
    complete -c hg -n "__fish_hg_using_command $cmd" -s f -l force -d "overwrite existing files"
    complete -c hg -n "__fish_hg_using_command $cmd" -s r -l rev -x -a "(__fish_hg_labels)" -d "place existing revisions under mq control"
    complete -c hg -n "__fish_hg_using_command $cmd" -s g -l git -d "use git extended diff format"
    complete -c hg -n "__fish_hg_using_command $cmd" -s P -l push -d "qpush after importing"
end

# hg qnew
complete -c hg -n "__fish_hg_using_command qnew" -s e -l edit -d "invoke editor on commit messages"
complete -c hg -n "__fish_hg_using_command qnew" -s g -l git -d "use git extended diff format"
complete -c hg -n "__fish_hg_using_command qnew" -s U -l currentuser -d "add \"From: <current user>\" to patch"
complete -c hg -n "__fish_hg_using_command qnew" -s u -l user -x -d "add \"From: <USER>\" to patch"
complete -c hg -n "__fish_hg_using_command qnew" -s D -l currentdate -d "add \"Date: <current date>\" to patch"
complete -c hg -n "__fish_hg_using_command qnew" -s d -l date -x -d "add \"Date: <DATE>\" to patch"
complete -c hg -n "__fish_hg_using_command qnew" -s I -l include -x -d "include names matching the given patterns"
complete -c hg -n "__fish_hg_using_command qnew" -s X -l exclude -x -d "exclude names matching the given patterns"
complete -c hg -n "__fish_hg_using_command qnew" -s m -l message -x -d "use text as commit message"
complete -c hg -n "__fish_hg_using_command qnew" -s l -l logfile -r -d "read commit message from file"
complete -c hg -n "__fish_hg_using_command qnew" -s i -l interactive -d "interactively record a new patch"

# hg qnext
for cmd in qnex qnext
    complete -c hg -n "__fish_hg_using_command $cmd" -s s -l summary -d "print first line of patch header"
end

# hg qpop
for cmd in qpo qpop
    complete -c hg -n "__fish_hg_using_command $cmd" -f -a "(__fish_hg_patches)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s a -l all -d "pop all patches"
    complete -c hg -n "__fish_hg_using_command $cmd" -l keep-changes -d "tolerate non-conflicting local changes"
    complete -c hg -n "__fish_hg_using_command $cmd" -s f -l force -d "forget any local changes to patched files"
    complete -c hg -n "__fish_hg_using_command $cmd" -l no-backup -d "do not save backup copies of files"
end

# hg qprev
for cmd in qpr qpre qprev
    complete -c hg -n "__fish_hg_using_command $cmd" -s s -l summary -d "print first line of patch header"
end

# hg qpush
for cmd in qpu qpus qpush
    complete -c hg -n "__fish_hg_using_command $cmd" -f -a "(__fish_hg_patches)"
    complete -c hg -n "__fish_hg_using_command $cmd" -l keep-changes -d "tolerate non-conflicting local changes"
    complete -c hg -n "__fish_hg_using_command $cmd" -s f -l force -d "apply on top of local changes"
    complete -c hg -n "__fish_hg_using_command $cmd" -s e -l exact -d "apply the target patch to its recorded parent"
    complete -c hg -n "__fish_hg_using_command $cmd" -s l -l list -d "list patch name in commit text"
    complete -c hg -n "__fish_hg_using_command $cmd" -s a -l all -d "apply all patches"
    complete -c hg -n "__fish_hg_using_command $cmd" -l move -d "reorder patch series and apply only the patch"
    complete -c hg -n "__fish_hg_using_command $cmd" -l no-backup -d "do not save backup copies of files"
end

# hg qqueue
for cmd in qq qqu qque qqueu qqueue
    complete -c hg -n "__fish_hg_using_command $cmd" -x -a "(__fish_hg_patch_queues)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s l -l list -d "list all available queues"
    complete -c hg -n "__fish_hg_using_command $cmd" -l active -d "print name of active queue"
    complete -c hg -n "__fish_hg_using_command $cmd" -s c -l create -d "create new queue"
    complete -c hg -n "__fish_hg_using_command $cmd" -l rename -d "rename active queue"
    complete -c hg -n "__fish_hg_using_command $cmd" -l delete -d "delete reference to queue"
    complete -c hg -n "__fish_hg_using_command $cmd" -l purge -d "delete queue, and remove patch dir"
end

# hg qrecord
for cmd in qrec qreco qrecor qrecord
    complete -c hg -n "__fish_hg_using_command $cmd" -f -a "(__fish_hg_status -amr)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s e -l edit -d "edit commit message"
    complete -c hg -n "__fish_hg_using_command $cmd" -s g -l git -d "use git extended diff format"
    complete -c hg -n "__fish_hg_using_command $cmd" -s U -l currentuser -d "add \"From: <current user>\" to patch"
    complete -c hg -n "__fish_hg_using_command $cmd" -s u -l user -x -d "add \"From: <USER>\" to patch"
    complete -c hg -n "__fish_hg_using_command $cmd" -s D -l currentdate -d "add \"Date: <current date>\" to patch"
    complete -c hg -n "__fish_hg_using_command $cmd" -s d -l date -x -d "add \"Date: <DATE>\" to patch"
    complete -c hg -n "__fish_hg_using_command $cmd" -s I -l include -x -d "include names matching the given patterns"
    complete -c hg -n "__fish_hg_using_command $cmd" -s X -l exclude -x -d "exclude names matching the given patterns"
    complete -c hg -n "__fish_hg_using_command $cmd" -s m -l message -x -d "use text as commit message"
    complete -c hg -n "__fish_hg_using_command $cmd" -s l -l logfile -x -d "read commit message from file"
    complete -c hg -n "__fish_hg_using_command $cmd" -s w -l ignore-all-space -d "ignore white space when comparing lines"
    complete -c hg -n "__fish_hg_using_command $cmd" -s b -l ignore-space-change -d "ignore changes in the amount of white space"
    complete -c hg -n "__fish_hg_using_command $cmd" -s B -l ignore-blank-lines -d "ignore changes whose lines are all blank"
end

# hg qrefresh
for cmd in qref qrefr qrefre qrefres qrefresh
    complete -c hg -n "__fish_hg_using_command $cmd" -f -a "(__fish_hg_status -amr)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s e -l edit -d "invoke editor on commit messages"
    complete -c hg -n "__fish_hg_using_command $cmd" -s g -l git -d "use git extended diff format"
    complete -c hg -n "__fish_hg_using_command $cmd" -s s -l short -d "refresh only files already in the patch and specified files"
    complete -c hg -n "__fish_hg_using_command $cmd" -s U -l currentuser -d "add/update author field in patch with current user"
    complete -c hg -n "__fish_hg_using_command $cmd" -s u -l user -x -d "add/update author field in patch with given user"
    complete -c hg -n "__fish_hg_using_command $cmd" -s D -l currentdate -d "add/update date field in patch with current date"
    complete -c hg -n "__fish_hg_using_command $cmd" -s d -l date -x -d "add/update date field in patch with given date"
    complete -c hg -n "__fish_hg_using_command $cmd" -s I -l include -x -d "include names matching the given patterns"
    complete -c hg -n "__fish_hg_using_command $cmd" -s X -l exclude -x -d "exclude names matching the given patterns"
    complete -c hg -n "__fish_hg_using_command $cmd" -s m -l message -x -d "use text as commit message"
    complete -c hg -n "__fish_hg_using_command $cmd" -s l -l logfile -r -d "read commit message from file"
    complete -c hg -n "__fish_hg_using_command $cmd" -s i -l interactive -d "interactively select changes to refresh"
end

# hg qrename
for cmd in qren qrena qrenam qrename qm qmv
    complete -c hg -n "__fish_hg_using_command $cmd" -x -a "(__fish_hg_patches)"
end

# hg qselect
for cmd in qsel qsele qselec qselect
    complete -c hg -n "__fish_hg_using_command $cmd" -s n -l none -d "disable all guards"
    complete -c hg -n "__fish_hg_using_command $cmd" -s s -l series -d "list all guards in series file"
    complete -c hg -n "__fish_hg_using_command $cmd" -l pop -d "pop to before first guarded applied patch"
    complete -c hg -n "__fish_hg_using_command $cmd" -l reapply -d "pop, then reapply patches"
end

# hg qseries
for cmd in qser qseri qserie qseries
    complete -c hg -n "__fish_hg_using_command $cmd" -s m -l missing -d "print patches not in series"
    complete -c hg -n "__fish_hg_using_command $cmd" -s s -l summary -d "print first line of patch header"
end

# hg qtop
for cmd in qt qto qtop
    complete -c hg -n "__fish_hg_using_command $cmd" -s s -l summary -d "print first line of patch header"
end

# hg qunapplied
for cmd in qu qun quna qunap qunapp qunappl qunappli qunapplie qunapplied
    complete -c hg -n "__fish_hg_using_command $cmd" -x -a "(__fish_hg_patches)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s 1 -l first -d "show only the preceding applied patch"
    complete -c hg -n "__fish_hg_using_command $cmd" -s s -l summary -d "print first line of patch header"
end

# hg rebase
for cmd in reb reba rebas rebase
    complete -c hg -n "__fish_hg_using_command $cmd" -s s -l source -x -a "(__fish_hg_labels)" -d "rebase the specified changeset and descendants"
    complete -c hg -n "__fish_hg_using_command $cmd" -s b -l base -x -a "(__fish_hg_labels)" -d "rebase everything from branching point of specified changeset"
    complete -c hg -n "__fish_hg_using_command $cmd" -s r -l rev -x -a "(__fish_hg_labels)" -d "rebase these revisions"
    complete -c hg -n "__fish_hg_using_command $cmd" -s d -l dest -x -a "(__fish_hg_labels)" -d "rebase onto the specified changeset"
    complete -c hg -n "__fish_hg_using_command $cmd" -l collapse -d "collapse the rebased changesets"
    complete -c hg -n "__fish_hg_using_command $cmd" -s m -l message -x -d "use text as collapse commit message"
    complete -c hg -n "__fish_hg_using_command $cmd" -s e -l edit -d "invoke editor on commit messages"
    complete -c hg -n "__fish_hg_using_command $cmd" -s l -l logfile -r -d "read collapse commit message from file"
    complete -c hg -n "__fish_hg_using_command $cmd" -s k -l keep -d "keep original changesets"
    complete -c hg -n "__fish_hg_using_command $cmd" -l keepbranches -d "keep original branch names"
    complete -c hg -n "__fish_hg_using_command $cmd" -s t -l tool -x -a "(__fish_hg_merge_tools)" -d "specify merge tool"
    complete -c hg -n "__fish_hg_using_command $cmd" -s c -l continue -d "continue an interrupted rebase"
    complete -c hg -n "__fish_hg_using_command $cmd" -s a -l abort -d "abort an interrupted rebase"
    complete -c hg -n "__fish_hg_using_command $cmd" -s n -l dry-run -d "do not perform actions, just print output"
    complete -c hg -n "__fish_hg_using_command $cmd" -l confirm -d "ask before applying actions"
    complete -c hg -n "__fish_hg_using_command $cmd; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"
end

# hg record
for cmd in recor record
    complete -c hg -n "__fish_hg_using_command $cmd" -f -a "(__fish_hg_status -amr)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s A -l addremove -d "mark new/missing files as added/removed before committing"
    complete -c hg -n "__fish_hg_using_command $cmd" -l close-branch -d "mark a branch as closed, hiding it from the branch list"
    complete -c hg -n "__fish_hg_using_command $cmd" -l amend -d "amend the parent of the working dir"
    complete -c hg -n "__fish_hg_using_command $cmd" -s s -l secret -d "use the secret phase for committing"
    complete -c hg -n "__fish_hg_using_command $cmd" -s I -l include -x -d "include names matching the given patterns"
    complete -c hg -n "__fish_hg_using_command $cmd" -s X -l exclude -x -d "exclude names matching the given patterns"
    complete -c hg -n "__fish_hg_using_command $cmd" -s m -l message -x -d "use text as commit message"
    complete -c hg -n "__fish_hg_using_command $cmd" -s l -l logfile -x -d "read commit message from file"
    complete -c hg -n "__fish_hg_using_command $cmd" -s d -l date -x -d "record the specified date as commit date"
    complete -c hg -n "__fish_hg_using_command $cmd" -s u -l user -x -d "record the specified user as committer"
    complete -c hg -n "__fish_hg_using_command $cmd" -s S -l subrepos -d "recurse into subrepositories"
    complete -c hg -n "__fish_hg_using_command $cmd" -s w -l ignore-all-space -d "ignore white space when comparing lines"
    complete -c hg -n "__fish_hg_using_command $cmd" -s b -l ignore-space-change -d "ignore changes in the amount of white space"
    complete -c hg -n "__fish_hg_using_command $cmd" -s B -l ignore-blank-lines -d "ignore changes whose lines are all blank"
    complete -c hg -n "__fish_hg_using_command $cmd; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"
end

# hg recover
for cmd in recov recove recover
    complete -c hg -n "__fish_hg_using_command $cmd; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"
end

# hg remove
for cmd in rem remo remov remove rm
    complete -c hg -n "__fish_hg_using_command $cmd" -x -a "(__fish_hg_status -c)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s A -l after -d "record delete for missing files"
    complete -c hg -n "__fish_hg_using_command $cmd" -s f -l force -d "forget added files, delete modified files"
    complete -c hg -n "__fish_hg_using_command $cmd" -s S -l subrepos -d "recurse into subrepositories"
    complete -c hg -n "__fish_hg_using_command $cmd" -s I -l include -x -d "include names matching the given patterns"
    complete -c hg -n "__fish_hg_using_command $cmd" -s X -l exclude -x -d "exclude names matching the given patterns"
    complete -c hg -n "__fish_hg_using_command $cmd" -s n -l dry-run -d "do not perform actions, just print output"
    complete -c hg -n "__fish_hg_using_command $cmd; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"
end

# hg rename
for cmd in ren rena renam rename mo mov move mv
    complete -c hg -n "__fish_hg_using_command $cmd" -r -a "(__fish_hg_status -cam)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s A -l after -d "record a rename that has already occurred"
    complete -c hg -n "__fish_hg_using_command $cmd" -s f -l force -d "forcibly copy over an existing managed file"
    complete -c hg -n "__fish_hg_using_command $cmd" -s I -l include -x -d "include names matching the given patterns"
    complete -c hg -n "__fish_hg_using_command $cmd" -s X -l exclude -x -d "exclude names matching the given patterns"
    complete -c hg -n "__fish_hg_using_command $cmd" -s n -l dry-run -d "do not perform actions, just print output"
    complete -c hg -n "__fish_hg_using_command $cmd; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"
end

# hg resolve
for cmd in res reso resol resolv resolve
    complete -c hg -n "__fish_hg_using_command $cmd" -f -a "(__fish_hg_locate 'set:unresolved()')"
    complete -c hg -n "__fish_hg_using_command $cmd" -s a -l all -d "select all unresolved files"
    complete -c hg -n "__fish_hg_using_command $cmd" -s l -l list -d "list state of files needing merge"
    complete -c hg -n "__fish_hg_using_command $cmd" -s m -l mark -x -a "(__fish_hg_locate 'set:unresolved()')" -d "mark files as resolved"
    complete -c hg -n "__fish_hg_using_command $cmd" -s u -l unmark -x -a "(__fish_hg_locate 'set:resolved()')" -d "mark files as unresolved"
    complete -c hg -n "__fish_hg_using_command $cmd" -s n -l no-status -d "hide status prefix"
    complete -c hg -n "__fish_hg_using_command $cmd" -s t -l tool -x -a "(__fish_hg_merge_tools)" -d "specify merge tool"
    complete -c hg -n "__fish_hg_using_command $cmd" -s I -l include -x -d "include names matching the given patterns"
    complete -c hg -n "__fish_hg_using_command $cmd" -s X -l exclude -x -d "exclude names matching the given patterns"
    complete -c hg -n "__fish_hg_using_command $cmd; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"
end

# hg revert
for cmd in rev reve rever revert
    complete -c hg -n "__fish_hg_using_command $cmd" -f -a "(__fish_hg_status -camrd)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s a -l all -d "revert all changes when no arguments given"
    complete -c hg -n "__fish_hg_using_command $cmd" -s d -l date -x -d "tipmost revision matching date"
    complete -c hg -n "__fish_hg_using_command $cmd" -s r -l rev -x -a "(__fish_hg_labels)" -d "revert to the specified revision"
    complete -c hg -n "__fish_hg_using_command $cmd" -s C -l no-backup -d "do not save backup copies of files"
    complete -c hg -n "__fish_hg_using_command $cmd" -s i -l interactive -d "interactively select the changes"
    complete -c hg -n "__fish_hg_using_command $cmd" -s I -l include -x -d "include names matching the given patterns"
    complete -c hg -n "__fish_hg_using_command $cmd" -s X -l exclude -x -d "exclude names matching the given patterns"
    complete -c hg -n "__fish_hg_using_command $cmd" -s n -l dry-run -d "do not perform actions, just print output"
    complete -c hg -n "__fish_hg_using_command $cmd; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"
end

# hg root
for cmd in roo root
    complete -c hg -n "__fish_hg_using_command $cmd; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"
end

# hg serve
for cmd in se ser serv serve
    complete -c hg -n "__fish_hg_using_command $cmd" -s A -l accesslog -r -d "name of access log file to write to"
    complete -c hg -n "__fish_hg_using_command $cmd" -s d -l daemon -d "run server in background"
    complete -c hg -n "__fish_hg_using_command $cmd" -l daemon-postexec -x -d "used internally by daemon mode"
    complete -c hg -n "__fish_hg_using_command $cmd" -s E -l errorlog -r -d "name of error log file to write to"
    complete -c hg -n "__fish_hg_using_command $cmd" -s p -l port -x -d "port to listen on (default: 8000)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s a -l address -x -d "address to listen on (default: all interfaces)"
    complete -c hg -n "__fish_hg_using_command $cmd" -l prefix -x -d "prefix path to serve from (default: server root)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s n -l name -x -d "name to show in web pages (default: working directory)"
    complete -c hg -n "__fish_hg_using_command $cmd" -l web-conf -r -d "name of the hgweb config file (see 'hg help hgweb')"
    complete -c hg -n "__fish_hg_using_command $cmd" -l pid-file -r -d "name of file to write process ID to"
    complete -c hg -n "__fish_hg_using_command $cmd" -l stdio -d "for remote clients"
    complete -c hg -n "__fish_hg_using_command $cmd" -l cmdserver -x -d "for remote clients"
    complete -c hg -n "__fish_hg_using_command $cmd" -s t -l templates -x -d "web templates to use"
    complete -c hg -n "__fish_hg_using_command $cmd" -l style -x -d "template style to use"
    complete -c hg -n "__fish_hg_using_command $cmd" -s 6 -l ipv6 -d "use IPv6 in addition to IPv4"
    complete -c hg -n "__fish_hg_using_command $cmd" -l certificate -r -d "SSL certificate file"
    complete -c hg -n "__fish_hg_using_command $cmd" -l print-url -d "start and print only the URL"
    complete -c hg -n "__fish_hg_using_command $cmd" -s S -l subrepos -d "recurse into subrepositories"
    complete -c hg -n "__fish_hg_using_command $cmd; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"
end

# hg shelve
for cmd in she shel shelv shelve
    complete -c hg -n "__fish_hg_using_command $cmd" -x -a "(__fish_hg_status -amrd)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s A -l addremove -d "mark new/missing files as added/removed before shelving"
    complete -c hg -n "__fish_hg_using_command $cmd" -s u -l unknown -d "store unknown files in the shelve"
    complete -c hg -n "__fish_hg_using_command $cmd" -l cleanup -d "delete all shelved changes"
    complete -c hg -n "__fish_hg_using_command $cmd" -l date -x -d "shelve with the specified commit date"
    complete -c hg -n "__fish_hg_using_command $cmd" -s d -l delete -d "delete the named shelved change(s)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s e -l edit -d "invoke editor on commit messages"
    complete -c hg -n "__fish_hg_using_command $cmd" -s l -l list -d "list current shelves"
    complete -c hg -n "__fish_hg_using_command $cmd" -s m -l message -x -d "use text as shelve message"
    complete -c hg -n "__fish_hg_using_command $cmd" -s n -l name -x -d "use the given name for the shelved commit"
    complete -c hg -n "__fish_hg_using_command $cmd" -s p -l patch -d "output patches for changes (provide the names of the shelved changes as positional arguments)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s i -l interactive -d "interactive mode, only works while creating a shelve"
    complete -c hg -n "__fish_hg_using_command $cmd" -l stat -d "output diffstat-style summary of changes"
    complete -c hg -n "__fish_hg_using_command $cmd" -s I -l include -x -d "include names matching the given patterns"
    complete -c hg -n "__fish_hg_using_command $cmd" -s X -l exclude -x -d "exclude names matching the given patterns"
    complete -c hg -n "__fish_hg_using_command $cmd; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"
end

# hg status
for cmd in st sta stat statu status
    complete -c hg -n "__fish_hg_using_command $cmd" -s A -l all -d "show status of all files"
    complete -c hg -n "__fish_hg_using_command $cmd" -s m -l modified -d "show only modified files"
    complete -c hg -n "__fish_hg_using_command $cmd" -s a -l added -d "show only added files"
    complete -c hg -n "__fish_hg_using_command $cmd" -s r -l removed -d "show only removed files"
    complete -c hg -n "__fish_hg_using_command $cmd" -s d -l deleted -d "show only deleted (but tracked) files"
    complete -c hg -n "__fish_hg_using_command $cmd" -s c -l clean -d "show only files without changes"
    complete -c hg -n "__fish_hg_using_command $cmd" -s u -l unknown -d "show only unknown (not tracked) files"
    complete -c hg -n "__fish_hg_using_command $cmd" -s i -l ignored -d "show only ignored files"
    complete -c hg -n "__fish_hg_using_command $cmd" -s n -l no-status -d "hide status prefix"
    complete -c hg -n "__fish_hg_using_command $cmd" -s C -l copies -d "show source of copied files"
    complete -c hg -n "__fish_hg_using_command $cmd" -s 0 -l print0 -d "end filenames with NUL, for use with xargs"
    complete -c hg -n "__fish_hg_using_command $cmd" -l rev -x -a "(__fish_hg_labels)" -d "show difference from revision"
    complete -c hg -n "__fish_hg_using_command $cmd" -l change -x -a "(__fish_hg_labels)" -d "list the changed files of a revision"
    complete -c hg -n "__fish_hg_using_command $cmd" -s I -l include -x -d "include names matching the given patterns"
    complete -c hg -n "__fish_hg_using_command $cmd" -s X -l exclude -x -d "exclude names matching the given patterns"
    complete -c hg -n "__fish_hg_using_command $cmd" -s S -l subrepos -d "recurse into subrepositories"
    complete -c hg -n "__fish_hg_using_command $cmd; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"
end

# hg strip
for cmd in str stri strip
    complete -c hg -n "__fish_hg_using_command $cmd" -x -a "(__fish_hg_labels)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s r -l rev -x -a "(__fish_hg_labels)" -d "strip specified revision"
    complete -c hg -n "__fish_hg_using_command $cmd" -s f -l force -d "force removal of changesets, discard uncommitted changes (no backup)"
    complete -c hg -n "__fish_hg_using_command $cmd" -l no-backup -d "do not save backup bundle"
    complete -c hg -n "__fish_hg_using_command $cmd" -s k -l keep -d "do not modify working directory during strip"
    complete -c hg -n "__fish_hg_using_command $cmd" -s B -l bookmark -x -a "(__fish_hg_bookmarks)" -d "remove revs only reachable from given bookmark"
    complete -c hg -n "__fish_hg_using_command $cmd; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"
end

# hg summary
for cmd in su sum summ summa summar summary
    complete -c hg -n "__fish_hg_using_command $cmd" -l remote -d "check for push and pull"
    complete -c hg -n "__fish_hg_using_command $cmd; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"
end

# hg tag
complete -c hg -n "__fish_hg_using_command tag" -s f -l force -d "force tag"
complete -c hg -n "__fish_hg_using_command tag" -s l -l local -d "make the tag local"
complete -c hg -n "__fish_hg_using_command tag" -s r -l rev -x -a "(__fish_hg_labels)" -d "revision to tag"
complete -c hg -n "__fish_hg_using_command tag" -l remove -d "remove a tag"
complete -c hg -n "__fish_hg_using_command tag" -s e -l edit -d "invoke editor on commit messages"
complete -c hg -n "__fish_hg_using_command tag" -s m -l message -x -d "use <text> as commit message"
complete -c hg -n "__fish_hg_using_command tag" -s d -l date -x -d "record the specified date as commit date"
complete -c hg -n "__fish_hg_using_command tag" -s u -l user -x -d "record the specified user as committer"
complete -c hg -n "__fish_hg_using_command tag; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"

# hg tags
complete -c hg -n "__fish_hg_using_command tags; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"

# hg unbundle
for cmd in un unb unbu unbun unbund unbundl unbundle
    complete -c hg -n "__fish_hg_using_command $cmd" -s u -l update -d "update to new branch head if changesets were unbundled"
    complete -c hg -n "__fish_hg_using_command $cmd; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"
end

# hg unshelve
for cmd in uns unsh unshe unshel unshelv unshelve
    complete -c hg -n "__fish_hg_using_command $cmd" -x -a "(__fish_hg_shelves)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s a -l abort -d "abort an incomplete unshelve operation"
    complete -c hg -n "__fish_hg_using_command $cmd" -s c -l continue -d "continue an incomplete unshelve operation"
    complete -c hg -n "__fish_hg_using_command $cmd" -s k -l keep -d "keep shelve after unshelving"
    complete -c hg -n "__fish_hg_using_command $cmd" -s n -l name -x -a "(__fish_hg_shelves)" -d "restore shelved change with given name"
    complete -c hg -n "__fish_hg_using_command $cmd" -s t -l tool -x -a "(__fish_hg_merge_tools)" -d "specify merge tool"
    complete -c hg -n "__fish_hg_using_command $cmd; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"
end

# hg update
for cmd in up upd upda updat update che chec check checko checkou checkout co
    complete -c hg -n "__fish_hg_using_command $cmd" -x -a "(__fish_hg_labels)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s C -l clean -d "discard uncommitted changes (no backup)"
    complete -c hg -n "__fish_hg_using_command $cmd" -s c -l check -d "require clean working directory"
    complete -c hg -n "__fish_hg_using_command $cmd" -s m -l merge -d "merge uncommitted changes"
    complete -c hg -n "__fish_hg_using_command $cmd" -s d -l date -x -d "tipmost revision matching date"
    complete -c hg -n "__fish_hg_using_command $cmd" -s r -l rev -x -a "(__fish_hg_labels)" -d revision
    complete -c hg -n "__fish_hg_using_command $cmd" -s t -l tool -x -a "(__fish_hg_merge_tools)" -d "specify merge tool"
    complete -c hg -n "__fish_hg_using_command $cmd; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"
end

# hg verify
for cmd in veri verif verify
    complete -c hg -n "__fish_hg_using_command $cmd; and __fish_hg_mq_enabled" -l mq -d "operate on patch repository"
end
