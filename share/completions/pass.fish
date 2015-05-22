complete -c pass -n '__fish_use_subcommand' -a "help" -x -d "Show help"
complete -c pass -n '__fish_use_subcommand' -a "grep" -x -d "Search for password files"
complete -c pass -n '__fish_use_subcommand' -a "generate" -x -d "Generates a password"
complete -c pass -n '__fish_use_subcommand' -a "rm" -x -d "Removes a password"
complete -c pass -n '__fish_use_subcommand' -a "cp" -x -d "Copies a password"
complete -c pass -n '__fish_use_subcommand' -a "mv" -x -d "Moves a password"
complete -c pass -n '__fish_use_subcommand' -a "ls" -x -d "Lists passwords"
complete -c pass -n '__fish_use_subcommand' -a "find" -x -d "Finds passwords matching pass names"
complete -c pass -n '__fish_use_subcommand' -a "init" -x -d "Initialize new password storage"
complete -c pass -n '__fish_use_subcommand' -a "insert" -x -d "Insert a password"
complete -c pass -n '__fish_use_subcommand' -a "edit" -x -d "Insert or edit a password"
complete -c pass -n '__fish_use_subcommand' -a "git" -x -d "Run a git command"
complete -c pass -n '__fish_use_subcommand' -a "show" -x -d "Show a password"

complete -c pass -s c -l clip --description "Copy to clipboard"

complete -c pass -n "__fish_seen_subcommand_from rm cp mv generate insert" -s f -l force

complete -c pass -n "__fish_seen_subcommand_from rm"       -s r -l recursive
complete -c pass -n "__fish_seen_subcommand_from generate" -s n -l no-symbols
complete -c pass -n "__fish_seen_subcommand_from generate" -s i -l in-place
complete -c pass -n "__fish_seen_subcommand_from init"     -s p -l path
complete -c pass -n "__fish_seen_subcommand_from insert"   -s e -l echo
complete -c pass -n "__fish_seen_subcommand_from insert"   -s m -l multiline
complete -c pass -n "__fish_seen_subcommand_from git" -a "init push pull config log reflog rebase"

set -l files (pushd $HOME/.password-store; echo **/*.gpg | sed 's/\.gpg//g'; popd)

complete -c pass -n "not __fish_seen_subcommand_from git generate init help grep" -x -a "$files"
