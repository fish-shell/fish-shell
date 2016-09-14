set -l cmds build help init serve test watch

complete -c mdbook -f
complete -c mdbook -s l -l help -d "Prints help"
complete -c mdbook -s V -l version -d "Prints version"

complete -c mdbook -n "not __fish_seen_subcommand_from $cmds" -a build -d "Build book from markdown files"
complete -c mdbook -n "not __fish_seen_subcommand_from $cmds" -a help -d "Prints help"

complete -c mdbook -n "not __fish_seen_subcommand_from $cmds" -a init -d "Create boilerplate structure and files in directory"
complete -c mdbook -n "__fish_seen_subcommand_from init" -l force -d "Skip confirmation prompts"
complete -c mdbook -n "__fish_seen_subcommand_from init" -l theme -d "Copy default theme into source folder"

complete -c mdbook -n "not __fish_seen_subcommand_from $cmds" -a serve -d "Serve book at http://localhost:3000"
complete -c mdbook -n "__fish_seen_subcommand_from serve" -l port -s p -x -d "Use another port (default 3000)"
complete -c mdbook -n "__fish_seen_subcommand_from serve" -l websocket-port -s w -x -d "Use another port for websocket (default 3001)"

complete -c mdbook -n "not __fish_seen_subcommand_from $cmds" -a test -d "Test that code samples compile"
complete -c mdbook -n "not __fish_seen_subcommand_from $cmds" -a watch -d "Watch file changes"
