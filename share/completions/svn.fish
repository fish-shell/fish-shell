function _svn_cmpl_ -d 'Make a completion for a subcommand' --no-scope-shadowing --argument-names subcommand
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

complete -c svn -n '__fish_use_subcommand' -x -a $add --description 'Put files and directories under version control, scheduling them for addition to repository.  They will be added in next commit.'
complete -c svn -n '__fish_use_subcommand' -x -a $blame --description 'Output the content of specified files or URLs with revision and author information in-line.'
complete -c svn -n '__fish_use_subcommand' -x -a $cat --description 'Output the content of specified files or URLs.'
complete -c svn -n '__fish_use_subcommand' -x -a $changelist --description 'Output the content of specified files or URLs with revision and author information in-line.'
complete -c svn -n '__fish_use_subcommand' -x -a $checkout --description 'Check out a working copy from a repository.'
complete -c svn -n '__fish_use_subcommand' -x -a $cleanup --description 'Recursively clean up the working copy, removing locks, resuming unfinished operations, etc.'
complete -c svn -n '__fish_use_subcommand' -x -a $commit --description 'Send changes from your working copy to the repository.'
complete -c svn -n '__fish_use_subcommand' -x -a $copy --description 'Duplicate something in working copy or repository, remembering history.'
complete -c svn -n '__fish_use_subcommand' -x -a $diff --description 'Display the differences between two revisions or paths.'
complete -c svn -n '__fish_use_subcommand' -x -a $export --description 'Create an unversioned copy of a tree.'
complete -c svn -n '__fish_use_subcommand' -x -a $help --description 'Describe the usage of this program or its subcommands.'
complete -c svn -n '__fish_use_subcommand' -x -a $import --description 'Commit an unversioned file or tree into the repository.'
complete -c svn -n '__fish_use_subcommand' -x -a $info --description 'Display information about a local or remote item.'
complete -c svn -n '__fish_use_subcommand' -x -a $list --description 'List directory entries in the repository.'
complete -c svn -n '__fish_use_subcommand' -x -a $lock --description 'Lock working copy paths or URLs in the repository, so that no other user can commit changes to them.'
complete -c svn -n '__fish_use_subcommand' -x -a $log --description 'Show the log messages for a set of revision(s) and/or file(s).'
complete -c svn -n '__fish_use_subcommand' -x -a $merge --description 'Apply the differences between two sources to a working copy path.'
complete -c svn -n '__fish_use_subcommand' -x -a $mergeinfo --description 'Display information related to merges'
complete -c svn -n '__fish_use_subcommand' -x -a $mkdir --description 'Create a new directory under version control.'
complete -c svn -n '__fish_use_subcommand' -x -a $move --description 'Move and/or rename something in working copy or repository.'
complete -c svn -n '__fish_use_subcommand' -x -a $patch --description 'Apply a unidiff patch to the working copy'
complete -c svn -n '__fish_use_subcommand' -x -a $propdel --description 'Remove a property from files, dirs, or revisions.'
complete -c svn -n '__fish_use_subcommand' -x -a $propedit --description 'Edit a property with an external editor.'
complete -c svn -n '__fish_use_subcommand' -x -a $propget --description 'Print the value of a property on files, dirs, or revisions.'
complete -c svn -n '__fish_use_subcommand' -x -a $proplist --description 'List all properties on files, dirs, or revisions.'
complete -c svn -n '__fish_use_subcommand' -x -a $propset --description 'Set the value of a property on files, dirs, or revisions.'
complete -c svn -n '__fish_use_subcommand' -x -a $relocate --description 'Rewrite working copy url metadata'
complete -c svn -n '__fish_use_subcommand' -x -a $remove --description 'Remove files and directories from version control.'
complete -c svn -n '__fish_use_subcommand' -x -a $resolve --description 'Remove conflicts on working copy files or directories.'
complete -c svn -n '__fish_use_subcommand' -x -a $resolved --description 'Remove \'conflicted\' state on working copy files or directories.'
complete -c svn -n '__fish_use_subcommand' -x -a $revert --description 'Restore pristine working copy file (undo most local edits).'
complete -c svn -n '__fish_use_subcommand' -x -a $stat --description 'Print the status of working copy files and directories.'
complete -c svn -n '__fish_use_subcommand' -x -a $switch --description 'Update the working copy to a different URL.'
complete -c svn -n '__fish_use_subcommand' -x -a $unlock --description 'Unlock working copy paths or URLs.'
complete -c svn -n '__fish_use_subcommand' -x -a $update --description 'Bring changes from the repository into the working copy.'
complete -c svn -n '__fish_use_subcommand' -x -a $upgrade --description 'Upgrade the metadata storage format for a working copy.'

#
# Global commands
#
complete -c svn -n 'not __fish_use_subcommand' -l username -x --description 'Specify a username ARG'
complete -c svn -n 'not __fish_use_subcommand' -l password -x --description 'Specify a password ARG'
complete -c svn -n 'not __fish_use_subcommand' -l no-auth-cache --description 'Do not cache authentication tokens'
complete -c svn -n 'not __fish_use_subcommand' -l non-interactive --description 'Do no interactive prompting'
complete -c svn -n 'not __fish_use_subcommand' -l trust-server-cert --description 'Accept SSL server certificates from unknown authorities (ony with --non-interactive)'
complete -c svn -n 'not __fish_use_subcommand' -l config-dir -x --description 'Read user configuration files from directory ARG'
complete -c svn -n 'not __fish_use_subcommand' -l config-option -x --description 'Set user configuration option in the format: FILE:SECTION:OPTION=[VALUE]'

#
# local commands
#
for cmd in $commit $copy $import $lock $mkdir $move $propedit $remove
    if not test $cmd = lock
        _svn_cmpl_ $cmd -l editor-cmd -x --description 'Use ARG as external editor'
    end
    _svn_cmpl_ $cmd -l message -s m --description 'Specify log message'
    _svn_cmpl_ $cmd -l encoding -x --description 'Treat value as being in charset encoding ARG'
    _svn_cmpl_ $cmd -l file -s F --description 'Read log message from file'
    _svn_cmpl_ $cmd -l force-log --description 'Force validity of log message source'
end

for cmd in $merge $resolve $switch $update
    _svn_cmpl_ $cmd -l accept -d 'Specify automatic conflict resolution source' -xa 'base working mine-conflict theirs-conflict mine-full theirs-full'
end

for cmd in $add $changelist $checkout $commit $diff $export $import $info $list $log $merge $mergeinfo $propdel $propget $proplist $propset $resolve $resolved $revert $stat $switch $update
    _svn_cmpl_ $cmd -l depth -d 'Limit operation by depth' -xa 'empty files immediates infinity'
end

for cmd in $add $changelist $checkout $commit $copy $delete $elog $export $import $merge $mkdir $move $patch $propdel $proplist $propset $resolve $resolved $revert $stat $switch $update $upgrade
    _svn_cmpl_ $cmd -l quiet -s q --description 'Print nothing, or only summary information'
end

for cmd in $add $blame $checkout $delete $diff $export $import $lock $merge $move $propedit $propset $remove $switch $unlock $update
    _svn_cmpl_ $cmd -l force --description 'Force operation to run'
end

for cmd in $add $changelist $commit $delete $info $lock $log $propset $resolve $resolved $revert $unlock
    _svn_cmpl_ $cmd -l targets --description 'Process contents of file ARG as additional args' -r
end

for cmd in $changelist $commit $diff $info $propdel $propget $proplist $propset $revert $stat $update
    _svn_cmpl_ $cmd -l changelist -l cl --description 'Operate only on members of changelist' -r
end

for cmd in $blame $diff $info $list $log $propget $proplist $stat
    _svn_cmpl_ $cmd -l xml --description 'Output in xml'
end

for cmd in $commit $copy $delete $import $log $mkdir $move $propedit
    _svn_cmpl_ $cmd -l with-revprop --description 'Retrieve revision property' -x
end

for cmd in $diff $log $merge
    _svn_cmpl_ $cmd -l change -s c -d 'The change made in revision ARG' -xa '(__fish_print_svn_rev)'
end

for cmd in $blame $cat $checkout $copy $diff $export $info $list $log $merge $mergeinfo $move $propedit $propget $propdel $proplist $propset $switch $update
    _svn_cmpl_ $cmd -l revision -s r -d 'Which revision the target is first looked up' -xa '(__fish_print_svn_rev)'
end

for cmd in $checkout $copy $export $relocate $stat $switch $update
    _svn_cmpl_ $cmd -l ignore-externals --description 'Ignore externals definitions'
end

for cmd in $blame $list $log $propget $proplist $stat
    _svn_cmpl_ $cmd -l verbose -s v --description 'Print extra information'
end

for cmd in $propdel $propedit $propget $proplist $propset
    _svn_cmpl_ $cmd -l revprop --description 'Operate on a revision property (use with -r)'
end

for cmd in $add $copy $mkdir $move $update
    _svn_cmpl_ $cmd -l parents --description 'Add intermediate parents'
end

for cmd in $blame $diff $log $merge
    _svn_cmpl_ $cmd -l extensions -s x -d 'Output 3 lines of unified context' -xa '-u --unified'
    _svn_cmpl_ $cmd -l extensions -s x -d 'Ignore changes in amount of whitespace' -xa '-b --ignore-space-change'
    _svn_cmpl_ $cmd -l extensions -s x -d 'Ignore all whitespace' -xa '-w --ignore-all-space'
    _svn_cmpl_ $cmd -l extensions -s x -d 'Ignore eol style' -xa '-w --ignore-eol-style'
    _svn_cmpl_ $cmd -l extensions -s x -d 'Show C function name' -xa '-p --show-c-function'

    # Next completion doesn't work, since fish doesn't respect -x key
    #_svn_cmpl_ $cmd -l extensions -n '__fish_seen_subcommand_from --diff-cmd' -xa '(__fish_complete_svn_diff)'
end

for cmd in $cleanup $merge $switch $update
    _svn_cmpl_ $cmd -l diff3-cmd -d 'Use as merge command' -xa "(complete -C(commandline -ct))"
end

for cmd in $blame $info $list $log $stat
    _svn_cmpl_ $cmd -l incremental -d 'Give output suitable for concatenation'
end

for cmd in $add $import $stat
    _svn_cmpl_ $cmd -l no-ignore -d 'Disregard default and svn:ignore property ignores'
end

for cmd in $merge $patch
    _svn_cmpl_ $cmd -l dry-run --description 'Try operation but make no changes'
end

for cmd in $merge $switch
    _svn_cmpl_ $cmd -l ignore-ancestry --description 'Ignore ancestry when calculating merges'
end

for cmd in $diff $log
    _svn_cmpl_ $cmd -l internal-diff --description 'Override diff-cmd specified in config file'
    _svn_cmpl_ $cmd -l diff-cmd --description 'Use external diff command' -xa "(complete -C(commandline -ct))"
end

for cmd in $add $import
    _svn_cmpl_ $cmd -l no-auto-props --description 'Disable automatic properties'
end

for cmd in $switch $update
    _svn_cmpl_ $cmd -l set-depth --description 'Set new working copy depth' -xa 'exclude empty files immediates infinity'
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
_svn_cmpl_ $commit -l keep-changelists --description 'don\'t delete changelists after commit'
_svn_cmpl_ $commit -l no-unlock --description 'Don\'t unlock the targets'

#
# Completions for the 'remove', 'rm', 'delete', 'del' subcommands
#
_svn_cmpl_ $remove -l keep-local -x --description 'Keep path in working copy'

#
# Completions for the 'diff', 'di' subcommands
#
_svn_cmpl_ $diff -l old --description 'Use ARG as the older target' -xa '(__fish_print_svn_rev)'
_svn_cmpl_ $diff -l new --description 'Use ARG as the newer target' -xa '(__fish_print_svn_rev)'
_svn_cmpl_ $diff -l no-diff-deleted --description 'Do not print differences for deleted files'
_svn_cmpl_ $diff -l notice-ancestry --description 'Notice ancestry when calculating differences'
_svn_cmpl_ $diff -l summarize --description 'Show a summary of the results'
_svn_cmpl_ $diff -l git -d 'Use git\'s extended diff format'
_svn_cmpl_ $diff -l show-copies-as-adds -d 'Don\'t diff copied or moved files with their source'

#
# Completions for the 'export' subcommand
#
_svn_cmpl_ $export -l native-eol -x --description 'Use a different EOL marker than the standard'

#
# Completions for the 'log' subcommand
#
_svn_cmpl_ $log -l stop-on-copy --description 'Do not cross copies while traversing history'
_svn_cmpl_ $log -l limit -s l -x --description 'Maximum number of log entries'
_svn_cmpl_ $log -l diff --description 'Produce diff output'
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
_svn_cmpl_ $propget -l strict --description 'Use strict semantics'

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
_svn_cmpl_ svn:keywords -a "URL" -d 'The URL for the head version of the object'
_svn_cmpl_ svn:keywords -a "Author" -d 'The last person to modify the file'
_svn_cmpl_ svn:keywords -a "Date" -d 'Last changed date'
_svn_cmpl_ svn:keywords -a "Rev" -d 'The last revision the object changed'
_svn_cmpl_ svn:keywords -a "Id" -d 'A compressed summary of all keywords'

#
# Completions for the 'relocate' subcommand
#
_svn_cmpl_ $relocate -xa '( svn info | grep URL: | cut -d " " -f 2 ) http:// ftp:// svn+ssh:// svn+ssh://(__fish_print_hostnames)'

#
# Completions for the 'switch', 'sw' subcommands
#
_svn_cmpl_ $switch -l relocate --description 'Relocate via URL-rewriting' -xa '( svn info | grep URL: | cut -d " " -f 2 ) http:// ftp:// svn+ssh:// svn+ssh://(__fish_print_hostnames)'

#
# Completions for the 'status', 'st' subcommands
#
_svn_cmpl_ $stat -l show-updates -s u -d 'Display update information'

#
# Completions for the 'update', 'up' subcommand
#
_svn_cmpl_ $update -l editor-cmd -x --description 'Use ARG as external editor'

functions -e _svn_cmpl_
