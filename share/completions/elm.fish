set -l commands repl init reactor make install bump diff publish

complete -c elm -f
# repl completions
complete -c elm -n "not __fish_seen_subcommand_from $commands" -a repl -d 'Open up an interactive programming session'
complete -c elm -n "__fish_seen_subcommand_from repl" -l no-colors -d 'Turn off the colors in REPL'
complete -c elm -n "__fish_seen_subcommand_from repl" -l interpreter -d 'Path to an alternative JS interpreter'
# reactor completions
complete -c elm -n "not __fish_seen_subcommand_from $commands" -a reactor -d 'Compile code with a click'
complete -c elm -n "__fish_seen_subcommand_from reactor" -l port -d 'Compile code with a click'
# make completions
complete -c elm -n "not __fish_seen_subcommand_from $commands" -a make -d 'Compiles Elm code in JS or HTML'
complete -c elm -n "__fish_seen_subcommand_from make" -l output -F -r -d 'Specify the name of resulting JS file'
complete -c elm -n "__fish_seen_subcommand_from make" -l debug -d 'Turn on the time-travelling debugger'
complete -c elm -n "__fish_seen_subcommand_from make" -l optimize -d 'Turn on optimizations to make code smaller and faster'
complete -c elm -n "__fish_seen_subcommand_from make" -l docs -d 'Generate a JSON file of documentation for a package'
#other commands completions
complete -c elm -n "not __fish_seen_subcommand_from $commands" -a init -d 'Start an Elm project'
complete -c elm -n "not __fish_seen_subcommand_from $commands" -a install -d 'Fetches packages from Elm repository'
complete -c elm -n "not __fish_seen_subcommand_from $commands" -a bump -d 'Figures out the next version number based on API changes'
complete -c elm -n "not __fish_seen_subcommand_from $commands" -a diff -d 'See what changed in a package between different versions'
complete -c elm -n "not __fish_seen_subcommand_from $commands" -a publish -d 'Publishes your package on package.elm-lang.org so anyone in the community can use it'

complete -c elm -l help -d "Show a more detailed description"
