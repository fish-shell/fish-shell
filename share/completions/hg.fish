function __fish_hg
    set -lx HGPLAIN 1
    command hg $argv ^ /dev/null
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
                printf "$command\ttrack a line of development with movable markers\n"
            case branch
                printf "$command\tset or show the current branch name\n"
            case branches
                printf "$command\tlist repository named branches\n"
            case bundle
                printf "$command\tcreate a changegroup file\n"
            case cat
                printf "$command\toutput the current or given revision of files\n"
            case churn
                printf "$command\thistogram of changes to the repository\n"
            case clone
                printf "$command\tmake a copy of an existing repository\n"
            case commit
                printf "$command\tcommit the specified files or all outstanding changes\n"
            case convert
                printf "$command\tconvert a foreign SCM repository to a Mercurial one\n"
            case copy
                printf "$command\tmark files as copied for the next commit\n"
            case diff
                printf "$command\tdiff repository (or selected files)\n"
            case export
                printf "$command\tdump the header and diffs for one or more changesets\n"
            case extdiff
                printf "$command\tuse external program to diff repository (or selected files)\n"
            case forget
                printf "$command\tforget the specified files on the next commit\n"
            case glog
                printf "$command\tshow revision history alongside an ASCII revision graph\n"
            case graft
                printf "$command\tcopy changes from other branches onto the current branch\n"
            case grep
                printf "$command\tsearch for a pattern in specified files and revisions\n"
            case heads
                printf "$command\tshow branch heads\n"
            case help
                printf "$command\tshow help for a given topic or a help overview\n"
            case histedit
                printf "$command\tinteractively edit changeset history\n"
            case identify
                printf "$command\tidentify the working copy or specified revision\n"
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
                printf "$command\tmerge working directory with another revision\n"
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
            case showconfig
                printf "$command\tshow combined config settings from all hgrc files\n"
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
                printf "$command\tapply one or more changegroup files\n"
            case update
                printf "$command\tupdate working directory (or switch revisions)\n"
            case verify
                printf "$command\tverify the integrity of the repository\n"
            case version
                printf "$command\toutput version and copyright information\n"
            case view
                printf "$command\tstart interactive history viewer\n"
            case '*'
                printf "$command\n"
        end
    end
end

function __fish_hg_labels
    if set -l labels (__fish_hg debuglabelcomplete ^ /dev/null)
        printf "%s\tlabel\n" $labels
    else
        __fish_hg_branches
        __fish_hg_bookmarks
        printf "%s\ttag\n" (__fish_hg tags | cut -d " " -f 1)
    end
end

function __fish_hg_help_topics
    printf "%s\tcommand\n" (__fish_hg debugcomplete)
    printf "%s\thelp topic\n" (__fish_hg help | grep "^ [a-zA-Z]" | cut -d " " -f 2)
end

function __fish_hg_config_entries
    printf "%s\tconfig entry\n" (__fish_hg showconfig | cut -d = -f 1)
end

function __fish_hg_patches
    printf "%s\tpatch\n" (__fish_hg qseries)
end

function __fish_hg_patch_queues
    printf "%s\tpatch queue\n" (__fish_hg qqueue -l | cut -d " " -f 1)
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
    if begin; test (count $bookmarks) -gt 1; or test $bookmarks != "no bookmarks set"; end
        printf "%s\tbookmark\n" (printf "%s\n" $bookmarks | cut -c 4- | cut -d " " -f 1)
    end
end

function __fish_hg_branches
    printf "%s\tbranch\n" (__fish_hg branches | cut -d " " -f 1)
end

function __fish_hg_merge_tools
    for tool in internal:dump internal:fail internal:local internal:merge internal:other internal:prompt
        printf "$tool\tmerge tool\n"
    end
    printf "%s\tmerge tool\n" (__fish_hg showconfig merge-tools | cut -d . -f 2)
end

function __fish_hg_sources
    printf "%s\tsource\n" (__fish_hg paths | cut -d = -f 1)
end

function __fish_hg_mq_enabled
    set -l val (__fish_hg showconfig | grep extensions.hgext.mq)
    if test -z $val
        return 1
    end
    set -l val (echo $val | cut -d = -f 2)
    switch $val
        case "!*"
            return 1
        case "*"
            return 0
    end
end

# global options
complete -c hg -s R -l repository -x -d 'repository root directory or name of overlay bundle file'
complete -c hg -l cwd -x -d 'change working directory'
complete -c hg -s y -l noninteractive -d 'do not prompt, automatically pick the first choice for all prompts'
complete -c hg -s q -l quiet -d 'suppress output'
complete -c hg -s v -l verbose -d 'enable additional output'
complete -c hg -l config -x -a '(__fish_hg_config_entries)' -d 'set/override config option'
complete -c hg -l debug -d 'enable debugging output'
complete -c hg -l debugger -d 'start debugger'
complete -c hg -l encoding -x -d 'set the charset encoding'
complete -c hg -l encodingmode -x -d 'set the charset encoding mode'
complete -c hg -l traceback -d 'always print a traceback on exception'
complete -c hg -l time -d 'time how long the command takes'
complete -c hg -l profile -d 'print command execution profile'
complete -c hg -l version -d 'output version information and exit'
complete -c hg -s h -l help -d 'display help and exit'
complete -c hg -l hidden -d 'consider hidden changesets'
complete -c hg -l color -x -a 'true false always auto never' -d 'when to colorize'
complete -c hg -l pager -x -a 'true false always auto never' -d 'when to paginate'

# subcommands
complete -c hg -n '__fish_use_subcommand' -x -a '(__fish_hg_commands)'

# hg add
complete -c hg -n 'contains add (commandline -poc)' -f -a '(__fish_hg_status -u)'
complete -c hg -n 'contains add (commandline -poc)' -s I -l include -x -d 'include names matching the given patterns'
complete -c hg -n 'contains add (commandline -poc)' -s X -l exclude -x -d 'exclude names matching the given patterns'
complete -c hg -n 'contains add (commandline -poc)' -s S -l subrepos -d 'recurse into subrepositories'
complete -c hg -n 'contains add (commandline -poc)' -s n -l dry-run -d 'do not perform actions, just print output'
complete -c hg -n 'contains add (commandline -poc); and __fish_hg_mq_enabled' -l mq -d 'operate on patch repository'

# hg addremove
complete -c hg -n 'contains addremove (commandline -poc)' -f -a '(__fish_hg_status -ud)'
complete -c hg -n 'contains addremove (commandline -poc)' -s s -l similarity -x -d 'guess renamed files by similarity (0<=s<=100)'
complete -c hg -n 'contains addremove (commandline -poc)' -s I -l include -x -d 'include names matching the given patterns'
complete -c hg -n 'contains addremove (commandline -poc)' -s X -l exclude -x -d 'exclude names matching the given patterns'
complete -c hg -n 'contains addremove (commandline -poc)' -s n -l dry-run -d 'do not perform actions, just print output'
complete -c hg -n 'contains addremove (commandline -poc); and __fish_hg_mq_enabled' -l mq -d 'operate on patch repository'

# hg annotate
complete -c hg -n 'contains annotate (commandline -poc)' -x -a '(__fish_hg_status -cmdr)'
complete -c hg -n 'contains annotate (commandline -poc)' -s r -l rev -x -a '(__fish_hg_labels)' -d 'annotate the specified revision'
complete -c hg -n 'contains annotate (commandline -poc)' -l no-follow -d 'don\'t follow copies and renames'
complete -c hg -n 'contains annotate (commandline -poc)' -s a -l text -d 'treat all files as text'
complete -c hg -n 'contains annotate (commandline -poc)' -s u -l user -d 'list the author (long with -v)'
complete -c hg -n 'contains annotate (commandline -poc)' -s f -l file -d 'list the filename'
complete -c hg -n 'contains annotate (commandline -poc)' -s d -l date -d 'list the date (short with -q)'
complete -c hg -n 'contains annotate (commandline -poc)' -s n -l number -d 'list the revision number (default)'
complete -c hg -n 'contains annotate (commandline -poc)' -s c -l changeset -d 'list the changeset'
complete -c hg -n 'contains annotate (commandline -poc)' -s l -l line-number -d 'show line number at the first appearance'
complete -c hg -n 'contains annotate (commandline -poc)' -s w -l ignore-all-space -d 'ignore white space when comparing lines'
complete -c hg -n 'contains annotate (commandline -poc)' -s b -l ignore-space-change -x -d 'changes in the amount of white space'
complete -c hg -n 'contains annotate (commandline -poc)' -s B -l ignore-blank-lines -d 'ignore changes whose lines are all blank'
complete -c hg -n 'contains annotate (commandline -poc)' -s I -l include -x -d 'include names matching the given patterns'
complete -c hg -n 'contains annotate (commandline -poc)' -s X -l exclude -x -d 'exclude names matching the given patterns'
complete -c hg -n 'contains annotate (commandline -poc); and __fish_hg_mq_enabled' -l mq -d 'operate on patch repository'

# hg archive
complete -c hg -n 'contains archive (commandline -poc)' -l no-decode -d 'do not pass files through decoders'
complete -c hg -n 'contains archive (commandline -poc)' -s p -l prefix -x -d 'directory prefix for files in archive'
complete -c hg -n 'contains archive (commandline -poc)' -s r -l rev -x -a '(__fish_hg_labels)' -d 'revision to distribute'
complete -c hg -n 'contains archive (commandline -poc)' -s t -l type -x -d 'type of distribution to create'
complete -c hg -n 'contains archive (commandline -poc)' -s S -l subrepos -d 'recurse into subrepositories'
complete -c hg -n 'contains archive (commandline -poc)' -s I -l include -x -d 'include names matching the given patterns'
complete -c hg -n 'contains archive (commandline -poc)' -s X -l exclude -x -d 'exclude names matching the given patterns'
complete -c hg -n 'contains archive (commandline -poc); and __fish_hg_mq_enabled' -l mq -d 'operate on patch repository'

# hg backout
complete -c hg -n 'contains backout (commandline -poc)' -x -a '(__fish_hg_labels)'
complete -c hg -n 'contains backout (commandline -poc)' -l merge -d 'merge with old dirstate parent after backout'
complete -c hg -n 'contains backout (commandline -poc)' -s r -l rev -x -a '(__fish_hg_labels)' -d 'revision to backout'
complete -c hg -n 'contains backout (commandline -poc)' -s t -l tool -x -a '(__fish_hg_merge_tools)' -d 'specify merge tool'
complete -c hg -n 'contains backout (commandline -poc)' -s I -l include -x -d 'include names matching the given patterns'
complete -c hg -n 'contains backout (commandline -poc)' -s X -l exclude -x -d 'exclude names matching the given patterns'
complete -c hg -n 'contains backout (commandline -poc)' -s m -l message -x -d 'use text as commit message'
complete -c hg -n 'contains backout (commandline -poc)' -s l -l logfile -x -d 'read commit message from file'
complete -c hg -n 'contains backout (commandline -poc)' -s d -l date -x -d 'record the specified date as commit date'
complete -c hg -n 'contains backout (commandline -poc)' -s u -l user -x -d 'record the specified user as committer'
complete -c hg -n 'contains backout (commandline -poc); and __fish_hg_mq_enabled' -l mq -d 'operate on patch repository'

# hg bisect
complete -c hg -n 'contains bisect (commandline -poc)' -f -a '(__fish_hg_labels)'
complete -c hg -n 'contains bisect (commandline -poc)' -s r -l reset -d 'reset bisect state'
complete -c hg -n 'contains bisect (commandline -poc)' -s g -l good -d 'mark changeset good'
complete -c hg -n 'contains bisect (commandline -poc)' -s b -l bad -d 'mark changeset bad'
complete -c hg -n 'contains bisect (commandline -poc)' -s s -l skip -d 'skip testing changeset'
complete -c hg -n 'contains bisect (commandline -poc)' -s e -l extend -d 'extend the bisect range'
complete -c hg -n 'contains bisect (commandline -poc)' -s c -l command -x -d 'use command to check changeset state'
complete -c hg -n 'contains bisect (commandline -poc)' -s U -l noupdate -d 'do not update to target'
complete -c hg -n 'contains bisect (commandline -poc); and __fish_hg_mq_enabled' -l mq -d 'operate on patch repository'

# hg bookmarks
for cmd in bookmarks bookmark
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -f -a '(__fish_hg_bookmarks)'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s f -l force -d 'force'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s r -l rev -x -a '(__fish_hg_labels)' -d 'revision'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s d -l delete -d 'delete a given bookmark'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s m -l rename -x -a '(__fish_hg_bookmarks)' -d 'rename a given bookmark'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s i -l inactive -d 'mark a bookmark inactive'
    complete -c hg -n 'contains '$cmd' (commandline -poc); and __fish_hg_mq_enabled' -l mq -d 'operate on patch repository'
end

# hg branch
complete -c hg -n 'contains branch (commandline -poc)' -f -a '(__fish_hg_branches)'
complete -c hg -n 'contains branch (commandline -poc)' -s f -l force -x -d 'branch name even if it shadows an existing branch'
complete -c hg -n 'contains branch (commandline -poc)' -s C -l clean -x -d 'branch name to parent branch name'
complete -c hg -n 'contains branch (commandline -poc); and __fish_hg_mq_enabled' -l mq -d 'operate on patch repository'

# hg branches
complete -c hg -n 'contains branches (commandline -poc)' -s a -l active -x -d 'only branches that have unmerged heads'
complete -c hg -n 'contains branches (commandline -poc)' -s c -l closed -x -d 'normal and closed branches'
complete -c hg -n 'contains branches (commandline -poc); and __fish_hg_mq_enabled' -l mq -d 'operate on patch repository'

# hg bundle
complete -c hg -n 'contains bundle (commandline -poc)' -s f -l force -d 'run even when the destination is unrelated'
complete -c hg -n 'contains bundle (commandline -poc)' -s r -l rev -x -a '(__fish_hg_labels)' -d 'a changeset intended to be added to the destination'
complete -c hg -n 'contains bundle (commandline -poc)' -s b -l branch -x -a '(__fish_hg_branches)' -d 'a specific branch you would like to bundle'
complete -c hg -n 'contains bundle (commandline -poc)' -l base -x -a '(__fish_hg_labels)' -d 'a base changeset assumed to be available at the destination'
complete -c hg -n 'contains bundle (commandline -poc)' -s a -l all -d 'bundle all changesets in the repository'
complete -c hg -n 'contains bundle (commandline -poc)' -s t -l type -x -d 'bundle compression type to use (default: bzip2)'
complete -c hg -n 'contains bundle (commandline -poc)' -s e -l ssh -x -d 'specify ssh command to use'
complete -c hg -n 'contains bundle (commandline -poc)' -l remotecmd -x -d 'specify hg command to run on the remote side'
complete -c hg -n 'contains bundle (commandline -poc)' -l insecure -d 'do not verify server certificate (ignoring web.cacerts'
complete -c hg -n 'contains bundle (commandline -poc); and __fish_hg_mq_enabled' -l mq -d 'operate on patch repository'

# hg cat
complete -c hg -n 'contains cat (commandline -poc)' -x -a '(__fish_hg_status -cmrd)'
complete -c hg -n 'contains cat (commandline -poc)' -s o -l output -x -d 'print output to file with formatted name'
complete -c hg -n 'contains cat (commandline -poc)' -s r -l rev -x -a '(__fish_hg_labels)' -d 'print the given revision'
complete -c hg -n 'contains cat (commandline -poc)' -l decode -d 'apply any matching decode filter'
complete -c hg -n 'contains cat (commandline -poc)' -s I -l include -x -d 'include names matching the given patterns'
complete -c hg -n 'contains cat (commandline -poc)' -s X -l exclude -x -d 'exclude names matching the given patterns'
complete -c hg -n 'contains cat (commandline -poc); and __fish_hg_mq_enabled' -l mq -d 'operate on patch repository'

# hg clone
complete -c hg -n 'contains clone (commandline -poc)' -r -a '(__fish_hg_sources)'
complete -c hg -n 'contains clone (commandline -poc)' -s U -l noupdate -d 'the clone will include an empty working copy (only a repository)'
complete -c hg -n 'contains clone (commandline -poc)' -s u -l updaterev -x -d 'revision, tag or branch to check out'
complete -c hg -n 'contains clone (commandline -poc)' -s r -l rev -x -d 'include the specified changeset'
complete -c hg -n 'contains clone (commandline -poc)' -s b -l branch -x -d 'clone only the specified branch'
complete -c hg -n 'contains clone (commandline -poc)' -l pull -d 'use pull protocol to copy metadata'
complete -c hg -n 'contains clone (commandline -poc)' -l uncompressed -d 'use uncompressed transfer (fast over LAN)'
complete -c hg -n 'contains clone (commandline -poc)' -s e -l ssh -x -d 'specify ssh command to use'
complete -c hg -n 'contains clone (commandline -poc)' -l remotecmd -x -d 'specify hg command to run on the remote side'
complete -c hg -n 'contains clone (commandline -poc)' -l insecure -d 'do not verify server certificate (ignoring web.cacerts config)'

# hg commit
for cmd in commit ci
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -f -a '(__fish_hg_status -amr)'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s A -l addremove -d 'mark new/missing files as added/removed before committing'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -l close-branch -d 'mark a branch as closed, hiding it from the branch list'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -l amend -d 'amend the parent of the working directory'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s s -l secret -d 'use the secret phase for committing'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s I -l include -x -d 'include names matching the given patterns'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s X -l exclude -x -d 'exclude names matching the given patterns'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s m -l message -x -d 'use text as commit message'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s l -l logfile -x -d 'read commit message from file'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s d -l date -x -d 'record the specified date as commit date'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s u -l user -x -d 'record the specified user as committer'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s S -l subrepos -d 'recurse into subrepositories'
    complete -c hg -n 'contains '$cmd' (commandline -poc); and __fish_hg_mq_enabled' -l mq -d 'operate on patch repository'
end

# hg copy
for cmd in copy cp
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -x -a '(__fish_hg_status -cmrd)'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s A -l after -d 'record a copy that has already occurred'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s f -l force -d 'forcibly copy over an existing managed file'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s I -l include -x -d 'include names matching the given patterns'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s X -l exclude -x -d 'exclude names matching the given patterns'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s n -l dry-run -d 'do not perform actions, just print output'
    complete -c hg -n 'contains '$cmd' (commandline -poc); and __fish_hg_mq_enabled' -l mq -d 'operate on patch repository'
end

# hg diff
complete -c hg -n 'contains diff (commandline -poc)' -f -a '(__fish_hg_status -m)'
complete -c hg -n 'contains diff (commandline -poc)' -s r -l rev -x -a '(__fish_hg_labels)' -d 'revision'
complete -c hg -n 'contains diff (commandline -poc)' -s c -l change -x -a '(__fish_hg_labels)' -d 'change made by revision'
complete -c hg -n 'contains diff (commandline -poc)' -s a -l text -d 'treat all files as text'
complete -c hg -n 'contains diff (commandline -poc)' -s g -l git -d 'use git extended diff format'
complete -c hg -n 'contains diff (commandline -poc)' -l nodates -d 'omit dates from diff headers'
complete -c hg -n 'contains diff (commandline -poc)' -s p -l show-function -d 'show which function each change is in'
complete -c hg -n 'contains diff (commandline -poc)' -l reverse -d 'produce a diff that undoes the changes'
complete -c hg -n 'contains diff (commandline -poc)' -s w -l ignore-all-space -d 'ignore white space when comparing lines'
complete -c hg -n 'contains diff (commandline -poc)' -s b -l ignore-space-change -x -d 'changes in the amount of white space'
complete -c hg -n 'contains diff (commandline -poc)' -s B -l ignore-blank-lines -d 'ignore changes whose lines are all blank'
complete -c hg -n 'contains diff (commandline -poc)' -s U -l unified -x -d 'number of lines of context to show'
complete -c hg -n 'contains diff (commandline -poc)' -l stat -d 'output diffstat-style summary of changes'
complete -c hg -n 'contains diff (commandline -poc)' -s I -l include -x -d 'include names matching the given patterns'
complete -c hg -n 'contains diff (commandline -poc)' -s X -l exclude -x -d 'exclude names matching the given patterns'
complete -c hg -n 'contains diff (commandline -poc)' -s S -l subrepos -d 'recurse into subrepositories'
complete -c hg -n 'contains diff (commandline -poc); and __fish_hg_mq_enabled' -l mq -d 'operate on patch repository'

# hg export
complete -c hg -n 'contains export (commandline -poc)' -f -a '(__fish_hg_labels)'
complete -c hg -n 'contains export (commandline -poc)' -s o -l output -x -d 'print output to file with formatted name'
complete -c hg -n 'contains export (commandline -poc)' -l switch-parent -x -d 'against the second parent'
complete -c hg -n 'contains export (commandline -poc)' -s r -l rev -x -a '(__fish_hg_labels)' -d 'revisions to export'
complete -c hg -n 'contains export (commandline -poc)' -s a -l text -d 'treat all files as text'
complete -c hg -n 'contains export (commandline -poc)' -s g -l git -d 'use git extended diff format'
complete -c hg -n 'contains export (commandline -poc)' -l nodates -d 'omit dates from diff headers'
complete -c hg -n 'contains export (commandline -poc); and __fish_hg_mq_enabled' -l mq -d 'operate on patch repository'

# hg forget
complete -c hg -n 'contains forget (commandline -poc)' -x -a '(__fish_hg_status -ca)'
complete -c hg -n 'contains forget (commandline -poc)' -s I -l include -x -d 'include names matching the given patterns'
complete -c hg -n 'contains forget (commandline -poc)' -s X -l exclude -x -d 'exclude names matching the given patterns'
complete -c hg -n 'contains forget (commandline -poc); and __fish_hg_mq_enabled' -l mq -d 'operate on patch repository'

# hg graft
complete -c hg -n 'contains graft (commandline -poc)' -x -a '(__fish_hg_labels)'
complete -c hg -n 'contains graft (commandline -poc)' -s r -l rev -x -a '(__fish_hg_labels)' -d 'revisions to graft'
complete -c hg -n 'contains graft (commandline -poc)' -s c -l continue -d 'resume interrupted graft'
complete -c hg -n 'contains graft (commandline -poc)' -s e -l edit -d 'invoke editor on commit messages'
complete -c hg -n 'contains graft (commandline -poc)' -l log -d 'append graft info to log message'
complete -c hg -n 'contains graft (commandline -poc)' -s D -l currentdate -x -d 'the current date as commit date'
complete -c hg -n 'contains graft (commandline -poc)' -s U -l currentuser -x -d 'the current user as committer'
complete -c hg -n 'contains graft (commandline -poc)' -s d -l date -x -d 'record the specified date as commit date'
complete -c hg -n 'contains graft (commandline -poc)' -s u -l user -x -d 'record the specified user as committer'
complete -c hg -n 'contains graft (commandline -poc)' -s t -l tool -x -a '(__fish_hg_merge_tools)' -d 'specify merge tool'
complete -c hg -n 'contains graft (commandline -poc)' -s n -l dry-run -d 'do not perform actions, just print output'
complete -c hg -n 'contains graft (commandline -poc); and __fish_hg_mq_enabled' -l mq -d 'operate on patch repository'

# hg grep
complete -c hg -n 'contains grep (commandline -poc)' -f -a '(__fish_hg_status -cmrd)'
complete -c hg -n 'contains grep (commandline -poc)' -s 0 -l print0 -d 'end fields with NUL'
complete -c hg -n 'contains grep (commandline -poc)' -l all -d 'print all revisions that match'
complete -c hg -n 'contains grep (commandline -poc)' -s a -l text -d 'treat all files as text'
complete -c hg -n 'contains grep (commandline -poc)' -s f -l follow -d 'follow changeset history, or file history across copies and renames'
complete -c hg -n 'contains grep (commandline -poc)' -s i -l ignore-case -d 'ignore case when matching'
complete -c hg -n 'contains grep (commandline -poc)' -s l -l files-with-matches -d 'print only filenames and revisions that match'
complete -c hg -n 'contains grep (commandline -poc)' -s n -l line-number -d 'print matching line numbers'
complete -c hg -n 'contains grep (commandline -poc)' -s r -l rev -x -a '(__fish_hg_labels)' -d 'only search files changed within revision range'
complete -c hg -n 'contains grep (commandline -poc)' -s u -l user -d 'list the author (long with -v)'
complete -c hg -n 'contains grep (commandline -poc)' -s d -l date -d 'list the date (short with -q)'
complete -c hg -n 'contains grep (commandline -poc)' -s I -l include -x -d 'include names matching the given patterns'
complete -c hg -n 'contains grep (commandline -poc)' -s X -l exclude -x -d 'exclude names matching the given patterns'
complete -c hg -n 'contains grep (commandline -poc); and __fish_hg_mq_enabled' -l mq -d 'operate on patch repository'

# hg heads
complete -c hg -n 'contains heads (commandline -poc)' -x -a '(__fish_hg_labels)'
complete -c hg -n 'contains heads (commandline -poc)' -s r -l rev -x -a '(__fish_hg_labels)' -d 'show only heads which are descendants of STARTREV'
complete -c hg -n 'contains heads (commandline -poc)' -s t -l topo -d 'show topological heads only'
complete -c hg -n 'contains heads (commandline -poc)' -s c -l closed -d 'show normal and closed branch heads'
complete -c hg -n 'contains heads (commandline -poc)' -l style -x -d 'display using template map file'
complete -c hg -n 'contains heads (commandline -poc)' -l template -x -d 'display with template'
complete -c hg -n 'contains heads (commandline -poc); and __fish_hg_mq_enabled' -l mq -d 'operate on patch repository'

# hg help
complete -c hg -n 'contains help (commandline -poc)' -x -a '(__fish_hg_help_topics)'
complete -c hg -n 'contains help (commandline -poc)' -s e -l extension -d 'only help for extensions'
complete -c hg -n 'contains help (commandline -poc)' -s c -l command -d 'show only help for commands'
complete -c hg -n 'contains help (commandline -poc)' -s k -l keyword -x -d 'show topics matching keyword'

# hg histedit
complete -c hg -n 'contains histedit (commandline -poc)' -x -a '(__fish_hg_labels)'
complete -c hg -n 'contains histedit (commandline -poc)' -l commands -r -d 'read history edits from the specified file'
complete -c hg -n 'contains histedit (commandline -poc)' -s c -l continue -d 'continue an edit already in progress'
complete -c hg -n 'contains histedit (commandline -poc)' -s k -l keep -d 'don\'t strip old nodes after edit is complete'
complete -c hg -n 'contains histedit (commandline -poc)' -l abort -d 'abort an edit in progress'
complete -c hg -n 'contains histedit (commandline -poc)' -s o -l outgoing -d 'changesets not found in destination'
complete -c hg -n 'contains histedit (commandline -poc)' -s f -l force -d 'force outgoing even for unrelated repositories'
complete -c hg -n 'contains histedit (commandline -poc)' -s r -l rev -x -a '(__fish_hg_labels)' -d 'first revision to be edited'
complete -c hg -n 'contains histedit (commandline -poc); and __fish_hg_mq_enabled' -l mq -d 'operate on patch repository'

# hg identify
for cmd in identify id
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -f -a '(__fish_hg_sources)'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s r -l rev -x -a '(__fish_hg_labels)' -d 'identify the specified revision'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s n -l num -d 'show local revision number'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s i -l id -d 'show global revision id'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s b -l branch -d 'show branch'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s t -l tags -d 'show tags'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s B -l bookmarks -d 'show bookmarks'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s e -l ssh -x -d 'specify ssh command to use'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -l remotecmd -x -d 'specify hg command to run on the remote side'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -l insecure -d 'do not verify server certificate (ignoring web.cacerts config)'
    complete -c hg -n 'contains '$cmd' (commandline -poc); and __fish_hg_mq_enabled' -l mq -d 'operate on patch repository'
end

# hg import
for cmd in import patch
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s p -l strip -x -d 'directory strip option for patch'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s e -l edit -d 'invoke editor on commit messages'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -l no-commit -d 'don\'t commit, just update the working directory'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -l bypass -x -d 'apply patch without touching the working directory'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -l exact -d 'apply patch to the nodes from which it was generated'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -l import-branch -d 'use any branch information in patch (implied by --exact)'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s m -l message -x -d 'use text as commit message'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s l -l logfile -x -d 'read commit message from file'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s d -l date -x -d 'record the specified date as commit date'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s u -l user -x -d 'record the specified user as committer'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s s -l similarity -x -d 'guess renamed files by similarity (0<=s<=100)'
    complete -c hg -n 'contains '$cmd' (commandline -poc); and __fish_hg_mq_enabled' -l mq -d 'operate on patch repository'
end

# hg incoming
for cmd in incoming in
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -f -a '(__fish_hg_sources)'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s f -l force -d 'run even if remote repository is unrelated'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s n -l newest-first -d 'show newest record first'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -l bundle -x -d 'file to store the bundles into'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s r -l rev -x -d 'a remote changeset intended to be added'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s B -l bookmarks -d 'compare bookmarks'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s b -l branch -x -d 'a specific branch you would like to pull'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s p -l patch -d 'show patch'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s g -l git -d 'use git extended diff format'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s l -l limit -x -d 'limit number of changes displayed'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s M -l no-merges -d 'do not show merges'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -l stat -d 'output diffstat-style summary of changes'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s G -l graph -d 'show the revision DAG'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -l style -x -d 'display using template map file'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -l template -x -d 'display with template'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s e -l ssh -x -d 'specify ssh command to use'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -l remotecmd -x -d 'specify hg command to run on the remote side'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -l insecure -d 'do not verify server certificate (ignoring web.cacerts config)'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s S -l subrepos -d 'recurse into subrepositories'
    complete -c hg -n 'contains '$cmd' (commandline -poc); and __fish_hg_mq_enabled' -l mq -d 'operate on patch repository'
end

# hg init
complete -c hg -n 'contains init (commandline -poc)' -s e -l ssh -x -d 'specify ssh command to use'
complete -c hg -n 'contains init (commandline -poc)' -l remotecmd -x -d 'specify hg command to run on the remote side'
complete -c hg -n 'contains init (commandline -poc)' -l insecure -d 'do not verify server certificate (ignoring web.cacerts config)'
complete -c hg -n 'contains init (commandline -poc); and __fish_hg_mq_enabled' -l mq -d 'operate on patch repository'

# hg locate
complete -c hg -n 'contains locate (commandline -poc)' -s r -l rev -x -a '(__fish_hg_labels)' -d 'search the repository as it is in REV'
complete -c hg -n 'contains locate (commandline -poc)' -s 0 -l print0 -d 'end filenames with NUL, for use with xargs'
complete -c hg -n 'contains locate (commandline -poc)' -s f -l fullpath -d 'print complete paths from the filesystem root'
complete -c hg -n 'contains locate (commandline -poc)' -s I -l include -x -d 'include names matching the given patterns'
complete -c hg -n 'contains locate (commandline -poc)' -s X -l exclude -x -d 'exclude names matching the given patterns'
complete -c hg -n 'contains locate (commandline -poc); and __fish_hg_mq_enabled' -l mq -d 'operate on patch repository'

# hg log
for cmd in log glog history
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -f -a '(__fish_hg_status -cmrd)'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s f -l follow -x -d 'follow changeset history, or file history across copies and renames'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s d -l date -x -d 'show revisions matching date spec'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s C -l copies -d 'show copied files'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s k -l keyword -x -d 'do case-insensitive search for a given text'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s r -l rev -x -a '(__fish_hg_labels)' -d 'show the specified revision or range'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -l removed -d 'include revisions where files were removed'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s u -l user -x -d 'revisions committed by user'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s b -l branch -x -a '(__fish_hg_branches)' -d 'show changesets within the given named branch'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s P -l prune -x -a '(__fish_hg_labels)' -d 'do not display revision or any of its ancestors'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s p -l patch -d 'show patch'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s g -l git -d 'use git extended diff format'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s l -l limit -x -d 'limit number of changes displayed'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s M -l no-merges -d 'do not show merges'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -l stat -d 'output diffstat-style summary of changes'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s G -l graph -d 'show the revision DAG'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -l style -x -d 'display using template map file'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -l template -x -d 'display with template'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s I -l include -x -d 'include names matching the given patterns'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s X -l exclude -x -d 'exclude names matching the given patterns'
    complete -c hg -n 'contains '$cmd' (commandline -poc); and __fish_hg_mq_enabled' -l mq -d 'operate on patch repository'
end

# hg manifest
complete -c hg -n 'contains manifest (commandline -poc)' -s r -l rev -x -a '(__fish_hg_labels)' -d 'revision to display'
complete -c hg -n 'contains manifest (commandline -poc)' -l all -d 'list files from all revisions'
complete -c hg -n 'contains manifest (commandline -poc); and __fish_hg_mq_enabled' -l mq -d 'operate on patch repository'

# hg merge
complete -c hg -n 'contains merge (commandline -poc)' -f -a '(__fish_hg_labels)'
complete -c hg -n 'contains merge (commandline -poc)' -s r -l rev -x -a '(__fish_hg_labels)' -d 'revision to merge'
complete -c hg -n 'contains merge (commandline -poc)' -s P -l preview -d 'review revisions to merge (no merge is performed)'
complete -c hg -n 'contains merge (commandline -poc)' -s t -l tool -x -a '(__fish_hg_merge_tools)' -d 'specify merge tool'
complete -c hg -n 'contains merge (commandline -poc); and __fish_hg_mq_enabled' -l mq -d 'operate on patch repository'

# hg outgoing
for cmd in outgoing out
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -f -a '(__fish_hg_sources)'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s f -l force -d 'run even when the destination is unrelated'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s r -l rev -x -a '(__fish_hg_labels)' -d 'a changeset intended to be included in the destination'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s n -l newest-first -d 'show newest record first'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s B -l bookmarks -d 'compare bookmarks'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s b -l branch -x -a '(__fish_hg_branches)' -d 'a specific branch you would like to push'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s p -l patch -d 'show patch'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s g -l git -d 'use git extended diff format'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s l -l limit -x -d 'limit number of changes displayed'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s M -l no-merges -d 'do not show merges'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -l stat -d 'output diffstat-style summary of changes'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s G -l graph -d 'show the revision DAG'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -l style -x -d 'display using template map file'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -l template -x -d 'display with template'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s e -l ssh -x -d 'specify ssh command to use'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -l remotecmd -x -d 'specify hg command to run on the remote side'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -l insecure -d 'do not verify server certificate (ignoring web.cacerts config)'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s S -l subrepos -d 'recurse into subrepositories'
    complete -c hg -n 'contains '$cmd' (commandline -poc); and __fish_hg_mq_enabled' -l mq -d 'operate on patch repository'
end

# hg parents
complete -c hg -n 'contains parents (commandline -poc)' -f -a '(__fish_hg_status -cmrd)'
complete -c hg -n 'contains parents (commandline -poc)' -s r -l rev -x -a '(__fish_hg_labels)' -d 'show parents of the specified revision'
complete -c hg -n 'contains parents (commandline -poc)' -l style -x -d 'display using template map file'
complete -c hg -n 'contains parents (commandline -poc)' -l template -x -d 'display with template'
complete -c hg -n 'contains parents (commandline -poc); and __fish_hg_mq_enabled' -l mq -d 'operate on patch repository'

# hg paths
complete -c hg -n 'contains paths (commandline -poc); and __fish_hg_mq_enabled' -l mq -d 'operate on patch repository'

# hg phase
complete -c hg -n 'contains phase (commandline -poc)' -x -a '(__fish_hg_labels)'
complete -c hg -n 'contains phase (commandline -poc)' -s p -l public -d 'set changeset phase to public'
complete -c hg -n 'contains phase (commandline -poc)' -s d -l draft -d 'set changeset phase to draft'
complete -c hg -n 'contains phase (commandline -poc)' -s s -l secret -d 'set changeset phase to secret'
complete -c hg -n 'contains phase (commandline -poc)' -s f -l force -d 'allow to move boundary backward'
complete -c hg -n 'contains phase (commandline -poc)' -s r -l rev -x -a '(__fish_hg_labels)' -d 'target revision'
complete -c hg -n 'contains phase (commandline -poc); and __fish_hg_mq_enabled' -l mq -d 'operate on patch repository'

# hg pull
complete -c hg -n 'contains pull (commandline -poc)' -r -a '(__fish_hg_sources)'
complete -c hg -n 'contains pull (commandline -poc)' -s u -l update -d 'update to new branch head if changesets were pulled'
complete -c hg -n 'contains pull (commandline -poc)' -s f -l force -d 'run even when remote repository is unrelated'
complete -c hg -n 'contains pull (commandline -poc)' -s r -l rev -x -d 'a remote changeset inteded to be added'
complete -c hg -n 'contains pull (commandline -poc)' -s B -l bookmark -x -d 'bookmark to pull'
complete -c hg -n 'contains pull (commandline -poc)' -s b -l branch -x -d 'a specific branch you would like to pull'
complete -c hg -n 'contains pull (commandline -poc)' -s e -l ssh -x -d 'specify ssh command to use'
complete -c hg -n 'contains pull (commandline -poc)' -l remotecmd -x -d 'specify hg command to run on the remote side'
complete -c hg -n 'contains pull (commandline -poc)' -l insecure -d 'do not verify server certificate (ignoring web.cacerts config)'
complete -c hg -n 'contains pull (commandline -poc); and __fish_hg_mq_enabled' -l mq -d 'operate on patch repository'

# hg push
complete -c hg -n 'contains push (commandline -poc)' -r -a '(__fish_hg_sources)'
complete -c hg -n 'contains push (commandline -poc)' -s f -l force -d 'force push'
complete -c hg -n 'contains push (commandline -poc)' -s r -l rev -x -a '(__fish_hg_labels)' -d 'a changeset intended to be included in the destination'
complete -c hg -n 'contains push (commandline -poc)' -s B -l bookmark -x -a '(__fish_hg_bookmarks)' -d 'bookmark to push'
complete -c hg -n 'contains push (commandline -poc)' -s b -l branch -x -a '(__fish_hg_branches)' -d 'a specific branch you would like to push'
complete -c hg -n 'contains push (commandline -poc)' -l new-branch -d 'allow pushing a new branch'
complete -c hg -n 'contains push (commandline -poc)' -s e -l ssh -x -d 'specify ssh command to use'
complete -c hg -n 'contains push (commandline -poc)' -l remotecmd -x -d 'specify hg command to run on the remote side'
complete -c hg -n 'contains push (commandline -poc)' -l insecure -d 'do not verify server certificate (ignoring web.cacerts config)'
complete -c hg -n 'contains push (commandline -poc); and __fish_hg_mq_enabled' -l mq -d 'operate on patch repository'

# hg qapplied
complete -c hg -n 'contains qapplied (commandline -poc)' -x -a '(__fish_hg_patches)'
complete -c hg -n 'contains qapplied (commandline -poc)' -s 1 -l last -d 'show only the preceding applied patch'
complete -c hg -n 'contains qapplied (commandline -poc)' -s s -l summary -d 'print first line of patch header'

# hg qclone
complete -c hg -n 'contains qclone (commandline -poc)' -r -a '(__fish_hg_sources)'
complete -c hg -n 'contains qclone (commandline -poc)' -l pull -d 'use pull protocol to copy metadata'
complete -c hg -n 'contains qclone (commandline -poc)' -s U -l noupdate -d 'do not update the new working directories'
complete -c hg -n 'contains qclone (commandline -poc)' -l uncompressed -d 'use uncompressed transfer (fast over LAN)'
complete -c hg -n 'contains qclone (commandline -poc)' -s p -l patches -d 'location of source patch repository'
complete -c hg -n 'contains qclone (commandline -poc)' -s e -l ssh -d 'specify ssh command to use'
complete -c hg -n 'contains qclone (commandline -poc)' -l remotecmd -d 'specify hg command to run on the remote side'
complete -c hg -n 'contains qclone (commandline -poc)' -l insecure -d 'do not verify server certificate (ignoring web.cacerts config)'

# hg qdelete
for cmd in qdelete qremove qrm
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -x -a '(__fish_hg_patches)'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s k -l keep -d 'keep patch file'
end

# hg qdiff
complete -c hg -n 'contains qdiff (commandline -poc)' -f -a '(__fish_hg_status -mrd --rev .^)'
complete -c hg -n 'contains qdiff (commandline -poc)' -s a -l text -d 'treat all files as text'
complete -c hg -n 'contains qdiff (commandline -poc)' -s g -l git -d 'use git extended diff format'
complete -c hg -n 'contains qdiff (commandline -poc)' -l nodates -d 'omit dates from diff headers'
complete -c hg -n 'contains qdiff (commandline -poc)' -s p -l show-function -d 'show which function each change is in'
complete -c hg -n 'contains qdiff (commandline -poc)' -l reverse -d 'produce a diff that undoes the changes'
complete -c hg -n 'contains qdiff (commandline -poc)' -s w -l ignore-all-space -d 'ignore white space when comparing lines'
complete -c hg -n 'contains qdiff (commandline -poc)' -s b -l ignore-space-change -d 'ignore changes in the amount of white space'
complete -c hg -n 'contains qdiff (commandline -poc)' -s B -l ignore-blank-lines -d 'ignore changes whose lines are all blank'
complete -c hg -n 'contains qdiff (commandline -poc)' -s U -l unified -d 'number of lines of context to show'
complete -c hg -n 'contains qdiff (commandline -poc)' -l stat -d 'output diffstat-style summary of changes'
complete -c hg -n 'contains qdiff (commandline -poc)' -s I -l include -x -d 'include names matching the given patterns'
complete -c hg -n 'contains qdiff (commandline -poc)' -s X -l exclude -x -d 'exclude names matching the given patterns'

# hg qfinish
complete -c hg -n 'contains qfinish (commandline -poc)' -x -a '(__fish_hg_labels; __fish_hg_patches)'
complete -c hg -n 'contains qfinish (commandline -poc)' -s a -l applied -d 'finish all applied changesets'

# hg qfold
complete -c hg -n 'contains qfold (commandline -poc)' -x -a '(__fish_hg_patches)'
complete -c hg -n 'contains qfold (commandline -poc)' -s e -l edit -d 'edit patch header'
complete -c hg -n 'contains qfold (commandline -poc)' -s k -l keep -d 'keep folded patch files'
complete -c hg -n 'contains qfold (commandline -poc)' -s m -l message -x -d 'use text as commit message'
complete -c hg -n 'contains qfold (commandline -poc)' -s l -l logfile -x -d 'read commit message from file'

# hg qgoto
complete -c hg -n 'contains qgoto (commandline -poc)' -x -a '(__fish_hg_patches)'
complete -c hg -n 'contains qgoto (commandline -poc)' -l keep-changes -d 'tolerate non-conflicting local changes'
complete -c hg -n 'contains qgoto (commandline -poc)' -s f -l force -d 'overwrite any local changes'
complete -c hg -n 'contains qgoto (commandline -poc)' -l no-backup -d 'do not save backup copies of files'

# hg qguard
complete -c hg -n 'contains qguard (commandline -poc)' -x -a '(__fish_hg_patches)'
complete -c hg -n 'contains qguard (commandline -poc)' -s l -l list -d 'all patches and guards'
complete -c hg -n 'contains qguard (commandline -poc)' -s n -l none -d 'drop all guards'

# hg qheader
complete -c hg -n 'contains qheader (commandline -poc)' -x -a '(__fish_hg_patches)'

# hg qimport
complete -c hg -n 'contains qimport (commandline -poc)' -s e -l existing -d 'import file in patch directory'
complete -c hg -n 'contains qimport (commandline -poc)' -s n -l name -d 'name of patch file'
complete -c hg -n 'contains qimport (commandline -poc)' -s f -l force -d 'overwrite existing files'
complete -c hg -n 'contains qimport (commandline -poc)' -s r -l rev -x -a '(__fish_hg_labels)' -d 'place existing revisions under mq control'
complete -c hg -n 'contains qimport (commandline -poc)' -s g -l git -d 'use git extended diff format'
complete -c hg -n 'contains qimport (commandline -poc)' -s P -l push -d 'qpush after importing'

# hg qnew
complete -c hg -n 'contains qnew (commandline -poc)' -s e -l edit -d 'edit commit message'
complete -c hg -n 'contains qnew (commandline -poc)' -s g -l git -d 'use git extended diff format'
complete -c hg -n 'contains qnew (commandline -poc)' -s U -l currentuser -d 'add "From: <current user>" to patch'
complete -c hg -n 'contains qnew (commandline -poc)' -s u -l user -x -d 'add "From: <USER>" to patch'
complete -c hg -n 'contains qnew (commandline -poc)' -s D -l currentdate -d 'add "Date: <current date>" to patch'
complete -c hg -n 'contains qnew (commandline -poc)' -s d -l date -x -d 'add "Date: <DATE>" to patch'
complete -c hg -n 'contains qnew (commandline -poc)' -s I -l include -d 'include names matching the given patterns'
complete -c hg -n 'contains qnew (commandline -poc)' -s X -l exclude -d 'exclude names matching the given patterns'
complete -c hg -n 'contains qnew (commandline -poc)' -s m -l message -d 'use text as commit message'
complete -c hg -n 'contains qnew (commandline -poc)' -s l -l logfile -d 'read commit message from file'
complete -c hg -n 'contains qnew (commandline -poc)' -s i -l interactive -d 'interactively record a new patch'

# hg qnext
complete -c hg -n 'contains qnext (commandline -poc)' -s s -l summary -d 'print first line of patch header'

# hg qpop
complete -c hg -n 'contains qpop (commandline -poc)' -f -a '(__fish_hg_patches)'
complete -c hg -n 'contains qpop (commandline -poc)' -s a -l all -d 'pop all patches'
complete -c hg -n 'contains qpop (commandline -poc)' -l keep-changes -d 'tolerate non-conflicting local changes'
complete -c hg -n 'contains qpop (commandline -poc)' -s f -l force -d 'forget any local changes to patched files'
complete -c hg -n 'contains qpop (commandline -poc)' -l no-backup -d 'do not save backup copies of files'

# hg qprev
complete -c hg -n 'contains qprev (commandline -poc)' -s s -l summary -d 'print first line of patch header'

# hg qpush
complete -c hg -n 'contains qpush (commandline -poc)' -f -a '(__fish_hg_patches)'
complete -c hg -n 'contains qpush (commandline -poc)' -l keep-changes -d 'tolerate non-conflicting local changes'
complete -c hg -n 'contains qpush (commandline -poc)' -s f -l force -d 'apply on top of local changes'
complete -c hg -n 'contains qpush (commandline -poc)' -s e -l exact -d 'apply the target patch to its recorded parent'
complete -c hg -n 'contains qpush (commandline -poc)' -s l -l list -d 'list patch name in commit text'
complete -c hg -n 'contains qpush (commandline -poc)' -s a -l all -d 'apply all patches'
complete -c hg -n 'contains qpush (commandline -poc)' -l move -d 'reorder patch series and apply only the patch'
complete -c hg -n 'contains qpush (commandline -poc)' -l no-backup -d 'do not save backup copies of files'

# hg qqueue
complete -c hg -n 'contains qqueue (commandline -poc)' -x -a '(__fish_hg_patch_queues)'
complete -c hg -n 'contains qqueue (commandline -poc)' -s l -l list -d 'list all available queues'
complete -c hg -n 'contains qqueue (commandline -poc)' -l active -d 'print name of active queue'
complete -c hg -n 'contains qqueue (commandline -poc)' -s c -l create -d 'create new queue'
complete -c hg -n 'contains qqueue (commandline -poc)' -l rename -d 'rename active queue'
complete -c hg -n 'contains qqueue (commandline -poc)' -l delete -d 'delete reference to queue'
complete -c hg -n 'contains qqueue (commandline -poc)' -l purge -d 'delete queue, and remove patch dir'

# hg qrecord
complete -c hg -n 'contains qrecord (commandline -poc)' -f -a '(__fish_hg_status -amr)'
complete -c hg -n 'contains qrecord (commandline -poc)' -s e -l edit -d 'edit commit message'
complete -c hg -n 'contains qrecord (commandline -poc)' -s g -l git -d 'use git extended diff format'
complete -c hg -n 'contains qrecord (commandline -poc)' -s U -l currentuser -d 'add "From: <current user>" to patch'
complete -c hg -n 'contains qrecord (commandline -poc)' -s u -l user -x -d 'add "From: <USER>" to patch'
complete -c hg -n 'contains qrecord (commandline -poc)' -s D -l currentdate -d 'add "Date: <current date>" to patch'
complete -c hg -n 'contains qrecord (commandline -poc)' -s d -l date -x -d 'add "Date: <DATE>" to patch'
complete -c hg -n 'contains qrecord (commandline -poc)' -s I -l include -x -d 'include names matching the given patterns'
complete -c hg -n 'contains qrecord (commandline -poc)' -s X -l exclude -x -d 'exclude names matching the given patterns'
complete -c hg -n 'contains qrecord (commandline -poc)' -s m -l message -x -d 'use text as commit message'
complete -c hg -n 'contains qrecord (commandline -poc)' -s l -l logfile -x -d 'read commit message from file'
complete -c hg -n 'contains qrecord (commandline -poc)' -s w -l ignore-all-space -d 'ignore white space when comparing lines'
complete -c hg -n 'contains qrecord (commandline -poc)' -s b -l ignore-space-change -d 'ignore changes in the amount of white space'
complete -c hg -n 'contains qrecord (commandline -poc)' -s B -l ignore-blank-lines -d 'ignore changes whose lines are all blank'

# hg qrefresh
complete -c hg -n 'contains qrefresh (commandline -poc)' -f -a '(__fish_hg_status -amr)'
complete -c hg -n 'contains qrefresh (commandline -poc)' -s e -l edit -d 'edit commit message'
complete -c hg -n 'contains qrefresh (commandline -poc)' -s g -l git -d 'use git extended diff format'
complete -c hg -n 'contains qrefresh (commandline -poc)' -s s -l short -d 'refresh only files already in the patch and specified files'
complete -c hg -n 'contains qrefresh (commandline -poc)' -s U -l currentuser -d 'add/update author field in patch with current user'
complete -c hg -n 'contains qrefresh (commandline -poc)' -s u -l user -x -d 'add/update author field in patch with given user'
complete -c hg -n 'contains qrefresh (commandline -poc)' -s D -l currentdate -d 'add/update date field in patch with current date'
complete -c hg -n 'contains qrefresh (commandline -poc)' -s d -l date -x -d 'add/update date field in patch with given date'
complete -c hg -n 'contains qrefresh (commandline -poc)' -s I -l include -x -d 'include names matching the given patterns'
complete -c hg -n 'contains qrefresh (commandline -poc)' -s X -l exclude -x -d 'exclude names matching the given patterns'
complete -c hg -n 'contains qrefresh (commandline -poc)' -s m -l message -x -d 'use text as commit message'
complete -c hg -n 'contains qrefresh (commandline -poc)' -s l -l logfile -x -d 'read commit message from file'
complete -c hg -n 'contains qrefresh (commandline -poc)' -s i -l interactive -d 'interactively select changes to refresh'

# hg qrename
for cmd in qrename qmv
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -x -a '(__fish_hg_patches)'
end

# hg qselect
complete -c hg -n 'contains qselect (commandline -poc)' -s n -l none -d 'disable all guards'
complete -c hg -n 'contains qselect (commandline -poc)' -s s -l series -d 'list all guards in series file'
complete -c hg -n 'contains qselect (commandline -poc)' -l pop -d 'pop to before first guarded applied patch'
complete -c hg -n 'contains qselect (commandline -poc)' -l reapply -d 'pop, then reapply patches'

# hg qseries
complete -c hg -n 'contains qseries (commandline -poc)' -s m -l missing -d 'print patches not in series'
complete -c hg -n 'contains qseries (commandline -poc)' -s s -l summary -d 'print first line of patch header'

# hg qtop
complete -c hg -n 'contains qtop (commandline -poc)' -s s -l summary -d 'print first line of patch header'

# hg qunapplied
complete -c hg -n 'contains qunapplied (commandline -poc)' -x -a '(__fish_hg_patches)'
complete -c hg -n 'contains qunapplied (commandline -poc)' -s 1 -l first -d 'show only the preceding applied patch'
complete -c hg -n 'contains qunapplied (commandline -poc)' -s s -l summary -d 'print first line of patch header'

# hg record
complete -c hg -n 'contains record (commandline -poc)' -f -a '(__fish_hg_status -amr)'
complete -c hg -n 'contains record (commandline -poc)' -s A -l addremove -d 'mark new/missing files as added/removed before committing'
complete -c hg -n 'contains record (commandline -poc)' -l close-branch -d 'mark a branch as closed, hiding it from the branch list'
complete -c hg -n 'contains record (commandline -poc)' -l amend -d 'amend the parent of the working dir'
complete -c hg -n 'contains record (commandline -poc)' -s s -l secret -d 'use the secret phase for committing'
complete -c hg -n 'contains record (commandline -poc)' -s I -l include -x -d 'include names matching the given patterns'
complete -c hg -n 'contains record (commandline -poc)' -s X -l exclude -x -d 'exclude names matching the given patterns'
complete -c hg -n 'contains record (commandline -poc)' -s m -l message -x -d 'use text as commit message'
complete -c hg -n 'contains record (commandline -poc)' -s l -l logfile -x -d 'read commit message from file'
complete -c hg -n 'contains record (commandline -poc)' -s d -l date -x -d 'record the specified date as commit date'
complete -c hg -n 'contains record (commandline -poc)' -s u -l user -x -d 'record the specified user as committer'
complete -c hg -n 'contains record (commandline -poc)' -s S -l subrepos -d 'recurse into subrepositories'
complete -c hg -n 'contains record (commandline -poc)' -s w -l ignore-all-space -d 'ignore white space when comparing lines'
complete -c hg -n 'contains record (commandline -poc)' -s b -l ignore-space-change -d 'ignore chnages in the amount of white space'
complete -c hg -n 'contains record (commandline -poc)' -s B -l ignore-blank-lines -d 'ignore changes whose lines are all blank'
complete -c hg -n 'contains record (commandline -poc); and __fish_hg_mq_enabled' -l mq -d 'operate on patch repository'

# hg recover
complete -c hg -n 'contains recover (commandline -poc); and __fish_hg_mq_enabled' -l mq -d 'operate on patch repository'

# hg remove
for cmd in remove rm
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -x -a '(__fish_hg_status -c)'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s A -l after -d 'record delete for missing files'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s f -l force -d 'remove (and delete) file even if added or modified'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s I -l include -x -d 'include names matching the given patterns'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s X -l exclude -x -d 'exclude names matching the given patterns'
    complete -c hg -n 'contains '$cmd' (commandline -poc); and __fish_hg_mq_enabled' -l mq -d 'operate on patch repository'
end

# hg rename
for cmd in rename move mv
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -r -a '(__fish_hg_status -cam)'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s A -l after -d 'record a rename that has already occurred'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s f -l force -d 'forcibly copy over an existing managed file'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s I -l include -x -d 'include names matching the given patterns'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s X -l exclude -x -d 'exclude names matching the given patterns'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s n -l dry-run -d 'do not perform actions, just print output'
    complete -c hg -n 'contains '$cmd' (commandline -poc); and __fish_hg_mq_enabled' -l mq -d 'operate on patch repository'
end

# hg resolve
complete -c hg -n 'contains resolve (commandline -poc)' -f -a '(__fish_hg_locate "set:unresolved()")'
complete -c hg -n 'contains resolve (commandline -poc)' -s a -l all -d 'select all unresolved files'
complete -c hg -n 'contains resolve (commandline -poc)' -s l -l list -d 'list state of files needing merge'
complete -c hg -n 'contains resolve (commandline -poc)' -s m -l mark -x -a '(__fish_hg_locate "set:unresolved()")' -d 'mark files as resolved'
complete -c hg -n 'contains resolve (commandline -poc)' -s u -l unmark -x -a '(__fish_hg_locate "set:resolved()")' -d 'mark files as unresolved'
complete -c hg -n 'contains resolve (commandline -poc)' -s n -l no-status -d 'hide status prefix'
complete -c hg -n 'contains resolve (commandline -poc)' -s t -l tool -x -a '(__fish_hg_merge_tools)' -d 'specify merge tool'
complete -c hg -n 'contains resolve (commandline -poc)' -s I -l include -x -d 'include names matching the given patterns'
complete -c hg -n 'contains resolve (commandline -poc)' -s X -l exclude -x -d 'exclude names matching the given patterns'
complete -c hg -n 'contains resolve (commandline -poc); and __fish_hg_mq_enabled' -l mq -d 'operate on patch repository'

# hg revert
complete -c hg -n 'contains revert (commandline -poc)' -f -a '(__fish_hg_status -camr)'
complete -c hg -n 'contains revert (commandline -poc)' -s a -l all -d 'revert all changes when no arguments given'
complete -c hg -n 'contains revert (commandline -poc)' -s d -l date -x -d 'tipmost revision matching date'
complete -c hg -n 'contains revert (commandline -poc)' -s r -l rev -x -a '(__fish_hg_labels)' -d 'revert to the specified revision'
complete -c hg -n 'contains revert (commandline -poc)' -s C -l no-backup -d 'do not save backup copies of files'
complete -c hg -n 'contains revert (commandline -poc)' -s I -l include -x -d 'include names matching the given patterns'
complete -c hg -n 'contains revert (commandline -poc)' -s X -l exclude -x -d 'exclude names matching the given patterns'
complete -c hg -n 'contains revert (commandline -poc)' -s n -l dry-run -d 'do not perform actions, just print output'
complete -c hg -n 'contains revert (commandline -poc); and __fish_hg_mq_enabled' -l mq -d 'operate on patch repository'

# hg root
complete -c hg -n 'contains root (commandline -poc); and __fish_hg_mq_enabled' -l mq -d 'operate on patch repository'

# hg serve
complete -c hg -n 'contains serve (commandline -poc)' -s A -l accesslog -x -d 'name of access log file to write to'
complete -c hg -n 'contains serve (commandline -poc)' -s d -l daemon -d 'run server in background'
complete -c hg -n 'contains serve (commandline -poc)' -l daemon-pipefds -x -d 'used internally by daemon mode'
complete -c hg -n 'contains serve (commandline -poc)' -s E -l errorlog -x -d 'name of error log file to write to'
complete -c hg -n 'contains serve (commandline -poc)' -s p -l port -x -d 'port to listen on (default: 8000)'
complete -c hg -n 'contains serve (commandline -poc)' -s a -l address -x -d 'address to listen on (default: all interfaces)'
complete -c hg -n 'contains serve (commandline -poc)' -l prefix -x -d 'prefix path to serve from (default: server root)'
complete -c hg -n 'contains serve (commandline -poc)' -s n -l name -x -d 'name to show in web pages (default: working directory)'
complete -c hg -n 'contains serve (commandline -poc)' -l web-conf -x -d 'name of the hgweb config file (see "hg help hgweb")'
complete -c hg -n 'contains serve (commandline -poc)' -l pid-file -x -d 'name of file to write process ID to'
complete -c hg -n 'contains serve (commandline -poc)' -l stdio -d 'for remote clients'
complete -c hg -n 'contains serve (commandline -poc)' -l cmdserver -x -d 'for remote clients'
complete -c hg -n 'contains serve (commandline -poc)' -s t -l templates -x -d 'web templates to use'
complete -c hg -n 'contains serve (commandline -poc)' -l style -x -d 'template style to use'
complete -c hg -n 'contains serve (commandline -poc)' -s 6 -l ipv6 -d 'use IPv6 in addition to IPv4'
complete -c hg -n 'contains serve (commandline -poc)' -l certificate -x -d 'SSL certificate file'
complete -c hg -n 'contains serve (commandline -poc); and __fish_hg_mq_enabled' -l mq -d 'operate on patch repository'

# hg showconfig
complete -c hg -n 'contains showconfig (commandline -poc)' -f -a '(__fish_hg_config_entries)'
complete -c hg -n 'contains showconfig (commandline -poc)' -s u -l untrusted -x -d 'untrusted configuration options'
complete -c hg -n 'contains showconfig (commandline -poc); and __fish_hg_mq_enabled' -l mq -d 'operate on patch repository'

# hg status
for cmd in status st
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s A -l all -d 'show status of all files'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s m -l modified -d 'show only modified files'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s a -l added -d 'show only added files'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s r -l removed -d 'show only removed files'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s d -l deleted -d 'show only deleted (but tracked) files'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s c -l clean -d 'show only files without changes'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s u -l unknown -d 'show only unknown (not tracked) files'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s i -l ignored -d 'show only ignored files'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s n -l no-status -d 'hide status prefix'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s C -l copies -d 'show source of copied files'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s 0 -l print0 -d 'end filenames with NUL, for use with xargs'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -l rev -x -a '(__fish_hg_labels)' -d 'show difference from revision'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -l change -x -a '(__fish_hg_labels)' -d 'list the changed files of a revision'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s I -l include -x -d 'include names matching the given patterns'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s X -l exclude -x -d 'exclude names matching the given patterns'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s S -l subrepos -d 'recurse into subrepositories'
    complete -c hg -n 'contains '$cmd' (commandline -poc); and __fish_hg_mq_enabled' -l mq -d 'operate on patch repository'
end

# hg strip
complete -c hg -n 'contains strip (commandline -poc)' -x -a '(__fish_hg_labels)'
complete -c hg -n 'contains strip (commandline -poc)' -s r -l rev -x -a '(__fish_hg_labels)' -d 'strip specified revision'
complete -c hg -n 'contains strip (commandline -poc)' -s f -l force -d 'force removal of changesets, discard uncommitted changes'
complete -c hg -n 'contains strip (commandline -poc)' -l no-backup -d 'no backups'
complete -c hg -n 'contains strip (commandline -poc)' -s k -l keep -d 'do not modify working copy during strip'
complete -c hg -n 'contains strip (commandline -poc)' -s B -l bookmark -x -a '(__fish_hg_bookmarks)' -d 'remove revs only reachable from given bookmark'
complete -c hg -n 'contains strip (commandline -poc); and __fish_hg_mq_enabled' -l mq -d 'operate on patch repository'

# hg summary
for cmd in summary sum
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -l remote -x -d 'check for push and pull'
    complete -c hg -n 'contains '$cmd' (commandline -poc); and __fish_hg_mq_enabled' -l mq -d 'operate on patch repository'
end

# hg tag
complete -c hg -n 'contains tag (commandline -poc)' -s f -l force -d 'force tag'
complete -c hg -n 'contains tag (commandline -poc)' -s l -l local -d 'make the tag local'
complete -c hg -n 'contains tag (commandline -poc)' -s r -l rev -x -a '(__fish_hg_labels)' -d 'revision to tag'
complete -c hg -n 'contains tag (commandline -poc)' -l remove -d 'remove a tag'
complete -c hg -n 'contains tag (commandline -poc)' -s e -l edit -d 'edit commit message'
complete -c hg -n 'contains tag (commandline -poc)' -s m -l message -x -d 'use <text> as commit message'
complete -c hg -n 'contains tag (commandline -poc)' -s d -l date -x -d 'record the specified date as commit date'
complete -c hg -n 'contains tag (commandline -poc)' -s u -l user -x -d 'record the specified user as committer'
complete -c hg -n 'contains tag (commandline -poc); and __fish_hg_mq_enabled' -l mq -d 'operate on patch repository'

# hg tags
complete -c hg -n 'contains tags (commandline -poc); and __fish_hg_mq_enabled' -l mq -d 'operate on patch repository'

# hg unbundle
complete -c hg -n 'contains unbundle (commandline -poc)' -s u -l update -x -d 'update to new branch head if changesets were unbundled'
complete -c hg -n 'contains unbundle (commandline -poc); and __fish_hg_mq_enabled' -l mq -d 'operate on patch repository'

# hg update
for cmd in update up checkout co
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -x -a '(__fish_hg_labels)'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s C -l clean -d 'discard uncommitted changes (no backup)'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s c -l check -d 'update across branches if no uncommitted changes'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s d -l date -x -d 'tipmost revision matching date'
    complete -c hg -n 'contains '$cmd' (commandline -poc)' -s r -l rev -x -a '(__fish_hg_labels)' -d 'revision'
    complete -c hg -n 'contains '$cmd' (commandline -poc); and __fish_hg_mq_enabled' -l mq -d 'operate on patch repository'
end

# hg verify
complete -c hg -n 'contains verify (commandline -poc); and __fish_hg_mq_enabled' -l mq -d 'operate on patch repository'
