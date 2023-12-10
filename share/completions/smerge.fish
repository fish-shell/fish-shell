# Sublime Merge CLI tool

complete -c smerge -s n -l new-window -d 'Open a new window'
complete -c smerge -l launch-or-new-window -d 'Open new window only if app is running'
complete -c smerge -s b -l background -d "Don't activate the application"
complete -c smerge -l safe-mode -d 'Launch in a sandboxed environment'
complete -c smerge -s h -l help -d 'Show help (this message) and exit'
complete -c smerge -s v -l version -d 'Show version and exit'
complete -c smerge -a search -x -d 'Search for commits in the current repository'
complete -c smerge -a blame -r -d 'Blame the given file in the current repo'
complete -c smerge -a log -r -d 'Show the file history in the current repo'
complete -c smerge -a mergetool -rF -d 'Open the merge tool for the given files'
complete -c smerge -l no-wait -d "Don't wait for the application to close" -n '__fish_seen_subcommand_from mergetool'
complete -c smerge -s o -rF -d "Merged output file" -n "__fish_seen_subcommand_from mergetool"
