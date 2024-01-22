# tig - text-mode interface for Git

not functions -q __fish_git && source $__fish_data_dir/completions/git.fish

set -l subcommands log show reflog blame grep refs statsh status
complete -c tig -n "not contains -- -- (commandline -xpc) && not __fish_seen_subcommand_from $subcommands" -xa 'show\t"Open diff view using the given git-show(1) options"
blame\t"Annotate the given file, takes git-blame(1) options"
status\t"Start up in status view"
log\t"Start up in log view view, displaying git-log(1) output"
reflog\t"Start up in reflog view"
refs\t"Start up in refs view"
stash\t"Start up in stash view"
grep\t"Open the grep view. Supports the same options as git-grep(1)"
'
complete -c tig -n 'not contains -- -- (commandline -xpc)' -l stdin -d 'Read git commit IDs from stdin'
complete -c tig -n 'not contains -- -- (commandline -xpc)' -l pretty=raw -d 'Read git log output from stdin'
complete -c tig -n 'not contains -- -- (commandline -xpc)' -s C -r -d 'Run as if Tig was started in this directory'
complete -c tig -n 'not contains -- -- (commandline -xpc)' -s v -l version -d 'Show version and exit'
complete -c tig -n 'not contains -- -- (commandline -xpc)' -s h -l help -d 'Show help message and exit'

complete -c tig -n 'not contains -- -- (commandline -xpc) && __fish_seen_subcommand_from show' -xa '(set -l t (commandline -ct); complete -C"git show $t")'
complete -c tig -n 'not contains -- -- (commandline -xpc) && __fish_seen_subcommand_from blame' -xa '(set -l t (commandline -ct); complete -C"git blame $t")'
complete -c tig -n 'not contains -- -- (commandline -xpc) && __fish_seen_subcommand_from log' -xa '(set -l t (commandline -ct); complete -C"git log $t")'
complete -c tig -n 'not contains -- -- (commandline -xpc) && __fish_seen_subcommand_from grep' -xa '(set -l t (commandline -ct); complete -C"git grep $t")'

complete -c tig -f -n 'not contains -- -- (commandline -xpc)' -a '(__fish_git_ranges)'
complete -c tig -n 'contains -- -- (commandline -xpc)' -F
