# Completions for trash-cli
# There are many implementations of trash cli tools, but the name ``trash-restore`` is unique.

# https://github.com/andreafrancia/trash-cli
complete -f -c trash-restore -s h -l help -d 'show help message'
complete -f -c trash-restore -l print-completion -xa 'bash zsh tcsh' -d 'print shell completion script(default: None)'
complete -f -c trash-restore -l sort -a 'date path none' -d 'sort candidates(default: date)'
complete -f -c trash-restore -l version -d 'show version number'
