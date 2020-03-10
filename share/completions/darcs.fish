#
# Completions from commandline
#

complete -c darcs -n "not __fish_use_subcommand" -a "(test -f _darcs/prefs/repos; and cat _darcs/prefs/repos)" -d "Darcs repo"
complete -c darcs -a "test predist boringfile binariesfile" -n "contains setpref (commandline -poc)" -d "Set the specified option" -x

#
# subcommands
#

complete -c darcs -n __fish_use_subcommand -x -a help -d 'Display help about darcs and darcs commands'
complete -c darcs -n __fish_use_subcommand -x -a add -d 'Add new files to version control'
complete -c darcs -n __fish_use_subcommand -x -a remove -d 'Remove files from version control'
complete -c darcs -n __fish_use_subcommand -x -a move -d 'Move or rename files'
complete -c darcs -n __fish_use_subcommand -x -a replace -d 'Substitute one word for another'
complete -c darcs -n __fish_use_subcommand -x -a revert -d 'Discard unrecorded changes'
complete -c darcs -n __fish_use_subcommand -x -a unrevert -d 'Undo the last revert'
complete -c darcs -n __fish_use_subcommand -x -a whatsnew -d 'List unrecorded changes in the working tree'
complete -c darcs -n __fish_use_subcommand -x -a record -d 'Create a patch from unrecorded changes'
complete -c darcs -n __fish_use_subcommand -x -a unrecord -d 'Remove recorded patches without changing the working tree'
complete -c darcs -n __fish_use_subcommand -x -a amend -d 'Improve a patch before it leaves your repository'
complete -c darcs -n __fish_use_subcommand -x -a mark-conflicts -d 'Mark unresolved conflicts in working tree, for manual resolution'
complete -c darcs -n __fish_use_subcommand -x -a tag -d 'Name the current repository state for future reference'
complete -c darcs -n __fish_use_subcommand -x -a setpref -d 'Set a preference (test, predist, boringfile or binariesfile)'
complete -c darcs -n __fish_use_subcommand -x -a diff -d 'Create a diff between two versions of the repository'
complete -c darcs -n __fish_use_subcommand -x -a log -d 'List patches in the repository'
complete -c darcs -n __fish_use_subcommand -x -a annotate -d 'Annotate lines of a file with the last patch that modified it'
complete -c darcs -n __fish_use_subcommand -x -a dist -d 'Create a distribution archive'
complete -c darcs -n __fish_use_subcommand -x -a show -d 'Show information about the given repository'
complete -c darcs -n __fish_use_subcommand -x -a pull -d 'Copy and apply patches from another repository to this one'
complete -c darcs -n __fish_use_subcommand -x -a obliterate -d 'Delete selected patches from the repository'
complete -c darcs -n __fish_use_subcommand -x -a rollback -d 'Apply the inverse of recorded changes to the working tree'
complete -c darcs -n __fish_use_subcommand -x -a push -d 'Copy and apply patches from this repository to another one'
complete -c darcs -n __fish_use_subcommand -x -a send -d 'Prepare a bundle of patches to be applied to some target repository'
complete -c darcs -n __fish_use_subcommand -x -a apply -d 'Apply a patch bundle created by `darcs send\''
complete -c darcs -n __fish_use_subcommand -x -a clone -d 'Make a copy of an existing repository'
complete -c darcs -n __fish_use_subcommand -x -a initialize -d 'Create an empty repository'
complete -c darcs -n __fish_use_subcommand -x -a optimize -d 'Optimize the repository'
complete -c darcs -n __fish_use_subcommand -x -a repair -d 'Repair a corrupted repository'
complete -c darcs -n __fish_use_subcommand -x -a convert -d 'Convert repositories between various formats'

function __fish_darcs_use_show_command
    set -l cmd (commandline -poc)
    set -e cmd[1]

    if contains show $cmd
        for i in $cmd
            switch $i
                case contents
                    return 1
                case dependencies
                    return 1
                case files
                    return 1
                case index
                    return 1
                case pristine
                    return 1
                case repo
                    return 1
                case authors
                    return 1
                case tags
                    return 1
                case patch-index
                    return 1
            end
        end
        return 0
    end
    return 1
end

complete -c darcs -n __fish_darcs_use_show_command -x -a contents -d 'Outputs a specific version of a file'
complete -c darcs -n __fish_darcs_use_show_command -x -a dependencies -d 'Generate the graph of dependencies'
complete -c darcs -n __fish_darcs_use_show_command -x -a files -d 'Show version-controlled files in the working tree'
complete -c darcs -n __fish_darcs_use_show_command -x -a index -d 'Dump contents of working tree index'
complete -c darcs -n __fish_darcs_use_show_command -x -a pristine -d 'Dump contents of pristine cache'
complete -c darcs -n __fish_darcs_use_show_command -x -a repo -d 'Show repository summary information'
complete -c darcs -n __fish_darcs_use_show_command -x -a authors -d 'List authors by patch count'
complete -c darcs -n __fish_darcs_use_show_command -x -a tags -d 'Show all tags in the repository'
complete -c darcs -n __fish_darcs_use_show_command -x -a patch-index -d 'Check integrity of patch index'

function __fish_darcs_use_optimize_command
    set -l cmd (commandline -poc)
    set -e cmd[1]

    if contains optimize $cmd
        for i in $cmd
            switch $i
                case clean
                    return 1
                case http
                    return 1
                case reorder
                    return 1
                case enable-patch-index
                    return 1
                case disable-patch-index
                    return 1
                case compress
                    return 1
                case uncompress
                    return 1
                case relink
                    return 1
                case pristine
                    return 1
                case upgrade
                    return 1
                case cache
                    return 1
            end
        end
        return 0
    end
    return 1
end

complete -c darcs -n __fish_darcs_use_optimize_command -x -a clean -d 'Garbage collect pristine, inventories and patches'
complete -c darcs -n __fish_darcs_use_optimize_command -x -a http -d 'Optimize repository for getting over network'
complete -c darcs -n __fish_darcs_use_optimize_command -x -a reorder -d 'Reorder the patches in the repository'
complete -c darcs -n __fish_darcs_use_optimize_command -x -a enable-patch-index -d 'Enable patch index'
complete -c darcs -n __fish_darcs_use_optimize_command -x -a disable-patch-index -d 'Disable patch index'
complete -c darcs -n __fish_darcs_use_optimize_command -x -a compress -d 'Compress patches and inventories'
complete -c darcs -n __fish_darcs_use_optimize_command -x -a uncompress -d 'Uncompress patches and inventories'
complete -c darcs -n __fish_darcs_use_optimize_command -x -a relink -d 'Relink random internal data to a sibling'
complete -c darcs -n __fish_darcs_use_optimize_command -x -a pristine -d 'Optimize hashed pristine layout'
complete -c darcs -n __fish_darcs_use_optimize_command -x -a upgrade -d 'Upgrade repository to latest compatible format'
complete -c darcs -n __fish_darcs_use_optimize_command -x -a cache -d 'Garbage collect global cache'


function __fish_darcs_use_convert_command
    set -l cmd (commandline -poc)
    set -e cmd[1]

    if contains convert $cmd
        for i in $cmd
            switch $i
                case darcs-2
                    return 1
                case export
                    return 1
                case import
                    return 1
            end
        end
        return 0
    end
    return 1
end

complete -c darcs -l help -d 'Show a brief description of the command and its options'
complete -c darcs -l list-options -d 'Show plain list of available options and commands, for auto-completion'
complete -c darcs -l run-posthook -d 'Run posthook command without prompting [DEFAULT]'
complete -c darcs -l prompt-posthook -d 'Prompt before running posthook'
complete -c darcs -l no-posthook -d 'Don\'t run posthook command [DEFAULT]'
complete -c darcs -l posthook -x -a '(__fish_complete_command)' -d 'Specify command to run after this darcs command'
complete -c darcs -l run-prehook -d 'Run prehook command without prompting [DEFAULT]'
complete -c darcs -l prompt-prehook -d 'Prompt before running prehook'
complete -c darcs -l no-prehook -d 'Don\'t run prehook command [DEFAULT]'
complete -c darcs -l prehook -x -a '(__fish_complete_command)' -d 'Specify command to run before this darcs command'
complete -c darcs -l disable -d 'Disable this command'
complete -c darcs -l debug -d 'Give only debug output'
complete -c darcs -l debug-http -d 'Debug output from libcurl'
complete -c darcs -l quiet -s q -d 'Suppress informational output'
complete -c darcs -l standard-verbosity -d 'Neither verbose nor quiet output [DEFAULT]'
complete -c darcs -l verbose -s v -d 'Give verbose output'
complete -c darcs -l timings -d 'Provide debugging timings information'
complete -c darcs -l no-cache -d 'Don\'t use patch caches'

#
# Completions for the 'add' subcommand
#

complete -c darcs -n 'contains \'add\' (commandline -poc)' -l boring -d 'Don\'t skip boring files'
complete -c darcs -n 'contains \'add\' (commandline -poc)' -l no-boring -d 'Skip boring files [DEFAULT]'
complete -c darcs -n 'contains \'add\' (commandline -poc)' -l case-ok -d 'Don\'t refuse to add files differing only in case'
complete -c darcs -n 'contains \'add\' (commandline -poc)' -l no-case-ok -d 'Refuse to add files whose name differ only in case [DEFAULT]'
complete -c darcs -n 'contains \'add\' (commandline -poc)' -l reserved-ok -d 'Don\'t refuse to add files with Windows-reserved names'
complete -c darcs -n 'contains \'add\' (commandline -poc)' -l no-reserved-ok -d 'Refuse to add files with Windows-reserved names [DEFAULT]'
complete -c darcs -n 'contains \'add\' (commandline -poc)' -l recursive -s r -d 'Recurse into subdirectories'
complete -c darcs -n 'contains \'add\' (commandline -poc)' -l not-recursive -d 'Don\'t recurse into subdirectories [DEFAULT]'
complete -c darcs -n 'contains \'add\' (commandline -poc)' -l no-recursive -d 'Don\'t recurse into subdirectories [DEFAULT]'
complete -c darcs -n 'contains \'add\' (commandline -poc)' -l repodir -x -a '(__fish_complete_directories (commandline -ct))' -d 'Specify the repository directory in which to run'
complete -c darcs -n 'contains \'add\' (commandline -poc)' -l dry-run -d 'Don\'t actually take the action'
complete -c darcs -n 'contains \'add\' (commandline -poc)' -l umask -d 'Specify umask to use when writing'

#
# Completions for the 'remove' subcommand
#

complete -c darcs -n 'contains \'remove\' (commandline -poc)' -l repodir -x -a '(__fish_complete_directories (commandline -ct))' -d 'Specify the repository directory in which to run'
complete -c darcs -n 'contains \'remove\' (commandline -poc)' -l recursive -s r -d 'Recurse into subdirectories'
complete -c darcs -n 'contains \'remove\' (commandline -poc)' -l not-recursive -d 'Don\'t recurse into subdirectories [DEFAULT]'
complete -c darcs -n 'contains \'remove\' (commandline -poc)' -l no-recursive -d 'Don\'t recurse into subdirectories [DEFAULT]'
complete -c darcs -n 'contains \'remove\' (commandline -poc)' -l umask -d 'Specify umask to use when writing'

#
# Completions for the 'move' subcommand
#

complete -c darcs -n 'contains \'move\' (commandline -poc)' -l case-ok -d 'Don\'t refuse to add files differing only in case'
complete -c darcs -n 'contains \'move\' (commandline -poc)' -l no-case-ok -d 'Refuse to add files whose name differ only in case [DEFAULT]'
complete -c darcs -n 'contains \'move\' (commandline -poc)' -l reserved-ok -d 'Don\'t refuse to add files with Windows-reserved names'
complete -c darcs -n 'contains \'move\' (commandline -poc)' -l no-reserved-ok -d 'Refuse to add files with Windows-reserved names [DEFAULT]'
complete -c darcs -n 'contains \'move\' (commandline -poc)' -l repodir -x -a '(__fish_complete_directories (commandline -ct))' -d 'Specify the repository directory in which to run'
complete -c darcs -n 'contains \'move\' (commandline -poc)' -l umask -d 'Specify umask to use when writing'

#
# Completions for the 'replace' subcommand
#

complete -c darcs -n 'contains \'replace\' (commandline -poc)' -l token-chars -d 'Define token to contain these characters'
complete -c darcs -n 'contains \'replace\' (commandline -poc)' -l force -s f -d 'Proceed with replace even if 'new' token already exists'
complete -c darcs -n 'contains \'replace\' (commandline -poc)' -l no-force -d 'Don\'t force the replace if it looks scary [DEFAULT]'
complete -c darcs -n 'contains \'replace\' (commandline -poc)' -l repodir -x -a '(__fish_complete_directories (commandline -ct))' -d 'Specify the repository directory in which to run'
complete -c darcs -n 'contains \'replace\' (commandline -poc)' -l ignore-times -d 'Don\'t trust the file modification times'
complete -c darcs -n 'contains \'replace\' (commandline -poc)' -l no-ignore-times -d 'Trust modification times to find modified files [DEFAULT]'
complete -c darcs -n 'contains \'replace\' (commandline -poc)' -l umask -d 'Specify umask to use when writing'

#
# Completions for the 'revert' subcommand
#

complete -c darcs -n 'contains \'revert\' (commandline -poc)' -l all -s a -d 'Answer yes to all patches'
complete -c darcs -n 'contains \'revert\' (commandline -poc)' -l no-interactive -d 'Answer yes to all patches'
complete -c darcs -n 'contains \'revert\' (commandline -poc)' -l interactive -s i -d 'Prompt user interactively'
complete -c darcs -n 'contains \'revert\' (commandline -poc)' -l repodir -x -a '(__fish_complete_directories (commandline -ct))' -d 'Specify the repository directory in which to run'
complete -c darcs -n 'contains \'revert\' (commandline -poc)' -l unified -s u -d 'Output changes in a darcs-specific format similar to diff -u'
complete -c darcs -n 'contains \'revert\' (commandline -poc)' -l no-unified -d 'Output changes in darcs\' usual format [DEFAULT]'
complete -c darcs -n 'contains \'revert\' (commandline -poc)' -l myers -d 'Use myers diff algorithm'
complete -c darcs -n 'contains \'revert\' (commandline -poc)' -l patience -d 'Use patience diff algorithm [DEFAULT]'
complete -c darcs -n 'contains \'revert\' (commandline -poc)' -l ignore-times -d 'Don\'t trust the file modification times'
complete -c darcs -n 'contains \'revert\' (commandline -poc)' -l no-ignore-times -d 'Trust modification times to find modified files [DEFAULT]'
complete -c darcs -n 'contains \'revert\' (commandline -poc)' -l umask -d 'Specify umask to use when writing'

#
# Completions for the 'unrevert' subcommand
#

complete -c darcs -n 'contains \'unrevert\' (commandline -poc)' -l ignore-times -d 'Don\'t trust the file modification times'
complete -c darcs -n 'contains \'unrevert\' (commandline -poc)' -l no-ignore-times -d 'Trust modification times to find modified files [DEFAULT]'
complete -c darcs -n 'contains \'unrevert\' (commandline -poc)' -l all -s a -d 'Answer yes to all patches'
complete -c darcs -n 'contains \'unrevert\' (commandline -poc)' -l no-interactive -d 'Answer yes to all patches'
complete -c darcs -n 'contains \'unrevert\' (commandline -poc)' -l interactive -s i -d 'Prompt user interactively'
complete -c darcs -n 'contains \'unrevert\' (commandline -poc)' -l repodir -x -a '(__fish_complete_directories (commandline -ct))' -d 'Specify the repository directory in which to run'
complete -c darcs -n 'contains \'unrevert\' (commandline -poc)' -l unified -s u -d 'Output changes in a darcs-specific format similar to diff -u'
complete -c darcs -n 'contains \'unrevert\' (commandline -poc)' -l no-unified -d 'Output changes in darcs\' usual format [DEFAULT]'
complete -c darcs -n 'contains \'unrevert\' (commandline -poc)' -l myers -d 'Use myers diff algorithm'
complete -c darcs -n 'contains \'unrevert\' (commandline -poc)' -l patience -d 'Use patience diff algorithm [DEFAULT]'
complete -c darcs -n 'contains \'unrevert\' (commandline -poc)' -l umask -d 'Specify umask to use when writing'

#
# Completions for the 'whatsnew' subcommand
#

complete -c darcs -n 'contains \'whatsnew\' (commandline -poc)' -l summary -s s -d 'Summarize changes'
complete -c darcs -n 'contains \'whatsnew\' (commandline -poc)' -l no-summary -d 'Don\'t summarize changes'
complete -c darcs -n 'contains \'whatsnew\' (commandline -poc)' -l unified -s u -d 'Output changes in a darcs-specific format similar to diff -u'
complete -c darcs -n 'contains \'whatsnew\' (commandline -poc)' -l no-unified -d 'Output changes in darcs\' usual format [DEFAULT]'
complete -c darcs -n 'contains \'whatsnew\' (commandline -poc)' -l human-readable -d 'Give human-readable output [DEFAULT]'
complete -c darcs -n 'contains \'whatsnew\' (commandline -poc)' -l machine-readable -d 'Give machine-readable output'
complete -c darcs -n 'contains \'whatsnew\' (commandline -poc)' -l look-for-adds -s -l -d 'Look for (non-boring) files that could be added'
complete -c darcs -n 'contains \'whatsnew\' (commandline -poc)' -l dont-look-for-adds -d 'Don\'t look for any files that could be added [DEFAULT]'
complete -c darcs -n 'contains \'whatsnew\' (commandline -poc)' -l no-look-for-adds -d 'Don\'t look for any files that could be added [DEFAULT]'
complete -c darcs -n 'contains \'whatsnew\' (commandline -poc)' -l look-for-replaces -d 'Look for replaces that could be marked'
complete -c darcs -n 'contains \'whatsnew\' (commandline -poc)' -l dont-look-for-replaces -d 'Don\'t look for any replaces [DEFAULT]'
complete -c darcs -n 'contains \'whatsnew\' (commandline -poc)' -l no-look-for-replaces -d 'Don\'t look for any replaces [DEFAULT]'
complete -c darcs -n 'contains \'whatsnew\' (commandline -poc)' -l look-for-moves -d 'Look for files that may be moved/renamed'
complete -c darcs -n 'contains \'whatsnew\' (commandline -poc)' -l dont-look-for-moves -d 'Don\'t look for any files that could be moved/renamed [DEFAULT]'
complete -c darcs -n 'contains \'whatsnew\' (commandline -poc)' -l no-look-for-moves -d 'Don\'t look for any files that could be moved/renamed [DEFAULT]'
complete -c darcs -n 'contains \'whatsnew\' (commandline -poc)' -l myers -d 'Use myers diff algorithm'
complete -c darcs -n 'contains \'whatsnew\' (commandline -poc)' -l patience -d 'Use patience diff algorithm [DEFAULT]'
complete -c darcs -n 'contains \'whatsnew\' (commandline -poc)' -l repodir -x -a '(__fish_complete_directories (commandline -ct))' -d 'Specify the repository directory in which to run'
complete -c darcs -n 'contains \'whatsnew\' (commandline -poc)' -l all -s a -d 'Answer yes to all patches'
complete -c darcs -n 'contains \'whatsnew\' (commandline -poc)' -l no-interactive -d 'Answer yes to all patches'
complete -c darcs -n 'contains \'whatsnew\' (commandline -poc)' -l interactive -s i -d 'Prompt user interactively'
complete -c darcs -n 'contains \'whatsnew\' (commandline -poc)' -l ignore-times -d 'Don\'t trust the file modification times'
complete -c darcs -n 'contains \'whatsnew\' (commandline -poc)' -l no-ignore-times -d 'Trust modification times to find modified files [DEFAULT]'
complete -c darcs -n 'contains \'whatsnew\' (commandline -poc)' -l boring -d 'Don\'t skip boring files'
complete -c darcs -n 'contains \'whatsnew\' (commandline -poc)' -l no-boring -d 'Skip boring files [DEFAULT]'

#
# Completions for the 'record' subcommand
#

complete -c darcs -n 'contains \'record\' (commandline -poc)' -l name -s m -d 'Name of patch'
complete -c darcs -n 'contains \'record\' (commandline -poc)' -l author -s A -d 'Specify author id'
complete -c darcs -n 'contains \'record\' (commandline -poc)' -l test -d 'Run the test script'
complete -c darcs -n 'contains \'record\' (commandline -poc)' -l no-test -d 'Don\'t run the test script [DEFAULT]'
complete -c darcs -n 'contains \'record\' (commandline -poc)' -l leave-test-directory -d 'Don\'t remove the test directory [DEFAULT]'
complete -c darcs -n 'contains \'record\' (commandline -poc)' -l remove-test-directory -d 'Remove the test directory'
complete -c darcs -n 'contains \'record\' (commandline -poc)' -l all -s a -d 'Answer yes to all patches'
complete -c darcs -n 'contains \'record\' (commandline -poc)' -l no-interactive -d 'Answer yes to all patches'
complete -c darcs -n 'contains \'record\' (commandline -poc)' -l interactive -s i -d 'Prompt user interactively'
complete -c darcs -n 'contains \'record\' (commandline -poc)' -l pipe -d 'Ask user interactively for the patch metadata'
complete -c darcs -n 'contains \'record\' (commandline -poc)' -l ask-deps -d 'Manually select dependencies'
complete -c darcs -n 'contains \'record\' (commandline -poc)' -l no-ask-deps -d 'Automatically select dependencies [DEFAULT]'
complete -c darcs -n 'contains \'record\' (commandline -poc)' -l edit-long-comment -d 'Edit the long comment by default'
complete -c darcs -n 'contains \'record\' (commandline -poc)' -l skip-long-comment -d 'Don\'t give a long comment'
complete -c darcs -n 'contains \'record\' (commandline -poc)' -l prompt-long-comment -d 'Prompt for whether to edit the long comment'
complete -c darcs -n 'contains \'record\' (commandline -poc)' -l look-for-adds -s l -d 'Look for (non-boring) files that could be added'
complete -c darcs -n 'contains \'record\' (commandline -poc)' -l dont-look-for-adds -d 'Don\'t look for any files that could be added [DEFAULT]'
complete -c darcs -n 'contains \'record\' (commandline -poc)' -l no-look-for-adds -d 'Don\'t look for any files that could be added [DEFAULT]'
complete -c darcs -n 'contains \'record\' (commandline -poc)' -l look-for-replaces -d 'Look for replaces that could be marked'
complete -c darcs -n 'contains \'record\' (commandline -poc)' -l dont-look-for-replaces -d 'Don\'t look for any replaces [DEFAULT]'
complete -c darcs -n 'contains \'record\' (commandline -poc)' -l no-look-for-replaces -d 'Don\'t look for any replaces [DEFAULT]'
complete -c darcs -n 'contains \'record\' (commandline -poc)' -l look-for-moves -d 'Look for files that may be moved/renamed'
complete -c darcs -n 'contains \'record\' (commandline -poc)' -l dont-look-for-moves -d 'Don\'t look for any files that could be moved/renamed [DEFAULT]'
complete -c darcs -n 'contains \'record\' (commandline -poc)' -l no-look-for-moves -d 'Don\'t look for any files that could be moved/renamed [DEFAULT]'
complete -c darcs -n 'contains \'record\' (commandline -poc)' -l repodir -x -a '(__fish_complete_directories (commandline -ct))' -d 'Specify the repository directory in which to run'
complete -c darcs -n 'contains \'record\' (commandline -poc)' -l unified -s u -d 'Output changes in a darcs-specific format similar to diff -u'
complete -c darcs -n 'contains \'record\' (commandline -poc)' -l no-unified -d 'Output changes in darcs\' usual format [DEFAULT]'
complete -c darcs -n 'contains \'record\' (commandline -poc)' -l myers -d 'Use myers diff algorithm'
complete -c darcs -n 'contains \'record\' (commandline -poc)' -l patience -d 'Use patience diff algorithm [DEFAULT]'
complete -c darcs -n 'contains \'record\' (commandline -poc)' -l logfile -d 'Give patch name and comment in file'
complete -c darcs -n 'contains \'record\' (commandline -poc)' -l delete-logfile -d 'Delete the logfile when done'
complete -c darcs -n 'contains \'record\' (commandline -poc)' -l no-delete-logfile -d 'Keep the logfile when done [DEFAULT]'
complete -c darcs -n 'contains \'record\' (commandline -poc)' -l compress -d 'Compress patch data [DEFAULT]'
complete -c darcs -n 'contains \'record\' (commandline -poc)' -l dont-compress -d 'Don\'t compress patch data'
complete -c darcs -n 'contains \'record\' (commandline -poc)' -l no-compress -d 'Don\'t compress patch data'
complete -c darcs -n 'contains \'record\' (commandline -poc)' -l ignore-times -d 'Don\'t trust the file modification times'
complete -c darcs -n 'contains \'record\' (commandline -poc)' -l no-ignore-times -d 'Trust modification times to find modified files [DEFAULT]'
complete -c darcs -n 'contains \'record\' (commandline -poc)' -l umask -d 'Specify umask to use when writing'
complete -c darcs -n 'contains \'record\' (commandline -poc)' -l set-scripts-executable -d 'Make scripts executable'
complete -c darcs -n 'contains \'record\' (commandline -poc)' -l dont-set-scripts-executable -d 'Don\'t make scripts executable [DEFAULT]'
complete -c darcs -n 'contains \'record\' (commandline -poc)' -l no-set-scripts-executable -d 'Don\'t make scripts executable [DEFAULT]'
complete -c darcs -n 'contains \'record\' (commandline -poc)' -l boring -d 'Don\'t skip boring files'
complete -c darcs -n 'contains \'record\' (commandline -poc)' -l no-boring -d 'Skip boring files [DEFAULT]'

#
# Completions for the 'unrecord' subcommand
#

complete -c darcs -n 'contains \'unrecord\' (commandline -poc)' -l not-in-remote -d 'Select all patches not in the default push/pull repository or at location URL/PATH'
complete -c darcs -n 'contains \'unrecord\' (commandline -poc)' -l from-match -d 'Select changes starting with a patch matching PATTERN'
complete -c darcs -n 'contains \'unrecord\' (commandline -poc)' -l from-patch -d 'Select changes starting with a patch matching REGEXP'
complete -c darcs -n 'contains \'unrecord\' (commandline -poc)' -l from-hash -d 'Select changes starting with a patch with HASH'
complete -c darcs -n 'contains \'unrecord\' (commandline -poc)' -l from-tag -d 'Select changes starting with a tag matching REGEXP'
complete -c darcs -n 'contains \'unrecord\' (commandline -poc)' -l last -d 'Select the last NUMBER patches'
complete -c darcs -n 'contains \'unrecord\' (commandline -poc)' -l matches -d 'Select patches matching PATTERN'
complete -c darcs -n 'contains \'unrecord\' (commandline -poc)' -l patches -s p -d 'Select patches matching REGEXP'
complete -c darcs -n 'contains \'unrecord\' (commandline -poc)' -l tags -s t -d 'Select tags matching REGEXP'
complete -c darcs -n 'contains \'unrecord\' (commandline -poc)' -l hash -s h -d 'Select a single patch with HASH'
complete -c darcs -n 'contains \'unrecord\' (commandline -poc)' -l no-deps -d 'Don\'t automatically fulfill dependencies'
complete -c darcs -n 'contains \'unrecord\' (commandline -poc)' -l auto-deps -d 'Don\'t ask about patches that are depended on by matched patches (with --match or --patch)'
complete -c darcs -n 'contains \'unrecord\' (commandline -poc)' -l dont-prompt-for-dependencies -d 'Don\'t ask about patches that are depended on by matched patches (with --match or --patch)'
complete -c darcs -n 'contains \'unrecord\' (commandline -poc)' -l prompt-deps -d 'Prompt about patches that are depended on by matched patches [DEFAULT]'
complete -c darcs -n 'contains \'unrecord\' (commandline -poc)' -l prompt-for-dependencies -d 'Prompt about patches that are depended on by matched patches [DEFAULT]'
complete -c darcs -n 'contains \'unrecord\' (commandline -poc)' -l all -s a -d 'Answer yes to all patches'
complete -c darcs -n 'contains \'unrecord\' (commandline -poc)' -l no-interactive -d 'Answer yes to all patches'
complete -c darcs -n 'contains \'unrecord\' (commandline -poc)' -l interactive -s i -d 'Prompt user interactively'
complete -c darcs -n 'contains \'unrecord\' (commandline -poc)' -l repodir -x -a '(__fish_complete_directories (commandline -ct))' -d 'Specify the repository directory in which to run'
complete -c darcs -n 'contains \'unrecord\' (commandline -poc)' -l compress -d 'Compress patch data [DEFAULT]'
complete -c darcs -n 'contains \'unrecord\' (commandline -poc)' -l dont-compress -d 'Don\'t compress patch data'
complete -c darcs -n 'contains \'unrecord\' (commandline -poc)' -l no-compress -d 'Don\'t compress patch data'
complete -c darcs -n 'contains \'unrecord\' (commandline -poc)' -l umask -d 'Specify umask to use when writing'
complete -c darcs -n 'contains \'unrecord\' (commandline -poc)' -l reverse -d 'Show/consider changes in reverse order'
complete -c darcs -n 'contains \'unrecord\' (commandline -poc)' -l no-reverse -d 'Show/consider changes in the usual order [DEFAULT]'

#
# Completions for the 'amend' subcommand
#

complete -c darcs -n 'contains \'amend\' (commandline -poc)' -l unrecord -d 'Remove changes from the patch'
complete -c darcs -n 'contains \'amend\' (commandline -poc)' -l record -d 'Add more changes to the patch [DEFAULT]'
complete -c darcs -n 'contains \'amend\' (commandline -poc)' -l match -d 'Select a single patch matching PATTERN'
complete -c darcs -n 'contains \'amend\' (commandline -poc)' -l patch -s p -d 'Select a single patch matching REGEXP'
complete -c darcs -n 'contains \'amend\' (commandline -poc)' -l hash -s h -d 'Select a single patch with HASH'
complete -c darcs -n 'contains \'amend\' (commandline -poc)' -l test -d 'Run the test script'
complete -c darcs -n 'contains \'amend\' (commandline -poc)' -l no-test -d 'Don\'t run the test script [DEFAULT]'
complete -c darcs -n 'contains \'amend\' (commandline -poc)' -l leave-test-directory -d 'Don\'t remove the test directory [DEFAULT]'
complete -c darcs -n 'contains \'amend\' (commandline -poc)' -l remove-test-directory -d 'Remove the test directory'
complete -c darcs -n 'contains \'amend\' (commandline -poc)' -l all -s a -d 'Answer yes to all patches'
complete -c darcs -n 'contains \'amend\' (commandline -poc)' -l no-interactive -d 'Answer yes to all patches'
complete -c darcs -n 'contains \'amend\' (commandline -poc)' -l interactive -s i -d 'Prompt user interactively'
complete -c darcs -n 'contains \'amend\' (commandline -poc)' -l author -s A -d 'Specify author id'
complete -c darcs -n 'contains \'amend\' (commandline -poc)' -l select-author -d 'Select author id from a menu'
complete -c darcs -n 'contains \'amend\' (commandline -poc)' -l name -s m -d 'Name of patch'
complete -c darcs -n 'contains \'amend\' (commandline -poc)' -l ask-deps -d 'Manually select dependencies'
complete -c darcs -n 'contains \'amend\' (commandline -poc)' -l no-ask-deps -d 'Automatically select dependencies [DEFAULT]'
complete -c darcs -n 'contains \'amend\' (commandline -poc)' -l edit-long-comment -d 'Edit the long comment by default'
complete -c darcs -n 'contains \'amend\' (commandline -poc)' -l skip-long-comment -d 'Don\'t give a long comment'
complete -c darcs -n 'contains \'amend\' (commandline -poc)' -l prompt-long-comment -d 'Prompt for whether to edit the long comment'
complete -c darcs -n 'contains \'amend\' (commandline -poc)' -l keep-date -d 'Keep the date of the original patch'
complete -c darcs -n 'contains \'amend\' (commandline -poc)' -l no-keep-date -d 'Use the current date for the amended patch [DEFAULT]'
complete -c darcs -n 'contains \'amend\' (commandline -poc)' -l look-for-adds -s l -d 'Look for (non-boring) files that could be added'
complete -c darcs -n 'contains \'amend\' (commandline -poc)' -l dont-look-for-adds -d 'Don\'t look for any files that could be added [DEFAULT]'
complete -c darcs -n 'contains \'amend\' (commandline -poc)' -l no-look-for-adds -d 'Don\'t look for any files that could be added [DEFAULT]'
complete -c darcs -n 'contains \'amend\' (commandline -poc)' -l look-for-replaces -d 'Look for replaces that could be marked'
complete -c darcs -n 'contains \'amend\' (commandline -poc)' -l dont-look-for-replaces -d 'Don\'t look for any replaces [DEFAULT]'
complete -c darcs -n 'contains \'amend\' (commandline -poc)' -l no-look-for-replaces -d 'Don\'t look for any replaces [DEFAULT]'
complete -c darcs -n 'contains \'amend\' (commandline -poc)' -l look-for-moves -d 'Look for files that may be moved/renamed'
complete -c darcs -n 'contains \'amend\' (commandline -poc)' -l dont-look-for-moves -d 'Don\'t look for any files that could be moved/renamed [DEFAULT]'
complete -c darcs -n 'contains \'amend\' (commandline -poc)' -l no-look-for-moves -d 'Don\'t look for any files that could be moved/renamed [DEFAULT]'
complete -c darcs -n 'contains \'amend\' (commandline -poc)' -l repodir -x -a '(__fish_complete_directories (commandline -ct))' -d 'Specify the repository directory in which to run'
complete -c darcs -n 'contains \'amend\' (commandline -poc)' -l unified -s u -d 'Output changes in a darcs-specific format similar to diff -u'
complete -c darcs -n 'contains \'amend\' (commandline -poc)' -l no-unified -d 'Output changes in darcs\' usual format [DEFAULT]'
complete -c darcs -n 'contains \'amend\' (commandline -poc)' -l myers -d 'Use myers diff algorithm'
complete -c darcs -n 'contains \'amend\' (commandline -poc)' -l patience -d 'Use patience diff algorithm [DEFAULT]'
complete -c darcs -n 'contains \'amend\' (commandline -poc)' -l compress -d 'Compress patch data [DEFAULT]'
complete -c darcs -n 'contains \'amend\' (commandline -poc)' -l dont-compress -d 'Don\'t compress patch data'
complete -c darcs -n 'contains \'amend\' (commandline -poc)' -l no-compress -d 'Don\'t compress patch data'
complete -c darcs -n 'contains \'amend\' (commandline -poc)' -l ignore-times -d 'Don\'t trust the file modification times'
complete -c darcs -n 'contains \'amend\' (commandline -poc)' -l no-ignore-times -d 'Trust modification times to find modified files [DEFAULT]'
complete -c darcs -n 'contains \'amend\' (commandline -poc)' -l umask -d 'Specify umask to use when writing'
complete -c darcs -n 'contains \'amend\' (commandline -poc)' -l set-scripts-executable -d 'Make scripts executable'
complete -c darcs -n 'contains \'amend\' (commandline -poc)' -l dont-set-scripts-executable -d 'Don\'t make scripts executable [DEFAULT]'
complete -c darcs -n 'contains \'amend\' (commandline -poc)' -l no-set-scripts-executable -d 'Don\'t make scripts executable [DEFAULT]'

#
# Completions for the 'mark-conflicts' subcommand
#

complete -c darcs -n 'contains \'mark-conflicts\' (commandline -poc)' -l ignore-times -d 'Don\'t trust the file modification times'
complete -c darcs -n 'contains \'mark-conflicts\' (commandline -poc)' -l no-ignore-times -d 'Trust modification times to find modified files [DEFAULT]'
complete -c darcs -n 'contains \'mark-conflicts\' (commandline -poc)' -l repodir -x -a '(__fish_complete_directories (commandline -ct))' -d 'Specify the repository directory in which to run'
complete -c darcs -n 'contains \'mark-conflicts\' (commandline -poc)' -l myers -d 'Use myers diff algorithm'
complete -c darcs -n 'contains \'mark-conflicts\' (commandline -poc)' -l patience -d 'Use patience diff algorithm [DEFAULT]'
complete -c darcs -n 'contains \'mark-conflicts\' (commandline -poc)' -l dry-run -d 'Don\'t actually take the action'
complete -c darcs -n 'contains \'mark-conflicts\' (commandline -poc)' -l xml-output -d 'Generate XML formatted output'
complete -c darcs -n 'contains \'mark-conflicts\' (commandline -poc)' -l umask -d 'Specify umask to use when writing'

#
# Completions for the 'tag' subcommand
#

complete -c darcs -n 'contains \'tag\' (commandline -poc)' -l name -s m -d 'Name of patch'
complete -c darcs -n 'contains \'tag\' (commandline -poc)' -l author -s A -d 'Specify author id'
complete -c darcs -n 'contains \'tag\' (commandline -poc)' -l pipe -d 'Ask user interactively for the patch metadata'
complete -c darcs -n 'contains \'tag\' (commandline -poc)' -l edit-long-comment -d 'Edit the long comment by default'
complete -c darcs -n 'contains \'tag\' (commandline -poc)' -l skip-long-comment -d 'Don\'t give a long comment'
complete -c darcs -n 'contains \'tag\' (commandline -poc)' -l prompt-long-comment -d 'Prompt for whether to edit the long comment'
complete -c darcs -n 'contains \'tag\' (commandline -poc)' -l ask-deps -d 'Manually select dependencies'
complete -c darcs -n 'contains \'tag\' (commandline -poc)' -l no-ask-deps -d 'Automatically select dependencies [DEFAULT]'
complete -c darcs -n 'contains \'tag\' (commandline -poc)' -l repodir -x -a '(__fish_complete_directories (commandline -ct))' -d 'Specify the repository directory in which to run'
complete -c darcs -n 'contains \'tag\' (commandline -poc)' -l compress -d 'Compress patch data [DEFAULT]'
complete -c darcs -n 'contains \'tag\' (commandline -poc)' -l dont-compress -d 'Don\'t compress patch data'
complete -c darcs -n 'contains \'tag\' (commandline -poc)' -l no-compress -d 'Don\'t compress patch data'
complete -c darcs -n 'contains \'tag\' (commandline -poc)' -l umask -d 'Specify umask to use when writing'

#
# Completions for the 'setpref' subcommand
#

complete -c darcs -n 'contains \'setpref\' (commandline -poc)' -l repodir -x -a '(__fish_complete_directories (commandline -ct))' -d 'Specify the repository directory in which to run'
complete -c darcs -n 'contains \'setpref\' (commandline -poc)' -l umask -d 'Specify umask to use when writing'

#
# Completions for the 'diff' subcommand
#

complete -c darcs -n 'contains \'diff\' (commandline -poc)' -l to-match -d 'Select changes up to a patch matching PATTERN'
complete -c darcs -n 'contains \'diff\' (commandline -poc)' -l to-patch -d 'Select changes up to a patch matching REGEXP'
complete -c darcs -n 'contains \'diff\' (commandline -poc)' -l to-hash -d 'Select changes up to a patch with HASH'
complete -c darcs -n 'contains \'diff\' (commandline -poc)' -l to-tag -d 'Select changes up to a tag matching REGEXP'
complete -c darcs -n 'contains \'diff\' (commandline -poc)' -l from-match -d 'Select changes starting with a patch matching PATTERN'
complete -c darcs -n 'contains \'diff\' (commandline -poc)' -l from-patch -d 'Select changes starting with a patch matching REGEXP'
complete -c darcs -n 'contains \'diff\' (commandline -poc)' -l from-hash -d 'Select changes starting with a patch with HASH'
complete -c darcs -n 'contains \'diff\' (commandline -poc)' -l from-tag -d 'Select changes starting with a tag matching REGEXP'
complete -c darcs -n 'contains \'diff\' (commandline -poc)' -l match -d 'Select a single patch matching PATTERN'
complete -c darcs -n 'contains \'diff\' (commandline -poc)' -l patch -s p -d 'Select a single patch matching REGEXP'
complete -c darcs -n 'contains \'diff\' (commandline -poc)' -l hash -s h -d 'Select a single patch with HASH'
complete -c darcs -n 'contains \'diff\' (commandline -poc)' -l last -d 'Select the last NUMBER patches'
complete -c darcs -n 'contains \'diff\' (commandline -poc)' -l index -s n -d 'Select a range of patches'
complete -c darcs -n 'contains \'diff\' (commandline -poc)' -l diff-command -d 'Specify diff command (ignores --diff-opts)'
complete -c darcs -n 'contains \'diff\' (commandline -poc)' -l diff-opts -d 'Options to pass to diff'
complete -c darcs -n 'contains \'diff\' (commandline -poc)' -l unified -s u -d 'Pass -u option to diff [DEFAULT]'
complete -c darcs -n 'contains \'diff\' (commandline -poc)' -l no-unified -d 'Output patch in diff\'s dumb format'
complete -c darcs -n 'contains \'diff\' (commandline -poc)' -l repodir -x -a '(__fish_complete_directories (commandline -ct))' -d 'Specify the repository directory in which to run'
complete -c darcs -n 'contains \'diff\' (commandline -poc)' -l store-in-memory -d 'Do patch application in memory rather than on disk'
complete -c darcs -n 'contains \'diff\' (commandline -poc)' -l no-store-in-memory -d 'Do patch application on disk [DEFAULT]'
complete -c darcs -n 'contains \'diff\' (commandline -poc)' -l pause-for-gui -d 'Pause for an external diff or merge command to finish [DEFAULT]'
complete -c darcs -n 'contains \'diff\' (commandline -poc)' -l no-pause-for-gui -d 'Return immediately after external diff or merge command finishes'

#
# Completions for the 'log' subcommand
#

complete -c darcs -n 'contains \'log\' (commandline -poc)' -l to-match -d 'Select changes up to a patch matching PATTERN'
complete -c darcs -n 'contains \'log\' (commandline -poc)' -l to-patch -d 'Select changes up to a patch matching REGEXP'
complete -c darcs -n 'contains \'log\' (commandline -poc)' -l to-hash -d 'Select changes up to a patch with HASH'
complete -c darcs -n 'contains \'log\' (commandline -poc)' -l to-tag -d 'Select changes up to a tag matching REGEXP'
complete -c darcs -n 'contains \'log\' (commandline -poc)' -l from-match -d 'Select changes starting with a patch matching PATTERN'
complete -c darcs -n 'contains \'log\' (commandline -poc)' -l from-patch -d 'Select changes starting with a patch matching REGEXP'
complete -c darcs -n 'contains \'log\' (commandline -poc)' -l from-hash -d 'Select changes starting with a patch with HASH'
complete -c darcs -n 'contains \'log\' (commandline -poc)' -l from-tag -d 'Select changes starting with a tag matching REGEXP'
complete -c darcs -n 'contains \'log\' (commandline -poc)' -l last -d 'Select the last NUMBER patches'
complete -c darcs -n 'contains \'log\' (commandline -poc)' -l index -s n -d 'Select a range of patches'
complete -c darcs -n 'contains \'log\' (commandline -poc)' -l matches -d 'Select patches matching PATTERN'
complete -c darcs -n 'contains \'log\' (commandline -poc)' -l patches -s p -d 'Select patches matching REGEXP'
complete -c darcs -n 'contains \'log\' (commandline -poc)' -l tags -s t -d 'Select tags matching REGEXP'
complete -c darcs -n 'contains \'log\' (commandline -poc)' -l hash -s h -d 'Select a single patch with HASH'
complete -c darcs -n 'contains \'log\' (commandline -poc)' -l max-count -d 'Return only NUMBER results'
complete -c darcs -n 'contains \'log\' (commandline -poc)' -l only-to-files -d 'Show only changes to specified files'
complete -c darcs -n 'contains \'log\' (commandline -poc)' -l no-only-to-files -d 'Show changes to all files [DEFAULT]'
complete -c darcs -n 'contains \'log\' (commandline -poc)' -l context -d 'Give output suitable for clone --context'
complete -c darcs -n 'contains \'log\' (commandline -poc)' -l xml-output -d 'Generate XML formatted output'
complete -c darcs -n 'contains \'log\' (commandline -poc)' -l human-readable -d 'Give human-readable output'
complete -c darcs -n 'contains \'log\' (commandline -poc)' -l machine-readable -d 'Give machine-readable output'
complete -c darcs -n 'contains \'log\' (commandline -poc)' -l number -d 'Number the changes'
complete -c darcs -n 'contains \'log\' (commandline -poc)' -l count -d 'Output count of changes'
complete -c darcs -n 'contains \'log\' (commandline -poc)' -l summary -s s -d 'Summarize changes'
complete -c darcs -n 'contains \'log\' (commandline -poc)' -l no-summary -d 'Don\'t summarize changes'
complete -c darcs -n 'contains \'log\' (commandline -poc)' -l reverse -d 'Show/consider changes in reverse order'
complete -c darcs -n 'contains \'log\' (commandline -poc)' -l no-reverse -d 'Show/consider changes in the usual order [DEFAULT]'
complete -c darcs -n 'contains \'log\' (commandline -poc)' -l repo -d 'Specify the repository URL'
complete -c darcs -n 'contains \'log\' (commandline -poc)' -l repodir -x -a '(__fish_complete_directories (commandline -ct))' -d 'Specify the repository directory in which to run'
complete -c darcs -n 'contains \'log\' (commandline -poc)' -l all -s a -d 'Answer yes to all patches'
complete -c darcs -n 'contains \'log\' (commandline -poc)' -l no-interactive -d 'Answer yes to all patches'
complete -c darcs -n 'contains \'log\' (commandline -poc)' -l interactive -s i -d 'Prompt user interactively'
complete -c darcs -n 'contains \'log\' (commandline -poc)' -l no-http-pipelining -d 'Disable HTTP pipelining'
complete -c darcs -n 'contains \'log\' (commandline -poc)' -l remote-darcs -d 'Name of the darcs executable on the remote server'
complete -c darcs -n 'contains \'log\' (commandline -poc)' -l with-patch-index -d 'Build patch index [DEFAULT]'
complete -c darcs -n 'contains \'log\' (commandline -poc)' -l no-patch-index -d 'Don\'t build patch index'

#
# Completions for the 'annotate' subcommand
#

complete -c darcs -n 'contains \'annotate\' (commandline -poc)' -l human-readable -d 'Give human-readable output [DEFAULT]'
complete -c darcs -n 'contains \'annotate\' (commandline -poc)' -l machine-readable -d 'Give machine-readable output'
complete -c darcs -n 'contains \'annotate\' (commandline -poc)' -l match -d 'Select a single patch matching PATTERN'
complete -c darcs -n 'contains \'annotate\' (commandline -poc)' -l patch -s p -d 'Select a single patch matching REGEXP'
complete -c darcs -n 'contains \'annotate\' (commandline -poc)' -l hash -s h -d 'Select a single patch with HASH'
complete -c darcs -n 'contains \'annotate\' (commandline -poc)' -l tag -s t -d 'Select tag matching REGEXP'
complete -c darcs -n 'contains \'annotate\' (commandline -poc)' -l index -s n -d 'Select one patch'
complete -c darcs -n 'contains \'annotate\' (commandline -poc)' -l repodir -x -a '(__fish_complete_directories (commandline -ct))' -d 'Specify the repository directory in which to run'
complete -c darcs -n 'contains \'annotate\' (commandline -poc)' -l with-patch-index -d 'Build patch index [DEFAULT]'
complete -c darcs -n 'contains \'annotate\' (commandline -poc)' -l no-patch-index -d 'Don\'t build patch index'

#
# Completions for the 'dist' subcommand
#

complete -c darcs -n 'contains \'dist\' (commandline -poc)' -l dist-name -s d -d 'Name of version'
complete -c darcs -n 'contains \'dist\' (commandline -poc)' -l zip -d 'Generate zip archive instead of gzip\'ed tar'
complete -c darcs -n 'contains \'dist\' (commandline -poc)' -l repodir -x -a '(__fish_complete_directories (commandline -ct))' -d 'Specify the repository directory in which to run'
complete -c darcs -n 'contains \'dist\' (commandline -poc)' -l match -d 'Select a single patch matching PATTERN'
complete -c darcs -n 'contains \'dist\' (commandline -poc)' -l patch -s p -d 'Select a single patch matching REGEXP'
complete -c darcs -n 'contains \'dist\' (commandline -poc)' -l hash -s h -d 'Select a single patch with HASH'
complete -c darcs -n 'contains \'dist\' (commandline -poc)' -l tag -s t -d 'Select tag matching REGEXP'
complete -c darcs -n 'contains \'dist\' (commandline -poc)' -l index -s n -d 'Select one patch'
complete -c darcs -n 'contains \'dist\' (commandline -poc)' -l set-scripts-executable -d 'Make scripts executable'
complete -c darcs -n 'contains \'dist\' (commandline -poc)' -l dont-set-scripts-executable -d 'Don\'t make scripts executable [DEFAULT]'
complete -c darcs -n 'contains \'dist\' (commandline -poc)' -l no-set-scripts-executable -d 'Don\'t make scripts executable [DEFAULT]'
complete -c darcs -n 'contains \'dist\' (commandline -poc)' -l store-in-memory -d 'Do patch application in memory rather than on disk'
complete -c darcs -n 'contains \'dist\' (commandline -poc)' -l no-store-in-memory -d 'Do patch application on disk [DEFAULT]'

#
# Completions for the 'show contents' subcommand
#

complete -c darcs -n 'contains \'show\' (commandline -poc) && contains \'contents\' (commandline -poc)' -l match -d 'Select a single patch matching PATTERN'
complete -c darcs -n 'contains \'show\' (commandline -poc) && contains \'contents\' (commandline -poc)' -l patch -s p -d 'Select a single patch matching REGEXP'
complete -c darcs -n 'contains \'show\' (commandline -poc) && contains \'contents\' (commandline -poc)' -l hash -s h -d 'Select a single patch with HASH'
complete -c darcs -n 'contains \'show\' (commandline -poc) && contains \'contents\' (commandline -poc)' -l tag -s t -d 'Select tag matching REGEXP'
complete -c darcs -n 'contains \'show\' (commandline -poc) && contains \'contents\' (commandline -poc)' -l index -s n -d 'Select one patch'
complete -c darcs -n 'contains \'show\' (commandline -poc) && contains \'contents\' (commandline -poc)' -l repodir -x -a '(__fish_complete_directories (commandline -ct))' -d 'Specify the repository directory in which to run'

#
# Completions for the 'show dependencies' subcommand
#

complete -c darcs -n 'contains \'show\' (commandline -poc) && contains \'dependencies\' (commandline -poc)' -l from-match -d 'Select changes starting with a patch matching PATTERN'
complete -c darcs -n 'contains \'show\' (commandline -poc) && contains \'dependencies\' (commandline -poc)' -l from-patch -d 'Select changes starting with a patch matching REGEXP'
complete -c darcs -n 'contains \'show\' (commandline -poc) && contains \'dependencies\' (commandline -poc)' -l from-hash -d 'Select changes starting with a patch with HASH'
complete -c darcs -n 'contains \'show\' (commandline -poc) && contains \'dependencies\' (commandline -poc)' -l from-tag -d 'Select changes starting with a tag matching REGEXP'
complete -c darcs -n 'contains \'show\' (commandline -poc) && contains \'dependencies\' (commandline -poc)' -l last -d 'Select the last NUMBER patches'
complete -c darcs -n 'contains \'show\' (commandline -poc) && contains \'dependencies\' (commandline -poc)' -l matches -d 'Select patches matching PATTERN'
complete -c darcs -n 'contains \'show\' (commandline -poc) && contains \'dependencies\' (commandline -poc)' -l patches -s p -d 'Select patches matching REGEXP'
complete -c darcs -n 'contains \'show\' (commandline -poc) && contains \'dependencies\' (commandline -poc)' -l tags -s t -d 'Select tags matching REGEXP'
complete -c darcs -n 'contains \'show\' (commandline -poc) && contains \'dependencies\' (commandline -poc)' -l hash -s h -d 'Select a single patch with HASH'

#
# Completions for the 'show files' subcommand
#

complete -c darcs -n 'contains \'show\' (commandline -poc) && contains \'files\' (commandline -poc)' -l files -d 'Include files in output [DEFAULT]'
complete -c darcs -n 'contains \'show\' (commandline -poc) && contains \'files\' (commandline -poc)' -l no-files -d 'Don\'t include files in output'
complete -c darcs -n 'contains \'show\' (commandline -poc) && contains \'files\' (commandline -poc)' -l directories -d 'Include directories in output [DEFAULT]'
complete -c darcs -n 'contains \'show\' (commandline -poc) && contains \'files\' (commandline -poc)' -l no-directories -d 'Don\'t include directories in output'
complete -c darcs -n 'contains \'show\' (commandline -poc) && contains \'files\' (commandline -poc)' -l pending -d 'Reflect pending patches in output [DEFAULT]'
complete -c darcs -n 'contains \'show\' (commandline -poc) && contains \'files\' (commandline -poc)' -l no-pending -d 'Only include recorded patches in output'
complete -c darcs -n 'contains \'show\' (commandline -poc) && contains \'files\' (commandline -poc)' -l null -s 0 -d 'Separate file names by NUL characters'
complete -c darcs -n 'contains \'show\' (commandline -poc) && contains \'files\' (commandline -poc)' -l match -d 'Select a single patch matching PATTERN'
complete -c darcs -n 'contains \'show\' (commandline -poc) && contains \'files\' (commandline -poc)' -l patch -s p -d 'Select a single patch matching REGEXP'
complete -c darcs -n 'contains \'show\' (commandline -poc) && contains \'files\' (commandline -poc)' -l hash -s h -d 'Select a single patch with HASH'
complete -c darcs -n 'contains \'show\' (commandline -poc) && contains \'files\' (commandline -poc)' -l tag -s t -d 'Select tag matching REGEXP'
complete -c darcs -n 'contains \'show\' (commandline -poc) && contains \'files\' (commandline -poc)' -l index -s n -d 'Select one patch'
complete -c darcs -n 'contains \'show\' (commandline -poc) && contains \'files\' (commandline -poc)' -l repodir -x -a '(__fish_complete_directories (commandline -ct))' -d 'Specify the repository directory in which to run'

#
# Completions for the 'show index' subcommand
#

complete -c darcs -n 'contains \'show\' (commandline -poc) && contains \'index\' (commandline -poc)' -l null -s 0 -d 'Separate file names by NUL characters'
complete -c darcs -n 'contains \'show\' (commandline -poc) && contains \'index\' (commandline -poc)' -l repodir -x -a '(__fish_complete_directories (commandline -ct))' -d 'Specify the repository directory in which to run'

#
# Completions for the 'show pristine' subcommand
#

complete -c darcs -n 'contains \'show\' (commandline -poc) && contains \'pristine\' (commandline -poc)' -l null -s 0 -d 'Separate file names by NUL characters'
complete -c darcs -n 'contains \'show\' (commandline -poc) && contains \'pristine\' (commandline -poc)' -l repodir -x -a '(__fish_complete_directories (commandline -ct))' -d 'Specify the repository directory in which to run'

#
# Completions for the 'show repo' subcommand
#

complete -c darcs -n 'contains \'show\' (commandline -poc) && contains \'repo\' (commandline -poc)' -l repodir -x -a '(__fish_complete_directories (commandline -ct))' -d 'Specify the repository directory in which to run'
complete -c darcs -n 'contains \'show\' (commandline -poc) && contains \'repo\' (commandline -poc)' -l xml-output -d 'Generate XML formatted output'
complete -c darcs -n 'contains \'show\' (commandline -poc) && contains \'repo\' (commandline -poc)' -l enum-patches -d 'Include statistics requiring enumeration of patches [DEFAULT]'
complete -c darcs -n 'contains \'show\' (commandline -poc) && contains \'repo\' (commandline -poc)' -l no-enum-patches -d 'Don\'t include statistics requiring enumeration of patches'

#
# Completions for the 'show authors' subcommand
#

complete -c darcs -n 'contains \'show\' (commandline -poc) && contains \'authors\' (commandline -poc)' -l repodir -x -a '(__fish_complete_directories (commandline -ct))' -d 'Specify the repository directory in which to run'

#
# Completions for the 'show tags' subcommand
#

complete -c darcs -n 'contains \'show\' (commandline -poc) && contains \'tags\' (commandline -poc)' -l repo -d 'Specify the repository URL'

#
# Completions for the 'show patch-index' subcommand
#

complete -c darcs -n 'contains \'show\' (commandline -poc) && contains \'patch-index\' (commandline -poc)' -l null -s 0 -d 'Separate file names by NUL characters'
complete -c darcs -n 'contains \'show\' (commandline -poc) && contains \'patch-index\' (commandline -poc)' -l repodir -x -a '(__fish_complete_directories (commandline -ct))' -d 'Specify the repository directory in which to run'

#
# Completions for the 'pull' subcommand
#

complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l matches -d 'Select patches matching PATTERN'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l patches -s p -d 'Select patches matching REGEXP'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l tags -s t -d 'Select tags matching REGEXP'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l hash -s h -d 'Select a single patch with HASH'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l reorder-patches -d 'Reorder the patches in the repository'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l no-reorder-patches -d 'Don\'t reorder the patches in the repository [DEFAULT]'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l all -s a -d 'Answer yes to all patches'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l no-interactive -d 'Answer yes to all patches'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l interactive -s i -d 'Prompt user interactively'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l mark-conflicts -d 'Mark conflicts [DEFAULT]'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l allow-conflicts -d 'Allow conflicts, but don\'t mark them'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l dont-allow-conflicts -d 'Fail if there are patches that would create conflicts'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l no-allow-conflicts -d 'Fail if there are patches that would create conflicts'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l no-resolve-conflicts -d 'Fail if there are patches that would create conflicts'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l skip-conflicts -d 'Filter out any patches that would create conflicts'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l external-merge -d 'Use external tool to merge conflicts'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l test -d 'Run the test script'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l no-test -d 'Don\'t run the test script [DEFAULT]'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l dry-run -d 'Don\'t actually take the action'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l xml-output -d 'Generate XML formatted output'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l summary -s s -d 'Summarize changes'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l no-summary -d 'Don\'t summarize changes'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l no-deps -d 'Don\'t automatically fulfill dependencies'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l auto-deps -d 'Don\'t ask about patches that are depended on by matched patches (with --match or --patch)'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l dont-prompt-for-dependencies -d 'Don\'t ask about patches that are depended on by matched patches (with --match or --patch)'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l prompt-deps -d 'Prompt about patches that are depended on by matched patches [DEFAULT]'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l prompt-for-dependencies -d 'Prompt about patches that are depended on by matched patches [DEFAULT]'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l set-default -d 'Set default repository'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l no-set-default -d 'Don\'t set default repository'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l repodir -x -a '(__fish_complete_directories (commandline -ct))' -d 'Specify the repository directory in which to run'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l ignore-unrelated-repos -d 'Do not check if repositories are unrelated'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l myers -d 'Use myers diff algorithm'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l patience -d 'Use patience diff algorithm [DEFAULT]'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l intersection -d 'Take intersection of all repositories'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l union -d 'Take union of all repositories [DEFAULT]'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l complement -d 'Take complement of repositories (in order listed)'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l compress -d 'Compress patch data [DEFAULT]'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l dont-compress -d 'Don\'t compress patch data'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l no-compress -d 'Don\'t compress patch data'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l ignore-times -d 'Don\'t trust the file modification times'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l no-ignore-times -d 'Trust modification times to find modified files [DEFAULT]'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l remote-repo -d 'Specify the remote repository URL to work with'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l set-scripts-executable -d 'Make scripts executable'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l dont-set-scripts-executable -d 'Don\'t make scripts executable [DEFAULT]'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l no-set-scripts-executable -d 'Don\'t make scripts executable [DEFAULT]'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l umask -d 'Specify umask to use when writing'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l restrict-paths -d 'Don\'t allow darcs to touch external files or repo metadata [DEFAULT]'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l dont-restrict-paths -d 'Allow darcs to modify any file or directory (unsafe)'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l no-restrict-paths -d 'Allow darcs to modify any file or directory (unsafe)'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l reverse -d 'Show/consider changes in reverse order'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l no-reverse -d 'Show/consider changes in the usual order [DEFAULT]'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l pause-for-gui -d 'Pause for an external diff or merge command to finish [DEFAULT]'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l no-pause-for-gui -d 'Return immediately after external diff or merge command finishes'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l no-http-pipelining -d 'Disable HTTP pipelining'
complete -c darcs -n 'contains \'pull\' (commandline -poc)' -l remote-darcs -d 'Name of the darcs executable on the remote server'

#
# Completions for the 'obliterate' subcommand
#

complete -c darcs -n 'contains \'obliterate\' (commandline -poc)' -l not-in-remote -d 'Select all patches not in the default push/pull repository or at location URL/PATH'
complete -c darcs -n 'contains \'obliterate\' (commandline -poc)' -l from-match -d 'Select changes starting with a patch matching PATTERN'
complete -c darcs -n 'contains \'obliterate\' (commandline -poc)' -l from-patch -d 'Select changes starting with a patch matching REGEXP'
complete -c darcs -n 'contains \'obliterate\' (commandline -poc)' -l from-hash -d 'Select changes starting with a patch with HASH'
complete -c darcs -n 'contains \'obliterate\' (commandline -poc)' -l from-tag -d 'Select changes starting with a tag matching REGEXP'
complete -c darcs -n 'contains \'obliterate\' (commandline -poc)' -l last -d 'Select the last NUMBER patches'
complete -c darcs -n 'contains \'obliterate\' (commandline -poc)' -l matches -d 'Select patches matching PATTERN'
complete -c darcs -n 'contains \'obliterate\' (commandline -poc)' -l patches -s p -d 'Select patches matching REGEXP'
complete -c darcs -n 'contains \'obliterate\' (commandline -poc)' -l tags -s t -d 'Select tags matching REGEXP'
complete -c darcs -n 'contains \'obliterate\' (commandline -poc)' -l hash -s h -d 'Select a single patch with HASH'
complete -c darcs -n 'contains \'obliterate\' (commandline -poc)' -l no-deps -d 'Don\'t automatically fulfill dependencies'
complete -c darcs -n 'contains \'obliterate\' (commandline -poc)' -l auto-deps -d 'Don\'t ask about patches that are depended on by matched patches (with --match or --patch)'
complete -c darcs -n 'contains \'obliterate\' (commandline -poc)' -l dont-prompt-for-dependencies -d 'Don\'t ask about patches that are depended on by matched patches (with --match or --patch)'
complete -c darcs -n 'contains \'obliterate\' (commandline -poc)' -l prompt-deps -d 'Prompt about patches that are depended on by matched patches [DEFAULT]'
complete -c darcs -n 'contains \'obliterate\' (commandline -poc)' -l prompt-for-dependencies -d 'Prompt about patches that are depended on by matched patches [DEFAULT]'
complete -c darcs -n 'contains \'obliterate\' (commandline -poc)' -l all -s a -d 'Answer yes to all patches'
complete -c darcs -n 'contains \'obliterate\' (commandline -poc)' -l no-interactive -d 'Answer yes to all patches'
complete -c darcs -n 'contains \'obliterate\' (commandline -poc)' -l interactive -s i -d 'Prompt user interactively'
complete -c darcs -n 'contains \'obliterate\' (commandline -poc)' -l repodir -x -a '(__fish_complete_directories (commandline -ct))' -d 'Specify the repository directory in which to run'
complete -c darcs -n 'contains \'obliterate\' (commandline -poc)' -l summary -s s -d 'Summarize changes'
complete -c darcs -n 'contains \'obliterate\' (commandline -poc)' -l no-summary -d 'Don\'t summarize changes'
complete -c darcs -n 'contains \'obliterate\' (commandline -poc)' -l output -s o -d 'Specify output filename'
complete -c darcs -n 'contains \'obliterate\' (commandline -poc)' -l output-auto-name -s O -d 'Output to automatically named file in DIRECTORY, default: current directory'
complete -c darcs -n 'contains \'obliterate\' (commandline -poc)' -l minimize -d 'Minimize context of patch bundle [DEFAULT]'
complete -c darcs -n 'contains \'obliterate\' (commandline -poc)' -l no-minimize -d 'Don\'t minimize context of patch bundle'
complete -c darcs -n 'contains \'obliterate\' (commandline -poc)' -l myers -d 'Use myers diff algorithm'
complete -c darcs -n 'contains \'obliterate\' (commandline -poc)' -l patience -d 'Use patience diff algorithm [DEFAULT]'
complete -c darcs -n 'contains \'obliterate\' (commandline -poc)' -l dry-run -d 'Don\'t actually take the action'
complete -c darcs -n 'contains \'obliterate\' (commandline -poc)' -l xml-output -d 'Generate XML formatted output'
complete -c darcs -n 'contains \'obliterate\' (commandline -poc)' -l compress -d 'Compress patch data [DEFAULT]'
complete -c darcs -n 'contains \'obliterate\' (commandline -poc)' -l dont-compress -d 'Don\'t compress patch data'
complete -c darcs -n 'contains \'obliterate\' (commandline -poc)' -l no-compress -d 'Don\'t compress patch data'
complete -c darcs -n 'contains \'obliterate\' (commandline -poc)' -l ignore-times -d 'Don\'t trust the file modification times'
complete -c darcs -n 'contains \'obliterate\' (commandline -poc)' -l no-ignore-times -d 'Trust modification times to find modified files [DEFAULT]'
complete -c darcs -n 'contains \'obliterate\' (commandline -poc)' -l umask -d 'Specify umask to use when writing'
complete -c darcs -n 'contains \'obliterate\' (commandline -poc)' -l reverse -d 'Show/consider changes in reverse order'
complete -c darcs -n 'contains \'obliterate\' (commandline -poc)' -l no-reverse -d 'Show/consider changes in the usual order [DEFAULT]'

#
# Completions for the 'rollback' subcommand
#

complete -c darcs -n 'contains \'rollback\' (commandline -poc)' -l from-match -d 'Select changes starting with a patch matching PATTERN'
complete -c darcs -n 'contains \'rollback\' (commandline -poc)' -l from-patch -d 'Select changes starting with a patch matching REGEXP'
complete -c darcs -n 'contains \'rollback\' (commandline -poc)' -l from-hash -d 'Select changes starting with a patch with HASH'
complete -c darcs -n 'contains \'rollback\' (commandline -poc)' -l from-tag -d 'Select changes starting with a tag matching REGEXP'
complete -c darcs -n 'contains \'rollback\' (commandline -poc)' -l last -d 'Select the last NUMBER patches'
complete -c darcs -n 'contains \'rollback\' (commandline -poc)' -l matches -d 'Select patches matching PATTERN'
complete -c darcs -n 'contains \'rollback\' (commandline -poc)' -l patches -s p -d 'Select patches matching REGEXP'
complete -c darcs -n 'contains \'rollback\' (commandline -poc)' -l tags -s t -d 'Select tags matching REGEXP'
complete -c darcs -n 'contains \'rollback\' (commandline -poc)' -l hash -s h -d 'Select a single patch with HASH'
complete -c darcs -n 'contains \'rollback\' (commandline -poc)' -l all -s a -d 'Answer yes to all patches'
complete -c darcs -n 'contains \'rollback\' (commandline -poc)' -l no-interactive -d 'Answer yes to all patches'
complete -c darcs -n 'contains \'rollback\' (commandline -poc)' -l interactive -s i -d 'Prompt user interactively'
complete -c darcs -n 'contains \'rollback\' (commandline -poc)' -l repodir -x -a '(__fish_complete_directories (commandline -ct))' -d 'Specify the repository directory in which to run'
complete -c darcs -n 'contains \'rollback\' (commandline -poc)' -l myers -d 'Use myers diff algorithm'
complete -c darcs -n 'contains \'rollback\' (commandline -poc)' -l patience -d 'Use patience diff algorithm [DEFAULT]'
complete -c darcs -n 'contains \'rollback\' (commandline -poc)' -l umask -d 'Specify umask to use when writing'

#
# Completions for the 'push' subcommand
#

complete -c darcs -n 'contains \'push\' (commandline -poc)' -l matches -d 'Select patches matching PATTERN'
complete -c darcs -n 'contains \'push\' (commandline -poc)' -l patches -s p -d 'Select patches matching REGEXP'
complete -c darcs -n 'contains \'push\' (commandline -poc)' -l tags -s t -d 'Select tags matching REGEXP'
complete -c darcs -n 'contains \'push\' (commandline -poc)' -l hash -s h -d 'Select a single patch with HASH'
complete -c darcs -n 'contains \'push\' (commandline -poc)' -l no-deps -d 'Don\'t automatically fulfill dependencies'
complete -c darcs -n 'contains \'push\' (commandline -poc)' -l auto-deps -d 'Don\'t ask about patches that are depended on by matched patches (with --match or --patch)'
complete -c darcs -n 'contains \'push\' (commandline -poc)' -l dont-prompt-for-dependencies -d 'Don\'t ask about patches that are depended on by matched patches (with --match or --patch)'
complete -c darcs -n 'contains \'push\' (commandline -poc)' -l prompt-deps -d 'Prompt about patches that are depended on by matched patches [DEFAULT]'
complete -c darcs -n 'contains \'push\' (commandline -poc)' -l prompt-for-dependencies -d 'Prompt about patches that are depended on by matched patches [DEFAULT]'
complete -c darcs -n 'contains \'push\' (commandline -poc)' -l all -s a -d 'Answer yes to all patches'
complete -c darcs -n 'contains \'push\' (commandline -poc)' -l no-interactive -d 'Answer yes to all patches'
complete -c darcs -n 'contains \'push\' (commandline -poc)' -l interactive -s i -d 'Prompt user interactively'
complete -c darcs -n 'contains \'push\' (commandline -poc)' -l sign -d 'Sign the patch with your gpg key'
complete -c darcs -n 'contains \'push\' (commandline -poc)' -l sign-as -d 'Sign the patch with a given keyid'
complete -c darcs -n 'contains \'push\' (commandline -poc)' -l sign-ssl -d 'Sign the patch using openssl with a given private key'
complete -c darcs -n 'contains \'push\' (commandline -poc)' -l dont-sign -d 'Don\'t sign the patch [DEFAULT]'
complete -c darcs -n 'contains \'push\' (commandline -poc)' -l no-sign -d 'Don\'t sign the patch [DEFAULT]'
complete -c darcs -n 'contains \'push\' (commandline -poc)' -l dry-run -d 'Don\'t actually take the action'
complete -c darcs -n 'contains \'push\' (commandline -poc)' -l xml-output -d 'Generate XML formatted output'
complete -c darcs -n 'contains \'push\' (commandline -poc)' -l summary -s s -d 'Summarize changes'
complete -c darcs -n 'contains \'push\' (commandline -poc)' -l no-summary -d 'Don\'t summarize changes'
complete -c darcs -n 'contains \'push\' (commandline -poc)' -l repodir -x -a '(__fish_complete_directories (commandline -ct))' -d 'Specify the repository directory in which to run'
complete -c darcs -n 'contains \'push\' (commandline -poc)' -l set-default -d 'Set default repository'
complete -c darcs -n 'contains \'push\' (commandline -poc)' -l no-set-default -d 'Don\'t set default repository'
complete -c darcs -n 'contains \'push\' (commandline -poc)' -l ignore-unrelated-repos -d 'Do not check if repositories are unrelated'
complete -c darcs -n 'contains \'push\' (commandline -poc)' -l apply-as -d 'Apply patch as another user using sudo'
complete -c darcs -n 'contains \'push\' (commandline -poc)' -l no-apply-as -d 'Don\'t use sudo to apply as another user [DEFAULT]'
complete -c darcs -n 'contains \'push\' (commandline -poc)' -l remote-repo -d 'Specify the remote repository URL to work with'
complete -c darcs -n 'contains \'push\' (commandline -poc)' -l reverse -d 'Show/consider changes in reverse order'
complete -c darcs -n 'contains \'push\' (commandline -poc)' -l no-reverse -d 'Show/consider changes in the usual order [DEFAULT]'
complete -c darcs -n 'contains \'push\' (commandline -poc)' -l compress -d 'Compress patch data [DEFAULT]'
complete -c darcs -n 'contains \'push\' (commandline -poc)' -l dont-compress -d 'Don\'t compress patch data'
complete -c darcs -n 'contains \'push\' (commandline -poc)' -l no-compress -d 'Don\'t compress patch data'
complete -c darcs -n 'contains \'push\' (commandline -poc)' -l no-http-pipelining -d 'Disable HTTP pipelining'
complete -c darcs -n 'contains \'push\' (commandline -poc)' -l remote-darcs -d 'Name of the darcs executable on the remote server'

#
# Completions for the 'send' subcommand
#

complete -c darcs -n 'contains \'send\' (commandline -poc)' -l matches -d 'Select patches matching PATTERN'
complete -c darcs -n 'contains \'send\' (commandline -poc)' -l patches -s p -d 'Select patches matching REGEXP'
complete -c darcs -n 'contains \'send\' (commandline -poc)' -l tags -s t -d 'Select tags matching REGEXP'
complete -c darcs -n 'contains \'send\' (commandline -poc)' -l hash -s h -d 'Select a single patch with HASH'
complete -c darcs -n 'contains \'send\' (commandline -poc)' -l no-deps -d 'Don\'t automatically fulfill dependencies'
complete -c darcs -n 'contains \'send\' (commandline -poc)' -l auto-deps -d 'Don\'t ask about patches that are depended on by matched patches (with --match or --patch)'
complete -c darcs -n 'contains \'send\' (commandline -poc)' -l dont-prompt-for-dependencies -d 'Don\'t ask about patches that are depended on by matched patches (with --match or --patch)'
complete -c darcs -n 'contains \'send\' (commandline -poc)' -l prompt-deps -d 'Prompt about patches that are depended on by matched patches [DEFAULT]'
complete -c darcs -n 'contains \'send\' (commandline -poc)' -l prompt-for-dependencies -d 'Prompt about patches that are depended on by matched patches [DEFAULT]'
complete -c darcs -n 'contains \'send\' (commandline -poc)' -l all -s a -d 'Answer yes to all patches'
complete -c darcs -n 'contains \'send\' (commandline -poc)' -l no-interactive -d 'Answer yes to all patches'
complete -c darcs -n 'contains \'send\' (commandline -poc)' -l interactive -s i -d 'Prompt user interactively'
complete -c darcs -n 'contains \'send\' (commandline -poc)' -l to -d 'Specify destination email'
complete -c darcs -n 'contains \'send\' (commandline -poc)' -l cc -d 'Mail results to additional EMAIL(s)'
complete -c darcs -n 'contains \'send\' (commandline -poc)' -l from -d 'Specify email address'
complete -c darcs -n 'contains \'send\' (commandline -poc)' -l subject -d 'Specify mail subject'
complete -c darcs -n 'contains \'send\' (commandline -poc)' -l in-reply-to -d 'Specify in-reply-to header'
complete -c darcs -n 'contains \'send\' (commandline -poc)' -l author -s A -d 'Specify author id'
complete -c darcs -n 'contains \'send\' (commandline -poc)' -l charset -d 'Specify mail charset'
complete -c darcs -n 'contains \'send\' (commandline -poc)' -l mail -d 'Send patch using sendmail'
complete -c darcs -n 'contains \'send\' (commandline -poc)' -l sendmail-command -d 'Specify sendmail command'
complete -c darcs -n 'contains \'send\' (commandline -poc)' -l output -s o -d 'Specify output filename'
complete -c darcs -n 'contains \'send\' (commandline -poc)' -l output-auto-name -s O -d 'Output to automatically named file in DIRECTORY, default: current directory'
complete -c darcs -n 'contains \'send\' (commandline -poc)' -l sign -d 'Sign the patch with your gpg key'
complete -c darcs -n 'contains \'send\' (commandline -poc)' -l sign-as -d 'Sign the patch with a given keyid'
complete -c darcs -n 'contains \'send\' (commandline -poc)' -l sign-ssl -d 'Sign the patch using openssl with a given private key'
complete -c darcs -n 'contains \'send\' (commandline -poc)' -l dont-sign -d 'Don\'t sign the patch [DEFAULT]'
complete -c darcs -n 'contains \'send\' (commandline -poc)' -l no-sign -d 'Don\'t sign the patch [DEFAULT]'
complete -c darcs -n 'contains \'send\' (commandline -poc)' -l dry-run -d 'Don\'t actually take the action'
complete -c darcs -n 'contains \'send\' (commandline -poc)' -l xml-output -d 'Generate XML formatted output'
complete -c darcs -n 'contains \'send\' (commandline -poc)' -l summary -s s -d 'Summarize changes'
complete -c darcs -n 'contains \'send\' (commandline -poc)' -l no-summary -d 'Don\'t summarize changes'
complete -c darcs -n 'contains \'send\' (commandline -poc)' -l edit-description -d 'Edit the patch bundle description [DEFAULT]'
complete -c darcs -n 'contains \'send\' (commandline -poc)' -l dont-edit-description -d 'Don\'t edit the patch bundle description'
complete -c darcs -n 'contains \'send\' (commandline -poc)' -l no-edit-description -d 'Don\'t edit the patch bundle description'
complete -c darcs -n 'contains \'send\' (commandline -poc)' -l set-default -d 'Set default repository'
complete -c darcs -n 'contains \'send\' (commandline -poc)' -l no-set-default -d 'Don\'t set default repository'
complete -c darcs -n 'contains \'send\' (commandline -poc)' -l repodir -x -a '(__fish_complete_directories (commandline -ct))' -d 'Specify the repository directory in which to run'
complete -c darcs -n 'contains \'send\' (commandline -poc)' -l minimize -d 'Minimize context of patch bundle [DEFAULT]'
complete -c darcs -n 'contains \'send\' (commandline -poc)' -l no-minimize -d 'Don\'t minimize context of patch bundle'
complete -c darcs -n 'contains \'send\' (commandline -poc)' -l ignore-unrelated-repos -d 'Do not check if repositories are unrelated'
complete -c darcs -n 'contains \'send\' (commandline -poc)' -l logfile -d 'Give patch name and comment in file'
complete -c darcs -n 'contains \'send\' (commandline -poc)' -l delete-logfile -d 'Delete the logfile when done'
complete -c darcs -n 'contains \'send\' (commandline -poc)' -l no-delete-logfile -d 'Keep the logfile when done [DEFAULT]'
complete -c darcs -n 'contains \'send\' (commandline -poc)' -l remote-repo -d 'Specify the remote repository URL to work with'
complete -c darcs -n 'contains \'send\' (commandline -poc)' -l context -d 'Send to context stored in FILENAME'
complete -c darcs -n 'contains \'send\' (commandline -poc)' -l reverse -d 'Show/consider changes in reverse order'
complete -c darcs -n 'contains \'send\' (commandline -poc)' -l no-reverse -d 'Show/consider changes in the usual order [DEFAULT]'
complete -c darcs -n 'contains \'send\' (commandline -poc)' -l no-http-pipelining -d 'Disable HTTP pipelining'
complete -c darcs -n 'contains \'send\' (commandline -poc)' -l remote-darcs -d 'Name of the darcs executable on the remote server'

#
# Completions for the 'apply' subcommand
#

complete -c darcs -n 'contains \'apply\' (commandline -poc)' -l verify -d 'Verify that the patch was signed by a key in PUBRING'
complete -c darcs -n 'contains \'apply\' (commandline -poc)' -l verify-ssl -d 'Verify using openSSL with authorized keys from file KEYS'
complete -c darcs -n 'contains \'apply\' (commandline -poc)' -l no-verify -d 'Don\'t verify patch signature [DEFAULT]'
complete -c darcs -n 'contains \'apply\' (commandline -poc)' -l reorder-patches -d 'Reorder the patches in the repository'
complete -c darcs -n 'contains \'apply\' (commandline -poc)' -l no-reorder-patches -d 'Don\'t reorder the patches in the repository [DEFAULT]'
complete -c darcs -n 'contains \'apply\' (commandline -poc)' -l all -s a -d 'Answer yes to all patches'
complete -c darcs -n 'contains \'apply\' (commandline -poc)' -l no-interactive -d 'Answer yes to all patches'
complete -c darcs -n 'contains \'apply\' (commandline -poc)' -l interactive -s i -d 'Prompt user interactively'
complete -c darcs -n 'contains \'apply\' (commandline -poc)' -l dry-run -d 'Don\'t actually take the action'
complete -c darcs -n 'contains \'apply\' (commandline -poc)' -l xml-output -d 'Generate XML formatted output'
complete -c darcs -n 'contains \'apply\' (commandline -poc)' -l matches -d 'Select patches matching PATTERN'
complete -c darcs -n 'contains \'apply\' (commandline -poc)' -l patches -s p -d 'Select patches matching REGEXP'
complete -c darcs -n 'contains \'apply\' (commandline -poc)' -l tags -s t -d 'Select tags matching REGEXP'
complete -c darcs -n 'contains \'apply\' (commandline -poc)' -l hash -s h -d 'Select a single patch with HASH'
complete -c darcs -n 'contains \'apply\' (commandline -poc)' -l mark-conflicts -d 'Mark conflicts'
complete -c darcs -n 'contains \'apply\' (commandline -poc)' -l allow-conflicts -d 'Allow conflicts, but don\'t mark them'
complete -c darcs -n 'contains \'apply\' (commandline -poc)' -l dont-allow-conflicts -d 'Fail if there are patches that would create conflicts [DEFAULT]'
complete -c darcs -n 'contains \'apply\' (commandline -poc)' -l no-allow-conflicts -d 'Fail if there are patches that would create conflicts [DEFAULT]'
complete -c darcs -n 'contains \'apply\' (commandline -poc)' -l no-resolve-conflicts -d 'Fail if there are patches that would create conflicts [DEFAULT]'
complete -c darcs -n 'contains \'apply\' (commandline -poc)' -l skip-conflicts -d 'Filter out any patches that would create conflicts'
complete -c darcs -n 'contains \'apply\' (commandline -poc)' -l external-merge -d 'Use external tool to merge conflicts'
complete -c darcs -n 'contains \'apply\' (commandline -poc)' -l test -d 'Run the test script'
complete -c darcs -n 'contains \'apply\' (commandline -poc)' -l no-test -d 'Don\'t run the test script [DEFAULT]'
complete -c darcs -n 'contains \'apply\' (commandline -poc)' -l leave-test-directory -d 'Don\'t remove the test directory [DEFAULT]'
complete -c darcs -n 'contains \'apply\' (commandline -poc)' -l remove-test-directory -d 'Remove the test directory'
complete -c darcs -n 'contains \'apply\' (commandline -poc)' -l repodir -d 'Specify the repository directory in which to run'
complete -c darcs -n 'contains \'apply\' (commandline -poc)' -l myers -d 'Use myers diff algorithm'
complete -c darcs -n 'contains \'apply\' (commandline -poc)' -l patience -d 'Use patience diff algorithm [DEFAULT]'
complete -c darcs -n 'contains \'apply\' (commandline -poc)' -l reply -d 'Reply to email-based patch using FROM address'
complete -c darcs -n 'contains \'apply\' (commandline -poc)' -l cc -d 'Mail results to additional EMAIL(s). Requires --reply'
complete -c darcs -n 'contains \'apply\' (commandline -poc)' -l happy-forwarding -d 'Forward unsigned messages without extra header'
complete -c darcs -n 'contains \'apply\' (commandline -poc)' -l no-happy-forwarding -d 'Don\'t forward unsigned messages without extra header [DEFAULT]'
complete -c darcs -n 'contains \'apply\' (commandline -poc)' -l mail -d 'Send patch using sendmail'
complete -c darcs -n 'contains \'apply\' (commandline -poc)' -l sendmail-command -d 'Specify sendmail command'
complete -c darcs -n 'contains \'apply\' (commandline -poc)' -l ignore-times -d 'Don\'t trust the file modification times'
complete -c darcs -n 'contains \'apply\' (commandline -poc)' -l no-ignore-times -d 'Trust modification times to find modified files [DEFAULT]'
complete -c darcs -n 'contains \'apply\' (commandline -poc)' -l compress -d 'Compress patch data [DEFAULT]'
complete -c darcs -n 'contains \'apply\' (commandline -poc)' -l dont-compress -d 'Don\'t compress patch data'
complete -c darcs -n 'contains \'apply\' (commandline -poc)' -l no-compress -d 'Don\'t compress patch data'
complete -c darcs -n 'contains \'apply\' (commandline -poc)' -l set-scripts-executable -d 'Make scripts executable'
complete -c darcs -n 'contains \'apply\' (commandline -poc)' -l dont-set-scripts-executable -d 'Don\'t make scripts executable [DEFAULT]'
complete -c darcs -n 'contains \'apply\' (commandline -poc)' -l no-set-scripts-executable -d 'Don\'t make scripts executable [DEFAULT]'
complete -c darcs -n 'contains \'apply\' (commandline -poc)' -l umask -d 'Specify umask to use when writing'
complete -c darcs -n 'contains \'apply\' (commandline -poc)' -l restrict-paths -d 'Don\'t allow darcs to touch external files or repo metadata [DEFAULT]'
complete -c darcs -n 'contains \'apply\' (commandline -poc)' -l dont-restrict-paths -d 'Allow darcs to modify any file or directory (unsafe)'
complete -c darcs -n 'contains \'apply\' (commandline -poc)' -l no-restrict-paths -d 'Allow darcs to modify any file or directory (unsafe)'
complete -c darcs -n 'contains \'apply\' (commandline -poc)' -l reverse -d 'Show/consider changes in reverse order'
complete -c darcs -n 'contains \'apply\' (commandline -poc)' -l no-reverse -d 'Show/consider changes in the usual order [DEFAULT]'
complete -c darcs -n 'contains \'apply\' (commandline -poc)' -l pause-for-gui -d 'Pause for an external diff or merge command to finish [DEFAULT]'
complete -c darcs -n 'contains \'apply\' (commandline -poc)' -l no-pause-for-gui -d 'Return immediately after external diff or merge command finishes'

#
# Completions for the 'clone' subcommand
#

complete -c darcs -n 'contains \'clone\' (commandline -poc)' -l repo-name -d 'Path of output directory'
complete -c darcs -n 'contains \'clone\' (commandline -poc)' -l repodir -x -a '(__fish_complete_directories (commandline -ct))' -d 'Path of output directory'
complete -c darcs -n 'contains \'clone\' (commandline -poc)' -l lazy -d 'Get patch files only as needed'
complete -c darcs -n 'contains \'clone\' (commandline -poc)' -l complete -d 'Get a complete copy of the repository'
complete -c darcs -n 'contains \'clone\' (commandline -poc)' -l to-match -d 'Select changes up to a patch matching PATTERN'
complete -c darcs -n 'contains \'clone\' (commandline -poc)' -l to-patch -d 'Select changes up to a patch matching REGEXP'
complete -c darcs -n 'contains \'clone\' (commandline -poc)' -l to-hash -d 'Select changes up to a patch with HASH'
complete -c darcs -n 'contains \'clone\' (commandline -poc)' -l tag -s t -d 'Select tag matching REGEXP'
complete -c darcs -n 'contains \'clone\' (commandline -poc)' -l context -d 'Version specified by the context in FILENAME'
complete -c darcs -n 'contains \'clone\' (commandline -poc)' -l set-default -d 'Set default repository'
complete -c darcs -n 'contains \'clone\' (commandline -poc)' -l no-set-default -d 'Don\'t set default repository'
complete -c darcs -n 'contains \'clone\' (commandline -poc)' -l set-scripts-executable -d 'Make scripts executable'
complete -c darcs -n 'contains \'clone\' (commandline -poc)' -l dont-set-scripts-executable -d 'Don\'t make scripts executable [DEFAULT]'
complete -c darcs -n 'contains \'clone\' (commandline -poc)' -l no-set-scripts-executable -d 'Don\'t make scripts executable [DEFAULT]'
complete -c darcs -n 'contains \'clone\' (commandline -poc)' -l with-working-dir -d 'Create a working tree (normal repository) [DEFAULT]'
complete -c darcs -n 'contains \'clone\' (commandline -poc)' -l no-working-dir -d 'Do not create a working tree (bare repository)'
complete -c darcs -n 'contains \'clone\' (commandline -poc)' -l packs -d 'Use repository packs [DEFAULT]'
complete -c darcs -n 'contains \'clone\' (commandline -poc)' -l no-packs -d 'Don\'t use repository packs'
complete -c darcs -n 'contains \'clone\' (commandline -poc)' -l with-patch-index -d 'Build patch index'
complete -c darcs -n 'contains \'clone\' (commandline -poc)' -l no-patch-index -d 'Don\'t build patch index [DEFAULT]'
complete -c darcs -n 'contains \'clone\' (commandline -poc)' -l no-http-pipelining -d 'Disable HTTP pipelining'
complete -c darcs -n 'contains \'clone\' (commandline -poc)' -l remote-darcs -d 'Name of the darcs executable on the remote server'

#
# Completions for the 'initialize' subcommand
#

complete -c darcs -n 'contains \'initialize\' (commandline -poc)' -l darcs-2 -d 'Standard darcs patch format [DEFAULT]'
complete -c darcs -n 'contains \'initialize\' (commandline -poc)' -l darcs-1 -d 'Older patch format (for compatibility)'
complete -c darcs -n 'contains \'initialize\' (commandline -poc)' -l with-working-dir -d 'Create a working tree (normal repository) [DEFAULT]'
complete -c darcs -n 'contains \'initialize\' (commandline -poc)' -l no-working-dir -d 'Do not create a working tree (bare repository)'
complete -c darcs -n 'contains \'initialize\' (commandline -poc)' -l repodir -x -a '(__fish_complete_directories (commandline -ct))' -d 'Specify the repository directory in which to run'
complete -c darcs -n 'contains \'initialize\' (commandline -poc)' -l with-patch-index -d 'Build patch index'
complete -c darcs -n 'contains \'initialize\' (commandline -poc)' -l no-patch-index -d 'Don\'t build patch index [DEFAULT]'
complete -c darcs -n 'contains \'initialize\' (commandline -poc)' -l hashed -d 'Deprecated, use --darcs-1 instead'

#
# Completions for the 'optimize' subcommands
#

complete -c darcs -n 'contains \'optimize\' (commandline -poc)' -l repodir -x -a '(__fish_complete_directories (commandline -ct))' -d 'Specify the repository directory in which to run'
complete -c darcs -n 'contains \'optimize\' (commandline -poc)' -l umask -d 'Specify umask to use when writing'

#
# Completions for the 'optimize relink' subcommand
#

complete -c darcs -n 'contains \'optimize\' (commandline -poc) && contains \'relink\' (commandline -poc)' -l sibling -d 'Specify a sibling directory'

#
# Completions for the 'repair' subcommand
#

complete -c darcs -n 'contains \'repair\' (commandline -poc)' -l repodir -x -a '(__fish_complete_directories (commandline -ct))' -d 'Specify the repository directory in which to run'
complete -c darcs -n 'contains \'repair\' (commandline -poc)' -l ignore-times -d 'Don\'t trust the file modification times'
complete -c darcs -n 'contains \'repair\' (commandline -poc)' -l no-ignore-times -d 'Trust modification times to find modified files [DEFAULT]'
complete -c darcs -n 'contains \'repair\' (commandline -poc)' -l myers -d 'Use myers diff algorithm'
complete -c darcs -n 'contains \'repair\' (commandline -poc)' -l patience -d 'Use patience diff algorithm [DEFAULT]'
complete -c darcs -n 'contains \'repair\' (commandline -poc)' -l dry-run -d 'Don\'t actually take the action'
complete -c darcs -n 'contains \'repair\' (commandline -poc)' -l umask -d 'Specify umask to use when writing'

#
# Completions for the 'convert darcs-2' subcommand
#

complete -c darcs -n 'contains \'convert\' (commandline -poc) && contains \'darcs-2\' (commandline -poc)' -l repo-name -d 'Path of output directory'
complete -c darcs -n 'contains \'convert\' (commandline -poc) && contains \'darcs-2\' (commandline -poc)' -l repodir -x -a '(__fish_complete_directories (commandline -ct))' -d 'Path of output directory'
complete -c darcs -n 'contains \'convert\' (commandline -poc) && contains \'darcs-2\' (commandline -poc)' -l set-scripts-executable -d 'Make scripts executable'
complete -c darcs -n 'contains \'convert\' (commandline -poc) && contains \'darcs-2\' (commandline -poc)' -l dont-set-scripts-executable -d 'Don\'t make scripts executable [DEFAULT]'
complete -c darcs -n 'contains \'convert\' (commandline -poc) && contains \'darcs-2\' (commandline -poc)' -l no-set-scripts-executable -d 'Don\'t make scripts executable [DEFAULT]'
complete -c darcs -n 'contains \'convert\' (commandline -poc) && contains \'darcs-2\' (commandline -poc)' -l with-working-dir -d 'Create a working tree (normal repository) [DEFAULT]'
complete -c darcs -n 'contains \'convert\' (commandline -poc) && contains \'darcs-2\' (commandline -poc)' -l no-working-dir -d 'Do not create a working tree (bare repository)'
complete -c darcs -n 'contains \'convert\' (commandline -poc) && contains \'darcs-2\' (commandline -poc)' -l no-http-pipelining -d 'Disable HTTP pipelining'
complete -c darcs -n 'contains \'convert\' (commandline -poc) && contains \'darcs-2\' (commandline -poc)' -l remote-darcs -d 'Name of the darcs executable on the remote server'
complete -c darcs -n 'contains \'convert\' (commandline -poc) && contains \'darcs-2\' (commandline -poc)' -l with-patch-index -d 'Build patch index'
complete -c darcs -n 'contains \'convert\' (commandline -poc) && contains \'darcs-2\' (commandline -poc)' -l no-patch-index -d 'Don\'t build patch index [DEFAULT]'

#
# Completions for the 'convert export' subcommand
#

complete -c darcs -n 'contains \'convert\' (commandline -poc) && contains \'export\' (commandline -poc)' -l repo-name -d 'Path of output directory'
complete -c darcs -n 'contains \'convert\' (commandline -poc) && contains \'export\' (commandline -poc)' -l repodir -x -a '(__fish_complete_directories (commandline -ct))' -d 'Path of output directory'
complete -c darcs -n 'contains \'convert\' (commandline -poc) && contains \'export\' (commandline -poc)' -l read-marks -d 'Continue conversion, previously checkpointed by --write-marks'
complete -c darcs -n 'contains \'convert\' (commandline -poc) && contains \'export\' (commandline -poc)' -l write-marks -d 'Checkpoint conversion to continue it later'
complete -c darcs -n 'contains \'convert\' (commandline -poc) && contains \'export\' (commandline -poc)' -l no-http-pipelining -d 'Disable HTTP pipelining'
complete -c darcs -n 'contains \'convert\' (commandline -poc) && contains \'export\' (commandline -poc)' -l remote-darcs -d 'Name of the darcs executable on the remote server'

#
# Completions for the 'convert import' subcommand
#

complete -c darcs -n 'contains \'convert\' (commandline -poc) && contains \'import\' (commandline -poc)' -l repo-name -d 'Path of output directory'
complete -c darcs -n 'contains \'convert\' (commandline -poc) && contains \'import\' (commandline -poc)' -l repodir -x -a '(__fish_complete_directories (commandline -ct))' -d 'Path of output directory'
complete -c darcs -n 'contains \'convert\' (commandline -poc) && contains \'import\' (commandline -poc)' -l set-scripts-executable -d 'Make scripts executable'
complete -c darcs -n 'contains \'convert\' (commandline -poc) && contains \'import\' (commandline -poc)' -l dont-set-scripts-executable -d 'Don\'t make scripts executable [DEFAULT]'
complete -c darcs -n 'contains \'convert\' (commandline -poc) && contains \'import\' (commandline -poc)' -l no-set-scripts-executable -d 'Don\'t make scripts executable [DEFAULT]'
complete -c darcs -n 'contains \'convert\' (commandline -poc) && contains \'import\' (commandline -poc)' -l darcs-2 -d 'Standard darcs patch format [DEFAULT]'
complete -c darcs -n 'contains \'convert\' (commandline -poc) && contains \'import\' (commandline -poc)' -l darcs-1 -d 'Older patch format (for compatibility)'
complete -c darcs -n 'contains \'convert\' (commandline -poc) && contains \'import\' (commandline -poc)' -l with-working-dir -d 'Create a working tree (normal repository) [DEFAULT]'
complete -c darcs -n 'contains \'convert\' (commandline -poc) && contains \'import\' (commandline -poc)' -l no-working-dir -d 'Do not create a working tree (bare repository)'
complete -c darcs -n 'contains \'convert\' (commandline -poc) && contains \'import\' (commandline -poc)' -l with-patch-index -d 'Build patch index'
complete -c darcs -n 'contains \'convert\' (commandline -poc) && contains \'import\' (commandline -poc)' -l no-patch-index -d 'Don\'t build patch index [DEFAULT]'
