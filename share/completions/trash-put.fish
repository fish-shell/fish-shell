# Completions for trash-cli
# There are many implementations of trash cli tools, but the name ``trash-put`` is unique.

# https://github.com/andreafrancia/trash-cli
complete -f -c trash-put -s h -l help -d 'show help message'
complete -f -c trash-put -l print-completion -xa 'bash zsh tcsh' -d 'print shell completion script'
complete -f -c trash-put -s d -l directory -d 'ignored (for GNU rm compatibility)'
complete -f -c trash-put -s f -l force -d 'silently ignore nonexistent files'
complete -f -c trash-put -s i -l interactive -d 'prompt before every removal'
complete -f -c trash-put -s r -s R -l recursive -d 'ignored (for GNU rm compatibility)'
complete -F -c trash-put -l trash-dir -d 'specify trash folder'
complete -f -c trash-put -s v -l verbose -d 'be verbose'
complete -f -c trash-put -l version -d 'show version number'
