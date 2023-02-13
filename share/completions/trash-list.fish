# Completions for trash-cli
# There are many implementations of trash cli tools, but the name ``trash-list`` is unique.

# https://github.com/andreafrancia/trash-cli
complete -f -c trash-list -s h -l help -d 'show help message'
complete -f -c trash-list -l print-completion -xa 'bash zsh tcsh' -d 'print completion script'
complete -f -c trash-list -l version -d 'show version number'
complete -f -c trash-list -l trash-dirs -d 'list trash dirs'
complete -f -c trash-list -l trash-dir -d 'specify trash directory'
complete -f -c trash-list -l all-users -d 'list trashcans of all users'
