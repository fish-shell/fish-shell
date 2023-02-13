# Completions for trash-cli
# There are many implementations of trash cli tools, but the name ``trash-empty`` is unique.

# https://github.com/andreafrancia/trash-cli
complete -f -c trash-empty -s h -l help -d 'show help message'
complete -f -c trash-empty -l print-completion -xa 'bash zsh tcsh' -d 'print shell completion script'
complete -f -c trash-empty -l version -d 'show version number'
complete -f -c trash-empty -s v -l verbose -d 'list files that will be deleted'
complete -F -c trash-empty -l trash-dir -d 'specify trash directory'
complete -f -c trash-empty -l all-users -d 'empty trashcan of all users'
complete -f -c trash-empty -s i -l interactive -d 'prompt before emptying'
complete -f -c trash-empty -s f -d 'don\'t ask before emptying'
complete -f -c trash-empty -l dry-run -d 'show which files would have been removed'
