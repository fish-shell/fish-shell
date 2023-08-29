# Completions for trash-cli
# There are many implementations of trash cli tools, identify different version of ``trash`` excutable by its help message.

# https://github.com/andreafrancia/trash-cli
function __trash_by_andreafrancia
    complete -f -c trash -s h -l help -d 'show help message'
    complete -f -c trash -l print-completion -xa 'bash zsh tcsh' -d 'print shell completion script'
    complete -f -c trash -s d -l directory -d 'ignored (for GNU rm compatibility)'
    complete -f -c trash -s f -l force -d 'silently ignore nonexistent files'
    complete -f -c trash -s i -l interactive -d 'prompt before every removal'
    complete -f -c trash -s r -s R -l recursive -d 'ignored (for GNU rm compatibility)'
    complete -F -c trash -l trash-dir -d 'specify trash folder'
    complete -f -c trash -s v -l verbose -d 'be verbose'
    complete -f -c trash -l version -d 'show version number'
end

if string match -qr "https://github.com/andreafrancia/trash-cli" (trash --help 2>/dev/null)
    __trash_by_andreafrancia
end
