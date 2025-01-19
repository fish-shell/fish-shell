function __fish_complete_svn_diff --description 'Complete "svn diff" arguments'
    set -l cmdl (commandline -cxp | string escape)
    set -l diff diff
    set -l args
    while set -q cmdl[1]
        switch $cmdl[1]
            case --diff-cmd
                if set -q cmdl[2]
                    set diff $cmdl[2]
                    set -e cmd[2]
                end

            case --extensions
                if set -q cmdl[2]
                    set args $cmdl[2]
                    set -e cmdl[2]
                end
        end
        set -e cmdl[1]
    end
    set -l token (commandline -cpt)
    complete -C"$diff $args $token"

end

function _svn_cmpl_ -d 'Make a completion for a subcommand' --argument-names subcommand
    set -e argv[1]
    complete -c svn -n "__fish_seen_subcommand_from $subcommand" $argv
end

#
# subcommands
#
set -l add add
set -l blame 'blame ann annotate praise'
set -l cat cat
set -l changelist 'cl changelist'
set -l checkout 'co checkout'
set -l cleanup cleanup
set -l commit 'ci commit'
set -l copy 'cp copy'
set -l diff 'di diff'
set -l export export
set -l help '\? h help'
set -l import import
set -l info info
set -l list 'ls list'
set -l lock lock
set -l log log
set -l merge merge
set -l mergeinfo mergeinfo
set -l mkdir mkdir
set -l move 'mv move ren rename'
set -l patch patch
set -l propdel 'pd pdel propdel'
set -l propedit 'pe pedit propedit'
set -l propget 'pg pget propget'
set -l proplist 'pl plist proplist'
set -l propset 'ps pset propset'
set -l relocate relocate
set -l remove 'rm remove del delete'
set -l resolve resolve
set -l resolved resolved
set -l revert revert
set -l stat 'st stat status'
set -l switch 'sw switch'
set -l unlock unlock
set -l update 'up update'
set -l upgrade upgrade

complete -c svn -n __fish_use_subcommand -x -a $add -d 'Put files under version control, scheduling them for addition in the next commit'
complete -c svn -n __fish_use_subcommand -x -a $blame -d 'Output the content of specified files or URLs with revision and author information in-line'
complete -c svn -n __fish_use_subcommand -x -a $cat -d 'Output the content of specified files or URLs'
complete -c svn -n __fish_use_subcommand -x -a $changelist -d 'Output the content of specified files or URLs with revision and author information in-line'
complete -c svn -n __fish_use_subcommand -x -a $checkout -d 'Check out a working copy from a repository'
complete -c svn -n __fish_use_subcommand -x -a $cleanup -d 'Recursively clean up the working copy, removing locks, resuming unfinished operations, etc'
complete -c svn -n __fish_use_subcommand -x -a $commit -d 'Send changes from your working copy to the repository'
complete -c svn -n __fish_use_subcommand -x -a $copy -d 'Duplicate something in working copy or repository, remembering history'
complete -c svn -n __fish_use_subcommand -x -a $diff -d 'Display the differences between two revisions or paths'
complete -c svn -n __fish_use_subcommand -x -a $export -d 'Create an unversioned copy of a tree'
complete -c svn -n __fish_use_subcommand -x -a $help -d 'Describe the usage of this program or its subcommands'
complete -c svn -n __fish_use_subcommand -x -a $import -d 'Commit an unversioned file or tree into the repository'
complete -c svn -n __fish_use_subcommand -x -a $info -d 'Display information about a local or remote item'
complete -c svn -n __fish_use_subcommand -x -a $list -d 'List directory entries in the repository'
complete -c svn -n __fish_use_subcommand -x -a $lock -d 'Lock working copy paths/URLs in the repo, so that no other user can commit changes to them'
complete -c svn -n __fish_use_subcommand -x -a $log -d 'Show the log messages for a set of revision(s) and/or file(s)'
complete -c svn -n __fish_use_subcommand -x -a $merge -d 'Apply the differences between two sources to a working copy path'
complete -c svn -n __fish_use_subcommand -x -a $mergeinfo -d 'Display information related to merges'
complete -c svn -n __fish_use_subcommand -x -a $mkdir -d 'Create a new directory under version control'
complete -c svn -n __fish_use_subcommand -x -a $move -d 'Move and/or rename something in working copy or repository'
complete -c svn -n __fish_use_subcommand -x -a $patch -d 'Apply a unidiff patch to the working copy'
complete -c svn -n __fish_use_subcommand -x -a $propdel -d 'Remove a property from files, dirs, or revisions'
complete -c svn -n __fish_use_subcommand -x -a $propedit -d 'Edit a property with an external editor'
complete -c svn -n __fish_use_subcommand -x -a $propget -d 'Print the value of a property on files, dirs, or revisions'
complete -c svn -n __fish_use_subcommand -x -a $proplist -d 'List all properties on files, dirs, or revisions'
complete -c svn -n __fish_use_subcommand -x -a $propset -d 'Set the value of a property on files, dirs, or revisions'
complete -c svn -n __fish_use_subcommand -x -a $relocate -d 'Rewrite working copy url metadata'
complete -c svn -n __fish_use_subcommand -x -a $remove -d 'Remove files and directories from version control'
complete -c svn -n __fish_use_subcommand -x -a $resolve -d 'Remove conflicts on working copy files or directories'
complete -c svn -n __fish_use_subcommand -x -a $resolved -d 'Remove \'conflicted\' state on working copy files or directories'
complete -c svn -n __fish_use_subcommand -x -a $revert -d 'Restore pristine working copy file (undo most local edits)'
complete -c svn -n __fish_use_subcommand -x -a $stat -d 'Print the status of working copy files and directories'
complete -c svn -n __fish_use_subcommand -x -a $switch -d 'Update the working copy to a different URL'
complete -c svn -n __fish_use_subcommand -x -a $unlock -d 'Unlock working copy paths or URLs'
complete -c svn -n __fish_use_subcommand -x -a $update -d 'Bring changes from the repository into the working copy'
complete -c svn -n __fish_use_subcommand -x -a $upgrade -d 'Upgrade the metadata storage format for a working copy'

#
# Global commands
#
complete -c svn -n 'not __fish_use_subcommand' -l username -x -d 'Specify a username ARG'
complete -c svn -n 'not __fish_use_subcommand' -l password -x -d 'Specify a password ARG'
complete -c svn -n 'not __fish_use_subcommand' -l no-auth-cache -d 'Do not cache authentication tokens'
complete -c svn -n 'not __fish_use_subcommand' -l non-interactive -d 'Do no interactive prompting'
complete -c svn -n 'not __fish_use_subcommand' -l trust-server-cert -d 'Accept SSL server certificates from unknown authorities (ony with --non-interactive)'
complete -c svn -n 'not __fish_use_subcommand' -l config-dir -x -d 'Read user configuration files from directory ARG'
complete -c svn -n 'not __fish_use_subcommand' -l config-option -x -d 'Set user configuration option in the format: FILE:SECTION:OPTION=[VALUE]'

#
# local commands
#
for cmd in $commit $copy $import $lock $mkdir $move $propedit $remove
    if not test $cmd = lock
        _svn_cmpl_ $cmd -l editor-cmd -x -d 'Use ARG as external editor'
    end
    _svn_cmpl_ $cmd -l message -s m -d 'Specify log message'
    _svn_cmpl_ $cmd -l encoding -x -d 'Treat value as being in charset encoding ARG'
    _svn_cmpl_ $cmd -l file -s F -d 'Read log message from file'
    _svn_cmpl_ $cmd -l force-log -d 'Force validity of log message source'
end

for cmd in $merge $resolve $switch $update
    _svn_cmpl_ $cmd -l accept -d 'Specify automatic conflict resolution source' -xa 'base working mine-conflict theirs-conflict mine-full theirs-full'
end

for cmd in $add $changelist $checkout $commit $diff $export $import $info $list $log $merge $mergeinfo $propdel $propget $proplist $propset $resolve $resolved $revert $stat $switch $update
    _svn_cmpl_ $cmd -l depth -d 'Limit operation by depth' -xa 'empty files immediates infinity'
end

for cmd in $add $changelist $checkout $commit $copy $delete $elog $export $import $merge $mkdir $move $patch $propdel $proplist $propset $resolve $resolved $revert $stat $switch $update $upgrade
    _svn_cmpl_ $cmd -l quiet -s q -d 'Print nothing, or only summary information'
end

for cmd in $add $blame $checkout $delete $diff $export $import $lock $merge $move $propedit $propset $remove $switch $unlock $update
    _svn_cmpl_ $cmd -l force -d 'Force operation to run'
end

for cmd in $add $changelist $commit $delete $info $lock $log $propset $resolve $resolved $revert $unlock
    _svn_cmpl_ $cmd -l targets -d 'Process contents of file ARG as additional args' -r
end

for cmd in $changelist $commit $diff $info $propdel $propget $proplist $propset $revert $stat $update
    _svn_cmpl_ $cmd -l changelist -l cl -d 'Operate only on members of changelist' -r
end

for cmd in $blame $diff $info $list $log $propget $proplist $stat
    _svn_cmpl_ $cmd -l xml -d 'Output in xml'
end

for cmd in $commit $copy $delete $import $log $mkdir $move $propedit
    _svn_cmpl_ $cmd -l with-revprop -d 'Retrieve revision property' -x
end

for cmd in $diff $log $merge
    _svn_cmpl_ $cmd -l change -s c -d 'The change made in revision ARG' -xa '(__fish_print_svn_rev)'
end

for cmd in $blame $cat $checkout $copy $diff $export $info $list $log $merge $mergeinfo $move $propedit $propget $propdel $proplist $propset $switch $update
    _svn_cmpl_ $cmd -l revision -s r -d 'Which revision the target is first looked up' -xa '(__fish_print_svn_rev)'
end

for cmd in $checkout $copy $export $relocate $stat $switch $update
    _svn_cmpl_ $cmd -l ignore-externals -d 'Ignore externals definitions'
end

for cmd in $blame $list $log $propget $proplist $stat
    _svn_cmpl_ $cmd -l verbose -s v -d 'Print extra information'
end

for cmd in $propdel $propedit $propget $proplist $propset
    _svn_cmpl_ $cmd -l revprop -d 'Operate on a revision property (use with -r)'
end

for cmd in $add $copy $mkdir $move $update
    _svn_cmpl_ $cmd -l parents -d 'Add intermediate parents'
end

for cmd in $blame $diff $log $merge
    _svn_cmpl_ $cmd -l extensions -s x -d 'Output 3 lines of unified context' -xa '-u --unified'
    _svn_cmpl_ $cmd -l extensions -s x -d 'Ignore changes in amount of whitespace' -xa '-b --ignore-space-change'
    _svn_cmpl_ $cmd -l extensions -s x -d 'Ignore all whitespace' -xa '-w --ignore-all-space'
    _svn_cmpl_ $cmd -l extensions -s x -d 'Ignore eol style' -xa '-w --ignore-eol-style'
    _svn_cmpl_ $cmd -l extensions -s x -d 'Show C function name' -xa '-p --show-c-function'
    _svn_cmpl_ $cmd -l extensions -n '__fish_seen_subcommand_from --diff-cmd' -xa '(__fish_complete_svn_diff)'
end

for cmd in $cleanup $merge $switch $update
    _svn_cmpl_ $cmd -l diff3-cmd -d 'Use as merge command' -xa "(complete -C '')"
end

for cmd in $blame $info $list $log $stat
    _svn_cmpl_ $cmd -l incremental -d 'Give output suitable for concatenation'
end

for cmd in $add $import $stat
    _svn_cmpl_ $cmd -l no-ignore -d 'Disregard default and svn:ignore property ignores'
end

for cmd in $merge $patch
    _svn_cmpl_ $cmd -l dry-run -d 'Try operation but make no changes'
end

for cmd in $merge $switch
    _svn_cmpl_ $cmd -l ignore-ancestry -d 'Ignore ancestry when calculating merges'
end

for cmd in $diff $log
    _svn_cmpl_ $cmd -l internal-diff -d 'Override diff-cmd specified in config file'
    _svn_cmpl_ $cmd -l diff-cmd -d 'Use external diff command' -xa "(complete -C '')"
end

for cmd in $add $import
    _svn_cmpl_ $cmd -l no-auto-props -d 'Disable automatic properties'
end

for cmd in $switch $update
    _svn_cmpl_ $cmd -l set-depth -d 'Set new working copy depth' -xa 'exclude empty files immediates infinity'
end

for cmd in $blame $log
    _svn_cmpl_ $cmd -l use-merge-history -s g -d 'Use/display additional information from merge history'
end

#
# Individual commands
#

#
# Completions for the 'checkout', 'co' subcommands
#
_svn_cmpl_ $checkout -xa 'http:// ftp:// svn+ssh:// svn+ssh://(__fish_print_hostnames)'

#
# Completions for the 'changelist', 'cl' subcommands
#
_svn_cmpl_ $changelist -l remove -d 'Remove changelist association'

#
# Completions for the 'commit', 'ci' subcommands
#
_svn_cmpl_ $commit -l keep-changelists -d 'don\'t delete changelists after commit'
_svn_cmpl_ $commit -l no-unlock -d 'Don\'t unlock the targets'

#
# Completions for the 'remove', 'rm', 'delete', 'del' subcommands
#
_svn_cmpl_ $remove -l keep-local -x -d 'Keep path in working copy'

#
# Completions for the 'diff', 'di' subcommands
#
_svn_cmpl_ $diff -l old -d 'Use ARG as the older target' -xa '(__fish_print_svn_rev)'
_svn_cmpl_ $diff -l new -d 'Use ARG as the newer target' -xa '(__fish_print_svn_rev)'
_svn_cmpl_ $diff -l no-diff-deleted -d 'Do not print differences for deleted files'
_svn_cmpl_ $diff -l notice-ancestry -d 'Notice ancestry when calculating differences'
_svn_cmpl_ $diff -l summarize -d 'Show a summary of the results'
_svn_cmpl_ $diff -l git -d 'Use git\'s extended diff format'
_svn_cmpl_ $diff -l show-copies-as-adds -d 'Don\'t diff copied or moved files with their source'

#
# Completions for the 'export' subcommand
#
_svn_cmpl_ $export -l native-eol -x -d 'Use a different EOL marker than the standard'

#
# Completions for the 'log' subcommand
#
_svn_cmpl_ $log -l stop-on-copy -d 'Do not cross copies while traversing history'
_svn_cmpl_ $log -l limit -s l -x -d 'Maximum number of log entries'
_svn_cmpl_ $log -l diff -d 'Produce diff output'
_svn_cmpl_ $log -l with-all-revprops -d 'Retrieve all revision properties'
_svn_cmpl_ $log -l with-no-revprops -d 'Retrieve no revision properties'

#
# Completions for the 'merge' subcommand
#
_svn_cmpl_ $merge -l allow-mixed-variables -d 'Allow merge into mixed-revision working copy'
_svn_cmpl_ $merge -l record-only -d 'Merge only mergeinfo differences'
_svn_cmpl_ $merge -l reintegrate -d 'Merge a branch back into its parent branch'

#
# Completions for the 'patch' subcommand
#
_svn_cmpl_ $patch -l reverse-diff -d 'Apply the unidiff in reverse'
_svn_cmpl_ $patch -l strip -x -d 'Number of leading path components to strip from paths'

#
# Completions for the 'propget', 'pget', 'pg' subcommands
#
_svn_cmpl_ $propget -l strict -d 'Use strict semantics'

#
# Complete pget, pset, pedit options with svn props
#
set -l props svn:{ignore,keywords,executable,eol-style,mime-type,externals,need-lock}
_svn_cmpl_ $propget -a "$props"
_svn_cmpl_ $propedit -a "$props"
_svn_cmpl_ $propdel -a "$props"
_svn_cmpl_ $propset -a "$props"

_svn_cmpl_ svn:eol-style -a "native LF CR CRLF"
_svn_cmpl_ svn:mime-type -a "(__fish_print_xdg_mimetypes)"
_svn_cmpl_ svn:keywords -a URL -d 'The URL for the head version of the object'
_svn_cmpl_ svn:keywords -a Author -d 'The last person to modify the file'
_svn_cmpl_ svn:keywords -a Date -d 'Last changed date'
_svn_cmpl_ svn:keywords -a Rev -d 'The last revision the object changed'
_svn_cmpl_ svn:keywords -a Id -d 'A compressed summary of all keywords'

#
# Completions for the 'relocate' subcommand
#
_svn_cmpl_ $relocate -xa '( svn info | string match "*URL:*" | string split -f2 " ") http:// ftp:// svn+ssh:// svn+ssh://(__fish_print_hostnames)'

#
# Completions for the 'switch', 'sw' subcommands
#
_svn_cmpl_ $switch -l relocate -d 'Relocate via URL-rewriting' -xa '( svn info | string match "*URL:*" | string split -f2 " ") http:// ftp:// svn+ssh:// svn+ssh://(__fish_print_hostnames)'

#
# Completions for the 'status', 'st' subcommands
#
_svn_cmpl_ $stat -l show-updates -s u -d 'Display update information'

#
# Completions for the 'update', 'up' subcommand
#
_svn_cmpl_ $update -l editor-cmd -x -d 'Use ARG as external editor'

functions -e _svn_cmpl_
